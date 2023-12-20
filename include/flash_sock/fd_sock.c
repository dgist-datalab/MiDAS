#include "fd_sock.h"
#include "../types.h"
#include <string.h>
#define CLOSEDSESSION -1
#define NEWSESSION 2
#define RETRYSESSION 3

#define BUF_PREFETCH_SIZE 16384

char *packet_buffer;
char *buf_ptr;
int buf_size;
int buf_max;

int protocol_type(char *r){
	switch(r[0]){
		case 'Y': return YCSB;
		case 'R':
			if(r[1]=='E') return REDIS;
			else return ROCKSDB;
		case 'O': return OLTP;
		case 'F': return FILESOCK;
	}
}

void fd_sock_epoll_accept(fd_sock_m *f){
	struct sockaddr_in client_addr;
	socklen_t client_addr_size;
	redis_buffer *r=(redis_buffer*)f->private_data;
	struct epoll_event ev;
	int client= accept( f->server_socket, (struct sockaddr*)&client_addr, &client_addr_size);
	
	set_blocking_mode(client,false);
	memset(&ev,0,sizeof(ev));
	ev.events=EPOLLIN|EPOLLET;
	ev.data.fd=client;
	epoll_ctl(f->fd_epoll,EPOLL_CTL_ADD,client,&ev);
	for(int i=0; i<EPOLL_CLNT; i++){
		if(r->fd[i]==0){
			r->fd[i]=client;
			r->buf[i].fd=client;
			break;
		}
	}
}

bool set_blocking_mode(const int &socket, bool is_blocking){
	bool ret = true;
	const int flags = fcntl(socket, F_GETFL, 0);
	if ((flags & O_NONBLOCK) && !is_blocking) {
		return ret; 
	}
	if (!(flags & O_NONBLOCK) && is_blocking) {
		return ret; 
	}
	ret = 0 == fcntl(socket, F_SETFL, is_blocking ? flags ^ O_NONBLOCK : flags | O_NONBLOCK);

	return ret;
}

fd_sock_m *fd_sock_init(char *ip, int port, int type){
	fd_sock_m *res=(fd_sock_m*)calloc(sizeof(fd_sock_m),1);
	int server_socket=socket(PF_INET,SOCK_STREAM,0);
	int option=1;
	setsockopt( server_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option) );

	res->type=type;
	int flag;
	switch(type){
		case YCSB:
			break;
		case REDIS:
			res->fd_epoll=epoll_create(EPOLL_NUM);
			res->private_data=calloc(sizeof(redis_buffer),1);
			//((redis_buffer*)res->private_data)->buf.f=res;
			break;
		case ROCKSDB:
			break;
		case OLTP:
			break;
		case FILESOCK:
			res->fp=fopen("trace","r");
			return res;
	}

	struct sockaddr_in server_addr,client_addr;
	socklen_t client_addr_size;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family     = AF_INET;
	server_addr.sin_port       = htons(port);
	server_addr.sin_addr.s_addr= inet_addr(ip);

	if(-1 == bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr))){
		printf("bind error!\n");
		exit(1);
	}

	if(-1 == listen(server_socket, 5)){
		exit(1);
	}
	printf("server ip:%s port:%d\n",ip,port);
	printf("server waiting.....\n");
	client_addr_size  = sizeof( client_addr);
	res->server_socket=server_socket;

	if(type!=REDIS){
		res->fd = accept( server_socket, (struct sockaddr*)&client_addr, &client_addr_size);
		printf("connected!\n");
	}else{
		redis_buffer *r=(redis_buffer*)res->private_data;
		struct epoll_event ev;
		struct epoll_event events[EPOLL_NUM];
		ev.events=EPOLLIN;
		ev.data.fd=server_socket;
		epoll_ctl(res->fd_epoll,EPOLL_CTL_ADD,server_socket,&ev);
	}

	packet_buffer = (char *)calloc(BUF_PREFETCH_SIZE, sizeof(char));
	buf_ptr = packet_buffer;
	buf_size = 0;

	return res;
}

void fd_sock_clear(fd_sock_m *f){
	free(f->private_data);
	free(f);
}

