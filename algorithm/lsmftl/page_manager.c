#include "page_manager.h"
#include "oob_manager.h"
#include "io.h"
#include "lsmtree.h"
#include "compaction.h"
#include <stdlib.h>
#include <stdio.h>
#include<queue>

extern lsmtree LSM;
//uint32_t debug_piece_ppa=16705;
uint32_t debug_piece_ppa=UINT32_MAX;
bool temp_debug_flag;
extern uint32_t debug_lba;

void validate_piece_ppa(blockmanager *bm, uint32_t piece_num, uint32_t *piece_ppa,
		uint32_t *lba, uint32_t *version, bool should_abort){
	static int cnt=0;
	for(uint32_t i=0; i<piece_num; i++){
		oob_manager *oob=get_oob_manager(PIECETOPPA(piece_ppa[i]));
		oob->lba[piece_ppa[i]%L2PGAP]=lba[i];
		oob->version[piece_ppa[i]%L2PGAP]=version[i];
	//	memcpy(&oob[(piece_ppa[i]%L2PGAP)*sizeof(uint32_t)], &lba[i], sizeof(uint32_t));
#ifdef LSM_DEBUG
		if(piece_ppa[i]==debug_piece_ppa){
			printf("%u ", should_abort?++cnt:cnt);
			EPRINT("validate piece here!\n", false);	
		}
#endif

		if(!bm->populate_bit(bm, piece_ppa[i]) && should_abort){
			EPRINT("bit error", true);
		}
	}
}

static inline void gc_debug_checking(gc_read_node *gn){
#ifdef LSM_DEBUG
	if(gn->piece_ppa/L2PGAP==425967 && LSM.global_debug_flag){
		EPRINT("debug point", false);
	}

	if(gn->piece_ppa==debug_piece_ppa && gn->lba==debug_lba){
		EPRINT("break!", false);
	}
	if(LSM.global_debug_flag && gn->piece_ppa==debug_piece_ppa){
		EPRINT("break!", false);
	}
#endif
}

bool page_manager_oob_lba_checker(page_manager *pm, uint32_t piece_ppa, uint32_t lba, uint32_t *idx){
	oob_manager *oob=get_oob_manager(PIECETOPPA(piece_ppa));
	if(get_lba_from_oob_manager(oob, piece_ppa%L2PGAP)==lba){
		*idx=piece_ppa%L2PGAP;
		return true;
	}

	*idx=L2PGAP;
	return false;

//	char *oob=pm->bm->get_oob(pm->bm, PIECETOPPA(piece_ppa));
//	if(*(uint32_t*)&oob[(piece_ppa%L2PGAP)*sizeof(uint32_t)]==lba){
//		*idx=piece_ppa%L2PGAP;
//		return true;
//	}
//	*idx=L2PGAP;
//	return false;
}

bool invalidate_piece_ppa(blockmanager *bm, uint32_t piece_ppa, bool should_abort){
#ifdef LSM_DEBUG
	if(piece_ppa==debug_piece_ppa){
		static int cnt=0;
		printf("%u ", should_abort?++cnt:cnt);
		EPRINT("invalidate piece here!\n",false);
	}
#endif

	if(!bm->unpopulate_bit(bm, piece_ppa)){
		if(should_abort){
			EPRINT("bit error", true);
		}
		else{
			bm->invalidate_number_decrease(bm, piece_ppa);
			return false;
		}
	}
	return true;
}

page_manager* page_manager_init(struct blockmanager *_bm){
	page_manager *pm=(page_manager*)calloc(1,sizeof(page_manager));
	pm->bm=_bm;
	pm->current_segment[DATA_S]=_bm->get_segment(_bm, false);
	pm->seg_type_checker[pm->current_segment[DATA_S]->seg_idx]=SEPDATASEG;
	pm->reserve_segment[DATA_S]=_bm->get_segment(_bm, true);

	pm->current_segment[MAP_S]=_bm->get_segment(_bm, false);
	pm->seg_type_checker[pm->current_segment[MAP_S]->seg_idx]=MAPSEG;
	pm->reserve_segment[MAP_S]=_bm->get_segment(_bm, true);
	return pm;
}

void page_manager_free(page_manager* pm){
	free(pm->current_segment[DATA_S]);
	free(pm->current_segment[MAP_S]);
	free(pm->reserve_segment[DATA_S]);
	free(pm->reserve_segment[MAP_S]);
	free(pm);
}

