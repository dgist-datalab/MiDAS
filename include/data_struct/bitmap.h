#ifndef __BITMAP_H__
#define __BITMAP_H__

#include <stdint.h>
#include <stdlib.h>

#include "../../include/settings.h"

typedef uint8_t bitmap;

static inline bitmap *bitmap_init(uint32_t member_num){
	return (bitmap*)calloc(member_num/8+(member_num%8?1:0), sizeof(uint8_t));
}

static inline bitmap *bitamp_set_init(uint32_t member_num){
	bitmap *res=(bitmap*)malloc(member_num/8+(member_num%8?1:0) * sizeof(uint8_t));
	memset(res, -1, member_num/8+(member_num%8?1:0) * sizeof(uint8_t));
	return res;
}

static inline bool bitmap_is_set(bitmap *b, uint32_t idx){
	return b[idx/8] & (1<<(idx%8));
}

static inline bool bitmap_set(bitmap *b, uint32_t idx){
	char res=bitmap_is_set(b, idx);
	b[idx/8]|=(1<<(idx%8));
	return res;
}

static inline bool bitmap_unset(bitmap *b, uint32_t idx){
	char res=bitmap_is_set(b, idx);
	b[idx/8]&=~(1<<idx%8);
	return res;
}

static inline void bitmap_free(bitmap* b){
	free(b);
}

#endif
