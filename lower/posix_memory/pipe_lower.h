#ifndef PIPE_HEADER
#define PIPE_HEADER


#include <stdint.h>
#include <limits.h>
#include "../../include/settings.h"
#include "posix.h"

#define for_each_header_start(idx,key,ppa_ptr,bitmap,body)\
	for(idx=1; bitmap[idx]!=UINT16_MAX && idx<=bitmap[0]; idx++){\
		ppa_ptr=(ppa_t*)&body[bitmap[idx]];\
		key.key=(char*)&body[bitmap[idx]+sizeof(ppa_t)];\
		key.len=bitmap[idx+1]-bitmap[idx]-sizeof(ppa_t);\

#define for_each_header_end }

typedef struct pipe_lower_body{
	uint32_t max_page;
	uint32_t pidx;
	uint32_t *map_ppa_list;
	mem_seg *data_ptr;

	char *now_page;
	uint16_t *bitmap_ptr;
	uint32_t length;
	uint32_t max_key;
	uint32_t kidx;
}pl_body;

pl_body *plbody_init(mem_seg *data, uint32_t *map_ppa_list, uint32_t list_size);
KEYT plbody_get_next_key(pl_body *p, uint32_t *r_ppa);
char *plbody_insert_new_key(pl_body *p,KEYT key, uint32_t ppa,bool f);

void plbody_data_print(char *data);
uint32_t find_ppa_from(char *body, char *key, uint32_t len);
#endif
