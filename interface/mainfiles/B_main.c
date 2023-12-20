#define _LARGEFILE64_SOURCE

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <argp.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include <linux/nbd.h>
#include <signal.h>

#include "../include/container.h"
#include "interface.h"
#include "buse.h"
#include "queue.h"
typedef struct fs_req{
	struct nbd_reply *r;
	char *data;
	char type;
	int max;
	int now;
	int sk;
	int seq;
	uint64_t len;
}fs_req;

//std::queue<fs_req *> *main_q;
queue *main_q;
void *returner(void *a){
	while(1){
		while(main_q->size){
			fs_req *temp=(fs_req*)q_pick(main_q);
			if(temp->now==temp->max){
				if(temp->type==FS_GET_T){
					write_all(temp->sk,(char*)temp->r,sizeof(struct nbd_reply));
					write_all(temp->sk,(char*)temp->data,temp->len);
					free(temp->data);
				}
				else{	
					free(temp->data);
					write_all(temp->sk,(char*)temp->r,sizeof(struct nbd_reply));
				}
				//printf("check temp:%d %d\n",temp->seq,main_q->size());
			}
			else{
				continue;
			}
			free(temp->r);
			free(temp);
			q_dequeue(main_q);
		}
	}
}

void *fs_end_req(void *arg){
	fs_req *res=(fs_req*)arg;
	//printf("%d end_req\n",res->seq);
	res->now++;
	return NULL;
}

static fs_req *make_fs_req_from_data(int sk,char type,const void *_buf, u_int32_t len, u_int64_t offset, void *userdata){
	static int cnt=0;
	int i;
	fs_req *res=(fs_req*)malloc(sizeof(fs_req));
	
	int nreq=len/PAGESIZE+len%PAGESIZE?1:0;
	res->r=(struct nbd_reply*)userdata;
	res->max=nreq;
	res->now=0;
	res->data=(char*)_buf;
	res->type=type;
	res->sk=sk;
	res->len=len;
	res->seq=cnt++;
	
	uint32_t target=offset;
	uint64_t remain=len;
	char *buf=(char*)_buf;
	int _type;
	for(i=0; i<nreq; i++){
		KEYT key=target/PAGESIZE;
		if(target%(PAGESIZE)==0 && remain>=PAGESIZE){
			inf_make_req_fromApp(type,-1,key,0,PAGESIZE,(type!=FS_DELETE_T?(char*)&buf[len-remain]:NULL),res,fs_end_req);
			remain-=PAGESIZE;
			target+=PAGESIZE;
		}else if(target%(PAGESIZE)==0 && remain%PAGESIZE!=0){
			_type=(type==FS_SET_T?FS_RMW_T:type);
			inf_make_req_fromApp(_type,1,key,target%PAGESIZE,remain%PAGESIZE,(type!=FS_DELETE_T?(char*)&buf[len-remain]:NULL),res,fs_end_req);
			remain-=(remain%PAGESIZE);
			target+=(remain%PAGESIZE);
		}else if(target % (PAGESIZE)!=0){
			_type=(type==FS_SET_T?FS_RMW_T:type);
			uint32_t target_len;
			target_len = remain<PAGESIZE/2?remain:PAGESIZE/2;
			inf_make_req_fromApp(_type,0,key,target%PAGESIZE,target_len,(type!=FS_DELETE_T?(char*)&buf[len-remain]:NULL),res,fs_end_req);
			remain-=target_len;
			target+=target_len;
		}
		else{
			abort();
		}
	}
	while(!q_enqueue((void*)res,main_q)){}
//	printf("done temp:%d\n",res->seq);
	return res;
}

static int fs_read(int sk,void *buf, u_int32_t len, u_int64_t offset, void *userdata)
{
	make_fs_req_from_data(sk,FS_GET_T,buf,len,offset,userdata);
	return 0;
}

static int fs_write(int sk,const void *buf, u_int32_t len, u_int64_t offset, void *userdata)
{
	make_fs_req_from_data(sk,FS_SET_T,buf,len,offset,userdata);
	return 0;
}
static int fs_trim(int sk,u_int64_t from, u_int32_t len, void *userdata){
	make_fs_req_from_data(sk,FS_DELETE_T,NULL,len,from,userdata);
	return 0;
}

struct buse_operations bop = {0,};
struct arguments{
	unsigned long long size;
	char *devices;
	int verbose;
};
pthread_t return_t;
int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("should input nbd device name\n");
		return -1;
	}
	inf_init();
	struct arguments args;
	args.size=TOTALSIZE;
	args.devices=argv[1];
	args.verbose=1;

	bop.read=fs_read;
	bop.write=fs_write;
	bop.trim=fs_trim;
	bop.size=TOTALSIZE;
	
	q_init(&main_q,1024);
	pthread_create(&return_t,NULL,returner,NULL);

	buse_main(args.devices, &bop,(void *)&args.verbose);
	return 0;
}
