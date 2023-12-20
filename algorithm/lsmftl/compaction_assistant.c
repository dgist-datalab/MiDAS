#include "compaction.h"
#include "lsmtree.h"
#include "segment_level_manager.h"
#include "io.h"
#include <math.h>
extern lsmtree LSM;
extern uint32_t debug_lba;

void* compaction_main(void *);
static volatile bool compaction_stop_flag;
compaction_master* compaction_init(uint32_t compaction_queue_num){
	compaction_master *cm=(compaction_master*)malloc(sizeof(compaction_master));
	cm->tm=tag_manager_init(compaction_queue_num);
	q_init(&cm->req_q, compaction_queue_num);

	cm->issue_worker=thpool_init(1);
	pthread_create(&cm->tid, NULL, compaction_main, (void*)cm);
	compaction_stop_flag=false;


	cm->read_param=(inter_read_alreq_param*)calloc(COMPACTION_TAGS, sizeof(inter_read_alreq_param));

	cm->read_param_queue=new std::queue<inter_read_alreq_param*>();
	for(uint32_t i=0; i<COMPACTION_TAGS; i++){
		cm->read_param_queue->push(&cm->read_param[i]);
	}
	//cm->kv_wrapper_slab=slab_master_init(sizeof(key_value_wrapper), COMPACTION_TAGS);
	return cm;
}

void compaction_free(compaction_master *cm){
	compaction_stop_flag=true;
	pthread_join(cm->tid, NULL);
	tag_manager_free_manager(cm->tm);

	free(cm->read_param);
	delete cm->read_param_queue;

	q_free(cm->req_q);
	free(cm);
}

void compaction_issue_req(compaction_master *cm, compaction_req *req){
	if(compaction_stop_flag) return;
	req->tag=tag_manager_get_tag(cm->tm);
	q_enqueue((void*)req, cm->req_q);
	return;
}

static inline void disk_change(level **up, level *src, level** des, uint32_t* idx_set){
	if(up!=NULL){
		level *new_up=level_init((*up)->max_sst_num, (*up)->max_run_num, (*up)->level_type, (*up)->idx, (*up)->max_contents_num, false);
		level_free(*up, LSM.pm);
		*up=new_up;
	}

	level *delete_target_level=*des;
	(*des)=src;
	level_free(delete_target_level, LSM.pm);
}

