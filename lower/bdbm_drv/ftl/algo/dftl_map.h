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

#ifndef __FTL_DFTL_MAP_H
#define __FTL_DFTL_MAP_H

/* data structures for DFTL */
typedef enum {
	DFTL_DIR_EMPTY = 0,
	DFTL_DIR_FLASH = 0x100,
	DFTL_DIR_DRAM = 0x200,
	DFTL_DIR_CLEAN = DFTL_DIR_DRAM | 0x1,
	DFTL_DIR_DIRTY = DFTL_DIR_DRAM | 0x2,
} dir_stat;

enum BDBM_DFTL_PAGE_STATUS {
	DFTL_PAGE_NOT_EXIST = 0,
	DFTL_PAGE_NOT_MAPPED,
	DFTL_PAGE_VALID,
	DFTL_PAGE_INVALID,
	DFTL_PAGE_INVALID_ADDR = -1,
};

typedef struct {
	uint8_t channel_no;
	uint8_t chip_no;
	uint64_t block_no;
	uint64_t page_no;
} mapblk_phyaddr_t;

typedef struct {
	uint8_t status;
	mapblk_phyaddr_t phyaddr; /* physical location */
} mapping_entry_t;

typedef struct {
	/* linked-list: to quickly find a victim for eviction */
	struct list_head list;
	uint64_t id;
	dir_stat status;
	bdbm_phyaddr_t phyaddr;	/* the physical location where mapping entries are stored */
	mapping_entry_t* me;	/* the size of me is equal to a single flash size */

	uint32_t is_under_load;
} directory_slot_t;

typedef struct {
	struct list_head lru_list; /* dirty-list header */
	uint64_t mapping_entry_size;
	uint64_t nr_entires_per_dir_slot;
	uint64_t nr_total_dir_slots;
	uint64_t max_cached_dir_slots;
	atomic64_t nr_cached_slots;
	directory_slot_t* dir;	/* always maintained in DRAM */
} dftl_mapping_table_t;


dftl_mapping_table_t* bdbm_dftl_create_mapping_table (bdbm_device_params_t* np);
void bdbm_dftl_destroy_mapping_table (dftl_mapping_table_t* mt);
void bdbm_dftl_init_mapping_table (dftl_mapping_table_t* mt, bdbm_device_params_t* np);

/* management of mapping entres */
mapping_entry_t bdbm_dftl_get_mapping_entry (dftl_mapping_table_t* mt, uint64_t lpa);
int bdbm_dftl_set_mapping_entry (dftl_mapping_table_t* mt, uint64_t lpa, mapping_entry_t* me);
int bdbm_dftl_invalidate_mapping_entry (dftl_mapping_table_t* mt, uint64_t lpa);

/* management of directory slots */
int bdbm_dftl_check_mapping_entry (dftl_mapping_table_t* mt, uint64_t lpa);
directory_slot_t* 
bdbm_dftl_prepare_victim_mapblk (dftl_mapping_table_t* mt);

void 
bdbm_dftl_finish_victim_mapblk (dftl_mapping_table_t* mt, directory_slot_t* ds, bdbm_phyaddr_t* phyaddr);

directory_slot_t* 
bdbm_dftl_missing_dir_prepare (dftl_mapping_table_t* mt, uint64_t lpa);

int 
bdbm_dftl_missing_dir_done (dftl_mapping_table_t* mt, directory_slot_t* ds, mapping_entry_t* me);

void bdbm_dftl_update_dir_phyaddr (
	dftl_mapping_table_t* mt, 
	uint64_t ds_id,
	bdbm_phyaddr_t* phyaddr);

int 
bdbm_dftl_missing_dir_done_error (
	dftl_mapping_table_t* mt, 
	directory_slot_t* ds,
	mapping_entry_t* me);


#endif
