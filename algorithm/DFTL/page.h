#include "../../include/settings.h"
#include "../../include/container.h"
#include <map>
typedef struct page_params{
	request *parents;
	uint32_t address;
	value_set *value;
}page_params;

typedef struct align_buffer{
	uint8_t idx;
	uint32_t prefetching_info[L2PGAP];
	KEYT key[L2PGAP];
	char *value;
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

void send_user_req(request *const req, uint32_t type, ppa_t ppa,value_set *value);
