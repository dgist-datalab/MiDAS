#include "gc.h"
#include "map.h"
#include "midas.h"
#include "hot.h"
#include "../../include/data_struct/list.h"
#include <stdlib.h>
#include <stdint.h>

uint32_t gc_norm_count=0;
//extern FILE *gFile;
//extern FILE *wFile;

extern STAT *midas_stat;
extern algorithm page_ftl;
extern long cur_timestamp;
extern HF *hotfilter;

void invalidate_ppa(uint32_t t_ppa){
	/*when the ppa is invalidated this function must be called*/
	page_ftl.bm->unpopulate_bit(page_ftl.bm, t_ppa);
}

void validate_ppa(uint32_t ppa, KEYT *lbas, uint32_t max_idx){
	/*when the ppa is validated this function must be called*/
	for(uint32_t i=0; i<max_idx; i++){
		page_ftl.bm->populate_bit(page_ftl.bm,ppa * L2PGAP+i);
	}

	/*this function is used for write some data to OOB(spare area) for reverse mapping*/
	
	int len=sizeof(KEYT)*L2PGAP+1;
	char *oob_data = (char*)calloc(len, 1);
	memcpy(oob_data, (char*)lbas, sizeof(KEYT)*L2PGAP);
	page_ftl.bm->set_oob(page_ftl.bm, oob_data, len,ppa);
	//set_migration_count(page_ftl.bm, mig_count, sizeof(KEYT)*max_idx, ppa);
	free(oob_data);
}


char *get_lbas(struct blockmanager* bm, char* oob_data, int len) {
	//printf("oob_data: 0x%x\n", oob_data);
	char *res = (char *)malloc(len);
	memcpy(res, oob_data, len);
	return res;
}

gc_value* send_req(uint32_t ppa, uint8_t type, value_set *value){
	algo_req *my_req=(algo_req*)malloc(sizeof(algo_req));
	my_req->parents=NULL;
	my_req->end_req=page_gc_end_req;//call back function for GC
	my_req->type=type;
	
	/*for gc, you should assign free space for reading valid data*/
	gc_value *res=NULL;
	switch(type){
		case GCDR:
			res=(gc_value*)malloc(sizeof(gc_value));
			res->isdone=false;
			res->ppa=ppa;
			my_req->param=(void *)res;
			my_req->type_lower=0;
			/*when read a value, you can assign free value by this function*/
			res->value=inf_get_valueset(NULL,FS_MALLOC_R,PAGESIZE);
			page_ftl.li->read(ppa,PAGESIZE,res->value,ASYNC,my_req);
			break;
		case GCDW:
			res=(gc_value*)malloc(sizeof(gc_value));
			res->value=value;
			my_req->param=(void *)res;
			page_ftl.li->write(ppa,PAGESIZE,res->value,ASYNC,my_req);
			break;
	}
	return res;
}

void new_do_gc(){
	/*this function return a block which have the most number of invalidated page*/
	__gsegment *target=page_ftl.bm->get_gc_target(page_ftl.bm);
	uint32_t page;
	uint32_t bidx, pidx;
	blockmanager *bm=page_ftl.bm;
	pm_body *p=(pm_body*)page_ftl.algo_body;
	//list *temp_list=list_init();
	gc_value **gv_array=(gc_value**)malloc(sizeof(gc_value*)*_PPS);
	align_gc_buffer g_buffer;
	gc_value *gv;
	uint32_t gv_idx=0;

	/*by using this for loop, you can traversal all page in block*/
	for_each_page_in_seg(target,page,bidx,pidx){
		//this function check the page is valid or not
		gv=send_req(page,GCDR,NULL);
		gv_array[gv_idx++]=gv;
	}

	g_buffer.idx=0;
	char *oobs;
	KEYT *lbas;

	gv_idx=0;
	uint32_t done_cnt=0;
	while(done_cnt!=_PPS){
		gv=gv_array[gv_idx];
		if(!gv->isdone){
			goto next;
		}
		oobs=bm->get_oob(bm, gv->ppa);
		lbas=(KEYT*)get_lbas(bm, oobs, sizeof(KEYT)*L2PGAP);
		
		for(uint32_t i=0; i<L2PGAP; i++){
			if(page_map_pick(lbas[i])!=gv->ppa*L2PGAP+i) continue;
			memcpy(&g_buffer.value[g_buffer.idx*LPAGESIZE],&gv->value->value[i*LPAGESIZE],LPAGESIZE);
			g_buffer.key[g_buffer.idx]=lbas[i];

			g_buffer.idx++;

			if(g_buffer.idx==L2PGAP){
				uint32_t res=page_map_gc_update(g_buffer.key, L2PGAP, 0);
				validate_ppa(res, g_buffer.key, g_buffer.idx);
				send_req(res, GCDW, inf_get_valueset(g_buffer.value, FS_MALLOC_W, PAGESIZE));
				g_buffer.idx=0;
			}
		}

		done_cnt++;

		inf_free_valueset(gv->value, FS_MALLOC_R);
		free(gv);
next:
		gv_idx=(gv_idx+1)%_PPS;
	}	

	if(g_buffer.idx!=0){
		uint32_t res=page_map_gc_update(g_buffer.key, g_buffer.idx, 0);
		validate_ppa(res, g_buffer.key, g_buffer.idx);
		send_req(res, GCDW, inf_get_valueset(g_buffer.value, FS_MALLOC_W, PAGESIZE));
		g_buffer.idx=0;	
	}

	bm->trim_segment(bm,target,page_ftl.li); //erase a block

	bm->free_segment(bm, p->active[0]);

	//p->active=p->reserve;//make reserved to active block
	//p->reserve=bm->change_reserve(bm,p->reserve); //get new reserve block from block_manager
}

