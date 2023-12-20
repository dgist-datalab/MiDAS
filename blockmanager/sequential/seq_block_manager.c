#include "seq_block_manager.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static uint64_t total_validate_piece_ppa;
static uint64_t total_invalidate_piece_ppa;
long cur_timestamp=1;

struct blockmanager seq_bm = {
	.create=seq_create,
	.q_return = return_heap_queue,
	.destroy=seq_destroy,
	.get_block=seq_get_block,
	.pick_block=seq_pick_block,
	.get_segment=seq_get_segment,
	.jy_get_time_segment=seq_jy_get_time_segment,
	.jy_get_block_idx=seq_jy_get_block_idx,
	.jy_get_timestamp=jy_get_timestamp,
	.get_page_num=seq_get_page_num,
	.pick_page_num=seq_pick_page_num,
	.check_full=seq_check_full,

	.is_gc_needed=seq_is_gc_needed,
	.get_free_segment_number=seq_get_free_segment_number,

	.get_gc_target=seq_get_gc_target,
	.jy_get_gc_target=seq_jy_get_gc_target,
	.trim_segment=seq_trim_segment,
	.free_segment=seq_free_segment,
	.populate_bit=seq_populate_bit,
	.unpopulate_bit=seq_unpopulate_bit,
	.query_bit=seq_query_bit,
	.erase_bit=seq_erase_bit,
	.is_valid_page=seq_is_valid_page,
	.is_invalid_page=seq_is_invalid_page,
	.set_oob=seq_set_oob,
	.get_oob=seq_get_oob,
	.change_reserve=seq_change_reserve,
	.jy_change_reserve=seq_jy_change_reserve,
	.jy_add_queue=seq_jy_add_queue,
	.jy_add_time_queue=seq_jy_add_time_queue,
	.jy_move_q2h=seq_jy_move_q2h,
	.reinsert_segment=seq_reinsert_segment,
	.jy_time_reinsert_segment=seq_time_reinsert_segment,
	
	.remain_free_page=seq_remain_free_page,
	.invalidate_number_decrease=seq_invalidate_number_decrease,
	.get_invalidate_number=seq_get_invalidate_number,
	.get_invalidate_blk_number=seq_get_invalidate_blk_number,

	.pt_create=seq_pt_create,
	.pt_destroy=seq_pt_destroy,
	.pt_get_segment=seq_pt_get_segment,
	.pt_get_gc_target=seq_pt_get_gc_target,
	.pt_trim_segment=seq_pt_trim_segment,
	.pt_remain_page=seq_pt_remain_page,
	.pt_isgc_needed=seq_pt_isgc_needed,
	.change_pt_reserve=seq_change_pt_reserve,
	.pt_reserve_to_free=seq_pt_reserve_to_free,

	.li = NULL,
	.private_data = NULL,
	.assigned_page = 0,
};

void seq_mh_swap_hptr(void *a, void *b){
	block_set *aa=(block_set*)a;
	block_set *bb=(block_set*)b;

	void *temp=aa->hptr;
	aa->hptr=bb->hptr;
	bb->hptr=temp;
}

void seq_mh_assign_hptr(void *a, void *hn){
	block_set *aa=(block_set*)a;
	aa->hptr=hn;
}

int seq_get_cnt(void *a){
	block_set *aa=(block_set*)a;
	/*
	if(aa->total_invalid_number==UINT_MAX){
		for(uint32_t i=0; i<BPS; i++){
			res+=aa->blocks[i]->invalid_number;
		}
		aa->total_invalid_number=res;
	}
	else res=aa->total_invalid_number;*/

	return aa->total_invalid_number;
}

float seq_get_cnt_new(void *a, void *b) {
        block_set *aa = (block_set*)a;
        struct blockmanager* bm = (struct blockmanager*) b;
        sbm_pri *p = (sbm_pri*)bm->private_data;

        int idx = aa->blocks[0]->block_num/BPS;
        if (p->timestamp[idx] == 0) {
		printf("timestamp of segment is null\n");
		abort();
	}
	long interval_t = (long)(cur_timestamp - p->timestamp[idx]);
        double int_t = sqrt(interval_t); //age value

        float cand_value = (float)int_t*(double)aa->total_invalid_number/((double)_PPS*L2PGAP-aa->total_invalid_number);
	return cand_value;
}