void validate_map_ppa(blockmanager *bm, uint32_t map_ppa, uint32_t start_lba, uint32_t end_lba, bool should_abort){
	oob_manager *oob=get_oob_manager(map_ppa);
	oob->lba[0]=start_lba;
	oob->lba[1]=end_lba;

//	char *oob=bm->get_oob(bm, map_ppa);
//	((uint32_t*)oob)[0]=start_lba;
//	((uint32_t*)oob)[1]=end_lba;
#ifdef LSM_DEBUG
	if(map_ppa*L2PGAP==debug_piece_ppa || map_ppa*L2PGAP+1==debug_piece_ppa){
		static int cnt=0;
		printf("%u ", should_abort?++cnt:cnt);
		EPRINT("validate map here!\n", false);
	}
#endif

	for(uint32_t i=0; i<L2PGAP; i++){
		if(!bm->populate_bit(bm, map_ppa*L2PGAP+i) && should_abort){
			EPRINT("bit error", true);
		}
	}
}

void invalidate_map_ppa(blockmanager *bm, uint32_t map_ppa, bool should_abort){
#ifdef LSM_DEBUG
	if(map_ppa*L2PGAP==debug_piece_ppa || map_ppa*L2PGAP+1==debug_piece_ppa){
		static int cnt=0;
		printf("%u ", should_abort?++cnt:cnt);
		EPRINT("invalidate map here!\n", false);
	}
#endif

	for(uint32_t i=0; i<L2PGAP; i++){
		if(!bm->unpopulate_bit(bm, map_ppa*L2PGAP+i)){
			if(should_abort){
				EPRINT("bit error", true);
			} 
			else{
				bm->invalidate_number_decrease(bm, map_ppa*L2PGAP+i);
			}
		}
	}
}

uint32_t page_manager_get_new_ppa(page_manager *pm, bool is_map, uint32_t type){
	uint32_t res;
	blockmanager *bm=pm->bm;
	bool temp_used=false;
	__segment *seg;
retry:
	if(!LSM.function_test_flag && pm->temp_data_segment && !is_map){
		temp_used=true;
		seg=pm->temp_data_segment;
	}
	else{
		seg=is_map?pm->current_segment[MAP_S] : pm->current_segment[DATA_S];
	}
	if(!seg || bm->check_full(bm, seg, MASTER_PAGE)){
		if(temp_used){
			temp_used=false;
			free(pm->temp_data_segment);
			pm->temp_data_segment=NULL;
			goto retry;
		}
		if(bm->is_gc_needed(bm)){
			//EPRINT("before get ppa, try to gc!!\n", true);
			if(__do_gc(pm,is_map, is_map?1:(1+1))){ //just trim
				free(seg);
				pm->current_segment[is_map?MAP_S:DATA_S]=bm->get_segment(bm,false);
				goto retry;
			}
			else{ //copy trim
				
			}
			if(pm->current_segment[is_map?MAP_S:DATA_S] &&
					pm->current_segment[is_map?MAP_S:DATA_S]->used_page_num<_PPS){
			
			}
			else{
				pm->current_segment[is_map?MAP_S:DATA_S]=bm->get_segment(bm,false);
			}
			
		}
		else{
			if(seg){
				free(seg);
			}
			pm->current_segment[is_map?MAP_S:DATA_S]=bm->get_segment(bm,false);
		}

		pm->seg_type_checker[pm->current_segment[is_map?MAP_S:DATA_S]->seg_idx]=type;
		goto retry;
	}
	res=bm->get_page_num(bm, seg);
	return res;
}

__segment *page_manager_get_seg(page_manager *pm, bool is_map, uint32_t type){
	blockmanager *bm=pm->bm;
	bool temp_used=false;
	__segment *seg;
retry:
	if(pm->temp_data_segment && !is_map){
		temp_used=true;
		seg=pm->temp_data_segment;
	}
	else{
		seg=is_map?pm->current_segment[MAP_S] : pm->current_segment[DATA_S];
	}
	if(!seg || bm->check_full(bm, seg, MASTER_PAGE)){
		if(temp_used){
			free(pm->temp_data_segment);
			temp_used=false;
			pm->temp_data_segment=NULL;
			goto retry;
		}
		if(bm->is_gc_needed(bm)){
			//EPRINT("before get ppa, try to gc!!\n", true);
			if(__do_gc(pm,is_map, _PPS)){ //just trim
				free(seg);
			}
			else{ //copy trim
				
			}
			pm->current_segment[is_map?MAP_S:DATA_S]=bm->get_segment(bm,false);
		}
		else{
			if(seg){
				free(seg);
			}
			pm->current_segment[is_map?MAP_S:DATA_S]=bm->get_segment(bm,false);
		}

		pm->seg_type_checker[pm->current_segment[is_map?MAP_S:DATA_S]->seg_idx]=type;
		goto retry;
	}
	if(temp_used){
		pm->temp_data_segment=NULL;
	}
	else{
		pm->current_segment[is_map?MAP_S:DATA_S]=NULL;
	}
	return seg;
}

