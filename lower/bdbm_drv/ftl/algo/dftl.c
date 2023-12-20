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
#include "upage.h"

#else
#error Invalid Platform (KERNEL_MODE or USER_MODE)
#endif

#include "bdbm_drv.h"
#include "params.h"
#include "debug.h"
#include "utime.h"
#include "ufile.h"

#include "algo/abm.h"
#include "algo/dftl.h"
#include "algo/dftl_map.h"


/* FTL interface */
bdbm_ftl_inf_t _ftl_dftl = {
	.ptr_private = NULL,
	.create = bdbm_dftl_create,
	.destroy = bdbm_dftl_destroy,
	.get_free_ppa = bdbm_dftl_get_free_ppa,
	.get_ppa = bdbm_dftl_get_ppa,
	.map_lpa_to_ppa = bdbm_dftl_map_lpa_to_ppa,
	.invalidate_lpa = bdbm_dftl_invalidate_lpa,
	.do_gc = bdbm_dftl_do_gc,
	.is_gc_needed = bdbm_dftl_is_gc_needed,

	/* intializationn */
	.scan_badblocks = bdbm_dftl_badblock_scan,
	.load = bdbm_dftl_load,
	.store = bdbm_dftl_store,

	/* mapping-blks management */
	.check_mapblk = bdbm_dftl_check_mapblk,
	.prepare_mapblk_eviction = bdbm_dftl_prepare_mapblk_eviction,
	.finish_mapblk_eviction = bdbm_dftl_finish_mapblk_eviction,
	.prepare_mapblk_load = bdbm_dftl_prepare_mapblk_load,
	.finish_mapblk_load = bdbm_dftl_finish_mapblk_load,
};

typedef struct {
	bdbm_abm_info_t* bai;
	dftl_mapping_table_t* mt;
	bdbm_spinlock_t ftl_lock;
	uint64_t nr_punits;	

	/* for the management of active blocks */
	uint64_t curr_puid;
	uint64_t curr_page_ofs;
	bdbm_abm_block_t** ac_bab;

	/* reserved for gc (reused whenever gc is invoked) */
	bdbm_abm_block_t** gc_bab;
	bdbm_hlm_req_gc_t gc_hlm;

	/* for bad-block scanning */
	bdbm_sema_t badblk;
} bdbm_dftl_private_t;


uint32_t __bdbm_dftl_get_active_blocks (
	bdbm_device_params_t* np,
	bdbm_abm_info_t* bai,
	bdbm_abm_block_t** bab)
{
	uint64_t i, j;

	/* get a set of free blocks for active blocks */
	for (i = 0; i < np->nr_channels; i++) {
		for (j = 0; j < np->nr_chips_per_channel; j++) {
			/* prepare & commit free blocks */
			if ((*bab = bdbm_abm_get_free_block_prepare (bai, i, j))) {
				bdbm_abm_get_free_block_commit (bai, *bab);
				/*bdbm_msg ("active blk = %p", *bab);*/
				bab++;
			} else {
				bdbm_error ("bdbm_abm_get_free_block_prepare failed");
				return 1;
			}
		}
	}

	return 0;
}

bdbm_abm_block_t** __bdbm_dftl_create_active_blocks (
	bdbm_device_params_t* np,
	bdbm_abm_info_t* bai)
{
	uint64_t nr_punits;
	bdbm_abm_block_t** bab = NULL;

	nr_punits = np->nr_chips_per_channel * np->nr_channels;

	/*bdbm_msg ("nr_punits: %llu", nr_punits);*/

	/* create a set of active blocks */
	if ((bab = (bdbm_abm_block_t**)bdbm_zmalloc 
			(sizeof (bdbm_abm_block_t*) * nr_punits)) == NULL) {
		bdbm_error ("bdbm_zmalloc failed");
		goto fail;
	}

	/* get a set of free blocks for active blocks */
	if (__bdbm_dftl_get_active_blocks (np, bai, bab) != 0) {
		bdbm_error ("__bdbm_dftl_get_active_blocks failed");
		goto fail;
	}

	return bab;

fail:
	if (bab)
		bdbm_free (bab);
	return NULL;
}

void __bdbm_dftl_destroy_active_blocks (
	bdbm_abm_block_t** bab)
{
	if (bab == NULL)
		return;

	/* TODO: it might be required to save the status of active blocks 
	 * in order to support rebooting */
	bdbm_free (bab);
}