uint32_t seq_create (struct blockmanager* bm, lower_info *li){
	bm->li=li;
	//bb_checker_start(bm->li);/*check if the block is badblock*/
#ifdef AMF
	printf("NOC :%d _NOS:%ld\n", NOC,_NOS);
#endif

	sbm_pri *p=(sbm_pri*)malloc(sizeof(sbm_pri));
	p->seq_block=(__block*)calloc(sizeof(__block), _NOB);
	p->logical_segment=(block_set*)calloc(sizeof(block_set), _NOS);
	p->assigned_block=p->free_block=0;
	p->seg_populate_bit=(uint8_t*)calloc(_NOS/8+(_NOS%8?1:0), sizeof(uint8_t));
	p->timestamp = (long*)calloc(sizeof(long), _NOS);

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
		p->logical_segment[i].total_valid_number=0;
	}

	mh_init(&p->max_heap, _NOS, seq_mh_swap_hptr, seq_mh_assign_hptr, seq_get_cnt_new, bm);
	q_init(&p->free_logical_segment_q, _NOS);
	q_init(&p->invalid_block_q, _NOS);
	
	for(uint32_t i=0; i<_NOS; i++){
		q_enqueue((void*)&p->logical_segment[i], p->free_logical_segment_q);
		p->free_block++;
	}

	bm->private_data=(void*)p;
	return 1;
}

void* return_heap_queue(struct blockmanager* bm) {
	sbm_pri *p = (sbm_pri*)bm->private_data;
	queue* res = p->max_heap->q;
	return (void*)res;
}

uint32_t seq_destroy (struct blockmanager* bm){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	free(p->seq_block);
	free(p->logical_segment);
	mh_free(p->max_heap);
	free(p->seg_populate_bit);
	q_free(p->free_logical_segment_q);
	free(p);
	return 1;
}

__block* seq_get_block (struct blockmanager* bm, __segment* s){
	if(s->now+1>s->max) abort();
	return s->blocks[s->now++];
}

uint32_t seq_jy_get_block_idx(struct blockmanager* bm, void* b) {
	block_set *bs = (block_set*)b;
	return bs->blocks[0]->block_num/BPS;
}

long jy_get_timestamp(struct blockmanager* bm, uint32_t seg_idx) {
	sbm_pri *p=(sbm_pri*)bm->private_data;
	//int seg_idx = lba/_PPS/L2PGAP;
	long age = cur_timestamp - p->timestamp[seg_idx];
	if (p->timestamp[seg_idx] == 0) abort();
	//printf("%ld - %ld = %ld\n", cur_timestamp, p->timestamp[seg_idx], age);
	if (age < 0) {
		printf("age of the segment is below 0: %ld\n", age);
		abort();
	}
	return age;
}

__segment* seq_get_segment (struct blockmanager* bm, bool isreserve){
	__segment* res=(__segment*)malloc(sizeof(__segment));
	sbm_pri *p=(sbm_pri*)bm->private_data;
	
	block_set *free_block_set=(block_set*)q_dequeue(p->free_logical_segment_q);
	
	if(!free_block_set){
		EPRINT("dev full??", false);
		return NULL;
	}

	if(free_block_set->total_invalid_number || free_block_set->total_valid_number){
		EPRINT("how can it be!\n", true);
	}

	if(!free_block_set){
		printf("new block is null!\n");
		abort();
	}


	if(isreserve){

	}
	else{
		mh_insert_append(p->max_heap, (void*)free_block_set);
	}

	memcpy(res->blocks, free_block_set->blocks, sizeof(__block*)*BPS);

	res->now=0;
	res->max=BPS;
	res->invalid_blocks=0;
	res->used_page_num=0;
	res->seg_idx=res->blocks[0]->block_num/BPS;
	
	p->assigned_block++;
	p->free_block--;


	if(p->assigned_block+p->free_block!=_NOS){
		printf("missing segment error\n");
		abort();
	}
/*
	if(p->seg_populate_bit[res->seg_idx/8] & (1<<(res->seg_idx%8))){
		EPRINT("already populate!\n", true);
	}

	p->seg_populate_bit[res->seg_idx/8] |=(1<<(res->seg_idx%8));*/
	return res;
}