__segment *page_manager_get_seg_for_bis(page_manager *pm,  uint32_t type){
retry:
	__segment *seg=page_manager_get_seg(pm, false, type);
	if(_PPS-seg->used_page_num<=1){
		goto retry;	
	}
	return seg;
}

uint32_t page_manager_get_new_ppa_from_seg(page_manager *pm, __segment *seg){
	uint32_t res;
	blockmanager *bm=pm->bm;
	if(bm->check_full(bm, seg, MASTER_PAGE)){
		EPRINT("plz check gc before get new_ppa", true);
	}
	res=bm->get_page_num(bm, seg);
	return res;
}

uint32_t page_manager_pick_new_ppa(page_manager *pm, bool is_map, uint32_t type){
	blockmanager *bm=pm->bm;
	bool temp_used=false;
	__segment *seg;
retry:
	if(pm->temp_data_segment && !is_map){
		temp_used=true;
		seg=pm->temp_data_segment;
	}
	else{
		seg=is_map?pm->current_segment[MAP_S] : pm->current_segment[DATA_S];
	}

	if(!seg || bm->check_full(bm, seg, MASTER_PAGE)){
		if(temp_used){
			temp_used=false;
			free(pm->temp_data_segment);
			pm->temp_data_segment=NULL;
			goto retry;
		}
		if(bm->is_gc_needed(bm)){
			//EPRINT("before get ppa, try to gc!!\n", true);
			if(__do_gc(pm,is_map, is_map?1:(1+1))){ //just trim
				free(seg);
			}
			else{ //copy trim
				
			}
			pm->current_segment[is_map?MAP_S:DATA_S]=bm->get_segment(bm,false);
		}
		else{
			if(seg){
				free(seg);
			}
			pm->current_segment[is_map?MAP_S:DATA_S]=bm->get_segment(bm,false);
		}

		pm->seg_type_checker[pm->current_segment[is_map?MAP_S:DATA_S]->seg_idx]=type;
		goto retry;
	}
	return bm->pick_page_num(bm, seg);
}

uint32_t page_manager_pick_new_ppa_from_seg(page_manager *pm, __segment *seg){
	uint32_t res;
	blockmanager *bm=pm->bm;
	if(bm->check_full(bm, seg, MASTER_PAGE)){
		EPRINT("plz check gc before get new_ppa", true);
	}
	res=bm->pick_page_num(bm, seg);
	return res;
}

bool page_manager_is_gc_needed(page_manager *pm, uint32_t needed_page, 
		bool is_map){
	blockmanager *bm=pm->bm;
	__segment *seg=is_map?pm->current_segment[MAP_S] : pm->current_segment[DATA_S];
	if(seg->used_page_num + needed_page>= _PPS) return true;
	return bm->check_full(bm, seg, MASTER_PAGE) && bm->is_gc_needed(bm); 
}


uint32_t page_manager_get_remain_page(page_manager *pm, bool ismap){
	if(ismap){
		return _PPS-pm->current_segment[MAP_S]->used_page_num;
	}
	else{
		if(pm->temp_data_segment){
			return _PPS-pm->temp_data_segment->used_page_num;
		}
		return _PPS-pm->current_segment[DATA_S]->used_page_num;
	}
}

uint32_t page_aligning_data_segment(page_manager *pm){
	if(pm->temp_data_segment){
		free(pm->temp_data_segment);
		pm->temp_data_segment=NULL;
	}

	if(!pm->current_segment[DATA_S] || _PPS-pm->current_segment[DATA_S]->used_page_num < 2){
		free(pm->current_segment[DATA_S]);
		page_manager_move_next_seg(LSM.pm, false, false, DATASEG);
	}

	return _PPS-pm->current_segment[DATA_S]->used_page_num;
}

uint32_t page_manager_get_total_remain_page(page_manager *pm, bool ismap, bool include_inv_block){
	if(ismap){
		return pm->bm->remain_free_page(pm->bm, pm->current_segment[MAP_S]);
	}
	else{
		uint32_t inv_blk_num=include_inv_block?pm->bm->get_invalidate_blk_number(pm->bm):0;
		if(pm->current_segment[DATA_S]){
			return (pm->bm->remain_free_page(pm->bm, pm->current_segment[DATA_S])+
					(pm->temp_data_segment?_PPS-pm->temp_data_segment->used_page_num:0))+inv_blk_num*_PPS;
		}
		else{
			return pm->bm->remain_free_page(pm->bm, pm->temp_data_segment)+inv_blk_num*_PPS;
		}
	}
}

