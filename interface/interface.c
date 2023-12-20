#include "interface.h"
#include "vectored_interface.h"
#include "../include/container.h"
#include "../include/FS.h"
#include "../bench/bench.h"
#include "../bench/measurement.h"

#ifdef KVSSD
#include "../include/data_struct/hash_kv.h"
#else
#include "../include/data_struct/hash.h"
#endif

#include "../include/data_struct/redblack.h"
#include "../include/utils/cond_lock.h"
#include "../include/utils/tag_q.h"
#include "../include/utils/data_checker.h"
#include "buse.h"
#include "layer_info.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <map>

extern Redblack rb_tree;
extern pthread_mutex_t rb_lock;
extern MeasureTime write_opt_time[10];

master_processor mp;
tag_manager *tm;

bool sync_apps;
void *p_main(void*);


#ifdef BUSE_MEASURE
MeasureTime infTime;
MeasureTime infendTime;
#endif

static request *inf_get_req_instance(const FSTYPE type, KEYT key, char *value, int len,int mark, bool fromApp);

#ifdef interface_pq
static request *inf_get_req_instance(const FSTYPE type, KEYT key, char *value, int len,int mark, bool fromApp);
static request *inf_get_multi_req_instance(const FSTYPE type, KEYT *key, char **value, int *len,int req_num,int mark, bool fromApp);
static bool qmanager_write_checking(processor * t,request *req){
	bool res=false;
	Redblack finding;
	pthread_mutex_lock(&t->qm_lock);
#ifdef KVSSD
	if(rb_find_str(t->qmanager,req->key,&finding))
#else
	if(rb_find_int(t->qmanager,req->key,&finding))
#endif
	{
		res=true;
		//copy value
#ifdef KVSSD
		free(req->key.key);
#endif
	}
	else{
		
#ifdef KVSSD
		rb_insert_str(t->qmanager,req->key,(void*)req);
#else
		rb_insert_int(t->qmanager,req->key,(void*)req);
#endif
	}
	pthread_mutex_unlock(&t->qm_lock);
#if 0
#if defined(KVSSD) && defined(demand)
	if(res) return res;
	pthread_mutex_lock(&rb_lock);
	rb_insert_str(rb_tree, req->key, NULL);
	pthread_mutex_unlock(&rb_lock);
#endif
#endif
	return res;
}

static bool qmanager_read_checking(processor *t,request *req){
	bool res=false;
	Redblack finding;
	pthread_mutex_lock(&t->qm_lock);
#ifdef KVSSD
	if(rb_find_str(t->qmanager,req->key,&finding))
#else
	if(rb_find_int(t->qmanager,req->key,&finding))
#endif
	{
		res=true;
	}
	pthread_mutex_unlock(&t->qm_lock);
	return res;
}

static bool qmanager_delete(processor *t, request *req){
	bool res=false;
	Redblack finding;
	pthread_mutex_lock(&t->qm_lock);
#ifdef KVSSD
	if(rb_find_str(t->qmanager,req->key,&finding))
#else
	if(rb_find_int(t->qmanager,req->key,&finding))
#endif
	{
		res=true;
		rb_delete(finding,false);
	}
	else{
		abort();
	}
	pthread_mutex_unlock(&t->qm_lock);
	return res;
}

void *qmanager_find_by_algo(KEYT key){
	Redblack finding;
	for(int i=0; i<1; i++){
		processor *t=&mp.processors[i];
#ifdef KVSSD
		if(rb_find_str(t->qmanager,key,&finding))
#else
		if(rb_find_int(t->qmanager,key,&finding))
#endif	
		{
			return finding->item;
		}
		continue;
	}
	return NULL;
}
#endif
void assign_req(request* req){
	bool flag=false;
	req->tag_num=tag_manager_get_tag(tm);
	while(!flag){
		processor *t=&mp.processors[0];
#ifdef interface_pq
		switch(req->type){
			case FS_SET_T:
				if(qmanager_write_checking(t,req)){
					req->end_req(req);
					return;
				}
				else if(q_enqueue((void*)req,t->req_q)){
					flag=true;
				}
				break;
			case FS_GET_T:
				if(qmanager_read_checking(t,req)){
					if(!req->isstart) req->type_ftl=10;
					req->end_req(req);
					return;
				}
			default: //for read
				if(q_enqueue((void*)req,t->req_rq)){
					flag=true;
				}
				break;
		}
#else
		if(q_enqueue((void*)req,t->req_q)){
			flag=true; continue;
		}
#endif
	}


#ifdef BUSE_MEASURE
    if(req->type==FS_BUSE_R)
        MA(&infTime);
#endif
}
extern int32_t flying_cnt;
bool inf_assign_try(request *req){
	bool flag=false;
	for(int i=0; i<1; i++){
		processor *t=&mp.processors[i];
		tag_manager_free_tag(tm, req->tag_num);
		while(q_enqueue((void*)req,t->retry_q)){
			flag=true;
			break;
		}
	}

	if(flag==false){
		printf("%u retry queue is full\n", flying_cnt);
	}
	return flag;
}

