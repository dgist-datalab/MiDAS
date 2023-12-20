#include "../../interface/interface.h"
#include "lftl_slab.h"
#include "write_buffer.h"
#include "io.h"
#include "lsmtree.h"
#include <stdio.h>
#include <stdlib.h>
extern uint32_t debug_lba;
extern lsmtree LSM;
write_buffer *write_buffer_reinit(write_buffer *wb){
	std::map<uint32_t, buffer_entry*>::iterator it=wb->data->begin();
	for(;it!=wb->data->end(); it++){
		slab_free(wb->sm,(char*)it->second);
	}
	wb->data->clear();
	wb->buffered_entry_num=0;
	return wb;
}

write_buffer *write_buffer_init(uint32_t max_buffered_entry_num, page_manager *pm, uint32_t type){
	write_buffer *res=(write_buffer*)calloc(1, sizeof(write_buffer));
	res->data=new std::map<uint32_t, buffer_entry*>();
	res->max_buffer_entry_num=max_buffered_entry_num;
	//res->data=(buffer_entry**)calloc(max_buffered_entry_num,sizeof(buffer_entry));
	res->sm=slab_master_init(sizeof(buffer_entry), max_buffered_entry_num);
	res->pm=pm;
	fdriver_mutex_init(&res->cnt_lock);
	fdriver_mutex_init(&res->sync_lock);
	res->flushed_req_cnt=0;
	res->type=type;
	return res;
}

write_buffer *write_buffer_init_for_gc(uint32_t max_buffered_entry_num, page_manager *pm, uint32_t type, read_helper_param rhp){
	write_buffer *res=(write_buffer*)calloc(1, sizeof(write_buffer));
	res->data=new std::map<uint32_t, buffer_entry*>();
	res->max_buffer_entry_num=max_buffered_entry_num;
	//res->data=(buffer_entry**)calloc(max_buffered_entry_num,sizeof(buffer_entry));
	res->sm=slab_master_init(sizeof(buffer_entry), max_buffered_entry_num);
	res->pm=pm;
	fdriver_mutex_init(&res->cnt_lock);
	fdriver_mutex_init(&res->sync_lock);
	res->flushed_req_cnt=0;
	res->type=type;

	res->rhp=rhp;
	//res->rh=read_helper_init(rhp);
	return res;
}

typedef struct flush_req_param{
	value_set *value;
	uint32_t *cnt;
	fdriver_lock_t *cnt_lock;
	fdriver_lock_t *sync_lock;
}flush_req_param;

static void *flush_end_req(algo_req *req){
	flush_req_param *param=(flush_req_param*)req->param;
	value_set *value=(value_set*)param->value;
	if(param->sync_lock){
		fdriver_lock(param->sync_lock);
		(*param->cnt)--;
		if(!(*param->cnt)){
			fdriver_unlock(param->sync_lock);
		}
		fdriver_unlock(param->sync_lock);
	}
	inf_free_valueset(value, FS_MALLOC_W);
	free(param);
	free(req);
	return NULL;
}

static algo_req *make_flush_algo_req(write_buffer *wb,uint32_t ppa, value_set *value, bool sync, bool isgc){
	algo_req *res=(algo_req*)malloc(sizeof(algo_req));
	flush_req_param *param=(flush_req_param*)malloc(sizeof(flush_req_param));
	res->type=isgc?GCDW:DATAW;
	res->ppa=ppa;
	res->end_req=flush_end_req;
	param->value=value;
	if(sync){
		param->cnt_lock=&wb->cnt_lock;
		param->sync_lock=&wb->sync_lock;
		fdriver_lock(param->cnt_lock);
		param->cnt=&wb->flushed_req_cnt;
		wb->flushed_req_cnt++;	
		fdriver_unlock(param->cnt_lock);
	}
	else{
		param->cnt_lock=param->sync_lock=NULL;
	}

	res->param=(void*)param;
	return res;
}

static inline uint32_t number_of_write_page(write_buffer *wb){
	return wb->buffered_entry_num/L2PGAP + (wb->buffered_entry_num%L2PGAP?1:0);
}