extern bool naive_status;
int do_gc(){
	/*this function return a block which have the most number of invalidated page*/
	blockmanager *bm=page_ftl.bm;
        pm_body *p=(pm_body*)page_ftl.algo_body;
	//find victim segment
	//1. check MiDAS groups
	int victim_seg = -1;
	if (p->n->naive_start != 1) {
		for (int i=0;i<p->n->naive_start;i++) {
			if (p->m->config[i] < midas_stat->g->gsize[i]) {
				victim_seg = i;
				break;
			}
		}
	}
	if (victim_seg==-1) victim_seg = p->n->naive_start;
	/*
	//2. check if naive_on status
	if (victim_seg == -1) {
		int nstart = p->n->naive_start;
		if (p->n->naive_on) {
			uint32_t size = 0;
			for (int i=0;i<GNUMBER;i++) size += stat->g->gsize[nstart+i];
			if (p->m->config[nstart] < size) victim_seg = nstart;
		}
		if (victim_seg == -1) {
			printf("there is no full group??? in do_gc\n");
			abort();
		}
	}
	*/
	//get target segment
	__gsegment *target;
	if (victim_seg == p->n->naive_start) target=page_ftl.bm->get_gc_target(page_ftl.bm);
	else target = page_ftl.bm->jy_get_gc_target(page_ftl.bm, p->group[victim_seg]);

	uint32_t page;
	uint32_t bidx, pidx;
	list *temp_list=list_init();
	align_gc_buffer g_buffer;
	gc_value *gv;

	li_node *now,*nxt;
	uint32_t tmp_mig_count = seg_get_ginfo(target->seg_idx);
	if ((p->n->naive_on==false) && (tmp_mig_count != victim_seg)) {
		printf("group number of victim segment information is wrong: do_gc()\n");
		abort();
	}
	seg_assign_ginfo(target->seg_idx, UINT_MAX);
	uint32_t mig_count;
	if ((naive_status==false) && (tmp_mig_count > 0)) {
	       printf("naive mida off, but there is unother group\n");
		abort();
	}
	
	if (tmp_mig_count <(p->gnum-1)) mig_count = tmp_mig_count+1;
	else mig_count = tmp_mig_count;

	//static int cnt=0;
	//printf("gc: %d\n", cnt++);
	/*by using this for loop, you can traversal all page in block*/
	for_each_page_in_seg(target,page,bidx,pidx){
		//this function check the page is valid or not
		bool should_read=false;
		for(uint32_t i=0; i<L2PGAP; i++){
			if(bm->is_invalid_page(bm,page*L2PGAP+i)) continue;
			else{
				should_read=true;
				break;
			}
		}
		if(should_read){
			gv=send_req(page,GCDR,NULL);
			list_insert(temp_list,(void*)gv);
		}
	}

	g_buffer.idx=0;
	char* oobs;
	KEYT *lbas;
	const uint32_t tot_page_num=_PPS*L2PGAP;
	uint32_t page_num=0;
	while(temp_list->size){
		for_each_list_node_safe(temp_list,now,nxt){

			gv=(gc_value*)now->data;
			if(!gv->isdone) continue;
			oobs=bm->get_oob(bm, gv->ppa);
			lbas=(KEYT*)get_lbas(bm, oobs, sizeof(KEYT)*L2PGAP);

			for(uint32_t i=0; i<L2PGAP; i++){
				if(bm->is_invalid_page(bm,gv->ppa*L2PGAP+i)) continue;
				++page_num;
				midas_stat->copy++;
				midas_stat->tmp_copy++;
				memcpy(&g_buffer.value[g_buffer.idx*LPAGESIZE],&gv->value->value[i*LPAGESIZE],LPAGESIZE);
				g_buffer.key[g_buffer.idx]=lbas[i];
				g_buffer.idx++;

				if (tmp_mig_count == 0 || tmp_mig_count==1) hf_generate(lbas[i], tmp_mig_count, 0);

				if(g_buffer.idx==L2PGAP){
					//if (g_buffer.mig_count >= GNUMBER) printf("problem 1: %d\n", g_buffer.mig_count);
					uint32_t res=page_map_gc_update(g_buffer.key, L2PGAP, mig_count);
					validate_ppa(res, g_buffer.key, g_buffer.idx);
					send_req(res, GCDW, inf_get_valueset(g_buffer.value, FS_MALLOC_W, PAGESIZE));
					g_buffer.idx=0;
				}
			}

			inf_free_valueset(gv->value, FS_MALLOC_R);
			free(gv);
			free(lbas);
			//you can get lba from OOB(spare area) in physicall page
			list_delete_node(temp_list,now);
		}
	}
	if(g_buffer.idx!=0){
		//if (g_buffer.mig_count >= GNUMBER) printf("problem 2: %d\n", g_buffer.mig_count);
		uint32_t res=page_map_gc_update(g_buffer.key, g_buffer.idx, mig_count);
		validate_ppa(res, g_buffer.key, g_buffer.idx);
		send_req(res, GCDW, inf_get_valueset(g_buffer.value, FS_MALLOC_W, PAGESIZE));
		g_buffer.idx=0;	
	}
	if ((tmp_mig_count == 0) && (hotfilter->make_flag==1)) {
		long seg_age = page_ftl.bm->jy_get_timestamp(page_ftl.bm, target->seg_idx);
		//printf("\nseg age: %ld (%.3f%%)\n", seg_age, (double)seg_age/(double)_PPS/(double)L2PGAP);
		hotfilter->seg_age += seg_age;
		hotfilter->seg_num++;
		hotfilter->G0_vr_sum += (double)page_num/(double)tot_page_num;
		hotfilter->G0_vr_num++;
	}
	/*
	if (tmp_mig_count==1) {
		long seg_age=page_ftl.bm->jy_get_timestamp(page_ftl.bm, target->seg_idx);
		//printf("G1 seg age: %ld (%.3f%%)\n", seg_age, (double)seg_age/(double)(_PPS*L2PGAP));
	}
	*/

	
	bm->trim_segment(bm,target,page_ftl.li); //erase a block
	midas_stat->g->gsize[tmp_mig_count]--;
	midas_stat->erase++;
	midas_stat->g->tmp_vr[tmp_mig_count] += (double)page_num/(double)tot_page_num;
	midas_stat->g->tmp_erase[tmp_mig_count]++;
	
	if (midas_stat->e->collect) {
		midas_stat->e->vr[tmp_mig_count] += (double)page_num/(double)tot_page_num;
		midas_stat->e->erase[tmp_mig_count]++;
	}

	// print valid ratio
	
	//printf("gc occurs, valid page: %f%%\n", (float)page_num/(float)tot_page_num*(float)100);
	
	//print hot ratio
	
	// * print # of current segment number
	/*
	for (int i=0;i<(GNUMBER-1);++i) {
		printf("current %d group segment number: %d\n", i+1, seg_ratio[i]);
	}
	*/
	/*
	if (bm->check_full(bm, p->active, MASTER_PAGE)) {
		bm->free_segment(bm, p->active);

		p->active=bm->get_segment(page_ftl.bm, false);//make reserved to active block
	//p->reserve=bm->change_reserve(bm,p->reserve); //get new reserve block from block_manager
	}
	*/
	list_free(temp_list);
	return victim_seg;
}