int fd_sock_write(fd_sock_m* f,char *buf, int len){
	int wrote=0, temp_len;
	while(len!=wrote){
		if((temp_len=write(f->fd,buf,len-wrote))<0){
			if(errno==11) continue;
			printf("errno: %d\n",errno);
			printf("length write error!\n");
			abort();
		}
		wrote+=temp_len;
	}
	return len;
}

void fd_sock_reaccept(fd_sock_m *f){
	struct sockaddr_in client_addr;
	socklen_t client_addr_size;
	f->fd= accept( f->server_socket, (struct sockaddr*)&client_addr, &client_addr_size);
}

int fd_sock_requests(fd_sock_m *f, netdata * nd){
	int res=0;
	static bool first=false;
	do{
		switch(f->type){
			case YCSB:
				res=fd_sock_read_ycsb(f,nd);
				break;
			case REDIS:
				res=fd_sock_read_redis(f,nd);
				break;
			case ROCKSDB:
				break;
			case OLTP:
				break;
			case FILESOCK:
				res=fd_sock_read_file(f,nd);
				break;
		}
	}while(res==CLOSEDSESSION || res==NEWSESSION || res==RETRYSESSION);

	return res;
}

int fd_sock_reply(fd_sock_m *f, netdata * nd){
	switch(f->type){
		case YCSB:
			fd_sock_write_ycsb(f,nd);
			break;
		case REDIS:
			fd_sock_write_redis(f,nd);
			break;
		case ROCKSDB:
			break;
		case OLTP:
			break;
		case FILESOCK:
			fd_sock_write_file(f,nd);
			break;
	}
	return 1;
}

int buf_read(int fd, char *buf, int len) {
	int remain = len;

buf_read_retry:
	if (buf_size >= remain) {
		memcpy(buf, buf_ptr, remain);
		buf_size -= remain;
		buf_ptr += remain;

	} else {
		if (buf_size > 0) {
			memcpy(buf, buf_ptr, buf_size);
			buf += buf_size;
			remain -= buf_size;
		}

		buf_size = read(fd, packet_buffer, BUF_PREFETCH_SIZE);
		if (buf_max < buf_size) buf_max = buf_size;
		buf_ptr = packet_buffer;

		if (buf_size == 0) {
			printf("buf orring?\n");
			return len - remain;
		}

		goto buf_read_retry;
	}

	return len;
}

int fd_sock_read(fd_sock_m* f,char *buf, int len){
	int readed=0, temp_len;
	while(len!=readed){
		if((temp_len=buf_read(f->fd,buf,len-readed))<0){
			if(errno==11) continue;
			printf("errno: %d temp_len:%d\n",errno,temp_len);
			printf("length read error!\n");
			abort();
		}else if(temp_len==0){
			printf("connection closed :%d\n", errno);
			printf("buf_max: %d\n", buf_max);
			buf_max=0;
			return -1;
		}
		readed+=temp_len;
	}
	return len;
}

int fd_sock_read_redis(fd_sock_m* f, netdata *nd){
	static int cnt=0;
	redis_buffer *r=(redis_buffer*)f->private_data;
	
	struct epoll_event event;
	int nfd=epoll_wait(f->fd_epoll,&event,1, -1);
	if(nfd==0) return RETRYSESSION;
	if(event.data.fd==f->server_socket){
		fd_sock_epoll_accept(f);
		printf("new accept!\n");
		return NEWSESSION;
	}
	int clnt_idx=-1;
	for(int i=0; i<EPOLL_CLNT; i++){
		if(r->fd[i]==event.data.fd){
			clnt_idx=i; break;
		}
	}
	nd->seq=clnt_idx;

	mybuf *b=&r->buf[clnt_idx];
	b->isfitbuffer=false;
	if(buf_is_close_with_add(b,1)){ //skip *
		epoll_ctl(f->fd_epoll,EPOLL_CTL_DEL,event.data.fd,&event);
		close(r->fd[clnt_idx]);
		printf("close!\n");
		r->fd[clnt_idx]=0;
		return CLOSEDSESSION;
	}
	
	int phase=0,target_phase=buf_get_number(b),phase_word_size;
	bool skipping=false;

	while(phase!=target_phase){
		buf_add_idx(b,1); //skip $
		int phase_word_size=buf_get_number(b);
		if(skipping){
			buf_add_idx(b,phase_word_size);
			phase++;
			continue;
		}

		switch(phase){
			case 0:
				if(buf_strncmp(b,"SET",phase_word_size)==0){
					nd->type=WRITE_TYPE;		
				}else if(buf_strncmp(b,"GET",phase_word_size)==0){
					nd->type=READ_TYPE;
				}else{
					skipping=true;
				}
				break;
			case 1:
				nd->keylen=phase_word_size;
				buf_cpy_len(nd->key,b,phase_word_size);
				break;
			case 2:
				nd->valuelen=phase_word_size;
				break;
		}
		phase++;
	}
	return nd->type==WRITE_TYPE?1:0;
}

