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
#include "algo/dftl_map.h"


dftl_mapping_table_t* bdbm_dftl_create_mapping_table (bdbm_device_params_t* np)
{
	dftl_mapping_table_t* mt = NULL;
	uint64_t i;

	/* create a mapping tables */
	if ((mt = (dftl_mapping_table_t*)bdbm_zmalloc
			(sizeof (dftl_mapping_table_t))) == NULL) {
		return NULL;
	}
	INIT_LIST_HEAD (&mt->lru_list);
	mt->mapping_entry_size = sizeof (mapping_entry_t);
	mt->nr_entires_per_dir_slot = np->page_main_size / mt->mapping_entry_size;
	mt->nr_total_dir_slots = np->nr_pages_per_ssd / mt->nr_entires_per_dir_slot;
	/*mt->max_cached_dir_slots = 20000;*/
	/*mt->max_cached_dir_slots = 20;*/
	mt->max_cached_dir_slots = mt->nr_total_dir_slots * 0.2;
	atomic64_set (&mt->nr_cached_slots, 0);

	bdbm_msg ("DFTL: mapping_entry_size: %llu", mt->mapping_entry_size);
	bdbm_msg ("DFTL: nr_entires_per_dir_slot: %llu", mt->nr_entires_per_dir_slot);
	bdbm_msg ("DFTL: nr_total_dir_slots: %llu", mt->nr_total_dir_slots);
	bdbm_msg ("DFTL: # of cached dir slots: %llu", mt->max_cached_dir_slots);

	/* create a directory */
	if ((mt->dir = (directory_slot_t*)bdbm_zmalloc (
			sizeof (directory_slot_t) * mt->nr_total_dir_slots)) == NULL) {
		return NULL;
	}

	/* initialize directory slots */
	for (i = 0; i < mt->nr_total_dir_slots; i++) {
		directory_slot_t* ds = &mt->dir[i];

		ds->id = i;
		ds->status = DFTL_DIR_EMPTY;
		ds->is_under_load = 0;
		ds->phyaddr.channel_no = DFTL_PAGE_INVALID_ADDR;
		ds->phyaddr.chip_no = DFTL_PAGE_INVALID_ADDR;
		ds->phyaddr.block_no = DFTL_PAGE_INVALID_ADDR;
		ds->phyaddr.page_no = DFTL_PAGE_INVALID_ADDR;
		ds->me = NULL;

#if 0
		ds->me = (mapping_entry_t*)bdbm_malloc_atomic
			(sizeof (mapping_entry_t) * mt->nr_entires_per_dir_slot);
		bdbm_bug_on (ds->me == NULL);

		/* initialize all the entries */
		for (j = 0; j < mt->nr_entires_per_dir_slot; j++) {
			ds->me[j].status = DFTL_PAGE_NOT_MAPPED;
			ds->me[j].phyaddr.channel_no = DFTL_PAGE_INVALID_ADDR;
			ds->me[j].phyaddr.chip_no = DFTL_PAGE_INVALID_ADDR;
			ds->me[j].phyaddr.block_no = DFTL_PAGE_INVALID_ADDR;
			ds->me[j].phyaddr.page_no = DFTL_PAGE_INVALID_ADDR;
		}
		ds->status = DFTL_DIR_CLEAN;
		
		/* add the directory slot to the tail of the dirty linked-list */
		atomic64_inc (&mt->nr_cached_slots);
		list_add_tail (&ds->list, &mt->lru_list);
		/******/
#endif
	}

	/*bdbm_msg ("# of me = %lld", atomic64_read (&mt->nr_cached_slots));*/

	return mt;
}

void bdbm_dftl_destroy_mapping_table (dftl_mapping_table_t* mt)
{
	struct list_head* next, *temp;
	int i = 0;

	/* empty dirty list */
	list_for_each_safe (next, temp, &mt->lru_list) {
		directory_slot_t* ds = NULL;
		ds = list_entry (next, directory_slot_t, list);
		list_del (&ds->list);
	}

	/* remove directories */
	if (mt->dir) {
		for (i = 0; i < mt->nr_total_dir_slots; i++)
			if (mt->dir[i].me != NULL)
				bdbm_free(mt->dir[i].me);
		bdbm_free (mt->dir);
	}
	bdbm_free (mt);
}

void bdbm_dftl_init_mapping_table (dftl_mapping_table_t* mt, bdbm_device_params_t* np)
{
	uint64_t i = 0;
	struct list_head* next, *temp;

	/* initialize mapping table */
	for (i = 0; i < mt->nr_total_dir_slots; i++) {
		directory_slot_t* ds = &mt->dir[i];

		ds->id = i;
		ds->status = DFTL_DIR_EMPTY;
		ds->is_under_load = 0;
		ds->phyaddr.channel_no = DFTL_PAGE_INVALID_ADDR;
		ds->phyaddr.chip_no = DFTL_PAGE_INVALID_ADDR;
		ds->phyaddr.block_no = DFTL_PAGE_INVALID_ADDR;
		ds->phyaddr.page_no = DFTL_PAGE_INVALID_ADDR;
		if (ds->me != NULL)
			bdbm_free(ds->me);
		ds->me = NULL;
	}

	/* empty dirty list */
	list_for_each_safe (next, temp, &mt->lru_list) {
		directory_slot_t* ds = NULL;
		ds = list_entry (next, directory_slot_t, list);
		list_del (&ds->list);
	}
}

