#ifndef __IO_H__
#define __IO_H__
#include "../../include/container.h"
#include "../../include/settings.h"
#include "../../include/sem_lock.h"
#include "../../include/utils/tag_q.h"

typedef struct sync_wrapper{
	void* param;
	void *(*end_req)(algo_req*);
	uint32_t tag;
}sync_wrapper;

typedef struct {
	lower_info *li;
	tag_manager *tm;
	fdriver_lock_t sync_mutex[QSIZE];
	sync_wrapper wrapper[QSIZE];
}io_manager;

void io_manager_init(lower_info *li);
void io_manager_issue_internal_write(uint32_t ppa, value_set *value, algo_req *, bool sync);
void io_manager_issue_internal_read(uint32_t ppa, value_set *value, algo_req *, bool sync);
void io_manager_issue_write(uint32_t ppa, value_set *value, algo_req *req, bool sync);
void io_manager_issue_read(uint32_t ppa, value_set *value, algo_req *req, bool sync);
void io_manager_free();

void io_manager_test_read(uint32_t ppa, char *data, uint32_t type);
void io_manager_test_write(uint32_t ppa, char *data, uint32_t type);
#endif
