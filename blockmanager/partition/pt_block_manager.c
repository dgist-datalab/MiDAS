#include "pt_block_manager.h"
#include "../../include/container.h"
#include "../bb_checker.h"
#include <stdlib.h>
#include <stdio.h>
extern bb_checker checker;

struct blockmanager pt_bm={
	.create=NULL,
	.q_return=NULL,
	.destroy=NULL,
	.get_block=base_get_block,
	.pick_block=base_pick_block,
	.get_segment=NULL,
	.jy_get_time_segment=NULL,
	.jy_get_block_idx=NULL,
	.jy_get_timestamp=NULL,
	.get_page_num=base_get_page_num,
	.pick_page_num=base_pick_page_num,
	.check_full=base_check_full,
	.is_gc_needed=base_is_gc_needed, 

	.get_free_segment_number=NULL,

	.get_gc_target=NULL,
	.trim_segment=NULL,
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

	.pt_create=pbm_create,
	.pt_destroy=pbm_destroy,
	.pt_get_segment=pbm_pt_get_segment,
	.pt_get_gc_target=pbm_pt_get_gc_target,
	.pt_trim_segment=pbm_pt_trim_segment,
	.pt_remain_page=pbm_pt_remain_page,
	.pt_isgc_needed=pbm_pt_isgc_needed,
	.change_pt_reserve=pbm_change_pt_reserve,
	.pt_reserve_to_free=pbm_reserve_to_free,
};

void pt_mh_swap_hptr(void *a, void *b){
	__block *aa=(__block*)a;
	__block *bb=(__block*)b;

	void *temp=aa->hptr;
	aa->hptr=bb->hptr;
	bb->hptr=temp;
}

void pt_mh_assign_hptr(void *a, void *hn){
	__block *aa=(__block*)a;
	aa->hptr=hn;
}

int pt_get_cnt(void *a){
	__block *aa=(__block*)a;
	return aa->age;
}
void pbm_create_print(blockmanager *bm, int pnum){
/*
	bbm_pri *p=(bbm_pri*)bm->private_data;
	p_info *pinfo=(p_info*)p->private_data;
	for(int i=0; i<pnum; i++){
		if(i==DATA_S){
			printf("[DATA SEG INFO]\n");
		}
		else{
			printf("[MAP SEG INFO]\n");	
		}

		channel *c=&pinfo->p_channel[i][0];
		queue *t_q=c->free_block;
		__block *b;
		for_each_rqueue_type(t_q,b,__block*){
			printf("%d ",b->block_num);	
		}
		printf("\n");
	}
*/	
	__segment *d,*m;
	d=pbm_pt_get_segment(bm,DATA_S,false);
	m=pbm_pt_get_segment(bm,MAP_S,false);
	

	int page;	
	int idx=0;
	printf("MAP SEG blocks\n");
	while((page=base_get_page_num(bm,m))!=-1){
		printf("[%d]:%d\n",idx++,page);
	}

	printf("DATA SEG blocks\n");
	idx=0;
	while((page=base_get_page_num(bm,d))!=-1){
		printf("[%d]:%d\n",idx++,page);
	}

	exit(1);
}

uint32_t pbm_create(blockmanager *bm, int pnum, int *epn, lower_info *li){
	bm->li=li;
	bb_checker_start(bm->li);

	bbm_pri *p=(bbm_pri*)malloc(sizeof(bbm_pri));
	bm->private_data=(void*)p;
	p->base_block=(__block*)calloc(sizeof(__block),_NOS*PUNIT);

	int block_idx=0;
	for(int i=0; i<_NOS; i++){
		int seg_idx=bb_checker_get_segid();
		for(int j=0; j<PUNIT; j++){
			__block *b=&p->base_block[block_idx];
			b->block_num=seg_idx;
			b->punit_num=j;
			b->bitset=(uint8_t*)calloc(_PPB/8,1);
			block_idx++;
		}
	}

	p_info* pinfo=(p_info*)malloc(sizeof(p_info));
	p->private_data=(void*)pinfo;
	pinfo->pnum=pnum;
	pinfo->now_assign=(int*)malloc(sizeof(int)*pnum);
	pinfo->max_assign=(int*)malloc(sizeof(int)*pnum);
	pinfo->p_channel=(channel**)malloc(sizeof(channel) *pnum);
	pinfo->from=(int*)malloc(sizeof(int)*pnum);
	pinfo->to=(int*)malloc(sizeof(int)*pnum);
	int start=0;
	int end=0;
	checker.map_first=false;
	for(int i=0 ; i<pnum; i++){
		pinfo->now_assign[i]=0;
		pinfo->max_assign[i]=epn[i];
		pinfo->p_channel[i]=(channel*)malloc(sizeof(channel)*BPS);
		end+=epn[i];
		pinfo->from[i]=start;
		pinfo->to[i]=end-1;
		printf("%s assign block %d ~ %d( %d ~ %d )\n",i==0?"MAP":"DATA",start,end-1,p->base_block[pinfo->from[i]*64].block_num,p->base_block[(pinfo->to[i])*64].block_num+_PPS-1);
		for(int j=0; j<PUNIT; j++){
			channel *c=&pinfo->p_channel[i][j];
			q_init(&c->free_block,end-start);
			//mh_init(&c->max_heap,end-start,pt_mh_swap_hptr,pt_mh_assign_hptr,pt_get_cnt);
			mh_init(&c->max_heap, end-start, pt_mh_swap_hptr, pt_mh_assign_hptr, NULL, NULL);
			for(int k=start; k<end;k++){
				__block *n=&p->base_block[k*BPS+j%BPS];
				q_enqueue((void*)n,c->free_block);
			}
		}
		start=end;
	}

	p->seg_map=rb_create();
	p->seg_map_idx=0;
//	pbm_create_print(bm,pnum);
	return 1;
}

