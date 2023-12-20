#include "cheeze_hg_block.h"
#include "queue.h"
#include "interface.h"
#include "threading.h"
#include "../bench/bench.h"
#include "vectored_interface.h"
#include "../include/utils/crc32.h"
#include <pthread.h>
#include <time.h>

extern master_processor mp;

#define TOTAL_SIZE (3ULL *1024L *1024L *1024L)
#define TRACE_DEV_SIZE (128ULL * 1024L * 1024L * 1024L)
#define CRC_BUFSIZE (2ULL * 1024L * 1024L)

static uint64_t PHYS_ADDR=0x3800000000;
static void *page_addr;
static uint8_t *send_event_addr; // CHEEZE_QUEUE_SIZE ==> 16B
static uint8_t *recv_event_addr; // 16B
static uint64_t *seq_addr; // 8KB
struct cheeze_req_user *ureq_addr; // sizeof(req) * 1024
static char *data_addr[2]; // page_addr[1]: 1GB, page_addr[2]: 1GB
//static uint64_t seq = 0;
static int trace_fd = 0;

static long jy_req_start=0;
static long jy_req_end=0;

time_t time_begin;
time_t time_tmp;

static int id_req=0;
//static uint32_t trace_crc[TRACE_DEV_SIZE/LPAGESIZE];
//static uint32_t trace_crc_buf[CRC_BUFSIZE];

extern pthread_mutex_t req_cnt_lock;

static inline char *get_buf_addr(char **pdata_addr, int id) {
    int idx = id / ITEMS_PER_HP;
    return pdata_addr[idx] + ((id % ITEMS_PER_HP) * CHEEZE_BUF_SIZE);
}

static void shm_meta_init(void *ppage_addr) {
    memset(ppage_addr, 0, (1ULL * 1024 * 1024 * 1024));
    send_event_addr = (uint8_t*)((char*)ppage_addr + SEND_OFF); // CHEEZE_QUEUE_SIZE ==> 16B
    recv_event_addr =(uint8_t*)((char*)ppage_addr + RECV_OFF); // 16B
    seq_addr = (uint64_t*)((char*)ppage_addr + SEQ_OFF); // 8KB
    ureq_addr = (cheeze_req_user*)((char*)ppage_addr + REQS_OFF); // sizeof(req) * 1024
}

static void shm_data_init(void *ppage_addr) {
    data_addr[0] = ((char *)ppage_addr) + (1ULL * 1024 * 1024 * 1024);
    data_addr[1] = ((char *)ppage_addr) + (2ULL * 1024 * 1024 * 1024);
}


bool cheeze_end_req(request *const req);
bool jeeyun_end_req(request *const req);
char *null_value;

#if defined(CHECKINGDATA) || defined(TRACE_REPLAY)
uint32_t* CRCMAP;
#endif

#ifdef TRACE_COLLECT
uint32_t* crc_buffer;
#endif

#define TRACE_TARGET "/trace"
void init_trace_cheeze(){
	trace_fd = open(TRACE_TARGET, O_RDONLY);
	if (trace_fd < 0) {
		perror("Failed to open " TRACE_TARGET);
		abort();
		return;
	}

	null_value = (char*)malloc(PAGESIZE);
	memset(null_value, 0, PAGESIZE);

#if defined(CHECKINGDATA) || defined(TRACE_REPLAY)
	CRCMAP=(uint32_t*)malloc(sizeof(uint32_t) * RANGE);
	memset(CRCMAP, 0, sizeof(uint32_t) *RANGE);
#endif
}

void init_cheeze(uint64_t phy_addr){
	int chrfd = open("/dev/mem", O_RDWR);
	if (chrfd < 0) {
		perror("Failed to open /dev/mem");
		abort();
		return;
	}
	
	if(phy_addr){
		PHYS_ADDR=phy_addr;
	}

	uint64_t pagesize, addr, len;
	pagesize=getpagesize();
	addr = PHYS_ADDR & (~(pagesize - 1));
    len = (PHYS_ADDR & (pagesize - 1)) + TOTAL_SIZE;

	page_addr = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, chrfd, addr);
    if (page_addr == MAP_FAILED) {
        perror("Failed to mmap plain device path");
        exit(1);
    }
	close(chrfd);

	shm_meta_init(page_addr);
	shm_data_init(page_addr);
	
	null_value=(char*)malloc(PAGESIZE);
	memset(null_value,0,PAGESIZE);

#if defined(CHECKINGDATA) || defined(TRACE_REPLAY)
	CRCMAP=(uint32_t*)malloc(sizeof(uint32_t) * RANGE);
	memset(CRCMAP, 0, sizeof(uint32_t) *RANGE);