uint32_t bdbm_dftl_create (bdbm_drv_info_t* bdi)
{
	bdbm_dftl_private_t* p = NULL;
	bdbm_device_params_t* np = BDBM_GET_DEVICE_PARAMS (bdi);
	uint64_t i = 0, j = 0;
	uint64_t nr_kp_per_fp = np->page_main_size / KERNEL_PAGE_SIZE;	/* e.g., 2 = 8 KB / 4 KB */

	/* create a private data structure */
	if ((p = (bdbm_dftl_private_t*)bdbm_zmalloc 
			(sizeof (bdbm_dftl_private_t))) == NULL) {
		bdbm_error ("bdbm_malloc failed");
		return 1;
	}
	p->curr_puid = 0;
	p->curr_page_ofs = 0;
	p->nr_punits = np->nr_chips_per_channel * np->nr_channels;
	bdbm_spin_lock_init (&p->ftl_lock);
	_ftl_dftl.ptr_private = (void*)p;

	/* create 'bdbm_abm_info' with pst */
	if ((p->bai = bdbm_abm_create (np, 1)) == NULL) {
		bdbm_error ("bdbm_abm_create failed");
		bdbm_dftl_destroy (bdi);
		return 1;
	}

	/* create a mapping table */
	if ((p->mt = bdbm_dftl_create_mapping_table (np)) == NULL) {
		bdbm_error ("__bdbm_dftl_create_mapping_table failed");
		bdbm_dftl_destroy (bdi);
		return 1;
	}

	/* allocate active blocks */
	if ((p->ac_bab = __bdbm_dftl_create_active_blocks (np, p->bai)) == NULL) {
		bdbm_error ("__bdbm_dftl_create_active_blocks failed");
		bdbm_dftl_destroy (bdi);
		return 1;
	}

	/* allocate gc stuffs */
	if ((p->gc_bab = (bdbm_abm_block_t**)bdbm_zmalloc 
			(sizeof (bdbm_abm_block_t*) * p->nr_punits)) == NULL) {
		bdbm_error ("bdbm_zmalloc failed");
		bdbm_dftl_destroy (bdi);
		return 1;
	}
	if ((p->gc_hlm.llm_reqs = (bdbm_llm_req_t*)bdbm_zmalloc
			(sizeof (bdbm_llm_req_t) * p->nr_punits * np->nr_pages_per_block)) == NULL) {
		bdbm_error ("bdbm_zmalloc failed");
		bdbm_dftl_destroy (bdi);
		return 1;
	}

	while (i < p->nr_punits * np->nr_pages_per_block) {
		bdbm_llm_req_t* r = &p->gc_hlm.llm_reqs[i];
		r->kpg_flags = NULL;
		r->pptr_kpgs = (uint8_t**)bdbm_malloc_atomic (sizeof(uint8_t*) * nr_kp_per_fp);
		for (j = 0; j < nr_kp_per_fp; j++)
			r->pptr_kpgs[j] = (uint8_t*)get_zeroed_page (GFP_KERNEL);
		r->ptr_oob = (uint8_t*)bdbm_malloc_atomic (sizeof (uint8_t) * np->page_oob_size);
		i++;
	}
	bdbm_sema_init (&p->gc_hlm.gc_done);

	return 0;
}

void bdbm_dftl_destroy (bdbm_drv_info_t* bdi)
{
	bdbm_dftl_private_t* p = _ftl_dftl.ptr_private;
	bdbm_device_params_t* np = BDBM_GET_DEVICE_PARAMS (bdi);

	if (!p)
		return;

	if (p->gc_hlm.llm_reqs) {
		uint64_t i = 0, j = 0;
		uint64_t nr_kp_per_fp = np->page_main_size / KERNEL_PAGE_SIZE;	/* e.g., 2 = 8 KB / 4 KB */
		while (i < p->nr_punits * np->nr_pages_per_block) {
			bdbm_llm_req_t* r = &p->gc_hlm.llm_reqs[i];
			for (j = 0; j < nr_kp_per_fp; j++)
				free_page ((unsigned long)r->pptr_kpgs[j]);
			bdbm_free_atomic (r->pptr_kpgs);
			bdbm_free_atomic (r->ptr_oob);
			i++;
		}
		bdbm_free (p->gc_hlm.llm_reqs);
	}
	if (p->gc_bab)
		bdbm_free (p->gc_bab);
	if (p->ac_bab)
		__bdbm_dftl_destroy_active_blocks (p->ac_bab);
	if (p->mt) 
		bdbm_dftl_destroy_mapping_table (p->mt);
	if (p->bai)
		bdbm_abm_destroy (p->bai);
	bdbm_free (p);
}

uint32_t bdbm_dftl_get_free_ppa (bdbm_drv_info_t* bdi, uint64_t lpa, bdbm_phyaddr_t* ppa)
{
	bdbm_dftl_private_t* p = _ftl_dftl.ptr_private;
	bdbm_device_params_t* np = BDBM_GET_DEVICE_PARAMS (bdi);
	bdbm_abm_block_t* b = NULL;
	uint64_t curr_channel;
	uint64_t curr_chip;

	/* get the channel & chip numbers */
	curr_channel = p->curr_puid % np->nr_channels;
	curr_chip = p->curr_puid / np->nr_channels;

	/* get the physical offset of the active blocks */
	b = p->ac_bab[curr_channel * np->nr_chips_per_channel + curr_chip];
	ppa->channel_no =  b->channel_no;
	ppa->chip_no = b->chip_no;
	ppa->block_no = b->block_no;
	ppa->page_no = p->curr_page_ofs;
	ppa->punit_id = BDBM_GET_PUNIT_ID (bdi, ppa);

	/* check some error cases before returning the physical address */
	bdbm_bug_on (ppa->channel_no != curr_channel);
	bdbm_bug_on (ppa->chip_no != curr_chip);
	bdbm_bug_on (ppa->page_no >= np->nr_pages_per_block);

	/* go to the next parallel unit */
	if ((p->curr_puid + 1) == p->nr_punits) {
		p->curr_puid = 0;
		p->curr_page_ofs++;	/* go to the next page */

		/* see if there are sufficient free pages or not */
		if (p->curr_page_ofs == np->nr_pages_per_block) {
			/* get active blocks */
			if (__bdbm_dftl_get_active_blocks (np, p->bai, p->ac_bab) != 0) {
				/*
				bdbm_msg ("free_blks: %llu clean_blks: %llu, dirty_blks: %llu, total_blks: %llu",
						bdbm_abm_get_nr_free_blocks (p->bai),
						bdbm_abm_get_nr_clean_blocks (p->bai),
						bdbm_abm_get_nr_dirty_blocks (p->bai),
						bdbm_abm_get_nr_total_blocks (p->bai)
						);
				*/
				bdbm_error ("__bdbm_dftl_get_active_blocks failed");
				return 1;
			}
			/* ok; go ahead with 0 offset */
			/*bdbm_msg ("curr_puid = %llu", p->curr_puid);*/
			p->curr_page_ofs = 0;
		}
	} else {
		/*bdbm_msg ("curr_puid = %llu", p->curr_puid);*/
		p->curr_puid++;
	}

	return 0;
}

