#include "compaction.h"
#include "run.h"
#include "../../include/sem_lock.h"
#include "../../interface/interface.h"
#include "sst_page_file_stream.h"
#include "io.h"
#include "function_test.h"
#include <algorithm>

extern lsmtree LSM;
extern compaction_master *_cm;
static void *compaction_data_read_end_req(algo_req *req);
extern uint32_t debug_lba, debug_piece_ppa;

uint32_t issue_read_kv_for_bos_sorted_set(sst_bf_out_stream *bos, 
		std::queue<key_ptr_pair> *kpq,
		uint32_t *border_lba,
		bool merge, uint32_t merge_newer_version, uint32_t merge_older_version,
		bool round_final){
	key_value_wrapper *read_target;
	uint32_t res=0;

	while(kpq->size()){
		key_ptr_pair target_pair=kpq->front();
		key_value_wrapper *kv_wrapper=(key_value_wrapper*)calloc(1,sizeof(key_value_wrapper));

		kv_wrapper->piece_ppa=target_pair.piece_ppa;
		kv_wrapper->kv_ptr.lba=target_pair.lba;
	
		if(kv_wrapper->kv_ptr.lba==debug_lba && kv_wrapper->piece_ppa==debug_piece_ppa){
			printf("break!\n");
		}
#ifdef NOTSURE
		invalidate_kp_entry(kv_wrapper->kv_ptr.lba, kv_wrapper->piece_ppa, UINT32_MAX, true);
#endif
	
		/*version alread check in stream sorting logci*/
		if((read_target=sst_bos_add(bos, kv_wrapper, _cm))){
			*border_lba=bos->last_issue_lba;
			if(!read_target->param){
				EPRINT("can't be",true);
			}
			algo_req *read_req=(algo_req*)malloc(sizeof(algo_req));
			read_req->type=COMPACTIONDATAR;
			read_req->param=(void*)read_target;
			read_req->end_req=compaction_data_read_end_req;
			io_manager_issue_read(PIECETOPPA(read_target->piece_ppa),
					read_target->param->data, read_req, false);
		}

		kpq->pop();
		res++;
	}

	if(kpq->empty() && round_final){
		*border_lba=UINT32_MAX;
		if((read_target=sst_bos_get_pending(bos, _cm))){
			algo_req *read_req=(algo_req*)malloc(sizeof(algo_req));
			read_req->type=COMPACTIONDATAR;
			read_req->param=(void*)read_target;
			read_req->end_req=compaction_data_read_end_req;		
			io_manager_issue_read(PIECETOPPA(read_target->piece_ppa),
					read_target->param->data, read_req, false);
		}
	}
	return res;
}

int issue_read_kv_for_bos_stream(sst_bf_out_stream *bos, 
		sst_pf_out_stream *pos, 
		uint32_t target_num, 
		uint32_t now_level,
		bool demote,
		bool round_final){

	key_value_wrapper *read_target;
	uint32_t res=0;
/*
	static uint32_t bos_stream_cnt=0;
	printf("bos_stream_cnt :%u\n", bos_stream_cnt++);
*/
	for(uint32_t i=0; i<target_num  && !sst_pos_is_empty(pos); i++){
		key_ptr_pair target_pair=sst_pos_pick(pos);
		if(target_pair.lba==UINT32_MAX) continue;
		key_value_wrapper *kv_wrapper=(key_value_wrapper*)calloc(1,sizeof(key_value_wrapper));

		kv_wrapper->piece_ppa=target_pair.piece_ppa;
		kv_wrapper->kv_ptr.lba=target_pair.lba;

#ifdef NOTSURE
		if(slm_invalidate_enable(now_level, kv_wrapper->piece_ppa)){
			invalidate_kp_entry(kv_wrapper->kv_ptr.lba, kv_wrapper->piece_ppa, UINT32_MAX, true);
		}
#endif

		if(demote && now_level!=0){
			uint32_t target_version=version_level_to_start_version(LSM.last_run_version, now_level-1);
			if(version_map_lba(LSM.last_run_version, target_pair.lba) >= target_version){ 
				//already invalid
				free(kv_wrapper);
				sst_pos_pop(pos);
				continue;
			}
		}

		if((read_target=sst_bos_add(bos, kv_wrapper, _cm))){
			if(!read_target->param){
				EPRINT("can't be",true);
			}
			algo_req *read_req=(algo_req*)malloc(sizeof(algo_req));
			read_req->type=COMPACTIONDATAR;
			read_req->param=(void*)read_target;
			read_req->end_req=comp_alreq_end_req;
			read_req->ppa=read_target->piece_ppa;
			io_manager_issue_read(PIECETOPPA(read_target->piece_ppa),
					read_target->param->data, read_req, false);
		}

		sst_pos_pop(pos);
		res++;
	}

	if(sst_pos_is_empty(pos) && round_final){
		if((read_target=sst_bos_get_pending(bos, _cm))){
			algo_req *read_req=(algo_req*)malloc(sizeof(algo_req));
			read_req->type=COMPACTIONDATAR;
			read_req->param=(void*)read_target;
			read_req->end_req=comp_alreq_end_req;		
			io_manager_issue_read(PIECETOPPA(read_target->piece_ppa),
					read_target->param->data, read_req, false);
		}
	}
	return res;
}

