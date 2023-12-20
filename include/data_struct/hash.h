#ifndef _DATA_STRUCT_HASH_H__
#define _DATA_STRUCT_HASH_H__
#include<stdint.h>
#define MULTIFACTOR 5
typedef struct __hash_node{
	void *inter_ptr;
	void *data;
	uint32_t t_idx;
	uint32_t key;
}__hash_node;

typedef struct __hash{
	__hash_node* table;
	int32_t n_size;
	int32_t m_size;
	int32_t table_size;
}__hash;

__hash* __hash_init(uint32_t size);
int __hash_insert(__hash *h,uint32_t key,void *data, void* inter_ptr, void **updated);
void* __hash_delete_by_key(__hash *h,uint32_t key);
void* __hash_delete_by_idx(__hash *h,uint32_t idx);
void* __hash_find_data(__hash *h, uint32_t key);
__hash_node* __hash_find_node(__hash *h,uint32_t key);
__hash_node* __hash_get_node(__hash *h,uint32_t idx);
void __hash_free(__hash *h);
#endif
