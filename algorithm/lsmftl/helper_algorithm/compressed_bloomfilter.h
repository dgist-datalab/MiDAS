#ifndef __COMPRESSED_BLOOMFILTER_H__
#define __COMPRESSED_BLOOMFILTER_H__
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../../../include/settings.h"
typedef struct compressed_bloomfilter{
	uint32_t bits;
	uint32_t compressed_data;
}c_bf;

c_bf *cbf_init(uint32_t bits);
void cbf_init_mem(c_bf *, uint32_t bits);
static inline uint32_t cbf_get_bits(c_bf *cbf){return cbf->bits;}
static inline void cbf_free(c_bf*cbf){free(cbf);}
void cbf_put_lba(c_bf *cbf, uint32_t lba);
bool cbf_check_lba(c_bf *cbf, uint32_t lba);
#endif
