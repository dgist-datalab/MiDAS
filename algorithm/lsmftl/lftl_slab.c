#include "lftl_slab.h"
#include <stdlib.h>

slab_master *slab_master_init(uint32_t target_entry_size, uint32_t target_entry_max_num){
	slab_master* res=(slab_master*)calloc(1,sizeof(slab_master));
	res->entry_size=target_entry_size;
	res->entry_max_num=target_entry_max_num;
	res->slab_body=(char **)malloc(target_entry_max_num * sizeof(char*));
	for(uint32_t i=0; i<target_entry_max_num; i++){
		res->slab_body[i]=(char*)malloc(target_entry_size+sizeof(uint32_t));
		/*set tag*/
		*(uint32_t*)&res->slab_body[i][target_entry_size]=i;
	}
	res->tag_q=tag_manager_init(target_entry_max_num);
	return res;
}

void* slab_alloc(slab_master* sm){
	uint32_t tag=tag_manager_get_tag(sm->tag_q);
	return (void*)sm->slab_body[tag];
}

void slab_free(slab_master *sm, char *ptr){
	uint32_t tag=*(uint32_t*)&ptr[sm->entry_size];
	tag_manager_free_tag(sm->tag_q, tag);
}

void slab_master_free(slab_master* sm){
	for(uint32_t i=0; i<sm->entry_max_num; i++){
		free(sm->slab_body[i]);
	}
	tag_manager_free_manager(sm->tag_q);
	free(sm->slab_body);
	free(sm);
}
