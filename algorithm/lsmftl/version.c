#include "version.h"
#include "../../include/settings.h"
#include "lsmtree.h"
#include <math.h>

extern lsmtree LSM;

version *version_init(uint8_t total_version_number, uint32_t LBA_num, level **disk, uint32_t leveln){
	version *res=(version*)malloc(sizeof(version));
	res->start_hand=res->end_hand=0;
	res->key_version=(uint8_t*)calloc(LBA_num, sizeof(uint8_t));
	memset(res->key_version, -1, sizeof(uint8_t)*LBA_num);
	res->valid_version_num=0;
	res->max_valid_version_num=total_version_number;
	printf("max_version idx:%u\n", total_version_number);

	res->version_empty_queue=new std::queue<uint32_t>*[leveln];
	res->start_vidx_of_level=(uint32_t*)malloc(sizeof(uint32_t)*leveln);
	res->level_run_num=(uint32_t*)malloc(sizeof(uint32_t)*leveln);
	res->poped_version_num=(uint32_t*)calloc(leveln, sizeof(uint32_t));

	uint32_t version=0;
	for(int32_t i=leveln-1; i>=0; i--){
		res->version_empty_queue[i]=new std::queue<uint32_t>();
		res->start_vidx_of_level[i]=version;
		uint32_t limit=i==leveln-1?LSM.param.last_size_factor:LSM.param.normal_size_factor;
		switch(disk[i]->level_type){
			case LEVELING:
			case LEVELING_WISCKEY:
				res->version_empty_queue[i]->push(version++);
				res->level_run_num[i]=1;
				break;
			case TIERING_WISCKEY:
			case TIERING:
				res->level_run_num[i]=limit;
				for(uint32_t j=0; j<limit; j++){
					res->version_empty_queue[i]->push(version++);
				}
				break;
		}
	}


	res->version_populate_queue=new std::queue<uint32_t>*[leveln];
	for(uint32_t i=0; i<leveln; i++){
		res->version_populate_queue[i]=new std::queue<uint32_t>();
	}

	res->total_version_number=total_version_number;
	res->version_invalidation_cnt=(uint32_t*)calloc(res->total_version_number+1, sizeof(uint32_t));
	//res->version_early_invalidate=(bool*)calloc(res->total_version_number+1, sizeof(bool));
	res->memory_usage_bit=ceil(log2(res->max_valid_version_num))*LBA_num;
	fdriver_mutex_init(&res->version_lock);
	res->leveln=leveln;

	return res;
}

uint32_t version_get_empty_version(version *v, uint32_t level){
	if(v->version_empty_queue[level]->empty()){
		EPRINT("should merge before empty version", true);
	}
	uint32_t res=v->version_empty_queue[level]->front();
	v->version_empty_queue[level]->pop();
	return res;
}

void version_get_merge_target(version *v, uint32_t *version_set, uint32_t level){
	for(uint32_t i=0; i<(1+1); i++){
		version_set[i]=v->version_populate_queue[level]->front();
		v->version_populate_queue[level]->pop();
	}
}

void version_unpopulate(version *v, uint32_t version, uint32_t level_idx){
	v->version_empty_queue[level_idx]->push(version);
	v->version_invalidation_cnt[version]=0;
}

void version_populate(version *v, uint32_t version, uint32_t level_idx){
	v->version_populate_queue[level_idx]->push(version);
}

void version_sanity_checker(version *v){
#ifdef LSM_DEBUG
	uint32_t remain_empty_size=0;
	for(uint32_t i=0; i<LSM.param.LEVELN; i++){
		remain_empty_size+=v->version_empty_queue[i]->size();
	}

	uint32_t populate_size=0;
	for(uint32_t i=0; i<LSM.param.LEVELN; i++){
		populate_size+=v->version_populate_queue[i]->size();
	}
	if(remain_empty_size+populate_size!=v->max_valid_version_num){
		printf("error log : empty-size(%d) populate-size(%d)\n", remain_empty_size, populate_size);
		EPRINT("version sanity error", true);
	}
#endif
}

void version_free(version *v){
	for(uint32_t i=0; i<v->leveln; i++){
		delete v->version_empty_queue[i];
		delete v->version_populate_queue[i];
	}
	delete[] v->version_empty_queue;
	delete[] v->version_populate_queue;

	free(v->level_run_num);
	free(v->poped_version_num);
	free(v->start_vidx_of_level);
	//free(v->version_early_invalidate);
	free(v->version_invalidation_cnt);
	free(v->key_version);
	free(v);
}

extern uint32_t debug_lba;
void version_coupling_lba_version(version *v, uint32_t lba, uint8_t version){
	if(version!=UINT8_MAX && version>v->total_version_number){
		EPRINT("over version num", true);
	}
#ifdef LSM_DEBUG
	if(lba==debug_lba){
		printf("[version_map] lba:%u->%u lev:%u\n",lba, version, 
				version_to_level_idx(v, version, v->leveln));
	}
#endif
	fdriver_lock(&v->version_lock);
	if(v->key_version[lba]!=UINT8_MAX){
		v->version_invalidation_cnt[v->key_version[lba]]++;
	}

	v->key_version[lba]=version;
	fdriver_unlock(&v->version_lock);
}

