
#define _LARGEFILE64_SOURCE
#include "posix.h"
#include "../../include/settings.h"
#include "../../bench/bench.h"
#include "../../bench/measurement.h"
#include "../../algorithm/Lsmtree/lsmtree.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>

#ifdef BUSE_MEASURE
MeasureTime lowerTime;
MeasureTime lowerread;
MeasureTime lowerwrite;
#endif

static int _fd;
pthread_mutex_t fd_lock;
lower_info my_posix={
	.create=posix_create,
	.destroy=posix_destroy,
	.write=posix_push_data,
	.read=posix_pull_data,
	.device_badblock_checker=NULL,
	.trim_block=posix_trim_block,
	.refresh=posix_refresh,
	.stop=posix_stop,
	.lower_alloc=NULL,
	.lower_free=NULL,
	.lower_flying_req_wait=posix_flying_req_wait
};
static uint8_t test_type(uint8_t type){
	uint8_t t_type=0xff>>1;
	return type&t_type;
}

uint32_t posix_create(lower_info *li){
#ifdef BUSE_MEASURE
    measure_init(&lowerTime);
#endif
	li->NOB=_NOS;
	li->NOP=_NOP;
	li->SOB=BLOCKSIZE * BPS;
	li->SOP=PAGESIZE;
	li->SOK=sizeof(uint32_t);
	li->PPB=_PPB;
	li->PPS=_PPS;
	li->TS=TOTALSIZE;

	li->write_op=li->read_op=li->trim_op=0;
	_fd=open(LOWER_FILE_NAME,O_RDWR|O_CREAT|O_TRUNC,0666);
	if(_fd==-1){
		printf("file open errorno:%d!\n",errno);
		exit(-1);
	}
	pthread_mutex_init(&fd_lock,NULL);
	pthread_mutex_init(&my_posix.lower_lock,NULL);
	measure_init(&li->writeTime);
	measure_init(&li->readTime);
	return 1;
}

void *posix_refresh(lower_info *li){
	measure_init(&li->writeTime);
	measure_init(&li->readTime);
	li->write_op=li->read_op=li->trim_op=0;
	return NULL;
}

void *posix_destroy(lower_info *li){
	pthread_mutex_destroy(&my_posix.lower_lock);
	pthread_mutex_destroy(&fd_lock);
	close(_fd);
#ifdef BUSE_MEASURE
    printf("lowerTime : ");
    measure_adding_print(&lowerTime);
    printf("lowerread : ");
    measure_adding_print(&lowerread);
    printf("lowerwrite : ");
    measure_adding_print(&lowerwrite);
#endif
	for(int i=0; i<LREQ_TYPE_NUM;i++){
		fprintf(stderr,"%s %lu\n",bench_lower_type(i),li->req_type_cnt[i]);
	}
	return NULL;
}

uint32_t latest_write_ppa;
void *posix_push_data(uint32_t PPA, uint32_t size, value_set* value, bool async,algo_req *const req){
	/*
	if(PPA>6500)
		printf("PPA : %u\n", PPA);
	*/
	uint8_t t_type=test_type(req->type);
	if(t_type==DATAW){
		latest_write_ppa=PPA;
	}
	if(t_type < LREQ_TYPE_NUM){
		my_posix.req_type_cnt[t_type]++;
	}

	if(value->dmatag==-1){
		printf("dmatag -1 error!\n");
		abort();
	}

	pthread_mutex_lock(&fd_lock);

	//if(((lsm_params*)req->params)->lsm_type!=5){
	if(lseek64(_fd,((off64_t)my_posix.SOP)*PPA,SEEK_SET)==-1){
		printf("lseek error in write\n");
	}//
#ifdef BUSE_MEASURE
    MS(&lowerwrite);
#endif
	if(!write(_fd,value->value,size)){
		printf("write none!\n");
	}
    //syncfs(_fd);
    fsync(_fd);
#ifdef BUSE_MEASURE
    MA(&lowerwrite);
#endif
//	}
	pthread_mutex_unlock(&fd_lock);
	req->end_req(req);
/*
	if(async){
		req->end_req(req);
	}else{
	
	}
	*/
	return NULL;
}

void *posix_pull_data(uint32_t PPA, uint32_t size, value_set* value, bool async,algo_req *const req){	
	/*
	if(PPA>6500)
		printf("PPA : %u\n", PPA);
	*/
    //FIXME: find which code degrade posix read
#ifdef BUSE_MEASURE
    MS(&lowerTime);
#endif
	uint8_t t_type=test_type(req->type);
	if(t_type < LREQ_TYPE_NUM){
		my_posix.req_type_cnt[t_type]++;
	}
	if(value->dmatag==-1){
		printf("dmatag -1 error!\n");
		abort();
	}

	pthread_mutex_lock(&fd_lock);
	//if(((lsm_params*)req->params)->lsm_type!=4){
	if(lseek64(_fd,((off64_t)my_posix.SOP)*PPA,SEEK_SET)==-1){
		printf("lseek error in read:%d\n",PPA);
		abort();
	}
	int res;
#ifdef BUSE_MEASURE
    MS(&lowerread);
#endif
	if(!(res=read(_fd,value->value,size))){
		printf("%d:read none!\n",res);
		abort();
	}
#ifdef BUSE_MEASURE
    MA(&lowerread);
#endif
	//}
	pthread_mutex_unlock(&fd_lock);

#ifdef BUSE_MEASURE
    MA(&lowerTime);
#endif
	pthread_mutex_unlock(&fd_lock);

	req->end_req(req);
	/*
	if(async){
		req->end_req(req);
	}
	else{
	
	}*/
	return NULL;
}

void *posix_trim_block(uint32_t PPA, bool async){
	char *temp=(char *)malloc(my_posix.SOB);
	memset(temp,0,my_posix.SOB);
	pthread_mutex_lock(&fd_lock);
	if(lseek64(_fd,((off64_t)my_posix.SOP)*PPA,SEEK_SET)==-1){
		printf("lseek error in trim\n");
	}
	if(!write(_fd,temp,BLOCKSIZE*BPS)){
		printf("write none\n");
	}
	pthread_mutex_unlock(&fd_lock);
	free(temp);
	return NULL;
}

void posix_stop(){}

void posix_flying_req_wait(){
	return ;
}
