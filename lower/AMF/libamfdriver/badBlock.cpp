#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>

#include "AmfManager.h"
#include "time.h"

#define STARTPAGE (1*PAGES_PER_SEGMENT)
#define TESTNUM (TOTAL_PAGES-PAGES_PER_SEGMENT*100)/512

int error_cnt;
#ifdef WRITESYNC
sem_t global_lock;
#endif

struct test_struct{
	char buf[8192];
	uint32_t lpa;
};

test_struct *get_test_struct(uint32_t lpa){
	test_struct *res=(test_struct*)malloc(sizeof(test_struct));
	res->lpa=lpa;
	return res;
}

void set_buf(char *buf, uint32_t lpa){
	srand(lpa);
	for(uint32_t i=0; i<8192; i++){
		//buf[i]=rand()%UINT8_MAX;
		buf[i]=(i+lpa)%UINT8_MAX;
		/*
		if(lpa==(uint32_t)STARTPAGE + 10 && i<10){
			printf("buf[%u]:%u\n", i, (char)buf[i]);
		}*/
	}
}

bool check_buf(char *buf, uint32_t lpa){
	srand(lpa);
	bool ret = true;
	for(uint32_t i=0; i<8192; i++){
		//char a=rand()%UINT8_MAX;
		char a=(i+lpa)%UINT8_MAX;
		/*
		if(lpa==(uint32_t)STARTPAGE + 10 && i<10){
			printf("buf[%u]:%u %u\n", i, (char)buf[i], (char)a);
		}*/
		if(buf[i]!=a) {
			printf("Error detected lpa %d, %d-th Byte\n", lpa, i);
			ret = false;
		}
	}
	return ret;
}

void print_buf(char *buf){
	for(uint32_t i=1; i<=8192/4; i++){
		uint32_t a=*(uint32_t*)&buf[(i-1)*4];
		printf("%u ",a);
		if(i%16==0) printf("\n");
	}
}

double timespec_diff_sec( timespec start, timespec end ) {
	double t = end.tv_sec - start.tv_sec;
	t += ((double)(end.tv_nsec - start.tv_nsec)/1000000000L);
	return t;
}

void readCb(void *req) {
	test_struct *ts=(test_struct*)req;
	if(!check_buf(ts->buf,ts->lpa)){
		error_cnt++;
	}
	free(ts);
#ifdef WRITESYNC
	sem_post(&global_lock);
#endif
	// do nothing
}

void writeCb(void *req) {
#ifdef WRITESYNC
	//sem_post(&global_lock);
#endif
}

void eraseCb(void *req) {
	// do nothing
}

void readErrorCb(void *req) {
	test_struct *ts=(test_struct*)req;
	unsigned lpa = ts->lpa;
	fprintf(stderr, "lpa %u aftl read translation error\n", lpa);
	free(ts);
}

void writeErrorCb(void *req) {
	unsigned lpa = (uintptr_t)req;
	fprintf(stderr, "lpa %u aftl write translation error\n", lpa);
}

void eraseErrorCb(void *req) {
	// do nothing
}

int main(int argc, char *arv[]) {
	printf("Start page: %u, Test num:%u\n",STARTPAGE, TESTNUM);
	AmfManager *am;
	if(argc==1){
		printf("delete all blocks!!!!\n");
		am = AmfOpen(2); // Erase only mapped blocks (written blocks) so that device is clean state
	}
	else{
		printf("delete written blocks!!!!\n");
		am = AmfOpen(1);
	}


	SetReadCb(am, readCb, readErrorCb); // you can register NULL as a callback (ignored)
	SetWriteCb(am, writeCb, writeErrorCb);
	SetEraseCb(am, eraseCb, eraseErrorCb);

	char buf[8192];

	timespec start, now;
	int elapsed;
	
#ifdef FASTREAD
	printf("The read will be issued [right after] issuing a write request\n");
#else
	printf("The read will be issued after issuing  [all write] requests\n");
#endif


	clock_gettime(CLOCK_REALTIME, &start);
	uint32_t start_page=STARTPAGE;
	uint32_t test_num=TESTNUM;

#ifdef WRITESYNC
	sem_init(&global_lock, 0, 0);
#endif

	for (unsigned int i=0; i< test_num; i++) {
		uint32_t lba=(i+start_page) % test_num;
		set_buf(buf,lba);
		AmfWrite(am, lba, buf, (void*)(uintptr_t)lba);


#ifdef FASTREAD
	#ifdef WRITESYNC
		//sem_wait(&global_lock);
	#endif
		test_struct *my_req = get_test_struct(lba);
		AmfRead(am, lba, my_req->buf, (void*)my_req);

	#ifdef WRITESYNC
		sem_wait(&global_lock);
	#endif
#endif
	}
	clock_gettime(CLOCK_REALTIME, &now);

	fprintf(stderr, "WRITE SPEED: %f MB/s\n", ((1024*1024*4)/1000)/timespec_diff_sec(start,now));

	elapsed = 10000;
	while (true) {
		usleep(100);
		if (elapsed == 0) {
			elapsed = 10000;
		} else {
			elapsed--;
		}
		if (!IsAmfBusy(am)) break;
	}

#ifdef REOPEN
	AmfClose(am); // close device and dump "aftl.bin"

	am = AmfOpen(0); // mode = 0; Open the device as it is; AFTL must be programmed or aftl.bin must be provided

	SetReadCb(am, readCb, readErrorCb);
	SetWriteCb(am, writeCb, writeErrorCb);
	SetEraseCb(am, eraseCb, eraseErrorCb);
#endif

#ifndef FASTREAD
	clock_gettime(CLOCK_REALTIME, &start);
	for (unsigned int i=0; i< test_num; i++) {
		uint32_t lba=(i+start_page) % test_num;
		test_struct *my_req = get_test_struct(lba);
		AmfRead(am, lba, my_req->buf, (void*)my_req);
	}
	clock_gettime(CLOCK_REALTIME, &now);
#endif

	elapsed = 10000;
	while (true) {
		usleep(100);
		if (elapsed == 0) {
			elapsed = 10000;
		} else {
			elapsed--;
		}
		if (!IsAmfBusy(am)) break;
	}

	fprintf(stderr, "READ SPEED: %f MB/s\n", ((1024*1024*4)/1000)/timespec_diff_sec(start,now));
	printf("error cnt:%u\n",error_cnt);

	AmfClose(am); // close device and dump "aftl.bin"
}