/*
void version_reinit_early_invalidation(version *v, uint32_t version_num, uint32_t *version){
	EPRINT("should I need it?\n", false);
	for(uint32_t i=0; i<version_num; i++){
		v->version_invalidation_cnt[version[i]]=0;
		v->version_early_invalidate[version[i]]=false;
	}
}

uint32_t version_get_early_invalidation_target(version *v){
	EPRINT("should I need it?\n", false);

	for(uint32_t i=0; i<v->max_valid_version_num; i++){
		if(v->version_early_invalidate[version_to_run(v,i)]){
			return version_to_run(v,i);
		}
	}
	return UINT32_MAX;
}

static bool early_invalidate_available_check(uint32_t version){
	EPRINT("should I need it?\n", false);
	run *r=&LSM.disk[LSM.param.LEVELN-1]->array[version];
	sst_file *sptr;
	map_range *mptr;
	uint32_t sidx, midx;
	for_each_sst(r, sptr, sidx){
		for_each_map_range(sptr, mptr, midx){
			if(LSM.gc_unavailable_seg[mptr->ppa/_PPS]){
				return false;
			}
		}
	}
	return true;
}

uint32_t version_get_max_invalidation_target(version *v, uint32_t *invalidated_num, uint32_t *avg_invalidated_num){
	EPRINT("should I need it?\n", false);
	uint32_t target_version=UINT32_MAX;
	uint32_t target_invalidation_cnt=0;
	for(uint32_t i=v->last_level_version_num; i<v->max_valid_version_num; i++){
		if(v->version_early_invalidate[i]) continue;
		if(LSM.now_merging_run[0]==i || LSM.now_merging_run[1]==i) continue;
		if(target_invalidation_cnt<v->version_invalidation_cnt[i]){
			if(early_invalidate_available_check(i)){
				target_version=i;
				target_invalidation_cnt=v->version_invalidation_cnt[i];
			}	
		}
	}
	if(target_version==UINT32_MAX){
		return target_version;
	}

	if(invalidated_num){
		*invalidated_num=target_invalidation_cnt;
	}
	if(avg_invalidated_num){
		*avg_invalidated_num=(target_invalidation_cnt/
			LSM.disk[LSM.param.LEVELN-1]->array[target_version].now_sst_num);
	}

	return target_version;
}
*/
uint32_t version_update_for_trivial_move(version *v, uint32_t start_lba, uint32_t end_lba, 
		uint32_t src_level_idx, uint32_t des_level_idx, uint32_t target_version){
	for(uint32_t i=start_lba; i<=end_lba; i++){
		if(src_level_idx==UINT32_MAX){
			uint8_t now_version=version_map_lba(v,i);
			if(now_version!=(uint8_t)-1){
				continue;
			}
			version_coupling_lba_version(v, i, target_version);
		}
		else if(version_to_level_idx(v, version_map_lba(v, i), v->leveln)==src_level_idx){
			version_coupling_lba_version(v, i, target_version);
		}
		else{
		}
	}
	return 1;
}
/*
void version_make_early_invalidation_enable_old(version *v){
	EPRINT("should I need it?\n", false);
	for(uint32_t i=0; i<v->max_valid_version_num-1; i++){
		v->version_early_invalidate[version_to_run(v,i)]=true;
	}
}
*/
uint32_t version_level_to_start_version(version *v, uint32_t lev_idx){
	return v->start_vidx_of_level[lev_idx];
}
/*
void version_traversal(version *v){
	uint32_t* version_array=(uint32_t*)malloc(sizeof(uint32_t)*v->max_valid_version_num);
	for(uint32_t i=0; i<v->max_valid_version_num; i++){
		version_array[i]=i;
	}

	for(uint32_t i=0; i<v->max_valid_version_num-1; i++){
		uint32_t a=version_array[i];
		for(uint32_t j=i+1; j<v->max_valid_version_num; j++){
			uint32_t b=version_array[j];
			if(version_compare(v, a, b) > 0){
				uint32_t temp=version_array[i];
				version_array[i]=version_array[j];
				version_array[j]=temp;
			}
		}
	}

	printf("version order print\n");
	for(uint32_t i=0; i<v->max_valid_version_num; i++){
		printf("%u - %u\n",i, version_array[i]);
	}

	free(version_array);
}
*/
uint32_t version_get_level_invalidation_cnt(version *v, uint32_t level_idx){
	if(level_idx==0) return 0;
	uint32_t start_idx=version_level_to_start_version(v, level_idx);
	uint32_t end_idx=version_level_to_start_version(v, level_idx-1);
	uint32_t res=0;
	for(uint32_t i=start_idx; i<end_idx; i++){
		res+=v->version_invalidation_cnt[i];
	}
	return res;
}

uint32_t version_get_resort_version(version *v, uint32_t level_idx){
	uint32_t remain_size=v->version_empty_queue[level_idx]->size();
	for(uint32_t i=0; i<remain_size; i++){
		uint32_t target_version=version_get_empty_version(v, level_idx);
		version_populate(v, target_version, level_idx);
	}

	remain_size=v->version_populate_queue[level_idx]->size();
	for(uint32_t i=0; i<remain_size; i++){
		version_unpopulate(v, version_pop_oldest_version(v, level_idx), level_idx);
	}
	return 1;
}
