#include "../../include/container.h"

uint32_t posix_create(lower_info*);
void *posix_destroy(lower_info*);
void* posix_push_data(uint32_t ppa, uint32_t size, value_set *value,bool async, algo_req * const req);
void* posix_pull_data(uint32_t ppa, uint32_t size, value_set* value,bool async,algo_req * const req);
void* posix_trim_block(uint32_t ppa,bool async);
void *posix_refresh(lower_info*);
void posix_stop();
void posix_flying_req_wait();
