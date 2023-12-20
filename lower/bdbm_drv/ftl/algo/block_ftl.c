/*
The MIT License (MIT)

Copyright (c) 2014-2015 CSAIL, MIT

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#if defined (KERNEL_MODE)
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/log2.h>

#elif defined (USER_MODE)
#include <stdio.h>
#include <stdint.h>
#include "uilog.h"

#else
#error Invalid Platform (KERNEL_MODE or USER_MODE)
#endif

#include "bdbm_drv.h"
#include "params.h"
#include "debug.h"
#include "abm.h"
#include "umemory.h"
#include "block_ftl.h"

#define DBG_ALLOW_INPLACE_UPDATE
/*#define ENABLE_LOG*/

/* FTL interface */
bdbm_ftl_inf_t _ftl_block_ftl = {
	.ptr_private = NULL,
	.create = bdbm_block_ftl_create,
	.destroy = bdbm_block_ftl_destroy,
	.get_free_ppa = bdbm_block_ftl_get_free_ppa,
	.get_ppa = bdbm_block_ftl_get_ppa,
	.map_lpa_to_ppa = bdbm_block_ftl_map_lpa_to_ppa,
	.invalidate_lpa = bdbm_block_ftl_invalidate_lpa,
	.do_gc = bdbm_block_ftl_do_gc,
	.is_gc_needed = bdbm_block_ftl_is_gc_needed,	
	.scan_badblocks = bdbm_block_ftl_badblock_scan,
	.load = NULL,
	.store = NULL,
	/*.load = bdbm_block_ftl_load,*/
	/*.store = bdbm_block_ftl_store,*/

	/* custom interfaces for rsd */
	.get_segno = bdbm_block_ftl_get_segno,
};


/* data structures for block-level FTL */
enum BDBM_BFTL_BLOCK_STATUS {
	BFTL_NOT_ALLOCATED = 0,
	BFTL_ALLOCATED,
	BFTL_DEAD,
};

enum BDBM_BFTL_PAGE_STATUS {
	BFTL_PG_FREE = 0,
	BFTL_PG_VALID,
	BFTL_PG_INVALID,
};

typedef struct {
	uint8_t status;	/* BDBM_BFTL_BLOCK_STATUS */
	uint64_t channel_no;
	uint64_t chip_no;
	uint64_t block_no;
	int64_t rw_pg_ofs; /* recently-written page offset */
	uint8_t* pst;	/* status of pages in a block */
} bdbm_block_mapping_entry_t;

typedef struct {
	uint64_t nr_segs;	/* a segment is the unit of mapping */
	uint64_t nr_pgs_per_seg;	/* how many pages belong to a segment */
	uint64_t nr_blks_per_seg;	/* how many blocks belong to a segment */

	bdbm_spinlock_t ftl_lock;
	bdbm_abm_info_t* abm;
	bdbm_block_mapping_entry_t** mt;
	bdbm_abm_block_t** gc_bab;
	bdbm_hlm_req_gc_t gc_hlm;

	uint64_t* nr_trim_pgs;
	uint64_t* nr_valid_pgs;
	int64_t nr_dead_segs;
} bdbm_block_ftl_private_t;


/* function prototypes */
uint32_t __bdbm_block_ftl_do_gc_segment (bdbm_drv_info_t* bdi, uint64_t seg_no);
uint32_t __bdbm_block_ftl_do_gc_block_merge (bdbm_drv_info_t* bdi, uint64_t seg_no, uint64_t blk_no);
//uint32_t __hlm_rsd_make_rm_seg (bdbm_drv_info_t* bdi, uint32_t seg_no);


/* inline functions */
static inline
uint64_t __bdbm_block_ftl_get_segment_no (bdbm_block_ftl_private_t *p, uint64_t lpa) 
{
	return lpa / p->nr_pgs_per_seg;
}

static inline
uint64_t __bdbm_block_ftl_get_block_no (bdbm_block_ftl_private_t *p, uint64_t lpa) 
{
	return (lpa % p->nr_pgs_per_seg) % p->nr_blks_per_seg;
}

static inline
uint64_t __bdbm_block_ftl_get_page_ofs (bdbm_block_ftl_private_t *p, uint64_t lpa) 
{
	return (lpa % p->nr_pgs_per_seg) / p->nr_blks_per_seg;
}


/* functions for block-level FTL */
uint32_t bdbm_block_ftl_create (bdbm_drv_info_t* bdi)
{
	bdbm_abm_info_t* abm = NULL;
	bdbm_block_ftl_private_t* p = NULL;
	bdbm_device_params_t* np = (bdbm_device_params_t*)BDBM_GET_DEVICE_PARAMS (bdi);
	bdbm_ftl_params* dp = BDBM_GET_DRIVER_PARAMS(bdi);

	uint64_t nr_segs;
	uint64_t nr_blks_per_seg;
	uint64_t nr_pgs_per_seg;
	uint64_t i, j;

	/* check FTL parameters */
	if (dp->mapping_type != MAPPING_POLICY_RSD && 
		dp->mapping_type != MAPPING_POLICY_BLOCK) {
		return 1;
	}

	/* create 'bdbm_abm_info' */
	if ((abm = bdbm_abm_create (np, 0)) == NULL) {
		bdbm_error ("bdbm_abm_create failed");
		return 1;
	}

	/* create a private data structure */
	if ((p = (bdbm_block_ftl_private_t*)bdbm_zmalloc 
			(sizeof (bdbm_block_ftl_private_t))) == NULL) {
		bdbm_error ("bdbm_malloc failed");
		return 1;
	}
	_ftl_block_ftl.ptr_private = (void*)p;

	/* calculate # of mapping entries */
	nr_blks_per_seg = np->nr_chips_per_channel * np->nr_channels;
	nr_pgs_per_seg = np->nr_pages_per_block * nr_blks_per_seg;
	nr_segs = np->nr_blocks_per_chip;

	/* intiailize variables for ftl */
	p->nr_segs = nr_segs;
	p->nr_dead_segs = 0;
	p->nr_blks_per_seg = nr_blks_per_seg;
	p->nr_pgs_per_seg = nr_pgs_per_seg;
	p->abm = abm;

	bdbm_spin_lock_init (&p->ftl_lock);

	/* initialize a mapping table */
	if ((p->mt = (bdbm_block_mapping_entry_t**)bdbm_zmalloc 
			(sizeof (bdbm_block_mapping_entry_t*) * p->nr_segs)) == NULL) {
		bdbm_error ("bdbm_malloc failed");
		goto fail;
	}
	for (i= 0; i < p->nr_segs; i++) {
		if ((p->mt[i] = (bdbm_block_mapping_entry_t*)bdbm_zmalloc
				(sizeof (bdbm_block_mapping_entry_t) * 
				p->nr_blks_per_seg)) == NULL) {
			bdbm_error ("bdbm_malloc failed");
			goto fail;
		}
		for (j = 0; j < p->nr_blks_per_seg; j++) {
			p->mt[i][j].status = BFTL_NOT_ALLOCATED;
			p->mt[i][j].channel_no = -1;
			p->mt[i][j].chip_no = -1;
			p->mt[i][j].block_no = -1;
			p->mt[i][j].rw_pg_ofs = -1;
			
			/* initialized with BFTL_PG_FREE */
			p->mt[i][j].pst = (uint8_t*)bdbm_zmalloc (sizeof (uint8_t) * np->nr_pages_per_block);
			bdbm_memset (p->mt[i][j].pst, BFTL_PG_FREE, sizeof (uint8_t) * np->nr_pages_per_block); 
		}
	}
	if ((p->nr_trim_pgs = (uint64_t*)bdbm_zmalloc (sizeof (uint64_t) * p->nr_segs)) == NULL) {
		bdbm_error ("bdbm_malloc failed");
		goto fail;
	}
	if ((p->nr_valid_pgs = (uint64_t*)bdbm_zmalloc (sizeof (uint64_t) * p->nr_segs)) == NULL) {
		bdbm_error ("bdbm_malloc failed");
		goto fail;
	} 

	/* initialize gc_hlm */
	if ((p->gc_bab = (bdbm_abm_block_t**)bdbm_zmalloc 
			(sizeof (bdbm_abm_block_t*) * p->nr_blks_per_seg)) == NULL) {
		bdbm_error ("bdbm_zmalloc failed");
		goto fail;
	}
	if ((p->gc_hlm.llm_reqs = (bdbm_llm_req_t*)bdbm_zmalloc
			(sizeof (bdbm_llm_req_t) * p->nr_blks_per_seg)) == NULL) {
		bdbm_error ("bdbm_zmalloc failed");
		goto fail;
	}
	bdbm_sema_init (&p->gc_hlm.done);

	bdbm_msg ("nr_segs = %llu, nr_blks_per_seg = %llu, nr_pgs_per_seg = %llu",
		p->nr_segs, p->nr_blks_per_seg, p->nr_pgs_per_seg);

	return 0;

fail:
	bdbm_block_ftl_destroy (bdi);
	return 1;
}

