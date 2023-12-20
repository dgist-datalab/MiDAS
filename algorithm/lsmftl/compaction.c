#include "compaction.h"
#include "run.h"
#include "../../include/sem_lock.h"
#include "../../interface/interface.h"
#include "sst_page_file_stream.h"
#include "io.h"
#include "function_test.h"
#include <algorithm>
extern lsmtree LSM;

compaction_master *_cm;
extern uint32_t test_key;
//uint32_t debug_lba=3967;
uint32_t debug_lba=test_key;

extern uint32_t debug_piece_ppa;

void compaction_debug_func(uint32_t lba, uint32_t piece_ppa, uint32_t version, level *des){
#ifdef LSM_DEBUG
	static int cnt=0;
	if(lba==debug_lba){
		if(piece_ppa==debug_piece_ppa){
			printf("[GOLDEN-same_pice_ppa]");
		}
		if(des){
			printf("[%d] %u,%u (l,p) -> version-number:%u lev:%u\n",++cnt, lba,piece_ppa, version, des->idx);
		}
		else{
			printf("[%d] %u,%u (l,p) -> merging to %u\n",++cnt, lba,piece_ppa, version);
		}
	}
#endif
}

static inline void compaction_error_check
(key_ptr_pair *kp_set, level *src, level *des, level *res, uint32_t compaction_num){
	uint32_t min_lba=kp_set?
		MIN(kp_set[0].lba, GET_LEV_START_LBA(des)):
		MIN(GET_LEV_START_LBA(des), GET_LEV_START_LBA(src));

	uint32_t max_lba=kp_set?
		MAX(kp_get_end_lba((char*)kp_set), GET_LEV_END_LBA(des)):
		MAX(GET_LEV_END_LBA(des), GET_LEV_END_LBA(src));

	if(max_lba==UINT32_MAX){
		kp_get_end_lba((char*)kp_set);
	}

	uint32_t target_version=version_level_to_start_version(LSM.last_run_version, des->idx);
	if(!(GET_LEV_START_LBA(res) <= min_lba)){
		if(target_version==version_map_lba(LSM.last_run_version, min_lba)){
			if(src){
				printf("src\n");
				level_print(src);
			}
			printf("des\n");
			level_print(des);
			printf("res\n");
			level_print(res);
			if(kp_set){
				printf("first_compaction_cnt:%u\n", compaction_num);
			}
			else{
				printf("leveling_compaction_cnt:%u\n", compaction_num);
			}
			EPRINT("range error", true);
		}
	}
	if(!(GET_LEV_END_LBA(res)>=max_lba)){
		if(target_version==version_map_lba(LSM.last_run_version,max_lba)){
			printf("src\n");
			level_print(src);
			printf("des\n");
			level_print(des);
			printf("res\n");
			level_print(res);
			if(kp_set){
				printf("first_compaction_cnt:%u\n", compaction_num);
			}
			else{
				printf("leveling_compaction_cnt:%u\n", compaction_num);
			}
			EPRINT("range error", true);
		}
	}

}

static inline void tiering_compaction_error_check(level *src, run *r1, run *r2, run *res,
		uint32_t compaction_num){
#ifdef LSM_DEBUG
	uint32_t min_lba, max_lba;
	if(src){
		min_lba=GET_LEV_START_LBA(src);
		max_lba=GET_LEV_END_LBA(src);
	}
	else{
		min_lba=MIN(r1->start_lba, r2->start_lba);	
		max_lba=MAX(r1->start_lba, r2->start_lba);	
	}	

	if(!(res->start_lba==min_lba && res->end_lba==max_lba)){
		if(src){
			printf("tiering_compaction_cnt:%u\n",compaction_num);
		}
		else{
			printf("merge_compaction_cnt:%u\n",compaction_num);	
		}

		if(res->start_lba!=min_lba){
			if(version_map_lba(LSM.last_run_version,res->start_lba)!=
				version_map_lba(LSM.last_run_version,min_lba)){
				goto out;
			}
		}

		if(res->end_lba!=max_lba){
			if(version_map_lba(LSM.last_run_version,res->end_lba)!=
				version_map_lba(LSM.last_run_version, max_lba)){
				goto out;
			}
		}
		EPRINT("range error", true);
	}
	out: return;
#endif
}

void read_sst_job(void *arg, int th_num){
	read_arg_container *thread_arg=(read_arg_container*)arg;
	read_issue_arg **arg_set=thread_arg->arg_set;
	uint32_t *idx_set=(uint32_t*)calloc(thread_arg->set_num, sizeof(uint32_t));
	inter_read_alreq_param ***param_set=
		(inter_read_alreq_param ***)calloc(thread_arg->set_num, sizeof(inter_read_alreq_param**));

	for(uint32_t i=0; i<thread_arg->set_num; i++){
		param_set[i]=arg_set[i]->param;
		idx_set[i]=arg_set[i]->from;
	}

	uint32_t remain_checker=0, target=(1<<(thread_arg->set_num))-1;
	while(!(remain_checker==target)){
		for(uint32_t i=0; i<thread_arg->set_num; i++){
			if(idx_set[i]>(arg_set[i]->to)){
				remain_checker|=1<<i;
			}

			if(remain_checker&1<<i) continue;
			inter_read_alreq_param *now=param_set[i][idx_set[i]-arg_set[i]->from];
			algo_req *read_req=(algo_req*)malloc(sizeof(algo_req));
			//read_req->ppa=now->target->piece_ppa;
			read_req->type=MAPPINGR;
			read_req->param=(void*)now;
			read_req->end_req=thread_arg->end_req;
			if(now->target){
				io_manager_issue_read(now->target->file_addr.map_ppa, 
					now->data, read_req, false);
			}
			else if(now->map_target){
				io_manager_issue_read(now->map_target->ppa, 
					now->data, read_req, false);
			}
			else{
				EPRINT("should_check target!", true);
			}	

			idx_set[i]++;
		}
	}
	free(idx_set);
	free(param_set);
}

void issue_map_read_sst_job(compaction_master *cm, read_arg_container *thread_arg){
#ifdef LSM_DEBUG
	/*sanity check*/
	for(uint32_t i=0; i<thread_arg->set_num; i++){
		read_issue_arg *now_issue=thread_arg->arg_set[i];
		for(uint32_t j=now_issue->from, k=0; j<=now_issue->to; j++, k++){
			if(now_issue->param[k]->target){
				continue;
			}
			if(now_issue->param[k]->map_target){
				continue;
			}
			abort();
		}
	}
#endif
	thpool_add_work(cm->issue_worker, read_sst_job, (void*)thread_arg);
}

static void read_param_init(read_issue_arg *read_arg){
	inter_read_alreq_param *param;
	for(int i=0; i<read_arg->to-read_arg->from+1; i++){
		param=compaction_get_read_param(_cm);
		param->target=LEVELING_SST_AT_PTR(read_arg->des, read_arg->from+i);
		param->data=inf_get_valueset(NULL, FS_MALLOC_R, PAGESIZE);
		fdriver_lock_init(&param->done_lock, 0);
		read_arg->param[i]=param;
	}
}

bool read_done_check(inter_read_alreq_param *param, bool check_page_sst){
	if(check_page_sst){
		param->target->data=param->data->value;
	}
	fdriver_lock(&param->done_lock);
	return true;
}

bool file_done(inter_read_alreq_param *param){
	param->target->data=NULL;
	inf_free_valueset(param->data, FS_MALLOC_R);
	fdriver_destroy(&param->done_lock);
	invalidate_map_ppa(LSM.pm->bm, param->target->file_addr.map_ppa, true);
	compaction_free_read_param(_cm, param);
	return true;
}

