#include "base_block_manager.h"
#include <stdlib.h>
#include <stdio.h>
extern bb_checker checker;
struct blockmanager base_bm={
	.create=base_create,
	.q_return=NULL,
	.destroy=base_destroy,
	.get_block=base_get_block,
	.pick_block=base_pick_block,
	.get_segment=base_get_segment,
	.jy_get_time_segment=NULL,
	.jy_get_block_idx=NULL,
	.jy_get_timestamp=NULL,
	.get_page_num=base_get_page_num,
	.pick_page_num=base_pick_page_num,
	.check_full=base_check_full,
	.is_gc_needed=base_is_gc_needed,
	
	.get_free_segment_number=NULL,

	.get_gc_target=base_get_gc_target,
	.jy_get_gc_target=NULL,
	.trim_segment=base_trim_segment,
	.free_segment=base_free_segment,
	.populate_bit=base_populate_bit,
	.unpopulate_bit=base_unpopulate_bit,
	.query_bit=NULL,
	.erase_bit=base_erase_bit,
	.is_valid_page=base_is_valid_page,
	.is_invalid_page=base_is_invalid_page,
	.set_oob=base_set_oob,
	.get_oob=base_get_oob,
	.change_reserve=base_change_reserve,
	.jy_change_reserve=NULL,
	.jy_add_queue=NULL,
	.jy_add_time_queue=NULL,
	.jy_move_q2h=NULL,
	.reinsert_segment=NULL,
	.jy_time_reinsert_segment=NULL,
	.remain_free_page=NULL,
	.invalidate_number_decrease=NULL,
	.get_invalidate_number=NULL,
	.get_invalidate_blk_number=NULL,

	.pt_create=NULL,
	.pt_destroy=NULL,
	.pt_get_segment=NULL,
	.pt_get_gc_target=NULL,
	.pt_trim_segment=NULL,
	.pt_remain_page=NULL,
	.pt_isgc_needed=NULL,
	.change_pt_reserve=NULL,
	.pt_reserve_to_free=NULL,
};

void base_mh_swap_hptr(void *a, void *b){
	__block *aa=(__block*)a;
	__block *bb=(__block*)b;

	void *temp=aa->hptr;
	aa->hptr=bb->hptr;
	bb->hptr=temp;
}

void base_mh_assign_hptr(void *a, void *hn){
	__block *aa=(__block*)a;
	aa->hptr=hn;
}

int base_get_cnt(void *a){
	__block *aa=(__block*)a;
	return aa->invalidate_number;
}

uint32_t base_create (struct blockmanager* bm, lower_info *li){
	bm->li=li;
	bb_checker_start(bm->li);/*check if the block is badblock*/


	bbm_pri *p=(bbm_pri*)malloc(sizeof(bbm_pri));
	p->base_block=(__block*)calloc(sizeof(__block),_NOS*PUNIT);

	int block_idx=0;
	for(int i=0; i<_NOS; i++){
		int seg_idx=bb_checker_get_segid();
		for(int j=0; j<PUNIT; j++){
			__block *b=&p->base_block[block_idx];
			b->block_num=seg_idx;
			b->punit_num=j;
			b->bitset=(uint8_t*)calloc(_PPB*L2PGAP/8,1);
			block_idx++;
		}
	}

	p->base_channel=(channel*)malloc(sizeof(channel)*PUNIT);
	for(int i=0; i<PUNIT; i++){ //assign block to channel
		channel *c=&p->base_channel[i];
		q_init(&c->free_block,_NOB/BPS);
		//mh_init(&c->max_heap,_NOB/BPS,base_mh_swap_hptr,base_mh_assign_hptr,base_get_cnt);
		mh_init(&c->max_heap,_NOB/BPS,base_mh_swap_hptr,base_mh_assign_hptr, NULL, NULL);
		for(int j=0; j<_NOB/BPS; j++){
			__block *n=&p->base_block[j*BPS+i%BPS];
			q_enqueue((void*)n,c->free_block);
			//mh_insert_append(c->max_heap,(void*)n);
		}
	}
	p->seg_map=rb_create();
	p->seg_map_idx=0;

	bm->private_data=(void*)p;
	return 1;
}

uint32_t base_destroy (struct blockmanager* bm){
	bbm_pri *p=(bbm_pri*)bm->private_data;
	free(p->base_block);

	rb_delete(p->seg_map,true);
	for(int i=0; i<BPS; i++){
		channel *c=&p->base_channel[i];
		q_free(c->free_block);
		mh_free(c->max_heap);
	}
	free(p->base_channel);
	free(p);
	return 1;
}

