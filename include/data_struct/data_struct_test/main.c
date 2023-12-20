#include "heap.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

typedef struct int_node{
	int cnt;
	void *hptr;
}inode;

#define ALLNUM 1025

void inode_swap_hptr( void *a, void *b){
	inode *aa=(inode*)a;
	inode *bb=(inode*)b;

	void *temp=aa->hptr;
	aa->hptr=bb->hptr;
	bb->hptr=temp;
}

void inode_assign_ptr(void *a, void *mh){
	inode *aa=(inode*)a;
	aa->hptr=mh;
}

int inode_get_cnt(void *a){
	inode *aa=(inode*)a;
	return aa->cnt;
}

int main(){
	mh *h;
	inode *t=(inode *)malloc(sizeof(inode)*ALLNUM);
	mh_init(&h,ALLNUM,inode_swap_hptr,inode_assign_ptr,inode_get_cnt);

	for(int i=0; i<ALLNUM; i++){
		t[i].cnt=i;
	}

	for(int i=0; i<ALLNUM*2; i++){
		int a=rand()%ALLNUM;
		int b=rand()%ALLNUM;
		if(a==b) continue;

		int temp=t[a].cnt;
		t[a].cnt=t[b].cnt;
		t[b].cnt=temp;
	}

	printf("before heap!\n");
	for(int i=0; i<ALLNUM; i++){
		mh_insert_append(h,&t[i]);
	}
	printf("\nconstructing....\n");
	mh_construct(h);

	printf("\nafter heap\n");
	for(int i=0; i<ALLNUM; i++){
		inode *temp=(inode*)mh_get_max(h);
		printf("%d\n",temp->cnt);
	}
}