static void write_sst_file(sst_pf_in_stream *is, level *des){ //for page file
	sst_file *sptr;
	value_set *vs=sst_pis_get_result(is, &sptr);
	sptr->file_addr.map_ppa=page_manager_get_new_ppa(LSM.pm,true,MAPSEG);
	sptr->start_piece_ppa=((key_ptr_pair*)vs->value)[0].piece_ppa;
	validate_map_ppa(LSM.pm->bm, sptr->file_addr.map_ppa, sptr->start_lba, sptr->end_lba,true);
	
	algo_req *write_req=(algo_req*)malloc(sizeof(algo_req));
	write_req->type=MAPPINGW;
	write_req->param=(void*)vs;
	write_req->end_req=comp_alreq_end_req;

	io_manager_issue_internal_write(sptr->file_addr.map_ppa, vs, write_req, false);
	sptr->data=NULL;
	level_append_sstfile(des, sptr, true);
	sst_free(sptr, LSM.pm);
}

static uint32_t leveling_invalidation_function(level *des, uint32_t stream_id, uint32_t version, 
		key_ptr_pair kp, bool overlap, bool inplace){
	if(overlap){
		if(slm_invalidate_enable(des->idx, kp.piece_ppa)){
			invalidate_kp_entry(kp.lba, kp.piece_ppa, version, true);
			return false;
		}
	}
	else{
		uint32_t lba_version=version_map_lba(LSM.last_run_version, kp.lba);
		if(inplace){
			if(lba_version!=UINT8_MAX  && lba_version > version){
				invalidate_kp_entry(kp.lba, kp.piece_ppa, version, true);
				return false;
			}
		}
		else{
			if(lba_version!=UINT8_MAX && lba_version > version+1){
				if(stream_id==0 && des->idx!=0 && slm_invalidate_enable(des->idx-1, kp.piece_ppa)){
					invalidate_kp_entry(kp.lba, kp.piece_ppa, version, true);
				}
				else if(stream_id==1 && slm_invalidate_enable(des->idx, kp.piece_ppa)){
					invalidate_kp_entry(kp.lba, kp.piece_ppa, version, true);
				}
				return false;
			}
		}
	}
	return true;
}

uint32_t stream_sorting(level *des, uint32_t stream_num, sst_pf_out_stream **os_set, 
		sst_pf_in_stream *is, std::queue<key_ptr_pair> *kpq, 
		bool all_empty_stop, uint32_t limit, uint32_t target_version,
		bool merge_flag,
		uint32_t (*invalidate_function)(level *des, uint32_t stream_id, uint32_t target_version, key_ptr_pair kp, bool overlap, bool inplace),
		bool inplace){
	bool one_empty=false;
	if(stream_num >= 32){
		EPRINT("too many stream!!", true);
	}
	
	uint32_t all_empty_check=0;
	uint32_t sorting_idx=0;
	//uint32_t target_version=UINT32_MAX;

	while(!((all_empty_stop && all_empty_check==((1<<stream_num)-1)) || (!all_empty_stop && one_empty))){
		key_ptr_pair target_pair;
		target_pair.lba=UINT32_MAX;
		uint32_t target_idx=UINT32_MAX;
		for(uint32_t i=0; i<stream_num; i++){
			if(all_empty_check & 1<<i) continue;
			if(!os_set[i]){
				all_empty_check|=(1<<i);
				continue;
			}
			key_ptr_pair now=sst_pos_pick(os_set[i]);
			
			if(target_idx==UINT32_MAX){
				 target_pair=now;
				 target_idx=i;
			}
			else{
				if(target_pair.lba > now.lba){
					target_pair=now;
					target_idx=i;
				}
				else if(target_pair.lba!=UINT32_MAX && target_pair.lba==now.lba){
					invalidate_function(des, i, os_set[i]->version_idx, now, true, inplace);
					sst_pos_pop(os_set[i]);
					continue;
				}
				else{
					continue;
				}
			}
		}
		
		if((!all_empty_stop) && target_pair.lba>limit){
			break;
		}

		if(target_pair.lba!=UINT32_MAX){
			
			compaction_debug_func(target_pair.lba, target_pair.piece_ppa, target_version, des);
		
			uint32_t query_version;
			if(inplace){
				query_version=os_set[target_idx]->version_idx;
			}
			else{
				query_version=merge_flag?os_set[target_idx]->version_idx: target_version;
			}
			if(invalidate_function(des, target_idx, query_version, target_pair, false, inplace)){
				if(kpq){
					sorting_idx++;
					kpq->push(target_pair);
				}
				else if(is){
					version_coupling_lba_version(LSM.last_run_version, 
							target_pair.lba, target_version);
					sorting_idx++;
					if(sst_pis_insert(is, target_pair)){
						write_sst_file(is, des);
					}
				}else{
					EPRINT("plz set one of two(kpq or is)", true);
				}
			}
			sst_pos_pop(os_set[target_idx]);
		}
		if(sst_pos_is_empty(os_set[target_idx])){
			one_empty=true;
			all_empty_check|=1<<target_idx;
		}

	}

	if(is && all_empty_stop && sst_pis_remain_data(is)){
		write_sst_file(is,des);
	}
	return sorting_idx;
}

static sst_file *key_ptr_to_sst_file(key_ptr_pair *kp_set, bool should_flush){
	uint32_t map_ppa=UINT32_MAX;
	uint32_t end_idx=kp_end_idx((char*)kp_set);
	if(should_flush){
		value_set *vs=inf_get_valueset((char*)kp_set, FS_MALLOC_W, PAGESIZE);
		map_ppa=page_manager_get_new_ppa(LSM.pm, false, DATASEG); //DATASEG for sequential tiering 
		validate_map_ppa(LSM.pm->bm, map_ppa, kp_set[0].lba,  kp_set[end_idx].lba, true);
		algo_req *write_req=(algo_req*)malloc(sizeof(algo_req));
		write_req->type=MAPPINGW;
		write_req->param=(void*)vs;
		write_req->end_req=comp_alreq_end_req;
		io_manager_issue_internal_write(map_ppa, vs, write_req, false);
	}
	sst_file *sstfile=sst_pf_init(map_ppa, kp_set[0].lba, kp_set[end_idx].lba);
	if(SEGNUM(kp_set[0].piece_ppa)==SEGNUM(kp_set[end_idx].piece_ppa) && SEGNUM(kp_set[0].piece_ppa)==map_ppa/_PPS){
		sstfile->sequential_file=true;
	}

	return sstfile;
}

static void leveling_update_meta_for_trivial_move(level *up, level *down, uint32_t target_version){
	if(up->idx==UINT32_MAX){
		std::map<uint32_t, uint32_t>::iterator iter=LSM.flushed_kp_set->begin();
		uint32_t sidx=0;
		sst_file *sptr=NULL;
		uint32_t min_lba, max_lba;
		for(; iter!=LSM.flushed_kp_set->end(); iter++){
			if(sptr==NULL || !(min_lba<=iter->first && iter->first<=max_lba)){
				if(sptr){
					read_helper_insert_done(sptr->_read_helper);
				}
				sptr=LEVELING_SST_AT_PTR(up, sidx);
				min_lba=sptr->start_lba;
				max_lba=sptr->end_lba;
				sptr->_read_helper=read_helper_init(lsmtree_get_target_rhp(down->idx));
				sidx++;
			}
			version_coupling_lba_version(LSM.last_run_version, iter->first, target_version);
			if(min_lba<=iter->first && iter->first<=max_lba){
				read_helper_stream_insert(sptr->_read_helper, iter->first, iter->second);
			}
		}
		//for last sptr;
		if(sptr){
			read_helper_insert_done(sptr->_read_helper);
		}
	}
	else{
		compaction_trivial_move(&up->array[0], target_version, up->idx, down->idx, false);
	}
}

