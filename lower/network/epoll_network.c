#include "network.h"
#include <sys/epoll.h>
#include <errno.h>
#include <fcntl.h>

lower_info net_info = {
    .create      = net_info_create,
    .destroy     = net_info_destroy,
    .push_data   = net_info_push_data,
    .pull_data   = net_info_pull_data,
    .device_badblock_checker = NULL,
    .trim_block  = net_info_trim_block,
    .refresh     = net_refresh,
    .stop        = net_info_stop,
    .lower_alloc = NULL,
    .lower_free  = NULL,
    .lower_flying_req_wait = net_info_flying_req_wait,
    .lower_show_info = NULL
};

static int epoll_set;
static struct epoll_event send_ev[QDEPTH];
static struct epoll_event recv_ev[QDEPTH];
static int connected_session;
struct sockaddr_in serv_addr;

struct mem_seg *seg_table;
pthread_mutex_t flying_lock;

pthread_t tid;

algo_req *algo_req_arr[QDEPTH];
queue *free_list;

cl_lock *net_cond;

int32_t indice[QDEPTH];
int option;
static void make_connection();
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

static void close_connection(struct epoll_event ev){
	epoll_ctl(epoll_set,EPOLL_CTL_DEL,ev.data.fd,&ev);
	close(ev.data.fd);
	make_connection();
}

void *poller(void *arg) {
    struct net_data data;
    int8_t type;
	KEYT ppa;
    int32_t idx;
    algo_req *req;

	uint32_t readed,len;
	int nevents=0;

	struct epoll_event ch_ev;
	while (1) {
		nevents=epoll_wait(epoll_set,recv_ev,QDEPTH,0);
		for(int i=0; i<nevents; i++){
			if(recv_ev[i].events & EPOLLERR){
				printf("epoll error in recv!\n");
				close_connection(recv_ev[i]);
				continue;
			}
			else if(recv_ev[i].events & EPOLLOUT){ //ignore out
				continue;
			}
			int fd=recv_ev[i].data.fd;
			readed=0; len=sizeof(data);
			while(readed!=len){
				int r=read(fd,&(((char*)&data)[readed]),len-readed);
				if(r!=-1){
					readed+=r;
				}else{
					printf("read error!\n");
				}
			}

			type = data.type;
			ppa  = data.ppa;
			idx  = data.idx;
			if (idx != -1) {
				req  = algo_req_arr[idx];
				if(!req){
					printf("wtf!\n");
				}
				algo_req_arr[idx] = NULL;
				q_enqueue((void *)&indice[idx], free_list);
				cl_release(net_cond);
				req->type_lower = data.type_lower;
			}
			
	//		printf("ppa:%d\n",ppa);
			switch (type) {
				case RQ_TYPE_CREATE:
					break;
				case RQ_TYPE_DESTROY:
					break;
				case RQ_TYPE_PUSH:
					req->end_req(req);
					break;
				case RQ_TYPE_PULL:
					req->end_req(req);
					break;
				case RQ_TYPE_TRIM:
					break;
				case RQ_TYPE_FLYING:
					break;
			}

			ch_ev.data.fd=recv_ev[i].data.fd;
			ch_ev.events=EPOLLOUT;
			epoll_ctl(epoll_set,EPOLL_CTL_MOD,ch_ev.data.fd,&ch_ev);
		}
    }

    pthread_exit(NULL);
}

static ssize_t net_make_req(int8_t type, KEYT ppa, algo_req *req) {
    struct net_data data;
    data.type = type;
    data.ppa  = ppa;
    data.idx  = -1;

    if (req) {
        cl_grap(net_cond);
        data.idx = *(int32_t *)q_dequeue(free_list);
        algo_req_arr[data.idx] = req;
    }

	struct epoll_event ch_ev;
	int writed=0, len=sizeof(data);
	while(1){
		bool flag=false;
		int nevents=epoll_wait(epoll_set,send_ev,QDEPTH,0);
		for(int i=0; i<nevents; i++){
			if(send_ev[i].events & EPOLLERR){
				printf("epoll error in send");
				close_connection(send_ev[i]);
				continue;
			}
			else if(send_ev[i].events & EPOLLOUT){
				flag=true;
				int fd=send_ev[i].data.fd;
				while(writed!=len){
					int w=write(fd,&(((char*)&data)[writed]),sizeof(data)-writed);
					if(w!=-1){
						writed+=w;
					}
					else{
						printf("write error!\n");
					}
				}
				ch_ev.data.fd=send_ev[i].data.fd;
				ch_ev.events=EPOLLIN;
				epoll_ctl(epoll_set,EPOLL_CTL_MOD,ch_ev.data.fd,&ch_ev);
				break;
			}
		}
		if(flag) break;
	}
	return writed;
}

static void make_connection(){
    // Socket open
	int sock;
    sock = socket(PF_INET, SOCK_STREAM, 0); 

	option = 1;
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&option, sizeof(option));

	make_socket_non_blocking(sock);
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(IP);
    serv_addr.sin_port        = htons(PORT);
	
	int ret=0;
    if ((ret=connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0) {
		if(errno==EINPROGRESS){
			
		}
		else{
	        perror("ERROR connecting");
		    exit(1);
		}
    }
	
	connected_session++;
	struct epoll_event event;
	event.data.fd=sock;
	event.events=EPOLLOUT;
	epoll_ctl(epoll_set,EPOLL_CTL_ADD,sock,&event);
}

