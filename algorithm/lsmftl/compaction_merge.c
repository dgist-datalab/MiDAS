#include "compaction.h"
#include "version.h"
#include "lsmtree.h"
#include "page_manager.h"
#include "io.h"
#include <queue>
#include <list>
#include <stdlib.h>
#include <map>
extern compaction_master *_cm;
extern lsmtree LSM;
extern uint32_t debug_lba;
void *merge_end_req(algo_req *);
void read_map_param_init(read_issue_arg *read_arg, map_range *mr){
	inter_read_alreq_param *param;
	uint32_t param_idx=0;
	for(int i=read_arg->from; i<=read_arg->to; i++){
		//param=compaction_get_read_param(_cm);
		param=(inter_read_alreq_param*)calloc(1, sizeof(inter_read_alreq_param));
		param->map_target=&mr[i];
		mr[i].data=NULL;
		param->data=inf_get_valueset(NULL, FS_MALLOC_R, PAGESIZE);
		fdriver_lock_init(&param->done_lock, 0);
		read_arg->param[param_idx++]=param;
	}
}

bool read_map_done_check(inter_read_alreq_param *param, bool check_page_sst){
	if(check_page_sst){
		param->map_target->data=param->data->value;
	}
	fdriver_lock(&param->done_lock);
	return true;
}

static bool invalid_map_done(inter_read_alreq_param *param){
	param->map_target->data=NULL;
	inf_free_valueset(param->data, FS_MALLOC_R);
	fdriver_destroy(&param->done_lock);
	invalidate_map_ppa(LSM.pm->bm, param->map_target->ppa, true);
	free(param);
	//compaction_free_read_param(_cm, param);
	return true;
}

static bool not_invalid_map_done(inter_read_alreq_param *param){
	param->map_target->data=NULL;
	inf_free_valueset(param->data, FS_MALLOC_R);
	fdriver_destroy(&param->done_lock);
	free(param);
	return true;
}

static inline uint32_t coherence_sst_kp_pair_num(run *r, uint32_t start_idx, uint32_t *end_idx, uint32_t *map_num){
	uint32_t cnt=0;
	uint32_t now_map_num=0;
	sst_file *prev_sptr=NULL;
	sst_file *sptr;
	uint32_t iter_start_idx=start_idx;
	for(; iter_start_idx<r->now_sst_num; iter_start_idx++){
		sptr=&r->sst_set[iter_start_idx];
		if(!prev_sptr){
			prev_sptr=sptr;
			prev_sptr=&r->sst_set[iter_start_idx];
			now_map_num+=prev_sptr->map_num;
			continue;
		}
		if(sst_physical_range_overlap(prev_sptr, sptr)){
			prev_sptr=sptr;
			prev_sptr=&r->sst_set[iter_start_idx];
			now_map_num+=prev_sptr->map_num;
			cnt++;
		}
		else break;
	}

	*end_idx=start_idx+cnt;
	*map_num=now_map_num;

	return ((*map_num)*L2PGAP+(*map_num)*KP_IN_PAGE);
}

static map_range * make_mr_set(sst_file *set, uint32_t start_idx, uint32_t end_idx, uint32_t map_num){
	if(!map_num) return NULL;
	map_range *mr=(map_range*)calloc(map_num,sizeof(map_range));
	map_range *mptr;
	uint32_t idx=0;
	uint32_t mr_idx=0;
	for(uint32_t i=start_idx; i<=end_idx; i++){
		for_each_map_range(&set[i], mptr, idx){
			mr[mr_idx++]=*mptr;
		}
	}
	return mr;
}

static void  bulk_invalidation(run *r, uint32_t* border_idx, uint32_t border_lba){
	uint32_t i=0;
	for(i=(*border_idx); i<r->now_sst_num; i++){
		sst_file *sptr=&r->sst_set[i];
		if(sptr->end_lba<=border_lba){
			for(uint32_t j=sptr->file_addr.piece_ppa; j<sptr->end_ppa * L2PGAP; j++){
				invalidate_piece_ppa(LSM.pm->bm, j, false);
			}
		}
		else{
			break;
		}
	}
	(*border_idx)=i;
}

static uint32_t tiering_invalidation_function(level *des, uint32_t stream_id, uint32_t version,
		key_ptr_pair kp, bool overlap, bool inplace){
	if(overlap){
		invalidate_kp_entry(kp.lba, kp.piece_ppa, version, true);
		return false;
	}
	else{
		uint32_t a, b;
		a=version_map_lba(LSM.last_run_version, kp.lba);
		b=version;
		if(inplace){
			if(version_compare(LSM.last_run_version, a, b) > 0){
				invalidate_kp_entry(kp.lba, kp.piece_ppa, version, true);
				return false;
			}
		}
		else{
			if(des==NULL //for compaction_merge
					|| (des->idx!=0 && !version_belong_level(LSM.last_run_version, a, des->idx-1))){
				if(version_compare(LSM.last_run_version, a, b) > 0){
					invalidate_kp_entry(kp.lba, kp.piece_ppa, version, true);
					return false;
				}
			}
		}
		return true;
	}
}

typedef struct mr_free_set{
	map_range *mr_set;
	run *r;
	uint32_t start_idx;
	uint32_t end_idx;
	uint32_t map_num;
}mr_free_set;

static inline mr_free_set making_MFS(read_issue_arg *arg, map_range *mr_set, run *rptr){
	mr_free_set res={mr_set, rptr, (uint32_t)arg->from, arg->to, arg->max_num-1};
	return res;
}

static inline void map_range_preprocessing(mr_free_set free_set, std::list<mr_free_set>* mr_list){
	run *rptr=free_set.r;
	uint32_t start_sst_idx=free_set.start_idx, end_idx=free_set.end_idx;
	for(uint32_t i=start_sst_idx; i<=end_idx; i++){
		lsmtree_gc_unavailable_set(&LSM, &rptr->sst_set[i], UINT32_MAX);
	}
	mr_list->push_back(free_set);
}