uint64_t inter_cnt;
bool force_write_start;
int write_stop;
static request *get_next_request(processor *pr){

	void *_inf_req=NULL;
	if(force_write_start || (write_stop && pr->req_q->size==QDEPTH) || sync_apps)
		write_stop=false;
	if((_inf_req=q_dequeue(pr->retry_q))) goto send_req; //check retry
#ifdef interface_pq
	else if((_inf_req=q_dequeue(pr->req_rq))) goto send_req; //check read 
	else if(pr->retry_q->size || write_stop) goto send_req; //check write stop
#endif
	else if((_inf_req=q_dequeue(pr->req_q))){
#ifdef interface_pq
		qmanager_delete(pr,(request*)_inf_req);
#endif
	}

send_req:
	return (request*)_inf_req;
}

uint32_t inf_algorithm_caller(request *const inf_req){
	switch(inf_req->type){
		case FS_GET_T:
			mp.algo->read(inf_req);
			break;
		case FS_SET_T:
			return mp.algo->write(inf_req);
		case FS_DELETE_T:
			mp.algo->remove(inf_req);
			break;
		case FS_RMW_T:
			mp.algo->read(inf_req);
			break;
		case FS_BUSE_R:
			mp.algo->read(inf_req);
			break;
		case FS_BUSE_W:
			mp.algo->write(inf_req);
			break;
#ifdef KVSSD
		case FS_RANGEGET_T:
			mp.algo->range_query(inf_req);
			break;

		case FS_TRANS_BEGIN:
			mp.algo->trans_begin(inf_req);
			break;
		case FS_TRANS_COMMIT:
			mp.algo->trans_commit(inf_req);
			break;
		case FS_TRANS_ABORT:
			printf("it needs to be implemented!\n");
			abort();
			break;
#endif
		case FS_FLUSH_T:
			mp.algo->flush(inf_req);
			break;
		default:
			printf("wtf??, type %d\n", inf_req->type);
			inf_req->end_req(inf_req);
			break;
	}
	return 1;
}

void *p_main(void *__input){
	request *inf_req;
	processor *_this=NULL;
	for(int i=0; i<1; i++){
		if(pthread_self()==mp.processors[i].t_id){
			_this=&mp.processors[i];
		}
	}

	char thread_name[128]={0};
	sprintf(thread_name,"%s","inf_main_thread");
	pthread_setname_np(pthread_self(),thread_name);
	while(1){

		if(mp.stopflag)
			break;
		if(!(inf_req=get_next_request(_this))){

			continue;
		}
		inf_algorithm_caller(inf_req);
		inter_cnt++;
#ifdef CDF
		inf_req->isstart=true;
#endif
	}
	return NULL;
}

bool inf_make_req_fromApp(char _type, KEYT _key,uint32_t offset, uint32_t len,PTR _value,void *_req, void*(*end_func)(void*)){
	/*
	static bool start=false;
	if(!start){
		bench_init();
        _m=&_master->m[0];
        //_m->empty=false;
        //measure_init(&_m->benchTime);
        //MS(&_m->benchTime);
		bench_add(NOR,0,-1,-1);
		start=true;
	}
    //_m->m_num++;
	//value_set *value=(value_set*)malloc(sizeof(value_set));
    //value = req->value;
	if(_type!=FS_RMW_T){
        req=inf_get_req_instance(_type,_key,_value,len,0,true); //type, key, value, len, mark, fromApp
        value = req->value;
        ((struct buse*)_req)->value = value;
		//value->value=_value;
        if(_type!=FS_DELETE_T){
            value->rmw_value=NULL;
            value->offset=0;
        }
		//value->len=PAGESIZE;
	}else{
        req=inf_get_req_instance(_type,_key,_value,len,0,true);
        value = req->value;
        //((struct buse*)_req)->value=value;
		//value->value=(PTR)malloc(PAGESIZE);
		value->rmw_value=_value;
		value->offset=offset;
		//value->len=len;
	}
    if(_type!=FS_DELETE_T){
        value->length=len;
        value->dmatag=0;
        value->from_app=true;
    }

	//request *req=inf_get_req_instance(_type,_key,value,0,true);
	req->p_req=_req;

	cl_grap(flying);
#ifdef CDF
	req->isstart=false;
	measure_init(&req->latency_checker); //make_fromapps
	measure_start(&req->latency_checker);//make_fromapps
#endif
	assign_req(req);*/
	return true;
}

