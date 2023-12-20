#ifndef __COMPACTION_H__
#define __COMPACTION_H__
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../include/utils/tag_q.h"
#include "../../interface/queue.h"
#include "../../interface/interface.h"
#include "../../include/utils/thpool.h"
#include "../../include/sem_lock.h"
#include "key_value_pair.h"
#include "lftl_slab.h"
#include "level.h"
#include "sst_file.h"
#include "lsmtree.h"
#include "sst_block_file_stream.h"
#include "sst_page_file_stream.h"
#include <queue>

#define COMPACTION_TAGS ((QDEPTH)*4)
#define TARGETREADNUM(read_arg) (read_arg.to-read_arg.from+1)
#define COMPACTION_LEVEL_NUM (2)
#define CEILING_TARGET(N, G) ((N)/(G) + (N%G?1:0))

typedef struct inter_read_alreq_param{
	fdriver_lock_t done_lock;
	value_set *data;
	sst_file *target;
	map_range *map_target;
}inter_read_alreq_param;

typedef struct{
	level *des;
	int32_t from;
	uint32_t to;
	uint32_t max_num;
	inter_read_alreq_param *param[COMPACTION_TAGS];
}read_issue_arg;

typedef struct{
	read_issue_arg **arg_set;
	void *(*end_req)(algo_req*);
	uint32_t set_num;
}read_arg_container;

typedef struct key_value_wrapper{ //for data read!
	uint32_t piece_ppa;
	uint32_t prev_version;
	key_value_pair kv_ptr;
	bool free_target_req;
	bool wait_target_req;
	bool no_inter_param_alloc;
	inter_read_alreq_param *param;
}key_value_wrapper;

typedef struct compaction_req{
	int8_t start_level;
	int8_t end_level;
	//key_ptr_pair *target;
	write_buffer *wb;
	void (*end_req)(struct compaction_req* req);
	fdriver_lock_t *done_lock;
	void *param;
	uint32_t tag;
	bool gc_data;
}compaction_req;

typedef struct compaction_master{
	queue *req_q;
	tag_manager *tm;
	pthread_t tid;
	slab_master *sm_comp_alreq;
	threadpool issue_worker;
	std::queue<inter_read_alreq_param*> *read_param_queue;
	inter_read_alreq_param *read_param;
	//slab_master *kv_wrapper_slab;
}compaction_master;

typedef std::queue<sst_file *> sst_queue;

compaction_master *compaction_init(uint32_t compaction_queue_num);
void compaction_free(compaction_master *cm);
void compaction_issue_req(compaction_master *cm, compaction_req *);
void compaction_wait(compaction_master *cm);
//level* compaction_first_leveling(compaction_master *cm, key_ptr_pair *, level *des);
level* compaction_LW2LW(compaction_master *cm, level *src, level *des, uint32_t target_version); //done and debug
level* compaction_LW2TI(compaction_master *cm, level *src, level *des, uint32_t target_version); //done and debug
level* compaction_LW2LE(compaction_master *cm, level *src, level *des, uint32_t target_version);
level* compaction_LW2TW(compaction_master *cm, level *src, level *des, uint32_t target_version);
level* compaction_LE2LE(compaction_master *cm, level *src, level *des, uint32_t target_version);
level* compaction_LE2TI(compaction_master *cm, level *src, level *des, uint32_t target_version);	
level* compaction_TI2TI(compaction_master *cm, level *src, level *des, uint32_t target_version); //done
level *compaction_TI2TI_separation(compaction_master *cm, level *src, level *des,
		uint32_t target_version, bool *hot_cold_separation);
level *compaction_TW_convert_LW(compaction_master *cm, level *src);

level* compaction_merge(compaction_master *cm, level *tiered_level, uint32_t *merge_ridx); //done and debug
run* compaction_reclaim_run(compaction_master *cm, run *r, uint32_t version);
run *compaction_wisckey_to_normal(compaction_master *cm, level *src, 
		run *previous_run, uint32_t *moved_entry_num,  
		uint32_t start_sst_file_idx, uint32_t target_ridx, bool demote);