void bdbm_block_ftl_destroy (
	bdbm_drv_info_t* bdi)
{
	bdbm_block_ftl_private_t* p = (bdbm_block_ftl_private_t*)BDBM_FTL_PRIV (bdi);
	uint64_t i, j;

	if (p == NULL)
		return;
	if (p->nr_valid_pgs != NULL)
		bdbm_free (p->nr_valid_pgs);
	if (p->nr_trim_pgs != NULL)
		bdbm_free (p->nr_trim_pgs);
	if (p->gc_bab)
		bdbm_free (p->gc_bab);
	if (p->gc_hlm.llm_reqs)
		bdbm_free (p->gc_hlm.llm_reqs);
	if (p->mt != NULL) {
		for (i = 0; i < p->nr_segs; i++) {
			if (p->mt[i] != NULL) {
				for (j = 0; j < p->nr_blks_per_seg; j++)
					if (p->mt[i][j].pst)
						bdbm_free (p->mt[i][j].pst);
				bdbm_free (p->mt[i]);
			}
		}
		bdbm_free (p->mt);
	}
	if (p->abm != NULL)
		bdbm_abm_destroy (p->abm);
	bdbm_free (p);
}

uint32_t bdbm_block_ftl_get_ppa (
	bdbm_drv_info_t* bdi, 
	int64_t lpa,
	bdbm_phyaddr_t* ppa,
	uint64_t* sp_off)
{
	bdbm_block_ftl_private_t* p = (bdbm_block_ftl_private_t*)BDBM_FTL_PRIV (bdi);
	bdbm_block_mapping_entry_t* e = NULL;
	uint64_t segment_no;
	uint64_t block_no;
	uint64_t page_ofs;

	segment_no = __bdbm_block_ftl_get_segment_no (p, lpa);
	block_no = __bdbm_block_ftl_get_block_no (p, lpa);
	page_ofs = __bdbm_block_ftl_get_page_ofs (p, lpa);
	e = &p->mt[segment_no][block_no];

	if (e->status == BFTL_NOT_ALLOCATED) {
		/* NOTE: the host could send reads to not-allocated pages,
		 * especially when initialzing a file system;
		 * in that case, we just ignore requests */
		ppa->channel_no = 0;
		ppa->chip_no = 0;
		ppa->block_no = 0;
		ppa->page_no = 0;
		ppa->punit_id = 0;
		*sp_off = 0;
		return 0;
	} 

	/* see if the mapping entry has valid physical locations */ 
	bdbm_bug_on (e->channel_no == (uint64_t)-1);
	bdbm_bug_on (e->chip_no == (uint64_t)-1);
	bdbm_bug_on (e->block_no == (uint64_t)-1);

	/* return a phyical page address */
	ppa->channel_no = e->channel_no;
	ppa->chip_no = e->chip_no;
	ppa->block_no = e->block_no;
	ppa->page_no = page_ofs;
	ppa->punit_id = BDBM_GET_PUNIT_ID (bdi, ppa);

	/* TEMP */
	*sp_off = 0;
	/* END */

	return 0;
}

/* allocate a block one by one */
static uint64_t problem_seg_no = -1;

uint32_t __bdbm_block_ftl_is_allocated (
	bdbm_drv_info_t* bdi, 
	int64_t segment_no)
{
	bdbm_block_ftl_private_t* p = (bdbm_block_ftl_private_t*)BDBM_FTL_PRIV (bdi);
	bdbm_block_mapping_entry_t* e = NULL;
	uint32_t nr_alloc_blks = 0;
	uint32_t i;

	for (i = 0; i < p->nr_blks_per_seg; i++) {
		e = &p->mt[segment_no][i];
		if (e->status == BFTL_ALLOCATED) {
			nr_alloc_blks++;
		}
	}

	if (nr_alloc_blks != 0 &&
		nr_alloc_blks != p->nr_blks_per_seg) {
		bdbm_msg ("oops! # of allocated blocks per segment must be 0 or %d (%d)", 
			p->nr_blks_per_seg, nr_alloc_blks);
		bdbm_bug_on (1);
	}

	return nr_alloc_blks;
}

