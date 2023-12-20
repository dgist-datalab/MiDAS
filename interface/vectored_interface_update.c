#include "vectored_interface.h"
#include "layer_info.h"
#include "../include/container.h"
#include "../bench/bench.h"
#include "../include/utils/cond_lock.h"
#include "../include/utils/kvssd.h"
#include "../include/utils/tag_q.h"
#include "../include/utils/data_checker.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <map>

extern master_processor mp;
extern tag_manager *tm;
int32_t flying_cnt = QDEPTH;
static pthread_mutex_t flying_cnt_lock=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t req_cnt_lock=PTHREAD_MUTEX_INITIALIZER;
bool vectored_end_req (request * const req);

std::multimap<uint32_t, request *> *stop_req_list;
std::map<uint32_t, request *> *stop_req_log_list;
typedef std::multimap<uint32_t, request *>::iterator stop_req_iter;
/*request-length request-size tid*/
/*type key-len key offset length value*/

inline char *buf_parser(char *buf, uint32_t* idx, uint32_t length){
	char *res=&buf[*idx];
	(*idx)+=length;
	return res;
}

void* inf_transaction_end_req(void *req);
extern bool TXN_debug;
extern char *TXN_debug_ptr;
static uint32_t seq_val;
volatile vec_request *now_processing;

uint32_t inf_vector_make_req(char *buf, void* (*end_req) (void*), uint32_t mark){
	uint32_t idx=0;
	vec_request *txn=(vec_request*)malloc(sizeof(vec_request));
	//idx+=sizeof(uint32_t);//length;
	txn->tid=*(uint32_t*)buf_parser(buf, &idx, sizeof(uint32_t)); //get tid;
	txn->size=*(uint32_t*)buf_parser(buf, &idx, sizeof(uint32_t)); //request size;


	txn->buf=buf;
	txn->done_cnt=0;
	txn->end_req=end_req;
	txn->mark=mark;
	txn->req_array=(request*)malloc(sizeof(request)*txn->size);

	static uint32_t seq=0;
	uint32_t prev_lba=UINT32_MAX;
	uint32_t consecutive_cnt=0;
	for(uint32_t i=0; i<txn->size; i++){
		request *temp=&txn->req_array[i];
		temp->tid=txn->tid;
		temp->mark=txn->mark;
		temp->parents=txn;
		temp->type=*(uint8_t*)buf_parser(buf, &idx, sizeof(uint8_t));
		temp->end_req=vectored_end_req;
		temp->param=NULL;
		temp->value=NULL;
		temp->isAsync=ASYNC;
		temp->seq=seq++;
		temp->type_ftl=0;
		temp->type_lower=0;
		temp->flush_all=0;
		switch(temp->type){
#ifdef KVSSD
			case FS_TRANS_COMMIT:
				temp->tid=*(uint32_t*)buf_parser(buf,&idx, sizeof(uint32_t));
			case FS_TRANS_BEGIN:
			case FS_TRANS_ABORT:
				continue;
#endif
			case FS_GET_T:
				seq_val=temp->seq;
				temp->magic=0;
				temp->value=inf_get_valueset(NULL, FS_MALLOC_R, PAGESIZE);
				break;
			case FS_SET_T:
				temp->value=inf_get_valueset(NULL, FS_MALLOC_W, PAGESIZE);
				break;
			default:
				printf("error type!\n");
				abort();
				break;

		}

		temp->key=*(uint32_t*)buf_parser(buf,&idx,sizeof(uint32_t));
		if(prev_lba==UINT32_MAX){
			prev_lba=temp->key;
		}
		else{
			if(prev_lba+1==temp->key){
				consecutive_cnt++;
			}
			else{
				txn->req_array[i-consecutive_cnt-1].is_sequential_start=(consecutive_cnt!=0);
				txn->req_array[i-consecutive_cnt-1].consecutive_length=consecutive_cnt;
				consecutive_cnt=0;
			}
			prev_lba=temp->key;
			temp->consecutive_length=0;
		}

		if(mp._data_check_flag && temp->type==FS_SET_T){
			__checking_data_make( temp->key,temp->value->value);
		}
		temp->offset=*(uint32_t*)buf_parser(buf, &idx, sizeof(uint32_t));	
	}

	txn->req_array[(txn->size-1)-consecutive_cnt].is_sequential_start=(consecutive_cnt!=0);
	txn->req_array[(txn->size-1)-consecutive_cnt].consecutive_length=consecutive_cnt;

	while(1){
		pthread_mutex_lock(&flying_cnt_lock);
		if(flying_cnt - (int32_t)txn->size < 0){
			pthread_mutex_unlock(&flying_cnt_lock);
			continue;
		}
		else{
			flying_cnt-=txn->size;
			if(flying_cnt<0){
				printf("abort!!!\n");
				abort();
			}
			pthread_mutex_unlock(&flying_cnt_lock);
		}
		
		if(q_enqueue((void*)txn, mp.processors[0].req_q)){
			break;
		}
	}

	return 1;
}