uint32_t page_manager_get_reserve_new_ppa(page_manager *pm, bool ismap, uint32_t seg_idx){
	blockmanager *bm=pm->bm;
retry:
	__segment *seg=ismap?pm->current_segment[MAP_S] : pm->current_segment[DATA_S];

	if(!seg || bm->check_full(bm, seg, MASTER_PAGE) || seg->seg_idx==seg_idx){
		if(seg){
			free(seg);
		}
		pm->current_segment[ismap?MAP_S:DATA_S]=pm->reserve_segment[ismap?MAP_S:DATA_S];
		pm->reserve_segment[ismap?MAP_S:DATA_S]=NULL;
		//pm->current_segment[ismap?MAP_S:DATA_S]=bm->get_segment(bm,false);
		pm->seg_type_checker[pm->current_segment[ismap?MAP_S:DATA_S]->seg_idx]=ismap?MAPSEG:DATASEG;
		goto retry;
	}
	return bm->get_page_num(bm, seg);
}

uint32_t page_manager_get_reserve_remain_ppa(page_manager *pm, bool ismap, uint32_t seg_idx){
	blockmanager *bm=pm->bm;
retry:
	__segment *seg=ismap?pm->current_segment[MAP_S] : pm->current_segment[DATA_S];
	
	if(!seg || bm->check_full(bm, seg, MASTER_PAGE) || seg->seg_idx==seg_idx){
		if(seg){
			free(seg);
		}
		pm->current_segment[ismap?MAP_S:DATA_S]=pm->reserve_segment[ismap?MAP_S:DATA_S];
		pm->reserve_segment[ismap?MAP_S:DATA_S]=NULL;
		//pm->current_segment[ismap?MAP_S:DATA_S]=bm->get_segment(bm,false);
		pm->seg_type_checker[pm->current_segment[ismap?MAP_S:DATA_S]->seg_idx]=ismap?MAPSEG:DATASEG;
		goto retry;
	}
	return _PPS-seg->used_page_num;
}

uint32_t page_manager_change_reserve(page_manager *pm, bool ismap){
	blockmanager *bm=pm->bm;
	if(pm->reserve_segment[ismap?MAP_S:DATA_S]==NULL){
		pm->reserve_segment[ismap?MAP_S:DATA_S]=bm->change_reserve(bm, pm->current_segment[ismap?MAP_S:DATA_S]);
	}
	return 1;
}

uint32_t page_manager_move_next_seg(page_manager *pm, bool ismap, bool isreserve, uint32_t type){
	if(isreserve){
		pm->current_segment[ismap?MAP_S:DATA_S]=pm->reserve_segment[ismap?MAP_S:DATA_S];
		pm->reserve_segment[ismap?MAP_S:DATA_S]=NULL;
	}
	else{
		pm->current_segment[ismap?MAP_S:DATA_S]=pm->bm->get_segment(pm->bm, false);
		pm->seg_type_checker[pm->current_segment[ismap?MAP_S:DATA_S]->seg_idx]=type;
	}
	return 1;
}


bool __gc_mapping(page_manager *pm, blockmanager *bm, __gsegment *victim);
bool __gc_data(page_manager *pm, blockmanager *bm, __gsegment *victim);

void *gc_end_req(algo_req* req);

static inline gc_read_node *gc_read_node_init(bool ismapping, uint32_t ppa){
	gc_read_node *res=(gc_read_node*)malloc(sizeof(gc_read_node));
	res->is_mapping=ismapping;
	res->data=inf_get_valueset(NULL, FS_MALLOC_R, PAGESIZE);
	res->piece_ppa=ppa;
	res->lba=UINT32_MAX;
	fdriver_lock_init(&res->done_lock, 0);
	return res;
}

static inline void gc_read_node_free(gc_read_node *gn){
	//inf_free_valueset(gn->data, FS_MALLOC_R);
	fdriver_destroy(&gn->done_lock);
	free(gn);
}

static void gc_issue_read_node(gc_read_node *gn, lower_info *li){
	algo_req *req=(algo_req*)malloc(sizeof(algo_req));
	req->param=(void*)gn;
	req->end_req=gc_end_req;
	if(gn->is_mapping){
		req->type=GCMR;
		li->read(gn->piece_ppa, PAGESIZE, gn->data, ASYNC, req);
	}
	else{
		req->type=GCDR;
		li->read(PIECETOPPA(gn->piece_ppa), PAGESIZE, gn->data, ASYNC, req);
	}
}

static void gc_issue_write_node(uint32_t ppa, value_set *data, bool ismap, lower_info *li){
	algo_req *req=(algo_req*)malloc(sizeof(algo_req));
	req->param=(void*)data;
	req->type=ismap?GCMW:GCDW;
	req->end_req=gc_end_req;
	li->write(ppa, PAGESIZE, data, ASYNC, req);
}

