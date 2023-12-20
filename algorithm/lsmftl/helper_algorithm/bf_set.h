#ifndef __BF_SET_H__
#define __BF_SET_H__
#include "compressed_bloomfilter.h"
#include "../../../include/settings.h"

enum{
	BLOOM_PTR_PAIR, BLOOM_ONLY
};

typedef struct bf_ptr_pair{
	c_bf *bf;
	uint32_t piece_ppa;
}bp_pair;

typedef struct bf_set{
	//float fpr;
	uint32_t type;
	uint32_t bits;
	uint32_t now;
	uint32_t max;
	void *array;
	uint32_t memory_usage_bit;
}bf_set;

void bf_set_prepare(float target_fpr, uint32_t member, uint32_t type);

bf_set* bf_set_init(float target_fpr, uint32_t member, uint32_t type);
bool bf_set_insert(bf_set*, uint32_t lba, uint32_t piece_ppa);
uint32_t bf_set_get_piece_ppa(bf_set *, uint32_t* last_idx, uint32_t lba);
bf_set* bf_set_copy(bf_set *src);
void bf_set_move(bf_set *des, bf_set *src);
void bf_set_free(bf_set*);
uint32_t get_number_of_bits(float target_fpr);
double get_target_each_fpr(float block_fpr, uint32_t member_num);
#endif
