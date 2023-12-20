#include "../../include/container.h"
#include <libaio.h>
typedef struct iocb_container{
	struct iocb cb;
}iocb_container_t;

uint32_t no_create(lower_info*, blockmanager *bm);
void *no_destroy(lower_info*);
void* no_push_data(uint32_t ppa, uint32_t size, value_set *value,bool async, algo_req * const req);
void* no_pull_data(uint32_t ppa, uint32_t size, value_set* value,bool async,algo_req * const req);
void* no_trim_block(uint32_t ppa,bool async);
void *no_refresh(lower_info*);
void no_stop();
void no_flying_req_wait();