key_ptr_pair* write_buffer_flush(write_buffer *wb, uint32_t target_num, bool sync){
	std::map<uint32_t, buffer_entry*>::iterator it=wb->data->begin();
	value_set* target_value=NULL;
	uint32_t ppa=-1;
	//char *oob=NULL;
	key_ptr_pair *res=(key_ptr_pair*)malloc((wb->data->size()+1)*sizeof(key_ptr_pair));
	fdriver_lock(&LSM.flush_lock);

	uint32_t i=0;
	uint32_t flushed_cnt=MIN(KP_IN_PAGE, target_num);
	uint32_t prev_version;
	for(;it!=wb->data->end() && i<flushed_cnt; i++){
		uint8_t inter_idx=i%L2PGAP;
		if(inter_idx==0){
			ppa=page_manager_get_new_ppa(wb->pm, false, SEPDATASEG);
			target_value=inf_get_valueset(NULL, FS_MALLOC_W, PAGESIZE);
			target_value->ppa=ppa;
		//	oob=wb->pm->bm->get_oob(wb->pm->bm, ppa);
		}	

		//page_manager_insert_lba(wb->pm, it->first);
		memcpy(&target_value->value[inter_idx*LPAGESIZE], it->second->data.data->value, LPAGESIZE);//copy value
		//*(uint32_t*)&oob[sizeof(uint32_t)*inter_idx]=it->first;//copy lba to oob
		res[i].piece_ppa=ppa*L2PGAP+inter_idx;
		res[i].lba=it->first;
		prev_version=version_map_lba(LSM.last_run_version, it->first);
#ifdef LSM_DEBUG
		if(res[i].lba==debug_lba){
			printf("map target:%u -> %u in buffer\n", res[i].lba, res[i].piece_ppa);
		}
#endif

		validate_piece_ppa(wb->pm->bm, 1, &res[i].piece_ppa, &res[i].lba, &prev_version, true);
		inf_free_valueset(it->second->data.data, FS_MALLOC_W);
		it->second->data.data=NULL;

		if(inter_idx==(L2PGAP-1)){//issue data
			io_manager_issue_internal_write(ppa, target_value, make_flush_algo_req(wb, ppa, target_value, sync, false), false);
			ppa=-1;
		//	oob=NULL;
			target_value=NULL;
		}
		wb->buffered_entry_num--;
		wb->data->erase(it++);
	}

	res[i].lba=UINT32_MAX;
	res[i].piece_ppa=UINT32_MAX;

	if(sync){
		fdriver_lock(&wb->sync_lock);
	}	
	fdriver_unlock(&LSM.flush_lock);

	return res;
}

uint32_t write_buffer_insert(write_buffer *wb, uint32_t lba, value_set* value){
	if(wb->is_immutable){
		return encode_return_code(FAILED, WB_IS_IMMUTABLE);
	}

	std::map<uint32_t, buffer_entry*>::iterator it;
	it=wb->data->find(lba);
	if(!wb->data->size() || it==wb->data->end()){/*new data*/
		buffer_entry *ent=(buffer_entry*)slab_alloc(wb->sm);
		ent->lba=lba;
		ent->data.data=value;
		wb->data->insert(std::pair<uint32_t, buffer_entry*>(lba, ent));
		wb->buffered_entry_num++;
	}
	else{/*update data*/
		buffer_entry *obsolete=wb->data->at(lba);
		inf_free_valueset(obsolete->data.data, FS_MALLOC_W);
		obsolete->data.data=value;
		wb->data->at(lba)=obsolete;
	}

	if(wb->buffered_entry_num==wb->max_buffer_entry_num){
		return encode_return_code(SUCCESSED, WB_FULL);
	}
	else{
		return encode_return_code(SUCCESSED, WB_NONE);
	}
}


char *write_buffer_get(write_buffer *wb, uint32_t lba){
	std::map<uint32_t, buffer_entry*>::iterator it=wb->data->find(lba);
	if(it==wb->data->end()){
		return NULL;
	}
	else{
		if(wb->type==NORMAL_WB)
			return it->second->data.data->value;
		else
			return it->second->data.gc_data;
	}
}

void write_buffer_free(write_buffer* wb){
	std::map<uint32_t, buffer_entry*>::iterator it;
	if(wb->type==NORMAL_WB){
		for(it=wb->data->begin(); it!=wb->data->end(); it++){
			if(it->second->data.data){
				inf_free_valueset(it->second->data.data,FS_MALLOC_W);
			}
		}
	}

	delete wb->data;
	slab_master_free(wb->sm);
	fdriver_destroy(&wb->cnt_lock);
	fdriver_destroy(&wb->sync_lock);
	free(wb);
}

uint32_t write_buffer_insert_for_gc(write_buffer *wb, uint32_t lba, char *gc_data){
	if(wb->is_immutable){
		return encode_return_code(FAILED, WB_IS_IMMUTABLE);
	}

	std::map<uint32_t, buffer_entry*>::iterator it;
	it=wb->data->find(lba);
	if(!wb->data->size() || it==wb->data->end()){/*new data*/
		buffer_entry *ent=(buffer_entry*)slab_alloc(wb->sm);
		ent->lba=lba;
		ent->data.gc_data=gc_data;
		wb->data->insert(std::pair<uint32_t, buffer_entry*>(lba, ent));
		wb->buffered_entry_num++;
	}
	else{/*update data*/
		EPRINT("not allowed!", true);
	}

	return encode_return_code(SUCCESSED, WB_NONE);
}


