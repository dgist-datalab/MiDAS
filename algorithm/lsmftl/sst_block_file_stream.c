#include "sst_block_file_stream.h"
#include "lsmtree.h"
#include "oob_manager.h"
#include <stdlib.h>

extern lsmtree LSM;
extern uint32_t debug_lba;
sst_bf_out_stream *sst_bos_init(bool (*r_check_done)(inter_read_alreq_param *, bool), bool no_inter_param_alloc){
	sst_bf_out_stream *res=(sst_bf_out_stream*)calloc(1, sizeof(sst_bf_out_stream));
	res->kv_read_check_done=r_check_done;
	res->kv_wrapper_q=new std::queue<key_value_wrapper*>();
	res->prev_ppa=UINT32_MAX;
#ifdef LSM_DEBUG
	res->prev_lba=UINT32_MAX;
#endif
	res->no_inter_param_alloc=no_inter_param_alloc;
	return res;
}

static key_value_wrapper *kv_wrapper_setting(sst_bf_out_stream *bos, key_value_wrapper **kv_wrap_buf, 
		uint8_t num, compaction_master *cm){
	key_value_wrapper *res;
	inter_read_alreq_param *_param;
	char *value;
	for(uint32_t i=0; i<num; i++){
		if(i==0){
			res=kv_wrap_buf[i];
			if(!bos->no_inter_param_alloc){
				res->param=compaction_get_read_param(cm);
			}
			else{
				res->param=(inter_read_alreq_param*)calloc(1, sizeof(inter_read_alreq_param));
			}
			_param=res->param;
	//		printf("after size:%u\n", cm->read_param_queue->size());
			res->param->data=inf_get_valueset(NULL, FS_MALLOC_R, PAGESIZE);
			fdriver_lock_init(&res->param->done_lock, 0);
			value=res->param->data->value;
			kv_wrap_buf[i]->wait_target_req=true;
		}
		else{
			kv_wrap_buf[i]->wait_target_req=false;
		}

#ifndef NOTSURE
		invalidate_kp_entry(kv_wrap_buf[i]->kv_ptr.lba, kv_wrap_buf[i]->piece_ppa, UINT32_MAX, true);
#endif

		if(kv_wrap_buf[i]->kv_ptr.lba==debug_lba){
			printf("wrapping! %u -> offset:%u\n", debug_lba, kv_wrap_buf[i]->piece_ppa%L2PGAP);	
		}
		kv_wrap_buf[i]->kv_ptr.data=&value[LPAGESIZE*(kv_wrap_buf[i]->piece_ppa%L2PGAP)];
		kv_wrap_buf[i]->prev_version=UINT32_MAX;
		kv_wrap_buf[i]->free_target_req=false;

		bos->last_issue_lba=kv_wrap_buf[i]->kv_ptr.lba;
		if(i==num-1){
			kv_wrap_buf[i]->free_target_req=true;
			kv_wrap_buf[i]->no_inter_param_alloc=bos->no_inter_param_alloc;
			kv_wrap_buf[i]->param=_param;
		}
	}
	return res;
}

key_value_wrapper *sst_bos_add(sst_bf_out_stream *bos, 
		key_value_wrapper *kv_wrapper, compaction_master *cm){
#ifdef LSM_DEBUG
	if(bos->prev_lba==UINT32_MAX){
		bos->prev_lba=kv_wrapper->kv_ptr.lba;
	}
	else{
		if(bos->prev_lba>=kv_wrapper->kv_ptr.lba){
			EPRINT("lba shoud be inserted by increasing order", true);
		}
		bos->prev_lba=kv_wrapper->kv_ptr.lba;
	}
#endif

	key_value_wrapper *res=NULL;

	bos->kv_wrapper_q->push(kv_wrapper);

	if(bos->prev_ppa!=UINT32_MAX && bos->prev_ppa==PIECETOPPA(kv_wrapper->piece_ppa)){
		bos->kv_wrap_buffer[bos->kv_buf_idx++]=kv_wrapper;
		if(bos->kv_buf_idx==L2PGAP){
			res=kv_wrapper_setting(bos, bos->kv_wrap_buffer, bos->kv_buf_idx, cm);
			bos->kv_buf_idx=0;
			bos->prev_ppa=UINT32_MAX;
			return res;
		}
	}
	else{
		if(bos->prev_ppa!=UINT32_MAX){
			res=kv_wrapper_setting(bos ,bos->kv_wrap_buffer, bos->kv_buf_idx, cm);
		}
		bos->kv_buf_idx=0;
		bos->prev_ppa=PIECETOPPA(kv_wrapper->piece_ppa);
		bos->kv_wrap_buffer[bos->kv_buf_idx++]=kv_wrapper;
		return res;
	}

	return res;
}