static inline void map_range_postprocessing(std::list<mr_free_set>* mr_list,  uint32_t bound_lba, bool last, bool should_free){
	std::list<mr_free_set>::iterator mr_iter=mr_list->begin();
	for(;mr_iter!=mr_list->end(); ){
		mr_free_set now=*mr_iter;
		if(last || now.mr_set[now.map_num].end_lba <= bound_lba){
			for(uint32_t i=now.start_idx; i<=now.end_idx; i++){
				lsmtree_gc_unavailable_unset(&LSM, &now.r->sst_set[i], UINT32_MAX);
			}
			if(should_free){
				free(now.mr_set);
			}
			mr_list->erase(mr_iter++);
		}
		else{
			break;
		}
	}
}

level* compaction_merge(compaction_master *cm, level *des, uint32_t *idx_set){
	_cm=cm;
	run *new_run=run_init(des->max_sst_num/des->max_run_num, UINT32_MAX, 0);

	LSM.now_merging_run[0]=idx_set[0];
	LSM.now_merging_run[1]=idx_set[1];

	run *older=&des->array[idx_set[0]];
	run	*newer=&des->array[idx_set[1]];


	for(uint32_t i=0; i<MERGED_RUN_NUM; i++){
		version_unpopulate(LSM.last_run_version, idx_set[i], des->idx);
	}
	//version_reinit_early_invalidation(LSM.last_run_version, MERGED_RUN_NUM, idx_set);

	read_issue_arg read_arg1={0,}, read_arg2={0,};
	read_issue_arg read_arg1_prev={0,}, read_arg2_prev={0,};
	read_arg_container thread_arg;
	thread_arg.end_req=merge_end_req;
	thread_arg.arg_set=(read_issue_arg**)malloc(sizeof(read_issue_arg*)*MERGED_RUN_NUM);
	thread_arg.arg_set[0]=&read_arg1;
	thread_arg.arg_set[1]=&read_arg2;
	thread_arg.set_num=MERGED_RUN_NUM;

	uint32_t newer_sst_idx=0, newer_sst_idx_end;
	uint32_t older_sst_idx=0, older_sst_idx_end;
	uint32_t now_newer_map_num=0, now_older_map_num=0;
	//uint32_t newer_borderline=0;
	//uint32_t older_borderline=0;
	uint32_t border_lba;

	uint32_t target_version=version_get_empty_version(LSM.last_run_version, des->idx);
	uint32_t target_ridx=version_to_ridx(LSM.last_run_version, target_version, des->idx);
	sst_pf_out_stream *os_set[MERGED_RUN_NUM]={0,};

	sst_bf_out_stream *bos=NULL;
	sst_bf_in_stream *bis=NULL;

	bool init=true;
	uint32_t max_target_piece_num;
	std::queue<key_ptr_pair> *kpq=new std::queue<key_ptr_pair>();
	std::queue<uint32_t> *locked_seg_q=new std::queue<uint32_t>();
	std::list<mr_free_set> *new_range_set=new std::list<mr_free_set>();
	std::list<mr_free_set> *old_range_set=new std::list<mr_free_set>();
	while(!(older_sst_idx==older->now_sst_num && 
				newer_sst_idx==newer->now_sst_num)){
		now_newer_map_num=now_older_map_num=0;
		max_target_piece_num=0;
		max_target_piece_num+=
			newer_sst_idx<newer->now_sst_num?coherence_sst_kp_pair_num(newer,newer_sst_idx, &newer_sst_idx_end, &now_newer_map_num):0;
		max_target_piece_num+=
			older_sst_idx<older->now_sst_num?coherence_sst_kp_pair_num(older,older_sst_idx, &older_sst_idx_end, &now_older_map_num):0;

		if(bis){
			max_target_piece_num+=(bis->map_data->size()+1+1)*L2PGAP; // buffered + additional mapping
		}

		if(bos){
			max_target_piece_num+=bos->kv_wrapper_q->size()+L2PGAP;
		}

		map_range *newer_mr=make_mr_set(newer->sst_set, newer_sst_idx, newer_sst_idx_end, now_newer_map_num);
		map_range *older_mr=make_mr_set(older->sst_set, older_sst_idx, older_sst_idx_end, now_older_map_num);

		if(newer_mr){
			mr_free_set temp_mr_free_set={newer_mr, newer, newer_sst_idx, newer_sst_idx_end, now_newer_map_num-1};
			map_range_preprocessing(temp_mr_free_set, new_range_set);
		}

		if(older_mr){
			mr_free_set temp_mr_free_set={older_mr, older, older_sst_idx, older_sst_idx_end, now_older_map_num-1};
			map_range_preprocessing(temp_mr_free_set, old_range_set);
		}

		bool last_round_check=((newer_sst_idx_end+1==newer->now_sst_num) && (older_sst_idx_end+1==older->now_sst_num));
		uint32_t total_map_num=now_newer_map_num+now_older_map_num;
		uint32_t read_done=0;
		uint32_t older_prev=0, newer_prev=0;
		read_arg1={0,}; read_arg2={0,};
		while(read_done!=total_map_num){
	/*		if(LSM.global_debug_flag){
				printf("merge cnt:%u round info - o_idx:%u n_idx%u read_done:%u\n", LSM.monitor.compaction_cnt[des->idx+1],older_sst_idx, newer_sst_idx, read_done);
			}*/
			uint32_t shard=LOWQDEPTH/(1+1);

			if(newer_mr){
				read_arg1.from=newer_prev;
				read_arg1.to=MIN(newer_prev+shard, now_newer_map_num-1);
				if(TARGETREADNUM(read_arg1)){
					read_map_param_init(&read_arg1, newer_mr);
				}
			}
			else{
				read_arg1.from=1;
				read_arg1.to=0;
			}
			
			if(older_mr){
				read_arg2.from=older_prev;
				read_arg2.to=MIN(older_prev+shard, now_older_map_num-1);
				if(TARGETREADNUM(read_arg2)){
					read_map_param_init(&read_arg2, older_mr);
				}
			}
			else{
				read_arg2.from=1;
				read_arg2.to=0;
			}

//			printf("read_arg1:%u~%u, read_arg2:%u~%u\n", read_arg1.from, read_arg1.to, read_arg2.from, read_arg2.to);

			//pos setting
			if(init){
				init=false;
				os_set[0]=sst_pos_init_mr(&newer_mr[read_arg1.from], read_arg1.param,
						TARGETREADNUM(read_arg1), 
						idx_set[1],
						read_map_done_check, invalid_map_done);
				os_set[1]=sst_pos_init_mr(&older_mr[read_arg2.from], read_arg2.param,
						TARGETREADNUM(read_arg2), 
						idx_set[0],
						read_map_done_check, invalid_map_done);
			}
			else{
				if(newer_mr){
					sst_pos_add_mr(os_set[0], &newer_mr[read_arg1.from], read_arg1.param,
							TARGETREADNUM(read_arg1));
				}
				if(older_mr){
					sst_pos_add_mr(os_set[1], &older_mr[read_arg2.from], read_arg2.param,
							TARGETREADNUM(read_arg2));
				}
			}

			issue_map_read_sst_job(cm, &thread_arg);
			
			if(newer_mr && older_mr){
				border_lba=MIN(newer_mr[read_arg1.to].end_lba, 
					older_mr[read_arg2.to].end_lba);
			}
			else{
				border_lba=(newer_mr?newer_mr[read_arg1.to].end_lba:
						older_mr[read_arg2.to].end_lba);
			}

			//sorting
			LSM.monitor.merge_valid_entry_cnt+=stream_sorting(NULL, MERGED_RUN_NUM, os_set, NULL, kpq, 
				last_round_check,
				border_lba,/*limit*/
				target_version, 
				true,
				tiering_invalidation_function, false);

			read_done+=TARGETREADNUM(read_arg1)+TARGETREADNUM(read_arg2);
			if(newer_mr){
				newer_prev=read_arg1.to+1;
			}
			if(older_mr){
				older_prev=read_arg2.to+1;
			}
		}

		if(bos==NULL){
			bos=sst_bos_init(read_map_done_check, true);
		}
		if(bis==NULL){
			bis=tiering_new_bis(locked_seg_q, des->idx);
		}

		uint32_t entry_num=issue_read_kv_for_bos_sorted_set(bos, kpq, &border_lba,
				true, idx_set[1], idx_set[0], last_round_check);
		border_lba=issue_write_kv_for_bis(&bis, bos, 
				locked_seg_q, new_run, entry_num, 
				target_version, last_round_check);

//		bulk_invalidation(newer, &newer_borderline, border_lba);
//		bulk_invalidation(older, &older_borderline, border_lba);

		map_range_postprocessing(new_range_set, border_lba, last_round_check, true);
		map_range_postprocessing(old_range_set, border_lba, last_round_check, true);

		newer_sst_idx=newer_sst_idx_end+1;
		older_sst_idx=older_sst_idx_end+1;
		
		read_arg1_prev=read_arg1;
		read_arg2_prev=read_arg2;
	}

	thpool_wait(cm->issue_worker);

	sst_file *last_file;
	if((last_file=bis_to_sst_file(bis))){
		run_append_sstfile_move_originality(new_run, last_file);
		sst_free(last_file, LSM.pm);
	}

	sst_bis_free(bis);
	sst_bos_free(bos, _cm);

	release_locked_seg_q(locked_seg_q);
	LSM.monitor.merge_total_entry_cnt+=os_set[0]->total_poped_num+
		os_set[1]->total_poped_num;

	sst_pos_free(os_set[0]);
	sst_pos_free(os_set[1]);
	delete kpq;
	free(thread_arg.arg_set);

	level *res=level_init(des->max_sst_num, des->max_run_num, des->level_type, des->idx, des->max_contents_num, des->check_full_by_size);

	run *rptr; uint32_t ridx;
	for_each_run_max(des, rptr, ridx){
		if(ridx!=idx_set[0] && ridx!=idx_set[1]){
			if(rptr->now_sst_num){
				level_append_run_copy_move_originality(res, rptr, ridx);
			}
		}
	}

	level_update_run_at_move_originality(res, target_ridx, new_run, true);
	version_populate(LSM.last_run_version, target_version, des->idx);

	run_free(new_run);

	version_poped_update(LSM.last_run_version, des->idx, MERGED_RUN_NUM);

	printf("merge %u,%u to %u\n", idx_set[0], idx_set[1], target_ridx);
	delete new_range_set;
	delete old_range_set;

	LSM.now_merging_run[0]=UINT32_MAX;
	LSM.now_merging_run[1]=UINT32_MAX;
	return res;
}