void *gc_end_req(algo_req *req){
	gc_read_node *gn;
	value_set *v;
	switch(req->type){
		case GCMR:
		case GCDR:
			gn=(gc_read_node*)req->param;
			fdriver_unlock(&gn->done_lock);
			break;
		case GCDW:
		case GCMW:
			v=(value_set*)req->param;
			inf_free_valueset(v, FS_MALLOC_R);
			break;
	}
	free(req);
	return NULL;
}


bool  __do_gc(page_manager *pm, bool ismap, uint32_t target_page_num){
	bool res=false;
	__gsegment *victim_target;
	std::queue<uint32_t> temp_queue;
	std::queue<uint32_t> diff_type_queue;
	uint32_t seg_idx;
	uint32_t remain_page=0;
retry:
	victim_target=pm->bm->get_gc_target(pm->bm);
	if(victim_target==NULL){
		lsmtree_seg_debug(&LSM);
	}
	seg_idx=victim_target->seg_idx;

	if(ismap){
		if(pm->seg_type_checker[seg_idx]!=MAPSEG &&  
				(pm->current_segment[DATA_S]==NULL || pm->current_segment[DATA_S]->seg_idx!=seg_idx) && 
				victim_target->invalidate_number==victim_target->validate_number){
			goto out;
		}
		else if(pm->seg_type_checker[seg_idx]==MAPSEG){
			goto out;
		}
	}
	else{
		if(pm->seg_type_checker[seg_idx]==MAPSEG &&  
				(pm->current_segment[MAP_S]==NULL || pm->current_segment[MAP_S]->seg_idx!=seg_idx) && 
				victim_target->invalidate_number==victim_target->validate_number){
			goto out;
		}
		else if(pm->seg_type_checker[seg_idx]!=MAPSEG){
			goto out;
		}
	}

retry_logic:
	free(victim_target);
	if(ismap && pm->seg_type_checker[seg_idx]!=MAPSEG){
		diff_type_queue.push(seg_idx);
	}
	else if(!ismap && pm->seg_type_checker[seg_idx]==MAPSEG){
		diff_type_queue.push(seg_idx);
	}
	else{
		temp_queue.push(seg_idx);
	}
	goto retry;

out:
	if(LSM.gc_unavailable_seg[seg_idx]){
//		if(LSM.global_debug_flag) printf("[%lu] %u is locked\n", temp_queue.size(), seg_idx);
		goto retry_logic;
	}
	else if(victim_target->invalidate_number<=L2PGAP*2){
//		if(LSM.global_debug_flag) printf("[%lu] %u is not enough invalid\n", temp_queue.size(), seg_idx);
		goto retry_logic;
	}

	switch(pm->seg_type_checker[seg_idx]){
		case DATASEG:
		case SEPDATASEG:
			while(diff_type_queue.size()){
				seg_idx=diff_type_queue.front();
				pm->bm->reinsert_segment(pm->bm, seg_idx);
				diff_type_queue.pop();
			}
			res=__gc_data(pm, pm->bm, victim_target);
			remain_page=page_manager_get_total_remain_page(LSM.pm, false, false);
			break;
		case MAPSEG:
			while(diff_type_queue.size()){
				seg_idx=diff_type_queue.front();
				pm->bm->reinsert_segment(pm->bm, seg_idx);
				diff_type_queue.pop();
			}
			res=__gc_mapping(pm, pm->bm, victim_target);
			remain_page=page_manager_get_total_remain_page(LSM.pm, true, false);
			break;
	}
	while(temp_queue.size()){
		seg_idx=temp_queue.front();
		pm->bm->reinsert_segment(pm->bm, seg_idx);
		temp_queue.pop();
	}
	if(remain_page<target_page_num)
		goto retry;

	return res;
}

