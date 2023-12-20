#include "bb_checker.h"
#include "../interface/interface.h"
#include "../include/sem_lock.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
bb_checker checker;
volatile uint64_t target_cnt, _cnt, badblock_cnt;
uint32_t array[128];
fdriver_lock_t bb_lock;
#define TESTPAGE (16384*2)
//#define STARTBLOCKCHUNK 3
char *data_checker_data;

typedef struct temp_param{
	uint32_t ppa;
	value_set *v;
}tp;
void *temp_end_req(algo_req *temp){
	static int cnt=0;
	tp *param=(tp*)temp->param;
	uint32_t print_value=0;
	switch(temp->type){
		case FS_GET_T:
			memcpy(&print_value,param->v->value,sizeof(print_value));
			printf("read data:%u\n",print_value);
			inf_free_valueset(param->v,FS_GET_T);
			if(++cnt%10==0){
				fflush(stdout);
			}
			break;
		case FS_SET_T:
			inf_free_valueset(param->v,FS_SET_T);
			break;
	}
	if(cnt==TESTPAGE){
		fdriver_unlock(&bb_lock);
	}
	free(temp);
	free(param);
	return NULL;
}

void bb_write_bb_checker(lower_info *li,uint32_t testing_page){
	algo_req *temp;
	tp *param;
	char *temp_test=(char*)malloc(PAGESIZE);
	for(uint32_t i=0; i<testing_page; i++){
		memcpy(temp_test,&i,sizeof(i));
		temp=(algo_req*)calloc(sizeof(algo_req),1);
		temp->type=FS_SET_T;
		temp->end_req=temp_end_req;

		param=(tp*)calloc(sizeof(tp),1);
		param->ppa=i;
		param->v=inf_get_valueset(temp_test,FS_SET_T,PAGESIZE);
		temp->param=(void*)param;
		li->write(i,PAGESIZE,param->v,ASYNC,temp);
	}
}

void bb_read_bb_checker(lower_info *li,uint32_t testing_page){
	algo_req *temp;
	tp *param;
	for(uint32_t i=0; i<testing_page; i++){
		temp=(algo_req*)calloc(sizeof(algo_req),1);
		temp->type=FS_GET_T;
		temp->end_req=temp_end_req;

		param=(tp*)calloc(sizeof(tp),1);
		param->ppa=i;
		param->v=inf_get_valueset(NULL,FS_GET_T,PAGESIZE);
		temp->param=(void*)param;
		li->read(i,PAGESIZE,param->v,ASYNC,temp);
	}
}

void bb_checker_static_bad(){
	uint32_t arr[]={8438006,66560755,UINT32_MAX};
	for(int i=0; arr[i]!=UINT32_MAX;i++){
		uint32_t bad_seg=arr[i]>>14;
		if(!checker.ent[bad_seg].flag){
			checker.ent[bad_seg].flag=true;
			printf("new block bb:%d\n",bad_seg);
			badblock_cnt++;
		}
	}
}
void bb_checker_start(lower_info *li){
	memset(&checker,0,sizeof(checker));
	target_cnt=_RNOS*64;
//	srand(1);
//	srand((unsigned int)time(NULL));
	printf("_nos:%ld\n",_NOS);
	checker.assign=0;//(rand()%STARTBLOCKCHUNK)*_NOS;
	checker.start_block=checker.assign;
	checker.map_first=true;
	printf("start block number : %d\n",checker.assign);
	for(uint64_t i=0; i<_RNOS; i++){
		checker.ent[i].origin_segnum=i*_PPS;
		checker.ent[i].deprived_from_segnum=UINT_MAX;
		if(!li->device_badblock_checker){
			_cnt+=BPS;
			continue;
		}
		li->device_badblock_checker(i*_PPS,_PPS*PAGESIZE,bb_checker_process);
		//memio_trim(mio,i*(1<<14),(1<<14)*PAGESIZE,bb_checker_process);
	}

	while(target_cnt!=_cnt){}
	printf("\n");
//	bb_checker_process(0,true);
	data_checker_data=(char*)malloc(PAGESIZE);
	memset(data_checker_data,-1,PAGESIZE);

	bb_checker_static_bad();
	printf("badblock_cnt: %lu\n",badblock_cnt);
	bb_checker_fixing();
	printf("checking done!\n");	
/*
	printf("read badblock checking\n");
	fdriver_lock_init(&bb_lock,0);
	bb_write_bb_checker(li,TESTPAGE);
	bb_read_bb_checker(li,TESTPAGE);
	fdriver_lock(&bb_lock);
	free(data_checker_data);*/
	//exit(1);
	return;
}

