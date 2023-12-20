#include "lsmtree_param_module.h"
#include "../../../include/settings.h"
#include <cmath>
#define MAXRETRY_CNT (2)
static inline int checking(uint32_t level, uint32_t size_factor, uint32_t blocknumber){
	uint32_t head_num=0;
	uint32_t level_head_num=1;
	for(uint32_t i=0; i<=level; i++){
		head_num+=level_head_num;
		level_head_num*=size_factor;
	}
	if(head_num<blocknumber){
		return -1;
	}
	else if(head_num > blocknumber){
		return 1;
	}
	else return 0;
}

double get_size_factor(uint32_t level,  uint32_t blocknumber){
	double target=(std::pow(blocknumber, (double)1.0/level));
	int result;
	int retry_cnt=0;
retry:
	switch((result=checking(level, target, blocknumber))){
		case 1:
		case 0:
			return target;
		case -1:
			target++;
			retry_cnt++;
			if(retry_cnt>MAXRETRY_CNT) return UINT32_MAX;
			goto retry;
	}
	return UINT32_MAX;
}

uint32_t get_level(uint32_t sizefactor, uint32_t blocknumber){
	uint32_t target=(uint32_t)(std::ceil(std::log(blocknumber)/std::log((double)sizefactor)));
	int result;
	int retry_cnt=0;
retry:
	switch((result=checking(target, sizefactor, blocknumber))){
		case 1:
		case 0:
			return target;
		case -1:
			target++;
			retry_cnt++;
			if(retry_cnt>MAXRETRY_CNT) return UINT32_MAX;
			goto retry;
	}
	return UINT32_MAX;
}

uint32_t get_waf(char t, uint32_t level, uint32_t size_factor){
	switch (t){
		case TIER:
			return level;
		case LEVEL:
			return level*size_factor;
		case HYBRIDTL:
			return level-1+size_factor;
		case HYBRIDLT:
			return (level-1)*size_factor+1;
	}
	return UINT32_MAX;
}

float get_sep_waf(char t, uint32_t level, uint32_t size_factor){
	switch (t){
		case TIER:
			return (float)(level-1)/(PAGESIZE/sizeof(uint32_t))+1+1;
		case LEVEL:
			return (float)(level-1)*size_factor/(PAGESIZE/sizeof(uint32_t))+size_factor+1;
		case HYBRIDTL:
			return level-1+size_factor;
		case HYBRIDLT:
			return (float)(level-1)*size_factor/(PAGESIZE/sizeof(uint32_t))+1+1;
	}
	return UINT32_MAX;
}

uint32_t get_raf(char t, uint32_t level, uint32_t size_factor){
	switch (t){
		case TIER:
			return 	level*size_factor;
		case LEVEL:
			return level;
		case HYBRIDTL:
			return (level-1)*size_factor+1;
		case HYBRIDLT:
			return size_factor;
	}
	return UINT32_MAX;
}

uint32_t get_sparse_sorted_head(uint32_t level, uint32_t size_factor){
	uint32_t head_num=0;
	uint32_t level_head_num=1;
	for(uint32_t i=0; i<=level-1; i++){
		head_num+=level_head_num;
		level_head_num*=size_factor;
	}
	return head_num;
}

static inline uint32_t get_total_bn(uint32_t level, uint32_t size_factor, uint32_t *array){
	uint32_t res=0;
	uint32_t start=1;
	for(uint32_t i=0; i<level; i++){
		if(array){
			array[i]=(start*size_factor);
		}
		res+=(start*size_factor);
		start*=size_factor;
	}
	return res;
}

static inline uint32_t array_some(uint32_t level, uint32_t *array){
	uint32_t res=0;
	for(uint32_t i=0; i<level; i++) res+=array[i];
	return res;
}

uint32_t *get_blocknum_list(uint32_t *_level, uint32_t *_size_factor, uint32_t blocknum, float *ratio){
	if(!(_level || _size_factor)) return NULL;
	uint32_t level=*_level?*_level:get_level(*_size_factor,blocknum);
	uint32_t size_factor=*_size_factor?*_size_factor:get_size_factor(*_level, blocknum);
	uint32_t *res=(uint32_t *)malloc(sizeof(uint32_t)*level);
	uint32_t calc_bn=get_total_bn(level, size_factor, res);

	if(calc_bn>blocknum){
		while(1){
			calc_bn=get_total_bn(level,size_factor-1, res);
			if(calc_bn > blocknum){
				size_factor--;
				continue;
			}
			else{
				size_factor--;
				break;
			}
		}
	}

	if(calc_bn < blocknum){
		uint32_t remain=blocknum-calc_bn;
		res[level-1]+=remain;
	}

	if(ratio){
		*ratio=((float)array_some(level, res)-res[level-1])/res[level-1];
	}
	if(!(*_level)) *_level=level;
	if(!(*_size_factor)) *_size_factor=size_factor;
	return res;
}