uint32_t bdbm_dftl_map_lpa_to_ppa (bdbm_drv_info_t* bdi, uint64_t lpa, bdbm_phyaddr_t* ptr_phyaddr)
{
	bdbm_device_params_t* np = BDBM_GET_DEVICE_PARAMS (bdi);
	bdbm_dftl_private_t* p = _ftl_dftl.ptr_private;
	mapping_entry_t me;

	/* is it a valid logical address */
	if (lpa >= np->nr_pages_per_ssd) {
		bdbm_error ("LPA is beyond logical space (%llX)", lpa);
		return 1;
	}

	/* get the mapping entry for lpa */
	me = bdbm_dftl_get_mapping_entry (p->mt, lpa);
	if (me.status == DFTL_PAGE_NOT_EXIST) {
		bdbm_error ("A given lpa is not found in the mapping table (%llu)", lpa);
		return 1;
	}

	/* update the block status to dirty */
	if (me.status == DFTL_PAGE_VALID) {
		bdbm_abm_invalidate_page (
			p->bai, 
			me.phyaddr.channel_no, 
			me.phyaddr.chip_no,
			me.phyaddr.block_no,
			me.phyaddr.page_no
		);
	}

	/* update the mapping entry to point to a new physical location */
	me.status = DFTL_PAGE_VALID;
	me.phyaddr.channel_no = ptr_phyaddr->channel_no;
	me.phyaddr.chip_no = ptr_phyaddr->chip_no;
	me.phyaddr.block_no = ptr_phyaddr->block_no;
	me.phyaddr.page_no = ptr_phyaddr->page_no;
	bdbm_dftl_set_mapping_entry (p->mt, lpa, &me);

	return 0;
}

uint32_t bdbm_dftl_get_ppa (bdbm_drv_info_t* bdi, uint64_t lpa, bdbm_phyaddr_t* ppa)
{
	bdbm_device_params_t* np = BDBM_GET_DEVICE_PARAMS (bdi);
	bdbm_dftl_private_t* p = _ftl_dftl.ptr_private;
	mapping_entry_t me;
	uint32_t ret;

	/* is it a valid logical address */
	if (lpa >= np->nr_pages_per_ssd) {
		bdbm_error ("A given lpa is beyond logical space (%llu)", lpa);
		return 1;
	}

	/* get the mapping entry for lpa */
	me = bdbm_dftl_get_mapping_entry (p->mt, lpa);
	/*
	if (me.status == DFTL_PAGE_NOT_EXIST) {
		bdbm_error ("A given lpa is not found in the mapping table (%llu)", lpa);
		return 1;
	}
	*/

	/* NOTE: sometimes a file system attempts to read 
	 * a logical address that was not written before.
	 * in that case, we return 'address 0' */
	if (me.status != DFTL_PAGE_VALID) {
		ppa->channel_no = 0;
		ppa->chip_no = 0;
		ppa->block_no = 0;
		ppa->page_no = 0;
		ret = 1;
	} else {
		ppa->channel_no = me.phyaddr.channel_no;
		ppa->chip_no = me.phyaddr.chip_no;
		ppa->block_no = me.phyaddr.block_no;
		ppa->page_no = me.phyaddr.page_no;
		ppa->punit_id = BDBM_GET_PUNIT_ID (bdi, ppa);
		ret = 0;
	}

	return ret;
}

uint32_t bdbm_dftl_invalidate_lpa (bdbm_drv_info_t* bdi, uint64_t lpa, uint64_t len)
{	
	bdbm_device_params_t* np = BDBM_GET_DEVICE_PARAMS (bdi);
	bdbm_dftl_private_t* p = _ftl_dftl.ptr_private;
	mapping_entry_t me;
	uint64_t loop;

	/* check the range of input addresses */
	if ((lpa + len) > np->nr_pages_per_ssd) {
		bdbm_warning ("LPA is beyond logical space (%llu = %llu+%llu) %llu", 
			lpa+len, lpa, len, np->nr_pages_per_ssd);
		return 1;
	}

	/* make them invalid */
	for (loop = lpa; loop < (lpa + len); loop++) {
		me = bdbm_dftl_get_mapping_entry (p->mt, loop);
		if (me.status == DFTL_PAGE_NOT_EXIST) {
			/* don't do TRIM */
			return 0;
			/*bdbm_error ("A given lpa is not found in the mapping table (%llu)", lpa);*/
			/*return 1;*/
		}

		if (me.status == DFTL_PAGE_VALID) {
			/* update a block status to dirty */
			bdbm_abm_invalidate_page (
				p->bai, 
				me.phyaddr.channel_no, 
				me.phyaddr.chip_no,
				me.phyaddr.block_no,
				me.phyaddr.page_no
			);

			/* update a mapping entry to invalid */
			bdbm_dftl_invalidate_mapping_entry (p->mt, loop);
		} else {
			/* do nothing in other cases */
		}
	}

	return 0;
}

uint8_t bdbm_dftl_is_gc_needed (bdbm_drv_info_t* bdi)
{
	bdbm_dftl_private_t* p = _ftl_dftl.ptr_private;
	uint64_t nr_total_blks = bdbm_abm_get_nr_total_blocks (p->bai);
	uint64_t nr_free_blks = bdbm_abm_get_nr_free_blocks (p->bai);

	/* invoke gc when remaining free blocks are less than 1% of total blocks */
	if ((nr_free_blks * 100 / nr_total_blks) <= 2) {
		if (bdbm_abm_get_nr_dirty_blocks (p->bai) == 0) {
			/* there are no blocks to garbage collect */
			return 0;
		}
		/*
		bdbm_msg ("free_blks: %llu clean_blks: %llu, dirty_blks: %llu, total_blks: %llu",
				nr_free_blks,
				bdbm_abm_get_nr_clean_blocks (p->bai),
				bdbm_abm_get_nr_dirty_blocks (p->bai),
				bdbm_abm_get_nr_total_blocks (p->bai)
				);
		*/
		return 1;
	}

	/* invoke gc when there is only one dirty block (for debugging) */
	/*
	bdbm_dftl_private_t* p = _ftl_dftl.ptr_private;
	if (bdbm_abm_get_nr_dirty_blocks (p->bai) > 1) {
		return 1;
	}
	*/

	return 0;
}

