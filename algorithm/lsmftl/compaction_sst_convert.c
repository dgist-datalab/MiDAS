#include "compaction.h"
#include <stdlib.h>
extern lsmtree LSM;
extern compaction_master *_cm;
bool early_map_done(inter_read_alreq_param *param){
	param->map_target->data=NULL;
	inf_free_valueset(param->data, FS_MALLOC_R);
	fdriver_destroy(&param->done_lock);
	free(param);
	return true;
}
static void sst_sanity_checker(sst_file *res){
	if(res->end_lba < res->start_lba){
		EPRINT("range error", true);
	}

	if(!res->_read_helper){
		EPRINT("no read_helper", true);
	}

	if(res->end_ppa==UINT32_MAX){
		EPRINT("weired end ppa", true);
	}

	if(res->file_addr.piece_ppa==UINT32_MAX){
		EPRINT("weired file_addr", true);
	}
}

static sst_file *pf_queue_to_sstfile(sst_queue *pf_q){
	sst_file *res=sst_init_empty(BLOCK_FILE);
	res->sequential_file=true;
	map_range *mr_set=(map_range*)malloc(pf_q->size() * sizeof(map_range));
	uint32_t mr_idx=0;
	uint32_t start_lba=UINT32_MAX;
	uint32_t end_lba=0;

	while(!pf_q->empty()){
		sst_file *page_sst=pf_q->front();
		
		mr_set[mr_idx].start_lba=page_sst->start_lba;
		mr_set[mr_idx].end_lba=page_sst->end_lba;
		mr_set[mr_idx].ppa=page_sst->file_addr.map_ppa;

		if(start_lba>page_sst->start_lba){
			start_lba=page_sst->start_lba;
		}
		if(end_lba<page_sst->end_lba){
			end_lba=page_sst->end_lba;
		}
		mr_idx++;
		pf_q->pop();
	}

	sst_set_file_map(res, mr_idx, mr_set);
	res->start_lba=start_lba;
	res->end_lba=end_lba;
	res->map_num=mr_idx;
	res->file_addr.piece_ppa=UINT32_MAX;
	res->end_ppa=UINT32_MAX;
	res->_read_helper=NULL;
	return res;
}

static uint32_t stream_make_rh(sst_pf_out_stream *os, sst_file *file, 
		uint32_t target_version, uint32_t src_idx){
	uint32_t res=0;
	uint32_t end_ppa=file->end_ppa==UINT32_MAX?0:file->end_ppa;
	uint32_t start_piece_ppa=file->file_addr.piece_ppa;
	while(1){
		key_ptr_pair kp=sst_pos_pick(os);
		if(kp.lba==UINT32_MAX) break;
		read_helper_stream_insert(file->_read_helper, kp.lba, kp.piece_ppa);

		uint32_t recent_version=version_map_lba(LSM.last_run_version, kp.lba);
		if(version_belong_level(LSM.last_run_version, recent_version, src_idx)){
			version_coupling_lba_version(LSM.last_run_version, kp.lba, target_version);
		}

		sst_pos_pop(os);

		if(end_ppa<kp.piece_ppa/L2PGAP){
			end_ppa=kp.piece_ppa/L2PGAP;
		}

		if(start_piece_ppa > kp.piece_ppa){
			start_piece_ppa=kp.piece_ppa;
		}

		if(sst_pos_is_empty(os)){
			break;
		}
	}
	file->end_ppa=end_ppa;
	file->file_addr.piece_ppa=start_piece_ppa;
	return res;
}

sst_file *compaction_seq_pagesst_to_blocksst(sst_queue *pf_q, uint32_t des_idx, uint32_t target_version){
	sst_file *res=NULL;
	read_issue_arg read_arg;
	read_arg_container thread_arg;
	thread_arg.end_req=comp_alreq_end_req;
	thread_arg.arg_set=(read_issue_arg**)malloc(sizeof(read_issue_arg));
	thread_arg.arg_set[0]=&read_arg;
	thread_arg.set_num=1;

	sst_pf_out_stream *pos;

	uint32_t start_idx=0;
	uint32_t total_num=pf_q->size();
	
	uint32_t compaction_tag_num=compaction_read_param_remain_num(_cm);
	uint32_t round=(total_num/compaction_tag_num)+(total_num%compaction_tag_num?1:0);

	res=pf_queue_to_sstfile(pf_q);
	map_range *target_mr_set=res->block_file_map;
	read_helper_param temp_rhp=lsmtree_get_target_rhp(des_idx);
	temp_rhp.member_num=(res->map_num*KP_IN_PAGE);
	res->_read_helper=read_helper_init(temp_rhp);

	for(uint32_t i=0; i<round; i++){
		read_arg.from=start_idx+i*compaction_tag_num;
		if(i!=round-1){
			read_arg.to=start_idx+(i+1)*compaction_tag_num-1;
		}
		else{
			read_arg.to=total_num-1;
		}	
		read_map_param_init(&read_arg, target_mr_set);

		if(i==0){
			pos=sst_pos_init_mr(&target_mr_set[read_arg.from], read_arg.param,
					TARGETREADNUM(read_arg), UINT32_MAX,read_map_done_check, early_map_done);
		}
		else{
			sst_pos_add_mr(pos, &target_mr_set[read_arg.from], read_arg.param, 
					TARGETREADNUM(read_arg));
		}

		printf("%s:%u - read_sst_job\n", __FILE__, __LINE__);
		thpool_add_work(_cm->issue_worker, read_sst_job, (void*)&thread_arg);
		stream_make_rh(pos, res, target_version, des_idx-1);
	}
	

	for(uint32_t i=0; i<res->map_num; i++){
		if(res->end_ppa < res->block_file_map[i].ppa){
			res->end_ppa=res->block_file_map[i].ppa;
		}

		if(res->file_addr.piece_ppa >  res->block_file_map[i].ppa*L2PGAP){
			res->file_addr.piece_ppa=res->block_file_map[i].ppa*L2PGAP;
		}
	}
	sst_pos_free(pos);
	free(thread_arg.arg_set);
	read_helper_insert_done(res->_read_helper);
	sst_sanity_checker(res);
	return res;
}
