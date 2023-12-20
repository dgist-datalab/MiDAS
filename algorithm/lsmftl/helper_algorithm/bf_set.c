#include "bf_set.h"
#include "compressed_bloomfilter.h"
#include <math.h>
#define ABS(a) (a<0? -a:a)
#define EPSILON 0.00000000001f

volatile float prev_bf_target_fpr=0.0f;
volatile float prev_target_fpr=0.0f;
uint32_t prev_bits;
extern uint32_t debug_lba;

static inline double func(double x, uint32_t member, float target_fpr){
	return pow(x, member)+member*(target_fpr-1)*x-(target_fpr*member-member+1);
}

static inline double deriv_func(double x, uint32_t member, float target_fpr){
	return pow(x, member-1)+member*(target_fpr-1);
}

static inline double newton(double init_value, uint32_t member, float target_fpr){
	double h, res;
	do{
		h=func(res, member, target_fpr) / deriv_func(res, member, target_fpr);
//		printf("h: %lf res: %lf\n", h, res);
		if(ABS(h) >= EPSILON){
			res-=h;
		}
		else break;
	}while(1);

	return 1-res;
}

double get_target_each_fpr(float block_fpr, uint32_t member_num){
	return newton(0.5, member_num, block_fpr);
}

uint32_t get_number_of_bits(float target_fpr){
	float t=1/target_fpr;
	uint32_t tt=ceil(log2(t));
	//printf("tt:%u\n",tt);
	return pow(2,tt) < t ? tt+1: tt;
}

void bf_set_prepare(float target_fpr, uint32_t member_num, uint32_t type){
	if(prev_target_fpr==0.0f || prev_target_fpr!=target_fpr){
		prev_bf_target_fpr=get_target_each_fpr(target_fpr, member_num);
		prev_target_fpr=target_fpr;
		prev_bits=get_number_of_bits(prev_bf_target_fpr);
	}
}

bf_set* bf_set_init(float target_fpr, uint32_t member_num, uint32_t type){
	bf_set *res=(bf_set*)malloc(sizeof(bf_set));
	if(prev_target_fpr==0.0f || prev_target_fpr!=target_fpr){
		prev_bf_target_fpr=get_target_each_fpr(target_fpr, member_num);
		prev_target_fpr=target_fpr;
		prev_bits=get_number_of_bits(prev_bf_target_fpr);
	}

	res->memory_usage_bit=0;
	res->bits=prev_bits;
	res->now=0;
	res->max=member_num;
	res->type=type;
	uint32_t i=0;
	switch(type){
		case BLOOM_PTR_PAIR:
			res->array=malloc(sizeof(bp_pair) * member_num);
			for(; i<member_num; i++){
				((bp_pair*)(res->array))[i].bf=cbf_init(res->bits);
				((bp_pair*)(res->array))[i].piece_ppa=UINT32_MAX;
			}
			break;
		case BLOOM_ONLY:
			res->array=malloc(sizeof(c_bf*)*member_num);
			for(; i<member_num; i++){
				((c_bf**)(res->array))[i]=cbf_init(res->bits);
			}
			break;
		default:
			EPRINT("no type of bf_set", true);
			break;
	}
/*
	res->array=(bp_pair*)malloc(sizeof(bp_pair) * member_num);
*/
	return res;
}

bool bf_set_insert(bf_set *bfs, uint32_t lba, uint32_t piece_ppa){
//	bfs->bf_array[
	if(bfs->now>=bfs->max) {
		EPRINT("over flow", true);
		return false;
	}

	switch(bfs->type){
		case BLOOM_PTR_PAIR:
			cbf_put_lba(((bp_pair*)bfs->array)[bfs->now].bf, lba);
			((bp_pair*)(bfs->array))[bfs->now].piece_ppa=piece_ppa;
			bfs->memory_usage_bit+=
				((bp_pair*)bfs->array)[bfs->now].bf->bits+48;
			break;
		case BLOOM_ONLY:
			cbf_put_lba(((c_bf**)(bfs->array))[bfs->now], lba);
			bfs->memory_usage_bit+=
				((c_bf**)bfs->array)[bfs->now]->bits;
			break;
	}
	bfs->now++;

	return true;
}

uint32_t bf_set_get_piece_ppa(bf_set *bfs, uint32_t *last_idx, uint32_t lba)
{
	int32_t i=*last_idx==UINT32_MAX?bfs->now:*last_idx;
	if(i<0){
		EPRINT("minus index is not available", true);
	}
	switch(bfs->type){
		case BLOOM_PTR_PAIR:
			for(; i>=0; i--){
				*last_idx=i;
				if(cbf_check_lba(((bp_pair*)bfs->array)[i].bf,lba)){
					return ((bp_pair*)bfs->array)[i].piece_ppa;
				}
			}
			break;
		case BLOOM_ONLY:
			for(; i>=0; i--){
				*last_idx=i;
				if(cbf_check_lba(((c_bf**)bfs->array)[i], lba)){
					return i;
				}
			}
			break;
	}
	return UINT32_MAX;
}


bf_set* bf_set_copy(bf_set *src){
	uint32_t i=0;
	bf_set *res=(bf_set*)malloc(sizeof(bf_set));
	*res=*src;
	switch(src->type){
		case BLOOM_PTR_PAIR:
			res->array=malloc(sizeof(bp_pair)*src->max);
			for(; i<src->max; i++){
				((bp_pair*)res->array)[i].bf=cbf_init(src->bits);
				*((bp_pair*)res->array)[i].bf=*((bp_pair*)src->array)[i].bf;
				((bp_pair*)res->array)[i].piece_ppa=((bp_pair*)src->array)[i].piece_ppa;
			}
			break;
		case BLOOM_ONLY:
			res->array=malloc(sizeof(c_bf*)*src->max);
			for(;i<src->max; i++){
				((c_bf**)res->array)[i]=cbf_init(src->bits);
				*((c_bf**)res->array)[i]=*((c_bf**)src->array)[i];
			}
			break;
		default:
			EPRINT("no type!!", true);
			break;
	}
	return res;
}

void bf_set_move(bf_set *des, bf_set *src){
	*des=*src;
	src->array=NULL;
}

void bf_set_free(bf_set* bfs){
	if(bfs->array){
		uint32_t i=0;
		switch(bfs->type){
			case BLOOM_PTR_PAIR:
				for(; i<bfs->max; i++){
					cbf_free(((bp_pair*)bfs->array)[i].bf);
				}
				break;
			case BLOOM_ONLY:
				for(; i<bfs->max; i++){
					cbf_free(((c_bf**)bfs->array)[i]);
				}
				break;
		}
		free(bfs->array);
	}
	free(bfs);
}
