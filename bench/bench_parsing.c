#include "bench.h"
#include "../include/types.h"
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <vector>
#include <sstream>

static void make_benches(bench_parameters *bp, char *input){
	std::vector<std::string> tokens;
	std::string temp(input);
	std::stringstream check1(temp);
	std::string intermediate;

	while(getline(check1, intermediate, ',')){
		tokens.push_back(intermediate);
	}

	bp->max_bench_num=tokens.size();
	bp->bench_list=(bench_meta*)calloc(bp->max_bench_num, sizeof(bench_meta));
	for(int i=0; i<bp->max_bench_num; i++){
		std::string temp2=tokens[i];
		switch(temp2[0]){
			case 'r':
			case 'R':
				switch(temp2[1]){
					case 'w':
					case 'W':
						if(temp2.length()==3 && (temp2[2]=='U' || temp2[2]=='u')){
							bp->bench_list[i].type=VECTOREDUNIQRSET;		
							break;
						}
						bp->bench_list[i].type=VECTOREDRSET;		
						break;
					case 'R':
					case 'r':
						bp->bench_list[i].type=VECTOREDRGET;		
						break;
				}
				break;
			case 's':
			case 'S':
				switch(temp2[1]){
					case 'w':
					case 'W':
						bp->bench_list[i].type=VECTOREDSSET;		
						break;
					case 'R':
					case 'r':
						bp->bench_list[i].type=VECTOREDSGET;		
						break;
				}
				break;
		}
		bp->bench_list[i].start=0;
		bp->bench_list[i].end=RANGE;
		bp->bench_list[i].number=RANGE;
	}
}

bench_parameters *bench_parsing_parameters(int *argc, char **argv){
	struct option options[]={
		{"data-check",1,0,0},
		{"benchmarks",1,0,0},
		{0,0,0,0}
	};

	char *temp_argv[10];
	int temp_cnt=0;
	for(int i=0; i<*argc; i++){
		if(strncmp(argv[i],"--data-check",strlen("--data-check"))==0) continue;
		if(strncmp(argv[i],"--benchmarks",strlen("--benchmarks"))==0) continue;
		temp_argv[temp_cnt++]=argv[i];
	}
	temp_argv[temp_cnt]=NULL;
	if(temp_cnt==*argc) return NULL;
	int opt;
	int index;
	opterr=0;
	bench_parameters *res=NULL;
	res=(bench_parameters*)calloc(1, sizeof(bench_parameters));

	while((opt=getopt_long(*argc,argv,"",options,&index))!=-1){
		switch(opt){
			case 0:
				switch(index){
					case 0: //data-check flag
						if(optarg!=NULL){
							if(optarg[0]=='t' || optarg[0]=='T'){
								res->data_check_flag=true;
							}
							else{
								res->data_check_flag=false;
							}
						}
						break;
					case 1: //bench marks
						if(optarg!=NULL){
							make_benches(res, optarg);
						}
						break;
				}
				break;
			default:
				break;
		}
	}

	for(int i=0; i<=temp_cnt; i++){
		argv[i]=temp_argv[i];
	}
	*argc=temp_cnt;
	optind=0;
	return res;
}

void bench_parameters_free(bench_parameters* bp){
	free(bp->bench_list);
	free(bp);
}