int32_t __bdbm_block_ftl_allocate_segment (
	bdbm_drv_info_t* bdi, 
	int64_t segment_no)
{
	bdbm_block_ftl_private_t* p = (bdbm_block_ftl_private_t*)BDBM_FTL_PRIV (bdi);
	bdbm_device_params_t* np = (bdbm_device_params_t*)BDBM_GET_DEVICE_PARAMS (bdi);
	uint32_t i;

	for (i = 0; i < p->nr_blks_per_seg; i++) {
		uint64_t channel_no = i % np->nr_channels;
		uint64_t chip_no = i / np->nr_channels;
		bdbm_block_mapping_entry_t* e = &p->mt[segment_no][i];
		bdbm_abm_block_t* b = NULL;

		bdbm_bug_on (e->status != BFTL_NOT_ALLOCATED);
	
		if ((b = bdbm_abm_get_free_block_prepare (p->abm, channel_no, chip_no)) != NULL) {
			bdbm_abm_get_free_block_commit (p->abm, b);
			e->status = BFTL_ALLOCATED;
			e->channel_no = b->channel_no;
			e->chip_no = b->chip_no;
			e->block_no = b->block_no;
			e->rw_pg_ofs = -1;
		} else {
			bdbm_error ("oops! bdbm_abm_get_free_block_prepare failed (%llu %llu)", channel_no, chip_no);
			goto error;
		}
	}

	return 0;

error:
	return -1;
}

uint32_t bdbm_block_ftl_get_free_ppa (
	bdbm_drv_info_t* bdi, 
	int64_t lpa,
	bdbm_phyaddr_t* ppa)
{
	bdbm_block_ftl_private_t* p = (bdbm_block_ftl_private_t*)BDBM_FTL_PRIV (bdi);
	bdbm_block_mapping_entry_t* e = NULL;
	uint64_t segment_no;
	uint64_t block_no;
	int64_t page_ofs;
	uint32_t ret = 1;

	segment_no = __bdbm_block_ftl_get_segment_no (p, lpa);

	/* [STEP1] see if the desired segment is empty or full */
	if (__bdbm_block_ftl_is_allocated (bdi, segment_no) == 0) {
		if (__bdbm_block_ftl_allocate_segment (bdi, segment_no) == -1) {
			bdbm_error ("oops! __bdbm_block_ftl_allocate_segment failed");
			bdbm_bug_on (1);
		}
	}

	/* [STEP2] see if it is dead? */
	if (p->nr_trim_pgs[segment_no] == p->nr_pgs_per_seg) {
		/* perform gc for the segment */
		__bdbm_block_ftl_do_gc_segment (bdi, segment_no);
#ifdef ENABLE_LOG
		bdbm_msg ("E: [%llu] erase", segment_no);
#endif
	}

	/* [STEP3] get the information for the desired block in the segment */
	block_no = __bdbm_block_ftl_get_block_no (p, lpa);
	page_ofs = __bdbm_block_ftl_get_page_ofs (p, lpa);
	e = &p->mt[segment_no][block_no];

#ifdef ENABLE_LOG
	bdbm_msg ("W: [%llu] lpa: %llu ofs: %lld # of used pages: %d", segment_no, lpa, page_ofs, p->nr_valid_pgs[segment_no]);
#endif

	bdbm_bug_on (e == NULL);

	/* [STEP4] is it already mapped? */
	if (e->status == BFTL_ALLOCATED) {
		/* if so, see if the target block is writable or not */
		if (e->rw_pg_ofs < page_ofs) {
			/* [CASE 1] it is a writable block */
			ppa->channel_no = e->channel_no;
			ppa->chip_no = e->chip_no;
			ppa->block_no = e->block_no;
			ppa->page_no = page_ofs;
			ppa->punit_id = BDBM_GET_PUNIT_ID (bdi, ppa);
			ret = 0;

			if ((e->rw_pg_ofs + 1) != page_ofs) {
				bdbm_msg ("INFO: seg: %llu, %llu %lld", segment_no, (e->rw_pg_ofs + 1), page_ofs);
			}

			/*
			bdbm_msg ("INFO: seg: %llu, lpa: %llu, rw_pg_ofs: %llu, page_ofs: %llu",
				segment_no, lpa, e->rw_pg_ofs, page_ofs);
			*/
		} else {
			/* [CASE 2] it is a not-writable block */
#ifdef DBG_ALLOW_INPLACE_UPDATE
			/* TODO: it will be an error case in our final implementation,
			 * but we temporarly allows this case */
			ppa->channel_no = e->channel_no;
			ppa->chip_no = e->chip_no;
			ppa->block_no = e->block_no;
			ppa->page_no = page_ofs;
			ppa->punit_id = BDBM_GET_PUNIT_ID (bdi, ppa);
			ret = 0;

			problem_seg_no = segment_no;

			/*#else*/
		
			bdbm_msg("[%llu] [OVERWRITE] %llu %llu", 
				segment_no,	p->nr_trim_pgs[segment_no], p->nr_valid_pgs[segment_no]);

			bdbm_msg ("[%llu] [OVERWRITE] this should not occur (rw_pg_ofs:%d page_ofs:%llu)", 
				segment_no, e->rw_pg_ofs, page_ofs);

			bdbm_msg ("[%llu] [# of trimmed pages = %llu, lpa = %llu",
				segment_no, p->nr_trim_pgs[segment_no], lpa);
			/*bdbm_bug_on (1);*/
#endif
		}
	} else {
		bdbm_error ("'e->status' is not valid (%u)", e->status);
		bdbm_bug_on (1);
	}

	return ret;
}


#if 0 
/* allocate a block one by one */
static uint64_t problem_seg_no = -1;

