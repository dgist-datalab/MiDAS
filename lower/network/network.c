#include "network.h"
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

int sock_fd;
//int sock_fd2;
struct sockaddr_in serv_addr;

struct mem_seg *seg_table;
pthread_mutex_t flying_lock;

pthread_t tid;

algo_req *algo_req_arr[QDEPTH];
queue *free_list;

cl_lock *net_cond;

int32_t indice[QDEPTH];
int option;

void *poller(void *arg) {
    struct net_data data;
    int8_t type;
    KEYT ppa;
    int32_t idx;
    algo_req *req;

	uint32_t readed,len;

    while (1) {
	readed=0;
#if TCP
		while(readed<sizeof(data)){
			len=read(sock_fd,&((char*)&data)[readed],sizeof(data)-readed);
			if(len==-1)
				continue;
			readed+=len;
		}
//		write(sock_fd2,&ack,sizeof(char));
#else
        recv(sock_fd, &data, sizeof(data), MSG_WAITALL);	
	    send(sock_fd, &ack, sizeof(char), MSG_CONFIRM);
#endif

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
		//	printf("idx:%d\n",idx);
            req->type_lower = data.type_lower;
        }

        switch (type) {
        case RQ_TYPE_CREATE:
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
            //pthread_mutex_unlock(&flying_lock);
            break;
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

#if TCP
	int writed=0,len=sizeof(data);
	while(writed!=len){
		int w=write(sock_fd, &data, sizeof(data));
		if(w!=-1){
			writed+=w;
		}
	}
#else
    return send(sock_fd, &data, sizeof(data), MSG_CONFIRM);
#endif

/*
	char t;
#if TCP
	read(sock_fd,&t,sizeof(t));
#else	
	recv(sock_fd,&t,sizeof(t),MSG_WAITALL);
#endif*/
}

uint32_t net_info_create(lower_info *li) {
    li->NOB = _NOS;
    li->NOP = _NOP;
    li->SOB = BLOCKSIZE*BPS;
    li->SOP = PAGESIZE;
    li->SOK = sizeof(KEYT);
    li->PPB = _PPS;
    li->TS  = TOTALSIZE;

    // Mem table for metadata
    seg_table = (struct mem_seg *)malloc(sizeof(struct mem_seg) * li->NOB);
    for (int i = 0; i < li->NOB; i++) { seg_table[i].storage = NULL;
        seg_table[i].alloc   = false;
    }

    q_init(&free_list, QDEPTH);
    net_cond = cl_init(QDEPTH, false);

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

    // Socket open
#if TCP
    sock_fd = socket(PF_INET, SOCK_STREAM, 0); // TCP
	//sock_fd2= socket(PF_INET,SOCK_STREAM,0);
#else
    sock_fd = socket(PF_INET, SOCK_DGRAM, 0);
	//sock_fd2= socket(PF_INET,SOCK_DGRAM,0);
#endif
    if (sock_fd < 0 ){//|| sock_fd2<0) {
        perror("ERROR opening socket");
        exit(1);
    }
#if TCP
	option = 1;
	setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&option, sizeof(option));
	//setsockopt(sock_fd2, IPPROTO_TCP, TCP_NODELAY, (const char *)&option, sizeof(option));

	/*
	int flag=fcntl(sock_fd,F_GETFD,0);
	fcntl(sock_fd,F_SETFD,flag|O_NONBLOCK);*/
#endif

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(IP);
    serv_addr.sin_port        = htons(PORT);

    if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR connecting");
        exit(1);
    }

    // Polling thread create
    pthread_create(&tid, NULL, poller, NULL);

    return 1;
}

void *net_info_destroy(lower_info *li) {
    for (int i = 0; i < li->NOB; i++) {
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

    close(sock_fd);
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

