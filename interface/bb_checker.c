#include "bb_checker.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
bb_checker checker;
volatile uint64_t target_cnt, _cnt, badblock_cnt;
uint32_t array[128];

void bb_checker_start(lower_info *li){
	memset(&checker,0,sizeof(checker));
	target_cnt=_RNOS*64;
	printf("_nos:%ld\n",_NOS);
	
	if(!li->device_badblock_checker){
		return;
	}
	for(uint64_t i=0; i<_RNOS; i++){
		checker.ent[i].origin_segnum=i*_PPS;
		li->device_badblock_checker(i*_PPS,_PPS*PAGESIZE,bb_checker_process);
		//memio_trim(mio,i*(1<<14),(1<<14)*PAGESIZE,bb_checker_process);
	}

	while(target_cnt!=_cnt){}
	printf("\n");
//	bb_checker_process(0,true);
	printf("badblock_cnt: %lu\n",badblock_cnt);
	bb_checker_fixing();
	printf("checking done!\n");

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

uint32_t bb_checker_fix_ppa(uint32_t ppa){
	uint32_t res=ppa;
#ifdef SLC
	uint32_t bus  = res & 0x7;
	uint32_t chip = (res >> 3) & 0x7;
	uint32_t page= (res >> 6) & 0xFF;
	uint32_t block = (res >> 14);
	bool shouldchg=false;

	if(page>=4){
		if(page>=254 || page <6){
			page=page-4;
			shouldchg=true;
		}else if(page%4<2){
			page=page>6?page-6:page;
			shouldchg=true;
		}
	}

	if(shouldchg){	
		block+=_NOS;
	}
	res=bus+(chip<<3)+(page<<6)+(block<<14);
#endif
	
	if(checker.ent[res/_PPS].flag){
		uint32_t origin_remain=res%(_PPS);
		res=checker.ent[res/_PPS].fixed_segnum+origin_remain;
		return res;
	}
	else return res;
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
#ifdef MLC
	for(int i=0; i<1*_NOS; i++){
#else
	for(int i=0; i<2*_NOS; i++){
#endif
		if(checker.ent[i].flag){
			if(i+(_RNOS-checker.back_index)>_RNOS){
				printf("too many bad segment, please down scale the TOTALSIZE\n");
				exit(1);
			}
			else{
				while(checker.ent[checker.back_index].flag){checker.back_index--;}
				checker.ent[i].fixed_segnum=checker.ent[checker.back_index--].origin_segnum;
				printf("bad block %d -> %d\n",checker.ent[i].origin_segnum,checker.ent[i].fixed_segnum);
			}
		}
	}
}