uint32_t bdbm_block_ftl_get_free_ppa (
	bdbm_drv_info_t* bdi, 
	int64_t lpa,
	bdbm_phyaddr_t* ppa)
{
	bdbm_block_ftl_private_t* p = (bdbm_block_ftl_private_t*)BDBM_FTL_PRIV (bdi);
	bdbm_block_mapping_entry_t* e = NULL;
	uint64_t segment_no;
	uint64_t block_no;
	uint64_t page_ofs;
	uint32_t ret = 1;

	segment_no = __bdbm_block_ftl_get_segment_no (p, lpa);
	block_no = __bdbm_block_ftl_get_block_no (p, lpa);
	page_ofs = __bdbm_block_ftl_get_page_ofs (p, lpa);
	e = &p->mt[segment_no][block_no];

	bdbm_bug_on (e == NULL);

	/* is it dead? */
	if (p->nr_trim_pgs[segment_no] == p->nr_pgs_per_seg) {
		/* perform gc for the segment */
		__bdbm_block_ftl_do_gc_segment (bdi, segment_no);
		/*#ifdef ENABLE_LOG*/
		bdbm_msg ("E: [%llu] erase", segment_no);
		/*#endif*/
	}

#ifdef ENABLE_LOG
	bdbm_msg ("W: [%llu] lpa: %llu ofs: %llu # of used pages: %d", segment_no, lpa, page_ofs, p->nr_valid_pgs[segment_no]);
#endif

	/* is it already mapped? */
	if (e->status == BFTL_ALLOCATED) {
		/* if so, see if the target block is writable or not */
		if (e->rw_pg_ofs < page_ofs) {
			/* [CASE 1] it is a writable block */
			ppa->channel_no = e->channel_no;
			ppa->chip_no = e->chip_no;
			ppa->block_no = e->block_no;
			ppa->page_no = page_ofs;
			ppa->punit_id = BDBM_GET_PUNIT_ID (bdi, ppa);
			ret = 0;

			if ((e->rw_pg_ofs + 1) != page_ofs) {
				bdbm_msg ("INFO: seg: %llu, %llu %llu", segment_no, (e->rw_pg_ofs + 1), page_ofs);
			}

			/*
			bdbm_msg ("INFO: seg: %llu, lpa: %llu, rw_pg_ofs: %llu, page_ofs: %llu",
				segment_no, lpa, e->rw_pg_ofs, page_ofs);
			*/
		} else {
			/* [CASE 2] it is a not-writable block */
#ifdef DBG_ALLOW_INPLACE_UPDATE
			/* TODO: it will be an error case in our final implementation,
			 * but we temporarly allows this case */
			ppa->channel_no = e->channel_no;
			ppa->chip_no = e->chip_no;
			ppa->block_no = e->block_no;
			ppa->page_no = page_ofs;
			ppa->punit_id = BDBM_GET_PUNIT_ID (bdi, ppa);
			ret = 0;

			problem_seg_no = segment_no;

			/*#else*/
		
			bdbm_msg("[%llu] [OVERWRITE] %llu %llu", 
				segment_no,	p->nr_trim_pgs[segment_no], p->nr_valid_pgs[segment_no]);

			bdbm_msg ("[%llu] [OVERWRITE] this should not occur (rw_pg_ofs:%llu page_ofs:%llu)", 
				segment_no, e->rw_pg_ofs, page_ofs);

			bdbm_msg ("[%llu] [# of trimmed pages = %llu, lpa = %llu",
				segment_no, p->nr_trim_pgs[segment_no], lpa);
			/*bdbm_bug_on (1);*/
#endif
		}
	} else if (e->status == BFTL_NOT_ALLOCATED) {
		/* [CASE 3] allocate a new free block */
		uint64_t channel_no;
		uint64_t chip_no;
		bdbm_abm_block_t* b = NULL;
		bdbm_device_params_t* np = (bdbm_device_params_t*)BDBM_GET_DEVICE_PARAMS (bdi);

		channel_no = block_no % np->nr_channels;
		chip_no = block_no / np->nr_channels;
		
		if ((b = bdbm_abm_get_free_block_prepare (p->abm, channel_no, chip_no)) != NULL) {
			bdbm_abm_get_free_block_commit (p->abm, b);

			ppa->channel_no = b->channel_no;
			ppa->chip_no = b->chip_no;
			ppa->block_no = b->block_no;
			ppa->page_no = page_ofs;
			ppa->punit_id = BDBM_GET_PUNIT_ID (bdi, ppa);
			ret = 0;
		} else {
			bdbm_error ("oops! bdbm_abm_get_free_block_prepare failed (%llu %llu)", channel_no, chip_no);
			bdbm_bug_on (1);
		}
	} else {
		bdbm_error ("'e->status' is not valid (%u)", e->status);
		bdbm_bug_on (1);
	}

	return ret;
}
#endif

uint32_t bdbm_block_ftl_map_lpa_to_ppa (
	bdbm_drv_info_t* bdi, 
	bdbm_logaddr_t* logaddr,
	bdbm_phyaddr_t* ppa)
{
	bdbm_block_ftl_private_t* p = (bdbm_block_ftl_private_t*)BDBM_FTL_PRIV (bdi);
	bdbm_block_mapping_entry_t* e = NULL;
	uint64_t segment_no;
	uint64_t block_no;
	uint64_t page_ofs;
	uint64_t lpa = logaddr->lpa[0];

	segment_no = __bdbm_block_ftl_get_segment_no (p, lpa);
	block_no = __bdbm_block_ftl_get_block_no (p, lpa);
	page_ofs = __bdbm_block_ftl_get_page_ofs (p, lpa);
	e = &p->mt[segment_no][block_no];

	bdbm_bug_on (e->pst[page_ofs] != BFTL_PG_FREE);

	/* is it already mapped? */
	if (e->status == BFTL_ALLOCATED) {
		/* if so, see if the mapping information is valid or not */
		bdbm_bug_on (e->channel_no != ppa->channel_no);	
		bdbm_bug_on (e->chip_no != ppa->chip_no);
		bdbm_bug_on (e->block_no != ppa->block_no);

		/* update the offset of the recently written page */
		e->rw_pg_ofs = page_ofs;

		e->pst[page_ofs] = BFTL_PG_VALID;
		p->nr_valid_pgs[segment_no]++;

		goto done;
	}

	bdbm_bug_on (1);

	/* mapping lpa to ppa */
	e->status = BFTL_ALLOCATED;
	e->channel_no = ppa->channel_no;
	e->chip_no = ppa->chip_no;
	e->block_no = ppa->block_no;
	e->rw_pg_ofs = page_ofs;
	
	e->pst[page_ofs] = BFTL_PG_VALID;
	p->nr_valid_pgs[segment_no]++;

done:
#ifdef ENABLE_LOG
	bdbm_msg ("M: [%llu] lap: %llu, rw_pg_ofs: %llu, # of used pages: %llu", 
		segment_no, lpa, e->rw_pg_ofs, p->nr_valid_pgs[segment_no]);
#endif
	return 0;
}

