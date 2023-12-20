#define _LARGEFILE64_SOURCE
#include "posix.h"
#include "../../include/settings.h"
#include "../../bench/bench.h"
#include "../../bench/measurement.h"
#include "../../interface/queue.h"
#include "../../interface/bb_checker.h"
#include "../../include/utils/cond_lock.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
//#include <readline/readline.h>
//#include <readline/history.h>

pthread_mutex_t fd_lock;
mem_seg *seg_table;
#if (ASYNC==1)
queue *p_q;
pthread_t t_id;
bool stopflag;
#endif

cl_lock *lower_flying;

lower_info my_posix={
	.create=posix_create,
	.destroy=posix_destroy,
#if (ASYNC==1)
	.write=posix_make_push,
	.read=posix_make_pull,
#elif (ASYNC==0)
	.write=posix_push_data,
	.read=posix_pull_data,
#endif
	.device_badblock_checker=NULL,
#if (ASYNC==1)
	.trim_block=posix_make_trim,
	.trim_a_block=posix_trim_a_block,
#elif (ASYNC==0)
	.trim_block=posix_trim_block,
	.trim_a_block=posix_trim_a_block,
#endif
	.refresh=posix_refresh,
	.stop=posix_stop,
	.lower_alloc=NULL,
	.lower_free=NULL,
	.lower_flying_req_wait=posix_flying_req_wait
};
 uint32_t d_write_cnt, m_write_cnt, gcd_write_cnt, gcm_write_cnt;

#if (ASYNC==1)
void *l_main(void *__input){
	posix_request *inf_req;
	while(1){
		cl_grap(lower_flying);
		if(stopflag){
			//printf("posix bye bye!\n");
			pthread_exit(NULL);
			break;
		}
		if(!(inf_req=(posix_request*)q_dequeue(p_q))){
			continue;
		}
		switch(inf_req->type){
			case FS_LOWER_W:
				posix_push_data(inf_req->key, inf_req->size, inf_req->value, inf_req->isAsync, inf_req->upper_req);
				break;
			case FS_LOWER_R:
				posix_pull_data(inf_req->key, inf_req->size, inf_req->value, inf_req->isAsync, inf_req->upper_req);
				break;
			case FS_LOWER_T:
				posix_trim_block(inf_req->key, inf_req->isAsync);
				break;
		}
		free(inf_req);
	}
	return NULL;
}

void *posix_make_push(uint32_t PPA, uint32_t size, value_set* value, bool async, algo_req *const req){
	bool flag=false;
	posix_request *p_req=(posix_request*)malloc(sizeof(posix_request));
	p_req->type=FS_LOWER_W;
	p_req->key=PPA;
	p_req->value=value;
	p_req->upper_req=req;
	p_req->isAsync=async;
	p_req->size=size;

	while(!flag){
		if(q_enqueue((void*)p_req,p_q)){
			cl_release(lower_flying);
			flag=true;
		}

	}
	return NULL;
}

void *posix_make_pull(uint32_t PPA, uint32_t size, value_set* value, bool async, algo_req *const req){
	bool flag=false;
	posix_request *p_req=(posix_request*)malloc(sizeof(posix_request));
	p_req->type=FS_LOWER_R;
	p_req->key=PPA;
	p_req->value=value;
	p_req->upper_req=req;
	p_req->isAsync=async;
	p_req->size=size;
	req->type_lower=0;
	bool once=true;
	while(!flag){
		if(q_enqueue((void*)p_req,p_q)){
			cl_release(lower_flying);
			flag=true;
		}	
		if(!flag && once){
			req->type_lower=1;
			once=false;
		}
	}
	return NULL;
}

void *posix_make_trim(uint32_t PPA, bool async){
	bool flag=false;
	posix_request *p_req=(posix_request*)malloc(sizeof(posix_request));
	p_req->type=FS_LOWER_T;
	p_req->key=PPA;
	p_req->isAsync=async;
	
	while(!flag){
		if(q_enqueue((void*)p_req,p_q)){
			cl_release(lower_flying);
			flag=true;
		}
	}
	return NULL;
}
#endif

uint32_t posix_create(lower_info *li, blockmanager *b){
	li->NOB=_NOS;
	li->NOP=_NOP;
	li->SOB=BLOCKSIZE*BPS;
	li->SOP=PAGESIZE;
	li->SOK=sizeof(uint32_t);
	li->PPB=_PPB;
	li->PPS=_PPS;
	li->TS=TOTALSIZE;
	lower_flying=cl_init(QDEPTH*2,true);

	printf("!!! posix memory ASYNC: %d!!!\n", ASYNC);
	li->write_op=li->read_op=li->trim_op=0;
	seg_table = (mem_seg*)malloc(sizeof(mem_seg)*li->NOB);
	for(uint32_t i = 0; i < li->NOB; i++){
		seg_table[i].storage = NULL;
		seg_table[i].alloc = 0;
	}
	pthread_mutex_init(&fd_lock,NULL);
	pthread_mutex_init(&my_posix.lower_lock,NULL);
	measure_init(&li->writeTime);
	measure_init(&li->readTime);
#if (ASYNC==1)
	stopflag = false;
	q_init(&p_q, 1024);
	pthread_create(&t_id,NULL,&l_main,NULL);
#endif

	memset(li->req_type_cnt,0,sizeof(li->req_type_cnt));

	return 1;
}

