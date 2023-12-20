#ifndef __HEAP_HEADER
#define __HEAP_HEADER
#include "../settings.h"
#include "../../interface/queue.h"
#define MHPARENTPTR(mh,idx) &((mh->body)[idx/2])
#define MHL_CHIPTR(mh,idx) &((mh->body)[idx*2])
#define MHR_CHIPTR(mh,idx) &((mh->body)[idx*2+1])

typedef struct heap_node{
	//uint64_t cnt;
	double cnt;
	void *data;
	node *qnode;
}hn;

/*
typedef struct max_heap{
	hn* body;
	int size;
	int max;
	queue* q;
	void (*swap_hptr)(void * a, void *b);
	void (*assign_hptr)(void *a, void* mh);
	int (*get_cnt)(void *a);
}mh;
*/

typedef struct max_heap{
	hn* body;
	int size;
	int max;
	queue* q;
	void (*swap_hptr)(void * a, void *b);
	void (*assign_hptr)(void *a, void* mh);
	float (*get_cnt)(void *a, void *b);
	void *bm;
}mh;


//void mh_init(mh**, int bn, void (*swap_hptr)(void*,void*), void(*aassign_hptr)(void *, void *),int (*get_cnt)(void *a));
void mh_init(mh** h, int bn, void(*a)(void*,void*), void(*b)(void*a, void*), float (*get_cnt)(void *a, void* b), void* bm);
void mh_free(mh*);
queue* mh_free_wo_queue(mh *h);
void mh_change_msize(int, mh*);
int mh_freesize(mh*);;
void mh_insert(mh*, void *data, int number);
void mh_insert_append(mh *, void *data);
void mh_construct(mh *);
void* mh_get_max(mh*);
void mh_update(mh*,int number, void *hptr);
void mh_print(mh *, void (*print_func)(void *blk));
#endif
