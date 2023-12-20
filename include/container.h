#ifndef __H_CONTAINER__
#define __H_CONTAINER__
 
#include "settings.h"
#include "types.h"
#include "utils.h"
#include "sem_lock.h"
#include "../interface/queue.h"
#include <stdarg.h>
#include <pthread.h>

typedef struct lower_info lower_info;
typedef struct algorithm algorithm;
typedef struct algo_req algo_req;
typedef struct request request;
typedef struct blockmanager blockmanager;

typedef struct upper_request{
	const FSTYPE type;
	const KEYT key;
	uint32_t length;
	V_PTR value;
	//anything
}upper_request;

typedef struct value_set{
	PTR value;
	uint32_t length;
	int dmatag; //-1 == not dma_alloc, others== dma_alloc
	uint32_t ppa;
	bool from_app;
	PTR rmw_value;
	uint8_t status;
	uint32_t len;
	uint32_t offset;
}value_set;


typedef struct vectored_request{
	uint32_t tag_id;
	uint32_t size;
	uint32_t done_cnt;
	uint32_t tid;
	char* buf;

	request *req_array;
	uint32_t mark;
	void* (*end_req)(void*);
	void* origin_req;
} vec_request;

struct request {
	FSTYPE type;
	KEYT key;
	uint32_t tag_num;
	uint32_t offset;
	uint32_t tid;
	uint32_t length;
	char *buf;
	
	uint32_t crc_value;
	uint64_t ppa;/*it can be the iter_idx*/
	uint32_t seq;
	uint32_t global_seq;
#ifdef hash_dftl
	volatile int num; /*length of requests*/
	volatile int cpl; /*number of completed requests*/
#endif
	int not_found_cnt;
	value_set *value;
	//value_set **multi_value;
	char **app_result;

	//KEYT *multi_key;
	bool (*end_req)(struct request *const);
	void *(*special_func)(void *);
	bool (*added_end_req)(struct request *const);
	bool isAsync;
	bool flush_all;
	uint8_t magic;
	void *param;
	void *__hash_node;
	//pthread_mutex_t async_mutex;
	//fdriver_lock_t sync_lock;
	int mark;
	bool is_sequential_start;
	uint32_t consecutive_length; 
	uint32_t round_cnt;

/*s:for application req
	char *target_buf;
	uint32_t inter_offset;
	uint32_t target_len;
	char istophalf;
	FSTYPE org_type;
e:for application req*/

	uint8_t type_ftl;
	uint8_t type_lower;
	uint8_t before_type_lower;
	bool isstart;
	MeasureTime latency_checker;

	/* HASH_KVSSD */
#ifdef hash_dftl
	void *hash_param;
#endif
	struct vectored_request *parents;
	bool is_board;
};

struct algo_req{
	uint32_t ppa;
	uint32_t test_ppa; //for lower layer
	request * parents;
	MeasureTime latency_lower;
	uint8_t type;
	bool rapid;
	uint8_t type_lower;
	//0: normal, 1 : no tag, 2: read delay 4:write delay
	void *(*end_req)(struct algo_req *const);
	void *param;
};

struct lower_info {
	uint32_t (*create)(struct lower_info*, blockmanager *bm);
	void* (*destroy)(struct lower_info*);
	void* (*write)(uint32_t ppa, uint32_t size, value_set *value,bool async,algo_req * const req);
	void* (*read)(uint32_t ppa, uint32_t size, value_set *value,bool async,algo_req * const req);
	void* (*device_badblock_checker)(uint32_t ppa,uint32_t size,void *(*process)(uint64_t, uint8_t));
	void* (*trim_block)(uint32_t ppa,bool async);
	void* (*trim_a_block)(uint32_t ppa,bool async);
	void* (*refresh)(struct lower_info*);
	void (*stop)();
	int (*lower_alloc) (int type, char** buf);
	void (*lower_free) (int type, int dmaTag);
	void (*lower_flying_req_wait) ();
	void (*lower_show_info)();
	uint32_t (*lower_tag_num)();
#ifdef Lsmtree
	void* (*read_hw)(uint32_t ppa, char *key,uint32_t key_len, value_set *value,bool async,algo_req * const req);
	uint32_t (*hw_do_merge)(uint32_t lp_num, ppa_t *lp_array, uint32_t hp_num,ppa_t *hp_array,ppa_t *tp_array, uint32_t* ktable_num, uint32_t *invliadate_num);
	char *(*hw_get_kt)();
	char *(*hw_get_inv)();
#endif
	struct blockmanager *bm;

