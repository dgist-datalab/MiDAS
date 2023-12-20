#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include "lsmtree_param_module.h"
#include "plr_memory_calculator.h"
#include "bf_memory_calculator.h"
#include "../helper_algorithm/guard_bf_set.h"
#include "../../../include/settings.h"
#define MIN(a,b) ((a)<(b)?(a):(b))
#define DIR_PATH "/home/kukania/BloomFTL-project/FlashFTLDriver/algorithm/lsmftl/design_knob"
double *plr_dp;
extern float gbf_min_bit;
void init_memory_info(uint32_t error){
	plr_dp=(double*)calloc(1001, sizeof(double));
	char plr_table_map[512]={0,};
    sprintf(plr_table_map,"%s/plr_table/%d.map", DIR_PATH, error);
	int fd=open(plr_table_map, O_CREAT | O_RDWR, 0666);
	if(fd==-1){
		perror("????\n");
		exit(1);
	}
	bool new_file=false;
	if(lseek(fd, 0, SEEK_END)==0){
        new_file=true;
    }   
    lseek(fd,0,SEEK_SET);
	if(new_file){
		EPRINT("please add new table for fpr", true);
		exit(1);
	}

	if(read(fd, plr_dp, 1001*sizeof(double))){

	}else{
		printf("???\n");
	}
/*
	for(uint32_t i=1; i<=1000; i++){
		printf("%u : %lf\n", i, plr_dp[i]);
	}*/
	gbf_set_prepare((float)error/100, 1000000, BLOOM_ONLY);
	close(fd);
}

double bf_memory_per_ent(double ratio){
	return gbf_min_bit;
}

double plr_memory_per_ent(double ratio){
	uint32_t temp=ratio*1000;
	if(temp==0) 
		return 48;
	return plr_dp[temp];
}

void destroy_memory_info(){
	free(plr_dp);
}