bool __gc_mapping(page_manager *pm, blockmanager *bm, __gsegment *victim){
	LSM.monitor.gc_mapping++;
//	printf("gc_mapping:%u (seg_idx:%u)\n", LSM.monitor.gc_mapping, victim->seg_idx);
	if(victim->invalidate_number==_PPS*L2PGAP || victim->all_invalid){
#ifdef LSM_DEBUG
		if(debug_piece_ppa/L2PGAP/_PPS==victim->seg_idx){
			printf("gc_mapping:%u (seg_idx%u) clean\n", LSM.monitor.gc_mapping, victim->seg_idx);
		}
#endif
		bm->trim_segment(bm, victim, bm->li);
		page_manager_change_reserve(pm, true);
		return true;
	}
	else if(victim->invalidate_number>_PPS*L2PGAP){
		EPRINT("????", true);
	}
	
	//static int cnt=0;
	//printf("mapping gc:%u\n", ++cnt);

	std::queue<gc_read_node*> *gc_target_queue=new std::queue<gc_read_node*>();
	uint32_t bidx;
	uint32_t pidx, page;
	gc_read_node *gn;

	for_each_page_in_seg(victim, page, bidx, pidx){
		if(bm->is_invalid_page(bm, page*L2PGAP)) continue;
		else{
			gn=gc_read_node_init(true, page);
			gc_target_queue->push(gn);
			gc_issue_read_node(gn, bm->li);
		}
	}

	while(!gc_target_queue->empty()){
		fdriver_lock(&gn->done_lock);
		gn=gc_target_queue->front();

		oob_manager *oob=get_oob_manager(gn->piece_ppa);
		gn->lba=oob->lba[0];

//		char *oob=bm->get_oob(bm, gn->piece_ppa);
//		gn->lba=*(uint32_t*)oob;
		sst_file *target_sst_file=lsmtree_find_target_sst_mapgc(gn->lba, gn->piece_ppa);
		target_sst_file->sequential_file=false;
		invalidate_map_ppa(pm->bm, gn->piece_ppa, true);

		uint32_t ppa=page_manager_get_reserve_new_ppa(pm, true, victim->seg_idx);
		target_sst_file->file_addr.map_ppa=ppa;
		validate_map_ppa(pm->bm, ppa, gn->lba, target_sst_file->end_lba, true);
		gc_issue_write_node(ppa, gn->data, true, bm->li);
		gn->data=NULL;
		gc_read_node_free(gn);
		gc_target_queue->pop();
	}
	
	bm->trim_segment(bm, victim, bm->li);
	page_manager_change_reserve(pm, true);

	delete gc_target_queue;
	return false;
}

typedef struct gc_sptr_node{
	sst_file *sptr;
	uint32_t lev_idx;
	uint32_t version;
	uint32_t sidx;
	write_buffer *wb;
}gc_sptr_node;

static gc_sptr_node * gc_sptr_node_init(sst_file *sptr, uint32_t validate_num, 
		uint32_t lev_idx, uint32_t version, uint32_t sidx){
	gc_sptr_node *res=(gc_sptr_node*)malloc(sizeof(gc_sptr_node));
	//res->gc_kv_node=new std::queue<key_value_pair>();
	res->sptr=sptr;
	res->wb=write_buffer_init_for_gc(_PPS*L2PGAP, LSM.pm, GC_WB,LSM.param.plr_rhp);
	res->lev_idx=lev_idx;
	res->version=version;
	res->sidx=sidx;
	return res;
}

static inline void gc_sptr_node_free(gc_sptr_node *gsn){
	write_buffer_free(gsn->wb);
	free(gsn);
}

static void insert_target_sptr(gc_sptr_node* gsn, uint32_t lba, char *value){
	write_buffer_insert_for_gc(gsn->wb, lba, value);
}

static void move_sptr(gc_sptr_node *gsn, uint32_t seg_idx, uint32_t lev_idx, 
		uint32_t version, uint32_t sidx){
	// if no kv in gsn sptr should be checked trimed;
	if(gsn->wb->buffered_entry_num==0){
		gsn->sptr->trimed_sst_file=true;
	}
	else{
		sst_file *sptr=NULL;
		uint32_t round=0;
		uint32_t ridx=version_to_ridx(LSM.last_run_version, version, lev_idx);
		while(gsn->wb->buffered_entry_num){
			sptr=sst_init_empty(BLOCK_FILE);
			uint32_t map_num=gsn->wb->buffered_entry_num/KP_IN_PAGE+
			(gsn->wb->buffered_entry_num%KP_IN_PAGE?1:0);
			key_ptr_pair **kp_set=(key_ptr_pair**)malloc(sizeof(key_ptr_pair*)*map_num);
			uint32_t kp_set_idx=0;
			key_ptr_pair *now_kp_set;
			bool force_stop=false;
			while((now_kp_set=write_buffer_flush_for_gc(gsn->wb, false, seg_idx, &force_stop, 
							kp_set_idx, NULL))){
				kp_set[kp_set_idx]=now_kp_set;
				kp_set_idx++;
				if(force_stop) break;
			}

			uint32_t map_ppa;
			map_range *mr_set=(map_range*)malloc(sizeof(map_range) * kp_set_idx);
			for(uint32_t i=0; i<kp_set_idx; i++){
				algo_req *write_req=(algo_req*)malloc(sizeof(algo_req));
				value_set *data=inf_get_valueset((char*)kp_set[i], FS_MALLOC_W, PAGESIZE);
				write_req->type=GCDW;
				write_req->param=data;
				write_req->end_req=gc_end_req;
				map_ppa=page_manager_get_reserve_new_ppa(LSM.pm, false, seg_idx);

				mr_set[i].start_lba=kp_set[i][0].lba;
				mr_set[i].end_lba=kp_get_end_lba((char*)kp_set[i]);
				mr_set[i].ppa=map_ppa;
				validate_map_ppa(LSM.pm->bm, map_ppa, mr_set[i].start_lba,mr_set[i].end_lba, true);
				io_manager_issue_write(map_ppa, data, write_req, false);
			}

			//	free(sptr->block_file_map);
			sptr->block_file_map=mr_set;

			sptr->file_addr.piece_ppa=kp_set[0][0].piece_ppa;
			sptr->end_ppa=map_ppa;
			if(sptr->file_addr.piece_ppa/L2PGAP/_PPS != sptr->end_ppa/_PPS){
				EPRINT("should same segment" ,true);
			}
			sptr->map_num=kp_set_idx;
			sptr->start_lba=kp_set[0][0].lba;
			sptr->end_lba=mr_set[kp_set_idx-1].end_lba;
			sptr->_read_helper=gsn->wb->rh;
			read_helper_insert_done(gsn->wb->rh);
			if(round==0){
				level_sptr_update_in_gc(LSM.disk[lev_idx], ridx, sidx, sptr);
			}
			else{
				level_sptr_add_at_in_gc(LSM.disk[lev_idx], ridx, sidx+round, sptr);
			}

			for(uint32_t i=0; i<kp_set_idx; i++){
				free(kp_set[i]);
			}

			gsn->wb->rh=NULL;
			round++;
			free(sptr);
			free(kp_set);
		}
	}
	gc_sptr_node_free(gsn);
}