void issue_bis_result(sst_bf_in_stream *bis, uint32_t target_version, bool final){
	uint32_t debug_idx=0;
	key_ptr_pair debug_kp[L2PGAP];
	value_set *result=sst_bis_get_result(bis, final, &debug_idx, debug_kp);

	uint32_t level_idx=version_to_level_idx(LSM.last_run_version, target_version, LSM.param.LEVELN);
	for(uint32_t i=0; i<debug_idx; i++){
		compaction_debug_func(debug_kp[i].lba, debug_kp[i].piece_ppa, target_version,
				LSM.disk[level_idx]);
	}

	algo_req *write_req=(algo_req*)malloc(sizeof(algo_req));
	write_req->type=COMPACTIONDATAW;
	write_req->param=(void*)result;
	write_req->end_req=comp_alreq_end_req;
	io_manager_issue_write(result->ppa, result, write_req, false);	
}

uint32_t issue_write_kv_for_bis(sst_bf_in_stream **bis, sst_bf_out_stream *bos, 
		std::queue<uint32_t> *locked_seg_q,run *new_run,
		int32_t entry_num, uint32_t target_version, bool final){
	int32_t inserted_entry_num=0;
	uint32_t last_lba=UINT32_MAX;

	fdriver_lock(&LSM.flush_lock);
	uint32_t level_idx=version_to_level_idx(LSM.last_run_version, target_version, LSM.param.LEVELN);

	uint32_t kv_pair_num=sst_bos_size(bos, final);
	uint32_t need_page_num=(kv_pair_num/L2PGAP+(kv_pair_num%L2PGAP?1:0))+
		(kv_pair_num/KP_IN_PAGE+(kv_pair_num%KP_IN_PAGE?1:0)); //map_num
	uint32_t need_seg_num=need_page_num/_PPS+(need_page_num%_PPS?1:0);
	if(page_manager_get_total_remain_page(LSM.pm, false, false) < need_seg_num*_PPS){
		__do_gc(LSM.pm, false, need_seg_num*_PPS);
	}

	while(!sst_bos_is_empty(bos)){
		key_value_wrapper *target=NULL;

		if(!final && !(target=sst_bos_pick(bos, entry_num-inserted_entry_num <=L2PGAP))){
			break;
		}
		else if(final){
			target=sst_bos_pick(bos, entry_num-inserted_entry_num<=L2PGAP);
			if(!target){
				target=sst_bos_get_pending(bos, _cm);
			}
		}

		if(target){
			last_lba=target->kv_ptr.lba;
			version_coupling_lba_version(LSM.last_run_version, target->kv_ptr.lba, target_version);
		}

		if((target && sst_bis_insert(*bis, target)) ||
				(final && sst_bos_kv_q_size(bos)==1)){
			issue_bis_result(*bis, target_version, final);
		}

		sst_bos_pop(bos, _cm);

		if(sst_bis_ppa_empty(*bis)){
			sst_file *sptr=bis_to_sst_file(*bis);
			run_append_sstfile_move_originality(new_run, sptr);
			sst_free(sptr, LSM.pm);
			sst_bis_free(*bis);
			*bis=tiering_new_bis(locked_seg_q, level_idx);
		}
		inserted_entry_num++;
	}
	
	if(final && sst_bos_kv_q_size(bos)==0 && (*bis)->buffer_idx){
		issue_bis_result(*bis, target_version, final);
	}
	fdriver_unlock(&LSM.flush_lock);
	return last_lba;
}