static inline void first_level_slm_coupling(key_ptr_pair *kp_set, bool is_gc_data){
	uint32_t end_idx=kp_end_idx((char*)kp_set);
	uint32_t prev_seg=UINT32_MAX;
	uint32_t prev_piece_ppa;

	if(SEGNUM(kp_set[0].piece_ppa)==SEGNUM(kp_set[end_idx].piece_ppa)){
		slm_coupling_level_seg(0, SEGNUM(kp_set[end_idx].piece_ppa), SEGPIECEOFFSET(kp_set[end_idx].piece_ppa), is_gc_data);
	}
	else{
		for(uint32_t i=0; i<=end_idx; i++){
			if(prev_seg==UINT32_MAX){
				prev_seg=SEGNUM(kp_set[i].piece_ppa);
				prev_piece_ppa=kp_set[i].piece_ppa;
				continue;
			}
			uint32_t now_seg=SEGNUM(kp_set[i].piece_ppa);
			if(now_seg==prev_seg){
				prev_piece_ppa=kp_set[i].piece_ppa;
			}
			else{
				slm_coupling_level_seg(0, prev_seg, SEGPIECEOFFSET(prev_piece_ppa), is_gc_data);
				prev_seg=now_seg;
				prev_piece_ppa=kp_set[i].piece_ppa;
			}
		}

		if(!slm_invalidate_enable(0, kp_set[end_idx].piece_ppa)){
			slm_coupling_level_seg(0, prev_seg, SEGPIECEOFFSET(prev_piece_ppa), is_gc_data);	
		}
	}
}
extern char all_set_page[PAGESIZE];
static sst_file *kp_to_sstfile(std::map<uint32_t, uint32_t> *flushed_kp_set, 
		std::map<uint32_t, uint32_t>::iterator *temp_iter, bool make_rh){
	if(temp_iter && *temp_iter==flushed_kp_set->end()){
		return NULL;
	}
	value_set *vs=inf_get_valueset(all_set_page, FS_MALLOC_W, PAGESIZE);
	key_ptr_pair *kp_set=(key_ptr_pair*)vs->value;

	std::map<uint32_t, uint32_t>::iterator iter=temp_iter?*temp_iter:flushed_kp_set->begin();
	//uint32_t now_remain_page=page_manager_get_remain_page(LSM.pm, false);
	//uint32_t max_iter_cnt=MIN(KP_IN_PAGE, (now_remain_page-1)*L2PGAP);
	uint32_t max_iter_cnt=KP_IN_PAGE;
	//printf("max_iter_cnt:%u now_remain_page:%u\n", max_iter_cnt, now_remain_page);
	uint32_t i=0;
	read_helper *rh=NULL;
	if(make_rh){
		rh=read_helper_init(lsmtree_get_target_rhp(0));
	}
	for(; iter!=flushed_kp_set->end() && i<max_iter_cnt; i++, iter++ ){
		kp_set[i].lba=iter->first;
		kp_set[i].piece_ppa=iter->second;
		if(make_rh){
			read_helper_stream_insert(rh, kp_set[i].lba, kp_set[i].piece_ppa);
		}
		slm_coupling_mem_lev_seg(SEGNUM(kp_set[i].piece_ppa), SEGPIECEOFFSET(kp_set[i].piece_ppa));
	}
	*temp_iter=iter;
	
	uint32_t last_idx=i-1;

	uint32_t map_ppa=page_manager_get_new_ppa(LSM.pm, false, DATASEG); //DATASEG for sequential tiering 
	validate_map_ppa(LSM.pm->bm, map_ppa, kp_set[0].lba,  kp_set[last_idx].lba, true);

	sst_file *res=sst_init_empty(PAGE_FILE);
	res->file_addr.map_ppa=map_ppa;

	res->start_lba=kp_set[0].lba;
	res->end_lba=kp_set[last_idx].lba;
	res->_read_helper=rh;
	res->start_piece_ppa=kp_set[0].piece_ppa;

	if(SEGNUM(kp_set[0].piece_ppa)==SEGNUM(kp_set[last_idx].piece_ppa) && 
			SEGNUM(kp_set[0].piece_ppa)==map_ppa/_PPS){
		res->sequential_file=true;
	}

	algo_req *write_req=(algo_req*)malloc(sizeof(algo_req));
	write_req->type=MAPPINGW;
	write_req->param=(void*)vs;
	write_req->end_req=comp_alreq_end_req;
	io_manager_issue_internal_write(map_ppa, vs, write_req, false);

	return res;
}

static inline bool check_sequential(std::map<uint32_t, uint32_t> *kp_set, write_buffer *wb){
	uint32_t start, end;
	if(kp_set->size()==0){
		start=wb->data->begin()->first;
		end=wb->data->rbegin()->first;	
		if(end-start+1==wb->buffered_entry_num) return true;
		else return false;
	}
	start=kp_set->begin()->first;
	end=kp_set->rbegin()->first;
	if(end-start+1==kp_set->size()){
		uint32_t prev_end=end;
		start=wb->data->begin()->first;
		end=wb->data->rbegin()->first;
		if(prev_end+1==start && 
			end-start+1==wb->buffered_entry_num){
			return true;	
		}
		else{
			return false;
		}
	}
	else{
		return false;
	}
}