#endif

#ifdef TRACE_COLLECT
	trace_fd=open(TRACE_TARGET, 0x666, O_CREAT | O_WRONLY | O_TRUNC);
	if (trace_fd < 0) {
		perror("Failed to open " TRACE_TARGET);
		abort();
		return;
	}
	crc_buffer=(uint32_t*)malloc(sizeof(uint32_t) * 512);
#endif
}

inline void error_check(cheeze_ureq *creq){
	if(unlikely(creq->len%LPAGESIZE)){
		printf("size not align %s:%d\n", __FILE__, __LINE__);
		abort();
	}
}

inline FSTYPE decode_type(int op){
	switch(op){
		case REQ_OP_READ: return FS_GET_T;
		case REQ_OP_WRITE: return FS_SET_T;
		case REQ_OP_FLUSH: return FS_FLUSH_T;
		case REQ_OP_DISCARD: return FS_DELETE_T;
		default:
			printf("not valid type!\n");
			abort();
	}
	return 1;
}

const char *type_to_str(uint32_t type){
	switch(type){
		case FS_GET_T: return "FS_GET_T";
		case FS_SET_T: return "FS_SET_T";
		case FS_FLUSH_T: return "FS_FLUSH_T";
		case FS_DELETE_T: return "FS_DELETE_T";
	}
	return NULL;
}

static inline vec_request *ch_ureq2vec_req(cheeze_ureq *creq, int id){
	vec_request *res=(vec_request *)calloc(1, sizeof(vec_request));
	res->tag_id=id;

	error_check(creq);
//++
	res->origin_req=(void*)creq;
	res->size=creq->len/LPAGESIZE;
	res->req_array=(request*)calloc(res->size,sizeof(request));
	res->end_req=NULL;
	res->mark=0;

	FSTYPE type=decode_type(creq->op);
	if(type!=FS_GET_T && type!=FS_SET_T){
		res->buf=NULL;
	}
	else{
#ifdef TRACE_REPLAY
		res->buf=NULL;	
#else
		res->buf=get_buf_addr(data_addr, id);
#endif
	}


	if(res->size > QSIZE){
		printf("----------------number of reqs is over %u < %u\n", QSIZE, res->size);
		abort();
		return NULL;
	}
	uint32_t prev_lba=UINT32_MAX;
	uint32_t consecutive_cnt=0;
	static uint32_t global_seq=0;

	for(uint32_t i=0; i<res->size; i++){
		request *temp=&res->req_array[i];
		temp->parents=res;
		temp->type=type;
		temp->end_req=cheeze_end_req;
		temp->isAsync=ASYNC;
		temp->seq=i;
		temp->type_ftl=0;
		temp->type_lower=0;
		temp->is_sequential_start=false;
		temp->flush_all=0;
		temp->global_seq=global_seq++;
		switch(type){
			case FS_GET_T:
				temp->value=inf_get_valueset(NULL, FS_MALLOC_R, PAGESIZE);
				break;
			case FS_SET_T:
#ifdef TRACE_REPLAY
				temp->value=inf_get_valueset(NULL, FS_MALLOC_W, PAGESIZE);
#else
				temp->value=inf_get_valueset(&res->buf[LPAGESIZE*i],FS_MALLOC_W,PAGESIZE);
#endif
				break;	
			case FS_FLUSH_T:
				break;
			case FS_DELETE_T:
				break;
			default:
				printf("error type!\n");
				abort();
				break;
		}
		temp->key=creq->pos+i;

		if(prev_lba==UINT32_MAX){
			prev_lba=temp->key;
		}
		else{
			if(prev_lba+1==temp->key){
				consecutive_cnt++;
			}
			else{
				res->req_array[i-consecutive_cnt-1].is_sequential_start=(consecutive_cnt!=0);
				res->req_array[i-consecutive_cnt-1].consecutive_length=consecutive_cnt;
				consecutive_cnt=0;
			}
			prev_lba=temp->key;
			temp->consecutive_length=0;
		}

	//	printf("start REQ-TYPE:%s INFO(%d:%d) LBA: %u crc:%u\n", type_to_str(temp->type),creq->id, i, temp->key, temp->crc_value);
#ifdef TRACE_REPLAY
		if(temp->type==FS_SET_T){
			read(trace_fd, &CRCMAP[temp->key],sizeof(uint32_t));
			if(temp->key==0){
				printf("0 crc value:%u\n", CRCMAP[temp->key]);
			}
			*(uint32_t*)temp->value->value=CRCMAP[temp->key];
		}
		else{
			uint32_t temp_crc;
			read(trace_fd, &temp_crc, sizeof(uint32_t));
			temp->crc_value=CRCMAP[temp->key];
		}
#endif

#ifdef CHECKINGDATA
		if(temp->type==FS_SET_T){
			CRCMAP[temp->key]=crc32(&res->buf[LPAGESIZE*i],LPAGESIZE);	
		}
		else if(temp->type==FS_DELETE_T){
	//		CRCMAP[temp->key]=crc32(null_value, LPAGESIZE);
		}
		else if(temp->type==FS_GET_T){
			temp->crc_value=CRCMAP[temp->key];
		}
#endif


#ifdef TRACE_COLLECT
		if(temp->type==FS_SET_T){
			crc_buffer[i]=crc32(temp->value->value, LPAGESIZE);
		}
#endif
		DPRINTF("[START] REQ-TYPE:%s INFO(%d:%d) LBA: %u\n", type_to_str(temp->type),creq->id, i, temp->key);
	}

	res->req_array[(res->size-1)-consecutive_cnt].is_sequential_start=(consecutive_cnt!=0);
	res->req_array[(res->size-1)-consecutive_cnt].consecutive_length=consecutive_cnt;
	return res;
}