void inf_init(int apps_flag, int total_num, int argc, char **argv, bool data_check_flag){
#ifdef BUSE_MEASURE
    measure_init(&infTime);
    measure_init(&infendTime);
#endif

	tm=tag_manager_init(QDEPTH);
	//tm=tag_manager_init(1);
	mp.processors=(processor*)malloc(sizeof(processor)*1);
	for(int i=0; i<1; i++){
		processor *t=&mp.processors[i];
		pthread_mutex_init(&t->flag,NULL);
		pthread_mutex_lock(&t->flag);
		t->master=&mp;


		q_init(&t->req_q,QSIZE);
		q_init(&t->retry_q,QSIZE);

#ifdef interface_vector
		pthread_create(&t->t_id,NULL,&vectored_main, NULL);
#else
		pthread_create(&t->t_id,NULL,&p_main,NULL);
#endif
	}


	pthread_mutex_init(&mp.flag,NULL);
	if(apps_flag){
		bench_init();
		bench_add(NOR,0,-1,total_num);
	}

	layer_info_mapping(&mp, argc, argv);
	mp._data_check_flag=data_check_flag;
	if(mp._data_check_flag){
		__checking_data_init();
	}
	
/*
	mp.li->create(mp.li,mp.bm);
#ifdef partition
	int temp[PARTNUM];
	temp[MAP_S]=MAPPART_SEGS;
	temp[DATA_S]=DATAPART_SEGS;
	mp.bm->pt_create(mp.bm,PARTNUM,temp,mp.li);
#else
	mp.bm->create(mp.bm,mp.li);
#endif
	mp.algo->create(mp.li,mp.bm,mp.algo);*/
}


static request* inf_get_req_common(request *req, bool fromApp, int mark){
	static uint32_t seq_num=0;
	req->end_req=inf_end_req;
	req->isAsync=ASYNC;
	req->param=NULL;
	req->type_ftl = 0;
	req->type_lower = 0;
	req->before_type_lower=0;
	req->seq=seq_num++;
	req->special_func=NULL;
	req->added_end_req=NULL;
#ifndef USINGAPP
	req->mark=mark;
#endif

#ifdef hash_dftl
	req->hash_params = NULL;
#endif
	req->parents = NULL;

	return req;
}

static request *inf_get_req_instance(const FSTYPE type, KEYT key, char *_value, int len,int mark,bool fromApp){
	request *req=(request*)malloc(sizeof(request));
	req->type=type;
//	req->key=key;
#ifdef hash_dftl
	req->num=0;
	req->cpl=0;
#endif

#ifdef KVSSD
	req->key.len=key.len;
	req->key.key=(char*)malloc(key.len);
	memcpy(req->key.key,key.key,key.len);
#else
	req->key=key;
#endif
	switch(type){
		case FS_DELETE_T:
			req->value=NULL;
			break;

		case FS_SET_T:
#ifdef DVALUE
			req->value=inf_get_valueset(NULL,FS_SET_T,len+key.len+sizeof(key.len));
			memcpy(&req->value->value[key.len+sizeof(key.len)],_value,len);
#else
			req->value=inf_get_valueset(_value,FS_SET_T,PAGESIZE);
#endif
#ifdef KVSSD
			memcpy(req->value->value,&key.len,sizeof(key.len));
			memcpy(&req->value->value[sizeof(key.len)],key.key,key.len);
#endif
			break;
		case FS_GET_T:
			req->value=inf_get_valueset(NULL,FS_GET_T,PAGESIZE);
			break;
		case FS_RANGEGET_T:
			/*
			req->multi_value=(value_set**)malloc(sizeof(value_set*)*req->num);
			for(int i=0; i<req->num; i++){
				req->multi_value[i]=inf_get_valueset(NULL,FS_GET_T,PAGESIZE);
			}*/
			break;
        case FS_BUSE_R:
            req->value=inf_get_valueset(_value,FS_BUSE_R,len);
            break;
        case FS_BUSE_W:
            req->value=inf_get_valueset(_value,FS_BUSE_W,len);
            break;
		default:
			break;
	}

	return inf_get_req_common(req,fromApp,mark);
}