sst_file *compaction_seq_pagesst_to_blocksst(sst_queue *, uint32_t des_idx, uint32_t target_version);

uint32_t compaction_read_param_remain_num(compaction_master *cm);
inter_read_alreq_param *compaction_get_read_param(compaction_master *cm);
void compaction_free_read_param(compaction_master *cm, inter_read_alreq_param *);
key_value_wrapper *compaction_get_kv_wrapper(uint32_t ppa);
void compaction_free_kv_wrapper(key_value_wrapper *kv_wrap);
//uint32_t compaction_early_invalidation(uint32_t target_version);
void read_sst_job(void *arg, int th_num);
void read_map_param_init(read_issue_arg *read_arg, map_range *mr);
bool read_map_done_check(inter_read_alreq_param *param, bool check_page_sst);

run **compaction_TI2RUN(compaction_master *cm, level *src, level *des, 
		uint32_t merging_num, uint32_t target_demote_version, uint32_t target_keep_version, 
		bool *issequential, bool inplace, bool hot_cold_mode);

uint32_t stream_sorting(level *des, uint32_t stream_num, struct sst_pf_out_stream **os_set, 
		struct sst_pf_in_stream *is, std::queue<key_ptr_pair> *kpq, 
		bool all_empty_stop, uint32_t limit, uint32_t version,
		bool merge_flag,
		uint32_t (*invalidate_function)(level *des, uint32_t taget_idx, uint32_t target_version, key_ptr_pair kp, bool merge, bool inplace), bool inplace);


sst_file *bis_to_sst_file(struct sst_bf_in_stream *bis);
struct sst_bf_in_stream *tiering_new_bis(std::queue<uint32_t> *locked_seg_q, uint32_t level_idx);
void release_locked_seg_q(std::queue<uint32_t> *locked_seg_q);
uint32_t issue_read_kv_for_bos_sorted_set(struct sst_bf_out_stream *bos, 
		std::queue<key_ptr_pair> *kpq,
		uint32_t *border_lba,
		bool merge, uint32_t merge_newer_version, uint32_t merge_older_version,
		bool round_final);

int issue_read_kv_for_bos_stream(struct sst_bf_out_stream *bos, 
		sst_pf_out_stream *pos, 
		uint32_t target_num, 
		uint32_t now_level,
		bool demote,
		bool round_final);

uint32_t issue_write_kv_for_bis(sst_bf_in_stream **bis, struct sst_bf_out_stream *bos, 
		std::queue<uint32_t> *locked_seg_q, run *new_run,
		int32_t entry_num, uint32_t target_version, bool final_);

uint32_t issue_write_kv_for_bis_hot_cold(sst_bf_in_stream ***bis, sst_bf_out_stream *bos, 
		std::queue<uint32_t> *locked_seg_q, run **new_run,
		int32_t entry_num, uint32_t target_demote_version, uint32_t target_keep_version, 
		uint32_t src_idx, bool final_);

void compaction_trivial_move(run *rptr, uint32_t target_version, 
		uint32_t from_lev_idx, uint32_t to_lev_idx, bool inplace);

void compaction_debug_func(uint32_t lba, uint32_t piece_ppa, uint32_t target_ridx, level *des);
void issue_map_read_sst_job(compaction_master *cm, read_arg_container* thread_arg);


void *comp_alreq_end_req(algo_req *req);

static inline compaction_req * alloc_comp_req(int8_t start, int8_t end, write_buffer *wb,
		void (*end_req)(compaction_req *), void *param, bool is_gc_data){
	compaction_req *res=(compaction_req*)malloc(sizeof(compaction_req));
	res->start_level=start; 
	res->end_level=end;
	res->wb=wb;
	res->end_req=end_req;
	res->param=param;
	res->gc_data=is_gc_data;
	return res;
}
#endif
