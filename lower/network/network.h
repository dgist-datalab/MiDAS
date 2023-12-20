#include "../../include/container.h"
#include "../../bench/measurement.h"
#include "../../interface/queue.h"
#include "../../include/settings.h"
#include "../../include/utils/cond_lock.h"


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netinet/tcp.h>


#define RQ_TYPE_CREATE	6
#define RQ_TYPE_DESTROY 0
#define RQ_TYPE_PUSH    1
#define RQ_TYPE_PULL    2
#define RQ_TYPE_TRIM    4
#define RQ_TYPE_FLYING  5

struct net_data {
    int8_t type;
    KEYT ppa;
    int32_t idx;
    uint8_t req_type;
    uint8_t type_lower;
};

struct mem_seg {
    PTR storage;
    bool alloc;
};


uint32_t net_info_create(lower_info *li);
void *net_info_destroy(lower_info *li);
void *net_info_push_data(KEYT ppa, uint32_t size, value_set *value, bool async, algo_req *const req);
void *net_info_pull_data(KEYT ppa, uint32_t size, value_set *value, bool async, algo_req *const req);
void *net_info_trim_block(KEYT ppa, bool async);
void net_info_flying_req_wait();
void *net_refresh(struct lower_info* li);
void net_info_stop();