__block* base_get_block (struct blockmanager* bm, __segment* s){
	if(s->now+1>s->max) abort();
	return s->blocks[s->now++];
}

__segment* base_get_segment (struct blockmanager* bm, bool isreserve){
	__segment* res=(__segment*)malloc(sizeof(__segment));
	bbm_pri *p=(bbm_pri*)bm->private_data;
	for(int i=0; i<BPS; i++){
		__block *b=(__block*)q_dequeue(p->base_channel[i].free_block);
		if(!isreserve){
			mh_insert_append(p->base_channel[i].max_heap,(void*)b);
		}
		if(!b) {
			printf("lack of free block!!\n");
			abort();
		}
		res->blocks[i]=b;
		b->seg_idx=p->seg_map_idx;
	}
	res->now=0;
	res->max=BPS;
	res->invalid_blocks=0;
	res->seg_idx=p->seg_map_idx++;
	res->used_page_num=0;

	rb_insert_int(p->seg_map,res->seg_idx,(void*)res);
	return res;
}

__segment* base_change_reserve(struct blockmanager* bm,__segment *reserve){
	__segment *res=base_get_segment(bm,true);
	
	bbm_pri *p=(bbm_pri*)bm->private_data;
	__block *tblock;
	int bidx;
	for_each_block(reserve,tblock,bidx){
		mh_insert_append(p->base_channel[bidx].max_heap,(void*)tblock);
	}
	return res;
}

bool base_is_gc_needed (struct blockmanager* bm){
	bbm_pri *p=(bbm_pri*)bm->private_data;
	if(p->base_channel[0].free_block->size==0) return true;
	return false;
}

__gsegment* base_get_gc_target (struct blockmanager* bm){
	bbm_pri *p=(bbm_pri*)bm->private_data;
	__gsegment* res=(__gsegment*)malloc(sizeof(__gsegment));
	res->invalidate_number=0;
	for(int i=0; i<BPS; i++){
		mh_construct(p->base_channel[i].max_heap);
		__block *b=(__block*)mh_get_max(p->base_channel[i].max_heap);
		if(!b) abort();
		res->blocks[i]=b;
		res->invalidate_number+=b->invalidate_number;
	}
	res->now=res->max=0;
	return res;
}

void base_trim_segment (struct blockmanager* bm, __gsegment* gs, struct lower_info* li){
	bbm_pri *p=(bbm_pri*)bm->private_data;
	Redblack target_node;
	__segment *target_seg;
	for(int i=0; i<BPS; i++){
		__block *b=gs->blocks[i];
		li->trim_a_block(GETBLOCKPPA(b),ASYNC);
		b->invalidate_number=0;
		b->validate_number=0;
		b->now=0;
		memset(b->bitset,0,_PPB/8);

		memset(b->oob_list,0,sizeof(b->oob_list));

		channel* c=&p->base_channel[i];
	//	mh_insert_append(c->max_heap,(void*)b);
		q_enqueue((void*)b,c->free_block);
		rb_find_int(p->seg_map,b->seg_idx,&target_node);
		target_seg=(__segment*)target_node->item;
		target_seg->invalid_blocks++;
		if(target_seg->invalid_blocks==BPS){
			free(target_seg);
			rb_delete(target_node,true);
		}
	}
	free(gs);
}

int base_populate_bit (struct blockmanager* bm, uint32_t ppa){
	int res=1;
	bbm_pri *p=(bbm_pri*)bm->private_data;
	uint32_t bn=GETBLOCKIDX(checker,ppa/L2PGAP);
	uint32_t pn=GETPAGEIDX(ppa/L2PGAP) * L2PGAP + (ppa%L2PGAP);
	uint32_t bt=pn/8;
	uint32_t of=pn%8;

	if(p->base_block[bn].bitset[bt]&(1<<of)){
		res=0;
	}
	p->base_block[bn].bitset[bt]|=(1<<of);
	__block *b=&p->base_block[bn];
	b->validate_number++;
	return res;
}