uint32_t update_read_arg_tiering(uint32_t read_done_flag, bool isfirst,sst_pf_out_stream **pos_set, 
		map_range **mr_set, read_issue_arg *read_arg_set, bool invalid_map,
		uint32_t stream_num, level *src, uint32_t version){
	uint32_t remain_num=0;
	for(uint32_t i=0; i<stream_num; i++){
		if(read_done_flag & (1<<i)) continue;
		else remain_num++;
	}
	uint32_t start_version;
	if(isfirst){
		if(src){
			start_version=version_level_to_start_version(LSM.last_run_version, src->idx);
		}
		else{
			start_version=version;
		}
	}
	for(uint32_t i=0 ; i<stream_num; i++){
		if(read_done_flag & (1<<i)) continue;
		if(!isfirst && read_arg_set[i].to==read_arg_set[i].max_num-1){
			read_arg_set[i].from=read_arg_set[i].to+1;
			read_done_flag|=(1<<i);
			continue;
		}

		if(isfirst){
			read_arg_set[i].from=0;
			read_arg_set[i].to=MIN(read_arg_set[i].from+COMPACTION_TAGS/remain_num-1, 
					read_arg_set[i].max_num-1);
			read_map_param_init(&read_arg_set[i], mr_set[i]);
			pos_set[i]=sst_pos_init_mr(&mr_set[i][read_arg_set[i].from], 
					read_arg_set[i].param, TARGETREADNUM(read_arg_set[i]),
					start_version+stream_num-1-i, //to set ridx_version
					read_map_done_check, invalid_map? invalid_map_done:not_invalid_map_done);
		}
		else{
			read_arg_set[i].from=read_arg_set[i].to+1;
			read_arg_set[i].to=MIN(read_arg_set[i].from+COMPACTION_TAGS/remain_num-1, 
					read_arg_set[i].max_num-1);
			read_map_param_init(&read_arg_set[i], mr_set[i]);
			sst_pos_add_mr(pos_set[i], &mr_set[i][read_arg_set[i].from], 
					read_arg_set[i].param, TARGETREADNUM(read_arg_set[i]));
		}
	}
	return read_done_flag;
}

