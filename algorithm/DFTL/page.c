#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <getopt.h>
#include "../../include/data_struct/lrucache.hpp"
#include "../../bench/bench.h"
#include "page.h"
#include "gc.h"
extern uint32_t test_key;
extern uint32_t debug_lba;
align_buffer a_buffer;
extern MeasureTime mt;
struct algorithm demand_ftl={
	.argument_set=demand_argument,
	.create=page_create,
	.destroy=page_destroy,
	.read=page_read,
	.write=page_write,
	.flush=page_flush,
	.remove=page_remove,
};

page_read_buffer rb;
uint32_t read_buffer_hit_cnt=0;

uint32_t page_create (lower_info* li,blockmanager *bm,algorithm *algo){
	algo->li=li; //lower_info means the NAND CHIP
	algo->bm=bm; //blockmanager is managing invalidation 

	demand_map_create(UINT32_MAX,li,bm);

	demand_ftl.algo_body=(void*)pm_body_create(bm);
	a_buffer.value=(char*)malloc(PAGESIZE);

	rb.pending_req=new std::multimap<uint32_t, algo_req *>();
	rb.issue_req=new std::multimap<uint32_t, algo_req*>();
	fdriver_mutex_init(&rb.pending_lock);
	fdriver_mutex_init(&rb.read_buffer_lock);
	rb.buffer_ppa=UINT32_MAX;
	return 1;
}

void page_destroy (lower_info* li, algorithm *algo){
	demand_map_free();
	printf("read buffer hit:%u\n", read_buffer_hit_cnt);
	free(a_buffer.value);

	delete rb.pending_req;
	delete rb.issue_req;
	return;
}

uint32_t page_read(request *const req){
	if(!req->param){
		for(uint32_t i=0; i<a_buffer.idx; i++){
			if(req->key==a_buffer.key[i]){
				memcpy(req->value->value, &a_buffer.value[i*LPAGESIZE], LPAGESIZE);
				req->end_req(req);		
				return 1;
			}
		}
	}

	return demand_page_read(req);
}

uint32_t align_buffering(request *const req, KEYT key, value_set *value){

	bool overlap=false;
	uint32_t overlapped_idx=UINT32_MAX;
	for(uint32_t i=0; i<a_buffer.idx; i++){
		if(a_buffer.key[i]==req->key){
			overlapped_idx=i;
			overlap=true;
			break;
		}
	}

	uint32_t target_idx=overlap?overlapped_idx:a_buffer.idx;

	if(req){
		memcpy(&a_buffer.value[target_idx*LPAGESIZE], req->value->value, LPAGESIZE);
		a_buffer.key[target_idx]=req->key;
		a_buffer.prefetching_info[target_idx]=req->consecutive_length;
	}
	else{
		memcpy(&a_buffer.value[target_idx*LPAGESIZE], req->value->value, LPAGESIZE);
		a_buffer.key[target_idx]=key;
		a_buffer.prefetching_info[target_idx]=req->consecutive_length;
	}
	if(req->key==debug_lba){
		printf("%u is buffered\n", debug_lba);
	}

	if(!overlap){ a_buffer.idx++;}

	if(a_buffer.idx==L2PGAP){
		ppa_t ppa=get_ppa(a_buffer.key, a_buffer.idx);
		value_set *value=inf_get_valueset(a_buffer.value, FS_MALLOC_W, PAGESIZE);
		send_user_req(NULL, DATAW, ppa, value);
		
		KEYT physical[L2PGAP];

		for(uint32_t i=0; i<L2PGAP; i++){
			physical[i]=ppa*L2PGAP+i;	
			if(a_buffer.key[i]==debug_lba){
				printf("%u -> %u[%u] %u \n", debug_lba, physical[i], i ,*(uint32_t*)&a_buffer.value[LPAGESIZE*i]);
			}
		}

		demand_map_assign(req, a_buffer.key, physical, a_buffer.prefetching_info);

		a_buffer.idx=0;
		return 1;
	}
	return 0;
}

uint32_t page_write(request *const req){
	if(req->param){
		return demand_map_assign(req, NULL, NULL, NULL);
	}

	/*std::pair<ppa_t, value_set *> r; 
	if(caching_num_lb!=0){
		r=buffer->put(req->key, req->value);
		if(r.first!=UINT_MAX){
			if(!align_buffering(NULL, r.first, r.second)){
				req->end_req(req);
			}
		}
	}
	else{*/
	if(!align_buffering(req, req->key, req->value)){
		req->end_req(req);
	}
	/*}*/

	return 0;
}


