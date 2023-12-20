#define _LARGEFILE64_SOURCE
#include "../../include/settings.h"
#include "../../bench/bench.h"
#include "../../bench/measurement.h"
#include "../../algorithm/Lsmtree/lsmtree.h"
#include "../../include/utils/cond_lock.h"
#include "../../include/utils/thpool.h"
#include "no_dev.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include<semaphore.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <semaphore.h>

lower_info no_info={
	.create=no_create,
	.destroy=no_destroy,
	.write=no_push_data,
	.read=no_pull_data,
	.device_badblock_checker=NULL,
	.trim_block=no_trim_block,
	.refresh=no_refresh,
	.stop=no_stop,
	.lower_alloc=NULL,
	.lower_free=NULL,
	.lower_flying_req_wait=no_flying_req_wait
};

threadpool thpool;

static int _fd;
static pthread_mutex_t fd_lock,flying_lock;
static pthread_t t_id;
static io_context_t ctx;
cl_lock *lower_flying;
bool flying_flag;
//static int write_cnt, read_cnt;
sem_t sem;
bool wait_flag;
bool stopflag;
uint64_t lower_micro_latency;
MeasureTime total_time;
long int a, b, sum1, sum2, max1, max2;

static uint8_t test_type(uint8_t type){
	uint8_t t_type=0xff>>1;
	return type&t_type;
}

uint32_t no_create(lower_info *li,blockmanager *bm){
	int ret;
	sem_init(&sem,0,0);
	li->NOB=_NOS;
	li->NOP=_NOP;
	li->SOB=BLOCKSIZE * BPS;
	li->SOP=PAGESIZE;
	li->SOK=sizeof(uint32_t);
	li->PPB=_PPB;
	li->PPS=_PPS;
	li->TS=TOTALSIZE;
	li->DEV_SIZE=DEVSIZE;
	li->all_pages_in_dev=DEVSIZE/PAGESIZE;

	li->write_op=li->read_op=li->trim_op=0;
	//_fd=open(LOWER_FILE_NAME,O_RDWR|O_DIRECT,0644);

	lower_flying=cl_init(128,false);

	sem_init(&sem,0,0);
    stopflag = false;

	return 1;
}

void *no_refresh(lower_info *li){
	measure_init(&li->writeTime);
	measure_init(&li->readTime);
	li->write_op=li->read_op=li->trim_op=0;
	return NULL;
}
void *no_destroy(lower_info *li){
	for(int i=0; i<LREQ_TYPE_NUM;i++){
		fprintf(stderr,"%s %lu\n",bench_lower_type(i),li->req_type_cnt[i]);
	}
	close(_fd);

	return NULL;
}
uint64_t offset_hooker(uint64_t origin_offset, uint8_t req_type){
	uint64_t res=origin_offset;
	switch(req_type){
		case TRIM:
			break;
		case MAPPINGR:
			break;
		case MAPPINGW:
			break;
		case GCMR:
			break;
		case GCMW:
			break;
		case DATAR:
			break;
		case DATAW:
			break;
		case GCDR:
			break;
		case GCDW:
			break;
	}
	return res%(no_info.DEV_SIZE);
}
void *no_push_data(uint32_t PPA, uint32_t size, value_set* value, bool async,algo_req *const req){
	req->ppa = PPA;
	if(value->dmatag==-1){
		printf("dmatag -1 error!\n");
		exit(1);
	}
	uint8_t t_type=test_type(req->type);
	if(t_type < LREQ_TYPE_NUM){
		no_info.req_type_cnt[t_type]++;
	}
	
	if(size !=PAGESIZE){
		abort();
	}
	return NULL;
}

void *no_pull_data(uint32_t PPA, uint32_t size, value_set* value, bool async,algo_req *const req){	
	req->ppa = PPA;
	if(value->dmatag==-1){
		printf("dmatag -1 error!\n");
		exit(1);
	}

	uint8_t t_type=test_type(req->type);
	if(t_type < LREQ_TYPE_NUM){
		no_info.req_type_cnt[t_type]++;
	}
	
	return NULL;
}

void *no_trim_block(uint32_t PPA, bool async){
	no_info.req_type_cnt[TRIM]++;
	uint64_t range[2];
	//range[0]=PPA*no_info.SOP;
	range[0]=offset_hooker((uint64_t)PPA*no_info.SOP,TRIM);
	range[1]=_PPB*no_info.SOP;
	fprintf(stderr,"T %u\n",PPA);
	ioctl(_fd,BLKDISCARD,&range);
	return NULL;
}

void no_stop(){}

void no_flying_req_wait(){
	while (lower_flying->now != 0) {}
	return ;
}