void *posix_refresh(lower_info *li){
	measure_init(&li->writeTime);
	measure_init(&li->readTime);
	li->write_op=li->read_op=li->trim_op=0;
	return NULL;
}

void *posix_destroy(lower_info *li){
	for(uint32_t i = 0; i < li->NOB; i++){
		if(seg_table[i].alloc){
			free(seg_table[i].storage);
		}
	}
	free(seg_table);
	pthread_mutex_destroy(&my_posix.lower_lock);
	pthread_mutex_destroy(&fd_lock);
#if (ASYNC==1)
	stopflag = true;
	q_free(p_q);
#endif

    printf("TRIM\t%lu\n", li->req_type_cnt[0]);
    printf("TR\t%lu\n", li->req_type_cnt[1]);
    printf("TW\t%lu\n", li->req_type_cnt[2]);
    printf("TGCR\t%lu\n", li->req_type_cnt[3]);
    printf("TGCW\t%lu\n", li->req_type_cnt[4]);
    printf("DR\t%lu\n", li->req_type_cnt[5]);
    printf("DW\t%lu\n", li->req_type_cnt[6]);
    printf("DGCR\t%lu\n", li->req_type_cnt[7]);
    printf("DGCW\t%lu\n\n", li->req_type_cnt[8]);

    printf("Total Read Traffic : %lu\n", li->req_type_cnt[1]+li->req_type_cnt[3]+li->req_type_cnt[5]+li->req_type_cnt[7]);
    printf("Total Write Traffic: %lu\n\n", li->req_type_cnt[2]+li->req_type_cnt[4]+li->req_type_cnt[6]+li->req_type_cnt[8]);
    printf("Total WAF: %.2f\n\n", (float)(li->req_type_cnt[2]+li->req_type_cnt[4]+li->req_type_cnt[6]+li->req_type_cnt[8]) / li->req_type_cnt[6]);
	return NULL;
}

static uint8_t convert_type(uint8_t type) {
	return (type & (0xff));
}

void *posix_push_data(uint32_t PPA, uint32_t size, value_set* value, bool async,algo_req *const req){
	uint8_t test_type;
	if(value->dmatag==-1){
		printf("dmatag -1 error!\n");
		abort();
	}
	pthread_mutex_lock(&fd_lock);

	if(my_posix.SOP*PPA >= my_posix.TS){
		printf("\nwrite error\n");
		abort();
	}

	test_type = convert_type(req->type);
	if(test_type < LREQ_TYPE_NUM){
		my_posix.req_type_cnt[test_type]++;
	}

	if(req->type<=GCMW){
		if(!seg_table[PPA/my_posix.PPS].alloc){
			seg_table[PPA/my_posix.PPS].storage = (PTR)malloc(my_posix.SOB);
			seg_table[PPA/my_posix.PPS].alloc = 1;
		}
		PTR loc = seg_table[PPA/my_posix.PPS].storage;
		memcpy(&loc[(PPA%my_posix.PPS)*my_posix.SOP],value->value,size);
	}

	pthread_mutex_unlock(&fd_lock);
	req->end_req(req);
	return NULL;
}

void *posix_pull_data(uint32_t PPA, uint32_t size, value_set* value, bool async,algo_req *const req){
	uint8_t test_type;
	if(req->type_lower!=1 && req->type_lower!=0){
		req->type_lower=0;
	}
	if(value->dmatag==-1){
		printf("dmatag -1 error!\n");
		abort();
	}

	pthread_mutex_lock(&fd_lock);

	if(my_posix.SOP*PPA >= my_posix.TS){
		printf("\nread error\n");
		abort();
	}

	test_type = convert_type(req->type);
	if(test_type < LREQ_TYPE_NUM){
		my_posix.req_type_cnt[test_type]++;
	}


	if(req->type <=GCMW){
		PTR loc = seg_table[PPA/my_posix.PPS].storage;
		memcpy(value->value,&loc[(PPA%my_posix.PPS)*my_posix.SOP],size);
		req->type_lower=1;
	}

	pthread_mutex_unlock(&fd_lock);

	req->end_req(req);
	return NULL;
}

void *posix_trim_block(uint32_t PPA, bool async){
	char *temp=(char *)malloc(my_posix.SOB);
	memset(temp,0,my_posix.SOB);
	pthread_mutex_lock(&fd_lock);
	if(my_posix.SOP*PPA >= my_posix.TS || PPA%my_posix.PPS != 0){
		printf("\ntrim error\n");
		abort();
	}
	
	my_posix.req_type_cnt[TRIM]++;
	if(seg_table[PPA/my_posix.PPS].alloc){
		free(seg_table[PPA/my_posix.PPS].storage);
		seg_table[PPA/my_posix.PPS].storage = NULL;
		seg_table[PPA/my_posix.PPS].alloc = 0;
	}
	pthread_mutex_unlock(&fd_lock);
	free(temp);
	return NULL;
}

void posix_stop(){}

void posix_flying_req_wait(){
#if (ASYNC==1)
	while(p_q->size!=0){}
#endif
}

void* posix_trim_a_block(uint32_t PPA, bool async){
	return NULL;
}