uint32_t bdbm_block_ftl_invalidate_lpa (
	bdbm_drv_info_t* bdi, 
	int64_t lpa,
	uint64_t len)
{
	bdbm_block_ftl_private_t* p = (bdbm_block_ftl_private_t*)BDBM_FTL_PRIV (bdi);
	bdbm_block_mapping_entry_t* e = NULL;
	uint64_t segment_no;
	uint64_t block_no;
	uint64_t page_ofs;

	segment_no = __bdbm_block_ftl_get_segment_no (p, lpa);
	block_no = __bdbm_block_ftl_get_block_no (p, lpa);
	page_ofs = __bdbm_block_ftl_get_page_ofs (p, lpa);
	e = &p->mt[segment_no][block_no];

	/* ignore trim requests for a free segment */
#if 0 /* FIXME: it could incur problems later... */
	if (e->status == BFTL_NOT_ALLOCATED) {
#ifdef ENABLE_LOG
		bdbm_msg ("T: [%llu] lpa: %llu (# of trimmed pages: %llu, # of used pages: %d)", 
			segment_no, lpa, p->nr_trim_pgs[segment_no], p->nr_valid_pgs[segment_no]);
#endif
		return 0;
	}
#endif

	/* ignore trim requests if it is already invalid */
#if 0
	if (e->pst[page_ofs] == 1)
		return 0;

	/* mark a corresponding page an invalid status */
	e->pst[page_ofs] = 1; 
	p->nr_trim_pgs[segment_no]++;
#endif

	switch (e->pst[page_ofs]) {
	case BFTL_PG_VALID:
		p->nr_valid_pgs[segment_no]--;
	case BFTL_PG_FREE:
		/* NOTE: it would be possible that the file-system discards unused pages */
		e->pst[page_ofs] = BFTL_PG_INVALID;
		p->nr_trim_pgs[segment_no]++;
		break;
	case BFTL_PG_INVALID:
		/* it is already trimmed -- ignore it */
		break;
	default:
		bdbm_bug_on (1);
		break;
	}

	if (p->nr_trim_pgs[segment_no] == p->nr_pgs_per_seg) {
		/* increate # of dead segments */
		//__hlm_rsd_make_rm_seg (bdi, segment_no);
		p->nr_dead_segs++;
	}

#ifdef ENABLE_LOG
	bdbm_msg ("T: [%llu] lpa: %llu (# of trimmed pages: %llu, # of used pages: %d)", 
		segment_no, lpa, p->nr_trim_pgs[segment_no], p->nr_valid_pgs[segment_no]);
#endif

	return 0;
}

uint8_t bdbm_block_ftl_is_gc_needed (bdbm_drv_info_t* bdi, int64_t lpa)
{
	bdbm_block_ftl_private_t* p = (bdbm_block_ftl_private_t*)BDBM_FTL_PRIV (bdi);
	bdbm_block_mapping_entry_t* e = NULL;
	uint64_t segment_no;
	uint64_t block_no;
	uint64_t page_ofs;
	uint8_t ret = 0;

	segment_no = __bdbm_block_ftl_get_segment_no (p, lpa);
	block_no = __bdbm_block_ftl_get_block_no (p, lpa);
	page_ofs = __bdbm_block_ftl_get_page_ofs (p, lpa);
	e = &p->mt[segment_no][block_no];
	bdbm_bug_on (e == NULL);

	if (e->status == BFTL_ALLOCATED) {
		if (e->rw_pg_ofs != -1 && e->rw_pg_ofs >= (int64_t)page_ofs) {
			/* see if all the segment are invalid */
			if (p->nr_valid_pgs[segment_no] == 0) {
				ret = 1; /* trigger GC */
			} else {
				bdbm_ftl_params* dp = BDBM_GET_DRIVER_PARAMS(bdi);
				if (dp->mapping_type == MAPPING_POLICY_RSD) {
					/* this case should not happen with RSD */
					bdbm_error ("[%llu] OOPS!!! # of valid pages: %llu, # of trimmed pages: %llu",
						segment_no,	p->nr_valid_pgs[segment_no], p->nr_trim_pgs[segment_no]);
				}
				ret = 1;
			}
		}
	}

	return ret;
}

uint32_t __bdbm_block_ftl_erase_block (bdbm_drv_info_t* bdi, uint64_t seg_no)
{
	bdbm_block_ftl_private_t* p = (bdbm_block_ftl_private_t*)BDBM_FTL_PRIV (bdi);
	bdbm_hlm_req_gc_t* hlm_gc = &p->gc_hlm;
	uint64_t i = 0, j = 0;

	bdbm_memset (p->gc_bab, 0x00, sizeof (bdbm_abm_block_t*) * p->nr_blks_per_seg);

	/* setup a set of reqs */
	for (i = 0; i < p->nr_blks_per_seg; i++) {
		bdbm_abm_block_t* b = NULL;
		bdbm_llm_req_t* r = NULL;
		bdbm_block_mapping_entry_t* e = &p->mt[seg_no][i];

		if (e->status == BFTL_NOT_ALLOCATED)
			continue;

		/* FIXME: this block has not been used -- it must be more general */
		if (e->rw_pg_ofs == -1) {
			bdbm_abm_erase_block (p->abm, e->channel_no, e->chip_no, e->block_no, 0);
			continue;
		}

		if ((b = bdbm_abm_get_block (p->abm, e->channel_no, e->chip_no, e->block_no)) == NULL) {
			bdbm_error ("oops! bdbm_abm_get_block failed (%llu %llu %llu)", 
				e->channel_no, e->chip_no, e->block_no);
			bdbm_bug_on (1);
		} else {
			p->gc_bab[j] = b;

			r = &hlm_gc->llm_reqs[j];
			r->req_type = REQTYPE_GC_ERASE;
			r->logaddr.lpa[0] = -1ULL; /* lpa is not available now */
			r->phyaddr.channel_no = b->channel_no;
			r->phyaddr.chip_no = b->chip_no;
			r->phyaddr.block_no = b->block_no;
			r->phyaddr.page_no = 0;
			r->phyaddr.punit_id = BDBM_GET_PUNIT_ID (bdi, (&r->phyaddr));
			r->ptr_hlm_req = (void*)hlm_gc;
			r->ret = 0;

			j++;
		}
	}

	/* send erase reqs to llm */
	hlm_gc->req_type = REQTYPE_GC_ERASE;
	hlm_gc->nr_llm_reqs = j;
	atomic64_set (&hlm_gc->nr_llm_reqs_done, 0);
	bdbm_sema_lock (&hlm_gc->done);
	for (i = 0; i < j; i++) {
		if ((bdi->ptr_llm_inf->make_req (bdi, &hlm_gc->llm_reqs[i])) != 0) {
			bdbm_error ("llm_make_req failed");
			bdbm_bug_on (1);
		}
	}
	bdbm_sema_lock (&hlm_gc->done);
	bdbm_sema_unlock (&hlm_gc->done);

	for (i = 0; i < j; i++) {
		uint8_t is_bad = 0;
		bdbm_abm_block_t* b = p->gc_bab[i];
		if (hlm_gc->llm_reqs[i].ret != 0)
			is_bad = 1; /* bad block */
		bdbm_abm_erase_block (p->abm, b->channel_no, b->chip_no, b->block_no, is_bad);
	}

	/* measure gc elapsed time */
	return 0;
}

