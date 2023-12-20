#include "../../include/settings.h"
#include "../../include/container.h"
#include "../bb_checker.h"
#include "../../lower/network/network.h"
#include "../lfqueue.h"
#include "../interface.h"
#include "../../bench/bench.h"
#include <pthread.h>
#include <sys/epoll.h>
#include <errno.h>
#include <fcntl.h>

struct serv_params {
    struct net_data *data;
    value_set *vs;
};

#if defined(bdbm_drv)
extern struct lower_info memio_info;
#elif defined(posix_memory)
extern struct lower_info my_posix;
#endif


int option;
static int epoll_set;
static struct epoll_event send_ev[QDEPTH+1];
static struct epoll_event recv_ev[QDEPTH+1];
static int connected_session;

struct sockaddr_in serv_addr;
socklen_t clnt_sz;

queue *end_req_q;

pthread_t tid;
pthread_mutex_t socket_lock;

static int make_socket_non_blocking(int sfd){
	int flags, s;
	flags = fcntl (sfd, F_GETFL, 0);
	if (flags == -1){
		perror ("fcntl");
		return -1;
	}

	flags |= O_NONBLOCK;
	s = fcntl (sfd, F_SETFL, flags);
	if (s == -1){
		perror ("fcntl");
		return -1;
	}

	return 0;
}

void *reactor(void *arg) {
    struct net_data *sent;

	int len=sizeof(struct net_data), writed=0, w, nevents=0;
	struct epoll_event ev;
	static int cnt=0;
    while (1) {
		nevents=epoll_wait(epoll_set,send_ev,QDEPTH+1, 0);
		for(int i=0; i<nevents; i++){
			if(send_ev[i].events & EPOLLERR){
				printf("epoll error in send\n");
				continue;
			}
			else if(send_ev[i].events & EPOLLIN){
				continue;
			}
			if (sent = (struct net_data *)q_dequeue(end_req_q)) {

			}
			else continue;

			int fd=send_ev[i].data.fd;
			writed=0;
			while(writed!=len){
				w=write(fd, &(((char*)sent)[writed]), sizeof(struct net_data)-writed);
				if(w!=-1){
					writed+=w;
				}else{
					printf("write error!\n");
				}
			}
			ev.data.fd=fd;
			ev.events=EPOLLIN;
			epoll_ctl(epoll_set,EPOLL_CTL_MOD,fd,&ev);
			free(sent);
		}
    }

    pthread_exit(NULL);
}

void *serv_end_req(algo_req *req) {
    struct serv_params *params = (struct serv_params *)req->params;

    if (params->data->type == RQ_TYPE_PUSH) {
        inf_free_valueset(params->vs, FS_MALLOC_W);
    } else if (params->data->type == RQ_TYPE_PULL) {
        inf_free_valueset(params->vs, FS_MALLOC_R);
    }

    params->data->type_lower = req->type_lower;

    q_enqueue((void *)(params->data), end_req_q);

    free(req->params);
    free(req);

    return NULL;
}

static algo_req *make_serv_req(value_set *vs, struct net_data *data) {
    algo_req *req = (algo_req *)malloc(sizeof(algo_req));

    struct serv_params *params = (struct serv_params *)malloc(sizeof(struct serv_params));
    params->data = (struct net_data *)malloc(sizeof(struct net_data));

    params->vs = vs;
    *(params->data) = *data;

    req->params  = (void *)params;
    req->end_req = serv_end_req;
    req->type    = data->req_type;
	req->type_lower =0;
	req->parents=NULL;

    return req;
}

static void accept_epoll(int s_sock){
	int fd;
	struct sockaddr_in clnt_addr;
	struct epoll_event ev;
	socklen_t clnt_size=sizeof(clnt_addr);
	
	fd=accept(s_sock,(struct sockaddr*)&clnt_addr,&clnt_size);
	if(fd==-1){
		printf("error in accept!:%d\n",errno);
		exit(1);
	}
	
	printf("connected session number:%d\n",connected_session++);
	make_socket_non_blocking(fd);
	ev.data.fd=fd;
	ev.events=EPOLLIN;
	epoll_ctl(epoll_set,EPOLL_CTL_ADD,fd,&ev);
}

int main(){
    struct lower_info *li;
    struct net_data data;

    int8_t type;
    KEYT ppa;

    algo_req *serv_req;
    value_set *dummy_vs;
	struct epoll_event ev;

#if defined(bdbm_drv)
    li = &memio_info;
#elif defined(posix_memory)
    li = &my_posix;
#endif

	li->create(li);
	bb_checker_start(li);

    q_init(&end_req_q, 1024);

	epoll_set=epoll_create(QDEPTH+1);

	pthread_mutex_init(&socket_lock, NULL);

	int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket openning ERROR");
        exit(1);
    }

    option = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&option, sizeof(option));

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORT);

    if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("Binding ERROR");
        exit(1);
    }

    if (listen(sock, QDEPTH) == -1) {
        perror("Listening ERROR");
        exit(1);
    }
	
	make_socket_non_blocking(sock);
	ev.data.fd=sock;
	ev.events=EPOLLIN;
	epoll_ctl(epoll_set,EPOLL_CTL_ADD,sock,&ev);

    pthread_create(&tid, NULL, reactor, NULL);

	int readed,len;
	int nevents;
	static int cnt=0;
    while (1) {

		nevents=epoll_wait(epoll_set,recv_ev,QDEPTH+1,0);
		for(int i=0; i<nevents; i++){
			if(recv_ev[i].events & EPOLLERR){
				printf("epoll error in recv!\n");
				continue;
			}
			else if(recv_ev[i].events & EPOLLOUT){
				continue;
			}
			if(recv_ev[i].data.fd==sock){
				accept_epoll(sock);
				continue;
			}
	
			int fd=recv_ev[i].data.fd;
			readed=0;
			while(readed<(int)sizeof(data)){
				len=read(fd,&(((char*)&data)[readed]),sizeof(data)-readed);
				if(len==-1){
					printf("read error\n");
					continue;
				}
				readed+=len;
			}
			type = data.type;
			ppa  = data.ppa;

			switch (type) {
				case RQ_TYPE_DESTROY:
					li->destroy(li);
					goto end;
					break;
				case RQ_TYPE_PUSH:
					dummy_vs = inf_get_valueset(NULL, FS_MALLOC_W, PAGESIZE);
					serv_req = make_serv_req(dummy_vs, &data);

					li->push_data(ppa, PAGESIZE, dummy_vs, ASYNC, serv_req);
					break;
				case RQ_TYPE_PULL:
					dummy_vs = inf_get_valueset(NULL, FS_MALLOC_R, PAGESIZE);
					serv_req = make_serv_req(dummy_vs, &data);

					li->pull_data(ppa, PAGESIZE, dummy_vs, ASYNC, serv_req);
					break;
				case RQ_TYPE_TRIM:
					li->trim_block(ppa, ASYNC);
					break;
				case RQ_TYPE_FLYING:
					li->lower_flying_req_wait();
					/*
					pthread_mutex_lock(&socket_lock);
					write(fd, &data, sizeof(data));
					pthread_mutex_unlock(&socket_lock);
					*/
					break;
			}

			ev.data.fd=fd;
			ev.events=EPOLLOUT;
			epoll_ctl(epoll_set,EPOLL_CTL_MOD,fd,&ev);
		}
    }

end:
	close(sock);
    q_free(end_req_q);

    return 0;
}