__segment* seq_jy_get_time_segment (struct blockmanager* bm, bool isreserve, double time_stamp){
	__segment* res=(__segment*)malloc(sizeof(__segment));
	sbm_pri *p=(sbm_pri*)bm->private_data;
	
	block_set *free_block_set=(block_set*)q_dequeue(p->free_logical_segment_q);
	
	if(!free_block_set){
		EPRINT("dev full??", false);
		return NULL;
	}

	if(free_block_set->total_invalid_number || free_block_set->total_valid_number){
		EPRINT("how can it be!\n", true);
	}

	if(!free_block_set){
		printf("new block is null!\n");
		abort();
	}


	if(isreserve){

	}
	else{
		mh_insert_append(p->max_heap, (void*)free_block_set);
	}

	memcpy(res->blocks, free_block_set->blocks, sizeof(__block*)*BPS);

	res->now=0;
	res->max=BPS;
	res->invalid_blocks=0;
	res->used_page_num=0;
	res->seg_idx=res->blocks[0]->block_num/BPS;
	
	p->assigned_block++;
	p->free_block--;
	p->timestamp[res->seg_idx] = time_stamp;


	if(p->assigned_block+p->free_block!=_NOS){
		printf("missing segment error\n");
		abort();
	}
/*
	if(p->seg_populate_bit[res->seg_idx/8] & (1<<(res->seg_idx%8))){
		EPRINT("already populate!\n", true);
	}

	p->seg_populate_bit[res->seg_idx/8] |=(1<<(res->seg_idx%8));*/
	return res;
}


__segment* seq_change_reserve(struct blockmanager* bm,__segment *reserve){

	sbm_pri *p=(sbm_pri*)bm->private_data;
	uint32_t segment_start_block_number=reserve->blocks[0]->block_num;
	uint32_t segment_idx=segment_start_block_number/BPS;
	if (reserve->seg_idx != segment_idx) printf("seg idx is different\n");
	block_set *bs=&p->logical_segment[segment_idx];

	mh_insert_append(p->max_heap, (void*)bs);

	return seq_get_segment(bm,true);
}

__segment* seq_jy_change_reserve(struct blockmanager* bm,__segment *reserve, double time_stamp){

	sbm_pri *p=(sbm_pri*)bm->private_data;
	uint32_t segment_start_block_number=reserve->blocks[0]->block_num;
	uint32_t segment_idx=segment_start_block_number/BPS;
	if (reserve->seg_idx != segment_idx) printf("seg idx is different\n");
	block_set *bs=&p->logical_segment[segment_idx];

	mh_insert_append(p->max_heap, (void*)bs);

	return seq_jy_get_time_segment(bm,true, time_stamp);
}


void seq_jy_add_queue(struct blockmanager* bm, queue* group_q, __segment *reserve) {
	if (group_q==NULL) printf("there is no queue: seq_jy_add_queue()\n");
	sbm_pri *p=(sbm_pri*)bm->private_data;
        uint32_t segment_start_block_number=reserve->blocks[0]->block_num;
        uint32_t segment_idx=segment_start_block_number/BPS;
        block_set *bs=&p->logical_segment[segment_idx];

	q_enqueue((void*)bs, group_q);
}

void seq_jy_add_time_queue(struct blockmanager* bm, queue* group_q, __segment *reserve, double timestamp) {
	if (group_q==NULL) printf("there is no queue: seq_jy_add_queue()\n");
	sbm_pri *p=(sbm_pri*)bm->private_data;
        uint32_t segment_start_block_number=reserve->blocks[0]->block_num;
        uint32_t segment_idx=segment_start_block_number/BPS;
        block_set *bs=&p->logical_segment[segment_idx];
	p->timestamp[segment_idx] = timestamp;

	q_enqueue((void*)bs, group_q);
}