/* VICTIM SELECTION - First Selection:
 * select the first dirty block in a list */
bdbm_abm_block_t* __bdbm_dftl_victim_selection (
	bdbm_drv_info_t* bdi,
	uint64_t channel_no,
	uint64_t chip_no)
{
	bdbm_dftl_private_t* p = _ftl_dftl.ptr_private;
	bdbm_device_params_t* np = BDBM_GET_DEVICE_PARAMS (bdi);
	bdbm_abm_block_t* a = NULL;
	bdbm_abm_block_t* b = NULL;
	struct list_head* pos = NULL;

	a = p->ac_bab[channel_no*np->nr_chips_per_channel + chip_no];
	bdbm_abm_list_for_each_dirty_block (pos, p->bai, channel_no, chip_no) {
		b = bdbm_abm_fetch_dirty_block (pos);
		if (a != b)
			break;
		b = NULL;
	}

	return b;
}

/* VICTIM SELECTION - Greedy:
 * select a dirty block with a small number of valid pages */
bdbm_abm_block_t* __bdbm_dftl_victim_selection_greedy (
	bdbm_drv_info_t* bdi,
	uint64_t channel_no,
	uint64_t chip_no)
{
	bdbm_dftl_private_t* p = _ftl_dftl.ptr_private;
	bdbm_device_params_t* np = BDBM_GET_DEVICE_PARAMS (bdi);
	bdbm_abm_block_t* a = NULL;
	bdbm_abm_block_t* b = NULL;
	bdbm_abm_block_t* v = NULL;
	struct list_head* pos = NULL;

	a = p->ac_bab[channel_no*np->nr_chips_per_channel + chip_no];

	bdbm_abm_list_for_each_dirty_block (pos, p->bai, channel_no, chip_no) {
		b = bdbm_abm_fetch_dirty_block (pos);
		if (a == b)
			continue;
		if (b->nr_invalid_pages == np->nr_pages_per_block) {
			v = b;
			break;
		}
		if (v == NULL) {
			v = b;
			continue;
		}
		if (a->nr_invalid_pages > v->nr_invalid_pages)
			v = b;
	}

	return v;
}