uint32_t pbm_destroy(blockmanager *bm){
	bbm_pri *p=(bbm_pri*)bm->private_data;
	p_info *pinfo=(p_info*)p->private_data;

	free(p->base_block);

	for(int i=0; i<pinfo->pnum; i++){
		for(int j=0; j<BPS; j++){
			channel *c=&pinfo->p_channel[i][j];
			q_free(c->free_block);
			mh_free(c->max_heap);
		}
		free(pinfo->p_channel[i]);
	}
	
	free(pinfo->from);
	free(pinfo->to);
	free(pinfo->now_assign);
	free(pinfo->max_assign);
	free(pinfo->p_channel);
	free(pinfo);
	free(p);
	return 1;
}

__segment* pbm_pt_get_segment(blockmanager *bm, int pnum, bool isreserve){
	__segment *res=(__segment*)malloc(sizeof(__segment));
	bbm_pri *p=(bbm_pri*)bm->private_data;
	p_info *pinfo=(p_info*) p->private_data;

	for(int i=0; i<BPS; i++){
		__block *b=(__block*)q_dequeue(pinfo->p_channel[pnum][i].free_block);
		if(!b) abort();

		if(!isreserve && pnum==DATA_S){
			mh_insert_append(pinfo->p_channel[pnum][i].max_heap,(void*)b);
		}
		res->blocks[i]=b;
		if(pnum==DATA_S){
			b->seg_idx=p->seg_map_idx;
		}
	}
	res->now=0;
	res->max=BPS;
	res->used_page_num=0;
	if(pnum==DATA_S){
		res->invalid_blocks=0;
		res->seg_idx=p->seg_map_idx++;
		rb_insert_int(p->seg_map,res->seg_idx,(void *)res);
	}

	if(pinfo->now_assign[pnum]++>pinfo->max_assign[pnum]){
		printf("over assgin\n");
		abort();
	}
	return res;
}

__segment* pbm_change_pt_reserve(blockmanager *bm, int pt_num, __segment* reserve){
	__segment *res=pbm_pt_get_segment(bm,pt_num,true);
	bbm_pri *p=(bbm_pri*)bm->private_data;
	p_info *pinfo=(p_info*) p->private_data;
	__block *tblock;
	int bidx;
	if(pt_num==DATA_S){
		for_each_block(reserve,tblock,bidx){
			mh_insert_append(pinfo->p_channel[pt_num][bidx].max_heap,(void*)tblock);
		}
	}
	return res;
}