run* tiering_trivial_move(level *src, uint32_t des_idx, uint32_t max_run_num, 
		uint32_t target_version, bool hot_cold_mode, bool inplace){
	run *src_rptr; uint32_t src_idx;
	uint32_t max_lba=0, min_lba=UINT32_MAX;

	for(uint32_t i=0; i<max_run_num; i++){
		src_idx=version_get_ridx_of_order(LSM.last_run_version, src->idx, i);
		src_rptr=LEVEL_RUN_AT_PTR(src, src_idx);
		if(src_rptr->end_lba > max_lba){
			max_lba=src_rptr->end_lba;
		}
		if(src_rptr->start_lba<min_lba){
			min_lba=src_rptr->start_lba;
		}

		run *comp_rptr; uint32_t comp_idx=src_idx+1;
		for_each_run_at(src, comp_rptr, comp_idx){
			if((src_rptr->start_lba > comp_rptr->end_lba) ||
				(src_rptr->end_lba<comp_rptr->start_lba)){
				continue;
			}
			else{
				return NULL;
			}
		}
	}

	LSM.monitor.trivial_move_cnt++;
	std::map<uint32_t, run*> temp_run;
	for(uint32_t i=0; i<max_run_num; i++){
		src_idx=version_get_ridx_of_order(LSM.last_run_version, src->idx, i);
		src_rptr=LEVEL_RUN_AT_PTR(src, src_idx);
		temp_run.insert(std::pair<uint32_t, run*>(src_rptr->start_lba, src_rptr));
	}

	run *res=run_init(src->max_sst_num, UINT32_MAX, 0);
	std::map<uint32_t, run*>::iterator iter;

	for(iter=temp_run.begin(); iter!=temp_run.end(); iter++){
		run *rptr=iter->second;
		if(target_version!=UINT32_MAX && src->idx!=UINT32_MAX){
			compaction_trivial_move(rptr, target_version, src->idx, des_idx, inplace);
		}
		sst_file *sptr; uint32_t sidx;
		for_each_sst(rptr, sptr, sidx){
			run_append_sstfile_move_originality(res, sptr);
		}
	}
	return res;
}

static inline uint32_t get_border_from_read_arg_set(read_issue_arg *arg_set, map_range **mr_set, 
		uint32_t stream_num, uint32_t read_done){
	uint32_t res=UINT32_MAX;
	if(read_done==(1<<stream_num)-1){
		return res;
	}
	for(int i=0; i<stream_num; i++){
		if(read_done & 1<<i) continue;
		uint32_t target=(arg_set[i].to==arg_set[i].max_num-1) ? UINT32_MAX:mr_set[i][arg_set[i].to].end_lba;
		if(res>target){
			res=target;
		}
	}
	return res;
}