	lower_status (*statusOfblock)(BLOCKT);
	
	uint64_t write_op;
	uint64_t read_op;
	uint64_t trim_op;

	uint32_t NOB;
	uint32_t NOP;
	uint32_t SOK;
	uint32_t SOB;
	uint32_t SOP;
	uint32_t PPB;
	uint32_t PPS;
	uint64_t TS;
	uint64_t DEV_SIZE;//for sacle up test
	uint64_t all_pages_in_dev;//for scale up test

	uint64_t req_type_cnt[LREQ_TYPE_NUM];
	//anything
};

struct algorithm{
	/*interface*/
	uint32_t (*argument_set) (int argc, char**argv);
	uint32_t (*create) (lower_info*, blockmanager *bm, struct algorithm *);
	void (*destroy) (lower_info*, struct algorithm *);
	uint32_t (*read)(request *const);
	uint32_t (*write)(request *const);
	uint32_t (*flush)(request *const);
	uint32_t (*remove)(request *const);
	uint32_t (*test)();
	lower_info* li;
	struct blockmanager *bm;
	void *algo_body;
};

typedef struct __OOBT{
	char d[128];
}__OOB;

typedef struct masterblock{
	uint32_t punit_num;
	uint32_t block_num;
	uint16_t now;
	uint16_t max;
	uint8_t* bitset;
	uint16_t invalidate_number;
	uint16_t validate_number;
	int age;
	uint32_t seg_idx;
	void *hptr;
	void *private_data;
	__OOB oob_list[_PPB];
}__block;

typedef struct mastersegment{
	uint32_t seg_idx;
	__block* blocks[BPS];
	uint16_t now;
	uint16_t max;
	uint32_t used_page_num;
	uint8_t invalid_blocks;
	void *private_data;
}__segment;

typedef struct ghostsegment{ //for gc
	bool all_invalid;
	__block* blocks[BPS];
	uint32_t seg_idx;
	uint16_t now;
	uint16_t max;
	uint32_t invalidate_number;
	uint32_t validate_number;
}__gsegment;

struct blockmanager{
	uint32_t (*create) (struct blockmanager*,lower_info *);
	void* (*q_return) (struct blockmanager*);
	uint32_t (*destroy) (struct blockmanager*);
	__block* (*get_block) (struct blockmanager*,__segment*);
	__block *(*pick_block)(struct blockmanager*, uint32_t page_num);
	__segment* (*get_segment) (struct blockmanager*, bool isreserve);
	__segment* (*jy_get_time_segment) (struct blockmanager*, bool isreserve, double time_stamp);
	uint32_t (*jy_get_block_idx) (struct blockmanager*, void* b);
	long (*jy_get_timestamp)(struct blockmanager* bm, uint32_t lba);
	int (*get_page_num)(struct blockmanager*, __segment*);
	int (*pick_page_num)(struct blockmanager*, __segment*);
	bool (*check_full)(struct blockmanager*, __segment*, uint8_t type);
	bool (*is_gc_needed) (struct blockmanager*);
	int (*get_free_segment_number) (struct blockmanager*);
	__gsegment* (*get_gc_target) (struct blockmanager*);
	__gsegment* (*jy_get_gc_target) (struct blockmanager*, queue*);
	void (*trim_segment) (struct blockmanager*, __gsegment*, struct lower_info*);
	void (*free_segment)(struct blockmanager *,__segment*);
	int (*populate_bit) (struct blockmanager*, uint32_t ppa);
	int (*unpopulate_bit) (struct blockmanager*, uint32_t ppa);
	bool (*query_bit) (struct blockmanager *, uint32_t ppa);
	int (*erase_bit)(struct blockmanager*, uint32_t ppa);
	bool (*is_valid_page) (struct blockmanager*, uint32_t ppa);
	bool (*is_invalid_page) (struct blockmanager*, uint32_t ppa);
	void (*set_oob)(struct blockmanager*, char* data, int len, uint32_t ppa);
	char *(*get_oob)(struct blockmanager*, uint32_t ppa);
	__segment* (*change_reserve)(struct blockmanager *, __segment *reserve);
	__segment* (*jy_change_reserve)(struct blockmanager *, __segment *reserve, double time_stamp);
	void (*jy_add_queue)(struct blockmanager*, queue* group_q, __segment* reserve);
	void (*jy_add_time_queue)(struct blockmanager* bm, queue* group_q, __segment *reserve, double timestamp);
	int (*jy_move_q2h)(struct blockmanager*, queue* q, int size);
	void (*reinsert_segment)(struct blockmanager *, uint32_t seg_idx);
	void (*jy_time_reinsert_segment)(struct blockmanager *, uint32_t seg_idx, double timestamp);
	uint32_t (*remain_free_page)(struct blockmanager *, __segment *active);
	void (*invalidate_number_decrease)(struct blockmanager *, uint32_t ppa);
	uint32_t (*get_invalidate_number)(struct blockmanager *, uint32_t seg_idx);
	uint32_t (*get_invalidate_blk_number)(struct blockmanager *);

