#ifndef SEGMENT_LEVEL_MANAGER
#define SEGMENT_LEVEL_MANAGER
#include <unordered_map>
#include <stdint.h>
typedef struct slm_node{
	uint32_t seg_idx;
	uint32_t piece_offset;
}slm_node;

typedef struct seg_level_manager{
	uint32_t level_num;
	std::unordered_map<uint32_t, slm_node*> **body;
}SLM;

void slm_init(uint32_t leveling_level_num);
void slm_coupling_level_seg(uint32_t level_idx, uint32_t seg_idx, uint32_t seg_piece_offset, bool is_gc_data);
void slm_coupling_mem_lev_seg(uint32_t seg_idx, uint32_t seg_piece_offset);
void slm_move_mem_lev_seg(uint32_t des_lev_idx);
void slm_move(uint32_t des_lev_idx, uint32_t src_leve_idx);
void slm_remove_node(uint32_t level_idx, uint32_t seg_idx);
void slm_free();
bool slm_invalidate_enable(uint32_t level_idx, uint32_t piece_ppa);
void slm_empty_level(uint32_t des_lev_idx);
std::unordered_map<uint32_t, slm_node*> *slm_get_target_map(uint32_t level_idx);
#endif
