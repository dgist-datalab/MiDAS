#include "page_manager.h"
#include "lsmtree.h"
#include "io.h"
extern lsmtree LSM;
void *gc_map_check_end_req(algo_req *req){
	gc_mapping_check_node *gmc;
	switch(req->type){
		case GCMR_DGC:
			gmc=(gc_mapping_check_node*)req->param;
			fdriver_unlock(&gmc->done_lock);
			break;
		case GCMW_DGC:
			inf_free_valueset((value_set*)req->param, FS_MALLOC_W);
			break;
	}
	free(req);
	return NULL;
}

void gc_issue_mapcheck_read(gc_mapping_check_node *gmc, lower_info *li){
	algo_req *req=(algo_req*)malloc(sizeof(algo_req));
	req->param=(void*)gmc;
	req->end_req=gc_map_check_end_req;
	req->type=GCMR_DGC;
	li->read(gmc->map_ppa, PAGESIZE, gmc->mapping_data, ASYNC, req);
}

static void sst_file_update_mapping(sst_file *sptr, value_set *vs, uint32_t level, uint32_t max_piece_ppa, page_manager *pm){
	invalidate_map_ppa(pm->bm, sptr->file_addr.map_ppa, true);
	sptr->file_addr.map_ppa=page_manager_get_new_ppa(LSM.pm, true, MAPSEG);
	validate_map_ppa(pm->bm, sptr->file_addr.map_ppa, sptr->start_lba, sptr->end_lba , true);

	algo_req *write_req=(algo_req*)malloc(sizeof(algo_req));
	write_req->type=GCMW_DGC;
	write_req->param=(void*)vs;
	write_req->end_req=gc_map_check_end_req;

	io_manager_issue_internal_write(sptr->file_addr.map_ppa, vs, write_req, false);
	slm_coupling_level_seg(level, SEGNUM(max_piece_ppa), SEGPIECEOFFSET(max_piece_ppa), false);
}

void gc_helper_for_normal(std::map<uint32_t, gc_mapping_check_node*> *gkv, 
		write_buffer *wb,
		uint32_t seg_idx){
	if(!wb) return;

	EPRINT("should I implement?", true);
	std::map<uint32_t, gc_mapping_check_node*>::iterator iter=gkv->begin();
	sst_file *now_check_sst=NULL;
	blockmanager *bm=LSM.pm->bm;

	key_ptr_pair *kp_set;
	while((kp_set=write_buffer_flush_for_gc(wb, false, seg_idx, NULL, UINT32_MAX, gkv))){
		free(kp_set);
	}

	bool remove_seg[10]={0,};
	gc_mapping_check_node *gmn;
	for(; iter!=gkv->end(); iter++){
		gmn=iter->second;
		if(gmn->type!=MAP_CHECK_FLUSHED_KP){
			EPRINT("it cannpt be", true);
		}

		if(now_check_sst){
			if(now_check_sst->start_lba <= gmn->lba && now_check_sst->end_lba>=gmn->lba){
				gmn->target_sst_file=now_check_sst;
				gmn->type=MAP_READ_DONE_PENDING;
				continue;
			}
		}

		now_check_sst=NULL;
		for(uint32_t i=0; i<LSM.param.LEVELN-1; i++){
			if(slm_invalidate_enable(i, gmn->piece_ppa)){
				now_check_sst=level_retrieve_sst(LSM.disk[i], gmn->lba);
				remove_seg[i]=true;
				gmn->level=i;
			}
		}

		if(!now_check_sst){
			EPRINT("wtf!!!", true);
		}

		gmn->type=MAP_READ_DONE;
		gmn->mapping_data=inf_get_valueset(NULL, FS_MALLOC_R, PAGESIZE);
		gmn->target_sst_file=now_check_sst;
		gmn->map_ppa=now_check_sst->file_addr.map_ppa;
		fdriver_lock_init(&gmn->done_lock, 0);
		gc_issue_mapcheck_read(gmn, bm->li);
	}

	for(uint32_t i=0; i<LSM.param.LEVELN-1; i++){
		if(remove_seg[i]){
			slm_remove_node(i, seg_idx);
		}
	}

	now_check_sst=NULL;
	gc_mapping_check_node *head_gmn=NULL;
	uint32_t max_piece_ppa=UINT32_MAX;
	char *now_data;
	for(iter=gkv->begin(); iter!=gkv->end(); ){
		gmn=iter->second;
		if(gmn->type==MAP_READ_DONE){
			if(head_gmn){
				sst_file_update_mapping(head_gmn->target_sst_file, head_gmn->mapping_data, gmn->level, 
						max_piece_ppa, LSM.pm);
				max_piece_ppa=UINT32_MAX;
				fdriver_destroy(&head_gmn->done_lock);
				free(head_gmn);
			}
			head_gmn=gmn;
			head_gmn->target_sst_file->data=head_gmn->mapping_data->value;
			now_data=head_gmn->mapping_data->value;
			fdriver_lock(&head_gmn->done_lock);
		}

		uint32_t idx=kp_find_idx(gmn->lba, now_data);
		if(((key_ptr_pair*)now_data)[idx].piece_ppa!=gmn->piece_ppa){
			EPRINT("invalid read", true);
		}	

		if(gmn->new_piece_ppa==UINT32_MAX){
			EPRINT("invalid ppa", true);
		}
		invalidate_kp_entry(gmn->lba, gmn->piece_ppa, UINT32_MAX, true);
		if(max_piece_ppa!=UINT32_MAX && SEGNUM(max_piece_ppa)!=SEGNUM(gmn->new_piece_ppa)){
			slm_coupling_level_seg(head_gmn->level, SEGNUM(max_piece_ppa), SEGPIECEOFFSET(max_piece_ppa), false);
			max_piece_ppa=UINT32_MAX;
		}

		if(max_piece_ppa==UINT32_MAX){
			max_piece_ppa=gmn->new_piece_ppa;
		}
		else{
			max_piece_ppa=max_piece_ppa<gmn->new_piece_ppa?gmn->new_piece_ppa:max_piece_ppa;
		}
		((key_ptr_pair*)now_data)[idx].piece_ppa=gmn->new_piece_ppa;

		if(gmn->type==MAP_READ_DONE){
			iter++;
			continue;
		}
		else{
			if(gmn->mapping_data){
				EPRINT("it should not be assigned memory", true);
			}
			free(gmn);
			gkv->erase(iter++);
		}
	}

	if(head_gmn){
		sst_file_update_mapping(head_gmn->target_sst_file, head_gmn->mapping_data, head_gmn->level,
				max_piece_ppa, LSM.pm);
		fdriver_destroy(&head_gmn->done_lock);
		free(head_gmn);
	}
}