static void leveling_trivial_move(key_ptr_pair *kp_set,level *up, level *down, 
		level *des, uint32_t target_version){
	uint32_t ridx, sidx;
	LSM.monitor.trivial_move_cnt++;
	run *rptr; sst_file *sptr;
	if(kp_set){
		sst_file *file=key_ptr_to_sst_file(kp_set, true);
		file->_read_helper=read_helper_kpset_to_rh(
				lsmtree_get_target_rhp(des->idx), 
				kp_set);
		if(down->now_sst_num==0){
			level_append_sstfile(des, file, true);
		}
		else{
			if(LAST_RUN_PTR(down)->end_lba < file->start_lba){
				for_each_sst_level(down, rptr, ridx, sptr, sidx){
					level_append_sstfile(des, sptr, true);
				}
			}
			level_append_sstfile(des,file, true);
			if(FIRST_RUN_PTR(down)->start_lba > file->end_lba){
				for_each_sst_level(down, rptr, ridx, sptr, sidx){
					level_append_sstfile(des, sptr, true);
				}
			}
		}

		for(uint32_t i=0; i<KP_IN_PAGE && kp_set[i].lba!=UINT32_MAX; i++){
			version_coupling_lba_version(LSM.last_run_version, kp_set[i].lba, target_version);
		}
		sst_free(file, LSM.pm);
	}
	else{
		leveling_update_meta_for_trivial_move(up, down, target_version);
		if(down->now_sst_num==0){
			for_each_sst_level(up, rptr, ridx, sptr, sidx){
				level_append_sstfile(des, sptr, true);
			}
		}
		else{
			if(LAST_RUN_PTR(down)->end_lba < FIRST_RUN_PTR(up)->start_lba){
				for_each_sst_level(down, rptr, ridx, sptr, sidx){
					level_append_sstfile(des, sptr, true);
				}
			}

			for_each_sst_level(up, rptr, ridx, sptr, sidx){
				level_append_sstfile(des, sptr, true);
			}

			if(FIRST_RUN_PTR(down)->start_lba > LAST_RUN_PTR(up)->end_lba){
				for_each_sst_level(down, rptr, ridx, sptr, sidx){
					level_append_sstfile(des, sptr, true);
				}
			}
		}
	}
}

static void compaction_move_unoverlapped_sst
(key_ptr_pair* kp_set, level *up, uint32_t up_start_sidx, 
 level *down, level *des, uint32_t* start_idx){
	bool is_close_target=false;
	uint32_t target_start_lba=kp_set?kp_set[0].lba:(LEVELING_SST_AT_PTR(up, up_start_sidx))->start_lba;
	sst_file *start_sst_file=level_retrieve_sst(down, target_start_lba);
	if(!start_sst_file){
		is_close_target=true;
		start_sst_file=level_retrieve_close_sst(down, target_start_lba);
		if(!start_sst_file){
			*start_idx=0;
			return;
		}
	}

	uint32_t _start_idx=GET_SST_IDX(down, start_sst_file);
	for(uint32_t i=0; i<_start_idx; i++){
		sst_file *sptr=LEVELING_SST_AT_PTR(down, i);
		level_append_sstfile(des, sptr, true);

		version_update_for_trivial_move(LSM.last_run_version, 
				sptr->start_lba, 
				sptr->end_lba,
				up? version_level_to_start_version(LSM.last_run_version, up->idx):UINT32_MAX,
				down->idx,
				version_level_to_start_version(LSM.last_run_version, down->idx));
	}
	if(is_close_target){
		sst_file *sptr=LEVELING_SST_AT_PTR(down, _start_idx);
		level_append_sstfile(des, sptr, true);
		version_update_for_trivial_move(LSM.last_run_version, 
				sptr->start_lba, 
				sptr->end_lba,
				up?version_level_to_start_version(LSM.last_run_version, up->idx):UINT32_MAX,
				down->idx,
				version_level_to_start_version(LSM.last_run_version, down->idx));
		_start_idx++;
	}

	*start_idx=_start_idx;
}


static void read_params_setting(read_issue_arg *read_arg1, read_issue_arg *read_arg2, level *src, 
		level *des, sst_file *sptr, uint32_t* sidx, uint32_t des_start_idx, 
		uint32_t* des_end_idx, uint32_t now_remain_tags){
	read_arg1->from=*sidx;
	read_arg2->from=des_start_idx;
	uint32_t round=*sidx;
	do{
		sst_file *down_target=level_retrieve_sst(des, sptr->end_lba);
		if(!down_target){
			down_target=level_retrieve_close_sst(des, sptr->end_lba);
		}
		if((round-read_arg1->from+1)+(GET_SST_IDX(des,down_target)-read_arg2->from+1) < now_remain_tags){
			read_arg1->to=round;
			read_arg2->to=GET_SST_IDX(des, down_target);
			*des_end_idx=read_arg2->to;

			(*sidx)++;
			if(!(*sidx<src->now_sst_num)){
				(*sidx)--;
				break;
			}
			sptr=LEVELING_SST_AT_PTR(src, (*sidx));
			round++;
		}
		else{
			break;
		}
	}while(1);

	read_param_init(read_arg1);
	read_param_init(read_arg2);
}

