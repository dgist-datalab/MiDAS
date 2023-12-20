#include "seq_block_manager.h"
//static uint32_t age=UINT_MAX;
uint32_t seq_pt_create(struct blockmanager *bm, int pnum, int *epn, lower_info *li){
	bm->li=li;
	sbm_pri *p=(sbm_pri*)malloc(sizeof(sbm_pri));
	p->seq_block=(__block*)calloc(sizeof(__block),_NOB);
	p->logical_segment=(block_set*)calloc(sizeof(block_set), _NOS);

	p->pnum=pnum;

	int glob_block_idx=0;
	for(int i=0; i<_NOS; i++){
		for(int j=0; j<BPS; j++){
			__block *b=&p->seq_block[glob_block_idx];
			b->block_num=i*BPS+j;
			b->bitset=(uint8_t*)calloc(_PPB*L2PGAP/8,1);
			p->logical_segment[i].blocks[j]=&p->seq_block[glob_block_idx];
			glob_block_idx++;
		}
		p->logical_segment[i].total_invalid_number=0;
	}

	p->max_heap_pt=(mh**)malloc(sizeof(mh*)*pnum);
	p->free_logical_seg_q_pt=(queue**)malloc(sizeof(queue*)*pnum);

	int global_seg_idx=0;
	for(int i=pnum-1; i>=0; i--){
		//mh_init(&p->max_heap_pt[i], epn[i], seq_mh_swap_hptr, seq_mh_assign_hptr, seq_get_cnt);
		mh_init(&p->max_heap_pt[i], epn[i], seq_mh_swap_hptr, seq_mh_assign_hptr, NULL, NULL);
		q_init(&p->free_logical_seg_q_pt[i], epn[i]);
		for(int j=0; j<epn[i]; j++){
			p->logical_segment[global_seg_idx].type=i;
			q_enqueue((void*)&p->logical_segment[global_seg_idx++], p->free_logical_seg_q_pt[i]);
		}
	}

	bm->private_data=(void*)p;
	return 1;
}

uint32_t seq_pt_destroy(struct blockmanager *bm){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	free(p->seq_block);
	free(p->logical_segment);
	for(int i=0; i<p->pnum; i++){
		mh_free(p->max_heap_pt[i]);
		q_free(p->free_logical_seg_q_pt[i]);
	}
	free(p);
	return 1;
}

__segment* seq_pt_get_segment (struct blockmanager*bm, int pt_num, bool isreserve){
	__segment* res=(__segment*)malloc(sizeof(__segment));
	sbm_pri *p=(sbm_pri*)bm->private_data;
	
	block_set *free_block_set=(block_set*)q_dequeue(p->free_logical_seg_q_pt[pt_num]);
	if(!free_block_set){
		printf("no free block error\n");
		abort();
	}

	if(!isreserve)
		mh_insert_append(p->max_heap_pt[pt_num], (void*)free_block_set);

	memcpy(res->blocks, free_block_set->blocks, sizeof(__block*)*BPS);
	res->seg_idx=res->blocks[0]->block_num/BPS;

	res->now=0;
	res->max=BPS;
	res->invalid_blocks=0;
	res->used_page_num=0;
	
	return res;
}

__gsegment* seq_pt_get_gc_target (struct blockmanager* bm, int pt_num){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	__gsegment* res=(__gsegment*)malloc(sizeof(__gsegment));
	res->invalidate_number=0;

	mh_construct(p->max_heap_pt[pt_num]);
	block_set* target=(block_set*)mh_get_max(p->max_heap_pt[pt_num]);
	res->invalidate_number=target->total_invalid_number;
	res->validate_number=target->total_valid_number;

	memcpy(res->blocks, target->blocks, sizeof(__block*)*BPS);

	res->seg_idx=res->blocks[0]->block_num/BPS;
	res->now=res->max=0;
	return res;
}

void seq_pt_trim_segment(struct blockmanager* bm, int pt_num, __gsegment *gs, lower_info* li){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	uint32_t segment_startblock_number=gs->blocks[0]->block_num;
	uint32_t segment_idx=segment_startblock_number/BPS;

	for(int i=0; i<BPS; i++){
		__block *b=gs->blocks[i];
		b->invalidate_number=0;
		b->validate_number=0;
		b->now=0;
		memset(b->bitset,0,_PPB*L2PGAP/8);
		memset(b->oob_list,0,sizeof(b->oob_list));
	}

	block_set *bs=&p->logical_segment[segment_idx];
	bs->total_invalid_number=0;

	q_enqueue((void*)bs, p->free_logical_seg_q_pt[pt_num]);

	li->trim_block(segment_startblock_number*_PPB, ASYNC);
	free(gs);
}

int seq_pt_remain_page(struct blockmanager* bm, __segment *active,int pt_num){
	int res=0;
	sbm_pri *p=(sbm_pri*)bm->private_data;
	res+=(p->free_logical_seg_q_pt[pt_num]->size * _PPS);
	res+=_PPS-active->used_page_num;
	return res;
}

bool seq_pt_isgc_needed(struct blockmanager* bm, int pt_num){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	if(p->free_logical_seg_q_pt[pt_num]->size==0) return true;
	return false;

}

__segment* seq_change_pt_reserve(struct blockmanager *bm,int pt_num, __segment *reserve){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	uint32_t segment_start_block_number=reserve->blocks[0]->block_num;
	uint32_t segment_idx=segment_start_block_number/BPS;

	block_set *bs=&p->logical_segment[segment_idx];
	mh_insert_append(p->max_heap_pt[pt_num], (void*)bs);
	return seq_pt_get_segment(bm, pt_num, true);
}

uint32_t seq_pt_reserve_to_free(struct blockmanager* bm, int pt_num, __segment *reserve){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	__block *b=NULL;
	uint32_t idx=0;
	for_each_block(reserve, b, idx){
		if(b->invalidate_number){
			printf("it can't have invalid_number\n");
			abort();
		}
		b->invalidate_number=0;
		b->validate_number=0;
		b->now=0;
	}
	uint32_t segment_idx=reserve->blocks[0]->block_num/BPS;
	block_set *bs=&p->logical_segment[segment_idx];
	q_enqueue((void *)bs, p->free_logical_seg_q_pt[pt_num]);
	free(reserve);
	return 1;
}
