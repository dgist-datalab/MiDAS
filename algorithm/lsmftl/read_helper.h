#ifndef __READ_HELPER_H__
#define __READ_HELPER_H__
#include "key_value_pair.h"
#include "../../include/settings.h"
#include "page_manager.h"
#include "sst_file.h"
#include "helper_algorithm/plr_helper.h"
#include "helper_algorithm/bf_set.h"
#include "helper_algorithm/guard_bf_set.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define READHELPER_NUM (4)

enum READ_HELPERTYPE{
	HELPER_NONE				=   0x0000000,
	HELPER_BF_PTR			=	0x0000001,
	HELPER_BF_ONLY			=   0x0000002,
	HELPER_GUARD			=	0x0000004,
	HELPER_PLR				=	0x0000008,

	HELPER_BF_PTR_GUARD 		=	HELPER_BF_PTR	| HELPER_GUARD,
	HELPER_BF_ONLY_GUARD		=	HELPER_BF_ONLY	| HELPER_GUARD,
	HELPER_BF_PLR				=	HELPER_BF_PTR	| HELPER_PLR,
	HELPER_ALL					=	HELPER_BF_PTR	| HELPER_PLR	| HELPER_GUARD,
};

typedef struct read_helper{
	uint32_t type;
	void *body;
}read_helper;

typedef struct read_helper_param{
	uint32_t type;
	float target_prob;
	uint32_t member_num;
	/*for PLR*/
	uint64_t slop_bit;
	uint32_t range;
}read_helper_param;

//read_helper *read_helper_stream_init(uint32_t helper_type);
void read_helper_prepare(float target_fpr, uint32_t member, uint32_t type);
read_helper *read_helper_init(read_helper_param rhp);
read_helper *read_helper_kpset_to_rh(read_helper_param rhp, key_ptr_pair *kp_set);
uint32_t read_helper_stream_insert(read_helper *, uint32_t lba, uint32_t piece_ppa);
bool read_helper_check(read_helper *, uint32_t lba, uint32_t *piece_ppa_result, struct sst_file *, uint32_t *idx);
uint32_t read_helper_memory_usage(read_helper *, uint32_t lba_unit);
void read_helper_print(read_helper *);
void read_helper_free(read_helper *);
read_helper* read_helper_copy(read_helper *src);
void read_helper_move(read_helper *des, read_helper *src);
bool read_helper_last(read_helper *rh, uint32_t idx);
uint32_t read_helper_idx_init(read_helper *rh, uint32_t lba);
bool read_helper_data_checking(read_helper *rh, struct page_manager*, uint32_t piece_ppa, 
		uint32_t lba, uint32_t *rh_idx, uint32_t *offset, sst_file *sptr);
uint32_t read_helper_get_cnt(read_helper *rh);
void read_helper_insert_done(read_helper *rh);
static inline char *read_helper_type(uint32_t i){
	switch(i){
		case HELPER_NONE: return "NONE";
		case HELPER_BF_PTR:
		case HELPER_BF_ONLY: return "BF";
		case HELPER_BF_PTR_GUARD: return "BF_PTR_GUARD";
		case HELPER_BF_ONLY_GUARD:
		case HELPER_GUARD: return "BF_GUARD";
		case HELPER_PLR: return "PLR";
		default: EPRINT("no type", true); break;
	}
	return NULL;
}	
#endif