int seq_jy_move_q2h(struct blockmanager* bm, queue* q, int size) {
	sbm_pri *p = (sbm_pri*)bm->private_data;
	block_set *bs;
	if (size > q->size) {
		printf("trying to move blocks more than existed\n");
		return 0;
	}
	if (size==0) size = q->size;
	int num=0;
	for (int i=0;i<size;i++) {
		num++;
		bs = (block_set*)q_dequeue(q);
		if (bs == NULL) {
			printf("queue size is different with real size: seq_jy_move_q2h() in block_manager.c\n");
			abort();
		}
		mh_insert_append(p->max_heap, (void*)bs);
	}
	return num;
}

void seq_reinsert_segment(struct blockmanager *bm, uint32_t seg_idx){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	block_set *bs=&p->logical_segment[seg_idx];
	mh_insert_append(p->max_heap, (void*)bs);
}

void seq_time_reinsert_segment(struct blockmanager *bm, uint32_t seg_idx, double timestamp) {
	sbm_pri *p=(sbm_pri*)bm->private_data;
        block_set *bs=&p->logical_segment[seg_idx];
        mh_insert_append(p->max_heap, (void*)bs);
	int idx = bs->blocks[0]->block_num/BPS;
	p->timestamp[idx] = timestamp;
}

bool seq_is_gc_needed (struct blockmanager* bm){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	if(p->free_logical_segment_q->size==0) return true;
	return false;
}

int seq_get_free_segment_number(struct blockmanager *bm) {
	sbm_pri *p=(sbm_pri*)bm->private_data;
	return p->free_logical_segment_q->size;
}

void temp_print(void *a){
	if(a){
		static int cnt=0;
		block_set* target=(block_set*)a;
		printf("[heap_print:%d] block_num:%u invalidate_number:%u\n", cnt++,
				target->blocks[0]->block_num/BPS,
				target->total_invalid_number);
	}
}

__gsegment* seq_get_gc_target (struct blockmanager* bm){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	__gsegment* res=(__gsegment*)malloc(sizeof(__gsegment));
	res->invalidate_number=0;

	block_set *target=NULL;
	if(p->invalid_block_q->size){
		target=(block_set*)q_dequeue(p->invalid_block_q);
	}
	else{
		mh_construct(p->max_heap);
		target=(block_set*)mh_get_max(p->max_heap);
	}

	if(!target) return NULL;

	memcpy(res->blocks, target->blocks, sizeof(__block*)*BPS);
	res->now=res->max=0;
	res->seg_idx=res->blocks[0]->block_num/BPS;

	res->invalidate_number=target->total_invalid_number;
	res->validate_number=target->total_valid_number;

	//p->timestamp[res->seg_idx]=0;

	if(target->total_invalid_number==target->total_valid_number){
		res->all_invalid=true;
	}
	else{
		res->all_invalid=false;
	}

	if(res->invalidate_number==0){
		/*
		printf("_NOS*_PPS*L2PGAP:%lu validate:%lu ivnalidate:%lu\n", 
			(uint64_t)_NOS*_PPS*L2PGAP, total_validate_piece_ppa, total_invalidate_piece_ppa);

		mh_print(p->max_heap, temp_print);
		for(uint32_t i=0; i<_NOS; i++){
			printf("[seg:%u] validate_num:%u invalidate_num:%u\n", i,
					p->logical_segment[i].total_valid_number,
					p->logical_segment[i].total_invalid_number);	
		}
		EPRINT("dev full", false);*/
		mh_construct(p->max_heap);
	}

	return res;
}

