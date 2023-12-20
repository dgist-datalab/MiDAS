#include "buffer_manager.h"
#include "fd_sock.h"
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
int memmove_read(mybuf* b){
	if(b->remain>0){
		memmove(&b->buf[0],&b->buf[b->idx],b->remain);
		b->idx=0;
	}
	if(b->start==0 || b->remain==0){ // start
		int len=0;
		len=read(b->fd,&b->buf[b->remain],MAXBUF-b->remain);
	//	printf("readed req:%.*s\n",len,&b->buf[b->remain]);
		if(len==0){
			return -1;
			//fd_sock_reaccept(b->f);
			//len=read(b->f->fd,&b->buf[b->remain],MAXBUF-b->remain);
		}
		b->remain+=len;
		b->start=1;
		b->idx=0;
		b->isfitbuffer=false;
	}
	else{ //processing
		if(b->buf[b->remain+b->idx-1]=='\n'){//it is probable end;
			b->isfitbuffer=true;
		}
		else{ //it should read
			b->remain+=read(b->fd,&b->buf[b->remain],MAXBUF-b->remain);
		}
	}
	return 1;
}

int get_digit_len(int num){
	int res=1;
	while(num/10){
		num/=10;
		res++;
	}
	return res;
}

void buf_add_idx(mybuf* b, int add){
	if(!b->isfitbuffer && b->remain<add){
		memmove_read(b);
	}
	b->idx+=add;
	b->remain-=add;
}

int buf_is_close_with_add(mybuf *b,int add){
	if(b->remain==0 && memmove_read(b)==-1){
		return 1;
	}
	else{
		b->idx+=add;
		b->remain-=add;
		return 0;
	}
}

int buf_strncmp(mybuf* b, char *test, int buf_len){
	if(!b->isfitbuffer && b->remain<buf_len+2){
		memmove_read(b);
	}
	int len=buf_len;
	int res=strncmp(&b->buf[b->idx],test,strlen(test));
	if(res==0){
		b->idx+=len+2;//\r\n
		b->remain-=len+2;
	}
	return res;
}

void buf_cpy_len(char *des, mybuf *b, int len){
	if(!b->isfitbuffer && b->remain<len+2){
		memmove_read(b);
	}
	memcpy(des,&b->buf[b->idx],len);
	b->idx+=len+2;//\r\n
	b->remain-=len+2;
}

int buf_get_number(mybuf *b){
	if(!b->isfitbuffer && b->remain<4+2){
		memmove_read(b);
	}
	int res=atoi(&b->buf[b->idx]);
	int len=get_digit_len(res);
	b->idx+=len+2; //\r\n
	b->remain-=len+2; //\r\n
	return res;
}