int base_unpopulate_bit (struct blockmanager* bm, uint32_t ppa){
	int res=1;
	bbm_pri *p=(bbm_pri*)bm->private_data;
	uint32_t bn=GETBLOCKIDX(checker,ppa/L2PGAP);
	uint32_t pn=GETPAGEIDX(ppa/L2PGAP) * L2PGAP + (ppa%L2PGAP);
	uint32_t bt=pn/8;
	uint32_t of=pn%8;
	__block *b=&p->base_block[bn];

	if(!(p->base_block[bn].bitset[bt]&(1<<of))){
		res=0;
	}
	b->bitset[bt]&=~(1<<of);
	b->invalidate_number++;
	/*
	if(0<=ppa && ppa< _PPS*MAPPART_SEGS){
		if(b->invalid_number>_PPB){
			abort();
		}
	}*/
	return res;
}

int base_erase_bit (struct blockmanager* bm, uint32_t ppa){
	int res=1;
	bbm_pri *p=(bbm_pri*)bm->private_data;
	uint32_t bn=GETBLOCKIDX(checker,ppa);
	uint32_t pn=GETPAGEIDX(ppa);
	uint32_t bt=pn/8;
	uint32_t of=pn%8;
	__block *b=&p->base_block[bn];

	if(!(p->base_block[bn].bitset[bt]&(1<<of))){
		res=0;
	}
	b->bitset[bt]&=~(1<<of);
	if(0<=ppa && ppa< _PPS*MAPPART_SEGS){
		if(b->invalidate_number>_PPB){
			abort();
		}
	}
	return res;
}

bool base_is_valid_page (struct blockmanager* bm, uint32_t ppa){
	bbm_pri *p=(bbm_pri*)bm->private_data;
	uint32_t bn=GETBLOCKIDX(checker,ppa/L2PGAP);
	uint32_t pn=GETPAGEIDX(ppa/L2PGAP) * L2PGAP + (ppa%L2PGAP);
	uint32_t bt=pn/8;
	uint32_t of=pn%8;

	return p->base_block[bn].bitset[bt]&(1<<of);
}

bool base_is_invalid_page (struct blockmanager* bm, uint32_t ppa){
	return !base_is_valid_page(bm,ppa);
}

void base_set_oob(struct blockmanager* bm, char *data,int len, uint32_t ppa){
	bbm_pri *p=(bbm_pri*)bm->private_data;
	__block *b=&p->base_block[GETBLOCKIDX(checker,ppa)];
	memcpy(b->oob_list[GETPAGEIDX(ppa)].d,data,len);
}

char *base_get_oob(struct blockmanager*bm,  uint32_t ppa){
	bbm_pri *p=(bbm_pri*)bm->private_data;
	uint32_t bidx=GETBLOCKIDX(checker,ppa);
	__block *b=&p->base_block[bidx];
	return b->oob_list[GETPAGEIDX(ppa)].d;
}

void base_release_segment(struct blockmanager* bm, __segment *s){
	free(s);
}

int base_get_page_num(struct blockmanager* bm,__segment *s){
	if(s->now==0){
		if(s->blocks[BPS-1]->now==_PPB) return -1;
	}

	int blocknumber=s->now++;
	if(s->now==BPS) s->now=0;
	__block *b=s->blocks[blocknumber];
	uint32_t page=b->now++;
	int res=b->block_num;
	res+=page<<6;
	res+=b->punit_num;
	
	s->used_page_num++;
	if(page>_PPB) abort();
	bm->assigned_page++;
	return res;
}

int base_pick_page_num(struct blockmanager* bm,__segment *s){
	if(s->now==0){
		if(s->blocks[BPS-1]->now==_PPB) return -1;
	}

	int blocknumber=s->now;
	if(s->now==BPS) s->now=0;
	__block *b=s->blocks[blocknumber];
	uint32_t page=b->now;
	int res=b->block_num;
	res+=page<<6;
	res+=b->punit_num;

	if(page>_PPB) abort();
	return res;
}

bool base_check_full(struct blockmanager *bm,__segment *active, uint8_t type){
	bool res=false;
//	__block *b=active->blocks[active->now];
	switch(type){
		case MASTER_SEGMENT:
			break;
		case MASTER_PAGE:
		case MASTER_BLOCK:
			if(active->blocks[BPS-1]->now==_PPB){
				res=true;
			}
			break;
			/*
			if(active->now >= active->max){
				res=true;
				abort();
			}
			break;*/
	}
	return res;
}

__block *base_pick_block(struct blockmanager *bm, uint32_t page_num){
	bbm_pri *p=(bbm_pri*)bm->private_data;
	return &p->base_block[GETBLOCKIDX(checker,page_num)];
}

void base_free_segment(struct blockmanager*, __segment*){

}
