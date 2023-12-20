#include "gc.h"
#include "../../include/data_struct/list.h"
#include "../../include/container.h"
#include "demand_mapping.h"
#include <stdlib.h>
#include <stdint.h>
#include <queue>

extern algorithm demand_ftl;
extern demand_map_manager dmm;
extern uint32_t test_key;
uint32_t debug_lba=UINT32_MAX;
uint32_t test_ppa=UINT32_MAX;
uint32_t test_ppa2=UINT32_MAX;
pm_body *pm_body_create(blockmanager *bm){
	pm_body *res=(pm_body*)malloc(sizeof(pm_body));

	res->active=bm->get_segment(bm, false);
	res->reserve=bm->get_segment(bm, true);
	res->seg_type_checker[res->active->seg_idx]=DATASEG;
	res->seg_type_checker[res->reserve->seg_idx]=DATASEG;

	res->map_active=bm->get_segment(bm, false);
	res->map_reserve=bm->get_segment(bm, true);
	res->seg_type_checker[res->map_active->seg_idx]=MAPSEG;
	res->seg_type_checker[res->map_reserve->seg_idx]=MAPSEG;
	return res;
}

void pm_body_destroy(pm_body *pm){
	free(pm->map_active);
	free(pm->active);
	free(pm->map_reserve);
	free(pm->reserve);
}

void invalidate_ppa(uint32_t t_ppa){
	/*when the ppa is invalidated this function must be called*/
	if(t_ppa==test_ppa){
		printf("%u unpopulated!\n",t_ppa);
	}
	if(t_ppa==test_ppa2){
		printf("%u unpopulated!\n",t_ppa);
	}

	if(!demand_ftl.bm->unpopulate_bit(demand_ftl.bm, t_ppa)){
		printf("target:%u ",t_ppa);
		EPRINT("double invalidation!", true);
	}
}

void validate_ppa(uint32_t ppa, KEYT *lbas, uint32_t max_idx){
	/*when the ppa is validated this function must be called*/
	for(uint32_t i=0; i<max_idx; i++){
		if(ppa*L2PGAP+i==test_ppa){
			printf("%u populated, it is ppa for %u!\n", test_ppa,lbas[i]);
		}
		if(ppa*L2PGAP+i==test_ppa2){
			printf("%u populated, it is ppa for %u!\n", test_ppa2,lbas[i]);
		}
		if(!demand_ftl.bm->populate_bit(demand_ftl.bm,ppa * L2PGAP+i)){
			printf("target:%u ", ppa*L2PGAP+i);
			EPRINT("double validation!", true);
		}
	}

	/*this function is used for write some data to OOB(spare area) for reverse mapping*/
	demand_ftl.bm->set_oob(demand_ftl.bm,(char*)lbas,sizeof(KEYT)*max_idx,ppa);
}

gc_value* send_req(uint32_t ppa, uint8_t type, value_set *value, gc_value *gv){
	algo_req *my_req=(algo_req*)malloc(sizeof(algo_req));
	my_req->parents=NULL;
	my_req->end_req=page_gc_end_req;//call back function for GC
	my_req->type=type;
	
	/*for gc, you should assign free space for reading valid data*/
	gc_value *res=NULL;
	switch(type){
		case GCMR:
		case GCDR:
			res=(gc_value*)malloc(sizeof(gc_value));
			res->isdone=false;
			res->ppa=ppa;
			my_req->param=(void *)res;
			my_req->type_lower=0;
			/*when read a value, you can assign free value by this function*/
			res->value=inf_get_valueset(NULL,FS_MALLOC_R,PAGESIZE);
			demand_ftl.li->read(ppa,PAGESIZE,res->value,ASYNC,my_req);
			break;
		case GCMW:
			my_req->param=(void*)gv;
			demand_ftl.li->write(ppa,PAGESIZE,gv->value,ASYNC,my_req );
			break;
		case GCDW:
			res=(gc_value*)malloc(sizeof(gc_value));
			res->value=value;
			my_req->param=(void *)res;
			demand_ftl.li->write(ppa,PAGESIZE,res->value,ASYNC,my_req);
			break;
	}
	return res;
}

uint32_t debug_gc_lba;

void segment_print(bool is_data_gc){
	if(is_data_gc){
		printf("is data gc!\n");
	}
	else{
		printf("is map gc!\n");
	}

	pm_body *p=(pm_body*)demand_ftl.algo_body;
	blockmanager *bm=demand_ftl.bm;
	for(uint32_t i=0; i<_NOS; i++){
		uint32_t invalidate_number=bm->get_invalidate_number(bm, i);
		printf("%u %s inv:%u - %s\n", i,
				p->seg_type_checker[i]==DATASEG?"DATASEG":"MAPSEG",
				invalidate_number,
				invalidate_number==_PPS*L2PGAP?"all":"no");
	}

	printf("\n");
}