static inline void flush_and_move(std::map<uint32_t, uint32_t> *kp_set, write_buffer *wb, uint32_t flushing_target_num){
	key_ptr_pair *temp_kp_set=write_buffer_flush(wb, flushing_target_num, false);
	for(uint32_t i=0; i<KP_IN_PAGE && temp_kp_set[i].lba!=UINT32_MAX; i++){
		std::map<uint32_t, uint32_t>::iterator find_iter=kp_set->find(temp_kp_set[i].lba);
		if(find_iter!=kp_set->end()){
#ifdef LSM_DEBUG
			if(debug_lba==find_iter->first){
				printf("target hit in mem level: %u, %u\n", find_iter->first, find_iter->second);
			}
#endif
			invalidate_kp_entry(find_iter->first, find_iter->second, UINT32_MAX, true);
			kp_set->erase(find_iter);
		}
		kp_set->insert(
				std::pair<uint32_t, uint32_t>(temp_kp_set[i].lba, temp_kp_set[i].piece_ppa));	
		version_coupling_lba_version(LSM.last_run_version, temp_kp_set[i].lba, UINT8_MAX);
	}
	free(temp_kp_set);
}

static inline level *make_pinned_level(std::map<uint32_t, uint32_t> * kp_set){
	level *res=level_init(kp_set->size()/KP_IN_PAGE+(kp_set->size()%KP_IN_PAGE?1:0),1, LEVELING_WISCKEY, UINT32_MAX, kp_set->size(), false);
	sst_file *sptr=NULL;
	std::map<uint32_t, uint32_t>::iterator iter=kp_set->begin();
	while((sptr=kp_to_sstfile(kp_set, &iter, false))){
		level_append_sstfile(res, sptr, true);
		sst_free(sptr, LSM.pm);
	}
	return res;
}

static inline level* flush_memtable(write_buffer *wb, bool is_gc_data){
	if(page_manager_get_total_remain_page(LSM.pm, false, false) < wb->buffered_entry_num/L2PGAP){
		__do_gc(LSM.pm, false, KP_IN_PAGE/L2PGAP);
	}
	
	rwlock_write_lock(&LSM.flush_wait_wb_lock);
	rwlock_write_lock(&LSM.flushed_kp_set_lock);
	if(LSM.flushed_kp_set==NULL){
		LSM.flushed_kp_set=new std::map<uint32_t, uint32_t>();
		LSM.flushed_kp_temp_set=new std::map<uint32_t, uint32_t>();
	}
	level *res=NULL;

	bool is_sequential=check_sequential(LSM.flushed_kp_set, wb);
	uint32_t now_remain_page_num=UINT32_MAX;
	uint32_t flushing_entry_num=KP_IN_PAGE;
	bool making_level=false;
	if(is_sequential){
		now_remain_page_num=page_manager_get_remain_page(LSM.pm, false);
		if(now_remain_page_num<2){
			now_remain_page_num=page_aligning_data_segment(LSM.pm);
		}
		uint32_t mapping_num=CEILING_TARGET(LSM.flushed_kp_set->size(), KP_IN_PAGE);
		uint32_t now_flushing_num=CEILING_TARGET(wb->buffered_entry_num, L2PGAP);
		uint32_t update_mapping_num=CEILING_TARGET(wb->buffered_entry_num+LSM.flushed_kp_set->size(), KP_IN_PAGE);

		if(now_remain_page_num < (now_flushing_num+update_mapping_num)){
			if(now_remain_page_num < mapping_num){
				EPRINT("sequential_error", true);
			}

			uint32_t remain_space=now_remain_page_num-mapping_num;
			uint32_t mapping_remain_space=LSM.flushed_kp_set->size()%KP_IN_PAGE;

			for(uint32_t i=1; i<=wb->buffered_entry_num; i++){
				uint32_t needed_data_page=CEILING_TARGET(i, L2PGAP);
				uint32_t needed_map_page=(int)(i-mapping_remain_space) <0 ? 0: CEILING_TARGET(i-mapping_remain_space, KP_IN_PAGE);
				if(needed_data_page+needed_map_page<=remain_space) continue;
				else{
					flushing_entry_num=i-1;
					break;
				}
			}
			making_level=true;
		}

		if(flushing_entry_num==0){
			res=make_pinned_level(LSM.flushed_kp_set); //aligned previous pinned_level
			making_level=false;
		}
		else{
			flush_and_move(LSM.flushed_kp_set, wb, flushing_entry_num); //inset data to remain space
		}

		if(making_level || (!res && LSM.flushed_kp_set->size()>=LSM.param.write_buffer_ent)){
			if(res){
				EPRINT("what happen?, 'res' should be NULL", true);
			}
			res=make_pinned_level(LSM.flushed_kp_set); //aligned pinned_level
		}
	
		/*insert remain entry to flushed_temp_kp_set*/
		if(wb->buffered_entry_num){
			flush_and_move(LSM.flushed_kp_temp_set, wb, wb->buffered_entry_num);
		}	
	}
	else{
		flush_and_move(LSM.flushed_kp_set, wb, wb->buffered_entry_num);
		if(LSM.flushed_kp_set->size() >= LSM.param.write_buffer_ent){
			res=make_pinned_level(LSM.flushed_kp_set);
		}
	}

	write_buffer_free(wb);
	LSM.flush_wait_wb=NULL;
	rwlock_write_unlock(&LSM.flushed_kp_set_lock);
	rwlock_write_unlock(&LSM.flush_wait_wb_lock);

	return res;
}