uint32_t issue_write_kv_for_bis_hot_cold(sst_bf_in_stream ***bis, sst_bf_out_stream *bos, 
		std::queue<uint32_t> *locked_seg_q, run **new_run,
		int32_t entry_num, uint32_t target_demote_version, uint32_t target_keep_version,
		uint32_t src_idx,
		bool final){
	int32_t inserted_entry_num=0;
	uint32_t last_lba=UINT32_MAX;

	fdriver_lock(&LSM.flush_lock);
	uint32_t level_idx=version_to_level_idx(LSM.last_run_version, target_demote_version, LSM.param.LEVELN);

	uint32_t kv_pair_num=sst_bos_size(bos, final);
	uint32_t need_page_num=(kv_pair_num/L2PGAP+(kv_pair_num%L2PGAP?1:0))+
		(kv_pair_num/KP_IN_PAGE+(kv_pair_num%KP_IN_PAGE?1:0)); //map_num
	uint32_t need_seg_num=need_page_num/_PPS+(need_page_num%_PPS?1:0);
	if(page_manager_get_total_remain_page(LSM.pm, false, false) < need_seg_num*_PPS){
		__do_gc(LSM.pm, false, need_seg_num*_PPS);
	}
	
	bool inserting_demote_run;
	while(!sst_bos_is_empty(bos)){
		inserting_demote_run=false;
		key_value_wrapper *target=NULL;

		if(!final && !(target=sst_bos_pick(bos, entry_num-inserted_entry_num <=L2PGAP))){
			break;
		}
		else if(final){
			target=sst_bos_pick(bos, entry_num-inserted_entry_num<=L2PGAP);
			if(!target){
				target=sst_bos_get_pending(bos, _cm);
			}
		}

		sst_bf_in_stream **target_bis=NULL;

		if(target){
			last_lba=target->kv_ptr.lba;
			uint32_t querying_version=target->prev_version;
			if(version_belong_level(LSM.last_run_version, querying_version, src_idx)){
				inserting_demote_run=false;		
				version_coupling_lba_version(LSM.last_run_version, target->kv_ptr.lba, target_keep_version);
			}
			else{
				inserting_demote_run=true;
				version_coupling_lba_version(LSM.last_run_version, target->kv_ptr.lba, target_demote_version);
			}
			target_bis=&((*bis)[inserting_demote_run?DEMOTE_RUN:KEEP_RUN]);


			if((target && sst_bis_insert(*target_bis, target)) ||
					(final && sst_bos_kv_q_size(bos)==1)){
				if(inserting_demote_run){
					issue_bis_result(*target_bis, target_demote_version, final);
				}
				else{
					issue_bis_result(*target_bis, target_keep_version, final);
				}
			}
		}

		sst_bos_pop(bos, _cm);

		if(target_bis && sst_bis_ppa_empty(*target_bis)){
			sst_file *sptr=bis_to_sst_file(*target_bis);
			run_append_sstfile_move_originality(new_run[inserting_demote_run?DEMOTE_RUN:KEEP_RUN], sptr);
			sst_free(sptr, LSM.pm);
			sst_bis_free(*target_bis);
			*target_bis=tiering_new_bis(locked_seg_q, inserting_demote_run? level_idx:src_idx);
		}
		inserted_entry_num++;
	}
	
	if(final && sst_bos_kv_q_size(bos)==0 && (*bis[DEMOTE_RUN])->buffer_idx){
		issue_bis_result((*bis)[DEMOTE_RUN], target_demote_version, final);
	}

	if(final && sst_bos_kv_q_size(bos)==0 && (*bis[KEEP_RUN])->buffer_idx){
		issue_bis_result((*bis)[KEEP_RUN], target_keep_version, final);
	}
	fdriver_unlock(&LSM.flush_lock);

	return last_lba;
}

void *compaction_data_read_end_req(algo_req *req){
	key_value_wrapper *kv_wrapper;
	switch(req->type){
		case COMPACTIONDATAR:
			kv_wrapper=(key_value_wrapper*)req->param;
			fdriver_unlock(&kv_wrapper->param->done_lock);
			break;

	}
	free(req);
	return NULL;
}