uint32_t __bdbm_block_ftl_do_gc_segment (
	bdbm_drv_info_t* bdi,
	uint64_t seg_no)
{
	bdbm_device_params_t* np = (bdbm_device_params_t*)BDBM_GET_DEVICE_PARAMS (bdi);
	bdbm_block_ftl_private_t* p = (bdbm_block_ftl_private_t*)BDBM_FTL_PRIV (bdi);
	bdbm_block_mapping_entry_t* e = NULL;
	uint64_t i;

	/* step 3: erase all the blocks that belong to the victim */
	if (__bdbm_block_ftl_erase_block (bdi, seg_no) != 0) {
		bdbm_error ("__bdbm_block_ftl_erase_block failed");
		return 1;
	}

	/* step 4: reset the victim segment */
	for (i = 0; i < p->nr_blks_per_seg; i++) {
		e = &p->mt[seg_no][i];

		/* is it not allocated? */
		if (e->status == BFTL_NOT_ALLOCATED)
			continue; /* if it is, ignore it */

		/* check error cases */
		bdbm_bug_on (e->channel_no == (uint64_t)-1);
		bdbm_bug_on (e->chip_no == (uint64_t)-1);
		bdbm_bug_on (e->block_no == (uint64_t)-1);

		/* reset all the variables */
		e->status = BFTL_NOT_ALLOCATED;
		e->channel_no = -1;
		e->chip_no = -1;
		e->block_no = -1;
		e->rw_pg_ofs = -1;
		bdbm_memset (e->pst, BFTL_PG_FREE, sizeof (uint8_t) * np->nr_pages_per_block);
	}

	bdbm_bug_on (p->nr_dead_segs <= 0);

	p->nr_trim_pgs[seg_no] = 0;
	p->nr_valid_pgs[seg_no] = 0;
	p->nr_dead_segs--;

	//__hlm_rsd_make_rm_seg (bdi, seg_no);

	return 0;
}

#include "hlm_reqs_pool.h"

uint32_t __bdbm_block_ftl_do_gc_block_merge (
	bdbm_drv_info_t* bdi,
	uint64_t seg_no,
	uint64_t blk_no)
{
	bdbm_block_ftl_private_t* p = (bdbm_block_ftl_private_t*)BDBM_FTL_PRIV (bdi);
	bdbm_block_mapping_entry_t* e = &p->mt[seg_no][blk_no];
	bdbm_device_params_t* np = (bdbm_device_params_t*)BDBM_GET_DEVICE_PARAMS (bdi);
	bdbm_hlm_req_gc_t* hlm_gc = &p->gc_hlm;
	uint64_t j, k, nr_valid_pgs = 0, nr_trim_pgs = 0;

	if (e->status == BFTL_NOT_ALLOCATED)
		return 0; /* if it is, ignore it */

	/* ---------------------------------------------------------------- */
	/* [STEP1] build hlm_req_gc for reads */
	for (j = 0; j < np->nr_pages_per_block; j++) {
		/* are there any valid page in a block */
		if (e->pst[j] == BFTL_PG_VALID) {
			bdbm_llm_req_t* r = &hlm_gc->llm_reqs[nr_valid_pgs++];

			hlm_reqs_pool_reset_fmain (&r->fmain);
			hlm_reqs_pool_reset_logaddr (&r->logaddr);

			r->logaddr.lpa[0] = -1; /* the subpage contains new data */
			r->fmain.kp_stt[0] = KP_STT_DATA;
			r->req_type = REQTYPE_GC_READ;
			r->phyaddr.channel_no = e->channel_no;
			r->phyaddr.chip_no = e->chip_no;
			r->phyaddr.block_no = e->block_no;
			r->phyaddr.page_no = j;
			r->phyaddr.punit_id = BDBM_GET_PUNIT_ID (bdi, (&r->phyaddr));
			r->ptr_hlm_req = (void*)hlm_gc;
			r->ret = 0;
		} else if (e->pst[j] == BFTL_PG_INVALID) {
			nr_trim_pgs++;
		}
	}

	bdbm_msg ("[MERGE-BEGIN] valid: %llu invalid: %llu", nr_valid_pgs, nr_trim_pgs);

	/* wait until Q in llm becomes empty 
	 * TODO: it might be possible to further optimize this */
	bdi->ptr_llm_inf->flush (bdi);

	/* send read reqs to llm */
	if (nr_valid_pgs > 0) {
		hlm_gc->req_type = REQTYPE_GC_READ;
		hlm_gc->nr_llm_reqs = nr_valid_pgs;
		atomic64_set (&hlm_gc->nr_llm_reqs_done, 0);
		bdbm_sema_lock (&hlm_gc->done);
		for (j = 0; j < nr_valid_pgs; j++) {
			if ((bdi->ptr_llm_inf->make_req (bdi, &hlm_gc->llm_reqs[j])) != 0) {
				bdbm_error ("llm_make_req failed");
				bdbm_bug_on (1);
			}
		}
		bdbm_sema_lock (&hlm_gc->done);
		bdbm_sema_unlock (&hlm_gc->done);
	}

	/* ---------------------------------------------------------------- */
	/* [STEP2] erase a block (do not consider wear-leveling new) */
	{
		uint8_t is_bad = 0;
		//bdbm_llm_req_t* r = &hlm_gc->llm_reqs[0];
		bdbm_llm_req_t rr;
		bdbm_llm_req_t* r = &rr;

		/* setup an erase request */
		r->req_type = REQTYPE_GC_ERASE;
		r->logaddr.lpa[0] = -1ULL; /* lpa is not available now */
		r->phyaddr.channel_no = e->channel_no;
		r->phyaddr.chip_no = e->chip_no;
		r->phyaddr.block_no = e->block_no;
		r->phyaddr.page_no = 0;
		r->phyaddr.punit_id = BDBM_GET_PUNIT_ID (bdi, (&r->phyaddr));
		r->ptr_hlm_req = (void*)hlm_gc;
		r->ret = 0;

		/* send erase reqs to llm */
		hlm_gc->req_type = REQTYPE_GC_ERASE;
		hlm_gc->nr_llm_reqs = 1;
		atomic64_set (&hlm_gc->nr_llm_reqs_done, 0);
		bdbm_sema_lock (&hlm_gc->done);
		if ((bdi->ptr_llm_inf->make_req (bdi, r)) != 0) {
			bdbm_error ("llm_make_req failed");
			bdbm_bug_on (1);
		}
		bdbm_sema_lock (&hlm_gc->done);
		bdbm_sema_unlock (&hlm_gc->done);

		if (r->ret != 0)
			is_bad = 1; /* bad block */
		bdbm_abm_erase_block (p->abm, e->channel_no, e->chip_no, e->block_no, is_bad);

		/* reset all the variables */
		e->status = BFTL_NOT_ALLOCATED;
		e->channel_no = -1;
		e->chip_no = -1;
		e->block_no = -1;
		e->rw_pg_ofs = -1;
		bdbm_memset (e->pst, BFTL_PG_FREE, sizeof (uint8_t) * np->nr_pages_per_block);

		bdbm_bug_on (nr_trim_pgs > p->nr_trim_pgs[seg_no]);
		bdbm_bug_on (nr_valid_pgs > p->nr_valid_pgs[seg_no]); 	
		p->nr_trim_pgs[seg_no] -= nr_trim_pgs;
		p->nr_valid_pgs[seg_no] -= nr_valid_pgs;
	}

	/* ---------------------------------------------------------------- */
	/* [STEP3] build hlm_req_gc for writes */
	if (nr_valid_pgs > 0) {
		for (j = 0; j < nr_valid_pgs; j++) {
			bdbm_llm_req_t* r = &hlm_gc->llm_reqs[j];

			bdbm_bug_on (r->fmain.kp_stt[0] != KP_STT_DATA);

			r->req_type = REQTYPE_GC_WRITE;	/* change to write */
			r->logaddr.lpa[0] = ((uint64_t*)r->foob.data)[0];

			if (bdbm_block_ftl_get_free_ppa (bdi, r->logaddr.lpa[0], &r->phyaddr) != 0) {
				bdbm_error ("bdbm_page_ftl_get_free_ppa failed");
				bdbm_bug_on (1);
			}
			if (bdbm_block_ftl_map_lpa_to_ppa (bdi, &r->logaddr, &r->phyaddr) != 0) {
				bdbm_error ("bdbm_page_ftl_map_lpa_to_ppa failed");
				bdbm_bug_on (1);
			}
		}

		/* send write reqs to llm */
		hlm_gc->req_type = REQTYPE_GC_WRITE;
		hlm_gc->nr_llm_reqs = nr_valid_pgs;
		atomic64_set (&hlm_gc->nr_llm_reqs_done, 0);
		bdbm_sema_lock (&hlm_gc->done);
		for (j = 0; j < nr_valid_pgs; j++) {
			if ((bdi->ptr_llm_inf->make_req (bdi, &hlm_gc->llm_reqs[j])) != 0) {
				bdbm_error ("llm_make_req failed");
				bdbm_bug_on (1);
			}
		}
		bdbm_sema_lock (&hlm_gc->done);
		bdbm_sema_unlock (&hlm_gc->done);
	}

	bdbm_msg ("[MERGE-END] valid: %llu invalid: %llu", nr_valid_pgs, nr_trim_pgs);

	return 0;
}

