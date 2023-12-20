/*
 * busexmp - example memory-based block device using BUSE
 * Copyright (C) 2013 Adam Cozzette
 *
 * This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <argp.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/nbd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>

#include "buse.h"
#include "queue.h"
#include "../bench/bench.h"
#include "../bench/measurement.h"
#include "../include/utils/cond_lock.h"

/* BUSE callbacks */
//static void *data;

extern master *_master;

#ifdef BUSE_MEASURE
MeasureTime buseTime;
MeasureTime buseendTime;
#endif

struct nbd_reply end_reply;
extern int g_sk;

static queue *request_q;
static queue *reply_q;
cl_lock *buse_flying;
cl_lock *buse_end_flying;


#if (BUSE_ASYNC==1)
static void* buse_end_request(void* buse_req){
    while(1){
        if(q_enqueue(buse_req, reply_q)){
            cl_release(buse_end_flying);
            break;
        }
    }
    //while(!q_enqueue(buse_req, reply_q));
    return NULL;
}
#endif

#if (BUSE_ASYNC==1)
void* buse_reply(void *args){
#elif (BUSE_ASYNC==0)
void* buse_end_request(void *args){
#endif
    //FIXME : run this as another thread
    struct buse *buse_req=(struct buse*)args;
#ifdef BUSE_MEASURE
    if(buse_req->type==FS_BUSE_R)
        MS(&buseendTime);
#endif
    u_int32_t len=buse_req->len;
    value_set *value=buse_req->value;
    void *chunk=buse_req->chunk;
    int flag=((buse_req->offset)+(buse_req->i_len))==len?1:0;
    //int cmp;
    //int diff_cnt=0;

    chunk+=buse_req->offset;
    if(buse_req->i_len!=PAGESIZE && buse_req->type==FS_BUSE_R){
        //printf("chunk : %p, value : %p, i_offset : %d, i_len : %u\n", chunk, value, buse_req->i_offset, buse_req->i_len);
        memcpy(chunk,(value->value)+(buse_req->i_offset),buse_req->i_len);
    }
    if(value){
        inf_free_valueset(value, buse_req->type);
    }
    if(!flag){
#ifdef BUSE_MEASURE
        if(buse_req->type==FS_BUSE_R)
            MA(&buseendTime);
#endif
        free(buse_req);
        return NULL;
    }

    memcpy(end_reply.handle,buse_req->handle,8);
    //memcpy(reply.handle,"10000000",8);
    //printf("reply handle : %s\n", end_reply.handle);
    write_all(g_sk,(char*)&end_reply,sizeof(struct nbd_reply));
    chunk-=(buse_req->offset);
    switch(buse_req->type){
        case FS_BUSE_R:
            write_all(g_sk,(char*)chunk,len);
            free(chunk);
            break;

        case FS_BUSE_W:
            free(chunk);
            break;

        case FS_DELETE_T:
            break;

        default:
            break;
    }
#ifdef BUSE_MEASURE
    if(buse_req->type==FS_BUSE_R)
        MA(&buseendTime);
#endif
    free(buse_req);
    return NULL;

    /*
    if(buse_req->type==FS_BUSE_R){
        memcpy(chunk, value->value, PAGESIZE);
    }

    if(flag){
        //printf("reply : %d, %u\n", buse_req->offset, (unsigned int)(buse_req->len)*PAGESIZE);
        write_all(sk,reply,sizeof(struct nbd_reply));
        chunk=chunk-(PAGESIZE*(buse_req->idx));
        if(buse_req->type==FS_BUSE_R){
            write_all(sk,(char*)chunk, len);
        }
        if(buse_req->type!=FS_DELETE_T)
            free(chunk);
        free(reply);
    }

    if(value){
        if(buse_req->type==FS_BUSE_R)
            inf_free_valueset(value, 0);
        if(buse_req->type==FS_BUSE_W)
            inf_free_valueset(value, 1);
    }

    free(buse_req);
    return NULL;
    */
}

static int buse_io(char _type, int sk, void *buf, u_int32_t len, u_int64_t offset, void *reply)
{
    //FIXME : only support read/write unit of PAGESIZE
    u_int32_t remain=len;
    u_int64_t target=offset;
    int i_offset=target%PAGESIZE;
    int i_len=len<PAGESIZE?len:PAGESIZE-i_offset;
    int b_offset=0;
    static u_int64_t write_count = 0;
    struct buse *buse_req;
    struct buse *temp_buse_req;
    void *temp_chunk;

    while(remain){
        buse_req=(struct buse*)malloc(sizeof(struct buse));
        *buse_req=(struct buse){
            .sk=sk, 
            .len=len,
            .chunk=buf,
            .type=_type,
            .reply=reply,
            .log_offset=offset,
            .offset=b_offset,
            .i_offset=i_offset,
            .write_check=false,
            .i_len=i_len
        };
        memcpy(buse_req->handle,(char*)reply,8);
#ifdef BUSE_MEASURE
        if(_type==FS_BUSE_R)
            MA(&buseTime);
#endif
        inf_make_req_fromApp(_type,target/PAGESIZE,0,i_len,buf+b_offset,(void*)buse_req,buse_end_request);
#ifdef BUSE_MEASURE
        if(_type==FS_BUSE_R)
            MS(&buseTime);
#endif
        remain-=i_len;
        target+=i_len;
        i_offset=target%PAGESIZE;
        i_len=remain<PAGESIZE?remain:PAGESIZE-i_offset;
        b_offset+=i_len;
    }

    return 0;
}

static int buse_make_read(int sk, void *buf, u_int32_t len, u_int64_t offset, void *userdata)
{
    struct buse_request *buse_req; 
    buse_req = (struct buse_request*)malloc(sizeof(struct buse_request));
    *buse_req = (struct buse_request){
        .type = FS_BUSE_R,
        .buf = buf,
        .len = len,
        .offset = offset
    };
    memcpy(buse_req->handle, userdata, sizeof(buse_req->handle));
    while(1){
        if(q_enqueue((void*)buse_req, request_q)){
            cl_release(buse_flying);
            break;
        }
    }
    return 0;
}

static int buse_make_write(int sk, const void *buf, u_int32_t len, u_int64_t offset, void *userdata)
{
    struct buse_request *buse_req; 
    buse_req = (struct buse_request*)malloc(sizeof(struct buse_request));
    *buse_req = (struct buse_request){
        .type = FS_BUSE_W,
        .buf = buf,
        .len = len,
        .offset = offset
    };
    memcpy(buse_req->handle, userdata, sizeof(buse_req->handle));
    while(1){
        if(q_enqueue((void*)buse_req, request_q)){
            cl_release(buse_flying);
            break;
        }
    }
    //while(!q_enqueue((void*)buse_req, request_q));
    return 0;
}

static int buse_make_trim(int sk, u_int64_t from, u_int32_t len, void *userdata)
{
    struct buse_request *buse_req; 
    buse_req = (struct buse_request*)malloc(sizeof(struct buse_request));
    *buse_req = (struct buse_request){
        .type = FS_DELETE_T,
        .buf = NULL,
        .len = len,
        .offset = from
    };
    memcpy(buse_req->handle, userdata, sizeof(buse_req->handle));
    while(1){
        if(q_enqueue((void*)buse_req, request_q)){
            cl_release(buse_flying);
            break;
        }
    }
    return 0;
}

static int buse_read(int sk, void *buf, u_int32_t len, u_int64_t offset, void *userdata)
{
    //if (*(int *)userdata)
    //fprintf(stdout, "R - %lu, %u\n", offset/PAGESIZE, len);
    //printf("R - handle : %s\n", (char*)userdata);
    
    buse_io(FS_BUSE_R,sk,buf,len,offset,userdata);

    return 0;
}

static int buse_write(int sk, const void *buf, u_int32_t len, u_int64_t offset, void *userdata)
{
    //if (*(int *)userdata)
    //fprintf(stdout, "W - %lu, %u\n", offset/PAGESIZE, len);
    //printf("W - handle : %s\n", (char*)userdata);

    buse_io(FS_BUSE_W,sk,buf,len,offset,userdata);

    return 0;
}

static void buse_disc(int sk, void *userdata)
{
    //if (*(int *)userdata)
    //fprintf(stdout, "Received a disconnect request.\n");
}

static int buse_flush(int sk, void *userdata)
{
    //if (*(int *)userdata)
    //fprintf(stdout, "Received a flush request.\n");
    return 0;
}

static int buse_trim(int sk, u_int64_t from, u_int32_t len, void *userdata)
{
    //if (*(int *)userdata)
    //fprintf(stdout, "T - %lu, %u\n", from/PAGESIZE, len);

    buse_io(FS_DELETE_T,sk,NULL,len,from,userdata);

    return 0;
}

/* argument parsing using argp */

static struct argp_option options[] = {
    {"verbose", 'v', 0, 0, "Produce verbose output", 0},
    {0},
};

struct arguments {
    unsigned long long size;
    char * device;
    int verbose;
};

static unsigned long long strtoull_with_prefix(const char * str, char * * end) {
    unsigned long long v = strtoull(str, end, 0);
    switch (**end) {
        case 'K':
            v *= 1024;
            *end += 1;
            break;
        case 'M':
            v *= 1024 * 1024;
            *end += 1;
            break;
        case 'G':
            v *= 1024 * 1024 * 1024;
            *end += 1;
            break;
    }
    return v;
}

/* Parse a single option. */
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = state->input;
    char * endptr;

    switch (key) {

        case 'v':
            arguments->verbose = 1;
            break;

        case ARGP_KEY_ARG:
            switch (state->arg_num) {

                /*
                   case 0:
                   arguments->size = strtoull_with_prefix(arg, &endptr);
                   if (*endptr != '\0') {
                // failed to parse integer
                errx(EXIT_FAILURE, "SIZE must be an integer");
                }
                break;
                */

                //case 1:
                case 0:
                    arguments->device = arg;
                    break;

                default:
                    /* Too many arguments. */
                    return ARGP_ERR_UNKNOWN;
            }
            break;

        case ARGP_KEY_END:
            if (state->arg_num < 1) {
                warnx("not enough arguments");
                argp_usage(state);
            }
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = {
    .options = options,
    .parser = parse_opt,
    .args_doc = "DEVICE",
    .doc = "BUSE virtual block device that stores its content in memory.\n"
        "`DEVICE` is path to block device, for example \"/dev/nbd0\".",
};

#if (BUSE_ASYNC==1)
void* buse_request_main(void* args){
    struct buse_request *buse_req;
    while(1){
		cl_grap(buse_flying);
        buse_req = (struct buse_request*)q_dequeue(request_q);
        if(!buse_req)
            continue;
        buse_io(buse_req->type, g_sk, buse_req->buf, buse_req->len, buse_req->offset, buse_req->handle);
        free(buse_req);
        //usleep(10000);
    }
    return NULL;
}

void* buse_reply_main(void* args){
    void *buse_req;
    while(1){
		cl_grap(buse_end_flying);
        buse_req = q_dequeue(reply_q);
        if(!buse_req)
            continue;
        buse_reply(buse_req);
        //usleep(10000);
    }
    return NULL;
}
#endif

int main(int argc, char *argv[]) {
    pthread_t tid[2];
    struct arguments arguments = {
        .verbose = 0,
    };
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    struct buse_operations aop = {
#if (BUSE_ASYNC==1)
        .read = buse_make_read,
        .write = buse_make_write,
        .trim = buse_make_trim,
#elif (BUSE_ASYNC==0)
        .read = buse_read,
        .write = buse_write,
        .trim = buse_trim,
#endif
        //.size = arguments.size,
        .disc = buse_disc,
        .flush = buse_flush,
        .size = TOTALSIZE,
    };

    /*
       data = malloc(aop.size);
       if (data == NULL) err(EXIT_FAILURE, "failed to alloc space for data");
    */

    //memset(rep,0,sizeof(struct replies)*10);
    inf_init();
    //bench_init();
    //measure_init(&totTime);
#ifdef BUSE_MEASURE
    measure_init(&buseTime);
    measure_init(&buseendTime);
#endif
    end_reply.magic = htonl(NBD_REPLY_MAGIC);
    end_reply.error = htonl(0);
#if (BUSE_ASYNC==1)
    q_init(&request_q, QSIZE);
    q_init(&reply_q, QSIZE);
	buse_flying=cl_init(QDEPTH*2,true);
	buse_end_flying=cl_init(QDEPTH*2,true);
    pthread_create(&tid[0], NULL, buse_request_main, NULL);
    pthread_create(&tid[1], NULL, buse_reply_main, NULL);
#endif
    buse_main(arguments.device, &aop, (void *)&arguments.verbose);
#if (BUSE_ASYNC==1)
    pthread_cancel(tid[0]);
    pthread_cancel(tid[1]);
    q_free(request_q);
    q_free(reply_q);
#endif

    inf_free();
#ifdef BUSE_MEASURE
    printf("buseTime : ");
    measure_adding_print(&buseTime);
    printf("buseendTime : ");
    measure_adding_print(&buseendTime);
#endif
    return 0;
}