void do_gc(){
	/*this function return a block which have the most number of invalidated page*/
	pm_body *p=(pm_body*)demand_ftl.algo_body;
	__gsegment *target=NULL;
	std::queue<uint32_t> temp_queue;
	blockmanager *bm=demand_ftl.bm;
	//static int cnt=0;
	//printf("gc:%u\n", ++cnt);
	while(!target || 
			p->seg_type_checker[target->seg_idx]!=DATASEG){
		if(target){
			if(p->seg_type_checker[target->seg_idx]==MAPSEG && target->invalidate_number==_PPS *L2PGAP){
				break;	
			}
			temp_queue.push(target->seg_idx);
			free(target);
		}
		target=bm->get_gc_target(bm);
	} 

/*
	segment_print(true);
	printf("target seg_idx:%u\n", target->seg_idx);
*/
	uint32_t seg_idx;
	while(temp_queue.size()){
		seg_idx=temp_queue.front();
		bm->reinsert_segment(bm, seg_idx);
		temp_queue.pop();
	}
//	printf("gc range : %u~%u\n", target->blocks[0]->block_num * _PPB, (target->blocks[63]->block_num+1) * _PPB-1);

	uint32_t page;
	uint32_t bidx, pidx;
	uint32_t update_target_idx=0;
	uint32_t debuging_cnt=0;
	list *temp_list=NULL;
	mapping_entry *update_target=NULL;
	if((p->seg_type_checker[target->seg_idx]==DATASEG && target->invalidate_number==_PPS*L2PGAP) ||
			(p->seg_type_checker[target->seg_idx]==MAPSEG && target->invalidate_number==_PPS * L2PGAP)){
		goto finish;
	}
	temp_list=list_init();
	align_gc_buffer g_buffer;
	gc_value *gv;

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
			gv=send_req(page,GCDR,NULL,NULL);
			list_insert(temp_list,(void*)gv);
			update_target_idx+=L2PGAP;
		}
	}
	update_target=(mapping_entry*)malloc(sizeof(mapping_entry)*update_target_idx);
	update_target_idx=0;

	uint32_t cached_ppa;
	li_node *now,*nxt;
	g_buffer.idx=0;
	KEYT *lbas;
	while(temp_list->size){
		for_each_list_node_safe(temp_list,now,nxt){
			gv=(gc_value*)now->data;
			if(!gv->isdone) continue;
			lbas=(KEYT*)bm->get_oob(bm, gv->ppa);
			for(uint32_t i=0; i<L2PGAP; i++){
				if(bm->is_invalid_page(bm,gv->ppa*L2PGAP+i)) continue;
				//check cache 
				if(dmm.cache->exist(dmm.cache, lbas[i])){
					cached_ppa=dmm.cache->get_mapping(dmm.cache, lbas[i]);
					if(cached_ppa!=gv->ppa*L2PGAP+i){
						printf("it can't be!! L:%u, P:%u, oldP:%u\n",lbas[i], cached_ppa, gv->ppa*L2PGAP+i);
						abort();
						//should not move data but mapping update
			//			update_target[update_target_idx].lba=lbas[i];
			//			update_target[update_target_idx].ppa=UINT32_MAX;
			//			update_target_idx++;
						continue;
					}
				}

				invalidate_ppa(gv->ppa*L2PGAP+i);
				memcpy(&g_buffer.value[g_buffer.idx*4096],&gv->value->value[i*4096],4096);
				g_buffer.key[g_buffer.idx]=lbas[i];


				if(test_key==lbas[i]){
					printf("gc org:%u -> %u data:%u\n", lbas[i], gv->ppa*L2PGAP+i, *(uint32_t*)&g_buffer.value[g_buffer.idx*LPAGESIZE]);
				}
				
				g_buffer.idx++;

				if(g_buffer.idx==L2PGAP){
					uint32_t res=get_rppa(g_buffer.key, L2PGAP, update_target, &update_target_idx);

		//			gc_data_check(g_buffer);

					send_req(res, GCDW, inf_get_valueset(g_buffer.value, FS_MALLOC_W, PAGESIZE), NULL);
					g_buffer.idx=0;
				}
			}

			inf_free_valueset(gv->value, FS_MALLOC_R);
			free(gv);
			//you can get lba from OOB(spare area) in physicall page
			list_delete_node(temp_list,now);
		}
	}

	if(g_buffer.idx!=0){
		uint32_t res=get_rppa(g_buffer.key, g_buffer.idx, update_target, &update_target_idx);
//		gc_data_check(g_buffer);
		send_req(res, GCDW, inf_get_valueset(g_buffer.value, FS_MALLOC_W, PAGESIZE), NULL);
		g_buffer.idx=0;	
	}

	demand_map_some_update(update_target, update_target_idx);
	free(update_target);

finish:
	bm->trim_segment(bm,target,demand_ftl.li); //erase a block
	free(p->active);
	p->active=p->reserve;//make reserved to active block
	p->reserve=bm->change_reserve(bm, p->reserve); //get new reserve block from block_manager
	p->seg_type_checker[p->reserve->seg_idx]=DATASEG;
	if(temp_list){
		list_free(temp_list);
	}
}


