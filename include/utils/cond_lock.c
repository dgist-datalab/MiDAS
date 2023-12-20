#include "cond_lock.h"
#include <stdlib.h>

cl_lock *cl_init(int cnt, bool zero_lock){
	cl_lock *res=(cl_lock*)malloc(sizeof(cl_lock));
	pthread_mutex_init(&res->mutex,NULL);
	pthread_cond_init(&res->cond,NULL);
	res->zero_lock=zero_lock;
	res->cnt=cnt;
	res->now=0;
	return res;
}

void cl_grap(cl_lock *cl){

	pthread_mutex_lock(&cl->mutex);
	if(cl->zero_lock){
		while(cl->now==0){
			pthread_cond_wait(&cl->cond,&cl->mutex);
		}
		cl->now--;
	}else{
		while(cl->now==cl->cnt){
			pthread_cond_wait(&cl->cond,&cl->mutex);
		}
		cl->now++;
	}
	pthread_mutex_unlock(&cl->mutex);
}

void cl_cond_grap(cl_lock *cl, bool flag){
	pthread_mutex_lock(&cl->mutex);
	if(cl->zero_lock){
		if(flag){
			pthread_cond_wait(&cl->cond,&cl->mutex);
		}
		cl->now--;
	}else{
		if(flag){
			pthread_cond_wait(&cl->cond,&cl->mutex);
		}
		cl->now++;
	}
	pthread_mutex_unlock(&cl->mutex);
}

void cl_always_release(cl_lock *cl){
	pthread_mutex_lock(&cl->mutex);
	pthread_cond_broadcast(&cl->cond);
	pthread_mutex_unlock(&cl->mutex);
}

void cl_now_update(cl_lock* cl,int input){
	pthread_mutex_lock(&cl->mutex);
	cl->now=input;
	pthread_mutex_unlock(&cl->mutex);
}

void cl_release(cl_lock *cl){;
	pthread_mutex_lock(&cl->mutex);
	if(cl->zero_lock){
		if(cl->now==0){
			cl->now++;
			pthread_cond_broadcast(&cl->cond);
		}
		else{
			cl->now++;
		}
	}
	else{
		if(cl->now==cl->cnt){
			cl->now--;
			pthread_cond_broadcast(&cl->cond);
		}else{
			cl->now--;
		}
	}
	pthread_mutex_unlock(&cl->mutex);
}


void cl_free(cl_lock *cl){
	pthread_mutex_destroy(&cl->mutex);
	pthread_cond_destroy(&cl->cond);
	free(cl);
}

void cl_grep_with_f(cl_lock *cl, int s, int d, bool (*cmp)(int,int)){
	pthread_mutex_lock(&cl->mutex);
	if(cmp(s,d)){
		pthread_cond_wait(&cl->cond,&cl->mutex);
	}
	pthread_mutex_unlock(&cl->mutex);
}

void cl_release_with_f(cl_lock *cl, int s, int d, bool (*cmp)(int,int)){
	pthread_mutex_lock(&cl->mutex);
	if(cmp(s,d)){
		pthread_cond_broadcast(&cl->cond);
	}
	pthread_mutex_unlock(&cl->mutex);
}