static inline void managing_remain_space_for_wisckey(level *src, level *des){
	uint32_t needed_map_page_num=src->now_sst_num+(des?des->now_sst_num:0);
	if(page_manager_get_total_remain_page(LSM.pm, true, false) < needed_map_page_num){
		__do_gc(LSM.pm, true, needed_map_page_num);	
	}
}

static inline level *TW_compaction(compaction_master *cm, level *src, level *des,
		uint32_t target_version){
	level *res=NULL;
	managing_remain_space_for_wisckey(src, NULL);
	level *temp=compaction_TW_convert_LW(cm, src);
	switch(des->level_type){
		case LEVELING:
			res=compaction_LW2LE(cm, temp, des, target_version);
			break;
		case LEVELING_WISCKEY:
			managing_remain_space_for_wisckey(temp, des);
			res=compaction_LW2LW(cm, temp, des, target_version);
			break;
		case TIERING:
			res=compaction_LW2TI(cm, temp, des, target_version);
			break;
		case TIERING_WISCKEY:
			res=compaction_LW2TW(cm, temp, des, target_version);
			break;
	}
	level_free(temp, LSM.pm);
	return res;
}

static inline void do_compaction_demote(compaction_master *cm, compaction_req *req, 
		level *temp_level, uint32_t start_idx, uint32_t end_idx){

	level *lev=NULL;
	if(temp_level){
		rwlock_write_lock(&LSM.level_rwlock[end_idx]);
	}else{
		rwlock_write_lock(&LSM.level_rwlock[end_idx]);
		rwlock_write_lock(&LSM.level_rwlock[start_idx]);
	}

	uint32_t end_type=LSM.disk[end_idx]->level_type;
	level *src_lev=temp_level?temp_level:LSM.disk[start_idx];
	level *des_lev=LSM.disk[end_idx];

	uint32_t target_version;
	if(des_lev->level_type==LEVELING || des_lev->level_type==LEVELING_WISCKEY){
		target_version=version_level_to_start_version(LSM.last_run_version, des_lev->idx);
	}
	else{	
		target_version=version_get_empty_version(LSM.last_run_version, des_lev->idx);
	}

	LSM.monitor.compaction_cnt[end_idx]++;
#ifdef HOT_COLD
	bool hot_cold_separation_compaction=false;
#endif

	switch(src_lev->level_type){
		case LEVELING:
			if(end_type==LEVELING){
				lev=compaction_LE2LE(cm, src_lev, des_lev, target_version);
			}
			else{
				lev=compaction_LE2TI(cm, src_lev, des_lev, target_version);
			}
			break;
		case LEVELING_WISCKEY:
			switch(end_type){
				case LEVELING_WISCKEY:
					managing_remain_space_for_wisckey(src_lev, des_lev);
					lev=compaction_LW2LW(cm, src_lev, des_lev, target_version);
					break;
				case LEVELING:
					lev=compaction_LW2LE(cm, src_lev, des_lev, target_version);
					break;
				case TIERING_WISCKEY:
					/*managing_remain_space_for_wisckey(src_lev, des_lev); no need to write data*/
					lev=compaction_LW2TW(cm, src_lev, des_lev, target_version);
					break;
				case TIERING:
					lev=compaction_LW2TI(cm, src_lev, des_lev, target_version);
					break;
			}
			break;
		case TIERING:
#ifdef HOT_COLD
			lev=compaction_TI2TI_separation(cm ,src_lev, des_lev, target_version, 
					&hot_cold_separation_compaction);
#else
			lev=compaction_TI2TI(cm, src_lev, des_lev, target_version);
#endif
			break;
		case TIERING_WISCKEY:
			lev=TW_compaction(cm, src_lev, des_lev, target_version);
			break;
	}

	/*populate version*/
	if(des_lev->level_type==LEVELING || des_lev->level_type==LEVELING_WISCKEY){
		if(!LSM.last_run_version->version_populate_queue[des_lev->idx]->size()){
			version_populate(LSM.last_run_version, target_version, des_lev->idx);
		}
	}
	else{	
		version_populate(LSM.last_run_version, target_version, des_lev->idx);
	}

	/*unpopulate_version*/
	if(!temp_level){
		if(src_lev->level_type==LEVELING || src_lev->level_type==LEVELING_WISCKEY){
			version_unpopulate(LSM.last_run_version,
					version_pop_oldest_version(LSM.last_run_version, src_lev->idx),
					src_lev->idx);
		}
		else{
			/*matching order*/
#ifdef HOT_COLD
			if(hot_cold_separation_compaction){
				/*do nothing*/
			}
			else{
				if(src_lev->run_num < src_lev->max_run_num){
					version_get_resort_version(LSM.last_run_version, src_lev->idx);
				}
				else{
					for(uint32_t i=0; i<src_lev->run_num; i++){
						version_unpopulate(LSM.last_run_version,
								version_pop_oldest_version(LSM.last_run_version, src_lev->idx),
								src_lev->idx);
					}
				}
			}
#else
			if(src_lev->run_num < src_lev->max_run_num){
				version_get_resort_version(LSM.last_run_version, src_lev->idx);
			}
			else{
				for(uint32_t i=0; i<src_lev->run_num; i++){
					version_unpopulate(LSM.last_run_version,
							version_pop_oldest_version(LSM.last_run_version, src_lev->idx),
							src_lev->idx);
				}
			}
#endif
		}
	}

#ifdef HOT_COLD
	if(hot_cold_separation_compaction){
		/*do nothing*/
	}
	else{
		disk_change(temp_level?NULL:&LSM.disk[start_idx], lev, &LSM.disk[end_idx], NULL);
	}
#else
	disk_change(temp_level?NULL:&LSM.disk[start_idx], lev, &LSM.disk[end_idx], NULL);
#endif

	if(end_idx==LSM.param.LEVELN-1){
		slm_empty_level(start_idx);
	}
	else if(temp_level){
		slm_move_mem_lev_seg(end_idx);
	}
	else{
		slm_move(end_idx, start_idx);
	}

	if(temp_level){
		rwlock_write_unlock(&LSM.level_rwlock[end_idx]);
		rwlock_write_lock(&LSM.flushed_kp_set_lock);
		std::map<uint32_t, uint32_t> *temp_map=LSM.flushed_kp_set;
		delete temp_map;

		if(LSM.flushed_kp_temp_set->size()){
			LSM.flushed_kp_set=LSM.flushed_kp_temp_set;
			LSM.flushed_kp_temp_set=new std::map<uint32_t, uint32_t>();
		}
		else{
			delete LSM.flushed_kp_temp_set;
			LSM.flushed_kp_temp_set=NULL;
			LSM.flushed_kp_set=NULL;
		}
		rwlock_write_unlock(&LSM.flushed_kp_set_lock);
	}
	else{
		rwlock_write_unlock(&LSM.level_rwlock[end_idx]);
		rwlock_write_unlock(&LSM.level_rwlock[start_idx]);
	}

	lsmtree_gc_unavailable_sanity_check(&LSM);
	version_sanity_checker(LSM.last_run_version);
}