/* TODO: need to improve it for background gc */
uint32_t bdbm_dftl_do_gc (bdbm_drv_info_t* bdi)
{
	bdbm_dftl_private_t* p = _ftl_dftl.ptr_private;
	bdbm_device_params_t* np = BDBM_GET_DEVICE_PARAMS (bdi);
	bdbm_hlm_req_gc_t* hlm_gc = &p->gc_hlm;
	uint64_t nr_gc_blks = 0;
	uint64_t nr_llm_reqs = 0;
	uint64_t nr_punits = 0;
	uint64_t i, j;

	nr_punits = np->nr_channels * np->nr_chips_per_channel;

	/* choose victim blocks for individual parallel units */
	bdbm_memset (p->gc_bab, 0x00, sizeof (bdbm_abm_block_t*) * nr_punits);
	for (i = 0, nr_gc_blks = 0; i < np->nr_channels; i++) {
		for (j = 0; j < np->nr_chips_per_channel; j++) {
			bdbm_abm_block_t* b; 
			if ((b = __bdbm_dftl_victim_selection_greedy (bdi, i, j))) {
				p->gc_bab[nr_gc_blks] = b;
				nr_gc_blks++;
			}
		}
	}
	if (nr_gc_blks < nr_punits) {
		/* TODO: we need to implement a load balancing feature to avoid this */
		/*bdbm_warning ("TODO: this warning will be removed with load-balancing");*/
		return 0;
	}

	/* build hlm_req_gc for reads */
	for (i = 0, nr_llm_reqs = 0; i < nr_gc_blks; i++) {
		bdbm_abm_block_t* b = p->gc_bab[i];
		if (b == NULL)
			break;
		for (j = 0; j < np->nr_pages_per_block; j++) {
			if (b->pst[j] != BDBM_ABM_PAGE_INVALID) {
				bdbm_llm_req_t* r = &hlm_gc->llm_reqs[nr_llm_reqs];
				r->req_type = REQTYPE_GC_READ;
				r->lpa = -1ULL; /* lpa is not available now */
				r->ptr_hlm_req = (void*)hlm_gc;
				r->phyaddr = &r->phyaddr_r;
				r->phyaddr->channel_no = b->channel_no;
				r->phyaddr->chip_no = b->chip_no;
				r->phyaddr->block_no = b->block_no;
				r->phyaddr->page_no = j;
				r->phyaddr->punit_id = BDBM_GET_PUNIT_ID (bdi, r->phyaddr);
				r->ret = 0;
				nr_llm_reqs++;
			}
		}
	}

	/* wait until Q in llm becomes empty 
	 * TODO: it might be possible to further optimize this */
	bdi->ptr_llm_inf->flush (bdi);

	if (nr_llm_reqs == 0) 
		goto erase_blks;

	/* send read reqs to llm */
	hlm_gc->req_type = REQTYPE_GC_READ;
	hlm_gc->nr_done_reqs = 0;
	hlm_gc->nr_reqs = nr_llm_reqs;
	bdbm_sema_lock (&hlm_gc->gc_done);
	for (i = 0; i < nr_llm_reqs; i++) {
		if ((bdi->ptr_llm_inf->make_req (bdi, &hlm_gc->llm_reqs[i])) != 0) {
			bdbm_error ("llm_make_req failed");
			bdbm_bug_on (1);
		}
	}
	bdbm_sema_lock (&hlm_gc->gc_done);
	bdbm_sema_unlock (&hlm_gc->gc_done);

	/* load mapping entries that do existing in DRAM */
	{
		bdbm_llm_req_t** rr = (bdbm_llm_req_t**)bdbm_malloc (sizeof (bdbm_llm_req_t*)*nr_llm_reqs);

		/* FIXME: need to improve to exploit parallelism */
		for (i = 0; i < nr_llm_reqs; i++) {
			uint64_t lpa = ((uint64_t*)hlm_gc->llm_reqs[i].ptr_oob)[0]; /* update LPA */

			/* is it a mapping entry? */
			if ((int64_t)lpa == -2LL) {
				continue;
			}

			if ((int64_t)lpa >= np->nr_pages_per_ssd || (int64_t)lpa < 0) {
				/*bdbm_msg ("what??? %llu", lpa);*/
				continue;
			}

			/* see if lpa exists in DRAM */
			if (bdbm_dftl_check_mapblk (bdi, lpa) == 0) 
				continue;	/* a mapping entry exists, so go to the next req */

			/* load missing maing entries from Flash */
			if ((rr[i] = bdbm_dftl_prepare_mapblk_load (bdi, lpa)) == NULL)
				continue;

			/* send reqs to llm */
			bdbm_sema_lock (rr[i]->done);
			bdi->ptr_llm_inf->make_req (bdi, rr[i]);
		}
		for (i = 0; i < nr_llm_reqs; i++) {
			if (rr[i]) {
				bdbm_sema_lock (rr[i]->done);
				bdbm_dftl_finish_mapblk_load (bdi, rr[i]);
			}
		}

		bdbm_free (rr);
	}

	/* build hlm_req_gc for writes */
	for (i = 0; i < nr_llm_reqs; i++) {
		bdbm_llm_req_t* r = &hlm_gc->llm_reqs[i];
		r->req_type = REQTYPE_GC_WRITE;	/* change to write */
		r->lpa = ((uint64_t*)r->ptr_oob)[0]; /* update LPA */

		if ((int64_t)r->lpa == -2LL) {
			/* This page currently keeps mapping entries;
			 * its phyaddr in DS must be updated */
			int64_t id = ((uint64_t*)r->ptr_oob)[1];
			
			if (bdbm_dftl_get_free_ppa (bdi, r->lpa, r->phyaddr) != 0) {
				bdbm_error ("bdbm_dftl_get_free_ppa failed");
				bdbm_bug_on (1);
			}

			bdbm_dftl_update_dir_phyaddr (p->mt, id, r->phyaddr);
		} else if ((int64_t)r->lpa >= np->nr_pages_per_ssd || (int64_t)r->lpa < 0) {
			/*bdbm_msg ("what??? %llu", r->lpa);*/
		} else {
			if (bdbm_dftl_get_free_ppa (bdi, r->lpa, r->phyaddr) != 0) {
				bdbm_error ("bdbm_dftl_get_free_ppa failed");
				bdbm_bug_on (1);
			}

			if (bdbm_dftl_map_lpa_to_ppa (bdi, r->lpa, r->phyaddr) != 0) {
				bdbm_error ("bdbm_dftl_map_lpa_to_ppa failed");
				bdbm_bug_on (1);
			}
		}
	}

	/* send write reqs to llm */
	hlm_gc->req_type = REQTYPE_GC_WRITE;
	hlm_gc->nr_done_reqs = 0;
	hlm_gc->nr_reqs = nr_llm_reqs;
	bdbm_sema_lock (&hlm_gc->gc_done);
	for (i = 0; i < nr_llm_reqs; i++) {
		bdbm_llm_req_t* r = &hlm_gc->llm_reqs[i];
		if ((int64_t)r->lpa >= np->nr_pages_per_ssd || (int64_t)r->lpa < 0) {
			/*bdbm_msg ("what??? %llu", r->lpa);*/
			hlm_gc->nr_reqs--;
			continue;
		}
		if ((bdi->ptr_llm_inf->make_req (bdi, &hlm_gc->llm_reqs[i])) != 0) {
			bdbm_error ("llm_make_req failed");
			bdbm_bug_on (1);
		}
	}
	/*bdbm_msg ("gc-3");*/
	bdbm_sema_lock (&hlm_gc->gc_done);
	bdbm_sema_unlock (&hlm_gc->gc_done);

	/* erase blocks */
erase_blks:
	for (i = 0; i < nr_gc_blks; i++) {
		bdbm_abm_block_t* b = p->gc_bab[i];
		bdbm_llm_req_t* r = &hlm_gc->llm_reqs[i];
		r->req_type = REQTYPE_GC_ERASE;
		r->lpa = -1ULL; /* lpa is not available now */
		r->ptr_hlm_req = (void*)hlm_gc;
		r->phyaddr = &r->phyaddr_w;
		r->phyaddr->channel_no = b->channel_no;
		r->phyaddr->chip_no = b->chip_no;
		r->phyaddr->block_no = b->block_no;
		r->phyaddr->page_no = 0;
		r->phyaddr->punit_id = BDBM_GET_PUNIT_ID (bdi, r->phyaddr);
		r->ret = 0;
	}

	/* send erase reqs to llm */
	hlm_gc->req_type = REQTYPE_GC_ERASE;
	hlm_gc->nr_done_reqs = 0;
	hlm_gc->nr_reqs = nr_gc_blks;
	bdbm_sema_lock (&hlm_gc->gc_done);
	for (i = 0; i < nr_gc_blks; i++) {
		if ((bdi->ptr_llm_inf->make_req (bdi, &hlm_gc->llm_reqs[i])) != 0) {
			bdbm_error ("llm_make_req failed");
			bdbm_bug_on (1);
		}
	}
	/*bdbm_msg ("gc-5");*/
	bdbm_sema_lock (&hlm_gc->gc_done);
	bdbm_sema_unlock (&hlm_gc->gc_done);

	/* FIXME: what happens if block erasure fails */
	for (i = 0; i < nr_gc_blks; i++) {
		uint8_t ret = 0;
		bdbm_abm_block_t* b = p->gc_bab[i];
		if (hlm_gc->llm_reqs[i].ret != 0) 
			ret = 1;	/* bad block */
		bdbm_abm_erase_block (p->bai, b->channel_no, b->chip_no, b->block_no, ret);
	}

	return 0;
}