ppa_t get_ppa(KEYT *lbas, uint32_t max_idx){
	uint32_t res;
	pm_body *p=(pm_body*)demand_ftl.algo_body;
	blockmanager *bm=demand_ftl.bm;
	/*you can check if the gc is needed or not, using this condition*/
	if(demand_ftl.bm->check_full(demand_ftl.bm, p->active,MASTER_PAGE) && demand_ftl.bm->is_gc_needed(demand_ftl.bm)){
		do_gc();//call gc
	}

retry:
	/*get a page by bm->get_page_num, when the active block doesn't have block, return UINT_MAX*/
	res=demand_ftl.bm->get_page_num(demand_ftl.bm,p->active);

	if(res==UINT32_MAX){
		demand_ftl.bm->free_segment(demand_ftl.bm, p->active);
		p->active=demand_ftl.bm->get_segment(demand_ftl.bm, false); //get a new block
		if(!p->active){
			for(uint32_t i=0; i<_NOS;i++){
				printf("[%u] type:%s inv-num:%u\n",
						i,
						p->seg_type_checker[i]==DATASEG?"data":"map",
						bm->get_invalidate_number(bm, i));
			}
		}
		p->seg_type_checker[p->active->seg_idx]=DATASEG;
		goto retry;
	}

	/*validate a page*/
	validate_ppa(res,lbas, max_idx);
	//printf("assigned %u\n",res);
	return res;
}

ppa_t get_rppa(KEYT *lbas, uint8_t idx, mapping_entry *target, uint32_t *_target_idx){
	uint32_t res=0;
	pm_body *p=(pm_body*)demand_ftl.algo_body;

	/*when the gc phase, It should get a page from the reserved block*/
	res=demand_ftl.bm->get_page_num(demand_ftl.bm,p->reserve);
	uint32_t target_idx=*_target_idx;
	for(uint32_t i=0; i<idx; i++){
		KEYT t_lba=lbas[i];
		target[target_idx].lba=t_lba;
		target[target_idx].ppa=res*L2PGAP+i;
		target_idx++;
		demand_ftl.bm->populate_bit(demand_ftl.bm, res*L2PGAP+i);
	}
	(*_target_idx)=target_idx;

	demand_ftl.bm->set_oob(demand_ftl.bm,(char*)lbas,sizeof(KEYT)*L2PGAP, res);
	return res;
}

void *page_gc_end_req(algo_req *input){
	gc_value *gv=(gc_value*)input->param;
	switch(input->type){
		case GCMR:
		case GCDR:
			gv->isdone=true;
			break;
		case GCMW:	
			inf_free_valueset(gv->value,FS_MALLOC_R);
			free(gv);
			break;
		case GCDW:
			/*free value which is assigned by inf_get_valueset*/
			inf_free_valueset(gv->value,FS_MALLOC_W);
			free(gv);
			break;
	}
	free(input);
	return NULL;
}

#if 0
void new_do_gc(){
	/*this function return a block which have the most number of invalidated page*/
	__gsegment *target=demand_ftl.bm->pt_get_gc_target(demand_ftl.bm, DATA_S);
	uint32_t page;
	uint32_t bidx, pidx;
	blockmanager *bm=demand_ftl.bm;
	pm_body *p=(pm_body*)demand_ftl.algo_body;
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
	KEYT *lbas;
	gv_idx=0;
	uint32_t done_cnt=0;
	while(done_cnt!=_PPS){
		gv=gv_array[gv_idx];
		if(!gv->isdone){
			goto next;
		}
		lbas=(KEYT*)bm->get_oob(bm, gv->ppa);
		for(uint32_t i=0; i<L2PGAP; i++){
			if(page_map_pick(lbas[i])!=gv->ppa*L2PGAP+i) continue;
			memcpy(&g_buffer.value[g_buffer.idx*4096],&gv->value->value[i*4096],4096);
			g_buffer.key[g_buffer.idx]=lbas[i];

			g_buffer.idx++;

			if(g_buffer.idx==L2PGAP){
				uint32_t res=page_map_gc_update(g_buffer.key, L2PGAP);
				validate_ppa(res, g_buffer.key);
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
		uint32_t res=page_map_gc_update(g_buffer.key, g_buffer.idx);
		validate_ppa(res, g_buffer.key);
		send_req(res, GCDW, inf_get_valueset(g_buffer.value, FS_MALLOC_W, PAGESIZE));
		g_buffer.idx=0;	
	}

	bm->pt_trim_segment(bm,DATA_S, target,demand_ftl.li); //erase a block

	bm->free_segment(bm, p->active);

	p->active=p->reserve;//make reserved to active block
	p->reserve=bm->change_pt_reserve(bm, DATA_S, p->reserve); //get new reserve block from block_manager
}
#endif
