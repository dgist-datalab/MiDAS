#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../../include/lsm_settings.h"
#include "../../include/FS.h"
#include "../../include/settings.h"
#include "../../include/types.h"
#include "../../bench/bench.h"
#include "../interface.h"
#include "../../algorithm/Lsmtree/lsmtree.h"
#include "../../algorithm/Lsmtree/skiplist.h"
#include "../../algorithm/Lsmtree/level.h"
#include "../../include/utils/kvssd.h"

extern lsmtree LSM;
skiplist *skip1;
skiplist *skip2;
skiplist *skip3;
char *skiplist_cvt2file(skiplist *mem){
	if(mem->all_length<=0) return NULL;
	char *res=(char*)malloc(PAGESIZE);
	char *ptr=(char*)res;
	uint16_t *bitmap=(uint16_t*)ptr;
	uint32_t idx=1;
	uint32_t cnt=0;
	memset(bitmap,-1,KEYBITMAP/sizeof(uint16_t));
	uint16_t data_start=KEYBITMAP;
	//uint32_t length_before=mem->all_length;
	/*end*/
	uint32_t length=0;
	snode *temp;
	do{	
		temp=skiplist_pop(mem);
		memcpy(&ptr[data_start],&temp->ppa,sizeof(temp->ppa));
		memcpy(&ptr[data_start+sizeof(temp->ppa)],temp->key.key,temp->key.len);
		bitmap[idx]=data_start;
		//fprintf(stderr,"[%d:%d] - %.*s\n",idx,data_start,KEYFORMAT(temp->key));
		data_start+=temp->key.len+sizeof(temp->ppa);
		length+=KEYLEN(temp->key);
		idx++;
		cnt++;
		//free the skiplist
		free(temp->list);
		free(temp);
	}
	while(mem->all_length && (length+KEYLEN(mem->header->list[1]->key)<=PAGESIZE-KEYBITMAP) && (cnt<KEYBITMAP/sizeof(uint16_t)-2));
	bitmap[0]=idx-1;
	bitmap[idx]=data_start;
	return res;
}

void skiplist_dump_write(char *filename, skiplist *skip){
	int fd=open(filename,O_RDWR|O_CREAT|O_TRUNC,0666);
	char *target;
	while((target=skiplist_cvt2file(skip))){
		write(fd,target,PAGESIZE);
	//	LSM.lop->header_print(target);
		free(target);
	}
	close(fd);
}

#define RANDRANGE 6000
bool testflag=false;
int main(int argc,char* argv[]){
	/*
	   to use the custom benchmark setting the first parameter of 'inf_init' set false
	   if not, set the parameter as true.
	   the second parameter is not used in anycase.
	 */
	int fd=open("invalidate_ppa.bin",O_RDWR|O_CREAT|O_TRUNC,0666);
	inf_init(0,0);
	skip1=skiplist_init();
	skip2=skiplist_init();
	skip3=skiplist_init();

	fprintf(stderr,"making 1\n");
	uint32_t ppa=0;
	snode *temp;
	while(skip1->size<5150){//skip1->all_length<PAGESIZE){
		KEYT *key=(KEYT*)malloc(sizeof(KEYT));
		key->len=my_itoa(rand()%RANDRANGE,&key->key);
		temp=skiplist_insert(skip1,*key,NULL,false);
		temp->ppa=ppa++;
	}

	snode *temp2;
	for_each_sk(temp2,skip1){
		temp=skiplist_insert(skip3,temp2->key,NULL,false);
		temp->ppa=temp2->ppa;
	}

	skiplist_dump_write("h_level.bin",skip1);

	fprintf(stderr,"making 2\n");
	uint32_t old_ppa;
	while(skip2->size<11200){//skip2->all_length<PAGEaSIZE){
		KEYT *key=(KEYT*)malloc(sizeof(KEYT));
		key->len=my_itoa(rand()%RANDRANGE,&key->key);
		temp=skiplist_insert(skip2,*key,NULL,false);
		temp->ppa=ppa++;
	}

	for_each_sk(temp2,skip2){
		temp=skiplist_insert(skip3,temp2->key,NULL,false);
		if(temp->ppa!=UINT_MAX){
			printf("%d temp\n",temp2->ppa);
			write(fd,&temp2->ppa,sizeof(temp->ppa));
		}
		else{
			temp->ppa=temp2->ppa;
		}
	}
	skiplist_dump_write("l_level.bin",skip2);
	printf("skiplist 3 size:%d\n",skip3->size);
	fprintf(stderr,"result\n");
	skiplist_dump_write("result.bin",skip3);
	return 0;
}