level* compaction_LW2LW(compaction_master *cm, level *src, level *des, uint32_t target_version){
	static uint32_t debug_cnt_flag=0;
	_cm=cm;
	level *res=level_init(des->max_sst_num, des->run_num, des->level_type, des->idx, des->max_contents_num, des->check_full_by_size);

	if(!level_check_overlap(src, des)){
		leveling_trivial_move(NULL,src, des, res, target_version);
		return res;
	}

	read_issue_arg read_arg1, read_arg2;
	read_arg1.des=src;
	read_arg2.des=des;

	read_arg_container thread_arg;
	thread_arg.end_req=comp_alreq_end_req;
	thread_arg.arg_set=(read_issue_arg**)malloc(sizeof(read_issue_arg*)*COMPACTION_LEVEL_NUM);
	thread_arg.arg_set[0]=&read_arg1;
	thread_arg.arg_set[1]=&read_arg2;
	thread_arg.set_num=COMPACTION_LEVEL_NUM;

	sst_pf_out_stream *os_set[COMPACTION_LEVEL_NUM];
	sst_pf_in_stream *is=sst_pis_init(true, 
			lsmtree_get_target_rhp(des->idx)
			);
	
	uint32_t upper_version=version_pick_oldest_version(LSM.last_run_version, src->idx);
	uint32_t lower_version=version_pick_oldest_version(LSM.last_run_version, des->idx);
	uint32_t des_start_idx=0;
	uint32_t des_end_idx=0;
	compaction_move_unoverlapped_sst(NULL, src, 0, des, res, &des_start_idx);

	uint32_t target_map_page_num=src->now_sst_num+(des->now_sst_num-des_start_idx);
	if(page_manager_is_gc_needed(LSM.pm, target_map_page_num, true)){
		__do_gc(LSM.pm, true, target_map_page_num);
	}

	run *rptr; sst_file *sptr;
	uint32_t ridx, sidx;
	bool isstart=true;
	uint32_t stream_cnt=0;
	for_each_sst_level(src, rptr, ridx, sptr, sidx){
		/*set read_arg start*/

		read_params_setting(&read_arg1, &read_arg2, src, des, sptr, &sidx, des_start_idx, 
				&des_end_idx, compaction_read_param_remain_num(_cm));
		sidx=read_arg1.to;

		if(isstart){
			os_set[0]=sst_pos_init_sst(LEVELING_SST_AT_PTR(src, read_arg1.from), read_arg1.param, 
					read_arg1.to-read_arg1.from+1, 
					upper_version,
					read_done_check, file_done);
			os_set[1]=sst_pos_init_sst(LEVELING_SST_AT_PTR(des, read_arg2.from), read_arg2.param,
					lower_version,
					read_arg2.to-read_arg2.from+1, read_done_check, file_done);
		}
		else{
			sst_pos_add_sst(os_set[0], LEVELING_SST_AT_PTR(src, read_arg1.from), read_arg1.param,
					read_arg1.to-read_arg1.from+1);
			sst_pos_add_sst(os_set[1], LEVELING_SST_AT_PTR(des, read_arg2.from), read_arg2.param,
					read_arg2.to-read_arg2.from+1);
		}

			/*
			printf("%u ~ %u && %u~%u\n",
					LEVELING_SST_AT(src,read_arg1.from).start_lba,
					LEVELING_SST_AT(src,read_arg1.to).end_lba,
					LEVELING_SST_AT(des,read_arg2.from).start_lba,
					LEVELING_SST_AT(des,read_arg2.to).end_lba
					);	
			*/

		/*send read I/O*/
		issue_map_read_sst_job(cm, &thread_arg);

		stream_sorting(res, COMPACTION_LEVEL_NUM, os_set, is, NULL, sidx==src->now_sst_num-1, 
				MIN(LEVELING_SST_AT(src,read_arg1.to).end_lba, LEVELING_SST_AT(des,read_arg2.to).end_lba),
				version_level_to_start_version(LSM.last_run_version, res->idx),
				false,
				leveling_invalidation_function, false);

		isstart=false;
		des_start_idx=des_end_idx+1;
		stream_cnt++;
	}

	for(uint32_t i=des_start_idx; i<des->now_sst_num; i++){
		level_append_sstfile(res, LEVELING_SST_AT_PTR(des, i), true);
	}

	thpool_wait(cm->issue_worker);
	sst_pos_free(os_set[0]);
	sst_pos_free(os_set[1]);

	sst_pis_free(is);

	/*level error check*/
	
	compaction_error_check(NULL, src, des, res, debug_cnt_flag++);
	free(thread_arg.arg_set);
	_cm=NULL;
	return res;

}

 sst_file *bis_to_sst_file(sst_bf_in_stream *bis){
	if(bis->map_data->size()==0) return NULL;
	sst_file *res=sst_init_empty(BLOCK_FILE);
	uint32_t map_num=0;
	uint32_t ppa;
	map_range *mr_set=(map_range*)malloc(sizeof(map_range) * 
			bis->map_data->size());
	uint32_t mr_idx=0;
	while(bis->map_data->size()){
		value_set *data=bis->map_data->front();
		algo_req *write_req=(algo_req*)malloc(sizeof(algo_req));
		write_req->type=COMPACTIONDATAW;
		write_req->param=(void*)data;
		write_req->end_req=comp_alreq_end_req;

		map_num++;

		ppa=page_manager_get_new_ppa_from_seg(LSM.pm, bis->seg);
		if(bis->start_piece_ppa/L2PGAP/_PPS!=ppa/_PPS){
			EPRINT("map data should same sgement", true);
		}

		mr_set[mr_idx].start_lba=((key_ptr_pair*)data->value)[0].lba;
		mr_set[mr_idx].end_lba=kp_get_end_lba(data->value);
		mr_set[mr_idx].ppa=ppa;

		//uint32_t temp_ppa=ppa*L2PGAP;
		validate_map_ppa(LSM.pm->bm, ppa, mr_set[mr_idx].start_lba, mr_set[mr_idx].end_lba, true);
		//validate_piece_ppa(LSM.pm->bm,1, &temp_ppa, &mr_set[mr_idx].start_lba);

		io_manager_issue_write(ppa, data, write_req, false);

		mr_idx++;
		bis->map_data->pop();
	}

	sst_set_file_map(res,mr_idx, mr_set);

	res->file_addr.piece_ppa=bis->start_piece_ppa;
	res->end_ppa=ppa;
	res->map_num=map_num;
	res->start_lba=bis->start_lba;
	res->end_lba=bis->end_lba;
	res->_read_helper=bis->rh;
	read_helper_insert_done(bis->rh);
	bis->rh=NULL;
	return res;
}

sst_bf_in_stream *tiering_new_bis(std::queue<uint32_t> *locked_seg_q, uint32_t level_idx){
	__segment *seg=page_manager_get_seg_for_bis(LSM.pm, DATASEG);
	lsmtree_gc_unavailable_set(&LSM, NULL, seg->seg_idx);
	locked_seg_q->push(seg->seg_idx);
	read_helper_param temp_rhp=lsmtree_get_target_rhp(level_idx);
	temp_rhp.member_num=(_PPS-seg->used_page_num)*L2PGAP;
	sst_bf_in_stream *bis=sst_bis_init(seg, LSM.pm, true, temp_rhp);
	return bis;
}
/*
static inline run *filter_sequential_file(level *src, uint32_t max_sst_num, 
		uint32_t target_version, uint32_t* start_sst_file_idx, uint32_t des_idx){
	run *new_run=run_init(max_sst_num, UINT32_MAX, 0);
	bool moved=false;
	run *rptr;
	sst_file *sptr;
	sst_file *prev_sptr=NULL;
	uint32_t ridx, sidx;
	sst_queue *pf_queue=NULL;
	for_each_sst_level(src, rptr, ridx, sptr, sidx){
		if(!sptr->sequential_file) break;
		moved=true;
		if(prev_sptr==NULL){
			prev_sptr=sptr;
			pf_queue=new sst_queue();
			pf_queue->push(sptr);
		}
		else{
			if(prev_sptr->file_addr.map_ppa/_PPS == sptr->file_addr.map_ppa/_PPS){
				pf_queue->push(sptr);
			}
			else{
				sst_file *block_file=compaction_seq_pagesst_to_blocksst(pf_queue, des_idx, target_version);
				run_append_sstfile_move_originality(new_run, block_file);
				sst_free(block_file, LSM.pm);
				delete pf_queue;
				pf_queue=new sst_queue();
				pf_queue->push(sptr);
			}
			prev_sptr=sptr;
		}
	}

	if(pf_queue){
		sst_file *block_file=compaction_seq_pagesst_to_blocksst(pf_queue, des_idx, target_version);
		run_append_sstfile_move_originality(new_run, block_file);
		sst_free(block_file, LSM.pm);
		delete pf_queue;
	}

	if(moved){
		*start_sst_file_idx=sidx;
	}
	return new_run;
}*/


