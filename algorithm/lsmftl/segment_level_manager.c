#include "segment_level_manager.h"
#include "../../include/settings.h"
#include "../../include/container.h"
#include <stdio.h>
#include <stdlib.h>
SLM slm_manager;

typedef std::unordered_map<uint32_t, slm_node*> map;
typedef std::unordered_map<uint32_t, slm_node*>::iterator map_iter;
typedef std::pair<uint32_t, slm_node*> map_pair;

//#define MAPPRINT

void slm_init(uint32_t leveling_level_num){
	slm_manager.body=(map**)malloc(sizeof(map*)*(leveling_level_num+1));
	for(uint32_t i=0; i<leveling_level_num+1; i++){
		slm_manager.body[i]=new map();
	}
	slm_manager.level_num=leveling_level_num+1;
}

static void map_print(map *m){
	for(map_iter mi=m->begin(); mi!=m->end(); mi++){
		printf("(%u,%u) ", mi->second->seg_idx, mi->second->piece_offset);
	}
	printf("\n");
}

void slm_coupling_level_seg(uint32_t level_idx, uint32_t seg_idx, uint32_t seg_piece_offset, bool is_gc_data){
	if(level_idx > slm_manager.level_num-1){
		EPRINT("overflow!", true);
	}
	map* target=slm_manager.body[level_idx];
	map_iter it;
	it=target->find(seg_idx);
	if(it!=target->end()){
		//update
		if(level_idx==slm_manager.level_num-1){
			if(it->second->piece_offset<seg_piece_offset){
				it->second->piece_offset=seg_piece_offset;
			}
		}
		else{
			if(it->second->piece_offset >= seg_piece_offset){
				if(!is_gc_data){
					map_print(target);
					EPRINT("append only error", true);
				}
			}
			it->second->piece_offset=seg_piece_offset;
		}
	}
	else{
		slm_node *sn=(slm_node*)malloc(sizeof(slm_node));
		sn->seg_idx=seg_idx;
		sn->piece_offset=seg_piece_offset;
		target->insert(map_pair(seg_idx, sn));
	}
#ifdef MAPPRINT
	printf("coupling - \n %u: ",level_idx);
	map_print(target);
#endif
}

void slm_coupling_mem_lev_seg(uint32_t seg_idx, uint32_t seg_piece_offset){
	slm_coupling_level_seg(slm_manager.level_num-1, seg_idx, seg_piece_offset, false);
}

void slm_move_mem_lev_seg(uint32_t des_lev_idx){
	slm_move(des_lev_idx, slm_manager.level_num-1);
}
void slm_move(uint32_t des_lev_idx, uint32_t src_lev_idx){
	if(des_lev_idx > slm_manager.level_num-1){
		EPRINT("overflow!", true);
	}
	if(src_lev_idx > slm_manager.level_num-1){
		EPRINT("overflow!", true);
	}
	map *des_target=slm_manager.body[des_lev_idx];
	map *src_target=slm_manager.body[src_lev_idx];
	
	for(map_iter it=src_target->begin(); it!=src_target->end(); it++){
		map_iter des_it=des_target->find(it->first);
		if(des_it!=des_target->end()){
			des_it->second->piece_offset=MAX(it->second->piece_offset,des_it->second->piece_offset);
			free(it->second);
		}
		else{
			des_target->insert(*it);
		}
	}
	src_target->clear();
#ifdef MAPPRINT
	printf("move----\n");
	printf("des %u: ", des_lev_idx);
	map_print(des_target);
#endif
}

void slm_free(){
	for(uint32_t i=0; i<slm_manager.level_num; i++){
		map *target=slm_manager.body[i];
		for(map_iter it=target->begin(); it!=target->end(); it++){
			free(it->second);
		}
		delete target;
	}
	free(slm_manager.body);
}

bool slm_invalidate_enable(uint32_t level_idx, uint32_t piece_ppa){
	if(level_idx > slm_manager.level_num-1){
		EPRINT("overflow!", true);
	}
	map *target=slm_manager.body[level_idx];
	map_iter it=target->find(SEGNUM(piece_ppa));

#ifdef MAPPRINT
//	printf("query---\n");
//	printf("%u: ",level_idx);
//	map_print(target);
#endif

	if(it==target->end()){
		return false;
	}
	if(it->second->piece_offset >= SEGPIECEOFFSET(piece_ppa)){
		return true;	
	}
	else{
		return false;
//		EPRINT("error", true);
	}
	return true;
}

void slm_remove_node(uint32_t level_idx, uint32_t seg_idx){
	if(level_idx > slm_manager.level_num-1){
		EPRINT("overflow!", true);
	}
	map *target=slm_manager.body[level_idx];
	map_iter it=target->find(seg_idx);
	if(it!=target->end()){
		target->erase(it);
	}

#ifdef MAPPRINT
	printf("remove---\n");
	printf("%u: ",level_idx);
	map_print(target);
#endif
}

void slm_empty_level(uint32_t des_lev_idx){
	if(des_lev_idx > slm_manager.level_num-1){
		EPRINT("overflow!", true);
	}
	map *target=slm_manager.body[des_lev_idx];
	for(map_iter it=target->begin(); it!=target->end(); it++){
		free(it->second);
	}
	target->clear();
}

std::unordered_map<uint32_t, slm_node*> *slm_get_target_map(uint32_t level_idx){
	return slm_manager.body[level_idx];
}
