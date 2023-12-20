#ifndef POSIX_HEADER
#define POSIX_HEADER
#include "../../include/container.h"

#define LASYNC
#define FS_LOWER_W 1
#define FS_LOWER_R 2
#define FS_LOWER_T 3

uint32_t posix_create(lower_info*,blockmanager *);
void *posix_destroy(lower_info*);
void* posix_push_data(uint32_t ppa, uint32_t size, value_set *value,bool async, algo_req * const req);
void* posix_pull_data(uint32_t ppa, uint32_t size, value_set* value,bool async,algo_req * const req);
void* posix_make_push(uint32_t ppa, uint32_t size, value_set *value,bool async, algo_req * const req);
void* posix_make_pull(uint32_t ppa, uint32_t size, value_set *value,bool async, algo_req * const req);
void* posix_badblock_checker(uint32_t ppa, uint32_t size, void*(*process)(uint64_t,uint8_t));
void* posix_trim_block(uint32_t ppa, bool async);
void* posix_trim_a_block(uint32_t ppa, bool async);
void *posix_make_trim(uint32_t ppa, bool async);
void *posix_refresh(lower_info*);
void posix_stop();
void posix_flying_req_wait();
void* posix_read_hw(uint32_t ppa, char *key,uint32_t key_len, value_set *value,bool async,algo_req * const req);
uint32_t posix_hw_do_merge(uint32_t lp_num, ppa_t *lp_array, uint32_t hp_num,ppa_t *hp_array,ppa_t *tp_array, uint32_t* ktable_num, uint32_t *invliadate_num);
char * posix_hw_get_kt();
char *posix_hw_get_inv();
uint32_t convert_ppa(uint32_t);

typedef struct posix_request {
	FSTYPE type;
	uint32_t key;
	value_set *value;
	algo_req *upper_req;
	bool isAsync;
	uint32_t size;
}posix_request;

typedef struct mem_seg {
	PTR storage;
} mem_seg;
#endif