run **compaction_TI2RUN(compaction_master *cm, level *src, level *des, uint32_t merging_num, 
		uint32_t target_demote_version, uint32_t target_keep_version, bool *issequential, bool inplace, bool hot_cold_mode){
	_cm=cm;
	run **new_run;
	if(hot_cold_mode){
		new_run=(run**)calloc(2, sizeof(run*));
		new_run[0]=run_init(src->max_sst_num, UINT32_MAX, 0);
		new_run[1]=run_init(src->max_sst_num, UINT32_MAX, 0);
	}else{
		new_run=(run**)calloc(1, sizeof(run*));
		new_run[0]=run_init(src->max_sst_num, UINT32_MAX, 0);
	}
	uint32_t stream_num=merging_num;
	if((new_run[0]=tiering_trivial_move(src, des->idx, merging_num, target_demote_version, 
					hot_cold_mode, inplace))){
		*issequential=true;
		return new_run;
	}
	*issequential=false;
	
	read_issue_arg *read_arg_set=(read_issue_arg*)calloc(stream_num, sizeof(read_issue_arg));
	read_arg_container thread_arg;
	thread_arg.end_req=merge_end_req;
	thread_arg.arg_set=(read_issue_arg**)calloc(stream_num, sizeof(read_issue_arg*));
	for(int32_t i=stream_num-1; i>=0; i--){
		thread_arg.arg_set[i]=&read_arg_set[i];
	}
	thread_arg.set_num=stream_num;

	map_range **mr_set=(map_range **)calloc(stream_num, sizeof(map_range*));
	/*make it reverse order for stream sorting*/
	for(int32_t i=stream_num-1, j=0; i>=0; i--, j++){
		uint32_t ridx=version_get_ridx_of_order(LSM.last_run_version, src->idx, i);
		uint32_t sst_file_num=src->array[ridx].now_sst_num;
		uint32_t map_num=0;
		for(uint32_t j=0; j<sst_file_num; j++){
			map_num+=src->array[ridx].sst_set[j].map_num;
		}
		read_arg_set[j].max_num=map_num;
		mr_set[j]=make_mr_set(src->array[ridx].sst_set, 0, src->array[ridx].now_sst_num-1, map_num);
	}

	std::queue<key_ptr_pair> *kpq=new std::queue<key_ptr_pair>();
	
	std::queue<uint32_t> *locked_seg_q=new std::queue<uint32_t>();

	std::list<mr_free_set> **MFS_set_ptr=new std::list<mr_free_set> *[stream_num]();
	for(uint32_t i=0; i<stream_num; i++){
		MFS_set_ptr[i]=new std::list<mr_free_set>();
	}

	uint32_t sorting_done=0;
	uint32_t read_done=0;
	sst_pf_out_stream **pos_set=(sst_pf_out_stream **)calloc(stream_num, sizeof(sst_pf_out_stream*));
	sst_bf_out_stream *bos=NULL;
	sst_bf_in_stream **bis=NULL;
	if(hot_cold_mode){
		bis=(sst_bf_in_stream**)calloc(2, sizeof(sst_bf_in_stream*));
	}
	else{
		bis=(sst_bf_in_stream**)calloc(1, sizeof(sst_bf_in_stream*));
	}
	bool isfirst=true;

	uint32_t border_lba=UINT32_MAX;
	while(!(sorting_done==((1<<stream_num)-1) && read_done==((1<<stream_num)-1))){
		read_done=update_read_arg_tiering(read_done, isfirst, pos_set, mr_set,
				read_arg_set, true, stream_num, src, UINT32_MAX);
		
		for(uint32_t k=0; k<stream_num; k++){
			if(read_done & (1<<k)) continue;
			mr_free_set temp_MFS=making_MFS(&read_arg_set[k], mr_set[k], LEVEL_RUN_AT_PTR(src, stream_num-1-k));
			map_range_preprocessing(temp_MFS, MFS_set_ptr[k]);
		}

		bool last_round=(read_done==(1<<stream_num)-1);
		if(!last_round){
			issue_map_read_sst_job(cm, &thread_arg);
		}

		border_lba=get_border_from_read_arg_set(read_arg_set, mr_set, stream_num, read_done);
		if(border_lba==UINT32_MAX){
			last_round=true;
		}

		uint32_t sorted_entry_num=stream_sorting(des, stream_num, pos_set, NULL, kpq, 
				last_round,
				border_lba,/*limit*/
				target_demote_version, 
				true,
				tiering_invalidation_function,
				inplace);

		if(bos==NULL){
			bos=sst_bos_init(read_map_done_check, true);
		}

		if(bis==NULL){
			if(hot_cold_mode){
				bis[0]=tiering_new_bis(locked_seg_q, inplace?src->idx: des->idx);	
				bis[1]=tiering_new_bis(locked_seg_q, inplace?src->idx: des->idx);			
			}
			else{
				bis[0]=tiering_new_bis(locked_seg_q, inplace?src->idx: des->idx);	
			}
		}
		
		if(last_round && sorted_entry_num==0){
			uint32_t read_num=issue_read_kv_for_bos_sorted_set(bos, kpq, &border_lba,
					false, UINT32_MAX, UINT32_MAX, last_round);

			for(uint32_t k=0; k<stream_num; k++){
				map_range_postprocessing(MFS_set_ptr[k], border_lba, last_round, false);
			}

			if(hot_cold_mode){
				border_lba=issue_write_kv_for_bis_hot_cold(&bis, bos, locked_seg_q, new_run, 
						read_num, target_demote_version, target_keep_version, src->idx, last_round);			
			}else{
				border_lba=issue_write_kv_for_bis(&bis[DEMOTE_RUN], bos, locked_seg_q, new_run[DEMOTE_RUN], 
						read_num, target_demote_version, last_round);
			}
		}
		else{
			for(uint32_t moved_num=0; moved_num<sorted_entry_num; ){
				uint32_t read_num=issue_read_kv_for_bos_sorted_set(bos, kpq, &border_lba,
						false, UINT32_MAX, UINT32_MAX, last_round);

				for(uint32_t k=0; k<stream_num; k++){
					map_range_postprocessing(MFS_set_ptr[k], border_lba, last_round, false);
				}

				if(hot_cold_mode){
					border_lba=issue_write_kv_for_bis_hot_cold(&bis, bos, locked_seg_q, new_run, 
							read_num, target_demote_version, target_keep_version, src->idx, last_round);			
				}else{
					border_lba=issue_write_kv_for_bis(&bis[DEMOTE_RUN], bos, locked_seg_q, new_run[DEMOTE_RUN], 
							read_num, target_demote_version, last_round);
				}
				moved_num+=read_num;
			}
		}

		for(uint32_t k=0; k<stream_num; k++){
			map_range_postprocessing(MFS_set_ptr[k], border_lba, last_round, false);
		}

		for(uint32_t i=0; i<stream_num; i++){
			if(!(read_done & (1<<i))) continue;
			if((sorting_done & (1<<i))) continue;
			if(sst_pos_is_empty(pos_set[i])){
				sorting_done |=(1<<i);
			}
		}
		isfirst=false;
	}


	sst_file *last_file;
	if(hot_cold_mode){
		if((last_file=bis_to_sst_file(bis[0]))){
			run_append_sstfile_move_originality(new_run[0], last_file);
			sst_free(last_file, LSM.pm);
		}

		if((last_file=bis_to_sst_file(bis[1]))){
			run_append_sstfile_move_originality(new_run[1], last_file);
			sst_free(last_file, LSM.pm);
		}
	}
	else{
		if((last_file=bis_to_sst_file(bis[0]))){
			run_append_sstfile_move_originality(new_run[0], last_file);
			sst_free(last_file, LSM.pm);
		}
	}


	for(uint32_t i=0; i<stream_num; i++){
		sst_pos_free(pos_set[i]);
	}

	release_locked_seg_q(locked_seg_q);
	for(uint32_t i=0; i<stream_num; i++){
		free(mr_set[i]);
		delete MFS_set_ptr[i];
	}
	delete[] MFS_set_ptr;

	if(hot_cold_mode){
		sst_bis_free(bis[0]);
		sst_bis_free(bis[1]);
	}else{
		sst_bis_free(bis[0]);
	}

	sst_bos_free(bos, _cm);
	delete kpq;
	free(bis);
	free(bos);
	free(pos_set);
	free(mr_set);
	free(thread_arg.arg_set);
	free(read_arg_set);
	return new_run;
}