char load_signal=0;
vec_request *jy_ureq2vec_req(char* request_raw) {
	uint32_t lba_r=0;
	uint32_t size_r=0;
	char* load_sig = "LOAD";
	//uint32_t out1_max = 1583789;
	//uint32_t out2_max = 7045835;
	//uint32_t out_384_max = 8982377;
//	uint32_t out_384_2_max = 31642273;
//	uint32_t tmp_max = 100663295;
//	uint32_t tmp2_max = 100663296;
//	uint32_t zip_08=201326592;
	//uint32_t zip_32=41943139;
	//printf("request: %s\n", request_raw);
	if (strstr(request_raw, load_sig)) {
		load_signal=1;
		return NULL;	
	}
	vec_request *res=(vec_request *)calloc(1, sizeof(vec_request));
	res->tag_id=id_req++;
	//cheeze_ureq *creq = ureq_addr+id_req;
	//TODO check if it is okay
	FSTYPE type;
	//char *write="W";
	//char *read="R";
	//printf("type: %s\n", tmp);
	//float perct = (float)id_req*(float)100/(float)zip_32;
	//if (id_req%1000==0) printf("\rpercent: %f%%", perct);
	//if (perct > 40) exit(0);
	//if (id_req%10000000==0) printf("\n");
	/*
	if (strchr(tmp, 'W')) type=FS_SET_T;
	else if (strchr(tmp, 'R')) type=FS_GET_T;
	else {
		printf("type err\n");
		abort();
	}
	*/
	char *tmp=strtok(request_raw, " \t");
	tmp = strtok(NULL, " \t");
	int ttp = atoi(tmp);
	if (ttp == 0) type=FS_GET_T;
	else if (ttp == 1) type = FS_SET_T;
	else if (ttp == 2) type = FS_FLUSH_T;
	else if (ttp == 3) type = FS_DELETE_T;
	else {
		printf("type err\n");
		abort();
	}
	if (ttp > 1) return NULL;

	lba_r = atoi(strtok(NULL, " \t"));
	size_r = atoi(strtok(NULL, " \t"));
	//printf("size: %d\n", size_r);
	res->origin_req=NULL;
	res->size=size_r/LPAGESIZE;
	res->req_array=(request*)calloc(res->size, sizeof(request));
	res->end_req=NULL;
	res->mark=0;

	res->buf=NULL;

	uint32_t prev_lba=UINT32_MAX;
	uint32_t consecutive_cnt=0;
	static uint32_t global_seq=0;
	for (uint32_t i=0; i<res->size; i++) {
		request *temp=&res->req_array[i];
		temp->parents=res;
		temp->type=type;
		//TODO make end request
		temp->end_req=jeeyun_end_req;
		temp->isAsync=ASYNC;
		temp->seq=i;
		temp->type_ftl=0;
		temp->type_lower=0;
		temp->is_sequential_start=false;
		temp->flush_all=0;
		temp->global_seq=global_seq++;
		switch(type) {
			case FS_GET_T:
				temp->value=inf_get_valueset(NULL, FS_MALLOC_R, PAGESIZE);
				break;
			case FS_SET_T:
		temp->value=inf_get_valueset(NULL, FS_MALLOC_W, PAGESIZE);
				break;
			default:
				printf("error type!\n");
				abort();
				break;
		}
		temp->key=lba_r+i;

		if (prev_lba == UINT32_MAX) {
			prev_lba = temp->key;
		} else {
			if (prev_lba+1==temp->key) {
				consecutive_cnt++;
			} else {
				res->req_array[i-consecutive_cnt-1].is_sequential_start=(consecutive_cnt!=0);
				res->req_array[i-consecutive_cnt-1].consecutive_length=consecutive_cnt;
			}
			prev_lba = temp->key;
			temp->consecutive_length=0;
		}

	}
	res->req_array[(res->size-1)-consecutive_cnt].is_sequential_start=(consecutive_cnt!=0);
	res->req_array[(res->size-1)-consecutive_cnt].consecutive_length=consecutive_cnt;
	++jy_req_start;
	return res;

}
extern volatile vectored_request *now_processing;

