#include <stdlib.h>
#include <stdio.h>
#include "queue.h"
#include "../include/FS.h"
#include "../include/container.h"
#include "../include/settings.h"

void q_init(queue **q,int qsize){
	*q=(queue*)malloc(sizeof(queue));
	(*q)->size=0;
	(*q)->head=(*q)->tail=NULL;
	pthread_mutex_init(&((*q)->q_lock),NULL);
	//printf("mutex_t : %p q:%p, size:%d\n",&(*q)->q_lock,*q,qsize);
	(*q)->firstFlag=true;
	(*q)->m_size=qsize;
}

bool q_enqueue(void* req, queue* q){
	pthread_mutex_lock(&q->q_lock);
	if(q->size==q->m_size){
		pthread_mutex_unlock(&q->q_lock);
		return false;
	}

	node *new_node=(node*)malloc(sizeof(node));
	new_node->d.req=req;
	new_node->next=NULL;
	if(q->size==0){
		q->head=q->tail=new_node;
	}
	else{
		q->tail->next=new_node;
		new_node->prev = q->tail;
		q->tail=new_node;
	}
	q->size++;
	pthread_mutex_unlock(&q->q_lock);
	return true;
}

node* q_enqueue_node(void* req, queue* q){
        pthread_mutex_lock(&q->q_lock);
        if(q->size==q->m_size){
                pthread_mutex_unlock(&q->q_lock);
                return NULL;
        }

        node *new_node=(node*)malloc(sizeof(node));
        new_node->d.req=req;
        new_node->next=NULL;
        if(q->size==0){
                q->head=q->tail=new_node;
        }
        else{
                q->tail->next=new_node;
		new_node->prev = q->tail;
                q->tail=new_node;
        }
        q->size++;
        pthread_mutex_unlock(&q->q_lock);
        return new_node;
}

node* q_enqueue_int_node(int req, queue* q){
        pthread_mutex_lock(&q->q_lock);
        if(q->size==q->m_size){
                pthread_mutex_unlock(&q->q_lock);
                return NULL;
        }

        node *new_node=(node*)malloc(sizeof(node));
        new_node->d.data=req;
        new_node->next=NULL;
        if(q->size==0){
                q->head=q->tail=new_node;
        }
        else{
                q->tail->next=new_node;
		new_node->prev = q->tail;
                q->tail=new_node;
        }
        q->size++;
        pthread_mutex_unlock(&q->q_lock);
        return new_node;
}

bool q_enqueue_front(void *req, queue*q){
	pthread_mutex_lock(&q->q_lock);
	if(q->size==q->m_size){	
		pthread_mutex_unlock(&q->q_lock);
		return false;
	}
	node *new_node=(node*)malloc(sizeof(node));
	new_node->d.req=req;
	new_node->next=NULL;
	if(q->size==0){
		q->head=q->tail=new_node;
	}
	else{
		new_node->next=q->head;
		q->head->prev = new_node;
		q->head=new_node;
	}
//	printf("ef-key:%u\n",((request*)req)->key);
	q->size++;
	pthread_mutex_unlock(&q->q_lock);
	return true;
}

void* q_dequeue(queue *q){
	pthread_mutex_lock(&q->q_lock);
	if(!q->head || q->size==0){
		pthread_mutex_unlock(&q->q_lock);
		return NULL;
	}
	node *target_node;
	target_node=q->head;
	q->head=q->head->next;
	q->size--;
	if (q->size != 0) q->head->prev = NULL;

	void *res=target_node->d.req;
//	printf("of-key:%u\n",((request*)res)->key);
	free(target_node);
	pthread_mutex_unlock(&q->q_lock);
	return res;
}

void* q_pick(queue *q){
	pthread_mutex_lock(&q->q_lock);
	if(!q->head || q->size==0){
		pthread_mutex_unlock(&q->q_lock);
		return NULL;
	}
	node *target_node;
	target_node=q->head;
	void *res=target_node->d.req;
	pthread_mutex_unlock(&q->q_lock);
	return res;
}

int q_pick_int(queue *q){
	pthread_mutex_lock(&q->q_lock);
	if(!q->head || q->size==0){
		pthread_mutex_unlock(&q->q_lock);
		return NULL;
	}
	node *target_node;
	target_node=q->head;
	int res=target_node->d.data;
	pthread_mutex_unlock(&q->q_lock);
	return res;
}


node* q_dequeue_node(queue* q) {
	pthread_mutex_lock(&q->q_lock);
	if(!q->head || q->size==0){
		pthread_mutex_unlock(&q->q_lock);
		return NULL;
	}
	node *target_node;
	target_node=q->head;
	q->head=q->head->next;
	q->size--;
	if (q->size != 0) q->head->prev = NULL;

//	printf("of-key:%u\n",((request*)res)->key);
	pthread_mutex_unlock(&q->q_lock);
	return target_node;
}

void q_free(queue* q){
	while(q_dequeue(q)){}
	pthread_mutex_destroy(&q->q_lock);
	free(q);
}

bool q_enqueue_int(int req, queue* q){
	pthread_mutex_lock(&q->q_lock);
	if(q->size==q->m_size){
		pthread_mutex_unlock(&q->q_lock);
		return false;
	}

	node *new_node=(node*)malloc(sizeof(node));
	new_node->d.data=req;
	new_node->next=NULL;
	if(q->size==0){
		q->head=q->tail=new_node;
	}
	else{
		q->tail->next=new_node;
		new_node->prev = q->tail;
		q->tail=new_node;
	}
	q->size++;
	pthread_mutex_unlock(&q->q_lock);
	return true;
}

int q_dequeue_int(queue* q){
	pthread_mutex_lock(&q->q_lock);
	if(!q->head || q->size==0){
		pthread_mutex_unlock(&q->q_lock);
		return 0;
	}
	node *target_node;
	target_node=q->head;
	q->head=q->head->next;
        q->head->prev=NULL;
	/*
	if (q->size == 1) {
		q->head=NULL;
		q->tail=NULL;
	} else {
		q->head=q->head->next;
		q->head->prev=NULL;
	}
	*/
	int res=target_node->d.data;
	q->size--;
//	printf("of-key:%u\n",((request*)res)->key);
	free(target_node);
	pthread_mutex_unlock(&q->q_lock);
	return res;
}

int q_dequeue_int_lba(queue* q){
        pthread_mutex_lock(&q->q_lock);
        if(!q->head || q->size==0){
                pthread_mutex_unlock(&q->q_lock);
                return -1;
        }
        node *target_node;
        target_node=q->head;
        //q->head=q->head->next;
        //q->head->prev=NULL;

        if (q->size == 1) {
                q->head=NULL;
                q->tail=NULL;
        } else {
                q->head=q->head->next;
                q->head->prev=NULL;
        }

        int res=target_node->d.data;
        q->size--;
//      printf("of-key:%u\n",((request*)res)->key);
        free(target_node);
        pthread_mutex_unlock(&q->q_lock);
        return res;
}

void *q_delete(queue *q, node* n) {
        pthread_mutex_lock(&q->q_lock);
        if (q->head == n) {
                q->head = n->next;
        } else if (q->tail == n) {
                q->tail = n->prev;
        } else {
                n->prev->next = n->next;
                n->next->prev = n->prev;
        }
        q->size--;
        void *res = n->d.req;
        free(n);
        pthread_mutex_unlock(&q->q_lock);
        return res;
}