static inline run *filter_sequential_file(level *src, uint32_t max_sst_num, 
		uint32_t target_version, uint32_t* start_sst_file_idx, uint32_t des_idx){
	run *new_run=run_init(max_sst_num, UINT32_MAX, 0);
	bool moved=false;
	run *rptr;
	sst_file *sptr;
	uint32_t ridx, sidx;
	for_each_sst_level(src, rptr, ridx, sptr, sidx){
		if(!sptr->sequential_file) break;
		moved=true;
		if(sptr->type==PAGE_FILE){
			if(LSM.disk[des_idx]->level_type==LEVELING || LSM.disk[des_idx]->level_type==TIERING){
				sst_convert_seq_pf_to_bf(sptr);
			}
		}
		run_append_sstfile_move_originality(new_run, sptr);
	}

	if(moved){
		*start_sst_file_idx=sidx;
	}
	return new_run;
}

run *compaction_wisckey_to_normal(compaction_master *cm, level *src, 
		run *previous_run, uint32_t *moved_entry_num, 
		uint32_t start_sst_file_idx, uint32_t target_version,
		bool demote){
	_cm=cm;
	run *new_run=previous_run?previous_run:run_init(src->now_sst_num, UINT32_MAX, 0);
	read_issue_arg read_arg;
	read_arg.des=src;
	read_arg_container thread_arg;
	thread_arg.end_req=comp_alreq_end_req;
	thread_arg.arg_set=(read_issue_arg**)malloc(sizeof(read_issue_arg));
	thread_arg.arg_set[0]=&read_arg;
	thread_arg.set_num=1;

	sst_pf_out_stream *pos=NULL;
	sst_bf_out_stream *bos=sst_bos_init(read_done_check, false);
	std::queue<uint32_t>* locked_seg_q=new std::queue<uint32_t>();
	sst_bf_in_stream *bis=tiering_new_bis(locked_seg_q, src->idx+1);

	uint32_t total_num=(start_sst_file_idx==UINT32_MAX?src->now_sst_num:src->now_sst_num-(start_sst_file_idx+1));
	start_sst_file_idx=(start_sst_file_idx==UINT32_MAX?0:start_sst_file_idx);

	uint32_t start_idx=start_sst_file_idx;
	uint32_t tier_compaction_tags=COMPACTION_TAGS/COMPACTION_LEVEL_NUM;
	uint32_t round=total_num/tier_compaction_tags+(total_num%tier_compaction_tags?1:0);

	uint32_t total_moved_num=0;

	uint32_t now_version=version_pick_oldest_version(LSM.last_run_version, src->idx);

	uint32_t j=0;
	bool temp_final=false;

	lsmtree_gc_lock_level(&LSM, src->idx);

	for(uint32_t i=0; i<round; i++){
		read_arg.from=start_idx+i*tier_compaction_tags;
		if(i!=round-1){
			read_arg.to=start_idx+(i+1)*tier_compaction_tags-1;
		}
		else{
			read_arg.to=src->now_sst_num-1;	
		}
		read_param_init(&read_arg);

		if(i==0){
			pos=sst_pos_init_sst(LEVELING_SST_AT_PTR(src, read_arg.from), read_arg.param,
					read_arg.to-read_arg.from+1, now_version,
					read_done_check, file_done);
		}
		else{
			sst_pos_add_sst(pos, LEVELING_SST_AT_PTR(src, read_arg.from), read_arg.param,
					read_arg.to-read_arg.from+1);
		}

		read_sst_job((void*)&thread_arg,-1);
		
		uint32_t round2_tier_compaction_tags, picked_kv_num;

		do{ 
			round2_tier_compaction_tags=cm->read_param_queue->size();
			picked_kv_num=issue_read_kv_for_bos_stream(bos, pos, round2_tier_compaction_tags, 
					src->idx, demote, true);
			LSM.monitor.tiering_valid_entry_cnt+=picked_kv_num;
			round2_tier_compaction_tags=MIN(round2_tier_compaction_tags, picked_kv_num);
			total_moved_num+=picked_kv_num;
			temp_final=((i==round-1 && sst_pos_is_empty(pos)));
			issue_write_kv_for_bis(&bis, bos, 
					locked_seg_q, new_run, 
					round2_tier_compaction_tags, target_version, 
					temp_final);
			j++;
		}
		while(!temp_final && !sst_pos_is_empty(pos));
	}

	thpool_wait(cm->issue_worker);
	//finishing bis
	sst_file *last_file;
	if((last_file=bis_to_sst_file(bis))){
		run_append_sstfile_move_originality(new_run, last_file);
		sst_free(last_file, LSM.pm);
	}

	if(!total_moved_num){
		EPRINT("no data moved", true);
	}

	if(moved_entry_num){
		*moved_entry_num=total_moved_num;
	}

	lsmtree_gc_unlock_level(&LSM, src->idx);

	release_locked_seg_q(locked_seg_q);
	sst_pos_free(pos);
	sst_bis_free(bis);
	sst_bos_free(bos, _cm);
	free(thread_arg.arg_set);
	return new_run;
}

level* compaction_LW2TI(compaction_master *cm, level *src, level *des, uint32_t target_version){ /*move to last level*/
	_cm=cm;
	uint32_t start_sst_file_idx=UINT32_MAX;
	run *new_run=filter_sequential_file(src, des->max_sst_num/des->max_run_num, target_version, 
			&start_sst_file_idx, des->idx); //version_update
	
	if(new_run->now_sst_num){
		compaction_trivial_move(new_run, target_version, src->idx, des->idx, false);
	}


	uint32_t target_run_idx=version_to_ridx(LSM.last_run_version, 
			target_version, des->idx);
	
	//level_content_print(LSM.disk[1], true);

	uint32_t needed_seg_num=0;
	if(start_sst_file_idx==UINT32_MAX){
		needed_seg_num=(((src->now_sst_num)*KP_IN_PAGE/L2PGAP+(src->now_sst_num))/_PPS+1+1);
	//	start_sst_file_idx=0;
	}
	else{
		if(src->now_sst_num==start_sst_file_idx){//all_sequential_file
			level *res=level_init(des->max_sst_num, des->max_run_num, des->level_type, des->idx, des->max_contents_num, des->check_full_by_size);
			run *rptr; uint32_t ridx;
			for_each_run_max(des, rptr, ridx){
				if(rptr->now_sst_num){
					level_append_run_copy_move_originality(res, rptr, ridx);
				}
			}
			level_update_run_at_move_originality(res, target_run_idx, new_run, true);
			tiering_compaction_error_check(src, NULL, NULL, new_run, LSM.monitor.compaction_cnt[des->idx]);
			run_free(new_run);
			return res;
		}
		else{
			needed_seg_num=(((src->now_sst_num-start_sst_file_idx)*KP_IN_PAGE/L2PGAP+(src->now_sst_num-start_sst_file_idx))/_PPS+1+1);
		}
	}

	uint32_t total_moved_num=0;
	new_run=compaction_wisckey_to_normal(cm, src, new_run, &total_moved_num, start_sst_file_idx, target_version, true);

	level *res=level_init(des->max_sst_num, des->max_run_num, des->level_type, des->idx, des->max_contents_num, des->check_full_by_size);
	run *rptr; uint32_t ridx;
	for_each_run_max(des, rptr, ridx){
		if(rptr->now_sst_num){
			level_append_run_copy_move_originality(res, rptr, ridx);
		}
	}

	//level_append_run(res, new_run);
	if(total_moved_num){
		level_update_run_at_move_originality(res, target_run_idx, new_run, true);
		/*
		if(level_is_full(res)){
			version_make_early_invalidation_enable_old(LSM.last_run_version);
		}*/
	}
	else{
		EPRINT("no data move", true);
	}

	run_free(new_run);
	/*finish logic*/
	return res;
}

