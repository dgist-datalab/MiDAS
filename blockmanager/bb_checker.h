#ifndef __H_BBCHECKER_H
#define __H_BBCHECKER_H
#include "../include/settings.h"
#include "../include/container.h"
#include <limits.h>
#define GETORGBLOCKID(checker,ppa)\
	(checker.ent[ppa/16384].deprived_from_segnum==UINT_MAX?(ppa/16384*16384):checker.ent[ppa/16384].deprived_from_segnum)

typedef struct badblock_checker_node{
	uint8_t flag; //0 normal 1 badblock
	uint32_t origin_segnum;//original seg ppa
	uint32_t deprived_from_segnum;
	uint32_t fixed_segnum;//fixed segnum
	uint32_t pair_segnum;//for slc
}bb_node;

typedef struct badblock_checker{
	bb_node ent[_RNOS];
	uint32_t back_index;
	uint32_t start_block;
	uint32_t assign;
	uint32_t max;
	bool map_first;
}bb_checker;

void bb_checker_start(lower_info*);
void *bb_checker_process(uint64_t,uint8_t);
void bb_checker_fixing();
uint32_t bb_checker_fixed_segment(uint32_t ppa);
uint32_t bb_checker_paired_segment(uint32_t ppa);
uint32_t bb_checker_get_segid();
uint32_t bb_checker_get_originid(uint32_t seg_id);
static inline uint32_t bb_checker_fix_ppa(bool isfixing,uint32_t fixed_block, uint32_t pair_block,uint32_t ppa){
	uint32_t res=ppa;
	uint32_t block = (res >> 14);
	//bool isfixing=_checker.ent[block].flag;
	//uint32_t fixed_block=_checker.ent[block].fixed_segnum;
	//uint32_t pair_block=_checker.ent[block].pair_segnum;


	uint32_t bus  = res & 0x7;
	uint32_t chip = (res >> 3) & 0x7;
	uint32_t page= (res >> 6) & 0xFF;
	bool shouldchg=false;

	if(page>=4){
		if(page>=254 || page <6){
			page=page-4;
			shouldchg=true;
		}else if(page%4<2){
			page=page>6?page-6:page;
			shouldchg=true;
		}
	}

	res=bus+(chip<<3)+(page<<6)+(block<<14);
	uint32_t origin_remain=res%(_PPS);
	if(!shouldchg && isfixing){
		return fixed_block+origin_remain;
	}
	else if(shouldchg){
		return pair_block+origin_remain;
	}
	return res;
}
#endif
