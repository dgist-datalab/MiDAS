#include "lsmtree.h"
#include "io.h"
#include <stdlib.h>
#include <getopt.h>
#include <math.h>
#include "function_test.h"
#include "segment_level_manager.h"
#include "./design_knob/design_knob.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
extern lsmtree LSM;
void lsmtree_param_defualt_setting(){
	/*
	LSM.param.LEVELN=2;
	LSM.param.mapping_num=(SHOWINGSIZE/LPAGESIZE/KP_IN_PAGE/LSM.param.last_size_factor);
	LSM.param.normal_size_factor=get_size_factor(LSM.param.LEVELN, LSM.param.mapping_num);
	LSM.param.last_size_factor=LSM.param.normal_size_factor;
	LSM.param.version_number=LSM.last_size_factor+1;
	LSM.param.version_enable=true;

	LSM.param.leveling_rhp.type=HELPER_BF_PTR_GUARD;
	LSM.param.leveling_rhp.target_prob=LSM.param.read_amplification;
	LSM.param.leveling_rhp.member_num=KP_IN_PAGE;

	LSM.param.tiering_rhp.type=HELPER_PLR;
	LSM.param.tiering_rhp.slop_bit=8;
	LSM.param.tiering_rhp.range=5;
	LSM.param.tiering_rhp.member_num=KP_IN_PAGE;*/
}

static void print_level_param(){
	printf("[param] # of level: %u\n", LSM.param.LEVELN);
	/*print summary*/
	tree_param tr=LSM.param.tr;

	printf("[param] normal size factor: %f\n", LSM.param.normal_size_factor);
	printf("[param] read amplification: %f\n", LSM.param.read_amplification+1);
	printf("[param] version number :%lu\n", tr.run_num);
	printf("[param] write_buffer ent: %u\n", LSM.param.write_buffer_ent);

	printf("[PERF] WAF:%.2lf+GC\n", tr.WAF);
	printf("[PERF] RAF:%.3lf\n", LSM.param.read_amplification+1);
}

static void print_help(){
	printf("-----help-----\n");
	printf("parameters (L,S,a,b,r,R)\n");
	printf("-L: set total levels of LSM-tree\n");
	printf("-S: set size factor of LSM-tree\n");
	printf("-a: set read amplification (float type)\n");
	//printf("-v: set number of version\n");
	printf("-b: bit num for plr line\n");
	printf("-r: error lange for plr\n");

	printf("-R: set read helper type\n");
	printf("\t%d: %s\n", 0, read_helper_type(0));
	for(int i=0; i<READHELPER_NUM; i++){
		printf("\t%d: %s\n", 1<<i, read_helper_type(1<<i));
	}
}

uint32_t lsmtree_argument_set(int argc, char *argv[]){
	int c;
	uint64_t target_memory_usage_bit=0;
	uint32_t percentage=25;
	while((c=getopt(argc,argv,"mMhH"))!=-1){
		switch(c){
			case 'h':
			case 'H':
				print_help();
				exit(1);
				break;
			case 'm':
			case 'M':
				percentage=atoi(argv[optind]);		
				break;
			default:
				printf("invalid parameters\n");
				exit(1);
				break;
		}
	}

	uint32_t error=TARGETFPR * 100;
	init_memory_info(error);

	target_memory_usage_bit=(uint64_t)((double)RANGE*48/100*percentage);
	LSM.param=lsmtree_memory_limit_to_setting(target_memory_usage_bit);

	printf("------------------------------------------\n");
	print_level_param();
	printf("------------------------------------------\n");
	return 1;
}

void print_tree_param(tree_param *set, uint32_t number){
	printf("LEVEL\tWAF\tsize_factor\trun_num\tmemory\n");
	for(uint32_t i=1; i<=number; i++){
		printf("%u\t%.2lf\t%.2lf\t%lu\t%.2lf\n", 
				i, set[i].WAF, set[i].size_factor, set[i].run_num,
				(double)set[i].memory_usage_bit/(RANGE*48));
	}
}

void print_tree_param_new(tree_param *set, uint32_t number, uint32_t divide){
	printf("BUFF\tLEVEL\tWAF\tsf\trun_num\tmemory\tbuffer\tsum\tTABLE\tENTRY\t\n", (double)divide/divide/divide*100);
	for(uint32_t i=1; i<=number; i++){
		printf("%.4f\t%u\t%.2lf\t%.2lf\t%lu\t%.2lf\t%.2lf\t%.3lf\t%.3f\t%.2f\n", 
				(double)divide/divide/divide*100,
				i, set[i].WAF, set[i].size_factor, set[i].run_num,
				(double)set[i].memory_usage_bit/(RANGE*48),
				(double)((double)RANGE*48/divide)/(RANGE*48),
				(double)((double)set[i].memory_usage_bit+(double)((double)RANGE*48/divide))/(RANGE*48),
				(double)set[i].total_run_num_bit/(RANGE*48),
				(double)set[i].entry_bit/(RANGE*48));
	}
}