static inline void do_compaction_inplace(compaction_master *cm, compaction_req *req, 
		uint32_t start_idx, uint32_t end_idx){
#ifdef LSM_DEBUG
	static int inplace_cnt=0;
	printf("inplace[%d] %u %u\n", inplace_cnt++, start_idx, end_idx);
	if(inplace_cnt==3){
		LSM.global_debug_flag=true;
	}
#endif
	rwlock_write_lock(&LSM.level_rwlock[start_idx]);
	level *src_lev=LSM.disk[start_idx];
	level *des_lev=LSM.disk[end_idx];
	level *res;

	bool issequential;
	uint32_t target_version=version_level_to_start_version(LSM.last_run_version, start_idx);
	run **target_run=compaction_TI2RUN(cm, src_lev, des_lev, 
			src_lev->run_num, target_version, UINT32_MAX, &issequential, true, false);

	if(src_lev->run_num < src_lev->max_run_num){
		version_get_resort_version(LSM.last_run_version, src_lev->idx);
	}
	else{
		for(uint32_t i=0; i<src_lev->run_num; i++){
			version_unpopulate(LSM.last_run_version,
					version_pop_oldest_version(LSM.last_run_version, src_lev->idx),
					src_lev->idx);
		}	
	}

	target_version=version_get_empty_version(LSM.last_run_version, src_lev->idx);
	LSM.monitor.compaction_cnt[start_idx]++;

	res=level_init(src_lev->max_sst_num, src_lev->max_run_num, src_lev->level_type, 
			src_lev->idx, src_lev->max_contents_num, true);
	version_populate(LSM.last_run_version, target_version, src_lev->idx);

	uint32_t target_ridx=version_to_ridx(LSM.last_run_version, target_version, src_lev->idx);
	level_update_run_at_move_originality(res, target_ridx, target_run[0], true);

	run_free(target_run[0]);
	if(!issequential){
		free(target_run);
	}

	level_free(src_lev, LSM.pm);
	LSM.disk[start_idx]=res;
	rwlock_write_unlock(&LSM.level_rwlock[start_idx]);

	lsmtree_gc_unavailable_sanity_check(&LSM);
	version_sanity_checker(LSM.last_run_version);
}

