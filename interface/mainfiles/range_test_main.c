#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "../include/lsm_settings.h"
#include "../include/FS.h"
#include "../include/settings.h"
#include "../include/types.h"
#include "../bench/bench.h"
#include "interface.h"
#include "../algorithm/Lsmtree/lsmtree.h"
#include "../include/utils/kvssd.h"
extern int req_cnt_test;
extern uint64_t dm_intr_cnt;
extern int LOCALITY;
extern float TARGETRATIO;
extern master *_master;
extern bool force_write_start;
extern lsmtree LSM;
#ifdef Lsmtree
int skiplist_hit;
#endif

int range_target_cnt,range_now_cnt;
bool last_end_req(struct request *const req){
	int i=0;
	static int cnt=0;
	switch(req->type){
		case FS_MSET_T:
			/*should implement*/
			break;
		case FS_ITER_CRT_T:
#ifdef KVSSD
			printf("create iter! id:%lu [%.*s]\n",req->ppa,KEYFORMAT(req->key));
#else
			printf("create iter! id:%u [%u]\n",req->ppa,req->key);
#endif
			break;
		case FS_ITER_NXT_T:
			for(i=0;i<req->num; i++){
				keyset *k=&((keyset*)req->value->value)[i];
#ifdef KVSSD
				printf("[%d]keyset:%.*s-%lu\n",cnt++,KEYFORMAT(k->lpa),k->ppa);
#else
				printf("keyset:%u-%u\n",k->lpa,k->ppa);
#endif
			}
			break;
		case FS_ITER_NXT_VALUE_T:
			for(i=0;i<req->num; i++){
				KEYT k=req->multi_key[i];
#ifdef KVSSD
				printf("next_value: %*.s\n",KEYFORMAT(k));
#else
				printf("next_value: keyset:%u\n",k);
#endif
			}		
			break;
		case FS_ITER_RLS_T:
			break;
		default:
			printf("error in inf_make_multi_req\n");
			return false;
	}
	range_now_cnt++;
	return true;
}

int main(int argc,char* argv[]){
	inf_init(0);

	bench_init();
	bench_add(RANDSET,0,RANGE,RANGE);
	bench_add(NOR,0,UINT_MAX,UINT_MAX);
	char temp_v[PAGESIZE];
	memset(temp_v,'x',PAGESIZE);
	bench_value *value;
	value_set temp;
	temp.dmatag=-1;
	temp.value=temp_v;
#ifdef KVSSD
	int cnt=0;
	int idx=rand()%((int)RANGE);
	KEYT t_key;
#endif
	while((value=get_bench())){
#ifdef KVSSD
		if(cnt++==idx){
			kvssd_cpy_key(&t_key,&value->key);
		}
#endif
		inf_make_req(value->type,value->key,temp.value,value->length,value->mark);
	}

	range_target_cnt=10;

//	int iter_id=
	t_key.len-=3;
#ifdef KVSSD
	inf_iter_create(t_key,last_end_req);
#else
	inf_iter_create(rand()%((uint32_t)RANGE),last_end_req);
#endif
	char *test_values[100];
	//inf_iter_next(0/*iter_id*/,100,test_values,last_end_req,true);
	for(int i=0; i<10; i++){
		inf_iter_next(0/*iter_id*/,test_values,last_end_req,false);
	}

	inf_iter_release(0/*iter_id*/,last_end_req);

	while(range_target_cnt!=range_now_cnt){}
	return 0;
}