__gsegment* seq_jy_get_gc_target (struct blockmanager* bm, queue* group_q){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	__gsegment* res=(__gsegment*)malloc(sizeof(__gsegment));
	res->invalidate_number=0;

	block_set *target=NULL;
	if(p->invalid_block_q->size){
		target=(block_set*)q_dequeue(p->invalid_block_q);
	}
	else{
		target=(block_set*)q_dequeue(group_q);
	}

	if(!target) return NULL;

	memcpy(res->blocks, target->blocks, sizeof(__block*)*BPS);
	res->now=res->max=0;
	res->seg_idx=res->blocks[0]->block_num/BPS;

	res->invalidate_number=target->total_invalid_number;
	res->validate_number=target->total_valid_number;

	//p->timestamp[res->seg_idx]=0;

	if(target->total_invalid_number==target->total_valid_number){
		res->all_invalid=true;
	}
	else{
		res->all_invalid=false;
	}

	return res;
}


uint32_t seq_get_invalidate_blk_number(struct blockmanager *bm){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	mh_construct(p->max_heap);
	uint32_t res=0;
	while(1){
		block_set* target=(block_set*)mh_get_max(p->max_heap);

		if(target->total_invalid_number==_PPS*L2PGAP){
			q_enqueue((void*)target, p->invalid_block_q);
			res++;
		}
		else{
			uint32_t seg_idx=target->blocks[0]->block_num/BPS;
			seq_reinsert_segment(bm, seg_idx);
			break;
		}
	}

	return res;
}

void seq_trim_segment (struct blockmanager* bm, __gsegment* gs, struct lower_info* li){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	uint32_t segment_startblock_number=gs->blocks[0]->block_num;

	for(int i=0; i<BPS; i++){
		__block *b=gs->blocks[i];
		b->invalidate_number=0;
		b->validate_number=0;
		b->now=0;
		memset(b->bitset,0,_PPB*L2PGAP/8);
		memset(b->oob_list,0,sizeof(b->oob_list));
	}

	uint32_t segment_idx=segment_startblock_number/BPS;
	block_set *bs=&p->logical_segment[segment_idx];
	bs->total_invalid_number=0;
	bs->total_valid_number=0;
	/*
	if(bs==&p->logical_segment[1228928/16384]){
		bs->blocks[(1228928%16384)%256]
	}*/
	q_enqueue((void*)bs, p->free_logical_segment_q);
	
	p->assigned_block--;
	p->free_block++;

	if(p->assigned_block+p->free_block!=_NOS){
		printf("missing segment error\n");
		abort();
	}

	li->trim_block(segment_startblock_number*_PPB, ASYNC);
	free(gs);
}

int seq_populate_bit (struct blockmanager* bm, uint32_t ppa){
	int res=1;
	sbm_pri *p=(sbm_pri*)bm->private_data;
	uint32_t bn=ppa/(_PPB * L2PGAP);
	uint32_t pn=ppa%(_PPB * L2PGAP);
	uint32_t bt=pn/8;
	uint32_t of=pn%8;

	__block *b=&p->seq_block[bn];
	b->validate_number++;
	uint32_t segment_idx=b->block_num/BPS;
	block_set *seg=&p->logical_segment[segment_idx];
	seg->total_valid_number++;
	total_validate_piece_ppa++;

	if(p->seq_block[bn].bitset[bt]&(1<<of)){
		res=0;
	//	abort();
	}
	p->seq_block[bn].bitset[bt]|=(1<<of);
	return res;
}

int seq_unpopulate_bit (struct blockmanager* bm, uint32_t ppa){
	int res=1;
	sbm_pri *p=(sbm_pri*)bm->private_data;
	uint32_t bn=ppa/(_PPB * L2PGAP);
	uint32_t pn=ppa%(_PPB * L2PGAP);
	uint32_t bt=pn/8;
	uint32_t of=pn%8;
	__block *b=&p->seq_block[bn];

	if(!(p->seq_block[bn].bitset[bt]&(1<<of))){
		res=0;
		//abort();
	}

	b->bitset[bt]&=~(1<<of);
	b->invalidate_number++;

	uint32_t segment_idx=b->block_num/BPS;
	block_set *seg=&p->logical_segment[segment_idx];
	seg->total_invalid_number++;
	total_invalidate_piece_ppa++;

	if(b->invalidate_number>_PPB * L2PGAP){
	//	printf("????\n");
		//abort();
	}
	return res;
}