static inline void do_compaction(compaction_master *cm, compaction_req *req, 
		level *temp_level, uint32_t start_idx, uint32_t end_idx){
#ifdef LSM_DEBUG
	printf("compaction %u %u\n", start_idx, end_idx);
	if(LSM.global_debug_flag && start_idx==2){
		printf("break!\n");
	}
#endif
	if(temp_level){
		do_compaction_demote(cm, req, temp_level, start_idx, end_idx);
		return;
	}

	level *src_lev=LSM.disk[start_idx];
	if(src_lev->level_type!=TIERING || start_idx==0){
		do_compaction_demote(cm, req, temp_level, start_idx, end_idx);
		return;
	}

	level *prev_lev=LSM.disk[start_idx-1];
	uint32_t now_invalidate_num=version_get_level_invalidation_cnt(LSM.last_run_version, start_idx);
	
	if(now_invalidate_num >= prev_lev->max_contents_num){ //the level is physically full but the capacity of invalidated data exceeds upper level capacity
		do_compaction_inplace(cm, req, start_idx, end_idx);
	}
	else if(!(src_lev->run_num<src_lev->max_run_num) && 
			(src_lev->now_contents_num+src_lev->max_contents_num/LSM.param.normal_size_factor < prev_lev->max_contents_num)){ 
			//the level is logically full but it has enough space to store upper level's data
		do_compaction_inplace(cm, req, start_idx, end_idx);
	}
	else{//level is physically full
		do_compaction_demote(cm, req, temp_level, start_idx, end_idx);
	}
}