/* for snapshot */
uint32_t bdbm_dftl_load (bdbm_drv_info_t* bdi, const char* fn)
{
	bdbm_warning ("bdbm_dftl_load is not implemented yet");
	return 1;
}

uint32_t bdbm_dftl_store (bdbm_drv_info_t* bdi, const char* fn)
{
	bdbm_warning ("bdbm_dftl_store is not implemented yet");
	return 1;
}

static void __bdbm_dftl_badblock_scan_eraseblks (
	bdbm_drv_info_t* bdi,
	uint64_t block_no)
{
	bdbm_dftl_private_t* p = _ftl_dftl.ptr_private;
	bdbm_device_params_t* np = BDBM_GET_DEVICE_PARAMS (bdi);
	bdbm_hlm_req_gc_t* hlm_gc = &p->gc_hlm;
	uint64_t i, j;

	/* setup blocks to erase */
	bdbm_memset (p->gc_bab, 0x00, sizeof (bdbm_abm_block_t*) * p->nr_punits);
	for (i = 0; i < np->nr_channels; i++) {
		for (j = 0; j < np->nr_chips_per_channel; j++) {
			bdbm_abm_block_t* b = NULL;
			bdbm_llm_req_t* r = NULL;
			uint64_t punit_id = i*np->nr_chips_per_channel+j;

			if ((b = bdbm_abm_get_block (p->bai, i, j, block_no)) == NULL) {
				bdbm_error ("oops! bdbm_abm_get_block failed");
				bdbm_bug_on (1);
			}
			p->gc_bab[punit_id] = b;

			r = &hlm_gc->llm_reqs[punit_id];
			r->req_type = REQTYPE_GC_ERASE;
			r->lpa = -1ULL; /* lpa is not available now */
			r->ptr_hlm_req = (void*)hlm_gc;
			r->phyaddr = &r->phyaddr_w;
			r->phyaddr->channel_no = b->channel_no;
			r->phyaddr->chip_no = b->chip_no;
			r->phyaddr->block_no = b->block_no;
			r->phyaddr->page_no = 0;
			r->phyaddr->punit_id = BDBM_GET_PUNIT_ID (bdi, r->phyaddr);
			r->ret = 0;
		}
	}

	/* send erase reqs to llm */
	hlm_gc->req_type = REQTYPE_GC_ERASE;
	hlm_gc->nr_done_reqs = 0;
	hlm_gc->nr_reqs = p->nr_punits;
	bdbm_sema_lock (&hlm_gc->gc_done);
	for (i = 0; i < p->nr_punits; i++) {
		if ((bdi->ptr_llm_inf->make_req (bdi, &hlm_gc->llm_reqs[i])) != 0) {
			bdbm_error ("llm_make_req failed");
			bdbm_bug_on (1);
		}
	}
	bdbm_sema_lock (&hlm_gc->gc_done);
	bdbm_sema_unlock (&hlm_gc->gc_done);

	for (i = 0; i < p->nr_punits; i++) {
		uint8_t ret = 0;
		bdbm_abm_block_t* b = p->gc_bab[i];

		if (hlm_gc->llm_reqs[i].ret != 0) {
			ret = 1; /* bad block */
		}

		bdbm_abm_erase_block (p->bai, b->channel_no, b->chip_no, b->block_no, ret);
	}

	/* measure gc elapsed time */
}

static void __bdbm_dftl_mark_it_dead (
	bdbm_drv_info_t* bdi,
	uint64_t block_no)
{
	bdbm_dftl_private_t* p = _ftl_dftl.ptr_private;
	bdbm_device_params_t* np = BDBM_GET_DEVICE_PARAMS (bdi);
	int i, j;

	for (i = 0; i < np->nr_channels; i++) {
		for (j = 0; j < np->nr_chips_per_channel; j++) {
			bdbm_abm_block_t* b = NULL;

			if ((b = bdbm_abm_get_block (p->bai, i, j, block_no)) == NULL) {
				bdbm_error ("oops! bdbm_abm_get_block failed");
				bdbm_bug_on (1);
			}

			bdbm_abm_set_to_dirty_block (p->bai, i, j, block_no);
		}
	}
}


uint32_t bdbm_dftl_badblock_scan (bdbm_drv_info_t* bdi)
{
	/*#if 0*/
	bdbm_dftl_private_t* p = _ftl_dftl.ptr_private;
	bdbm_device_params_t* np = BDBM_GET_DEVICE_PARAMS (bdi);
	uint64_t i = 0;
	uint32_t ret = 0;

	bdbm_msg ("[WARNING] 'bdbm_page_badblock_scan' is called! All of the flash blocks will be erased!!!");

	/* step1: reset the page-level mapping table */
	bdbm_msg ("step1: reset the page-level mapping table");
	bdbm_dftl_init_mapping_table (p->mt, np);

	/* step2: erase all the blocks */
	bdi->ptr_llm_inf->flush (bdi);
	for (i = 0; i < np->nr_blocks_per_chip; i++) {
		__bdbm_dftl_badblock_scan_eraseblks (bdi, i);
	}

	/* step3: store abm */
	if ((ret = bdbm_abm_store (p->bai, "/usr/share/bdbm_drv/abm.dat"))) {
		bdbm_error ("bdbm_abm_store failed");
		return 1;
	}

	/* step4: get active blocks */
	bdbm_msg ("step2: get active blocks");
	if (__bdbm_dftl_get_active_blocks (np, p->bai, p->ac_bab) != 0) {
		bdbm_error ("__bdbm_dftl_get_active_blocks failed");
		return 1;
	}
	p->curr_puid = 0;
	p->curr_page_ofs = 0;

	bdbm_msg ("done");
	 
	return 0;
	/*#endif*/

#if 0
	/* TEMP: on-demand format */
	bdbm_dftl_private_t* p = _ftl_dftl.ptr_private;
	bdbm_device_params_t* np = BDBM_GET_DEVICE_PARAMS (bdi);
	uint64_t i = 0;
	uint32_t ret = 0;
	uint32_t erased_blocks = 0;

	bdbm_msg ("[WARNING] 'bdbm_page_badblock_scan' is called! All of the flash blocks will be dirty!!!");

	/* step1: reset the page-level mapping table */
	bdbm_msg ("step1: reset the page-level mapping table");
	bdbm_dftl_init_mapping_table (p->mt, np);

	/* step2: erase all the blocks */
	bdi->ptr_llm_inf->flush (bdi);
	for (i = 0; i < np->nr_blocks_per_chip; i++) {
		if (erased_blocks <= p->nr_punits)
			__bdbm_dftl_badblock_scan_eraseblks (bdi, i);
		else
			__bdbm_dftl_mark_it_dead (bdi, i);
		erased_blocks += np->nr_channels;
	}

	/* step3: store abm */
	if ((ret = bdbm_abm_store (p->bai, "/usr/share/bdbm_drv/abm.dat"))) {
		bdbm_error ("bdbm_abm_store failed");
		return 1;
	}

	/* step4: get active blocks */
	bdbm_msg ("step2: get active blocks");
	if (__bdbm_dftl_get_active_blocks (np, p->bai, p->ac_bab) != 0) {
		bdbm_error ("__bdbm_dftl_get_active_blocks failed");
		return 1;
	}
	p->curr_puid = 0;
	p->curr_page_ofs = 0;

	bdbm_msg ("[summary] Total: %llu, Free: %llu, Clean: %llu, Dirty: %llu",
		bdbm_abm_get_nr_total_blocks (p->bai),
		bdbm_abm_get_nr_free_blocks (p->bai),
		bdbm_abm_get_nr_clean_blocks (p->bai),
		bdbm_abm_get_nr_dirty_blocks (p->bai)
	);
#endif
	bdbm_msg ("done");
	 
	return 0;

}

