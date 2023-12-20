#ifndef __SLAB_H__
#define __SLAB_H__
#include "../../include/utils/tag_q.h"

typedef struct slab_master{
	uint32_t entry_size;
	uint32_t entry_max_num;
	char **slab_body;
	tag_manager *tag_q;
}slab_master;

slab_master *slab_master_init(uint32_t target_entry_size, uint32_t target_entry_max_num);
void* slab_alloc(slab_master*);
void slab_free(slab_master *, char *ptr);
void slab_master_free(slab_master*);
#endif