ppa_t get_ppa(KEYT *lbas, uint32_t max_idx, int gnum){
	uint32_t res;
	pm_body *p=(pm_body*)page_ftl.algo_body;
	/*you can check if the gc is needed or not, using this condition*/
//	if(page_ftl.bm->check_full(page_ftl.bm, p->active,MASTER_PAGE) && (page_ftl.bm->get_free_segment_number(page_ftl.bm)<=5)){
//		new_do_gc();//call gc
	
	
	int gc_count=0;
	int gc_num = -1;
	int tmp_gnum = -1;
	if ((gnum != 0) && (gnum != 1)) {
		printf("gnum: %d\n", gnum);
		abort();
	}
	while (page_ftl.bm->get_free_segment_number(page_ftl.bm)<=FREENUM) {
		//printf("# of Free Blocks: %d\n",page_ftl.bm->get_free_segment_number(page_ftl.bm)); 
		gc_num = do_gc();//call gc
		if (tmp_gnum != gc_num) {
			gc_count=0;
			tmp_gnum = gc_num;
		}
		gc_count++;
		if (gc_count > 100) {
			printf("!!!!Infinite GC occur!!!!\n");
			if (gc_num == p->n->naive_start) {
				printf("=> infinite GC in last group...\n");
				if (p->n->naive_on == false) naive_mida_on();
				else {
					printf("already naive mida on!!! break\n");
					abort();
				}
			} else {
				if (p->n->naive_on) naive_mida_off();
				printf("=> MERGE GROUP : G%d ~ G%d\n", gc_num, p->gnum-1);
				for (int j=gc_num+1;j<p->gnum;j++) p->m->config[gnum] += p->m->config[j];
				merge_group(gc_num);
				naive_mida_on();
				gc_count=0;
			}
			stat_clear();
			errstat_clear();
		}
	}

	if (p->active[gnum]==NULL) {
                //initialize migration group
                //p->active[mig_count] = page_ftl.bm->get_segment(page_ftl.bm, true);
                p->active[gnum] = page_ftl.bm->jy_get_time_segment(page_ftl.bm,true, cur_timestamp);
                seg_assign_ginfo(p->active[gnum]->seg_idx, gnum);
                midas_stat->g->gsize[gnum]++;
                ++p->gcur;
        }

retry:
	/*get a page by bm->get_page_num, when the active block doesn't have block, return UINT_MAX*/
	res=page_ftl.bm->get_page_num(page_ftl.bm,p->active[gnum]);

	if(res==UINT32_MAX){
		/*
		 * print normal gc
		if (page_ftl.bm->get_free_segment_number(page_ftl.bm)<=2) {
			++gc_norm_count;
			printf("num of gc: %d\n", gc_norm_count);
		}
		*/
		//printf("free segment: %u\n", page_ftl.bm->get_free_segment_number(page_ftl.bm));
		++midas_stat->g->gsize[gnum];
		__segment *tmp = p->active[gnum];
		//set timestamp for new segment
		if (p->active_q->size) {
			p->active[gnum] = (__segment*)q_dequeue(p->active_q);
			if (p->n->naive_start==1) {
                                page_ftl.bm->jy_time_reinsert_segment(page_ftl.bm, tmp->seg_idx, cur_timestamp);
                        } else {
				page_ftl.bm->jy_add_time_queue(page_ftl.bm, p->group[gnum], tmp, cur_timestamp);
                        }
		} else {
			if (p->n->naive_start==1) {
                                p->active[gnum] = page_ftl.bm->jy_change_reserve(page_ftl.bm, p->active[gnum], cur_timestamp);
                        } else {
                                p->active[gnum] = page_ftl.bm->jy_get_time_segment(page_ftl.bm,true, cur_timestamp);
                                page_ftl.bm->jy_add_queue(page_ftl.bm, p->group[gnum], tmp);
                        }
		}
		page_ftl.bm->free_segment(page_ftl.bm,tmp);
		seg_assign_ginfo(p->active[gnum]->seg_idx, gnum);
		goto retry;
	}

	/*validate a page*/
	validate_ppa(res, lbas, max_idx);
	//printf("assigned %u\n",res);
	return res;
}

void *page_gc_end_req(algo_req *input){
	gc_value *gv=(gc_value*)input->param;
	switch(input->type){
		case GCDR:
			gv->isdone=true;
			break;
		case GCDW:
			/*free value which is assigned by inf_get_valueset*/
			inf_free_valueset(gv->value,FS_MALLOC_R);
			free(gv);
			break;
	}
	free(input);
	return NULL;
}