void compaction_leveling_gc_lock(level *lev, uint32_t from, uint32_t to){
	for(uint32_t i=from; i<=to; i++){
		lsmtree_gc_unavailable_set(&LSM, LEVELING_SST_AT_PTR(lev, i), UINT32_MAX);
	}
}

void compaction_leveling_gc_unlock(level *lev, uint32_t *unlocked_idx, uint32_t to, 
		uint32_t border_lba, bool last){
	for(uint32_t i=*unlocked_idx; i<=to; i++){
		sst_file *sptr=LEVELING_SST_AT_PTR(lev, i);
		if(last){
			lsmtree_gc_unavailable_unset(&LSM, sptr, UINT32_MAX);
		}
		else if(sptr->end_lba<=border_lba){
			lsmtree_gc_unavailable_unset(&LSM, sptr, UINT32_MAX);
			*unlocked_idx=i+1;
		}
		else
			break;
	}
}

/*
   This function is tightly coupled with compaction_LE2LE()
   If someone fixs this code, please check compaction_LE2LE() code.
 */
level* compaction_LW2LE(compaction_master *cm, level *src, level *des, uint32_t target_version){
	_cm=cm;
	if(!level_check_overlap(src, des)){ //sequential
		run *new_run=compaction_wisckey_to_normal(cm, src, NULL, NULL, UINT32_MAX, target_version, true);
		level *res=level_init(des->max_sst_num, des->run_num, des->level_type, des->idx, des->max_contents_num, des->check_full_by_size);
		sst_file *sptr;
		run *rptr; 
		uint32_t sidx, ridx;
		bool move=false;
		if(LAST_RUN_PTR(src)->end_lba < FIRST_RUN_PTR(des)->start_lba){
			move=true;
			for_each_sst(new_run, sptr, sidx){
				level_append_sstfile(res, sptr, true);
			}
		}

		for_each_sst_level(des, rptr, ridx, sptr, sidx){
			level_append_sstfile(res, sptr, true);
		}	

		if(!move){
			for_each_sst(new_run, sptr, sidx){
				level_append_sstfile(res, sptr, true);
			}	
		}
		run_free(new_run);
		/*insert keeping order into level*/
		return res;
	}

	uint32_t closed_upper_from=0, closed_upper_to=0;

	level *res=level_init(des->max_sst_num, des->run_num, des->level_type, des->idx, des->max_contents_num, des->check_full_by_size);
	/*move unoverlapped sst of above level*/
	bool trivial_move=leveling_get_sst_overlap_range(src, des, &closed_upper_from, &closed_upper_to);
	run *new_run=NULL;
	if(trivial_move){
		level *temp_level=level_split_lw_run_to_lev(&src->array[0], 0, closed_upper_from-1);
		new_run=compaction_wisckey_to_normal(cm, temp_level, NULL, NULL, closed_upper_from, target_version, true);
		sst_file *sptr;
		uint32_t sidx;
		for_each_sst(new_run, sptr, sidx){
			level_append_sstfile(res, sptr, true);
		}
		run_free(new_run);
		level_free(temp_level, LSM.pm);
	}

	new_run=run_init(des->now_sst_num+src->now_sst_num, UINT32_MAX, 0);

	uint32_t upper_version=version_pick_oldest_version(LSM.last_run_version, src->idx);
	uint32_t lower_version=version_pick_oldest_version(LSM.last_run_version, des->idx);

	uint32_t des_start_idx=0;
	uint32_t des_end_idx=0;
	/*move unoverlapped sst of below level*/
	compaction_move_unoverlapped_sst(NULL, src, closed_upper_from, des, res, &des_start_idx);

	read_issue_arg read_arg1={0,}, read_arg2={0,};
	read_arg1.des=src;

	read_arg_container thread_arg;
	thread_arg.end_req=comp_alreq_end_req;
	thread_arg.arg_set=(read_issue_arg**)malloc(sizeof(read_issue_arg*)*MERGED_RUN_NUM);
	thread_arg.arg_set[0]=&read_arg1;
	thread_arg.arg_set[1]=&read_arg2;
	thread_arg.set_num=COMPACTION_LEVEL_NUM;

	sst_pf_out_stream *os_set[COMPACTION_LEVEL_NUM];
	sst_bf_out_stream *bos=NULL;
	sst_bf_in_stream *bis=NULL;
	std::queue<key_ptr_pair> *kpq=new std::queue<key_ptr_pair>();

	std::queue<uint32_t> *locked_seg_q=new std::queue<uint32_t>();

	sst_file *sptr; run *rptr;
	uint32_t ridx, sidx=closed_upper_from;
	level *temp_des_level=level_convert_normal_run_to_LW(&des->array[0], LSM.pm, des_start_idx, 
			des->now_sst_num-1);
	read_arg2.des=temp_des_level;

	bool isstart=true;
	uint32_t stream_cnt=0;
	uint32_t upper_unlocked_idx=0, lower_unlocked_idx=0;
	for_each_sst_level_at(src, rptr, ridx, sptr, sidx){
		read_params_setting(&read_arg1, &read_arg2, src, temp_des_level, sptr, &sidx, 
				des_start_idx, &des_end_idx, compaction_read_param_remain_num(_cm));
		sidx=read_arg1.to;
		bool final_flag=sidx==src->now_sst_num-1;
	
		compaction_leveling_gc_lock(src, read_arg1.from, read_arg1.to);
		compaction_leveling_gc_lock(des, read_arg2.from, read_arg2.to);

		if(isstart){
			os_set[0]=sst_pos_init_sst(LEVELING_SST_AT_PTR(src, read_arg1.from), read_arg1.param, 
					read_arg1.to-read_arg1.from+1, upper_version,
					read_done_check, file_done);
			os_set[1]=sst_pos_init_sst(LEVELING_SST_AT_PTR(temp_des_level, read_arg2.from), read_arg2.param, 
					read_arg2.to-read_arg2.from+1, lower_version,
					read_done_check, file_done);
		}
		else{
			sst_pos_add_sst(os_set[0], LEVELING_SST_AT_PTR(src, read_arg1.from), read_arg1.param,
					read_arg1.to-read_arg1.from+1);
			sst_pos_add_sst(os_set[1], LEVELING_SST_AT_PTR(temp_des_level, read_arg2.from), read_arg2.param,
					read_arg2.to-read_arg2.from+1);
		}
		/*send read I/O*/
		printf("%s:%u - read_sst_job\n", __FILE__, __LINE__);
		issue_map_read_sst_job(cm, &thread_arg);

		stream_sorting(res, COMPACTION_LEVEL_NUM, os_set, NULL, kpq, final_flag, 
				MIN(LEVELING_SST_AT(src,read_arg1.to).end_lba, LEVELING_SST_AT(temp_des_level,read_arg2.to).end_lba),
				version_level_to_start_version(LSM.last_run_version, res->idx),
				false,
				leveling_invalidation_function, false
				);

		if(bos==NULL){
			bos=sst_bos_init(read_map_done_check, true);
		}
		if(bis==NULL){
			bis=tiering_new_bis(locked_seg_q, des->idx);
		}

		uint32_t border_lba;

		uint32_t entry_num=issue_read_kv_for_bos_sorted_set(bos, kpq, &border_lba,
				false, UINT32_MAX, UINT32_MAX, final_flag);

		border_lba=issue_write_kv_for_bis(&bis, bos, locked_seg_q, new_run, 
				entry_num, lower_version, final_flag);

		compaction_leveling_gc_unlock(src, &upper_unlocked_idx, read_arg1.to, 
				border_lba, final_flag);
		compaction_leveling_gc_unlock(des, &lower_unlocked_idx, read_arg2.to, 
				border_lba, final_flag);

		isstart=false;
		des_start_idx=des_end_idx+1;
		stream_cnt++;
	}

	sst_file *last_file;
	if((last_file=bis_to_sst_file(bis))){
		run_append_sstfile_move_originality(new_run, last_file);
		sst_free(last_file, LSM.pm);
	}

	for_each_sst(new_run, sptr, sidx){
		level_append_sstfile(res, sptr, true);
	}
	run_free(new_run);

	release_locked_seg_q(locked_seg_q);

	level_free(temp_des_level, LSM.pm);
	sst_bis_free(bis);
	sst_bos_free(bos, _cm);
	sst_pos_free(os_set[0]);
	sst_pos_free(os_set[1]);
	delete kpq;
	free(thread_arg.arg_set);
	return res;
}