bool check_request_pending(processor *pr){
	if( pr->req_q->size==0 && now_processing && flying_cnt < now_processing->size){
		return true;
	}
	else return false;
}


static inline void remove_req_from_stop_list(request *req){
	stop_req_log_list->erase(req->global_seq);
	stop_req_iter iter=stop_req_list->find(req->key);
	for(; iter->first==req->key && iter!=stop_req_list->end();){
		stop_req_list->erase(iter++);
	}
}

static inline bool processing_read_from_pending_req(request *req){
	stop_req_iter iter=stop_req_list->find(req->key);
	uint32_t temp_global_seq=0;
	request *target_req=NULL;
	//printf("req->key:%u\n", req->key);
	if(req->key==512){
		printf("break!\n");
	}
	MeasureTime mt;
	measure_init(&mt);
	measure_start(&mt);
	for(; iter->first==req->key && iter!=stop_req_list->end(); iter++){
		if(req->global_seq > iter->second->global_seq){
			if(temp_global_seq < iter->second->global_seq){
				temp_global_seq=iter->second->global_seq;
				target_req=iter->second;
			}
		}
	}

	if(target_req){
		memcpy(req->value->value, target_req->value->value, LPAGESIZE);
		measure_stamp(&mt);
		return true;
	}
	measure_stamp(&mt);
	return false;
}

static uint32_t get_next_request(processor *pr, request** inf_req, vec_request **vec_req,
		bool *read_only_flag){
	if(((*inf_req)=(request*)q_dequeue(pr->retry_q))){
		return 1;
	}
	else if(((*vec_req)=(vec_request*)q_dequeue(pr->req_q))){
		return 2;
	}
	else if(check_request_pending(pr) && stop_req_log_list->size()){
		*read_only_flag=false;
		request *res=stop_req_log_list->begin()->second;
		res->flush_all=true;
		(*inf_req)=res;
		return 1;
	}
	return 0;
}

request *get_retry_request(processor *pr){
	void *inf_req=NULL;
	inf_req=q_dequeue(pr->retry_q);

	return (request*)inf_req;
}

