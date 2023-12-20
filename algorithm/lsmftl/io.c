#include "io.h"
#include "../../interface/interface.h"
io_manager io_m;
void io_manager_init(lower_info *li){
	io_m.li=li;
	io_m.tm=tag_manager_init(QSIZE);

	for(uint32_t i=0; i<QSIZE; i++){
//		fdriver_mutex_init(&io_m.sync_mutex[i]);
		fdriver_lock_init(&io_m.sync_mutex[i], 0);
	}
	memset(io_m.wrapper, 0, sizeof(sync_wrapper)*QSIZE);
}

static inline void *io_sync_end_req(algo_req* al_req){
	sync_wrapper *wrapper=(sync_wrapper*)al_req->param;
	al_req->end_req=wrapper->end_req;
	al_req->param=wrapper->param;
	uint32_t tag=wrapper->tag;
	if(al_req->end_req){
		al_req->end_req(al_req);
	}
	fdriver_unlock(&io_m.sync_mutex[tag]);
	tag_manager_free_tag(io_m.tm, tag);
	return NULL;
}

static inline void set_wrapper(sync_wrapper *wrapper, algo_req *al_req, uint32_t tag){
	wrapper->tag=tag;
	wrapper->end_req=al_req->end_req;
	wrapper->param=al_req->param;
	al_req->param=(void*)wrapper;
	al_req->end_req=io_sync_end_req;
}

void io_manager_issue_internal_write(uint32_t ppa, value_set *value, algo_req *al_req, bool sync){
	if(sync){
		uint32_t tag=tag_manager_get_tag(io_m.tm);
		sync_wrapper *wrapper=&io_m.wrapper[tag];
		set_wrapper(wrapper, al_req, tag);
		io_m.li->write(ppa, PAGESIZE ,value, ASYNC, al_req);
		fdriver_lock(&io_m.sync_mutex[tag]);
	}
	else{
		io_m.li->write(ppa, PAGESIZE ,value, ASYNC, al_req);
	}
}

void io_manager_issue_internal_read(uint32_t ppa, value_set *value, algo_req *al_req, bool sync){
	if(sync){
		uint32_t tag=tag_manager_get_tag(io_m.tm);
		sync_wrapper *wrapper=&io_m.wrapper[tag];
		set_wrapper(wrapper, al_req, tag);
		io_m.li->read(ppa, PAGESIZE ,value, ASYNC, al_req);
		fdriver_lock(&io_m.sync_mutex[tag]);
	}else{
		io_m.li->read(ppa, PAGESIZE ,value, ASYNC, al_req);
	}
}

void io_manager_issue_write(uint32_t ppa, value_set *value, algo_req *al_req, bool sync){
	io_m.li->write(ppa, PAGESIZE ,value, ASYNC, al_req);
}

void io_manager_issue_read(uint32_t ppa, value_set *value, algo_req *al_req, bool sync){
	io_m.li->read(ppa, PAGESIZE ,value, ASYNC, al_req);
}

void io_manager_free(){
	tag_manager_free_manager(io_m.tm);
}

static void *temp_end_req(algo_req *req){
	free(req);
	return NULL;
}

void io_manager_test_read(uint32_t ppa, char *data, uint32_t type){
	algo_req *test_req=(algo_req*)malloc(sizeof(algo_req));
	test_req->type=type;
	test_req->end_req=NULL;
	value_set *vs=inf_get_valueset(NULL,FS_MALLOC_R, PAGESIZE);
	io_manager_issue_internal_read(ppa, vs, test_req, true);
	memcpy(data, vs->value, PAGESIZE);
	inf_free_valueset(vs, FS_MALLOC_R);
	free(test_req);
}

void io_manager_test_write(uint32_t ppa, char *data, uint32_t type){
	algo_req *test_req=(algo_req*)malloc(sizeof(algo_req));
	test_req->type=type;
	test_req->end_req=NULL;
	value_set *vs=inf_get_valueset(NULL,FS_MALLOC_R, PAGESIZE);
	memcpy(vs->value, data, PAGESIZE);
	io_manager_issue_internal_write(ppa, vs, test_req, true);
	inf_free_valueset(vs, FS_MALLOC_R);
	free(test_req);
}