key_value_wrapper *sst_bos_get_pending(sst_bf_out_stream *bos, 
		compaction_master *cm){
	if(bos->kv_buf_idx){
		key_value_wrapper *res=kv_wrapper_setting(bos, bos->kv_wrap_buffer, bos->kv_buf_idx, cm);
		bos->kv_buf_idx=0;
		bos->prev_ppa=UINT32_MAX;
		return res;
	}
	else return NULL;
}

key_value_wrapper* sst_bos_pick(sst_bf_out_stream * bos, bool should_buffer_check){
	key_value_wrapper *target=bos->kv_wrapper_q->front();
	if(target->wait_target_req){
		bos->kv_read_check_done(target->param, false);
	}
	else{
		if(should_buffer_check){
			for(uint32_t i=0; i<bos->kv_buf_idx; i++){
				if(bos->kv_wrap_buffer[i]==target){
					return NULL;
				}
			}
		}
	}

	if(target->kv_ptr.data==NULL){
		EPRINT("over entry pick!", true);
	}

	if(target->kv_ptr.lba==debug_lba){
		printf("pick target->piece_ppa:%u p:%p f:%u s:%u data:%u\n", target->piece_ppa, target->param, 
				target->free_target_req, should_buffer_check,
				*(uint32_t*)target->kv_ptr.data);
	}
	return target;
}

uint32_t sst_bos_size(sst_bf_out_stream *bos, bool include_pending){
	return include_pending?bos->kv_wrapper_q->size()+bos->kv_buf_idx:bos->kv_wrapper_q->size(); 
}

void sst_bos_pop(sst_bf_out_stream *bos, compaction_master *cm){
	/*
	key_value_wrapper *target=bos->kv_wrapper_q->front();
	if(PIECETOPPA(bos->now_kv_wrap->piece_ppa)!=PIECETOPPA(target->piece_ppa)){
		inf_free_valueset(bos->now_kv_wrap->param->data, FS_MALLOC_R);
		if(!bos->no_inter_param_alloc){
			compaction_free_read_param(cm, bos->now_kv_wrap->param);
		}
		else{
			free(bos->now_kv_wrap->param);
		}
		if(!target->param){
			EPRINT("can't be", true);
		}
		free(bos->now_kv_wrap);
		bos->now_kv_wrap=target;
	}
	else{
		if(target!=bos->now_kv_wrap){
			free(target);
		}
	}
	*/
	bos->kv_wrapper_q->pop();
}

bool sst_bos_is_empty(sst_bf_out_stream *bos){
	return bos->kv_wrapper_q->size()==0;
}

