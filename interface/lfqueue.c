#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include "lfqueue.c"
void q_init(struct queue** q, int size){
	(*q)=(queue*)malloc(sizeof(queue));
	(*q)->head=(node*)malloc(sizeof(node));
	(*q)->tail=(*q)->head;
	(*q)->head->n=NULL;
	(*q)->head->next=NULL;

	(*q)->size=0;
	(*q)->m_size=size;
}

bool q_enqueue(void *val,struct queue* q){
	struct node *n;
	struct node *_node=(node*)malloc(sizeof(node));
	_node->n=val;
	_node->next=NULL;
	
	while (1) {
		n = q->tail;
		if(__sync_bool_compare_and_swap((&q->size),q->m_size,q->m_size)){
			free(_node);
			return false;
		}
		if (__sync_bool_compare_and_swap(&(n->next), NULL, _node)) {
			break;
		} else {
			__sync_bool_compare_and_swap(&(q->tail), n, n->next);
		}
	}
	__sync_bool_compare_and_swap(&(q->tail), n, _node);
	__sync_fetch_and_add(&q->size,1);
	return true;
}

void *q_dequeue(struct queue *q){
	struct node* n;
	void *val;
	while(1){
		n=q->head;
		if(n->next==NULL){
			return NULL;
		}
		if(__sync_bool_compare_and_swap(&(q->head),n,n->next)){
			break;
		}
	}

	val=(void*)n->next->n;
	free(n);
	__sync_fetch_and_sub(&q->size,1);
	return val;
}

void q_free(struct queue *q){
	while(q_dequeue(q)){}
	free(q);
}
