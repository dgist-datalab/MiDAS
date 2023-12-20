
#ifndef __CHEEZE_INF_H__
#define __CHEEZE_INF_H__

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "../include/container.h"

#define barrier() __asm__ __volatile__("": : :"memory")

struct cheeze_req_user {
    int id; 
    int op; 
    unsigned int pos; // sector_t but divided by 4096
    unsigned int len;
} __attribute__((aligned(8), packed));

struct cheeze_trace_req_user {
    int id; 
    int op; 
    unsigned int pos; // sector_t but divided by 4096
    unsigned int len;
} __attribute__((aligned(8), packed));


#define CHEEZE_QUEUE_SIZE 1024
#define CHEEZE_BUF_SIZE (2ULL * 1024 * 1024)
#define ITEMS_PER_HP ((1ULL * 1024 * 1024 * 1024) / CHEEZE_BUF_SIZE)
#define BITS_PER_EVENT (sizeof(uint64_t) * 8)

#define EVENT_BYTES (CHEEZE_QUEUE_SIZE / BITS_PER_EVENT)

#define SEND_OFF 0
#define SEND_SIZE (CHEEZE_QUEUE_SIZE * sizeof(uint8_t))

#define RECV_OFF (SEND_OFF + SEND_SIZE)
#define RECV_SIZE (CHEEZE_QUEUE_SIZE * sizeof(uint8_t))

#define SEQ_OFF (RECV_OFF + RECV_SIZE)
#define SEQ_SIZE (CHEEZE_QUEUE_SIZE * sizeof(uint64_t))

#define REQS_OFF (SEQ_OFF + SEQ_SIZE)

#define ureq_print(u) \
    do { \
        printf("%s:%d\n    id=%d\n    op=%d\n    pos=%u\n    len=%u\n", __func__, __LINE__, u->id, u->op, u->pos, u->len); \
    } while (0);

enum req_opf {
    /* read sectors from the device */
    REQ_OP_READ     = 0,
    /* write sectors to the device */
    REQ_OP_WRITE        = 1,
    /* flush the volatile write cache */
    REQ_OP_FLUSH        = 2,
    /* discard sectors */
    REQ_OP_DISCARD      = 3,
    /* get zone information */
    REQ_OP_ZONE_REPORT  = 4,
    /* securely erase sectors */
    REQ_OP_SECURE_ERASE = 5,
    /* seset a zone write pointer */
    REQ_OP_ZONE_RESET   = 6,
    /* write the same sector many times */
    REQ_OP_WRITE_SAME   = 7,
    /* write the zero filled sector many times */
    REQ_OP_WRITE_ZEROES = 9,

    /* SCSI passthrough using struct scsi_request */
    REQ_OP_SCSI_IN      = 32, 
    REQ_OP_SCSI_OUT     = 33, 
    /* Driver private requests */
    REQ_OP_DRV_IN       = 34, 
    REQ_OP_DRV_OUT      = 35, 

    REQ_OP_LAST,
};

typedef struct cheeze_req_user cheeze_ureq;
typedef struct cheeze_trace_req_user cheeze_trace_ureq;


void init_cheeze(uint64_t phy_addr);
void init_trace_cheeze();
void free_cheeze();
void free_trace_cheeze();
vec_request **get_vectored_request_arr();
//vec_request *get_vectored_request();
vec_request *jy_ureq2vec_req(char* art);
vec_request *get_trace_vectored_request();
bool jy_is_finished();
#endif