uint32_t bdbm_block_ftl_do_gc (
	bdbm_drv_info_t* bdi,
	int64_t lpa)
{
#if 0
	bdbm_block_ftl_private_t* p = (bdbm_block_ftl_private_t*)BDBM_FTL_PRIV (bdi);
	uint64_t seg_no = -1; /* victim */
	uint64_t i;
	uint32_t ret;

	/* step 1: do we really need gc now? */
	if (bdbm_block_ftl_is_gc_needed (bdi, lpa) == 0)
		return 1;

	/* step 2: select a victim segment 
	 * TODO: need to improve it with a dead block list */
	for (i = 0; i < p->nr_segs; i++) {
		if (p->nr_trim_pgs[i] == p->nr_pgs_per_seg) {
			seg_no = i;
			break;
		}
	}

	/* it would be possible that there is no victim segment */
	if (seg_no == -1)
		return 1;

	/* step 3: do gc for a victim segment */
	ret = __bdbm_block_ftl_do_gc_segment (bdi, seg_no);
#endif

	bdbm_block_ftl_private_t* p = (bdbm_block_ftl_private_t*)BDBM_FTL_PRIV (bdi);
	uint64_t segment_no = __bdbm_block_ftl_get_segment_no (p, lpa);
	uint64_t block_no = __bdbm_block_ftl_get_block_no (p, lpa);

	if (p->nr_valid_pgs[segment_no] == 0) {
		return __bdbm_block_ftl_do_gc_segment (bdi, segment_no);
	} else
		return __bdbm_block_ftl_do_gc_block_merge (bdi, segment_no, block_no);
}

uint64_t bdbm_block_ftl_get_segno (bdbm_drv_info_t* bdi, uint64_t lpa)
{
	return __bdbm_block_ftl_get_segment_no (
		(bdbm_block_ftl_private_t*)BDBM_FTL_PRIV (bdi), lpa);
}

uint32_t bdbm_block_ftl_load (bdbm_drv_info_t* bdi, const char* fn)
{
	return 0;
}

uint32_t bdbm_block_ftl_store (bdbm_drv_info_t* bdi, const char* fn)
{
	return 0;
}

void __bdbm_block_ftl_badblock_scan_eraseblks (bdbm_drv_info_t* bdi, uint64_t block_no)
{
	bdbm_block_ftl_private_t* p = (bdbm_block_ftl_private_t*)BDBM_FTL_PRIV (bdi);
	bdbm_device_params_t* np = (bdbm_device_params_t*)BDBM_GET_DEVICE_PARAMS (bdi);
	bdbm_hlm_req_gc_t* hlm_gc = &p->gc_hlm;
	uint64_t i, j;

	/* setup blocks to erase */
	bdbm_memset (p->gc_bab, 0x00, sizeof (bdbm_abm_block_t*) * p->nr_blks_per_seg);
	for (i = 0; i < np->nr_channels; i++) {
		for (j = 0; j < np->nr_chips_per_channel; j++) {
			bdbm_abm_block_t* b = NULL;
			bdbm_llm_req_t* r = NULL;
			uint64_t punit_id = i*np->nr_chips_per_channel+j;

			if ((b = bdbm_abm_get_block (p->abm, i, j, block_no)) == NULL) {
				bdbm_error ("oops! bdbm_abm_get_block failed");
				bdbm_bug_on (1);
			}
			p->gc_bab[punit_id] = b;

			r = &hlm_gc->llm_reqs[punit_id];
			r->req_type = REQTYPE_GC_ERASE;
			r->logaddr.lpa[0] = -1ULL; /* lpa is not available now */
			r->phyaddr.channel_no = b->channel_no;
			r->phyaddr.chip_no = b->chip_no;
			r->phyaddr.block_no = b->block_no;
			r->phyaddr.page_no = 0;
			r->phyaddr.punit_id = BDBM_GET_PUNIT_ID (bdi, (&r->phyaddr));
			r->ptr_hlm_req = (void*)hlm_gc;
			r->ret = 0;
		}
	}

	/* send erase reqs to llm */
	hlm_gc->req_type = REQTYPE_GC_ERASE;
	hlm_gc->nr_llm_reqs = p->nr_blks_per_seg;
	atomic64_set (&hlm_gc->nr_llm_reqs_done, 0);
	bdbm_sema_lock (&hlm_gc->done);
	for (i = 0; i < p->nr_blks_per_seg; i++) {
		if ((bdi->ptr_llm_inf->make_req (bdi, &hlm_gc->llm_reqs[i])) != 0) {
			bdbm_error ("llm_make_req failed");
			bdbm_bug_on (1);
		}
	}
	bdbm_sema_lock (&hlm_gc->done);
	bdbm_sema_unlock (&hlm_gc->done);

	for (i = 0; i < p->nr_blks_per_seg; i++) {
		uint8_t is_bad = 0;
		bdbm_abm_block_t* b = p->gc_bab[i];
		if (hlm_gc->llm_reqs[i].ret != 0)
			is_bad = 1; /* bad block */
		/*
		bdbm_msg ("erase: %llu %llu %llu (%llu)", 
			b->channel_no, 
			b->chip_no, 
			b->block_no, 
			is_bad);
		*/
		bdbm_abm_erase_block (p->abm, b->channel_no, b->chip_no, b->block_no, is_bad);
	}

	/* measure gc elapsed time */
}

