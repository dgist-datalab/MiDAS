#ifndef __DFTL_LRU_LIST__
#define __DFTL_LRU_LIST__

#include <stdio.h>
#include <stdlib.h>
#include "./libart/src/art.h"

typedef struct lru_node{
	void *data;
	struct lru_node *next;
	struct lru_node *prev;
} lru_node;

typedef struct __lru{
	int size;
	lru_node *head;
	lru_node *tail;
	void (*free_data)(void *);
	uint32_t (*retrieve_key)(void*);
	art_tree map;
} LRU;

//lru
void lru_init(LRU**, void(*)(void*), uint32_t(*)(void*));
void lru_free(LRU*);
lru_node* lru_push(LRU*, void*);
lru_node* lru_push_last(LRU *, void *);
void* lru_find(LRU *, uint32_t key);
void* lru_pop(LRU*);
void lru_update(LRU*, lru_node*);
void lru_delete(LRU*, lru_node*);

#define for_each_lru_backword(lru, lru_node)\
	for(lru_node=lru->tail; lru_node!=NULL; lru_node=lru_node->prev)

#define for_each_lru_backword_safe(lru, lru_node, lru_node_prev)\
	for(lru_node=lru->tail, lru_node_prev=lru->tail->prev; lru_node!=NULL; lru_node=lru_node_prev, lru_node_prev=lru_node_prev?lru_node_prev->prev:NULL)

#define for_each_lru_list(lru, lru_node)\
	for(lru_node=lru->head; lru_node!=NULL; lru_node=lru_node->next)

#endif
