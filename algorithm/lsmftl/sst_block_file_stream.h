#ifndef __SST_BLOCK_FILE_STREAM_H__
#define __SST_BLOCK_FILE_STREAM_H__

#include "sst_page_file_stream.h"
#include "compaction.h"
#include "page_manager.h"
#include "read_helper.h"
#include <queue>

#define REMAIN_DATA_PPA(bis) \
	((int32_t)(((bis)->piece_ppa_length) - \
	 (bis)->map_data->size()*L2PGAP - \
	 (bis)->write_issued_kv_num))

typedef struct sst_bf_out_stream{
	bool (*kv_read_check_done)(struct inter_read_alreq_param*, bool check);
	std::queue<struct key_value_wrapper *> *kv_wrapper_q;
	uint32_t prev_ppa;
	uint8_t kv_buf_idx;
	uint8_t version_idx;
	struct key_value_wrapper *kv_wrap_buffer[L2PGAP];
	bool no_inter_param_alloc;

	uint32_t last_issue_lba;
	/*for pop*/
//	struct key_value_wrapper* now_kv_wrap;

#ifdef LSM_DEBUG
	bool isstart;
	uint32_t prev_lba;
#endif
}sst_bf_out_stream;

typedef struct sst_bf_in_stream{
	uint32_t prev_lba;
	uint32_t start_lba;
	uint32_t end_lba;

	uint32_t start_piece_ppa;
	uint32_t piece_ppa_length;
	uint32_t write_issued_kv_num;

	std::queue<value_set *>*map_data;
	value_set *now_map_data;
	uint32_t now_map_data_idx;

	bool make_read_helper;
	read_helper *rh;
	read_helper_param rhp;

	uint8_t buffer_idx;
	key_value_wrapper **buffer;//assigned L2PGAP * key_value_wrapper;

	page_manager *pm;
	__segment *seg;

}sst_bf_in_stream;

sst_bf_out_stream *sst_bos_init(bool (*r_check_done)(struct inter_read_alreq_param *, bool), bool no_inter_param_alloc);
struct key_value_wrapper *sst_bos_add(sst_bf_out_stream *bos, struct key_value_wrapper *, struct compaction_master *cm);
struct key_value_wrapper *sst_bos_get_pending(sst_bf_out_stream *bos, struct compaction_master *cm);
key_value_wrapper* sst_bos_pick(sst_bf_out_stream * bos, bool);
uint32_t sst_bos_size(sst_bf_out_stream *bos, bool include_pending);
bool sst_bos_is_empty(sst_bf_out_stream *bos);
void sst_bos_pop(sst_bf_out_stream *bos, struct compaction_master *);
void sst_bos_free(sst_bf_out_stream *bos, struct compaction_master *);

static inline uint32_t sst_bos_kv_q_size(sst_bf_out_stream *bos){
	return bos->kv_wrapper_q->size();
}

//sst_bf_in_stream * sst_bis_init(uint32_t start_piece_ppa, uint32_t piece_ppa_length, page_manager *pm, 
//		bool make_read_helper, read_helper_param rhp);
sst_bf_in_stream * sst_bis_init(__segment *seg, page_manager *pm, bool make_read_helper, read_helper_param rhp);
bool sst_bis_insert(sst_bf_in_stream *bis, key_value_wrapper *);
value_set* sst_bis_finish(sst_bf_in_stream*);
value_set* sst_bis_get_result(sst_bf_in_stream *bis, bool last, uint32_t *idx, key_ptr_pair *); //it should return unaligned value when it no space
bool sst_bis_ppa_empty(sst_bf_in_stream *bis);
void sst_bis_free(sst_bf_in_stream *bis);

#endif
