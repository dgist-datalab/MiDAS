#ifndef __LSM_P_M_H__
#define __LSM_P_M_H__
#include <stdint.h>
#include <stdio.h>

enum LSM_TYPE{
	TIER, LEVEL, HYBRIDTL, HYBRIDLT
};

double get_size_factor(uint32_t level, uint32_t mapping_num);
uint32_t get_level(uint32_t sizefactor, uint32_t mapping_num);
uint32_t get_waf(char t, uint32_t level, uint32_t size_factor);
uint32_t get_raf(char t, uint32_t level, uint32_t size_factor);
float get_sep_waf(char t, uint32_t level, uint32_t size_factor);
uint32_t get_sparse_sorted_head(uint32_t level, uint32_t size_factor);
uint32_t *get_blocknum_list(uint32_t* level, uint32_t* size_factor, uint32_t blocknum, float *ratio);

#endif
