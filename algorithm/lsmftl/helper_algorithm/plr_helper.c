#include "plr_helper.h"
#include "../../../include/settings.h"
#include <stdlib.h>
#include <stdio.h>

plr_helper *plr_init(uint64_t slop_bit, uint32_t range, uint32_t member_num){
	plr_helper *res=(plr_helper*)calloc(1, sizeof(plr_helper));
	res->max=member_num;
	res->now=0;
	res->body=new PLR(slop_bit, range);
	return res;
}

bool plr_insert(plr_helper *ph, uint32_t lba, uint32_t ppa){
	if(ph->now>=ph->max){
		EPRINT("over flow", true);
		return false;
	}
	ph->body->insert(lba, ppa);
	ph->now++;
	return true;
}

uint64_t plr_memory_usage(plr_helper *ph, uint32_t lba_unit){
	return ph->body->memory_usage(lba_unit);
}

uint32_t plr_get_ppa(plr_helper *ph, uint32_t lba, uint32_t ppa, uint32_t *idx){
	switch((*idx)){
		case PLR_NORMAL_PPA:
			 return (uint32_t)ph->body->get(lba);
		case PLR_FRONT_PPA:
			 return ppa-1;
		case PLR_DOUBLE_FRONT_PPA:
			 return ppa-2;
		case PLR_BEHIND_PPA:
			 return ppa+1;
		case PLR_DOUBLE_BEHIND_PPA:
			 return ppa+2;
		case PLR_SECOND_ROUND:
			 (*idx)=UINT32_MAX;
			 break;
	}
	return UINT32_MAX;
}

plr_helper *plr_copy(plr_helper *input){
	plr_helper *res=(plr_helper*)calloc(1, sizeof(plr_helper));
	*res=*input;
	res->body=input->body->copy();
	return res;
}

void plr_move(plr_helper *des, plr_helper *src){
	*des=*src;
	src->body=NULL;
}

void plr_free(plr_helper *ph){
	if(ph->body){
		ph->body->clear();
		delete ph->body;
	}
	free(ph);
}

void plr_insert_done(plr_helper *ph){
	ph->body->insert_end();
}