key_ptr_pair* write_buffer_flush_for_gc(write_buffer *wb, bool sync, uint32_t seg_idx, bool *force_stop, uint32_t prev_map, std::map<uint32_t, gc_mapping_check_node*>* gkv){
	if(wb->buffered_entry_num==0) return NULL;
	else if((int)wb->buffered_entry_num<0){
		EPRINT("minus number not allowed!", true);
	}
	std::map<uint32_t, buffer_entry*>::iterator it=wb->data->begin();
	value_set* target_value=NULL;
	uint32_t ppa=-1;
	//char *oob=NULL;
	key_ptr_pair *res=(key_ptr_pair*)malloc(PAGESIZE);
	memset(res, -1, PAGESIZE);
	uint32_t remain_page_num=0;
	std::map<uint32_t, gc_mapping_check_node*>::iterator gkv_iter;
	if(!wb->rh && wb->rhp.type!=HELPER_NONE){
retry:
		remain_page_num=page_manager_get_reserve_remain_ppa(LSM.pm, false, seg_idx);
		if(remain_page_num==1){
			//should change next ppa
			page_manager_move_next_seg(LSM.pm, false, true, DATASEG);
			goto retry;
		}
		else if(wb->buffered_entry_num+(1+prev_map)*L2PGAP <=remain_page_num*L2PGAP){
			wb->rhp.member_num=wb->buffered_entry_num;
		}
		else{
			wb->rhp.member_num=(remain_page_num-prev_map-1)*L2PGAP;
		}
		wb->rh=read_helper_init(wb->rhp);
	}

	uint8_t inter_idx;
	uint32_t prev_version;
	for(uint32_t i=0; it!=wb->data->end() && i<KP_IN_PAGE; i++){
		inter_idx=i%L2PGAP;
		if(gkv){
			gkv_iter=gkv->find(it->first);	
		}

		if(inter_idx==0){
			ppa=page_manager_get_reserve_new_ppa(wb->pm, false, seg_idx);
			target_value=inf_get_valueset(NULL, FS_MALLOC_W, PAGESIZE);
			target_value->ppa=ppa;
	//		oob=wb->pm->bm->get_oob(wb->pm->bm, ppa);
		}	
	
		//page_manager_insert_lba(wb->pm, it->first);
		memcpy(&target_value->value[inter_idx*LPAGESIZE], it->second->data.gc_data, LPAGESIZE);
	//	*(uint32_t*)&oob[sizeof(uint32_t)*inter_idx]=it->first;//copy lba to oob
		res[i].piece_ppa=ppa*L2PGAP+inter_idx;
		res[i].lba=it->first;
		prev_version=version_map_lba(LSM.last_run_version, it->first);
#ifdef LSM_DEBUG
		if(debug_lba==res[i].lba){
			static int cnt=0;
			printf("[%u] gc %u -> %u\n", cnt++, res[i].lba, res[i].piece_ppa);
		}
#endif
		validate_piece_ppa(wb->pm->bm, 1, &res[i].piece_ppa, &res[i].lba,  &prev_version, true);
		if(wb->rh){
			read_helper_stream_insert(wb->rh, res[i].lba, res[i].piece_ppa);
		}

		if(gkv){
			if(gkv_iter->first!=it->first){
				EPRINT("error", true);
			}
			gkv_iter->second->new_piece_ppa=res[i].piece_ppa;
			gkv_iter++;
		}

		if(inter_idx==(L2PGAP-1)){//issue data
			io_manager_issue_internal_write(ppa, target_value, make_flush_algo_req(wb, ppa, target_value, sync, true), false);
			if(force_stop && (ppa+prev_map+1)%_PPS==_PPS-1){
				*force_stop=true; 
			}
			if(force_stop && i==KP_IN_PAGE-1){
				if((ppa+prev_map+1+1)%_PPS==_PPS-1){//need two page, bound case
					*force_stop=true;	
				}
			}
			ppa=-1;
			//oob=NULL;
			target_value=NULL;
		}
		wb->buffered_entry_num--;
		wb->data->erase(it++);

		if(force_stop && (*force_stop)) break;
	}

	if(inter_idx!=(L2PGAP-1)){
		if(ppa==UINT32_MAX){
			EPRINT("it can't be", true);
		}
		io_manager_issue_internal_write(ppa, target_value, make_flush_algo_req(wb, ppa, target_value, sync,true), false);
		ppa=-1;
	}

	if(sync){
		fdriver_lock(&wb->sync_lock);
	}	

	return res;
}