level* compaction_LE2LE(compaction_master *cm, level *src, level *des, uint32_t target_version){
	_cm=cm;
	if(!level_check_overlap(src, des)){
		level *res=level_init(des->max_sst_num, des->run_num, des->level_type, des->idx, des->max_contents_num, des->check_full_by_size);
		sst_file *sptr;
		run *rptr; 
		uint32_t sidx, ridx;
		bool move=false;
		if(LAST_RUN_PTR(src)->end_lba < FIRST_RUN_PTR(des)->start_lba){
			for_each_sst_level(src, rptr, ridx, sptr, sidx){
				level_append_sstfile(res, sptr, true);
			}
			move=true;
		}

		for_each_sst_level(des, rptr, ridx, sptr, sidx){
			level_append_sstfile(res, sptr, true);
		}	

		if(!move){
			for_each_sst_level(src, rptr, ridx, sptr, sidx){
				level_append_sstfile(res, sptr, true);
			}	
		}
		/*insert keeping order into level*/
		return res;
	}

	uint32_t closed_upper_from=0, closed_upper_to=0;

	uint32_t upper_version=version_pick_oldest_version(LSM.last_run_version, src->idx);
	uint32_t lower_version=version_pick_oldest_version(LSM.last_run_version, des->idx);

	level *res=level_init(des->max_sst_num, des->run_num, des->level_type, des->idx, des->max_contents_num, des->check_full_by_size);
	/*move unoverlapped sst of above level*/
	bool trivial_move=leveling_get_sst_overlap_range(src, des, &closed_upper_from, &closed_upper_to);
	if(trivial_move){
		level *temp_level=level_split_lw_run_to_lev(&src->array[0], 0, closed_upper_from-1);
		run *rptr;
		sst_file *sptr;
		uint32_t sidx, ridx;
		for_each_sst_level(temp_level, rptr, ridx, sptr, sidx){
			level_append_sstfile(res, sptr, true);
		}
		level_free(temp_level, LSM.pm);
	}

	uint32_t des_start_idx=0, des_end_idx;
	/*move unoverlapped sst of below level*/
	compaction_move_unoverlapped_sst(NULL, src, closed_upper_from, des, res, &des_start_idx);

	read_issue_arg read_arg1={0,}, read_arg2={0,};
	read_arg_container thread_arg;
	thread_arg.end_req=comp_alreq_end_req;
	thread_arg.arg_set=(read_issue_arg**)malloc(sizeof(read_issue_arg*)*MERGED_RUN_NUM);
	thread_arg.arg_set[0]=&read_arg1;
	thread_arg.arg_set[1]=&read_arg2;
	thread_arg.set_num=COMPACTION_LEVEL_NUM;

	sst_pf_out_stream *os_set[COMPACTION_LEVEL_NUM];
	sst_bf_out_stream *bos=NULL;
	sst_bf_in_stream *bis=NULL;
	std::queue<key_ptr_pair> *kpq=new std::queue<key_ptr_pair>();

	sst_file *sptr; run *rptr;
	uint32_t ridx, sidx=closed_upper_from;
	level *temp_src_level=level_convert_normal_run_to_LW(&src->array[0], LSM.pm, closed_upper_from, 
			src->now_sst_num-1);
	read_arg1.des=temp_src_level;
	level *temp_des_level=level_convert_normal_run_to_LW(&des->array[0], LSM.pm, des_start_idx, 
			des->now_sst_num-1);
	read_arg2.des=temp_des_level;

	run *new_run=run_init(des->now_sst_num+src->now_sst_num, UINT32_MAX, 0);

	std::queue<uint32_t>* locked_seg_q=new std::queue<uint32_t>();

	bool isstart=true;
	uint32_t stream_cnt=0;
	for_each_sst_level_at(temp_src_level, rptr, ridx, sptr, sidx){
		read_params_setting(&read_arg1, &read_arg2, temp_src_level, temp_des_level, sptr, &sidx, 
				des_start_idx, &des_end_idx, compaction_read_param_remain_num(_cm));
		sidx=read_arg1.to;
		bool final_flag=sidx==temp_src_level->now_sst_num-1;

		if(isstart){
			os_set[0]=sst_pos_init_sst(LEVELING_SST_AT_PTR(temp_src_level, read_arg1.from), read_arg1.param, 
					read_arg1.to-read_arg1.from+1, upper_version,
					read_done_check, file_done);
			os_set[1]=sst_pos_init_sst(LEVELING_SST_AT_PTR(temp_des_level, read_arg2.from), read_arg2.param, 
					read_arg2.to-read_arg2.from+1, lower_version,
					read_done_check, file_done);
		}
		else{
			sst_pos_add_sst(os_set[0], LEVELING_SST_AT_PTR(temp_src_level, read_arg1.from), read_arg1.param,
					read_arg1.to-read_arg1.from+1);
			sst_pos_add_sst(os_set[1], LEVELING_SST_AT_PTR(temp_des_level, read_arg2.from), read_arg2.param,
					read_arg2.to-read_arg2.from+1);
		}

		/*send read I/O*/
		issue_map_read_sst_job(cm, &thread_arg);

		stream_sorting(res, COMPACTION_LEVEL_NUM, os_set, NULL, kpq, final_flag, 
				MIN(LEVELING_SST_AT(temp_src_level,read_arg1.to).end_lba, 
					LEVELING_SST_AT(temp_des_level,read_arg2.to).end_lba),
				version_level_to_start_version(LSM.last_run_version, res->idx),
				false,
				leveling_invalidation_function, false
				);

		if(bos==NULL){
			bos=sst_bos_init(read_map_done_check, true);
		}
		if(bis==NULL){
			bis=tiering_new_bis(locked_seg_q, des->idx);
		}

		uint32_t border_lba;
		uint32_t entry_num=issue_read_kv_for_bos_sorted_set(bos, kpq, &border_lba, 
				false, UINT32_MAX, UINT32_MAX,
				final_flag);

		issue_write_kv_for_bis(&bis, bos, locked_seg_q, new_run, 
				entry_num, lower_version, final_flag);
		isstart=false;
		des_start_idx=des_end_idx+1;
		stream_cnt++;
	}

	sst_file *last_file;
	if((last_file=bis_to_sst_file(bis))){
		run_append_sstfile_move_originality(new_run, last_file);
		sst_free(last_file, LSM.pm);
	}

	for_each_sst(new_run, sptr, sidx){
		level_append_sstfile(res, sptr, true);
	}
	run_free(new_run);

	release_locked_seg_q(locked_seg_q);
	level_free(temp_des_level, LSM.pm);
	level_free(temp_src_level, LSM.pm);
	sst_bis_free(bis);
	sst_bos_free(bos, _cm);
	sst_pos_free(os_set[0]);
	sst_pos_free(os_set[1]);
	delete kpq;
	free(thread_arg.arg_set);
	return res;
		
}

