#ifndef __P_BM_HEADER
#define __P_BM_HEADER
#include "../../include/settings.h"
#include "../../include/container.h"
#include "../../interface/queue.h"
#include "../../include/data_struct/heap.h"
#include "../base_block_manager.h"
#include <stdint.h>

typedef struct part_info{
	int pnum;
	int *now_assign;
	int *max_assign;
	int *from;
	int *to;
	channel **p_channel;
}p_info;

uint32_t pbm_create(blockmanager *bm, int pnum, int *epn, lower_info *li);
uint32_t pbm_destroy(blockmanager *bm);
__segment* pbm_pt_get_segment(blockmanager *bm, int pnum, bool isreserve);
__segment* pbm_change_pt_reserve(blockmanager *bm, int pt_num, __segment* reserve);
__gsegment* pbm_pt_get_gc_target(blockmanager* bm, int pnum);
int pbm_pt_remain_page(blockmanager* bm, __segment *active, int pt_num);

void pbm_pt_trim_segment(blockmanager* bm, int pt_num, __gsegment *target, lower_info *li);
bool pbm_pt_isgc_needed(struct blockmanager* bm, int pt_num);
uint32_t pbm_reserve_to_free(struct blockmanager *bm, int ptnum,__segment *reserve);
#endif