void *vectored_main(void *__input){
	vec_request *vec_req;
	request* inf_req;
	processor *_this=NULL;
	for(int i=0; i<1; i++){
		if(pthread_self()==mp.processors[i].t_id){
			_this=&mp.processors[i];
		}
	}
	char thread_name[128]={0};
	sprintf(thread_name,"%s","vecotred_main_thread");
	pthread_setname_np(pthread_self(),thread_name);
	uint32_t type;
	uint32_t caller_res=0;
	bool read_only_flag=false;
	while(1){
		if(mp.stopflag)
			break;
		type=get_next_request(_this, &inf_req, &vec_req, &read_only_flag);
		if(type==0){
			continue;
		}
		else if(type==1){ //rtry
			if(inf_req->flush_all==0){
				inf_req->tag_num=tag_manager_get_tag(tm);
				inf_algorithm_caller(inf_req);
			}
			else{
				while(stop_req_log_list->size()){
					request *treq=stop_req_log_list->begin()->second;
					if(treq->key==512){
						printf("key 512 erased! %p\n", treq);
					}
					if(stop_req_log_list->size()==1){
						treq->flush_all=true;
					}
					else{
						treq->flush_all=false;
					}
					measure_init(&treq->latency_checker);
					measure_start(&treq->latency_checker);
					treq->tag_num=tag_manager_get_tag(tm);
					remove_req_from_stop_list(treq);
					
					if(treq->type!=FS_SET_T){
						printf("type error!\n"); 
						abort();
					}
					caller_res=inf_algorithm_caller(treq);
				}

				if(caller_res==UINT32_MAX){
					read_only_flag=true;
				}
				continue;
			}

		}else{
			uint32_t size=vec_req->size;
			for(uint32_t i=0; i<size; i++){
				/*retry queue*/
				while(1){
					request *temp_req=get_retry_request(_this);
					if(temp_req){	
						temp_req->tag_num=tag_manager_get_tag(tm);
						inf_algorithm_caller(temp_req);
					}
					else{
						break;
					}
				}
	
				request *req=&vec_req->req_array[i];
				if(read_only_flag){
					if(req->type==FS_SET_T && !req->flush_all){
						if(req->key==512){
							printf("key 512 inserted! %p\n", req);
						}
						stop_req_list->insert(std::pair<uint32_t, request*>(req->key, req));
						stop_req_log_list->insert(std::pair<uint32_t, request*>(req->global_seq, req));
						continue;
					}

				}

				uint32_t req_type;
				bool flush_all_req=req->flush_all;

				switch(req->type){
					case FS_GET_T:
					case FS_SET_T:
						measure_init(&req->latency_checker);
						measure_start(&req->latency_checker);
					break;
				}

				if(req->type==FS_GET_T && req->key==0){
					printf("break!\n");
				}

				req->tag_num=tag_manager_get_tag(tm);
				if(req->type==FS_GET_T && processing_read_from_pending_req(req)){
					req->end_req(req);
					continue;
				}

				req_type=req->type;
				caller_res=inf_algorithm_caller(req);
				if(req_type==FS_SET_T){
					if(flush_all_req){
						printf("flush all req should finished before!\n");
						abort();
					}

					if(caller_res==UINT32_MAX){
						read_only_flag=true;
					}
				}
			}
		}

	}
	return NULL;
}

bool vectored_end_req (request * const req){
	vectored_request *preq=req->parents;
	switch(req->type){
		case FS_GET_T:
	//		printf("ack req->seq:%u\n", req->seq);
	//		fprintf(stderr,"read:%u\n",req->seq);
			if(mp._data_check_flag){
				__checking_data_check(req->key, req->value->value);
			}
		case FS_NOTFOUND_T:
			bench_reap_data(req, mp.li);
	//		memcpy(req->buf, req->value->value, 4096);
	//		printf("return read break %u!\n", req->seq);
			if(req->value)
				inf_free_valueset(req->value,FS_MALLOC_R);
			break;
		case FS_SET_T:
			bench_reap_data(req, mp.li);
			if(req->value) inf_free_valueset(req->value, FS_MALLOC_W);
			break;
		default:
			abort();
	}

	release_each_req(req);  

	pthread_mutex_lock(&req_cnt_lock);
	preq->done_cnt++;
	uint32_t tag_num=req->tag_num;
	if(preq->size==preq->done_cnt){
		if(preq->end_req){
			preq->end_req((void*)preq);	
		}
	}
	pthread_mutex_unlock(&req_cnt_lock);
	return true;
}

void inf_algorithm_testing(){
	mp.algo->test();
}

void release_each_req(request *req){
	uint32_t tag_num=req->tag_num;
	pthread_mutex_lock(&flying_cnt_lock);

	flying_cnt++;
	if(flying_cnt > QDEPTH){
		printf("???\n");
		abort();
	}
	pthread_mutex_unlock(&flying_cnt_lock);

	tag_manager_free_tag(tm, tag_num);
}

void assign_vectored_req(vec_request *txn){
	if(!stop_req_list){
		stop_req_list=new std::multimap<uint32_t, request *>();
		stop_req_log_list=new std::map<uint32_t, request*>();
	}
	while(1){
		now_processing=txn;
		pthread_mutex_lock(&flying_cnt_lock);
		if(flying_cnt - (int32_t)txn->size < 0){
			pthread_mutex_unlock(&flying_cnt_lock);
			continue;
		}
		else{
			flying_cnt-=txn->size;
			if(flying_cnt<0){
				printf("abort!!!\n");
				abort();
			}
			pthread_mutex_unlock(&flying_cnt_lock);
		}
	//	printf("flying tagnum %u, txn->size %u, req_q->size:%u\n", QDEPTH-flying_cnt, txn->size, mp.processors[0].req_q->size);
		
		if(q_enqueue((void*)txn, mp.processors[0].req_q)){
			break;
		}
	}
}

