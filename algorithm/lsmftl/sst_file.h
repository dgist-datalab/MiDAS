#ifndef __SST_H__
#define __SST_H__
#include "key_value_pair.h"
#include <stdint.h>
#include "../../interface/interface.h"
#include "page_manager.h"
#include "read_helper.h"
enum{
	PAGE_FILE, BLOCK_FILE
};

typedef union physcial_addr{
	uint32_t piece_ppa; // for block_file
	uint32_t map_ppa; //for page_file
}p_addr;


typedef struct map_range{
	uint32_t start_lba;
	uint32_t end_lba;
	uint32_t ppa;
	char *data;
}map_range;

typedef struct sst_file{
	uint8_t type;
	bool ismoved_originality;
	bool trimed_sst_file;
	bool sequential_file;

	p_addr file_addr;
	uint32_t start_piece_ppa;
	uint32_t end_ppa;
	uint32_t map_num;

	uint32_t start_lba;
	uint32_t end_lba;
	struct read_helper *_read_helper;
	
	map_range *block_file_map;
	bool is_issue_for_gc;
	char *data;
}sst_file;

sst_file *sst_init_empty(uint8_t type);
sst_file *sst_pf_init(uint32_t ppa, uint32_t start_lba, uint32_t end_lba);
sst_file *sst_bf_init(uint32_t ppa, uint32_t end_ppa,uint32_t start_lba, uint32_t end_lba);
void sst_destroy_content(sst_file*, struct page_manager *);
void sst_reinit(sst_file *);
void sst_free(sst_file*, struct page_manager *);
void sst_print(sst_file *);
void sst_deep_copy(sst_file *des, sst_file *src);
void sst_convert_seq_pf_to_bf(sst_file *src);
static inline void sst_shallow_copy_move_originality(sst_file *des, sst_file *src){
	*des=*src;
	//des->_read_helper=src->_read_helper;
	src->ismoved_originality=true;
}

static inline void sst_shallow_copy(sst_file *des, sst_file *src){
	*des=*src;
	//des->_read_helper=src->_read_helper;
	des->ismoved_originality=true;
}

void sst_set_file_map(sst_file *, uint32_t, map_range*);
uint32_t sst_find_map_addr(sst_file *, uint32_t lba);
sst_file *sst_MR_to_sst_file(map_range *mr);
static inline bool sst_physical_range_overlap(sst_file *a, sst_file *b){
	return SEGNUM(a->file_addr.piece_ppa)==SEGNUM(b->file_addr.piece_ppa); 
}

static inline bool is_map_ppa(sst_file *sptr, uint32_t ppa){
	for(uint32_t i=0; i<sptr->map_num; i++){
		if(ppa==sptr->block_file_map[i].ppa){
			return true;
		}
	}
	return false;
}

void map_print(map_range *mr);

#define for_each_kp(data_ptr, kp_ptr, kp_idx)\
	for(kp_idx=0, kp_ptr=(key_ptr_pair*)&data_ptr[kp_idx*sizeof(key_ptr_pair)];\
			(kp_idx*sizeof(key_ptr_pair) < PAGESIZE && kp_ptr->lba!=UINT32_MAX);\
			kp_idx++, kp_ptr=(key_ptr_pair*)&data_ptr[kp_idx*sizeof(key_ptr_pair)])

#define for_each_map_range(sptr, map_ptr, map_idx)\
	for(map_idx=0, map_ptr=&((sptr)->block_file_map[map_idx]);\
			map_idx<(sptr)->map_num;\
			map_idx++, map_ptr=&((sptr)->block_file_map[map_idx]))

#endif
