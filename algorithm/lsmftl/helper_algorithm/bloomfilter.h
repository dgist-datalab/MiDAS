#ifndef __BLOOM_H__
#define __BLOOM_H__
#include"../../../include/settings.h"
#include<stdlib.h>
#include<stdint.h>
typedef struct{
	int k;
	uint64_t m;
	uint32_t targetsize;
	int n;
	float p;
	char *body;
}BF;

BF* bf_init(int entry,float fpr);
float bf_fpr_from_memory(int entry,uint32_t memory);
float bf_fpr_from_memory_monkey(int entry, uint32_t memory, uint32_t level,float size_factor, float normal_fpr);
void bf_free(BF *);
uint64_t bf_bits(int entry, float fpr);
void bf_put(BF *,uint32_t);
BF* bf_cpy(BF *src);
bool bf_check(BF*,uint32_t);
void bf_save(BF*);
BF* bf_load();
#endif

