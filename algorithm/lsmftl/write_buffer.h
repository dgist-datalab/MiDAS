#ifndef __WRITE_BUFFER_H__
#define __WRITE_BUFFER_H__
#include "../../include/settings.h"
#include "../../interface/interface.h"
#include "read_helper.h"
#include "page_manager.h"
#include "global_return_code.h"
#include "lftl_slab.h"
#include "key_value_pair.h"
#include <map>

typedef enum write_buffer_return_code{
	WB_NONE=0,
	WB_FULL,
	WB_IS_IMMUTABLE,
}WB_return_code;

typedef union bf_data{
	char *gc_data;
	value_set *data;
}bf_data;

typedef struct buffer_entry{
	uint32_t lba;
	bf_data data;
}buffer_entry;

enum{
NORMAL_WB, GC_WB
};

typedef struct write_buffer{
	bool is_immutable;
	uint32_t type;
	uint32_t buffered_entry_num;
	uint32_t max_buffer_entry_num;
	slab_master *sm;
	struct page_manager *pm;
//	buffer_entry **data;
	std::map<uint32_t, buffer_entry *> *data;
	uint32_t flushed_req_cnt;
	fdriver_lock_t cnt_lock;
	fdriver_lock_t sync_lock;

	struct read_helper *rh;
	struct read_helper_param rhp;
}write_buffer;

write_buffer *write_buffer_reinit(write_buffer *wb);
write_buffer *write_buffer_init(uint32_t max_buffered_entry_num, page_manager *pm, uint32_t type);
write_buffer *write_buffer_init_for_gc(uint32_t max_buffered_entry_num, page_manager *pm, uint32_t type, read_helper_param rhp);
key_ptr_pair* write_buffer_flush(write_buffer *, uint32_t target_num, bool sync);
uint32_t write_buffer_insert(write_buffer *, uint32_t lba, value_set* value);
char *write_buffer_get(write_buffer *, uint32_t lba);
void write_buffer_free(write_buffer*);

uint32_t write_buffer_insert_for_gc(write_buffer *, uint32_t lba, char *gc_data);
key_ptr_pair* write_buffer_flush_for_gc(write_buffer *, bool sync, uint32_t seg_idx, bool *,
		uint32_t prev_map_num, std::map<uint32_t, struct gc_mapping_check_node*>* gkv);

inline static uint32_t write_buffer_isfull(write_buffer *wb){
	return wb->buffered_entry_num >= wb->max_buffer_entry_num;
}

inline static uint32_t write_needed_page(write_buffer *wb){
	return wb->data->size()+((wb->data->size()/MAPINPAGE)+ 
			(wb->data->size()%MAPINPAGE?1:0));
}

#endif