uint32_t net_info_create(lower_info *li) {
    li->NOB = _NOS;
    li->NOP = _NOP;
    li->SOB = BLOCKSIZE*BPS;
    li->SOP = PAGESIZE;
    li->SOK = sizeof(KEYT);
    li->PPB = _PPS;
    li->TS  = TOTALSIZE;
	
	memset(algo_req_arr,0,sizeof(algo_req_arr));
    // Mem table for metadata
    seg_table = (struct mem_seg *)malloc(sizeof(struct mem_seg) * li->NOB);
    for (uint32_t i = 0; i < li->NOB; i++) { seg_table[i].storage = NULL;
        seg_table[i].alloc   = false;
    }

    q_init(&free_list, QDEPTH);
    net_cond = cl_init(QDEPTH, false);
	epoll_set=epoll_create(QDEPTH);

    for (int i = 0; i < QDEPTH; i++) indice[i] = i;

    for (int i = 0; i < QDEPTH; i++) {
        q_enqueue((void *)&indice[i], free_list);
    }

	li->write_op=li->read_op=li->trim_op=0;

    pthread_mutex_init(&net_info.lower_lock, NULL);

    measure_init(&li->writeTime);
    measure_init(&li->readTime);

    memset(li->req_type_cnt, 0, sizeof(li->req_type_cnt));

    pthread_mutex_init(&flying_lock, NULL);
    pthread_mutex_lock(&flying_lock);
	
	for(int i=0; i<QDEPTH; i++){
		make_connection();
	}

    // Polling thread create
    pthread_create(&tid, NULL, poller, NULL);

    return 1;
}

void *net_info_destroy(lower_info *li) {
    for (uint32_t i = 0; i < li->NOB; i++) {
        if (seg_table[i].alloc) {
            free(seg_table[i].storage);
        }
    }
    free(seg_table);

    pthread_mutex_destroy(&flying_lock);

    measure_init(&li->writeTime);
    measure_init(&li->readTime);
/*
    for (int i = 0; i < LREQ_TYPE_NUM; i++) {
        printf("%d %ld\n", i, li->req_type_cnt[i]);
    }
*/
	li->write_op=li->read_op=li->trim_op=0;

    // Socket close
    net_make_req(RQ_TYPE_DESTROY, 0, NULL);
	
    return NULL;
}

static uint8_t test_type(uint8_t type) {
    uint8_t mask = 0xff >> 1;
    return type & mask;
}

void *net_info_push_data(KEYT ppa, uint32_t size, value_set *value, bool async, algo_req *const req) {
    uint8_t t_type;

    if (value->dmatag == -1) {
        fprintf(stderr, "dmatag -1 error!\n");
        exit(1);
    }

    t_type = test_type(req->type);
    if (t_type < LREQ_TYPE_NUM) {
        net_info.req_type_cnt[t_type]++;
    }

    if (req->type <= GCMW) {
        if (!seg_table[ppa/net_info.PPB].alloc) {
            seg_table[ppa/net_info.PPB].storage = (PTR)malloc(net_info.SOB);
            seg_table[ppa/net_info.PPB].alloc   = true;
        }
        PTR loc = seg_table[ppa/net_info.PPB].storage;
        memcpy(&loc[(ppa % net_info.PPB) * net_info.SOP], value->value, size);
    }

    net_make_req(RQ_TYPE_PUSH, ppa, req);
    return NULL;
}

void *net_info_pull_data(KEYT ppa, uint32_t size, value_set *value, bool async, algo_req *const req) {
    uint8_t t_type;

    if (value->dmatag == -1) {
        fprintf(stderr, "dmatag -1 error!\n");
        exit(1);
    }

    t_type = test_type(req->type);
    if (t_type < LREQ_TYPE_NUM) {
        net_info.req_type_cnt[t_type]++;
    }

    if (req->type <= GCMW) {
        if (!seg_table[ppa/net_info.PPB].alloc) {
            fprintf(stderr, "Metadata is not allocated (DATA NOT FOUND)\n");
            exit(1);
        }
        PTR loc = seg_table[ppa / net_info.PPB].storage;
        memcpy(value->value, &loc[(ppa % net_info.PPB) * net_info.SOP], size);
        req->type_lower = 1;
    }

    net_make_req(RQ_TYPE_PULL, ppa, req);
    return NULL;
}

void *net_info_trim_block(KEYT ppa, bool async) {
    net_make_req(RQ_TYPE_TRIM, ppa, NULL);

    net_info.req_type_cnt[TRIM]++;

    if (seg_table[ppa/net_info.PPB].alloc) {
        free(seg_table[ppa/net_info.PPB].storage);
        seg_table[ppa/net_info.PPB].storage = NULL;
        seg_table[ppa/net_info.PPB].alloc = false;
    }

    return NULL;
}

void net_info_flying_req_wait() {
    //net_make_req(RQ_TYPE_FLYING, 0, NULL);
    //pthread_mutex_lock(&flying_lock);
    while (free_list->size != QDEPTH) {}
}

void *net_refresh(struct lower_info *li) {
    measure_init(&li->writeTime);
    measure_init(&li->readTime);
    li->write_op = li->read_op = li->trim_op = 0;
    return NULL;
}

void net_info_stop() {}