	uint32_t (*pt_create) (struct blockmanager*, int part_num, int *each_part_seg_num, lower_info *);
	uint32_t (*pt_destroy) (struct blockmanager*);
	__segment* (*pt_get_segment) (struct blockmanager*, int pt_num, bool isreserve);
	__gsegment* (*pt_get_gc_target) (struct blockmanager*, int pt_num);
	void (*pt_trim_segment)(struct blockmanager*, int pt_num, __gsegment *, lower_info*);
	int (*pt_remain_page)(struct blockmanager*, __segment *active,int pt_num);
	bool (*pt_isgc_needed)(struct blockmanager*, int pt_num);
	__segment* (*change_pt_reserve)(struct blockmanager *,int pt_num, __segment *reserve);
	uint32_t (*pt_reserve_to_free)(struct blockmanager*, int pt_num, __segment *reserve);
	
	lower_info *li;
	void *private_data;
	uint32_t assigned_page;
};

#define SEGNUM(ppa)  (((ppa)/L2PGAP)/_PPS)
#define SEGOFFSET(ppa) (((ppa/L2PGAP))%_PPS)
#define SEGPIECEOFFSET(ppa) (((ppa))%(_PPS*L2PGAP))
#define BLOCKNUM(ppa)  ((((ppa)/L2PGAP)%_PPS)/_PPB)


#define for_each_block(segs,block,idx)\
	for(idx=0,block=segs->blocks[idx];idx<BPS; block=++idx>BPS?segs->blocks[idx-1]:segs->blocks[idx])

#define for_each_page(blocks,page,idx)\
	for(idx=0,page=blocks->ppa; idx!=PPB; page++,idx++)

#ifdef sequential
	#define PPAMAKER(bl,idx) ((bl)->block_num*_PPB+idx)
	#define for_each_page_in_seg(segs,page,bidx,pidx)\
		for(bidx=0; bidx<BPS; bidx++)\
			for(pidx=0, page=PPAMAKER(segs->blocks[bidx],pidx); pidx<_PPB; pidx++, page=PPAMAKER(segs->blocks[bidx],pidx))

	#define for_each_page_in_seg_blocks(segs,block,page,bidx,pidx)\
		for(bidx=0, block=segs->blocks[bidx]; bidx<BPS; bidx++, block=segs->blocks[(bidx!=BPS?bidx:BPS-1)])\
			for(pidx=0, page=PPAMAKER(segs->blocks[bidx],pidx); pidx<_PPB; pidx++, page=PPAMAKER(segs->blocks[bidx],pidx))
		
#else
	#define PPAMAKER(bl,idx) ((bl)->punit_num)+(idx<<6)+((bl)->block_num)
	#define for_each_page_in_seg(segs,page,bidx,pidx)\
		for(pidx=0;pidx<_PPB; pidx++)\
			for(bidx=0,page=PPAMAKER(segs->blocks[bidx],pidx); bidx<BPS; bidx++,page=PPAMAKER(segs->blocks[(bidx!=BPS?bidx:BPS-1)],pidx))


	#define for_each_page_in_seg_blocks(segs,block,page,bidx,pidx)\
		for(pidx=0;pidx<_PPB; pidx++)\
			for(bidx=0,block=segs->blocks[bidx],page=PPAMAKER(segs->blocks[bidx],pidx); bidx<BPS; bidx++,page=PPAMAKER(segs->blocks[(bidx!=BPS?bidx:BPS-1)],pidx),block=segs->blocks[(bidx!=BPS?bidx:BPS-1)])
#endif



#endif