bool __gc_data(page_manager *pm, blockmanager *bm, __gsegment *victim){
	LSM.monitor.gc_data++;
	if(victim->invalidate_number==victim->validate_number){
		bm->trim_segment(bm, victim, bm->li);
		page_manager_change_reserve(pm, false);
		return true;
	}
	else if(victim->invalidate_number>_PPS*L2PGAP){
		EPRINT("????", true);
	}
	printf("gc_data:%u (seg_idx:%u)\n", LSM.monitor.gc_data, victim->seg_idx);
	std::queue<gc_read_node*> *gc_target_queue=new std::queue<gc_read_node*>();
	uint32_t bidx;
	uint32_t pidx, page;
	gc_read_node *gn;
	bool should_read;
	uint32_t read_page_num=0;
	uint32_t valid_piece_ppa_num=0;
	uint32_t temp[_PPS*L2PGAP];

	for_each_page_in_seg(victim, page, bidx, pidx){
		should_read=false;
		for(uint32_t i=0; i<L2PGAP; i++){
			if(bm->is_invalid_page(bm, page*L2PGAP+i)) continue;
			else{
				should_read=true;
				temp[valid_piece_ppa_num++]=page*L2PGAP+i;
			}
		}
		if(should_read){
			read_page_num++;
			gn=gc_read_node_init(false, page*L2PGAP);
			gc_target_queue->push(gn);
			gc_issue_read_node(gn, bm->li);	
		}
	}

	if(((_PPS*L2PGAP-victim->invalidate_number)-(victim->validate_number-victim->invalidate_number))>=(1+1)){
		if(valid_piece_ppa_num+victim->invalidate_number!=victim->validate_number){
			printf("valid list\n");
			for(uint32_t i=0; i<BPS; i++){
				printf("%u invalid_number:%u validate_number:%u\n", i,victim->blocks[i]->invalidate_number, victim->blocks[i]->validate_number);
			}
			printf("_PPS:%u BPS:%u\n", _PPS, BPS);
			EPRINT("not match validation!",true);
		}
	}

	value_set **free_target=(value_set**)malloc(sizeof(value_set*)*read_page_num);

	uint32_t* oob_lba;
	uint32_t* oob_version;
	uint32_t ppa, piece_ppa;
	uint32_t q_idx=0;
	sst_file *sptr=NULL;
	uint32_t recent_version, target_version;
	uint32_t sptr_idx=0, level_idx=UINT32_MAX;

	write_buffer *wisckey_node_wb=NULL;
	std::map<uint32_t, gc_mapping_check_node *> *gc_kv_map=new std::map<uint32_t, gc_mapping_check_node*>();

	gc_mapping_check_node *gmc=NULL;

	gc_sptr_node *gsn=NULL;
	while(!gc_target_queue->empty()){
		gn=gc_target_queue->front();
		fdriver_lock(&gn->done_lock);
		ppa=PIECETOPPA(gn->piece_ppa);
		oob_manager *oob=get_oob_manager(ppa);
		oob_lba=oob->lba;
		oob_version=oob->version;

		for(uint32_t i=0; i<L2PGAP; i++){
			if(bm->is_invalid_page(bm, ppa*L2PGAP+i)) continue;
			else{
				if(oob_lba[i]==UINT32_MAX) continue;

				piece_ppa=ppa*L2PGAP+i;
				gn->piece_ppa=piece_ppa;
				gn->lba=oob_lba[i];
				gn->version=oob_version[i];
				if(gn->lba==debug_lba){
					printf("target is moved!\n");
				}
				
	//			gc_debug_checking(gn);

				/*check invalidation*/
				if(!sptr || (sptr && sptr->end_ppa*L2PGAP<piece_ppa)){ //first round or new sst_file
					if(sptr){
						if(gsn->wb->buffered_entry_num){
							move_sptr(gsn,victim->seg_idx, gsn->lev_idx, gsn->version, gsn->sidx);
						}
						else{
							level_sptr_remove_at_in_gc(LSM.disk[gsn->lev_idx], gsn->version, gsn->sidx);
							gc_sptr_node_free(gsn);
						}
					}
					sptr=lsmtree_find_target_normal_sst_datagc(gn->lba, gn->piece_ppa, &level_idx, &target_version, &sptr_idx);
					gsn=gc_sptr_node_init(sptr, valid_piece_ppa_num, level_idx, target_version, sptr_idx);
				}

				/*for wisckey level*/
				if(sptr==NULL || !(sptr && sptr->file_addr.piece_ppa<=piece_ppa && sptr->end_ppa*L2PGAP>=piece_ppa)){
					lsmtree_level_summary(&LSM);
					EPRINT("should I implement?", true);
				
					/*checking other level*/
					gmc=(gc_mapping_check_node*)malloc(sizeof(gc_mapping_check_node));
					gmc->data_ptr=&gn->data->value[LPAGESIZE*i];
					gmc->piece_ppa=piece_ppa;
					gmc->new_piece_ppa=UINT32_MAX;
					gmc->lba=oob_lba[i];
					gmc->map_ppa=UINT32_MAX;
					gmc->type=MAP_CHECK_FLUSHED_KP;
					gmc->level=LSM.param.LEVELN-(1+1);
					if(!wisckey_node_wb){
						wisckey_node_wb=write_buffer_init(_PPS*L2PGAP, pm, GC_WB);	
					}
					gmc->mapping_data=NULL;
					write_buffer_insert_for_gc(wisckey_node_wb, gmc->lba, gmc->data_ptr);
					gc_kv_map->insert(std::pair<uint32_t, gc_mapping_check_node*>(gmc->lba, gmc));
				}
				else{
					//sptr->trimed_sst_file=true;
					if(is_map_ppa(sptr, piece_ppa/L2PGAP)){
						invalidate_map_ppa(pm->bm, piece_ppa/L2PGAP, true);
						/*mapaddress*/
						continue;
					}

					if(sptr->end_ppa/_PPS != victim->seg_idx){
						EPRINT("target sptr should be in victim", true);
					}

					recent_version=version_map_lba(LSM.last_run_version, oob_lba[i]);

					/*should figure out map ppa*/
					if(version_compare(LSM.last_run_version, recent_version, target_version)<=0){
						invalidate_kp_entry(gn->lba, gn->piece_ppa, UINT32_MAX, true);
						insert_target_sptr(gsn, gn->lba, &gn->data->value[LPAGESIZE*i]);
					}
					else{
						invalidate_kp_entry(gn->lba, gn->piece_ppa, UINT32_MAX, true);
						continue; //already invalidate
					}
				}
			}
		}
		free_target[q_idx++]=gn->data;
		gc_read_node_free(gn);
		gc_target_queue->pop();
	}
	if(sptr){
		if(gsn->wb->buffered_entry_num){
			move_sptr(gsn,victim->seg_idx, gsn->lev_idx, gsn->version, gsn->sidx);
		}
		else{
			level_sptr_remove_at_in_gc(LSM.disk[gsn->lev_idx], gsn->version, gsn->sidx);
			gc_sptr_node_free(gsn);
		}
	}

	if(gc_kv_map->size()){
		printf("[%u] victim invalidation cnt:%u GMC target cnt:%lu ", LSM.monitor.gc_data,victim->invalidate_number, gc_kv_map->size());
		EPRINT("debug", false);
		//LSM.global_debug_flag=true;
	}
	gc_helper_for_normal(gc_kv_map, wisckey_node_wb, victim->seg_idx); 

	bm->trim_segment(bm, victim, bm->li);
	page_manager_change_reserve(pm, false);

	for(uint32_t i=0; i<q_idx; i++){
		inf_free_valueset(free_target[i], FS_MALLOC_R);
	}
	free(free_target);

	delete gc_target_queue;
	delete gc_kv_map;
	if(wisckey_node_wb){
		write_buffer_free(wisckey_node_wb);
	}
	return false;
}
