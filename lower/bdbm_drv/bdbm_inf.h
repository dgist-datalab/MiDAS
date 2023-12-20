#include "../../include/settings.h"
#include "../../bench/bench.h"
#include "../../bench/measurement.h"
#include "../../include/container.h"
#include "../../include/sem_lock.h"

typedef struct merge_params{
	fdriver_lock_t merge_lock;
	uint32_t *ktable_num;
	uint32_t invalidate_num;
}merge_params;

uint32_t memio_info_create(lower_info *li,blockmanager *bm);
void *memio_info_destroy(lower_info *li);
void *memio_info_push_data(uint32_t ppa, uint32_t size, value_set* value, bool async, algo_req *const req);
void *memio_info_pull_data(uint32_t ppa, uint32_t size, value_set* value, bool async, algo_req *const req);
void *memio_info_hw_read(uint32_t ppa, char* key, uint32_t key_len, value_set* value, bool async, algo_req *const req);
void *memio_info_trim_block(uint32_t ppa, bool async);
void *memio_info_trim_a_block(uint32_t ppa, bool async);
void *memio_info_refresh(struct lower_info* li);
void *memio_badblock_checker(uint32_t ppa, uint32_t size, void *(*process)(uint64_t,uint8_t));
void memio_flying_req_wait();
void memio_info_stop();
void memio_show_info_();

uint32_t memio_tag_num();
uint32_t memio_do_merge(uint32_t lp_num, ppa_t *lp_array, uint32_t hp_num,ppa_t *hp_array,ppa_t *tp_array, uint32_t* ktable_num, uint32_t *invliadate_num);
char *memio_get_kt();
char *memio_get_inv();