static request *inf_get_multi_req_instance(const FSTYPE type, KEYT *keys, char **_value, int *len,int req_num,int mark, bool fromApp){
	request *req=(request*)malloc(sizeof(request));
	req->type=type;
	int i;
	switch(type){
		case FS_RANGEGET_T:
			for(i=0; i<req_num; i++){
				req->value=inf_get_valueset(_value[i],FS_GET_T,len[i]);
			}
			break;
		default:
			break;
	}
	return inf_get_req_common(req,fromApp,mark);
}
#ifndef USINGAPP
bool inf_make_req(const FSTYPE type, const KEYT key, char *value, int len,int mark){
#else
bool inf_make_req(const FSTYPE type, const KEYT key,char* value){
#endif
#ifdef BUSE_MEASURE
    if(type==FS_GET_T){
        MS(&infTime);
    }
#endif
	request *req=inf_get_req_instance(type,key,value,len,mark,false);

#ifdef CDF
	req->isstart=false;
	measure_init(&req->latency_checker); //make_req
	measure_start(&req->latency_checker); //make_req
#endif
	assign_req(req);
	return true;
}


bool inf_make_multi_set(const FSTYPE type, KEYT *keys, char **values, int *lengths, int req_num, int mark){
	return 0;
}

bool inf_make_req_special(const FSTYPE type, const KEYT key, char* value, int len,uint32_t seq, void*(*special)(void*)){
	if(type==FS_RMW_T){
		printf("here!\n");
	}
	request *req=inf_get_req_instance(type,key,value,len,0,false);
	req->special_func=special;
	/*
	   static int cnt=0;
	   if(flying->now==1){
	   printf("[%d]will be sleep! type:%d\n",cnt++,type);
	   }*/


	//set sequential
	req->seq=seq;
#ifdef CDF
	req->isstart=false;
	measure_init(&req->latency_checker); //make_req_spe
	measure_start(&req->latency_checker); //make_req_spe
#endif

	assign_req(req);
	return true;
}

//int range_getcnt=0;
//static int end_req_num=0;
bool inf_end_req( request * const req){
#ifdef BUSE_MEASURE
    if(req->type==FS_BUSE_R)
        MS(&infendTime);
#endif
	if(req->type==FS_RMW_T){
		req->type=FS_SET_T;
		value_set *original=req->value;
		memcpy(&original->value[original->offset],original->rmw_value,original->len);
		value_set *temp=inf_get_valueset(req->value->value,FS_SET_T,req->value->length);

		free(original->value);
		req->value=temp;

		tag_manager_free_tag(tm, req->tag_num);

		assign_req(req);
		return 1;
	}

	if(req->isstart){
		bench_reap_data(req,mp.li);
	}else{
		bench_reap_nostart(req);
	}

	void *(*special)(void*);
	special=req->special_func;
	void **params;
	uint8_t *type;
	uint32_t *seq;
	if(special){
		params=(void**)malloc(sizeof(void*)*2);
		type=(uint8_t*)malloc(sizeof(uint8_t));
		seq=(uint32_t*)malloc(sizeof(uint32_t));
		*type=req->type;
		*seq=req->seq;
		params[0]=(void*)type;
		params[1]=(void*)seq;
		special((void*)params);
	}

	/*for range query*/
	if(req->added_end_req){
		req->added_end_req(req);
	}
	
	switch(req->type){
		case FS_ITER_NXT_T:
			inf_free_valueset(req->value,FS_MALLOC_R);
			break;
		case FS_RANGEGET_T:
		case FS_ITER_NXT_VALUE_T:
			printf("need to implement logic!\n");
			abort();
#ifdef KVSSD
			free(req->key.key);
#endif
			break;

		case FS_GET_T:
		case FS_NOTFOUND_T:
#ifdef KVSSD
			free(req->key.key);
#endif
			if(req->value) inf_free_valueset(req->value,FS_MALLOC_R);
			break;
		case FS_SET_T:
			
			if(req->value) inf_free_valueset(req->value,FS_MALLOC_W);
			break;
	}

	tag_manager_free_tag(tm, req->tag_num);
	free(req);

	return true;
}

extern std::multimap<uint32_t, request *> *stop_req_list;
extern std::map<uint32_t, request *> *stop_req_log_list;

void inf_free(){
	if(stop_req_list){
		delete stop_req_list;
		delete stop_req_log_list;
	}

	//bench_print();
	bench_free();
	mp.li->stop();
	mp.stopflag=true;

	 fprintf(stdout,"Total Read Traffic : %lu\n", 
			 mp.li->req_type_cnt[1]+mp.li->req_type_cnt[3]+mp.li->req_type_cnt[5]+mp.li->req_type_cnt[7]);
	 fprintf(stdout,"Total Write Traffic: %lu\n\n", 
			 mp.li->req_type_cnt[2]+mp.li->req_type_cnt[4]+mp.li->req_type_cnt[6]+mp.li->req_type_cnt[8]);
	 fprintf(stdout,"Total WAF: %.2f\n\n", 
			 (float)(mp.li->req_type_cnt[2]+mp.li->req_type_cnt[4]+mp.li->req_type_cnt[6]+mp.li->req_type_cnt[8]) / mp.li->req_type_cnt[6]);
	//printf("result of ms:\n");
	//printf("---\n");
	for(int i=0; i<1; i++){
		processor *t=&mp.processors[i];

		q_free(t->req_q);
#ifdef interface_pq
		q_free(t->req_rq);
#endif

		pthread_mutex_destroy(&t->flag);
	}
	free(mp.processors);

#ifdef BUSE_MEASURE
    printf("infTime : ");
    measure_adding_print(&infTime);
    printf("infendTime : ");
    measure_adding_print(&infendTime);
#endif
	mp.algo->destroy(mp.li,mp.algo);
	mp.li->destroy(mp.li);
	if(mp._data_check_flag){
		__checking_data_free();
	}
}

void inf_print_debug(){

}

bool inf_make_multi_req(char type, KEYT key,KEYT *keys,uint32_t iter_id,char **values,uint32_t lengths,bool (*added_end)(struct request *const)){
	request *req=inf_get_req_instance(type,key,NULL,PAGESIZE,0,false);

	switch(type){
		case FS_ITER_CRT_T:
		case FS_ITER_NXT_T:
		case FS_ITER_NXT_VALUE_T:
		case FS_ITER_RLS_T:
			printf("need to implementation!\n");
			abort();
			break;
		default:
			printf("error in inf_make_multi_req\n");
			return false;
	}
	req->added_end_req=added_end;
	req->isstart=false;
	measure_init(&req->latency_checker); //make_multi_req
	measure_start(&req->latency_checker); //make_multi_req
	assign_req(req);
	return true;
}
bool inf_iter_create(KEYT start,bool (*added_end)(struct request *const)){
	return inf_make_multi_req(FS_ITER_CRT_T,start,NULL,0,NULL,PAGESIZE,added_end);
}
bool inf_iter_next(uint32_t iter_id,char **values,bool (*added_end)(struct request *const),bool withvalue){
#ifdef KVSSD
	static KEYT null_key={0,};
#else
	static KEYT null_key=0;
#endif
	if(withvalue){
		return inf_make_multi_req(FS_ITER_NXT_VALUE_T,null_key,NULL,iter_id,values,0,added_end);
	}
	else{
		return inf_make_multi_req(FS_ITER_NXT_T,null_key,NULL,iter_id,values,0,added_end);
	}
}
bool inf_iter_release(uint32_t iter_id, bool (*added_end)(struct request *const)){
#ifdef KVSSD
	static KEYT null_key={0,};
#else
	static KEYT null_key=0;
#endif
	return inf_make_multi_req(FS_ITER_RLS_T,null_key,NULL,iter_id,NULL,0,added_end);
}

#ifdef KVSSD
bool inf_make_req_apps(char type, char *keys, uint8_t key_len,char *value,int len, int seq,void *_req,void (*end_req)(uint32_t,uint32_t,void*)){
	KEYT t_key;
	t_key.key=keys;
	t_key.len=key_len/16*16+((key_len%16?16:0))-sizeof(uint32_t);
	request *req=inf_get_req_instance(type,t_key,value,len,0,false);
	req->seq=seq;
	

#ifdef CDF
	req->isstart=false;
	measure_init(&req->latency_checker);//make_req_apps
	measure_start(&req->latency_checker);//make_req_apps
#endif
	assign_req(req);
	return true;
}
bool inf_make_range_query_apps(char type, char *keys, uint8_t key_len,int seq, int length,void *_req,void (*end_req)(uint32_t,uint32_t, void*)){
	KEYT t_key;
	t_key.key=keys;
	t_key.len=key_len;
	request *req=inf_get_req_instance(type,t_key,NULL,length,0,false);
	req->seq=seq;
	

//	printf("seq:%d\n",req->seq);
#ifdef CDF
	req->isstart=false;
	measure_init(&req->latency_checker);//make_range
	measure_start(&req->latency_checker);//make_range
#endif
	assign_req(req);
	return true;
}

bool inf_make_mreq_apps(char type, char **keys, uint8_t *key_len, char **values,int num, int seq, void *_req,void (*end_req)(uint32_t,uint32_t, void*)){
	printf("it needs to implementation!\n");
	abort();
	/*
#ifdef KVSSD
	static KEYT null_key={0,};
#else
	static KEYT null_key=0;
#endif

	request *req=inf_get_req_instance(type,null_key,NULL,PAGESIZE,0,false);
	req->multi_key=(KEYT*)malloc(sizeof(KEYT)*num);
	req->multi_value=(value_set**)malloc(sizeof(value_set*)*num);
	for(int i=0; i<num; i++){
		req->multi_key[i].key=keys[i];
		req->multi_key[i].len=key_len[i];
		req->multi_value[i]=inf_get_valueset(values[i],FS_MALLOC_R,PAGESIZE);
	}
	req->num=num;
	req->seq=seq;
#ifdef CDF
	req->isstart=false;
	measure_init(&req->latency_checker);//make_mreq
	measure_start(&req->latency_checker);//make_mreq
#endif
	assign_req(req);*/
	return true;
}

bool inf_iter_req_apps(char type, char *prefix, uint8_t key_len,char **value, int seq,void *_req, void (*end_req)(uint32_t,uint32_t, void *)){
#ifdef KVSSD
	static KEYT null_key={0,};
#else
	static KEYT null_key=0;
#endif
	request *req=inf_get_req_instance(type,null_key,NULL,0,0,false);
	switch(type){
		case FS_ITER_ALL_VALUE_T:
		case FS_ITER_ALL_T:
			req->key.key=prefix;
			req->key.len=key_len;
			req->app_result=value;
			break;
		case FS_ITER_RLS_T:
			break;
		case FS_ITER_CRT_T:
		case FS_ITER_NXT_T:
		case FS_ITER_NXT_VALUE_T:
			printf("not implemented");
			abort();
			break;
	}
	req->seq=seq;
#ifdef CDF
	req->isstart=false;
	measure_init(&req->latency_checker); //make_iter
	measure_start(&req->latency_checker);//make_iter
#endif
	assign_req(req);
	return true;
}
#endif

value_set *inf_get_valueset(PTR in_v, int type, uint32_t length){
	value_set *res=(value_set*)malloc(sizeof(value_set));
	//check dma alloc type
    if(type==FS_BUSE_R || type==FS_BUSE_W){
        res->dmatag=0;
        res->length=length;
        if(length==PAGESIZE)
            res->value=in_v;
        else{
            res->value=(char*)malloc(PAGESIZE);
        }
        return res;
    }
#ifdef DVALUE
	length=(length/PIECE+(length%PIECE?1:0))*PIECE;
#endif
	if(length==PAGESIZE)
		res->dmatag=F_malloc((void**)&(res->value),PAGESIZE,type);
	else if(length>PAGESIZE){
		abort();
	}
	else{
		res->dmatag=-1;
		res->value=(PTR)malloc(length);
	}
	res->length=length;

	res->from_app=false;
	if(in_v){
		memcpy(res->value,in_v,length);
	}
	else{
		memset(res->value,0,length);
	}
	res->ppa=UINT32_MAX;
	return res;
}

void inf_free_valueset(value_set *in, int type){
    if(type==FS_BUSE_R || type==FS_BUSE_W){
        if(in->length!=PAGESIZE)
            free(in->value);
        free(in);
        return;
    }
	if(!in->from_app){
		if(in->dmatag==-1){
			free(in->value);
		}
		else{
			F_free((void*)in->value,in->dmatag,type);
		}
	}
	free(in);
}


bool inf_wait_background(){
	/*
	if(mp.algo->wait_bg_jobs){
		mp.algo->wait_bg_jobs();
		return true;
	}*/
	return false;
}

void inf_lower_log_print(){
    for(int i=0; i<LREQ_TYPE_NUM;i++){
        //fprintf(stderr,"%s %lu\n",bench_lower_type(i),mp.li->req_type_cnt[i]);
    	printf("%s %lu\n",bench_lower_type(i),mp.li->req_type_cnt[i]);
    }
}