void sst_bos_free(sst_bf_out_stream *bos, compaction_master *cm){
	if(bos->kv_buf_idx){
		EPRINT("free after processing pending req", true);
	}
	/*
	if(bos->now_kv_wrap){
		inf_free_valueset(bos->now_kv_wrap->param->data, FS_MALLOC_R);
		if(!bos->no_inter_param_alloc){
			compaction_free_read_param(cm, bos->now_kv_wrap->param);
		}
		else{
			free(bos->now_kv_wrap->param);
		}
	}
	free(bos->now_kv_wrap);*/
	delete bos->kv_wrapper_q;
	free(bos);
}
/*
sst_bf_in_stream * sst_bis_init(uint32_t start_piece_ppa, uint32_t piece_ppa_length, page_manager*pm,
		bool make_read_helper, read_helper_param rhp){
	sst_bf_in_stream *res=(sst_bf_in_stream*)calloc(1, sizeof(sst_bf_in_stream));
	res->prev_lba=UINT32_MAX;
	res->start_lba=UINT32_MAX;
	res->start_piece_ppa=start_piece_ppa;
	res->piece_ppa_length=piece_ppa_length;
	res->map_data=new std::queue<value_set*>();
	res->pm=pm;
	res->buffer=(key_value_wrapper*)malloc(L2PGAP*sizeof(key_value_wrapper));

	res->make_read_helper=make_read_helper;

	if(make_read_helper){	
		res->rh=read_helper_init(rhp);
		res->rhp=rhp;
	}
	return res;
}
*/
sst_bf_in_stream * sst_bis_init(__segment *seg, page_manager *pm, bool make_read_helper, read_helper_param rhp){
	sst_bf_in_stream *res=(sst_bf_in_stream*)calloc(1, sizeof(sst_bf_in_stream));
	res->prev_lba=UINT32_MAX;
	res->start_lba=UINT32_MAX;

	res->start_piece_ppa=pm->bm->pick_page_num(pm->bm, seg)*L2PGAP;
	res->piece_ppa_length=(_PPS-seg->used_page_num)*L2PGAP;
	res->map_data=new std::queue<value_set*>();
	res->pm=pm;
	res->buffer=(key_value_wrapper**)malloc(L2PGAP*sizeof(key_value_wrapper*));
	res->seg=seg;

	res->make_read_helper=make_read_helper;

	if(make_read_helper){	
		res->rh=read_helper_init(rhp);
		res->rhp=rhp;
	}
	return res;
}

bool sst_bis_insert(sst_bf_in_stream *bis, key_value_wrapper *kv_wrapper){
	if(bis->buffer_idx==L2PGAP){
		EPRINT("plz check buffer_idx", true);
	}
	kv_wrapper->prev_version=get_version_from_piece_ppa(kv_wrapper->piece_ppa);
	bis->buffer[bis->buffer_idx++]=kv_wrapper;

	if(bis->start_lba>kv_wrapper->kv_ptr.lba){
		bis->start_lba=kv_wrapper->kv_ptr.lba;
	}
	if(bis->end_lba<kv_wrapper->kv_ptr.lba){
		bis->end_lba=kv_wrapper->kv_ptr.lba;
	}

	if(bis->prev_lba==UINT32_MAX){
		bis->prev_lba=kv_wrapper->kv_ptr.lba;
	}
	else{
		if(bis->prev_lba>=kv_wrapper->kv_ptr.lba){
			EPRINT("lba shoud be inserted by increasing order", true);
		}
		bis->prev_lba=kv_wrapper->kv_ptr.lba;
	}

	if(bis->buffer_idx==L2PGAP){
		 return true;
	}
	else return false;
}

value_set* sst_bis_finish(sst_bf_in_stream*){
	EPRINT("not implemented", true);
	return NULL;
}

extern char all_set_page[PAGESIZE];

