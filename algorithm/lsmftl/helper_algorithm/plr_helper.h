#ifndef __PLR_HELPER_H__
#define __PLR_HELPER_H__
#define PARAM
#include "plr/plr.h"

enum{
	PLR_NORMAL_PPA, PLR_FRONT_PPA, PLR_BEHIND_PPA, 
	PLR_DOUBLE_FRONT_PPA, PLR_DOUBLE_BEHIND_PPA,
	PLR_SECOND_ROUND
};

class PLR;

typedef struct plr_helper{
	uint32_t now;
	uint32_t max;
	PLR *body;
}plr_helper;

plr_helper *plr_init(uint64_t slop_bit, uint32_t range, uint32_t member_num);
bool plr_insert(plr_helper *ph, uint32_t lba, uint32_t ppa);
uint64_t plr_memory_usage(plr_helper *ph, uint32_t lba_unit);
uint32_t plr_get_ppa(plr_helper *ph, uint32_t lba, uint32_t ppa, uint32_t *idx);
plr_helper *plr_copy(plr_helper *input);
void plr_insert_done(plr_helper *ph);
void plr_move(plr_helper *des, plr_helper *src);
void plr_free(plr_helper *ph);

#endif
