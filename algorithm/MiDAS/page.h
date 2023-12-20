#include "../../include/settings.h"
#include "../../include/container.h"
#include <map>

#define GB_REQ 262144
typedef struct page_param{
	request *parents;
	uint32_t address;
	value_set *value;
}page_param;

typedef struct align_buffer{
	uint8_t idx;
	value_set *value[L2PGAP];
	KEYT key[L2PGAP];
	request *preq[L2PGAP];
}align_buffer;


typedef struct page_read_buffer{
	std::multimap<uint32_t, algo_req *> * pending_req;
	std::multimap<uint32_t, algo_req *>* issue_req;
	fdriver_lock_t pending_lock;
	fdriver_lock_t read_buffer_lock;
	uint32_t buffer_ppa;
	char buffer_value[PAGESIZE];
}page_read_buffer;

uint32_t page_create (lower_info*,blockmanager *, algorithm *);
void page_destroy (lower_info*,  algorithm *);
uint32_t page_argument(int argc, char **argv);
uint32_t page_read(request *const);
uint32_t page_write(request *const);
uint32_t page_remove(request *const);
uint32_t page_flush(request *const);
void *page_end_req(algo_req*);
