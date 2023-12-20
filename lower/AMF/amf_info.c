#include "./libamfdriver/AmfManager.h"
#include "amf_info.h"
#include "../../include/settings.h"
#include "../../bench/bench.h"
#include <pthread.h>
#include <unistd.h>
#include <queue>
AmfManager *am;
/*testtest*/
#ifdef LOWER_MEM_DEV
char **mem_pool;
char *temp_mem_buf;
#endif

void amf_call_back_r(void *req);
void amf_call_back_w(void *req);
void amf_call_back_e(void *req);
void amf_error_call_back_r(void *req);
void amf_error_call_back_w(void *req);
void amf_error_call_back_e(void *req);

typedef struct dummy_req{
	uint32_t test_ppa;
	uint8_t type;
}dummy_req;

typedef struct amf_wrapper{
	uint32_t cnt;
	algo_req *req;
}amf_wrapper;

amf_wrapper *wrapper_array;
std::queue<amf_wrapper*>* wrap_q;

lower_info amf_info={
	.create=amf_info_create,
	.destroy=amf_info_destroy,
	.write=amf_info_write,
	.read=amf_info_read,
	.device_badblock_checker=NULL,
	.trim_block=amf_info_trim_block,
	.trim_a_block=NULL,
	.refresh=amf_info_refresh,
	.stop=amf_info_stop,
	.lower_alloc=NULL,
	.lower_free=NULL,
	.lower_flying_req_wait=amf_flying_req_wait,
	.lower_show_info=amf_info_show_info,

	.lower_tag_num=amf_info_lower_tag_num,
};

static uint8_t test_type(uint8_t type){
	uint8_t t_type=0xff>>1;
	return type&t_type;
}

pthread_cond_t wrapper_cond=PTHREAD_COND_INITIALIZER;
pthread_mutex_t wrapper_lock=PTHREAD_MUTEX_INITIALIZER;

static inline amf_wrapper* get_amf_wrapper(){
	amf_wrapper *res;
	pthread_mutex_lock(&wrapper_lock);
	while(wrap_q->empty()){
		pthread_cond_wait(&wrapper_cond, &wrapper_lock);
	}
	res=wrap_q->front();
	res->cnt=0;
	wrap_q->pop();
	pthread_mutex_unlock(&wrapper_lock);
	return res;
}

static inline void release_amf_wrapper(amf_wrapper *amw){
	pthread_mutex_lock(&wrapper_lock);
	wrap_q->push(amw);
	pthread_cond_broadcast(&wrapper_cond);
	pthread_mutex_unlock(&wrapper_lock);
}

uint32_t amf_info_create(lower_info *li, blockmanager *bm){
	if (access("aftl.bin", F_OK)!=-1){
		am=AmfOpen(1);
	}
	else{
		am=AmfOpen(2);
	}


	SetReadCb(am, amf_call_back_r, amf_error_call_back_r);
	SetWriteCb(am, amf_call_back_w, amf_error_call_back_w);
	SetEraseCb(am, amf_call_back_e, amf_error_call_back_e);

#ifdef LOWER_MEM_DEV
	printf("lower mem dev  mode\n");
	mem_pool=(char**)malloc(sizeof(char*)*_NOP);
	for(uint32_t i=0; i<_NOP; i++){
		mem_pool[i]=(char*)malloc(PAGESIZE);
	}

	temp_mem_buf=(char*)malloc(PAGESIZE);
#endif

	wrapper_array=(amf_wrapper*)malloc(sizeof(amf_wrapper)*QDEPTH);
	wrap_q=new std::queue<amf_wrapper*>();
	for(uint32_t i=0; i<QDEPTH; i++){
		wrap_q->push(&wrapper_array[i]);
	}

	return 1;
}

void* amf_info_destroy(lower_info *li){
	amf_flying_req_wait();

	for(int i=0; i<LREQ_TYPE_NUM;i++){
		fprintf(stdout,"%s %lu\n",bench_lower_type(i),li->req_type_cnt[i]);
	}

    fprintf(stdout,"Total Read Traffic : %lu\n", li->req_type_cnt[1]+li->req_type_cnt[3]+li->req_type_cnt[5]+li->req_type_cnt[7]);
    fprintf(stdout,"Total Write Traffic: %lu\n\n", li->req_type_cnt[2]+li->req_type_cnt[4]+li->req_type_cnt[6]+li->req_type_cnt[8]);
    fprintf(stdout,"Total WAF: %.2f\n\n", (float)(li->req_type_cnt[2]+li->req_type_cnt[4]+li->req_type_cnt[6]+li->req_type_cnt[8]) / li->req_type_cnt[6]);

	li->write_op=li->read_op=li->trim_op=0;
	AmfClose(am);

#ifdef LOWER_MEM_DEV
	for(uint32_t i=0; i<_NOP; i++){
		free(mem_pool[i]);
	}
	free(mem_pool);
	free(temp_mem_buf);
#endif

	free(wrapper_array);
	delete wrap_q;
	return NULL;
}