mapping_entry_t bdbm_dftl_get_mapping_entry (dftl_mapping_table_t* mt, uint64_t lpa)
{
	uint64_t dir_idx = lpa / mt->nr_entires_per_dir_slot;
	uint64_t map_idx = lpa % mt->nr_entires_per_dir_slot;
	directory_slot_t* ds = NULL;
	mapping_entry_t me;

	bdbm_bug_on (dir_idx >= mt->nr_total_dir_slots);
	bdbm_bug_on (map_idx >= mt->nr_entires_per_dir_slot);

	/* get a directory slot */
	ds = &mt->dir[dir_idx];
	bdbm_bug_on (ds == NULL);

	/* see if mapping entries are already loaded in DRAM */
	/*if (ds->me != NULL) {*/
	if (ds->status == DFTL_DIR_DIRTY || 
		ds->status == DFTL_DIR_CLEAN) {
		/* get the mapping entry */
		me = ds->me[map_idx];
		goto found;
	}

	me.status = DFTL_PAGE_NOT_EXIST;

	/* NOTE: this is an error case */
#if 0
	bdbm_warning ("oops! a mapping entry was not found!!!\
		lpa = %llu, ds id: %llu", lpa, dir_idx);
#endif

found:
	return me;
}

int bdbm_dftl_set_mapping_entry (dftl_mapping_table_t* mt, uint64_t lpa, mapping_entry_t* me)
{
	uint64_t dir_idx = lpa / mt->nr_entires_per_dir_slot;
	uint64_t map_idx = lpa % mt->nr_entires_per_dir_slot;
	directory_slot_t* ds = NULL;

	bdbm_bug_on (dir_idx >= mt->nr_total_dir_slots);
	bdbm_bug_on (map_idx >= mt->nr_entires_per_dir_slot);

	/* get a directory slot */
	ds = &mt->dir[dir_idx];
	bdbm_bug_on (ds == NULL);
	bdbm_bug_on (ds->me == NULL);

	/* update the mapping entry */
	ds->me[map_idx] = *me;
	ds->status = DFTL_DIR_DIRTY;

	/* the directory slot is moved to the tail */
	list_del (&ds->list);
	list_add_tail (&ds->list, &mt->lru_list);

	return 0;
}

int bdbm_dftl_invalidate_mapping_entry (dftl_mapping_table_t* mt, uint64_t lpa)
{
	uint64_t dir_idx = lpa / mt->nr_entires_per_dir_slot;
	uint64_t map_idx = lpa % mt->nr_entires_per_dir_slot;
	directory_slot_t* ds = NULL;

	bdbm_bug_on (dir_idx >= mt->nr_total_dir_slots);
	bdbm_bug_on (map_idx >= mt->nr_entires_per_dir_slot);

	/* get a directory slot */
	ds = &mt->dir[dir_idx];
	bdbm_bug_on (ds == NULL);
	bdbm_bug_on (ds->me == NULL);

	/* update the mapping entry */
	ds->me[map_idx].status = DFTL_PAGE_INVALID;
	ds->status = DFTL_DIR_DIRTY;

	return 0;
}

int bdbm_dftl_check_mapping_entry (
	dftl_mapping_table_t* mt, 
	uint64_t lpa)
{
	directory_slot_t* ds = NULL;

	/* get a directory slot */
	ds = &mt->dir[lpa/mt->nr_entires_per_dir_slot];
	/*bdbm_bug_on (lpa/mt->nr_entires_per_dir_slot >= mt->nr_total_dir_slots);*/
	if (lpa/mt->nr_entires_per_dir_slot >= mt->nr_total_dir_slots) {
		bdbm_msg ("%llu (%llu) %llu",
			lpa/mt->nr_entires_per_dir_slot, lpa, mt->nr_total_dir_slots);
		bdbm_bug_on (1);
	}
	bdbm_bug_on (ds == NULL);

	if (ds->status == DFTL_DIR_EMPTY || 
		ds->status == DFTL_DIR_FLASH) {
		/* a mapping entry is not available */
		return 1; 
	}

	/* a mapping entry is available */
	return 0; 
}