void lsmtree_tiering_only_test(){
	for(uint32_t divide=1; divide<RANGE*48/3; divide*=4){
		uint32_t write_buffer_memory_bit=RANGE*48/divide;
		uint32_t buffered_ent=write_buffer_memory_bit/(48+48);
		tree_param *settings=NULL;
		if(buffered_ent==0) break;
		uint32_t chunk_num=RANGE/buffered_ent+(RANGE%buffered_ent?1:0);
		uint32_t max_level=get_level(2, chunk_num);
		max_level=max_level>20?20:max_level;
		settings=(tree_param*)calloc(max_level+1, sizeof(tree_param));

		for(uint32_t i=1; i<=max_level; i++){
			settings[i].size_factor=get_size_factor(i, chunk_num);
			settings[i].num_of_level=i;
			settings[i].memory_usage_bit=ceil(log2(settings[i].size_factor *i)) * RANGE;
			settings[i].total_run_num_bit=ceil(log2(settings[i].size_factor *i)) * RANGE;
			settings[i].WAF=i;
			settings[i].run_num=i*settings[i].size_factor;
			if(settings[i].size_factor<=2) break;
	//		settings[i].lp=(level_param*)calloc(i+1, sizeof(level_param));	
			for(uint32_t j=1; j<=i; j++){
				uint64_t num_range=RANGE;
				uint32_t run_size=buffered_ent * pow(settings[i].size_factor, j-1);
				uint64_t level_size=0;
				/*
				if(j==i){
					level_size=buffered_ent * ceil(pow(settings[i].size_factor, j));
				}
				else{*/
					level_size=buffered_ent * ceil(pow(settings[i].size_factor, j))-pow(settings[i].size_factor, j-1);
				//}
				level_size=level_size>RANGE?RANGE:level_size;
				run_size=run_size>RANGE?RANGE:run_size;
				double run_coverage_ratio=(double)run_size/num_range;
				uint64_t run_memory_usage_bit=level_size * MIN(bf_memory_per_ent(run_coverage_ratio),
						plr_memory_per_ent(run_coverage_ratio));
				
				settings[i].entry_bit+=run_memory_usage_bit;
				settings[i].memory_usage_bit+=run_memory_usage_bit;
			}
		}
		printf("\n");
		print_tree_param_new(settings, max_level, divide);
		free(settings);
	}
}

