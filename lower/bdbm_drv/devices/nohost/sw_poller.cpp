#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "debug.h"
#include "dm_nohost.h"
#include "dev_params.h"

#include "utime.h"
#include "umemory.h"


/***/
#include "FlashIndication.h"
#include "FlashRequest.h"
#include "DmaBuffer.h"
#include "sw_poller.h"

#define POLLED_QUEUE_SIZE 1024
pthread_t tid;
std::queue<void*> *polled_queue;
sem_t polled_queue_mutex;
sem_t fill_sem;
sem_t empty_sem;
bool polled_queue_stop_flag;
extern bdbm_drv_info_t* _bdi_dm;

void* sw_poller_main(void *argv);
void sw_poller_init(){
	fprintf(stderr,"sw_poller init!\n");
	sem_init(&polled_queue_mutex,0,1);
	sem_init(&empty_sem,0,POLLED_QUEUE_SIZE);
	sem_init(&fill_sem,0,0);
	polled_queue=new std::queue<void*>();
	pthread_create(&tid,NULL,sw_poller_main,NULL);
}

void sw_poller_enqueue(void *req){
	sem_wait(&empty_sem);

	sem_wait(&polled_queue_mutex);
	polled_queue->push(req);
	sem_post(&polled_queue_mutex);

	sem_post(&fill_sem);
}

void* sw_poller_dequeue(){
	sem_wait(&fill_sem);
	void *res=NULL;
	sem_wait(&polled_queue_mutex);
	if(!polled_queue->empty()){
		res=polled_queue->front();
		polled_queue->pop();
	}
	sem_post(&polled_queue_mutex);

	sem_post(&empty_sem);
	return res;
}

void* sw_poller_main(void *argv){
	fprintf(stderr,"sw_poller init!\n");
	while(!polled_queue_stop_flag){
		void *req=sw_poller_dequeue();
		if(req==NULL) break;
		dm_nohost_end_req(_bdi_dm,(bdbm_llm_req_t*)req);
	}
	
	while(sem_trywait(&fill_sem)!=-1){
		sem_wait(&polled_queue_mutex);
		void *res=polled_queue->front();
		polled_queue->pop();
		sem_post(&polled_queue_mutex);

		dm_nohost_end_req(_bdi_dm,(bdbm_llm_req_t*)res);
	}
	return NULL;
}

void sw_poller_destroy(){
	polled_queue_stop_flag=true;
	sem_post(&fill_sem);
	pthread_join(tid,NULL);
	sem_destroy(&fill_sem);
	sem_destroy(&empty_sem);
	sem_destroy(&polled_queue_mutex);
	delete polled_queue;
}