level* compaction_TI2TI(compaction_master *cm, level *src, level *des, uint32_t target_version){
	_cm=cm;
	bool issequential;
	run **new_run=compaction_TI2RUN(cm, src, des, src->run_num, target_version, UINT32_MAX, &issequential, false, false);
	uint32_t target_run_idx=version_to_ridx(LSM.last_run_version, target_version, des->idx);
	level *res=level_init(des->max_sst_num, des->max_run_num, des->level_type, des->idx, 
			des->max_contents_num, des->check_full_by_size);

	run *rptr;
	uint32_t ridx;
	for_each_run_max(des, rptr, ridx){
		if(rptr->now_sst_num){
			level_append_run_copy_move_originality(res, rptr, ridx);
		}
	}

	level_update_run_at_move_originality(res, target_run_idx, new_run[0], true);

	run_free(new_run[0]);
	free(new_run);

	level_print(res);
	return res;
}

level *compaction_TI2TI_separation(compaction_master *cm, level *src, level *des,
		uint32_t target_version, bool *hot_cold_separation){
	bool issequential;
	uint32_t target_keep_version=version_pick_oldest_version(LSM.last_run_version, src->idx);
	run **new_run=compaction_TI2RUN(cm, src, des, src->run_num-1, 
			target_version, target_keep_version, &issequential, false, true);

	uint32_t target_run_idx=version_to_ridx(LSM.last_run_version, target_version, des->idx);
	level *res=level_init(des->max_sst_num, des->max_run_num, des->level_type, des->idx, 
			des->max_contents_num, des->check_full_by_size);

	uint32_t ridx;
	run * rptr;
	for_each_run_max(des, rptr, ridx){
		if(rptr->now_sst_num){
			level_append_run_copy_move_originality(res, rptr, ridx);
		}
	}
	level_update_run_at_move_originality(res, target_run_idx, new_run[0], true);
	if(issequential) {
		*hot_cold_separation=false;
		return res;
	}

	*hot_cold_separation=true;

	for(uint32_t i=0; i<src->run_num-1; i++){
		uint32_t oldest_version=version_pop_oldest_version(LSM.last_run_version, src->idx);
		version_unpopulate(LSM.last_run_version, oldest_version, src->idx);
		uint32_t src_ridx=version_get_ridx_of_order(LSM.last_run_version, src->idx, oldest_version);
		run_reinit(&src->array[src_ridx]);
	}

	version_poped_update(LSM.last_run_version, des->idx, src->run_num-1);
	
	uint32_t target_lower_version=version_get_empty_version(LSM.last_run_version, src->idx);
	uint32_t target_lower_run_idx=version_to_ridx(LSM.last_run_version, target_lower_version, src->idx);
	level_update_run_at_move_originality(src, target_lower_run_idx, new_run[1], true);
	
	run_free(new_run[1]);
	run_free(new_run[0]);
	free(new_run);
	level_print(res);
	return res;
}

level *compaction_TW_convert_LW(compaction_master *cm, level *src){
	_cm=cm;
	run *new_run=NULL;
	uint32_t stream_num=src->run_num;
	level *res=level_init(src->now_sst_num, 1, LEVELING_WISCKEY, src->idx, src->max_contents_num, src->check_full_by_size);
	if((new_run=tiering_trivial_move(src, UINT32_MAX, src->run_num, UINT32_MAX, false, false))){
		sst_file *sptr; uint32_t sidx;
		for_each_sst(new_run, sptr, sidx){
			level_append_sstfile(res, sptr, true);
		}
		return res;
	}

	read_issue_arg *read_arg_set=(read_issue_arg*)calloc(stream_num, sizeof(read_issue_arg));
	read_arg_container thread_arg;
	thread_arg.end_req=merge_end_req;
	thread_arg.arg_set=(read_issue_arg**)calloc(stream_num, sizeof(read_issue_arg*));
	for(int32_t i=stream_num-1; i>=0; i--){
		thread_arg.arg_set[i]=&read_arg_set[i];
	}
	thread_arg.set_num=stream_num;

	map_range **mr_set=(map_range **)calloc(stream_num, sizeof(map_range*));
	run *rptr; uint32_t ridx; uint32_t set_idx=0, map_num;
	/*make it reverse order for stream sorting*/
	for_each_run_reverse(src, rptr, ridx){
		mr_set[set_idx]=run_to_MR(rptr, &map_num);
		read_arg_set[set_idx].max_num=map_num;
		set_idx++;
	}

	sst_pf_out_stream **pos_set=(sst_pf_out_stream **)calloc(stream_num, sizeof(sst_pf_out_stream*));
	uint32_t read_done=0;
	uint32_t sorting_done=0;
	bool isfirst=true;
	sst_pf_in_stream *pis=sst_pis_init(true, lsmtree_get_target_rhp(src->idx));
	uint32_t target_version=version_level_to_start_version(LSM.last_run_version, src->idx) 
		+ src->run_num-1;

	uint32_t border_lba=UINT32_MAX;
	while(!(sorting_done==((1<<stream_num)-1) && read_done==((1<<stream_num)-1))){
		read_done=update_read_arg_tiering(read_done, isfirst, pos_set, mr_set,
				read_arg_set, true, stream_num, src, UINT32_MAX);
		bool last_round=(read_done==(1<<stream_num)-1);

		if(!last_round){
			issue_map_read_sst_job(cm, &thread_arg);
		}
	
		border_lba=get_border_from_read_arg_set(read_arg_set, mr_set, stream_num, read_done);

		stream_sorting(res, stream_num, pos_set, pis, NULL,
				last_round,
				border_lba,/*limit*/
				target_version, 
				true,
				tiering_invalidation_function, false);

		for(uint32_t i=0; i<stream_num; i++){
			if(!(read_done & (1<<i))) continue;
			if((sorting_done & (1<<i))) continue;
			if(sst_pos_is_empty(pos_set[i])){
				sorting_done |=(1<<i);
			}
		}
		isfirst=false;
	}

	thpool_wait(cm->issue_worker);

	for(uint32_t i=0; i<stream_num; i++){
		free(mr_set[i]);
		sst_pos_free(pos_set[i]);
	}

	sst_pis_free(pis);
	free(pos_set);

	free(mr_set);
	free(thread_arg.arg_set);
	free(read_arg_set);
	return res;
}

