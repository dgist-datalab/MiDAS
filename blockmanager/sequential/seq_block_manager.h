#ifndef __S_BM_HEADER
#define __S_BM_HEADER
#include "../../include/container.h"
#include "../../interface/queue.h"
#include "../../include/data_struct/heap.h"
#include "../bb_checker.h"
#include <stdint.h>

typedef struct block_set{
        uint32_t total_invalid_number;
        uint32_t total_valid_number;
        uint32_t used_page_num;
        uint8_t type;
        __block *blocks[BPS];
        void *hptr;
}block_set;

typedef struct seq_bm_private{
	__block *seq_block;
	block_set *logical_segment;
	queue *free_logical_segment_q;
	mh *max_heap;
	uint32_t assigned_block;
	uint32_t free_block;
	int pnum;
	queue **free_logical_seg_q_pt;
	queue *invalid_block_q;
	uint8_t *seg_populate_bit;
	mh **max_heap_pt;
	long *timestamp;
}sbm_pri;

uint32_t seq_create (struct blockmanager*, lower_info *li);
void* return_heap_queue(struct blockmanager*);
uint32_t seq_destroy (struct blockmanager*);
__block* seq_get_block (struct blockmanager*, __segment *);
__block *seq_pick_block(struct blockmanager *, uint32_t page_num);
__segment* seq_get_segment (struct blockmanager*, bool isreserve);
__segment* seq_jy_get_time_segment (struct blockmanager* bm, bool isreserve, double time_stamp);
uint32_t seq_jy_get_block_idx (struct blockmanager*, void* b);
long jy_get_timestamp(struct blockmanager* bm, uint32_t lba);
bool seq_check_full(struct blockmanager *,__segment *active, uint8_t type);
bool seq_is_gc_needed (struct blockmanager*);

int seq_get_free_segment_number(struct blockmanager*);

__gsegment* seq_get_gc_target (struct blockmanager*);
__gsegment* seq_jy_get_gc_target (struct blockmanager*, queue* group_q);
void seq_trim_segment (struct blockmanager*, __gsegment*, struct lower_info*);
int seq_populate_bit (struct blockmanager*, uint32_t ppa);
int seq_unpopulate_bit (struct blockmanager*, uint32_t ppa);
bool seq_query_bit(struct blockmanager *, uint32_t ppa);
int seq_erase_bit (struct blockmanager*, uint32_t ppa);
bool seq_is_valid_page (struct blockmanager*, uint32_t ppa);
bool seq_is_invalid_page (struct blockmanager*, uint32_t ppa);
void seq_set_oob(struct blockmanager*, char *data, int len, uint32_t ppa);
char* seq_get_oob(struct blockmanager*, uint32_t ppa);
void seq_release_segment(struct blockmanager*, __segment *);
__segment* seq_change_reserve(struct blockmanager* ,__segment *reserve);
__segment* seq_jy_change_reserve(struct blockmanager* bm,__segment *reserve, double time_stamp);
void seq_jy_add_queue(struct blockmanager*, queue* group_q, __segment* reserve);
void seq_jy_add_time_queue(struct blockmanager* bm, queue* group_q, __segment *reserve, double timestamp);
int seq_jy_move_q2h(struct blockmanager*, queue* q, int size);
int seq_get_page_num(struct blockmanager* ,__segment *);
int seq_pick_page_num(struct blockmanager* ,__segment *);
void seq_reinsert_segment(struct blockmanager *, uint32_t seg_idx);
void seq_time_reinsert_segment(struct blockmanager *bm, uint32_t seg_idx, double timestamp);
uint32_t seq_remain_free_page(struct blockmanager *, __segment *);

uint32_t seq_map_ppa(struct blockmanager* , uint32_t lpa);
void seq_free_segment(struct blockmanager *, __segment *);
void seq_invalidate_number_decrease(struct blockmanager *bm, uint32_t ppa);
uint32_t seq_get_invalidate_number(struct blockmanager *bm, uint32_t seg_idx);
uint32_t seq_get_invalidate_blk_number(struct blockmanager *bm);


uint32_t seq_pt_create(struct blockmanager *, int, int*, lower_info *);
uint32_t seq_pt_destroy(struct blockmanager *);
__segment* seq_pt_get_segment (struct blockmanager*, int pt_num, bool isreserve);
__gsegment* seq_pt_get_gc_target (struct blockmanager*, int pt_num);
void seq_pt_trim_segment(struct blockmanager*, int pt_num, __gsegment *, lower_info*);
int seq_pt_remain_page(struct blockmanager*, __segment *active,int pt_num);
bool seq_pt_isgc_needed(struct blockmanager*, int pt_num);
__segment* seq_change_pt_reserve(struct blockmanager *,int pt_num, __segment *reserve);
uint32_t seq_pt_reserve_to_free(struct blockmanager*, int pt_num, __segment *reserve);


void seq_mh_swap_hptr(void *a, void *b);
void seq_mh_assign_hptr(void *a, void *hn);
int seq_get_cnt(void *a);

#endif
