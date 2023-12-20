#ifndef __H_QUEUE_Q_
#define __H_QUEUE_Q_
#include "../include/settings.h"
#include <pthread.h>
#define for_each_intqueue(q,a)\
	for(a=q_dequeue_int(q);\
			a!=-1;\
			a=q_dequeue_int(q))

#define for_each_rqueue(q,a)\
	for(a=q_dequeue(q);\
			a!=NULL;\
			a=q_dequeue(q))

#define for_each_rqueue_type(q,a,type)\
	for(a=(type)q_dequeue(q);\
			a!=NULL;\
			a=(type)q_dequeue(q))

typedef struct node{
	union{
		void *req;
		int data;
	}d;
	struct node *prev;
	struct node *next;
}node;

typedef struct queue{
	volatile int size;
	int m_size;
	pthread_mutex_t q_lock;
	bool firstFlag;
	node *head;
	node *tail;
}queue;

void q_init(queue**,int);
bool q_enqueue(void *,queue*);
bool q_enqueue_front(void *,queue*);
void* q_pick(queue*);
node* q_dequeue_node(queue*);
node* q_enqueue_int_node(int req, queue* q);
int q_pick_int(queue* q);

void q_lock(queue*);
void q_unlock(queue*);
void *q_dequeue(queue*);
void q_free(queue*);

node* q_enqueue_node(void *, queue*);
void *q_delete(queue *q, node* n);

bool q_enqueue_int(int, queue*);
int q_dequeue_int(queue*);
int q_dequeue_int_lba(queue*);
#endif