uint32_t page_remove(request *const req){
	req->end_req(req);
	return 0;
}

uint32_t page_flush(request *const req){
	abort();
	req->end_req(req);
	return 0;
}

extern struct algorithm demand_ftl;
typedef std::multimap<uint32_t, algo_req*>::iterator rb_r_iter;
inline void send_user_req(request *const req, uint32_t type, ppa_t ppa,value_set *value){
	/*you can implement your own structur for your specific FTL*/
	if(type==DATAR){
		fdriver_lock(&rb.read_buffer_lock);
		if(ppa==rb.buffer_ppa){
			read_buffer_hit_cnt++;
			memcpy(value->value, &rb.buffer_value[(value->ppa%L2PGAP)*LPAGESIZE], LPAGESIZE);
			req->end_req(req);
			fdriver_unlock(&rb.read_buffer_lock);
			return;
		}
		fdriver_unlock(&rb.read_buffer_lock);
	}

	page_params* params=(page_params*)malloc(sizeof(page_params));
	algo_req *my_req=(algo_req*)malloc(sizeof(algo_req));
	params->value=value;
	my_req->parents=req;//add the upper request
	my_req->end_req=page_end_req;//this is callback function
	my_req->param=(void*)params;//add your parameter structure 
	my_req->type=type;//DATAR means DATA reads, this affect traffics results
	/*you note that after read a PPA, the callback function called*/


	if(type==DATAR){
		fdriver_lock(&rb.pending_lock);
		rb_r_iter temp_r_iter=rb.issue_req->find(ppa);
		if(temp_r_iter==rb.issue_req->end()){
			rb.issue_req->insert(std::pair<uint32_t,algo_req*>(ppa, my_req));
			fdriver_unlock(&rb.pending_lock);
		}
		else{
			rb.pending_req->insert(std::pair<uint32_t, algo_req*>(ppa, my_req));
			fdriver_unlock(&rb.pending_lock);
			return;
		}
	}

	switch(type){
		case DATAR:
			demand_ftl.li->read(ppa,PAGESIZE,value,ASYNC,my_req);
			break;
		case DATAW:
			demand_ftl.li->write(ppa,PAGESIZE,value,ASYNC,my_req);
			break;
	}
}

static void processing_pending_req(algo_req *req, value_set *v){
	request *parents=req->parents;
	page_params *params=(page_params*)req->param;
	memcpy(params->value->value, &v->value[(params->value->ppa%L2PGAP)*LPAGESIZE], LPAGESIZE);
	//parents->type_ftl=parents->type_lower=0;
	parents->end_req(parents);
	free(params);
	free(req);
}

void *page_end_req(algo_req* input){
	//this function is called when the device layer(lower_info) finish the request.
	rb_r_iter target_r_iter;
	algo_req *pending_req;
	page_params* params=(page_params*)input->param;
	switch(input->type){
		case DATAW:
			inf_free_valueset(params->value,FS_MALLOC_W);
			break;
		case DATAR:
			fdriver_lock(&rb.pending_lock);
			target_r_iter=rb.pending_req->find(params->value->ppa/L2PGAP);
			for(;target_r_iter->first==params->value->ppa/L2PGAP && 
					target_r_iter!=rb.pending_req->end();){
				pending_req=target_r_iter->second;
				processing_pending_req(pending_req, params->value);
				rb.pending_req->erase(target_r_iter++);
			}
			rb.issue_req->erase(params->value->ppa/L2PGAP);
			fdriver_unlock(&rb.pending_lock);

			fdriver_lock(&rb.read_buffer_lock);
			rb.buffer_ppa=params->value->ppa/L2PGAP;
			memcpy(rb.buffer_value, params->value->value, PAGESIZE);
			fdriver_unlock(&rb.read_buffer_lock);

			if(params->value->ppa%L2PGAP){
				memmove(params->value->value, &params->value->value[(params->value->ppa%L2PGAP)*LPAGESIZE], LPAGESIZE);
			}

			break;
	}
	request *res=input->parents;
	if(res){
		//res->type_ftl=res->type_lower=0;
		res->end_req(res);//you should call the parents end_req like this
	}
	free(params);
	free(input);
	return NULL;
}
