#include"hash_kv.h"
#include "../utils/sha256.h"
#include<stdlib.h>
#include<stdio.h>
#ifdef KVSSD
static uint32_t function(uint32_t a){
	a = (a+0x7ed55d16) + (a<<12);
	a = (a^0xc761c23c) ^ (a>>19);
	a = (a+0x165667b1) + (a<<5);
	a = (a+0xd3a2646c) ^ (a<<9);
	a = (a+0xfd7046c5) + (a<<3);
	a = (a^0xb55a4f09) ^ (a>>16);
	return a;
}

__hash* __hash_init(uint32_t size){
	__hash* res=(__hash*)malloc(sizeof(__hash));
	res->table=(__hash_node*)malloc(sizeof(__hash_node)*size*MULTIFACTOR);
	for(uint32_t i=0; i<size*MULTIFACTOR; i++){
		res->table[i].t_idx=i;
		res->table[i].data=res->table[i].inter_ptr=NULL;
		res->table[i].key=0;

		res->table[i].kv_key.len=0;
		res->table[i].kv_key.key=NULL;
	}
	res->n_size=0;
	res->m_size=size;
	res->table_size=size*MULTIFACTOR;
	return res;
}

static uint32_t hashing_key(char* key,uint8_t len) {
	char* string;
	Sha256Context ctx;
	SHA256_HASH hash;
	int bytes_arr[8];
	uint32_t hashkey;

	string = key;

	Sha256Initialise(&ctx);
	Sha256Update(&ctx, (unsigned char*)string, len);
	Sha256Finalise(&ctx, &hash);

	for(int i=0; i<8; i++) {
		bytes_arr[i] = ((hash.bytes[i*4] << 24) | (hash.bytes[i*4+1] << 16) | \
				(hash.bytes[i*4+2] << 8) | (hash.bytes[i*4+3]));
	}

	hashkey = bytes_arr[0];
	for(int i=1; i<8; i++) {
		hashkey ^= bytes_arr[i];
	}

	return hashkey;
}

int __hash_insert(__hash *h,KEYT kv_key,void *data, void* inter_ptr, void** updated){
	if(h->n_size==h->m_size)
		return 0;
	uint32_t key = hashing_key(kv_key.key, kv_key.len);
	uint32_t h_key,org_key=key;
	int res=0;
	for(int i=0; i<2*MULTIFACTOR; i++){
		h_key=function(key);
		res=h_key%(h->m_size*MULTIFACTOR);
		__hash_node* node=&h->table[res];
		if(node->data){
			if(node->key==org_key && KEYCMP(node->kv_key, kv_key)==0){	
				*updated=(void*)node->data;
				node->data=data;
				node->key=org_key;
				node->inter_ptr=inter_ptr;
				return h->table_size+res;
			}
			key=h_key;
			continue;
		}
		else{
			node->data=data;
			node->key=org_key;
			node->inter_ptr=inter_ptr;

			node->kv_key.len = kv_key.len;
			node->kv_key.key = (char *)malloc(kv_key.len);
			memcpy(node->kv_key.key, kv_key.key, kv_key.len);

			break;
		}
	}
	h->n_size++;

	return res;
}

void* __hash_delete_by_key(__hash *h,KEYT key){
	__hash_node *node=__hash_find_node(h,key);
	void *res=node->inter_ptr;
	node->inter_ptr=node->data=NULL;
	node->key=0;
	h->n_size--;
	return res;
}

void* __hash_delete_by_idx(__hash *h,uint32_t idx){
	__hash_node *node=__hash_get_node(h,idx);
	void *res=node->inter_ptr;
	node->inter_ptr=node->data=NULL;
	node->key=0;
	free(node->kv_key.key);
	h->n_size--;
	return res;
}

void* __hash_find_data(__hash *h, KEYT kv_key){
	__hash_node *node=__hash_find_node(h,kv_key);
	if(!node)
		return NULL;
	return node->data;
}

__hash_node* __hash_find_node(__hash *h,KEYT kv_key){
	uint32_t key = hashing_key(kv_key.key, kv_key.len);
	uint32_t h_key,org_key=key;
	__hash_node *node=NULL;

	for(int i=0; i<2*MULTIFACTOR; i++){
		h_key=function(key);
		node=&h->table[h_key%(h->m_size*MULTIFACTOR)];

		if (!node->data) {
			key = h_key;
			continue;
		}
		else if (node->key==org_key && KEYCMP(node->kv_key, kv_key) == 0) {
			return node;
		}
		key=h_key;
	}
	return NULL;
}

__hash_node* __hash_get_node(__hash *h,uint32_t idx){
	return &h->table[idx];
}

void __hash_free(__hash *h){
	free(h->table);
	free(h);
}
#endif
