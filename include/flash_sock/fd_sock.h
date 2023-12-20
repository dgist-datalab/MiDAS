#ifndef FD_SOCK_H
#define FD_SOCK_H
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>

#include "buffer_manager.h"
#define EPOLL_CLNT 128
#define EPOLL_NUM (EPOLL_CLNT+1)

enum net_type{
	YCSB,REDIS,ROCKSDB,OLTP,FILESOCK
};

enum nt_type{
	NON_TYPE,WRITE_TYPE,READ_TYPE,RANGE_TYPE
};

typedef struct netdata_t{
	uint8_t type;
	uint8_t keylen;
	uint32_t seq;
	uint32_t scanlength;
	char key[UINT8_MAX];
	uint32_t valuelen;
}netdata;

typedef struct redis_buffer{
	int fd[EPOLL_CLNT];
	mybuf buf[EPOLL_CLNT];
}redis_buffer;

typedef struct fd_sock_manager{
	int type;
	int fd;
	FILE *fp;
	int fd_epoll;
	int server_socket;
	void *private_data;
}fd_sock_m;

fd_sock_m *fd_sock_init(char *ip, int port,int type);
void fd_print_netdata(FILE *fp, netdata*);
void fd_sock_reaccept(fd_sock_m *);
void fd_sock_clear(fd_sock_m *);
int fd_sock_write(fd_sock_m*,char *buf, int len);
int fd_sock_read(fd_sock_m*,char *buf, int len);
int fd_sock_normal_write(int,char *buf, int len);
int fd_sock_normal_read(int,char *buf, int len);
int fd_sock_requests(fd_sock_m *, netdata *);
int fd_sock_reply(fd_sock_m *, netdata *);
void print_byte(char *data, int len);
int protocol_type(char *);
bool set_blocking_mode(const int &socket, bool is_blocking);

/*for redis function*/
int fd_sock_read_redis(fd_sock_m*, netdata *);
int fd_sock_write_redis(fd_sock_m*, netdata*);

/*for ycsb_function*/
int fd_sock_read_ycsb(fd_sock_m*, netdata *);
int fd_sock_write_ycsb(fd_sock_m*, netdata*);

/*for file function*/
int fd_sock_read_file(fd_sock_m*, netdata *);
int fd_sock_write_file(fd_sock_m*, netdata*);
#endif