void* amf_info_write(uint32_t ppa, uint32_t size, value_set *value,bool async,algo_req * const req){
	uint8_t t_type=test_type(req->type);
	if(t_type < LREQ_TYPE_NUM){
		amf_info.req_type_cnt[t_type]++;
	}

	req->test_ppa=ppa;
	req->type_lower=0;
#ifdef LOWER_MEM_DEV
	amf_wrapper *temp_req=get_amf_wrapper();
	temp_req->req=req;
	memcpy(mem_pool[ppa], value->value, PAGESIZE);
	for(uint32_t i=0; i<R2PGAP; i++){
		AmfWrite(am, ppa*R2PGAP+i, temp_mem_buf, (void *)temp_req);
	}
#else
	AmfWrite(am, ppa, value->value, (void *)req);
#endif
	return NULL;
}


void* amf_info_read(uint32_t ppa, uint32_t size, value_set *value,bool async,algo_req * const req){
	uint8_t t_type=test_type(req->type);
	if(t_type < LREQ_TYPE_NUM){
		amf_info.req_type_cnt[t_type]++;
	}

	req->test_ppa=ppa;

	req->type_lower=0;
#ifdef LOWER_MEM_DEV
	amf_wrapper *temp_req=get_amf_wrapper();
	temp_req->req=req;
	memcpy(value->value, mem_pool[ppa], PAGESIZE);
	for(uint32_t i=0; i<R2PGAP; i++){
		AmfRead(am, ppa*R2PGAP+i, temp_mem_buf, (void *)temp_req);
	}
#else
	AmfRead(am, ppa, value->value, (void *)req);
#endif
	return NULL;
}

void* amf_info_trim_block(uint32_t ppa,bool async){
	dummy_req *temp=(dummy_req*)malloc(sizeof(dummy_req));
	amf_info.req_type_cnt[TRIM]++;
	temp->type=TRIM;
	if((ppa*R2PGAP)%PAGES_PER_SEGMENT){
		printf("not aligned! %u\n");
		print_stacktrace();
		abort();
	}
	temp->test_ppa=ppa;
	for(uint32_t i=0; i< 128; i++){
		AmfErase(am,ppa*R2PGAP+i,(void*)temp);
	}
	return NULL;
}


void* amf_info_refresh(struct lower_info* li){
	li->write_op=li->read_op=li->trim_op=0;
	return NULL;
}

void amf_info_stop(){}

void amf_info_show_info(){}

uint32_t amf_info_lower_tag_num(){
	return NUM_TAGS;
}

void amf_flying_req_wait(){
	while(IsAmfBusy(am)){}
	return;
}

void amf_call_back_r(void *_req){
	amf_wrapper *wrapper=(amf_wrapper*)_req;

	wrapper->cnt++;
	if(wrapper->cnt==R2PGAP){
		algo_req *req=(algo_req*)wrapper->req;
		req->end_req(req);
		release_amf_wrapper(wrapper);
	}
}

void amf_call_back_w(void *_req){
	amf_wrapper *wrapper=(amf_wrapper*)_req;
	wrapper->cnt++;
	if(wrapper->cnt==R2PGAP){
		algo_req *req=(algo_req*)wrapper->req;
		req->end_req(req);
		release_amf_wrapper(wrapper);
	}
}

void amf_call_back_e(void *_req){
}

void amf_error_call_back_r(void *_req){
	algo_req *req=(algo_req*)_req;

	printf("error! in AMF read ppa:%u\n",req->test_ppa);

	req->end_req(req);
}
void amf_error_call_back_w(void *_req){
	algo_req *req=(algo_req*)_req;

	printf("error! in AMF write ppa:%u\n",req->test_ppa);

	req->end_req(req);
}
void amf_error_call_back_e(void *_req){
	dummy_req *req=(dummy_req*)_req;

	printf("error! in AMF erase ppa:%u\n",req->test_ppa);

	//req->end_req(req);
}