void *bb_checker_process(uint64_t bad_seg,uint8_t isbad){
	if(!checker.ent[bad_seg].flag){
		checker.ent[bad_seg].flag=isbad;
		if(isbad){
			badblock_cnt++;
		}
	}
	_cnt++;
	if(_cnt%10==0){
		printf("\rbad_block_checking...[ %lf ]",(double)_cnt/target_cnt*100);
		fflush(stdout);
	}
	return NULL;
}

uint32_t bb_checker_get_segid(){
	/*
	uint32_t res=0;
	if(checker.ent[checker.assign].flag){
		res=checker.ent[checker.assign++].fixed_segnum;
	}else{
		res=checker.ent[checker.assign++].origin_segnum;
	}*/
	return checker.ent[checker.assign++].origin_segnum;
}



uint32_t bb_checker_fixed_segment(uint32_t ppa){
	uint32_t res=ppa/(1<<14);
	if(checker.ent[res].flag){
		return checker.ent[res].fixed_segnum;
	}
	else
		return ppa;
}

uint32_t bb_checker_paired_segment(uint32_t ppa){
	return bb_checker_fixed_segment((ppa/(1<<14)+_NOS)*(1<<14));
}

void bb_checker_fixing(){/*
	printf("bb list------------\n");
	for(int i=0; i<_RNOS; i++){
		if(checker.ent[i].flag){
			printf("[%d]seg can't used\n",i);
		}
	}*/
	printf("_RNOS:%ld\n",_RNOS);
	checker.back_index=_RNOS-1;
	int start_segnum=0; int max_segnum=_RNOS-1;
	while(start_segnum<=max_segnum){
		int test_cnt=0;
		if(checker.ent[start_segnum].flag){ //fix bad block
			while(checker.ent[max_segnum-test_cnt].flag){test_cnt++;}
			max_segnum-=test_cnt;
			if(max_segnum<=start_segnum){
				break;
			}
			checker.ent[start_segnum].fixed_segnum=checker.ent[max_segnum].origin_segnum;
			checker.ent[max_segnum].deprived_from_segnum=checker.ent[start_segnum].origin_segnum;
			max_segnum--;
			test_cnt=0;
		}
		//find pair segment;
		while(checker.ent[max_segnum-test_cnt].flag){
			test_cnt++;
		}
		if(max_segnum<=start_segnum){
			break;
		}

		max_segnum-=test_cnt;
		checker.ent[start_segnum].pair_segnum=checker.ent[max_segnum].origin_segnum;
		checker.ent[max_segnum].pair_segnum=checker.ent[start_segnum].origin_segnum;
		max_segnum--;
		start_segnum++;
	}
	
	printf("TOTAL segnum:%d\n",start_segnum);
	/*
	for(int i=0; i<128; i++){
		if(checker.ent[i].flag){
			printf("[badblock] %d(%d) ",checker.ent[i].fixed_segnum,checker.ent[i].origin_segnum);
		}
		else{
			printf("[normal] %d ",checker.ent[i].origin_segnum);
		}
		printf(" && %d\n",checker.ent[i].pair_segnum);
	}*/
}
