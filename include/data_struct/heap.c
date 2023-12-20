#include "heap.h"
#include <stdio.h>
#include <stdlib.h>

/*
void mh_init(mh** h, int bn, void(*a)(void*,void*), void(*b)(void*a, void*), int (*get_cnt)(void *a)){
	*h=(mh*)malloc(sizeof(mh));
	(*h)->size=0;
	(*h)->max=bn;

	(*h)->body=(hn*)calloc(sizeof(hn),2*(bn+1));
	(*h)->swap_hptr=a;
	(*h)->assign_hptr=b;
	(*h)->get_cnt=get_cnt;

	q_init(&((*h)->q), bn);
}
*/

void mh_init(mh** h, int bn, void(*a)(void*,void*), void(*b)(void*a, void*), float (*get_cnt)(void *a, void* b), void* bm) {
	*h=(mh*)malloc(sizeof(mh));
	(*h)->size=0;
	(*h)->max=bn;

	(*h)->body=(hn*)calloc(sizeof(hn),2*(bn+1));
	(*h)->swap_hptr=a;
	(*h)->assign_hptr=b;
	(*h)->get_cnt=get_cnt;
	(*h)->bm=bm;

	q_init(&((*h)->q), bn);
}

void mh_change_msize(int msize, mh *h) {
	if (msize <= h->size) {
		printf("heap max size is smaller than number of blocks\n");
	} else {
		hn* new_hn = (hn*)calloc(sizeof(hn), 2*(msize+1));
		memcpy(new_hn, h->body, h->max);
		free(h->body);
		h->body = new_hn;
		h->max = msize;
	}
}

int mh_freesize(mh* h) {
	return h->max - h->size;
}

void mh_free(mh* h){
	free(h->body);
	if (h->q) q_free(h->q);
	free(h);
}

queue* mh_free_wo_queue(mh *h) {
	free(h->body);
	queue* res = h->q;
	free(h);
	return res;
}

static hn* maxchild(mh *h, hn *n){
	hn *res=NULL;
	int idx=(n-h->body);
	if(!n->data) return res;
	n->cnt=h->get_cnt(n->data, h->bm);
	
	hn *lc=MHL_CHIPTR(h,idx);
	if(lc->data)
		lc->cnt=h->get_cnt(lc->data, h->bm);

	hn *rc=MHR_CHIPTR(h,idx);
	if(rc->data)
		rc->cnt=h->get_cnt(rc->data, h->bm);

	if(lc->data && !rc->data) res=lc;
	else if(!lc->data && rc->data) res=rc;
	else if(lc->data && rc->data) res=lc->cnt>rc->cnt?lc:rc;

	return res;
}

hn* mh_internal_update(mh *h, hn* n){
	int idx=(n-h->body);
	hn *chgd=n;
	while(idx>1){
		hn *p=MHPARENTPTR(h,idx);
		if(p->cnt<n->cnt){
			//int temp=n->cnt;
			double temp = n->cnt;
			n->cnt=p->cnt;
			p->cnt=temp;

			void *data=n->data;
			n->data=p->data;
			p->data=data;

			node* tnode = n->qnode;
			n->qnode = p->qnode;
			p->qnode=tnode;

			h->swap_hptr(p->data,n->data);
			chgd=p;
		}
		else break;
		idx/=2;
	}
	return chgd;
}

hn* mh_internal_downdate(mh *h, hn *n){
	hn *chgd=n;
	while(1){
		hn *child=maxchild(h,chgd);
		if(child){
			if(child->cnt > chgd->cnt){
				//int temp=child->cnt;
				double temp=child->cnt;
				child->cnt=chgd->cnt;
				chgd->cnt=temp;
				
				void *data=child->data;
				child->data=chgd->data;
				chgd->data=data;

				node* tnode = child->qnode;
				child->qnode = chgd->qnode;
				chgd->qnode = tnode;

				h->swap_hptr(child->data,chgd->data);
				chgd=child;
			}
			else break;
		}
		else break;
	}
	return chgd;
}

void mh_insert(mh* h, void *data, int number){
	if(h->size>h->max){
		printf("full heap!\n");
		abort();
		return;
	}
	h->size++;
	hn* n=&h->body[h->size];
	n->data=data;
	n->cnt=(double)number;

	n->qnode = q_enqueue_node(data, h->q);
	
	h->assign_hptr(data,(void*)n);
	mh_internal_update(h,n);
}

void *mh_get_max(mh* h){
	void *res=(void*)h->body[1].data;
	q_delete(h->q, h->body[1].qnode);
	
	h->body[1].data=h->body[h->size].data;
	h->body[1].cnt=h->body[h->size].cnt;
	h->body[1].qnode = h->body[h->size].qnode;
	h->body[h->size].data=NULL;

	mh_internal_downdate(h,&h->body[1]);
	h->size-=1;
	return res;
}

void mh_update(mh* h,int number, void *hptr){
	hn* p=(hn*)hptr;
	//int temp=p->cnt;
	double temp=p->cnt;
	p->cnt=number;

	if(temp<number)
		mh_internal_update(h,p);
	else
		mh_internal_downdate(h,p);
}

void mh_insert_append(mh *h, void *data){
	if(h->size>h->max){
		printf("full heap!\n");
		abort();
		return;
	}
	h->size++;
	hn* n=&h->body[h->size];
	n->data=data;
	n->qnode = q_enqueue_node(data, h->q);
	h->assign_hptr(data,(void*)n);
}

void mh_construct(mh *h){
	int all_size=h->size;
	int depth=1;
	while(all_size/=2){depth++;}

	for(int i=depth-1; i>=1; i--){
		int depth_start=(1<<(i-1));
		int depth_end=(1<<i);
		for(int j=depth_start; j<depth_end; j++){
			mh_internal_downdate(h,&h->body[j]);
		}
	}
}

void mh_print(mh *h, void(*print_func)(void*blk)){
	for(uint32_t i=0; i<h->size; i++){
		print_func(h->body[i].data);
	}
}