/*
	This function returnsa  new level by merging inserted two levels by the tiered compaction manner.
	Since the above level should be leveled level, it does not need to sort the above level.
	In this function, the above level just moves to the next level's empty run.
	There is no special logic for sequential workload.
 */
level *compaction_LE2TI(compaction_master *cm, level *src, level *des, uint32_t target_version){
	leveling_update_meta_for_trivial_move(src, des, target_version);
	run *new_run=level_LE_to_run(src, true);
	level *res=level_init(des->max_sst_num, des->max_run_num, des->level_type, des->idx, des->max_contents_num, des->check_full_by_size);
	//level_run_reinit(des, idx_set[1]);
	run *rptr;
	uint32_t ridx;
	for_each_run_max(des, rptr, ridx){
		if(rptr->now_sst_num){
			level_append_run_copy_move_originality(res, rptr, ridx);
		}
	}


	uint32_t target_ridx=target_version-version_level_to_start_version(LSM.last_run_version, des->idx);
	level_update_run_at_move_originality(res, target_ridx, new_run, true);
	run_free(new_run);
	return res;
}


level* compaction_LW2TW(compaction_master *cm, level *src, level *des, uint32_t target_version){
	static int cnt=0;
	if(cnt==528){
		printf("LW2TW cnt:\n");
	}
	cnt++;
	leveling_update_meta_for_trivial_move(src, des, target_version);
	run *new_run=level_LE_to_run(src, true);
	level *res=level_init(des->max_sst_num, des->max_run_num, des->level_type, des->idx, des->max_contents_num, des->check_full_by_size);
	//level_run_reinit(des, idx_set[1]);
	run *rptr;
	uint32_t ridx;
	for_each_run_max(des, rptr, ridx){
		if(rptr->now_sst_num){
			level_append_run_copy_move_originality(res, rptr, ridx);
		}
	}


/*
	uint32_t start_lba=src->array[0].start_lba;
	uint32_t end_lba=src->array[0].end_lba;
	version_update_for_trivial_move(LSM.last_run_version, start_lba, end_lba,
			src->idx, des->idx, target_version);*/

	uint32_t target_ridx=target_version-version_level_to_start_version(LSM.last_run_version, des->idx);
	level_update_run_at_move_originality(res, target_ridx, new_run, true);
	run_free(new_run);
	return res;
}

void release_locked_seg_q(std::queue<uint32_t> *locked_seg_q){
	while(locked_seg_q->size()){
		uint32_t seg_idx=locked_seg_q->front();
		lsmtree_gc_unavailable_unset(&LSM, NULL, seg_idx);
		locked_seg_q->pop();
	}
	delete locked_seg_q;
}

void *comp_alreq_end_req(algo_req *req){
	inter_read_alreq_param *r_param;
	value_set *vs;
	key_value_wrapper *kv_wrapper;
	switch(req->type){
		case MAPPINGW:
			vs=(value_set*)req->param;
			inf_free_valueset(vs, FS_MALLOC_W);
			break;
		case MAPPINGR:
			r_param=(inter_read_alreq_param*)req->param;
			fdriver_unlock(&r_param->done_lock);
			break;
		case COMPACTIONDATAR:
		case DATAR:
//			printf("read_done: %u\n", req->ppa);
			kv_wrapper=(key_value_wrapper*)req->param;
			fdriver_unlock(&kv_wrapper->param->done_lock);
			break;
		case COMPACTIONDATAW:
		case DATAW:
			vs=(value_set*)req->param;
			inf_free_valueset(vs, FS_MALLOC_W);
			break;
	}
	free(req);
	return NULL;
}

#if 0
level* compaction_first_leveling(compaction_master *cm, key_ptr_pair *kp_set, level *des){
	static int debug_cnt_flag=0;
	_cm=cm;
	level *res=level_init(des->max_sst_num, des->run_num, des->level_type, des->idx);

	if(!level_check_overlap_keyrange(kp_set[0].lba, kp_get_end_lba((char*)kp_set), des)){

		leveling_trivial_move(kp_set,NULL,  des, res, 
				version_level_to_start_version(LSM.last_run_version, des->idx));

		return res;
	}
	LSM.monitor.compaction_cnt[0]++;

	if(page_manager_is_gc_needed(LSM.pm, des->now_sst_num+1, true)){
		__do_gc(LSM.pm, true, des->now_sst_num+1);
	}

	/*each round read/write 128 data*/
	/*we need to have rate limiter!!*/
	read_issue_arg read_arg;
	read_arg.des=des;
	read_arg_container thread_arg;
	thread_arg.end_req=comp_alreq_end_req;
	thread_arg.arg_set=(read_issue_arg**)malloc(sizeof(read_issue_arg));
	thread_arg.arg_set[0]=&read_arg;
	thread_arg.set_num=1;

	sst_pf_out_stream *os_set[COMPACTION_LEVEL_NUM]={NULL,};
	sst_pf_out_stream *os;
	sst_pf_in_stream *is=sst_pis_init(true, 
			lsmtree_get_target_rhp(des->idx));

	uint32_t start_idx=0;
	compaction_move_unoverlapped_sst(kp_set, NULL, 0, des, res, &start_idx);
	uint32_t total_num=des->now_sst_num-start_idx+1;
	uint32_t level_compaction_tags=COMPACTION_TAGS/COMPACTION_LEVEL_NUM;
	uint32_t round=total_num/level_compaction_tags+(total_num%level_compaction_tags?1:0);
	for(uint32_t i=0; i<round; i++){
		/*set read_arg*/
		read_arg.from=start_idx+i*level_compaction_tags;
		if(i!=round-1){
			read_arg.to=start_idx+(i+1)*level_compaction_tags-1;
		}
		else{
			read_arg.to=des->now_sst_num-1;	
		}
		read_param_init(&read_arg);

		if(i==0){
			os=sst_pos_init_sst(LEVELING_SST_AT_PTR(des, read_arg.from), read_arg.param, 
					read_arg.to-read_arg.from+1, read_done_check, file_done);
			os_set[0]=sst_pos_init_kp(kp_set);
			os_set[1]=os;
		}
		else{
			sst_pos_add_sst(os_set[1], LEVELING_SST_AT_PTR(des, read_arg.from), read_arg.param,
					read_arg.to-read_arg.from+1);
		}
		/*send read I/O*/
		thpool_add_work(cm->issue_worker, read_sst_job, (void*)&thread_arg);

		stream_sorting(res, COMPACTION_LEVEL_NUM, os_set, is, NULL, i==round-1, 
				MIN(LEVELING_SST_AT(des,read_arg.to).end_lba, kp_set[LAST_KP_IDX].lba),
				version_level_to_start_version(LSM.last_run_version, res->idx),
				false,
				leveling_invalidation_function);

	}
	
	thpool_wait(cm->issue_worker);
	sst_pos_free(os_set[0]);
	sst_pos_free(os_set[1]);

	sst_pis_free(is);

	//level_print(res);
	compaction_error_check(kp_set, NULL, des, res, debug_cnt_flag++);

	free(thread_arg.arg_set);
	_cm=NULL;
	return res;
}
#endif
