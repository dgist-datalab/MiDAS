#ifndef __RUN_H__
#define __RUN_H__
#include "../../include/settings.h"
#include "sst_file.h"
#include <stdlib.h>
#include "page_manager.h"
/*
 d=data
 m=map
 d(start_data_physical_address)-d-d-d-d-d-...-d-m-m...
 */
typedef struct run{
	uint32_t now_sst_num;
	uint32_t max_sst_num;
	uint32_t start_lba;
	uint32_t end_lba;
	uint32_t start_data_physical_address;
	uint32_t now_contents_num;
	bool moved_originality;
	sst_file *sst_set;
}run;
/*
static inline uint32_t run_file_size(run *r){
	return r->pair_num*LPAGESIZE+r->map_num*PAGESIZE;
}
*/
#define LAST_SST_PTR(run_ptr) (&run_ptr->sst_set[run_ptr->now_sst_num-1])
#define FIRST_SST_PTR(run_ptr) (&run_ptr->sst_set[0])

#define for_each_sst(run_ptr, sst_ptr, idx)\
	for(idx=0, (sst_ptr)=&(run_ptr)->sst_set[0]; idx<(run_ptr)->now_sst_num; \
			idx++, (sst_ptr)=&(run_ptr)->sst_set[idx])

#define for_each_sst_at(run_ptr, sst_ptr, idx)\
	for((sst_ptr)=&(run_ptr)->sst_set[idx]; idx<(run_ptr)->now_sst_num; \
			idx++, (sst_ptr)=&(run_ptr)->sst_set[idx])


run *run_init(uint32_t sst_file_num, uint32_t start_lba, uint32_t end_lba);
sst_file *run_retrieve_sst(run *r, uint32_t lba);
sst_file *run_retrieve_close_sst(run *r, uint32_t lba);
map_range *run_to_MR(run *r, uint32_t* map_num);
void run_space_init(run *,uint32_t map_num, uint32_t start_lba, uint32_t end_lba);
void run_reinit(run *r);
void run_append_sstfile_move_originality(run *_run, sst_file *sstfile);
void run_append_sstfile(run *_run, sst_file *sstfile);
void run_deep_append_sstfile(run *_run, sst_file *sstfile);
void run_free(run *_run);
void run_print(run *r);
void run_content_print(run *r, bool print_sst);
static inline void run_empty_content(run *_run, struct page_manager *pm){
	sst_file *sptr; uint32_t sidx;
	for_each_sst(_run, sptr, sidx){
		sst_destroy_content(sptr, pm);
	}

}
static inline void run_destroy_content(run *_run, struct page_manager *pm){
	sst_file *sptr; uint32_t sidx;
	for_each_sst(_run, sptr, sidx){
		sst_destroy_content(sptr, pm);
	}
	free(_run->sst_set);
}

static inline void run_deep_copy(run *des, run *src){
	sst_file *des_file=des->sst_set;
	*des=*src;
	sst_file *each_src_file;
	uint32_t idx=0;
	for_each_sst(src, each_src_file, idx){
		sst_deep_copy(&des_file[idx], each_src_file);
	}
	des->sst_set=des_file;
}

static inline void run_shallow_copy_move_originality(run *des, run *src){
	if(src->max_sst_num!=des->max_sst_num){
		if(src->max_sst_num > des->max_sst_num){
			sst_file *new_sst_set=(sst_file*)calloc(src->max_sst_num, sizeof(sst_file));
			memcpy(new_sst_set, des->sst_set, des->now_sst_num);
			free(des->sst_set);
			des->sst_set=new_sst_set;
			des->max_sst_num=src->max_sst_num;
		}
	}
	sst_file *des_file=des->sst_set;
	*des=*src;
	sst_file *each_src_file;
	uint32_t idx=0;
	for_each_sst(src, each_src_file, idx){
		sst_shallow_copy_move_originality(&des_file[idx], each_src_file);
	}
	des->sst_set=des_file;
}

static inline void run_shallow_copy(run *des, run *src){
	sst_file *des_file=des->sst_set;
	*des=*src;
	sst_file *each_src_file;
	uint32_t idx=0;
	for_each_sst(des, each_src_file, idx){
		des_file[idx]=*each_src_file;
		des_file[idx].ismoved_originality=true;
	}
	des->sst_set=des_file;
}

#endif
