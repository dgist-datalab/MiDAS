#ifndef __SST_PAGE_FILE_STREAM_H__
#define __SST_PAGE_FILE_STREAM_H__
#include "key_value_pair.h"
#include "../../interface/interface.h"
#include "compaction.h"
#include <queue>
enum {
SST_PAGE_FILE_STREAM, KP_PAIR_STREAM, MAP_FILE_STREAM,
};

typedef struct sst_pf_out_stream{
	uint8_t type;
	bool (*check_done)(struct inter_read_alreq_param *check_flag, bool check_file_sst);
	bool (*file_done)(struct inter_read_alreq_param* check_flag);
	std::queue<sst_file*> *sst_file_set;
	std::queue<map_range*> *mr_set;
	std::queue<struct inter_read_alreq_param*> *check_flag_set;
	key_ptr_pair *kp_data;
	sst_file *now;
	map_range *now_mr;
	uint32_t idx;
	bool now_file_empty;
	bool file_set_empty;
	uint32_t version_idx;
	uint32_t total_poped_num;
#ifdef LSM_DEBUG
	bool isstart;
	uint32_t prev_lba;
#endif
}sst_pf_out_stream;

typedef struct sst_pf_in_stream{
	//std::queue<sst_file*> sst_file_set;
	sst_file *now;
	value_set *vs;
	bool make_read_helper;
	read_helper *rh;
	read_helper_param rhp;
	uint32_t idx;
}sst_pf_in_stream;

sst_pf_out_stream* sst_pos_init_sst(sst_file *, struct inter_read_alreq_param **,
		uint32_t set_number,
		uint32_t version_number,
		bool(*check_done)( struct inter_read_alreq_param*, bool), 
		bool (*file_done)(struct inter_read_alreq_param *));

sst_pf_out_stream* sst_pos_init_mr(map_range *, struct inter_read_alreq_param **,
		uint32_t set_number, 
		uint32_t version_number,
		bool(*check_done)( struct inter_read_alreq_param*, bool), 
		bool (*file_done)(struct inter_read_alreq_param *));

sst_pf_out_stream *sst_pos_init_kp(key_ptr_pair *data);
void sst_pos_add_sst(sst_pf_out_stream *os, sst_file *, 
		inter_read_alreq_param **, uint32_t num);
void sst_pos_add_mr(sst_pf_out_stream *os, map_range *, 
		inter_read_alreq_param **, uint32_t num);
key_ptr_pair sst_pos_pick(sst_pf_out_stream *os);
void sst_pos_pop(sst_pf_out_stream *os);
void sst_pos_free(sst_pf_out_stream *os);
bool sst_pos_is_empty(sst_pf_out_stream *os);

sst_pf_in_stream* sst_pis_init(bool make_read_helper, read_helper_param rhp);
void sst_pis_set_space(sst_pf_in_stream *is, value_set *data, uint8_t type);
bool sst_pis_insert(sst_pf_in_stream *is, key_ptr_pair kp);

value_set *sst_pis_get_result(sst_pf_in_stream *is, 
		sst_file **result_ptr);

static inline bool sst_pis_remain_data(sst_pf_in_stream *is){
	return !(is->idx==0);
}

//char *sst_in_get_result(sst_file **result_ptr);
void sst_pis_free(sst_pf_in_stream *is);

#endif