bool jeeyun_end_req(request *const req) {
	if (load_signal) {
		//printf("\nTRIM: %ld\n", (long) mp.li->req_type_cnt[0]);
		//printf("MAPPINGW: %ld\n", (long) mp.li->req_type_cnt[2]);
		//printf("GCMW: %ld\n", (long) mp.li->req_type_cnt[4]);
		//printf("DATAW: %ld\n", (long) mp.li->req_type_cnt[6]);
		//printf("GCDW: %ld\n", (long) mp.li->req_type_cnt[8]);
		load_signal=0;
	}
	vectored_request *preq=req->parents;
	switch(req->type) {
		case FS_NOTFOUND_T:
			bench_reap_data(req, mp.li);
			DPRINTF("%u not fount!\n", req->key);
			inf_free_valueset(req->value, FS_MALLOC_R);
			break;
		case FS_GET_T:
			bench_reap_data(req, mp.li);
			if(req->value) {
				inf_free_valueset(req->value, FS_MALLOC_R);
			}
			break;
		case FS_SET_T:
			bench_reap_data(req, mp.li);
			if(req->value) {
				inf_free_valueset(req->value, FS_MALLOC_W);  
			}
			break;
		case FS_FLUSH_T:
		case FS_DELETE_T:
			break;
		default:
			abort();
	}
	pthread_mutex_lock(&req_cnt_lock);
	preq->done_cnt++;
	release_each_req(req);
	if (preq->size == preq->done_cnt) {
		free(preq->req_array);
		now_processing=NULL;
		free(preq);
		++jy_req_end;
		if (jy_req_end % (100*1024*1024/4) == 0) {
			time(&time_tmp);
			//printf("[THROUGHPUT] 100GB per %0.0f sec (cur: %ldGB)\n", difftime(time_tmp, time_begin), jy_req_end/(1024*1024/4));
			printf("[THROUGHPUT] %0.0f MB/sec (current progress: %ldGB)\n", 
					100.0*1024.0/difftime(time_tmp, time_begin), jy_req_end/(1024*1024/4));
			time(&time_begin);
		}
	}
	pthread_mutex_unlock(&req_cnt_lock);
	return true;
}


bool jy_is_finished() {
        if (jy_req_start == jy_req_end) return true;
        else return false;
}


//extern int MS_TIME_SL;
//#define MS_TIME_SL 7
//set to time!!
//#define SLICE (32 * 1024)
vec_request **get_vectored_request_arr()
{
	static bool isstart = false;
	if (!isstart) {
		isstart = true;
		printf("now waiting req!!\n");
	}
	cheeze_ureq *ureq;
	vec_request **res = NULL;
	volatile uint8_t *send, *recv;
	bool check[CHEEZE_QUEUE_SIZE] = { 0, };
	int id;
	int check_idx = 0;
	int pre_i;
	uint32_t total_size = 0;

 retry:
	check_idx = 0;
	total_size = 0;
	memset(check, 0, sizeof(check));
	for (pre_i = 0; pre_i < CHEEZE_QUEUE_SIZE; pre_i++) {
		send = &send_event_addr[pre_i];
		if (*send) {
			id = pre_i;
			check[pre_i] = true;
			check_idx++;
			ureq = ureq_addr + id;
			total_size += ureq->len;
		}
	}
	if (!check_idx)
		goto retry;

#if 0
	uint32_t tmp = total_size / SLICE * MS_TIME_SL;
	if (!tmp)
		tmp = 1;
	static int debug_cnt = 0;
	if (++debug_cnt % 100000 == 0) {
		printf("tmp:%u\n", tmp);
	}
	usleep(tmp);
#endif

	int req_idx = 0;
	static int cnt=0;
	res = (vec_request **) malloc(sizeof(vec_request *) * (check_idx + 1));
	for (int i = 0; i < CHEEZE_QUEUE_SIZE; i++) {
		if (!check[i])
			continue;
		send = &send_event_addr[i];
		recv = &recv_event_addr[i];
		id = i;
		ureq = ureq_addr + id;
		res[req_idx++] = ch_ureq2vec_req(ureq, id);
		if(cnt%10000){
			printf("%u\n", cnt++);
		}
#ifdef TRACE_COLLECT
		write(trace_fd, ureq, sizeof(cheeze_req_user));
		write(trace_fd, crc_buffer, ureq->len/LPAGESIZE * sizeof(uint32_t));
#endif
		barrier();
		*send = 0;
		if (ureq->op!=REQ_OP_READ) {
			barrier();
			*recv = 1;
		}
	}
	res[req_idx] = NULL;
	return res;
}