static inline level* do_reclaim_level(compaction_master *cm, level *target_lev){
	if(target_lev->level_type!=TIERING){
		EPRINT("cannot do GC", true);
	}
	level *res;
	uint32_t merged_idx_set[MERGED_RUN_NUM];
	version_get_merge_target(LSM.last_run_version, merged_idx_set, LSM.param.LEVELN-1);
	res=compaction_merge(cm, target_lev, merged_idx_set);

	if(page_manager_get_total_remain_page(LSM.pm, false, true) > LSM.param.reclaim_ppa_target){
		return res;
	}

	for(uint32_t i=0; i<MERGED_RUN_NUM; i++){
		uint32_t t_version=version_ridx_to_version(LSM.last_run_version, merged_idx_set[i], target_lev->idx);
		LSM.last_run_version->version_invalidation_cnt[t_version]=0;
	}

	run *rptr;
	uint32_t ridx, cnt;
	for_each_old_ridx_in_lastlev(LSM.last_run_version, ridx, cnt){
		if(ridx==merged_idx_set[0]) continue;
		rptr=LEVEL_RUN_AT_PTR(res, ridx);
		if(rptr->now_sst_num){
			LSM.monitor.compaction_reclaim_run_num++;
			run *new_run=compaction_reclaim_run(cm, rptr, ridx);
			run_empty_content(rptr, LSM.pm);

			uint32_t t_version=version_ridx_to_version(LSM.last_run_version, ridx, target_lev->idx);
			LSM.last_run_version->version_invalidation_cnt[t_version]=0;

			level_update_run_at_move_originality(res, ridx, new_run, false);
			run_free(new_run);
		}
		if(page_manager_get_total_remain_page(LSM.pm, false, true) > LSM.param.reclaim_ppa_target){
			break;
		}
	}

	return res;
}

static void last_level_reclaim(compaction_master *cm, uint32_t level_idx){
	level *lev=LSM.disk[level_idx];
	if(lev->level_type!=TIERING) return;

	uint32_t merged_idx_set[MERGED_RUN_NUM];
	rwlock_write_lock(&LSM.level_rwlock[level_idx]);

	level *src=do_reclaim_level(cm, LSM.disk[level_idx]);

	disk_change(NULL, src, &LSM.disk[level_idx], merged_idx_set);
	LSM.monitor.compaction_cnt[level_idx+1]++;
	rwlock_write_unlock(&LSM.level_rwlock[level_idx]);
}