static uint32_t filter_invalidation(sst_pf_out_stream *pos, std::queue<key_ptr_pair> *kpq, 
		uint32_t now_version){
	uint32_t valid_num=0;
	while(!sst_pos_is_empty(pos)){
		key_ptr_pair target_pair=sst_pos_pick(pos);
		if(target_pair.lba==UINT32_MAX){
			continue;
		}
		uint32_t recent_version=version_map_lba(LSM.last_run_version, target_pair.lba);
		if(now_version==recent_version || 
				version_compare(LSM.last_run_version, now_version, recent_version)>0){
			kpq->push(target_pair);
			valid_num++;
		}
		else{
			invalidate_kp_entry(target_pair.lba, target_pair.piece_ppa, now_version, true);
		}
		sst_pos_pop(pos);
	}
	return valid_num;
}

static inline void gc_lock_run(run *r){
	sst_file *sptr;
	uint32_t idx;
	for_each_sst(r, sptr, idx){
	//	printf("lock lba:%u ->sidx:%u segidx:%u\n", sptr->end_lba, idx, sptr->end_ppa/_PPS);
		lsmtree_gc_unavailable_set(&LSM, sptr, UINT32_MAX);
	}
}

static inline void gc_unlock_run(run *r, uint32_t *sst_file_num, uint32_t border_lba){
	if(r->now_sst_num <= *sst_file_num) return;
	sst_file *sptr;
	uint32_t idx=*sst_file_num;
	for_each_sst_at(r, sptr, idx){
		if(sptr->end_lba<=border_lba){
	//		printf("unlock lba:%u ->sidx:%u border:%u\n", sptr->end_lba, idx, border_lba);
			lsmtree_gc_unavailable_unset(&LSM, sptr, UINT32_MAX);
		}
		else{
			break;
		}
	}
	*sst_file_num=idx;
}

run *compaction_reclaim_run(compaction_master *cm, run *target_rptr, uint32_t version){
	_cm=cm;
	read_issue_arg read_arg_set;
	read_arg_container thread_arg;
	thread_arg.end_req=comp_alreq_end_req;
	thread_arg.arg_set=(read_issue_arg**)malloc(sizeof(read_issue_arg));
	thread_arg.arg_set[0]=&read_arg_set;
	thread_arg.set_num=1;

	sst_pf_out_stream *pos=NULL;
	map_range *mr_set;
	uint32_t map_num;
	mr_set=run_to_MR(target_rptr, &map_num);
	uint32_t stream_num=1;
	uint32_t read_done=0;
	bool isfirst=true;
	bool last_round=false;
	std::queue<key_ptr_pair> *kpq=new std::queue<key_ptr_pair>();
	std::queue<uint32_t> *locked_seg_q=new std::queue<uint32_t>();
	sst_bf_out_stream *bos=NULL;
	sst_bf_in_stream *bis=NULL;
	uint32_t border_lba=0;

	read_arg_set.max_num=map_num;

	gc_lock_run(target_rptr);

	run *new_run=run_init(target_rptr->now_sst_num, UINT32_MAX, 0);

	uint32_t unlocked_sst_idx=0;
	while(read_done!=(1<<stream_num)-1){
		read_done=update_read_arg_tiering(read_done, isfirst, &pos, &mr_set, 
				&read_arg_set, true, stream_num, NULL, version);
		last_round=(read_done==(1<<stream_num)-1);

		if(!last_round){
			issue_map_read_sst_job(cm, &thread_arg);
		}

		filter_invalidation(pos, kpq, version);

		if(bos==NULL){
			bos=sst_bos_init(read_map_done_check, true);
		}
		if(bis==NULL){
			bis=tiering_new_bis(locked_seg_q, LSM.param.LEVELN-1);	
		}

		uint32_t read_num=issue_read_kv_for_bos_sorted_set(bos, kpq, &border_lba,
				false, UINT32_MAX, UINT32_MAX, last_round);
		border_lba=issue_write_kv_for_bis(&bis, bos, locked_seg_q, new_run, 
				read_num, version, last_round);
		
		gc_unlock_run(target_rptr, &unlocked_sst_idx, border_lba);

		isfirst=false;
	}
	thpool_wait(cm->issue_worker);
	/*finishing*/
	gc_unlock_run(target_rptr, &unlocked_sst_idx, UINT32_MAX);

	sst_file *last_file;
	if((last_file=bis_to_sst_file(bis))){
		run_append_sstfile_move_originality(new_run, last_file);
		sst_free(last_file, LSM.pm);
	}

	sst_bis_free(bis);
	sst_bos_free(bos, _cm);

	release_locked_seg_q(locked_seg_q);
	delete kpq;
	sst_pos_free(pos);
	free(mr_set);
	free(thread_arg.arg_set);
	lsmtree_gc_unavailable_sanity_check(&LSM);
	return new_run;
}

static inline void make_rh_for_trivial_move(sst_file *sptr, 
		std::queue<key_ptr_pair>*kp_q, uint32_t to_lev_idx){
	read_helper_param rhp=lsmtree_get_target_rhp(to_lev_idx);
	rhp.member_num=kp_q->size();
	read_helper *rh=read_helper_init(rhp);

	while(!kp_q->empty()){
		key_ptr_pair target_pair=kp_q->front();
		read_helper_stream_insert(rh, target_pair.lba, target_pair.piece_ppa);
		kp_q->pop();
	}

	read_helper_insert_done(rh);
	sptr->_read_helper=rh;
}