vec_request *get_trace_vectored_request(){
	static bool isstart=false;
	//unsigned int crc_len;

	
	if(!isstart){
		isstart=true;
		printf("now waiting req!!\n");
		fsync(1);
		fsync(2);
	}

	cheeze_ureq ureq;
	vec_request *res=NULL;
	int id=0;
	static int cnt=1;
	while(1){
		uint32_t len=0;
		len=read(trace_fd, &ureq, sizeof(ureq));
		if(len!=sizeof(ureq)){
			printf("read error!!: len:%u\n", len);
			break;
			abort();
		}
		res=ch_ureq2vec_req(&ureq, id);
		cnt++;
		if(cnt%1000==0){
			printf("%u\n",cnt);
		}
		return res;
	}

	return res;
}


bool cheeze_end_req(request *const req){
	vectored_request *preq=req->parents;
	//uint32_t temp_crc;
	switch(req->type){
		case FS_NOTFOUND_T:
			bench_reap_data(req, mp.li);
			DPRINTF("%u not found!\n",req->key);
			if(preq->buf){
				memcpy(&preq->buf[req->seq*LPAGESIZE], null_value,LPAGESIZE);
			}
			inf_free_valueset(req->value,FS_MALLOC_R);
			break;
		case FS_GET_T:
			bench_reap_data(req, mp.li);
			if(req->value){

				if(preq->buf){
					memcpy(&preq->buf[req->seq*LPAGESIZE], req->value->value,LPAGESIZE);
				}
#ifdef TRACE_REPLAY
			if(req->crc_value!=*(uint32_t*)req->value->value){
				printf("lba:%u data faile abort!\n", req->key);
				abort();
			}
#endif

#ifdef CHECKINGDATA
				temp_crc=crc32(&preq->buf[req->seq*LPAGESIZE], LPAGESIZE);
				if(req->crc_value!=temp_crc){
					printf("\n");
					printf("\t\tfound - crc checking error in key:%u %u -> org:%u\n", req->key, temp_crc, CRCMAP[req->key]);	
					abort();
				}
				if(req->key==0){
					printf("target 0 crc:%u\n", CRCMAP[req->key]);
				}
#endif
				inf_free_valueset(req->value,FS_MALLOC_R);
			}
			break;
		case FS_SET_T:
			bench_reap_data(req, mp.li);
			if(req->value) inf_free_valueset(req->value, FS_MALLOC_W);
			break;
		case FS_FLUSH_T:
		case FS_DELETE_T:
			break;
		default:
			abort();
	}

	pthread_mutex_lock(&req_cnt_lock);
	preq->done_cnt++;

#ifdef DEBUG
	cheeze_ureq *creq=(cheeze_ureq*)preq->origin_req;
	DPRINTF("[END]REQ-TYPE:%s INFO(%d:%d) LBA: %u -> preq-done:%u/%u\n", type_to_str(req->type),creq->id, req->seq, req->key, preq->done_cnt, preq->size);
#endif

	release_each_req(req);
	if(preq->size==preq->done_cnt){
#ifdef TRACE_REPLAY

#else
		if(req->type==FS_GET_T || req->type==FS_NOTFOUND_T){
			recv_event_addr[preq->tag_id]=1;		
		}
#endif
		free(preq->req_array);
		now_processing=NULL;
		free(preq);
	}
	pthread_mutex_unlock(&req_cnt_lock);

	return true;
}

void free_cheeze(){
#ifdef CHECKINGDATA
	free(CRCMAP);
#endif
	return;
}

void free_trace_cheeze(){
#ifdef CHECKINGDATA
	free(CRCMAP);
#endif
	free(null_value);
	close(trace_fd);
	return;
}