void* compaction_main(void *_cm){
	compaction_master *cm=(compaction_master*)_cm;
	queue *req_q=(queue*)cm->req_q;
	uint32_t above_sst_num;
	uint32_t version_inv_cnt;
#ifdef LSM_DEBUG
	uint32_t contents_num;
#endif
	while(!compaction_stop_flag){
		compaction_req *req=(compaction_req*)q_dequeue(req_q);
		if(!req) continue;
again:
		level *temp_level=NULL;
		if(req->start_level==-1 && req->end_level==0){
			if(req->wb){
				temp_level=flush_memtable(req->wb, false);
			}
			else{
				EPRINT("wtf", true);
			}
		}

		if(req->start_level==-1 && !temp_level){
			goto end;
		}


#ifdef LSM_DEBUG
		/*
		if(req->start_level!=-1){
			version_inv_cnt=version_get_level_invalidation_cnt(LSM.last_run_version, req->start_level);
			contents_num=get_level_content_num(LSM.disk[req->start_level]);
			printf("L%u %u %.2f inv_cnt:%u(%.2f) \n", req->start_level, contents_num, (float)contents_num*100/RANGE, version_inv_cnt, 
					(float)version_inv_cnt/LSM.disk[req->start_level]->max_contents_num);
			if(req->start_level >= 2){
				level_tiering_sst_analysis(LSM.disk[req->start_level], LSM.pm->bm, LSM.last_run_version, false);
			}
		}*/
#endif
		do_compaction(cm, req, temp_level, req->start_level, req->end_level);


		if(req->end_level==LSM.param.LEVELN-1){
			goto end;
		}

		if(level_is_full(LSM.disk[req->end_level], LSM.param.last_size_factor)){
			if(req->end_level==LSM.param.LEVELN-2 && level_is_full(LSM.disk[LSM.param.LEVELN-1], LSM.param.last_size_factor)){
#ifdef LSM_DEBUG
				/*
				contents_num=get_level_content_num(LSM.disk[LSM.param.LEVELN-1]);
				printf("L%u %u %.2f (merge)\n", LSM.param.LEVELN-1, contents_num, (float)contents_num*100/RANGE);
				level_tiering_sst_analysis(LSM.disk[LSM.param.LEVELN-1], LSM.pm->bm, LSM.last_run_version, true);*/
#endif
				last_level_reclaim(cm, LSM.param.LEVELN-1);

			}
			req->start_level=req->end_level;
			req->end_level++;
			goto again;
		}
		else if(LSM.disk[req->end_level]->level_type==LEVELING || 
				LSM.disk[req->end_level]->level_type==LEVELING_WISCKEY){
			/*this path will test leveling level*/
			above_sst_num=req->end_level==0? 
				temp_level->max_sst_num:
				LSM.disk[req->end_level-1]->max_sst_num; 
			if(!level_is_appendable(LSM.disk[req->end_level], above_sst_num)){
				if(req->end_level==LSM.param.LEVELN-2 && level_is_full(LSM.disk[LSM.param.LEVELN-1], LSM.param.last_size_factor)){
					last_level_reclaim(cm, LSM.param.LEVELN-1);
#ifdef LSM_DEBUG
					contents_num=get_level_content_num(LSM.disk[LSM.param.LEVELN-1]);
					printf("L%u %u %.2f\n", LSM.param.LEVELN-1, contents_num, (float)contents_num*100/RANGE);
#endif
				}
				req->start_level=req->end_level;
				req->end_level++;
				goto again;
			}
		}

end:
		tag_manager_free_tag(cm->tm,req->tag);
		req->end_req(req);
	}
	return NULL;
}

uint32_t compaction_read_param_remain_num(compaction_master *cm){
	return cm->read_param_queue->size();
}

inter_read_alreq_param *compaction_get_read_param(compaction_master *cm){
	if(cm->read_param_queue->size()==0){
		EPRINT("plz check size before get param", true);
	}
	inter_read_alreq_param *res=cm->read_param_queue->front();
	cm->read_param_queue->pop();
	return res;
}

void compaction_free_read_param(compaction_master *cm, inter_read_alreq_param *target){
	if(cm->read_param_queue->size()>COMPACTION_TAGS){
		EPRINT("debug point", false);
	}
	fdriver_destroy(&target->done_lock);
	cm->read_param_queue->push(target);
	return;
}
void compaction_wait(compaction_master *cm){
	uint32_t tag=tag_manager_get_tag(cm->tm);
	tag_manager_free_tag(cm->tm, tag);
}