int fd_sock_write_redis(fd_sock_m *f, netdata* nd){
	static char b[]="+OK\r\n";
	redis_buffer *r=(redis_buffer*)f->private_data;
	return fd_sock_normal_write(r->fd[nd->seq],b,strlen(b));
}

int fd_sock_read_ycsb(fd_sock_m* f, netdata *data){
	char data_temp[6];
	int res;
retry:
	res=fd_sock_read(f,(char*)data_temp,sizeof(data->keylen)+sizeof(data->type)+sizeof(data->seq));
	if(res==-1){
		fd_sock_reaccept(f);
		goto retry;
	}
	data->type=data_temp[0];
	data->keylen=data_temp[1];
	data->seq=*(uint32_t*)&data_temp[2];

	switch(data->type){
		case RANGE_TYPE:	
			fd_sock_read(f,(char*)&data->scanlength,sizeof(data->scanlength));
			data->scanlength=htobe32(data->scanlength);
			fd_sock_read(f,data->key,data->keylen);
			break;
		case READ_TYPE:
			fd_sock_read(f,data->key,data->keylen);
			break;
		case WRITE_TYPE:
			fd_sock_read(f,data->key,data->keylen);
			fd_sock_read(f,(char*)&data->valuelen,sizeof(data->valuelen));
			data->valuelen=htobe32(data->valuelen);
	//		printf("value_len:%d\n",data->valuelen);
			break;
	}
	return data->type==WRITE_TYPE?1:0;
}

int fd_sock_write_ycsb(fd_sock_m* f, netdata *nd){
	fd_sock_write(f,(char*)&nd->seq,sizeof(nd->seq));
	return 1;
}


void print_byte(char *data, int len){
	for(int i=0; i<len; i++){
		printf("%d ",data[i]);
	}
	printf("\n");
}

int fd_sock_normal_write(int fd,char *buf, int len){
	int wrote=0, temp_len;
	while(len!=wrote){
		if((temp_len=write(fd,buf,len-wrote))<0){
			if(errno==11) continue;
			printf("errno: %d\n",errno);
			printf("length write error!\n");
			abort();
		}
		wrote+=temp_len;
	}
	return len;
}

int fd_sock_normal_read(int fd,char *buf, int len){
	int readed=0, temp_len;
	while(len!=readed){
		if((temp_len=read(fd,buf,len-readed))<0){
			if(errno==11) continue;
			printf("errno: %d temp_len:%d\n",errno,temp_len);
			printf("length read error!\n");
			abort();
		}else if(temp_len==0){
			printf("connection closed :%d\n", errno);
			return -1;
		}
		readed+=temp_len;
	}
	return len;
}

int fd_sock_read_file(fd_sock_m* f, netdata *data){
	static int cnt=0;
	if(f->fp){
		cnt++;
		fscanf(f->fp,"%d%d%d%d%s%d", &data->type,&data->keylen,&data->seq, &data->scanlength, data->key,&data->valuelen);
		//fscanf(f->fp,"%d", &data->type);
	}else{
		printf("what have to do!\n");
		abort();
	}
	if(cnt%10240==0){
		printf("%d\n",cnt);
	}
	return data->type==WRITE_TYPE?1:0;
}

int fd_sock_write_file(fd_sock_m* f, netdata* nd){
	return 1;
}

void fd_print_netdata(FILE *fp, netdata* data){
	fprintf(fp,"%d %d %d %d %.*s %d\n",data->type,data->keylen,data->seq, data->scanlength, data->keylen,data->key,data->valuelen);
	fflush(fp);
}