/* for mapping blocks management */
uint8_t bdbm_dftl_check_mapblk (
	bdbm_drv_info_t* bdi,
	uint64_t lpa)
{
	bdbm_dftl_private_t* p = (bdbm_dftl_private_t*)BDBM_FTL_PRIV (bdi);

	return bdbm_dftl_check_mapping_entry (p->mt, lpa);
}

bdbm_llm_req_t* bdbm_dftl_prepare_mapblk_load (
	bdbm_drv_info_t* bdi,
	uint64_t lpa)
{
	bdbm_dftl_private_t* p = (bdbm_dftl_private_t*)BDBM_FTL_PRIV (bdi);
	bdbm_device_params_t* np = BDBM_GET_DEVICE_PARAMS (bdi);
	mapping_entry_t* me = NULL;
	directory_slot_t* ds = NULL;
	bdbm_llm_req_t* r = NULL;
	uint32_t nr_kp_per_fp = np->page_main_size / KERNEL_PAGE_SIZE;
	uint32_t i;

	/* is there a victim mapblk to evict to flash */
	if ((ds = bdbm_dftl_missing_dir_prepare (p->mt, lpa)) == NULL) {
		/* there are enough space to keep in-memory mapping entries */
		return NULL;
	}

	/* create a hlm_req that stores mapping entries */
	r = (bdbm_llm_req_t*)bdbm_malloc(sizeof (bdbm_llm_req_t));
	r->pptr_kpgs = (uint8_t**)bdbm_malloc(sizeof (uint8_t*) * nr_kp_per_fp);
	me = (mapping_entry_t*)bdbm_malloc(
			(sizeof (mapping_entry_t) * p->mt->nr_entires_per_dir_slot));
	bdbm_bug_on ((sizeof (mapping_entry_t) * p->mt->nr_entires_per_dir_slot) != 4096);
	for (i = 0; i < nr_kp_per_fp; i++)
		r->pptr_kpgs[i] = (uint8_t*)(me) + (i * KERNEL_PAGE_SIZE);

	/* build the parameters of the hlm_req */
	r->req_type = REQTYPE_META_READ;
	r->lpa = -2ULL;	/* not available for mapblks */
	r->phyaddr_r = ds->phyaddr;
	r->phyaddr_w = ds->phyaddr;
	r->phyaddr = &r->phyaddr_r;
	r->ds = (void*)ds;
	r->ptr_hlm_req = (void*)NULL;
	r->ptr_oob = (uint8_t*)bdbm_malloc(sizeof (uint8_t) * np->page_oob_size);

	r->done = NULL;
	r->done = (bdbm_sema_t*)bdbm_malloc(sizeof (bdbm_sema_t));
	bdbm_sema_init (r->done);

#ifdef DFTL_DEBUG
	bdbm_msg ("[dftl] [Fetch] lpa: %llu dir: %llu (phyaddr: %llu %lld %lld %lld %lld)", 
		lpa,
		ds->id,
		ds->phyaddr.punit_id,
		ds->phyaddr.channel_no,
		ds->phyaddr.chip_no,
		ds->phyaddr.block_no,
		ds->phyaddr.page_no);
#endif
	/* ok! return it */
	return r;
}

void bdbm_dftl_finish_mapblk_load (
	bdbm_drv_info_t* bdi, 
	bdbm_llm_req_t* r)
{
	bdbm_dftl_private_t* p = (bdbm_dftl_private_t*)BDBM_FTL_PRIV (bdi);
	directory_slot_t* ds = (directory_slot_t*)r->ds;
	mapping_entry_t* me = NULL;

	/* copy mapping entries to ds */
	me = (mapping_entry_t*)r->pptr_kpgs[0];

	if (((int64_t*)r->ptr_oob)[0] != -2LL) {
		/*
		bdbm_msg ("---------------------------------------------------------------------");
		bdbm_warning ("oob is not match: %lld", ((int64_t*)r->ptr_oob)[0]);
		bdbm_warning ("lpa: %lld", r->lpa);
		bdbm_warning ("dir: %llu (phyaddr: %llu %lld %lld %lld %lld)", 
			ds->id,
			ds->phyaddr.punit_id,
			ds->phyaddr.channel_no,
			ds->phyaddr.chip_no,
			ds->phyaddr.block_no,
			ds->phyaddr.page_no);
		bdbm_warning ("flash-dir: %lld", ((int64_t*)r->ptr_oob)[1]);
		bdbm_msg ("---------------------------------------------------------------------");
		*/

		bdbm_dftl_missing_dir_done_error (p->mt, ds, me);
	} else {
		/* finish the load */
		bdbm_dftl_missing_dir_done (p->mt, ds, me);
	}

	/* remove a llm_req */
	bdbm_free(r->ptr_oob);
	bdbm_free(r->done);
	bdbm_free(me); /* free an array of mapblks */
	bdbm_free(r->pptr_kpgs); /* free pptr_kpgs */
	bdbm_free(r);

#ifdef DFTL_DEBUG
	bdbm_msg ("[dftl] [Fetch] dir: %llu (done)\n", ds->id);
#endif
}

