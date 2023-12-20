#ifndef __KEY_VALUE_PAIR_H__
#define __KEY_VALUE_PAIR_H__
#include <stdint.h>
#include "../../include/settings.h"
#define LAST_KP_IDX (PAGESIZE/sizeof(key_ptr_pair)-1)
#define KP_IN_PAGE (PAGESIZE/sizeof(key_ptr_pair))
typedef struct key_ptr_pair{
	uint32_t lba;
	uint32_t piece_ppa;
} key_ptr_pair;

typedef struct key_value_pair{
	uint32_t lba; //oob
	char *data; //4KB value
} key_value_pair;

static inline uint32_t kp_find_piece_ppa(uint32_t lba, char *page_data){
	int16_t s=0, e=LAST_KP_IDX, mid;
	key_ptr_pair* map_set=(key_ptr_pair*)page_data;
	while(s<=e){
		mid=(s+e)/2;
		if(map_set[mid].lba==lba){
			return map_set[mid].piece_ppa;
		}
		if(map_set[mid].lba<lba){
			s=mid+1;
		}
		else{
			e=mid-1;
		}
	}
	return UINT32_MAX;
}

static inline uint32_t kp_find_idx(uint32_t lba, char *page_data){
	int16_t s=0, e=LAST_KP_IDX, mid;
	key_ptr_pair* map_set=(key_ptr_pair*)page_data;
	while(s<=e){
		mid=(s+e)/2;
		if(map_set[mid].lba==lba){
			return mid;
		}
		if(map_set[mid].lba<lba){
			s=mid+1;
		}
		else{
			e=mid-1;
		}
	}
	return UINT32_MAX;
}

static inline uint32_t kp_end_idx(char *data){
	key_ptr_pair* map_set=(key_ptr_pair*)data;
	if(map_set[LAST_KP_IDX].lba!=UINT32_MAX){
		return LAST_KP_IDX;
	}
	int16_t s=0, e=LAST_KP_IDX, mid;
	uint32_t target_lba=UINT32_MAX;
	while(s<=e){
		mid=(s+e)/2;
		if(map_set[mid].lba==target_lba){
			break;
		}
		if(map_set[mid].lba<target_lba){
			s=mid+1;
		}
		else{
			e=mid-1;
		}
	}

	if(map_set[mid].lba==target_lba){
		while(map_set[mid].lba==target_lba){
			mid--;
		}
	}
	else{
		while(mid+1<=LAST_KP_IDX){
			if(map_set[mid].lba==target_lba){
				mid--;
				break;
			}
			mid++;
		}
	}
	return mid;
}

static inline uint32_t kp_get_end_lba(char *data){
	key_ptr_pair* map_set=(key_ptr_pair*)data;
	uint32_t aa=kp_end_idx(data);
	return map_set[aa].lba;
}

#define MAPINPAGE (PAGESIZE/sizeof(key_ptr_pair))
#endif