value_set* sst_bis_get_result(sst_bf_in_stream *bis, bool last, uint32_t *debug_idx, key_ptr_pair *debug_kp){ 
	//it should return unaligned value when it no space
	if(!last && REMAIN_DATA_PPA(bis)==bis->buffer_idx){
		//EPRINT("change bis", false);
	}
	else if(bis->buffer_idx!=L2PGAP && !sst_bis_ppa_empty(bis) && !last){
		EPRINT("wtf??", true);
		return NULL;
	}
	else if(last && REMAIN_DATA_PPA(bis)< bis->buffer_idx){
		EPRINT("plz update new bis", true);
		return NULL;
	}


	value_set *res=inf_get_valueset(NULL, FS_MALLOC_W, PAGESIZE);
//	uint32_t ppa=page_manager_get_new_ppa(bis->pm, false, DATASEG);
	uint32_t ppa=page_manager_get_new_ppa_from_seg(bis->pm, bis->seg);
	if(ppa/_PPS != bis->start_piece_ppa/L2PGAP/_PPS){
		EPRINT("new data should same segment", true);
	}
	res->ppa=ppa;
	key_value_wrapper *kvw=NULL;
	for(uint32_t i=0; i<bis->buffer_idx;i++){
		kvw=bis->buffer[i];
		if(bis->now_map_data_idx==KP_IN_PAGE){
			bis->now_map_data=NULL;
		}

		if(!bis->now_map_data){
			bis->now_map_data=inf_get_valueset(all_set_page, FS_MALLOC_W, PAGESIZE);
			bis->map_data->push(bis->now_map_data);	
			bis->now_map_data_idx=0;
		}

		//printf("read : write target %u:%u - %u\n", kvw->kv_ptr.lba, kvw->piece_ppa, PIECETOPPA(kvw->piece_ppa));

		key_ptr_pair *map_pair=&((key_ptr_pair*)(bis->now_map_data->value))[bis->now_map_data_idx++];
		map_pair->lba=kvw->kv_ptr.lba;
		map_pair->piece_ppa=ppa*L2PGAP+(i%L2PGAP);

		debug_kp[i].lba=map_pair->lba;
		debug_kp[i].piece_ppa=map_pair->piece_ppa;
		validate_piece_ppa(bis->pm->bm, 1, &map_pair->piece_ppa, &map_pair->lba, &kvw->prev_version, true);

		memcpy(&res->value[(i%L2PGAP)*LPAGESIZE], kvw->kv_ptr.data, LPAGESIZE);
		if(kvw->kv_ptr.lba==debug_lba){
			printf("readed data before_write: %u (%u->%u), copied_data:%u (%u)\n", 
					*(uint32_t*)kvw->kv_ptr.data,
					kvw->piece_ppa, map_pair->piece_ppa,
					*(uint32_t*)&res->value[(i%L2PGAP)*LPAGESIZE], i);
		}

		if(bis->make_read_helper){
			read_helper_stream_insert(bis->rh, map_pair->lba, map_pair->piece_ppa);
		}


		bis->write_issued_kv_num++;
		if(kvw->free_target_req){
			inf_free_valueset(kvw->param->data, FS_MALLOC_R);
			if(!kvw->no_inter_param_alloc){
				compaction_free_read_param(LSM.cm, kvw->param);
			}
			else{
				free(kvw->param);
			}
		}
		free(kvw);
	}

	(*debug_idx)=bis->buffer_idx;
	bis->buffer_idx=0;
	return res;
}

bool sst_bis_ppa_empty(sst_bf_in_stream *bis){
	int32_t remain_ppa=REMAIN_DATA_PPA(bis);

	if(bis->now_map_data_idx==KP_IN_PAGE){
		if(remain_ppa<=(L2PGAP+L2PGAP)){
			return true;
		}
	}

	if(remain_ppa<0){
		EPRINT("can't be!", true);
	}
	if(remain_ppa==0 && bis->buffer_idx){
		EPRINT("it need to flush!", false);
	}
	return remain_ppa==0;
}

void sst_bis_free(sst_bf_in_stream *bis){
	if(!bis->map_data->empty() && bis->rh){
		EPRINT("make rh null before free", true);
	}

	if(!bis->map_data->empty()){
		EPRINT("not flush map data", true);	
	}
	
	if(bis->buffer_idx){
		EPRINT("remain data", true);	
	}
	
	if(bis->seg->used_page_num==_PPS){
		free(bis->seg);
	}
	else{
		if(LSM.pm->temp_data_segment){
			EPRINT("should be NULL", true);
		}
		LSM.pm->temp_data_segment=bis->seg;
	//	printf("temp_data_seg:%u\n", bis->seg->seg_idx);
	}

	delete bis->map_data;
	free(bis->buffer);
	free(bis);
}