bdbm_llm_req_t* bdbm_dftl_prepare_mapblk_eviction (
	bdbm_drv_info_t* bdi)
{
	bdbm_dftl_private_t* p = (bdbm_dftl_private_t*)BDBM_FTL_PRIV (bdi);
	bdbm_device_params_t* np = BDBM_GET_DEVICE_PARAMS (bdi);
	mapping_entry_t* me = NULL;
	directory_slot_t* ds = NULL;
	bdbm_llm_req_t* r = NULL;
	uint32_t nr_kp_per_fp = np->page_main_size / KERNEL_PAGE_SIZE;
	uint32_t i;

	/* is there a victim mapblk to evict to flash */
	if ((ds = bdbm_dftl_prepare_victim_mapblk (p->mt)) == NULL) {
		/* there are enough space to keep in-memory mapping entries */
		return NULL;
	}

	/* create a hlm_req that stores mapping entries */
	r = (bdbm_llm_req_t*)bdbm_malloc(sizeof (bdbm_llm_req_t));
	r->pptr_kpgs = (uint8_t**)bdbm_malloc(sizeof (uint8_t*) * nr_kp_per_fp);
	me = (mapping_entry_t*)bdbm_malloc(
			(sizeof (mapping_entry_t) * p->mt->nr_entires_per_dir_slot));
	bdbm_bug_on ((sizeof (mapping_entry_t) * p->mt->nr_entires_per_dir_slot) != 4096);
	for (i = 0; i < nr_kp_per_fp; i++)
		r->pptr_kpgs[i] = (uint8_t*)(me) + (i * KERNEL_PAGE_SIZE);

	/* build the parameters of the hlm_req */
	r->req_type = REQTYPE_META_WRITE;
	r->lpa = -2ULL;	/* not available for me */
	r->phyaddr = &r->phyaddr_w;
	r->ds = (void*)ds;
	if (ds->status != DFTL_DIR_CLEAN) {
		bdbm_dftl_get_free_ppa (bdi, r->lpa, r->phyaddr); /* get a new page */
	} else {
		/* if ds->status is not dirty, 
		 * we don't need to write it to NAND flash */
	}
	r->phyaddr_r = r->phyaddr_w;
	for (i = 0; i < p->mt->nr_entires_per_dir_slot; i++)
		me[i] = ds->me[i];
	r->ptr_oob = (uint8_t*)bdbm_malloc(np->page_oob_size);
	r->ptr_hlm_req = (void*)NULL;
	((int64_t*)r->ptr_oob)[0] = -2LL; /* magic # */
	((int64_t*)r->ptr_oob)[1] = ds->id; /* ds ID */

	r->done = NULL;
	r->done = (bdbm_sema_t*)bdbm_malloc(sizeof (bdbm_sema_t));
	bdbm_sema_init (r->done);

#ifdef DFTL_DEBUG
	if (ds->status != DFTL_DIR_CLEAN) {
		bdbm_msg ("[dftl] [Evict] dir: %llu (phyaddr: %llu %lld %lld %lld %lld)", 
			ds->id,
			r->phyaddr->punit_id,
			r->phyaddr->channel_no,
			r->phyaddr->chip_no,
			r->phyaddr->block_no,
			r->phyaddr->page_no);
	}
#endif
	/* ok! return it */
	return r;
}

void bdbm_dftl_finish_mapblk_eviction (
	bdbm_drv_info_t* bdi, 
	bdbm_llm_req_t* r)
{
	bdbm_dftl_private_t* p = (bdbm_dftl_private_t*)BDBM_FTL_PRIV (bdi);
	directory_slot_t* ds = (directory_slot_t*)r->ds;
	mapping_entry_t* me = (mapping_entry_t*)r->pptr_kpgs[0];

	/* invalidate an old page if ds was kept in flash before */
	if (ds->status != DFTL_DIR_CLEAN) {
		if (ds->phyaddr.channel_no != DFTL_PAGE_INVALID_ADDR) {
#ifdef DFTL_DEBUG
			bdbm_msg ("[dftl] [Evict] dir: %llu (invalidate: %lld %lld %lld %lld)", 
				ds->id,
				ds->phyaddr.channel_no, 
				ds->phyaddr.chip_no,
				ds->phyaddr.block_no,
				ds->phyaddr.page_no);
#endif
			bdbm_abm_invalidate_page (
				p->bai, 
				ds->phyaddr.channel_no, 
				ds->phyaddr.chip_no,
				ds->phyaddr.block_no,
				ds->phyaddr.page_no
			);
		}
	}

	/* finish the eviction */
	bdbm_dftl_finish_victim_mapblk (p->mt, ds, r->phyaddr);

	/* remove a llm_req */
	bdbm_free(r->done);
	bdbm_free(r->ptr_oob);
	bdbm_free(me);
	bdbm_free(r->pptr_kpgs); /* free pptr_kpgs */
	bdbm_free(r);

#ifdef DFTL_DEBUG
	bdbm_msg ("[dftl] [Evict] dir: %llu (done)\n", ds->id);
#endif
}