directory_slot_t* bdbm_dftl_missing_dir_prepare (
	dftl_mapping_table_t* mt,
	uint64_t lpa)
{
	directory_slot_t* ds = NULL;

	ds = &mt->dir[lpa/mt->nr_entires_per_dir_slot];
	bdbm_bug_on (lpa/mt->nr_entires_per_dir_slot >= mt->nr_total_dir_slots);

	/* check error cases */
	bdbm_bug_on (ds == NULL);
	/*bdbm_bug_on (ds->me != NULL);*/
	if (ds->status != DFTL_DIR_FLASH && 
		ds->status != DFTL_DIR_EMPTY) {
		bdbm_bug_on (1);
	}

	if (ds->status == DFTL_DIR_EMPTY) {
		int j = 0;

		/* this directory slot is not written before */
		ds->me = (mapping_entry_t*)bdbm_malloc
			(sizeof (mapping_entry_t) * mt->nr_entires_per_dir_slot);
		bdbm_bug_on (ds->me == NULL);

		/* initialize all the entries */
		for (j = 0; j < mt->nr_entires_per_dir_slot; j++) {
			ds->me[j].status = DFTL_PAGE_NOT_MAPPED;
			ds->me[j].phyaddr.channel_no = DFTL_PAGE_INVALID_ADDR;
			ds->me[j].phyaddr.chip_no = DFTL_PAGE_INVALID_ADDR;
			ds->me[j].phyaddr.block_no = DFTL_PAGE_INVALID_ADDR;
			ds->me[j].phyaddr.page_no = DFTL_PAGE_INVALID_ADDR;
		}
		ds->status = DFTL_DIR_DIRTY; /* this table is newly created, so it starts with dirty */

		/* add the directory slot to the tail of the dirty linked-list */
		atomic64_inc (&mt->nr_cached_slots);
		list_add_tail (&ds->list, &mt->lru_list);

		return NULL;
	}

	if (ds->is_under_load == 1)
		return NULL;

	ds->is_under_load = 1;

	return ds;
}

int bdbm_dftl_missing_dir_done (
	dftl_mapping_table_t* mt, 
	directory_slot_t* ds,
	mapping_entry_t* me)
{
	uint32_t i;

	/* build mapping entires for ds */
	if (ds->me == NULL) {
		bdbm_bug_on (ds->status != DFTL_DIR_EMPTY);
		ds->me = (mapping_entry_t*)bdbm_malloc
			(sizeof (mapping_entry_t) * mt->nr_entires_per_dir_slot);
	}

	for (i = 0; i < mt->nr_entires_per_dir_slot; i++) {
		ds->me[i] = me[i];
	}

	/* NOTE: initially, the status of ds is clean even if it has invalid pages.
	 * It becomes dirty only when its mapping entries are updated.  */
	bdbm_bug_on (ds->phyaddr.channel_no == DFTL_PAGE_INVALID_ADDR);
	ds->status = DFTL_DIR_CLEAN;
	ds->is_under_load = 0;

	atomic64_inc (&mt->nr_cached_slots);
	list_add_tail (&ds->list, &mt->lru_list);

	return 0;
}

int bdbm_dftl_missing_dir_done_error (
	dftl_mapping_table_t* mt, 
	directory_slot_t* ds,
	mapping_entry_t* me)
{
	uint32_t i;

	bdbm_bug_on (ds->status == DFTL_DIR_EMPTY);

	if (ds->status == DFTL_DIR_FLASH) {
		atomic64_inc (&mt->nr_cached_slots);
		list_add_tail (&ds->list, &mt->lru_list);
		ds->status = DFTL_DIR_CLEAN;
	}
	ds->is_under_load = 0;

	return 0;
}

directory_slot_t* bdbm_dftl_prepare_victim_mapblk (
	dftl_mapping_table_t* mt)
{
	directory_slot_t* ds = NULL;
	struct list_head* pos = NULL;
	uint64_t nr_slots = 0;

	/* get the number of slots kept in DRAM */
	nr_slots = atomic64_read (&mt->nr_cached_slots);
	if (nr_slots < mt->max_cached_dir_slots) {
		return NULL;
	}

	/* get a victim dir from lru-list */
	list_for_each (pos, &mt->lru_list) {
		ds = list_entry (pos, directory_slot_t, list);
		bdbm_bug_on (ds == NULL);
		break;
	}

	/* temp */
	list_del (&ds->list);
	atomic64_dec (&mt->nr_cached_slots);
	/* end */

	return ds;
}

void bdbm_dftl_finish_victim_mapblk (
	dftl_mapping_table_t* mt, 
	directory_slot_t* ds,
	bdbm_phyaddr_t* phyaddr)
{
	/* update a directory slot */
	if (ds->status != DFTL_DIR_CLEAN) {
		ds->phyaddr = *phyaddr;
	}

	ds->status = DFTL_DIR_FLASH;
#ifdef REMOVE_ME
	bdbm_free(ds->me);	
	ds->me = NULL;
#endif

	/*list_del (&ds->list);*/
	/*atomic64_dec (&mt->nr_cached_slots);*/
}

void bdbm_dftl_update_dir_phyaddr (
	dftl_mapping_table_t* mt, 
	uint64_t ds_id,
	bdbm_phyaddr_t* phyaddr)
{
	directory_slot_t* ds = &mt->dir[ds_id];
	bdbm_bug_on (ds_id>= mt->nr_total_dir_slots);
	bdbm_bug_on (ds == NULL);
	ds->phyaddr = *phyaddr;
}