lsmtree_parameter lsmtree_memory_limit_to_setting(uint64_t memory_limit_bit){
	lsmtree_parameter res;
	uint32_t write_buffer_memory_bit=RANGE*48/512;
	uint32_t buffered_ent=write_buffer_memory_bit/(48+48);
	uint32_t chunk_num=RANGE/buffered_ent+(RANGE%buffered_ent?1:0);
	uint32_t max_level=get_level(2, chunk_num);

	memory_limit_bit-=write_buffer_memory_bit;
	tree_param *settings=(tree_param*)calloc(max_level+1, sizeof(tree_param));
	/*start all leveling*/
	for(uint32_t i=1; i<=max_level; i++){
		settings[i].size_factor=get_size_factor(i, chunk_num);
		settings[i].num_of_level=i;
		settings[i].memory_usage_bit=0;
		settings[i].lp=(level_param*)calloc(i+1, sizeof(level_param));
		for(uint32_t j=i; j>=1; j--){
			uint64_t num_range=RANGE;
			uint64_t covered_range=buffered_ent * ceil(pow(settings[i].size_factor, j)-pow(settings[i].size_factor, j-1));
			covered_range=covered_range>RANGE?RANGE:covered_range;
			double coverage_ratio=(double)covered_range/num_range;
			settings[i].lp[j].level_type=LEVELING;
			settings[i].lp[j].is_wisckey=false;
			settings[i].lp[j].is_bf=false;
			if(bf_memory_per_ent(coverage_ratio) < plr_memory_per_ent(coverage_ratio)){
				settings[i].lp[j].is_bf=true;
			}
			settings[i].memory_usage_bit+= covered_range * 
				MIN(bf_memory_per_ent(coverage_ratio), plr_memory_per_ent(coverage_ratio));
		}
		settings[i].memory_usage_bit+=RANGE*ceil(log2(i));
		settings[i].WAF=i*settings[i].size_factor;
		settings[i].run_num=i;

		if(settings[i].memory_usage_bit > memory_limit_bit){
			settings[i].isinvalid=true;
		}
	}
	/*
	printf("after_leveling\n");
	print_tree_param(settings, max_level);*/

	/*change tiering from bottom*/
	for(uint32_t i=1; i<=max_level; i++){
		if(settings[i].isinvalid) continue;
		for(uint32_t j=i; j>=1; j--){
			uint32_t now_run_num=settings[i].run_num;
			uint64_t num_range=RANGE;
			uint64_t level_size=buffered_ent * ceil(pow(settings[i].size_factor, j)-pow(settings[i].size_factor, j-1));
			uint64_t run_size=buffered_ent * pow(settings[i].size_factor, j-1);

			level_size=level_size>RANGE?RANGE:level_size;
			run_size=run_size>RANGE?RANGE:run_size;
			
			double level_coverage_ratio=(double)level_size/num_range;
			double run_coverage_ratio=(double)run_size/num_range;
			
			uint64_t level_memory_usage_bit= level_size *
				MIN(bf_memory_per_ent(level_coverage_ratio), plr_memory_per_ent(level_coverage_ratio));
			uint64_t run_memory_usage_bit= level_size *
				MIN(bf_memory_per_ent(run_coverage_ratio), plr_memory_per_ent(run_coverage_ratio));
			
			uint64_t prev_table_memory_bit=RANGE*ceil(log2(now_run_num));
			uint64_t table_memory_bit=RANGE * ceil(log2(now_run_num-1+floor(settings[i].size_factor)));
			
			if(settings[i].memory_usage_bit - prev_table_memory_bit - level_memory_usage_bit 
					+ table_memory_bit + run_memory_usage_bit < memory_limit_bit){
				settings[i].lp[j].level_type=TIERING;
				if(bf_memory_per_ent(run_coverage_ratio) < plr_memory_per_ent(run_coverage_ratio)){
					settings[i].lp[j].is_bf=true;
				}
				settings[i].WAF-=(settings[i].size_factor-1); //changing leveling to tiering;
				if(j==i){
					settings[i].WAF+=1;
				}
				settings[i].memory_usage_bit=settings[i].memory_usage_bit-prev_table_memory_bit-level_memory_usage_bit 
					+table_memory_bit+run_memory_usage_bit; //update memory usage
				settings[i].run_num=settings[i].run_num-1+floor(settings[i].size_factor); //update run_num
			}
		}
	}
	/*
	printf("after_tiering\n");
	print_tree_param(settings, max_level);*/
	/*changing wisckey from top*/
	for(uint32_t i=1; i<=max_level; i++){
		if(settings[i].isinvalid) continue;
		for(uint32_t j=1; j<=i; j++){
			uint64_t level_size=buffered_ent * pow(settings[i].size_factor, j);
			if(settings[i].memory_usage_bit + level_size * 48 < memory_limit_bit){
				settings[i].memory_usage_bit+=level_size * 48;
				settings[i].WAF-=(settings[i].lp[j].level_type==LEVELING ? settings[i].size_factor: 1);
				settings[i].lp[j].is_wisckey=true;
				settings[i].lp[j].level_type=settings[i].lp[j].level_type==TIERING?TIERING_WISCKEY:LEVELING_WISCKEY;
			}
		}
	}
	
//	printf("after_wisckey\n");
	print_tree_param(settings, max_level);
	double min_WAF=UINT32_MAX;
	uint32_t target_level=0;
	/*find min WAF and setting params*/
	for(uint32_t i=1; i<=max_level; i++){
		if(settings[i].isinvalid) continue;
		if(min_WAF > settings[i].WAF){
			min_WAF=settings[i].WAF;
			target_level=i;
		}
	}

	for(uint32_t i=1; i<=max_level; i++){
		if(i!=target_level){
			free(settings[i].lp);
		}
	}

	printf("target settings memory usage!\n");
	//print_tree_param(&settings[target_level-1], 1);

	/*setting up lsmtree_parameter*/
	res.tr=settings[target_level];

	res.bf_ptr_guard_rhp.type=HELPER_BF_PTR_GUARD;
	res.bf_ptr_guard_rhp.target_prob=TARGETFPR;
	res.bf_ptr_guard_rhp.member_num=KP_IN_PAGE;

	res.bf_guard_rhp.type=HELPER_BF_ONLY_GUARD;
	res.bf_guard_rhp.target_prob=TARGETFPR;
	res.bf_guard_rhp.member_num=KP_IN_PAGE;

	res.plr_rhp.type=HELPER_PLR;
	res.plr_rhp.slop_bit=8;
	res.plr_rhp.range=(uint32_t)((double)TARGETFPR*100/2);
	res.plr_rhp.member_num=KP_IN_PAGE;


	res.LEVELN=target_level;
	res.mapping_num=chunk_num;
	res.last_size_factor=res.normal_size_factor=res.tr.size_factor;
	res.version_enable=true;
	res.write_buffer_ent=buffered_ent-(buffered_ent/KP_IN_PAGE*L2PGAP);
	res.read_amplification=TARGETFPR;

	res.reclaim_ppa_target=buffered_ent*ceil(pow(settings[target_level].size_factor, target_level-1));

	free(settings);

	lsmtree_tiering_only_test();
	return res;
}
