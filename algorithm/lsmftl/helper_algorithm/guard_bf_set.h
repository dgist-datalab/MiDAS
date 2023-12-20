#ifndef __GUARD_BF_SET_H__
#define __GUARD_BF_SET_H__
#include "bf_set.h"

typedef struct guard_bp_pair{
	uint32_t start_lba;
	uint32_t end_lba;
	void *array;
}guard_bp_pair;

typedef struct guard_bf_set{
	uint32_t type;
	uint32_t memory_usage_bit;
	uint32_t set_num;
	uint32_t now;
	uint32_t max;
	guard_bp_pair *body;
}guard_bf_set;

void gbf_set_prepare(float target_fpr, uint32_t member, uint32_t type);

guard_bf_set *gbf_set_init(float target_fpr, uint32_t member, uint32_t type);
bool gbf_set_insert(guard_bf_set *, uint32_t lba, uint32_t piece_ppa);
uint32_t gbf_set_get_piece_ppa(guard_bf_set *, uint32_t *last_idx, uint32_t lba);
uint32_t gbf_get_start_idx(guard_bf_set *, uint32_t lba);
guard_bf_set* gbf_set_copy(guard_bf_set *src);
void gbf_set_move(guard_bf_set *des, guard_bf_set *src);
void gbf_set_free(guard_bf_set*);

static inline uint32_t gbf_get_memory_usage_bit(guard_bf_set *gbf_set, uint32_t lba_unit){
	uint32_t res=0;
	for(uint32_t i=0; i<gbf_set->set_num; i++){
		uint32_t bf_set_memory=0;
		if((bf_set_memory=((bf_set*)gbf_set->body[i].array)->memory_usage_bit)){
			res+=bf_set_memory+2*lba_unit;
		}
		else break;
	}
	return res;
}

#endif