bool seq_query_bit(struct blockmanager *bm, uint32_t ppa){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	uint32_t bn=ppa/(_PPB * L2PGAP);
	uint32_t pn=ppa%(_PPB * L2PGAP);
	uint32_t bt=pn/8;
	uint32_t of=pn%8;
	__block *b=&p->seq_block[bn];

	return p->seq_block[bn].bitset[bt]&(1<<of);
}


void seq_invalidate_number_decrease(struct blockmanager *bm, uint32_t ppa){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	uint32_t bn=ppa/(_PPB * L2PGAP);
	//uint32_t pn=ppa%(_PPB * L2PGAP);
	//uint32_t bt=pn/8;
	//uint32_t of=pn%8;
	__block *b=&p->seq_block[bn];
	b->invalidate_number--;


	uint32_t segment_idx=b->block_num/BPS;
	block_set *seg=&p->logical_segment[segment_idx];
	seg->total_invalid_number--;

}

int seq_erase_bit (struct blockmanager* bm, uint32_t ppa){
	int res=1;
	sbm_pri *p=(sbm_pri*)bm->private_data;
	uint32_t bn=ppa/(_PPB * L2PGAP);
	uint32_t pn=ppa%(_PPB * L2PGAP );
	uint32_t bt=pn/8;
	uint32_t of=pn%8;
	__block *b=&p->seq_block[bn];

	if(!(p->seq_block[bn].bitset[bt]&(1<<of))){
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

bool seq_is_valid_page (struct blockmanager* bm, uint32_t ppa){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	uint32_t bn=ppa/(_PPB*L2PGAP);
	uint32_t pn=ppa%(_PPB * L2PGAP);
	uint32_t bt=pn/8;
	uint32_t of=pn%8;

	return p->seq_block[bn].bitset[bt]&(1<<of);
}

bool seq_is_invalid_page (struct blockmanager* bm, uint32_t ppa){
	return !seq_is_valid_page(bm,ppa);
}

void seq_set_oob(struct blockmanager* bm, char *data,int len, uint32_t ppa){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	__block *b=&p->seq_block[ppa/_PPB];
	memcpy(b->oob_list[ppa%_PPB].d,data,len);
}



char *seq_get_oob(struct blockmanager*bm,  uint32_t ppa){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	uint32_t bidx=ppa/_PPB;
	__block *b=&p->seq_block[bidx];
	return b->oob_list[ppa%_PPB].d;
}

void seq_release_segment(struct blockmanager* bm, __segment *s){
	free(s);
}

int seq_get_page_num(struct blockmanager* bm,__segment *s){
	if(s->now==BPS-1){
		if(s->blocks[BPS-1]->now==_PPB) return -1;
	}

	__block *b=s->blocks[s->now];
	if(b->now==_PPB){
		s->now++;
	}
	int blocknumber=s->now;
	b=s->blocks[blocknumber];

	uint32_t page=b->now++;
	int res=b->block_num*_PPB+page;
	
	if(page>_PPB) abort();
	s->used_page_num++;
	bm->assigned_page++;
	return res;
}

int seq_pick_page_num(struct blockmanager* bm,__segment *s){
	if(s->now==0){
		if(s->blocks[BPS-1]->now==_PPB) return -1;
	}

	int blocknumber=s->now;
	if(s->now==BPS) s->now=0;
	__block *b=s->blocks[blocknumber];
	uint32_t page=b->now;
	int res=b->block_num*_PPB+page;

	if(page>_PPB) abort();
	return res;
}

bool seq_check_full(struct blockmanager *bm,__segment *active, uint8_t type){
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

__block *seq_pick_block(struct blockmanager *bm, uint32_t page_num){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	return &p->seq_block[page_num/_PPB];
}

void seq_free_segment(struct blockmanager *, __segment *seg){
	free(seg);
}

uint32_t seq_remain_free_page(struct blockmanager *bm, __segment *active){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	return p->free_block*_PPS+(active?(_PPS-active->used_page_num):0);
}

uint32_t seq_get_invalidate_number(struct blockmanager *bm, uint32_t seg_idx){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	return p->logical_segment[seg_idx].total_invalid_number;
}