sst_file *trivial_move_processing(run *rptr, sst_pf_out_stream *pos, 
		read_issue_arg *read_arg, 
		uint32_t target_version, uint32_t from_lev_idx, 
		uint32_t to_lev_idx, std::queue<key_ptr_pair> *kp_q, bool make_rh, bool inplace){
	version *v=LSM.last_run_version;
	sst_file *sptr=NULL;
	uint32_t min_lba, max_lba;
	while(!sst_pos_is_empty(pos)){
		key_ptr_pair target_pair=sst_pos_pick(pos);
		if(target_pair.lba==UINT32_MAX){
			continue;
		}

		if(sptr==NULL || !(target_pair.lba >=min_lba && target_pair.lba<=max_lba)){
			if(sptr && make_rh){
				make_rh_for_trivial_move(sptr, kp_q, inplace?from_lev_idx:to_lev_idx);
			}
			sptr=run_retrieve_sst(rptr, target_pair.lba);

			min_lba=sptr->start_lba;
			max_lba=sptr->end_lba;
		}

		uint32_t recent_version=version_map_lba(v, target_pair.lba);
		if(version_belong_level(v, recent_version, from_lev_idx)){
			version_coupling_lba_version(v, target_pair.lba, target_version);
		}

		if(make_rh){
			kp_q->push(target_pair);
		}
		sst_pos_pop(pos);
	}
	return sptr;
}

void compaction_trivial_move(run *rptr, uint32_t target_version, uint32_t from_lev_idx, 
		uint32_t to_lev_idx, bool inplace){
	_cm=LSM.cm;
	read_issue_arg read_arg_set;
	read_arg_container thread_arg;
	thread_arg.end_req=comp_alreq_end_req;
	thread_arg.arg_set=(read_issue_arg**)malloc(sizeof(read_issue_arg));
	thread_arg.arg_set[0]=&read_arg_set;
	thread_arg.set_num=1;

	map_range *mr_set;
	uint32_t map_num=0;
	mr_set=run_to_MR(rptr, &map_num);
	uint32_t stream_num=1;
	uint32_t read_done=0;
	bool isfirst=true;
	bool last_round=false;

	read_helper_param rhp1=lsmtree_get_target_rhp(from_lev_idx);
	read_helper_param rhp2=lsmtree_get_target_rhp(to_lev_idx);
	bool make_rh=rhp1.type!=rhp2.type;

	if(make_rh){ //free rh
		uint32_t sidx;
		sst_file *sptr;
		for_each_sst(rptr, sptr, sidx){
			read_helper_free(sptr->_read_helper);
			sptr->_read_helper=NULL;
		}
	}

	read_arg_set.max_num=map_num;
	sst_pf_out_stream *pos=NULL;

	std::queue<key_ptr_pair> *kp_q=NULL;
	if(make_rh){
		kp_q=new std::queue<key_ptr_pair>();
	}

	sst_file *sptr;
	while(read_done!=(1<<stream_num)-1){
		read_done=update_read_arg_tiering(read_done, isfirst, &pos, &mr_set, &read_arg_set, 
				false, stream_num, NULL, UINT32_MAX);
		last_round=(read_done==(1<<stream_num)-1);
		if(last_round){
			if(sptr && make_rh){
				make_rh_for_trivial_move(sptr, kp_q, inplace?from_lev_idx:to_lev_idx);
			}
			break;
		}

		issue_map_read_sst_job(_cm, &thread_arg);
		sptr=trivial_move_processing(rptr, pos, &read_arg_set, target_version, 
				from_lev_idx, to_lev_idx, kp_q, make_rh, inplace);
		isfirst=false;
	}

	thpool_wait(_cm->issue_worker);

	if(make_rh){ //free rh
		uint32_t sidx;
		sst_file *sptr;
		for_each_sst(rptr, sptr, sidx){
			read_helper_insert_done(sptr->_read_helper);
		}
	}
	
	if(make_rh){
		delete kp_q;
	}
	sst_pos_free(pos);
	free(thread_arg.arg_set);
	free(mr_set);
}

void *merge_end_req(algo_req *req){
	inter_read_alreq_param *r_param;
	key_value_wrapper *kv_wrapper;
	switch(req->type){
		case MAPPINGR:
			r_param=(inter_read_alreq_param*)req->param;
			fdriver_unlock(&r_param->done_lock);
			break;
		case COMPACTIONDATAR:
			kv_wrapper=(key_value_wrapper*)req->param;

			fdriver_unlock(&kv_wrapper->param->done_lock);
			break;

	}
	free(req);
	return NULL;
}

/*
static inline uint32_t TI2_RUN_data_move(sst_bf_out_stream *bos, 
		sst_bf_in_stream **bis, 
		std::queue<key_ptr_pair> *kpq,
		std::list<mr_free_set> ** MFS_set_ptr,
		run **new_run;
		std::<uint32_t> *locked_seg_q,
		uint32_t *org_border_lba,
		uint32_t stream_num,
		uint32_t target_version, 
		bool last_round,
		bool hot_cold_mode){

	uint32_t read_num;
	uint32_t border_lba;
	if(hot_cold_mode){		
		read_num=issue_read_kv_for_bos_sorted_set(bos, kpq, org_border_lba,
				false, UINT32_MAX, UINT32_MAX, last_round);

		for(uint32_t k=0; k<stream_num; k++){
			map_range_postprocessing(MFS_set_ptr[k], MAX(border_lba1, border_lba2), last_round, false);
		}
		
		border_lba=issue_write_kv_for_bis_hot_cold(&bis, bos, locked_seg_q, new_run, 
					read_num1, target_version, last_round);
	}
	else{
		read_num=issue_read_kv_for_bos_sorted_set(bos, kpq, org_border_lba,
				false, UINT32_MAX, UINT32_MAX, last_round);

		for(uint32_t k=0; k<stream_num; k++){
			map_range_postprocessing(MFS_set_ptr[k], border_lba, last_round, false);
		}

		border_lba=issue_write_kv_for_bis(&bis[0], bos, locked_seg_q, new_run[0], 
					read_num1, target_version, last_round);
	}
	*org_border_lba=border_lba;
	return read_num;
}*/