__gsegment* pbm_pt_get_gc_target(blockmanager* bm, int pnum){
	__gsegment *res=(__gsegment*)calloc(sizeof(__gsegment),1);
	bbm_pri *p=(bbm_pri*)bm->private_data;
	p_info *pinfo=(p_info*) p->private_data;
	res->now=0;
	res->max=BPS;
	int invalidate_number=0;
	if(pnum==DATA_S){
		for(int i=0; i<BPS; i++){
			mh_construct(pinfo->p_channel[pnum][i].max_heap);
			__block *b=(__block*)mh_get_max(pinfo->p_channel[pnum][i].max_heap);
			if(!b) abort();
			res->blocks[i]=b;
			invalidate_number+=b->invalid_number;
		}
	}else{
		int max_invalid=0,now_invalid=0;
		int target_seg=0;
		for(int i=pinfo->from[pnum]; i<=pinfo->to[pnum]; i++){
			for(int j=0;j<BPS; j++){
				now_invalid+=p->base_block[i*BPS+j].invalid_number;
			}
			if(now_invalid>_PPS){
				abort();
			}
			if(now_invalid>max_invalid){
				target_seg=i;
				max_invalid=now_invalid;
			}
			now_invalid=0;
		}

		for(int j=0; j<BPS; j++){
			res->blocks[j]=&p->base_block[target_seg*BPS+j];
		}
		invalidate_number=max_invalid;
	}
	if(pnum==MAP_S && invalidate_number==0){
		printf("invalidate number 0 at %s\n",pnum==DATA_S?"DATA":"MAP");
		abort();
	}
	res->invalidate_number=invalidate_number;
	if(pnum==DATA_S){
	//	printf("invalidate_number:%d\n",res->invalidate_number);
	}
	return res;
}

void pbm_pt_trim_segment(blockmanager* bm, int pnum, __gsegment *target, lower_info *li){
	bbm_pri *p=(bbm_pri*)bm->private_data;
	p_info *pinfo=(p_info*) p->private_data;
	Redblack target_node;
	__segment *target_seg;
	
	for(int i=0; i<BPS; i++){
		__block *b=target->blocks[i];
	
		li->trim_a_block(GETBLOCKPPA(b),ASYNC);
		b->invalid_number=0;
		b->now=0;
		memset(b->bitset,0,_PPB/8);

		memset(b->oob_list,0,sizeof(b->oob_list));
		channel *c=&pinfo->p_channel[pnum][i];
	//	mh_insert_append(c->max_heap,(void*)b);
		q_enqueue((void*)b,c->free_block);
		if(pnum==MAP_S){
			//printf("free block :%d\n",c->free_block->size);
		}
		if(pnum==DATA_S){
			rb_find_int(p->seg_map,b->seg_idx,&target_node);
			target_seg=(__segment*)target_node->item;
			target_seg->invalid_blocks++;
			if(target_seg->invalid_blocks==BPS){
				//printf("delete segment!\n");
				free(target_seg);
				rb_delete(target_node,true);
			}
		}
	}

	pinfo->now_assign[pnum]--;
	if(pinfo->now_assign[pnum]<0){
		printf("under assign!\n");
		abort();
	}
}

int pbm_pt_remain_page(blockmanager* bm, __segment *active, int pt_num){
	int res=0;
	bbm_pri *p=(bbm_pri*)bm->private_data;
	p_info *pinfo=(p_info*) p->private_data;

	channel *c=&pinfo->p_channel[pt_num][0];	
	res+=c->free_block->size * _PPS;

	/*
	if(active->now <active->max){
		__block *t=active->blocks[active->now];
		res+=(active->max-active->now) * _PPB;
		res+=t->max-t->now;
	}*/

	res+=_PPS-active->used_page_num;
	return res;
}

bool pbm_pt_isgc_needed(struct blockmanager* bm, int pt_num){
	bbm_pri *p=(bbm_pri*)bm->private_data;
	p_info *pinfo=(p_info*) p->private_data;

	return pinfo->p_channel[pt_num][0].free_block->size==0;
}

uint32_t pbm_reserve_to_free(struct blockmanager *bm, int pnum,__segment *reserve){
	bbm_pri *p=(bbm_pri*)bm->private_data;
	p_info *pinfo=(p_info*) p->private_data;
	Redblack target_node;
	__segment *target_seg;

	for(int i=0; i<BPS;i++){
		__block* b=reserve->blocks[i];
		if(b->invalid_number){
			printf("it can't have invalid_number\n");
			abort();
		}
		b->invalid_number=0;
		b->now=0;
		channel *c=&pinfo->p_channel[pnum][i];
	//	mh_insert_append(c->max_heap,(void*)b);
		q_enqueue((void*)b,c->free_block);
		if(pnum==MAP_S){
	//		printf("free block :%d\n",c->free_block->size);
		}
		if(pnum==DATA_S){
			rb_find_int(p->seg_map,b->seg_idx,&target_node);
			target_seg=(__segment*)target_node->item;
			target_seg->invalid_blocks++;
			if(target_seg->invalid_blocks==BPS){
				//printf("delete segment!\n");
				free(target_seg);
				rb_delete(target_node,true);
			}
		}
	}
	free(reserve);
	pinfo->now_assign[pnum]--;
	return 1;
}