static void __bdbm_block_mark_it_dead (
	bdbm_drv_info_t* bdi,
	uint64_t block_no)
{
	bdbm_block_ftl_private_t* p = (bdbm_block_ftl_private_t*)BDBM_FTL_PRIV (bdi);
	bdbm_device_params_t* np = (bdbm_device_params_t*)BDBM_GET_DEVICE_PARAMS (bdi);
	uint64_t i, j;

	for (i = 0; i < np->nr_channels; i++) {
		for (j = 0; j < np->nr_chips_per_channel; j++) {
			bdbm_abm_block_t* b = NULL;

			if ((b = bdbm_abm_get_block (p->abm, i, j, block_no)) == NULL) {
				bdbm_error ("oops! bdbm_abm_get_block failed");
				bdbm_bug_on (1);
			}

			bdbm_abm_set_to_dirty_block (p->abm, i, j, block_no);
		}
	}
}

uint32_t bdbm_block_ftl_badblock_scan (bdbm_drv_info_t* bdi)
{
	bdbm_block_ftl_private_t* p = (bdbm_block_ftl_private_t*)BDBM_FTL_PRIV (bdi);
	bdbm_device_params_t* np = (bdbm_device_params_t*)BDBM_GET_DEVICE_PARAMS (bdi);
	bdbm_block_mapping_entry_t* me = NULL;
	uint64_t i = 0, j = 0;
	uint32_t ret = 0;

	bdbm_msg ("[WARNING] 'bdbm_block_ftl_badblock_scan' is called! All of the flash blocks will be erased!!!");

	/* step1: reset the page-level mapping table */
	bdbm_msg ("step1: reset the block-level mapping table");
	for (i = 0; i < p->nr_segs; i++) {
		me = p->mt[i];
		for (j = 0; j < p->nr_blks_per_seg; j++) {
			me[j].status = BFTL_NOT_ALLOCATED;
			me[j].channel_no = -1;
			me[j].chip_no = -1;
			me[j].block_no = -1;
			me[j].rw_pg_ofs = -1;
			bdbm_memset ((uint8_t*)me[j].pst, BFTL_PG_FREE, sizeof (uint8_t) * np->nr_pages_per_block);
		}
		p->nr_trim_pgs[i] = 0;
	}
	p->nr_dead_segs = 0;

	/* step2: erase all the blocks */
	bdbm_msg ("step2: erase all the blocks");
	bdi->ptr_llm_inf->flush (bdi);
	for (i = 0; i < np->nr_blocks_per_chip; i++) {
		__bdbm_block_ftl_badblock_scan_eraseblks (bdi, i);
	}

	/* step3: store abm */
	/*
	if ((ret = bdbm_abm_store (p->bai, "/usr/share/bdbm_drv/abm.dat"))) {
		bdbm_error ("bdbm_abm_store failed");
		return 1;
	}
	*/

#if 0	
	bdbm_block_ftl_private_t* p = (bdbm_block_ftl_private_t*)BDBM_FTL_PRIV (bdi);
	bdbm_device_params_t* np = (bdbm_device_params_t*)BDBM_GET_DEVICE_PARAMS (bdi);
	bdbm_block_mapping_entry_t* me = NULL;
	uint64_t i = 0, j = 0;
	uint32_t ret = 0;
	uint32_t erased_blocks = 0;

	bdbm_msg ("[WARNING] 'bdbm_block_ftl_badblock_scan' is called! All of the flash blocks will be dirty!!!");

	/* step1: reset the page-level mapping table */
	/*
	bdbm_msg ("step1: reset the block-level mapping table");
	for (i = 0; i < p->nr_segs; i++) {
		me = p->mt[i];
		for (j = 0; j < p->nr_blks_per_seg; j++) {
			me[j].status = BFTL_NOT_ALLOCATED;
			me[j].channel_no = -1;
			me[j].chip_no = -1;
			me[j].block_no = -1;
			me[j].rw_pg_ofs = -1;
			bdbm_memset ((uint8_t*)me[j].pst, 0x00, sizeof (uint8_t) * np->nr_pages_per_block);
		}
		p->nr_trim_pgs[i] = 0;
	}
	*/
	p->nr_dead_segs = 0;

	/* step2: erase all the blocks */
	bdbm_msg ("step2: erase all the blocks");
	bdi->ptr_llm_inf->flush (bdi);
	for (i = 0; i < p->nr_segs; i++) {
		if (erased_blocks <= p->nr_blks_per_seg) {
			/* reset mapping table (clean) */
			me = p->mt[i];
			for (j = 0; j < p->nr_blks_per_seg; j++) {
				me[j].status = BFTL_NOT_ALLOCATED;
				me[j].channel_no = -1;
				me[j].chip_no = -1;
				me[j].block_no = -1;
				me[j].rw_pg_ofs = -1;
				bdbm_memset ((uint8_t*)me[j].pst, 0x00, sizeof (uint8_t) * np->nr_pages_per_block);
			}
			p->nr_trim_pgs[i] = 0;
			
			/* erase them */
			__bdbm_block_ftl_badblock_scan_eraseblks (bdi, i);
		} else {
			/* reset mapping table (dirty) */
			me = p->mt[i];
			for (j = 0; j < p->nr_blks_per_seg; j++) {
				me[j].status = BFTL_NOT_ALLOCATED;
				me[j].channel_no = j % np->nr_channels;
				me[j].chip_no = j / np->nr_channels;
				me[j].block_no = i;
				me[j].rw_pg_ofs = np->nr_pages_per_block;
				bdbm_memset ((uint8_t*)me[j].pst, 1, sizeof (uint8_t) * np->nr_pages_per_block);
			}
			p->nr_trim_pgs[i] = p->nr_pgs_per_seg;
			p->nr_dead_segs += p->nr_blks_per_seg;

			/* make them dirty */
			__bdbm_block_mark_it_dead (bdi, i);
		}

		erased_blocks += p->nr_blks_per_seg;
	}

	/* step3: store abm */
	/*
	if ((ret = bdbm_abm_store (p->bai, "/usr/share/bdbm_drv/abm.dat"))) {
		bdbm_error ("bdbm_abm_store failed");
		return 1;
	}
	*/
#endif
	bdbm_msg ("done");
	 
	return ret;
}

