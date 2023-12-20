#ifndef __PAGE_MANAGER_H__
#define __PAGE_MANAGER_H__
#include "../../include/settings.h"
#include "../../include/container.h"
#include "write_buffer.h"
#include "sst_file.h"
#include <stdint.h>
#include <map>
#include <queue>

#define MAX_MAP (PAGESIZE/sizeof(uint32_t))
#define BLOCK_MAP_SIZE (_PPB*sizeof(uint32_t))
#define BLOCK_PER_MAP_NUM (BLOCK_MAP_SIZE/PAGESIZE+(BLOCK_MAP_SIZE%PAGESIZE?1:0))
#define PIECETOPPA(a) ((a)/L2PGAP)

enum{
	SEPDATASEG, DATASEG, MAPSEG
};

typedef struct page_manager{
	uint8_t seg_type_checker[_NOS];
	bool is_master_page_manager;
	__segment *temp_data_segment;
	__segment *current_segment[PARTNUM];
	__segment *reserve_segment[PARTNUM];
	struct blockmanager *bm;
}page_manager;

typedef struct gc_read_node{
	bool is_mapping;
	value_set *data;
	fdriver_lock_t done_lock;
	uint32_t piece_ppa;
	uint32_t lba;
	uint32_t version;
}gc_read_node;

enum{
	MAP_CHECK_FLUSHED_KP, MAP_READ_ISSUE, MAP_READ_DONE, MAP_READ_DONE_PENDING,
};

typedef struct gc_mapping_check_node{
	value_set *mapping_data;
	char *data_ptr;
	fdriver_lock_t done_lock;
	uint32_t type;
	uint32_t level;
	uint32_t map_ppa;
	uint32_t piece_ppa; 
	uint32_t new_piece_ppa;
	uint32_t lba;
	uint64_t validate_piece_cnt;
	uint64_t invalidate_piece_cnt;
	struct sst_file *target_sst_file;
}gc_mapping_check_node;

bool __do_gc(page_manager *pm, bool is_map, uint32_t target_page_num);

page_manager* page_manager_init(struct blockmanager *bm);
void page_manager_free(page_manager* pm);
bool page_manager_is_gc_needed(page_manager *pm, uint32_t needed_page, bool is_map);
bool page_manager_oob_lba_checker(page_manager *pm, uint32_t piece_ppa, uint32_t lba, uint32_t *idx);
uint32_t page_manager_get_new_ppa(page_manager *pm, bool ismap, uint32_t type);
uint32_t page_manager_pick_new_ppa(page_manager *pm, bool ismap, uint32_t type);
uint32_t page_manager_get_total_remain_page(page_manager *pm, bool ismap, bool include_invalid_block);
uint32_t page_manager_get_remain_page(page_manager *pm, bool ismap);
void validate_piece_ppa(blockmanager *bm, uint32_t piece_num, uint32_t* piece_ppa, uint32_t *lba, 
		uint32_t* version, bool);
bool invalidate_piece_ppa(blockmanager *bm, uint32_t piece_ppa, bool);
void validate_map_ppa(blockmanager *bm, uint32_t map_ppa, uint32_t start_lba, uint32_t end_lba, bool);
void invalidate_map_ppa(blockmanager *bm, uint32_t map_ppa, bool);
uint32_t page_manager_get_reserve_new_ppa(page_manager *pm, bool ismap, uint32_t seg_idx);
uint32_t page_manager_change_reserve(page_manager *pm, bool ismap);
uint32_t page_manager_get_reserve_remain_ppa(page_manager *pm, bool ismap, uint32_t seg_idx);
uint32_t page_manager_move_next_seg(page_manager *pm, bool ismap, bool isreserve, uint32_t type);
uint32_t page_manager_get_new_ppa_from_seg(page_manager *pm, __segment *seg);
uint32_t page_manager_pick_new_ppa_from_seg(page_manager *pm, __segment *seg);
__segment *page_manager_get_seg(page_manager *pm, bool ismap, uint32_t type);
__segment *page_manager_get_seg_for_bis(page_manager *pm,  uint32_t type);
uint32_t page_aligning_data_segment(page_manager *pm);
void gc_helper_for_normal(std::map<uint32_t, gc_mapping_check_node*>*, 
		struct write_buffer *wb, uint32_t seg_idx);

static inline  char *get_seg_type_name(page_manager *pm, uint32_t seg_idx){
	switch(pm->seg_type_checker[seg_idx]){
		case SEPDATASEG: return "SEPDATASEG";
		case DATASEG: return "DATASEG";
		case MAPSEG: return "MAPSEG";
	}
	return NULL;
}
#endif
