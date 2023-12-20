#include "level.h"
#include "run.h"
#include "io.h"
#include "../../include/settings.h"

level *level_init(uint32_t max_sst_num, uint32_t max_run_num, uint32_t level_type, uint32_t idx, 
		uint32_t max_contents_num, bool check_full_by_size){
	level *res=(level*)calloc(1,sizeof(level));
	res->idx=idx;
	res->array=(run*)calloc(max_run_num, sizeof(run));

	for(uint32_t i=0; i<max_run_num; i++){
		run_space_init(&res->array[i], max_sst_num/max_run_num, -1, 0);
	}
	res->level_type=level_type;
	if(res->level_type!=TIERING && res->level_type!=TIERING_WISCKEY){
		res->run_num=1;
	}

	res->now_sst_num=0;
	res->max_sst_num=max_sst_num;
	res->max_run_num=max_run_num;
	res->max_contents_num=max_contents_num;
	res->now_contents_num=0;
	res->check_full_by_size=check_full_by_size;
	return res;
}

uint32_t level_append_sstfile(level *lev, sst_file *sptr, bool move_originality){
	if(lev->level_type==TIERING || lev->level_type==TIERING_WISCKEY){
		EPRINT("it can't be", true);
		/*
		if(sptr->type!=BLOCK_FILE){
			EPRINT("tier must have block file", true);
		}
		if(lev->run_num >= lev->max_run_num){
			EPRINT("over run in level", true);
		}
		run_append_sstfile(&lev->array[lev->run_num], sptr);
		lev->run_num++;
		 */
	}
	else{
		if(move_originality){
			run_append_sstfile_move_originality(LAST_RUN_PTR(lev), sptr);
		}
		else{
			run_append_sstfile(LAST_RUN_PTR(lev), sptr);	
		}
		lev->now_sst_num++;
		if(lev->now_sst_num >lev->max_sst_num){
			EPRINT("over sst file level", true);
			return 1;
		}
	}
	return 0;
}

level *level_convert_normal_run_to_LW(run *r, page_manager *pm, 
		uint32_t closed_from, uint32_t closed_to){
	sst_file *sptr, *new_sptr;
	map_range *map_ptr;
	uint32_t sidx=closed_from, midx;
	uint32_t target_sst_file_num=0;
	for_each_sst_at(r, sptr, sidx){
		target_sst_file_num+=sptr->map_num;
	}
	sidx=closed_from;
	level *res=level_init(target_sst_file_num, 1, false, UINT32_MAX, r->now_contents_num, false);
	for_each_sst_at(r, sptr, sidx){
		for_each_map_range(sptr, map_ptr, midx){
			new_sptr=sst_MR_to_sst_file(map_ptr);
			level_append_sstfile(res, new_sptr, true);
			sst_free(new_sptr, pm);
		}
		if(sidx==closed_to) break;
	}
	return res;
}

level *level_split_lw_run_to_lev(run *rptr, 
		uint32_t closed_from, uint32_t closed_to){
	sst_file *sptr;
	uint32_t sidx=closed_from;
	uint32_t contents_num=0;
	for_each_sst_at(rptr, sptr, sidx){
	//	level_append_sstfile(res, sptr, false);
		contents_num+=rptr->now_contents_num;
		if(sidx==closed_to) break;
	}

	level *res=level_init(closed_to-closed_from+1, 1, LEVELING_WISCKEY, UINT32_MAX, contents_num, false);

	sidx=closed_from;
	for_each_sst_at(rptr, sptr, sidx){
		level_append_sstfile(res, sptr, false);
		if(sidx==closed_to) break;
	}
	return res;
}

uint32_t level_deep_append_sstfile(level *lev, sst_file *sptr){
	if(lev->level_type==TIERING || lev->level_type==TIERING_WISCKEY){
		EPRINT("it can't be", true);
		/*
		if(sptr->type!=BLOCK_FILE){
			EPRINT("tier must have block file", true);
		}
		if(lev->run_num >= lev->max_run_num){
			EPRINT("over run in level", true);
		}
		run_deep_append_sstfile(&lev->array[lev->run_num], sptr);
		lev->run_num++;*/
	}
	else{
		if(lev->now_sst_num >lev->max_sst_num){
			EPRINT("over sst file level", true);
			return 1;
		}
		run_deep_append_sstfile(LAST_RUN_PTR(lev), sptr);
		lev->now_sst_num++;
	}
	return 0;
}

void level_trivial_move_sstfile(level *des, run *src, uint32_t from, uint32_t to){
	sst_file *sst_ptr;
	for_each_sst_at(src, sst_ptr, from){
		if(from>to) break;
		des->now_sst_num++;
		run_deep_append_sstfile(LAST_RUN_PTR(des), sst_ptr);
	}
}

void level_trivial_move_run(level *des, level *src, uint32_t from, uint32_t to, uint32_t num){
	if(level_is_appendable(des, src->now_sst_num)){
		EPRINT("no space to insert run", true);
	}
	run *run_ptr;
	uint32_t idx=0;
	for_each_run(src, run_ptr, idx){
		des->array[des->run_num]=*run_ptr;
		des->run_num++;
	}
}

uint32_t level_append_run_copy_move_originality(level *lev, run *r, uint32_t ridx){
	if(lev->level_type!=TIERING && lev->level_type!=TIERING_WISCKEY){
		EPRINT("it must be tiering level", true);
	}
	if(lev->run_num >= lev->max_run_num){
		EPRINT("over run in level", true);
	}

	run_shallow_copy_move_originality(&lev->array[ridx], r);
	lev->run_num++;
	lev->now_sst_num+=r->now_sst_num;
	lev->now_contents_num+=r->now_contents_num;
	//run_copy_src_empty(&lev->array[lev->run_num++], r);
	return 1;
}

uint32_t level_deep_append_run(level *lev, run *r){
	if(lev->level_type!=TIERING && lev->level_type!=TIERING_WISCKEY){
		EPRINT("it must be tiering level", true);
	}
	if(lev->run_num >= lev->max_run_num){
		EPRINT("over run in level", true);
	}

	run_deep_copy(&lev->array[lev->run_num++], r);
	lev->now_contents_num+=r->now_contents_num;
	return 1;
}

sst_file* level_retrieve_sst(level *lev, uint32_t lba){
	if(lev->level_type==TIERING || lev->level_type==TIERING_WISCKEY){
		EPRINT("Tier level not allowed!", true);
	}
	return run_retrieve_sst(&lev->array[0], lba);
}

sst_file* level_retrieve_close_sst(level *lev, uint32_t lba){
	if(lev->level_type==TIERING || lev->level_type==TIERING_WISCKEY){
		EPRINT("Tier level not allowed!", true);
	}
	return run_retrieve_close_sst(&lev->array[0], lba);
}

sst_file* level_retrieve_sst_with_check(level *lev, uint32_t lba){
	if(lev->level_type==TIERING || lev->level_type==TIERING_WISCKEY){
		EPRINT("Tier level not allowed!", true);
	}
	sst_file *res=run_retrieve_sst(&lev->array[0], lba);
	uint32_t temp;
	if(!res) return NULL;
	if(res->_read_helper){
		uint32_t helper_idx=read_helper_idx_init(res->_read_helper, lba);
		if(read_helper_check(res->_read_helper, lba, &temp, res, &helper_idx)){
			return res;	
		}
		else
			return NULL;
	}
	return res;
}

sst_file* level_find_target_run_idx(level *lev, uint32_t lba, uint32_t piece_ppa, uint32_t *target_ridx, uint32_t *sptr_idx){
	if(lev->level_type!=TIERING && lev->level_type != TIERING_WISCKEY){
		EPRINT("tiering only",true);
	}

	run *run_ptr;
	uint32_t ridx;
	sst_file *sptr;
	for_each_run_max(lev, run_ptr, ridx){
		if(run_ptr->now_sst_num==0) continue;
		sptr=run_retrieve_sst(run_ptr, lba);
		if(sptr && sptr->file_addr.piece_ppa<=piece_ppa &&
				sptr->end_ppa*L2PGAP>=piece_ppa){
			*target_ridx=ridx;
			*sptr_idx=(sptr-run_ptr->sst_set);
			return sptr;
		}
	}
	return NULL;
}

static inline void level_destroy_content(level *lev, page_manager *pm){
	if(!lev) return;
	run *run_ptr;
	uint32_t idx;
	/*assigned level*/
	for_each_run_max(lev, run_ptr, idx){
		if(run_ptr->now_sst_num){
			run_destroy_content(run_ptr, pm);
		}
	}
	for_each_run_max(lev, run_ptr, idx){
		if(!run_ptr->now_sst_num){
			free(run_ptr->sst_set);
		}
	}
}

void level_free(level *lev, page_manager *pm){
	level_destroy_content(lev, pm);
	free(lev->array);
	free(lev);
}

uint32_t level_update_run_at_move_originality(level *lev, uint32_t idx, run *r, bool new_run){
	if(lev->level_type!=TIERING && lev->level_type !=TIERING_WISCKEY){
		EPRINT("it must be tiering level", true);
	}

	if(lev->run_num > lev->max_run_num){
		EPRINT("over run in level", true);
	}

	if(!new_run){
		lev->now_contents_num-=lev->array[idx].now_contents_num;
	}

	run_shallow_copy_move_originality(&lev->array[idx], r);
	if(new_run){
		lev->now_sst_num+=r->now_sst_num;
		lev->now_contents_num+=r->now_contents_num;
		lev->run_num++;
	}
	else{
		lev->now_contents_num+=r->now_contents_num;
	}
	return 1;
}

uint32_t get_level_content_num(level *lev){
	uint32_t sidx=0, ridx=0;
	sst_file *sptr;
	run *rptr;
	uint32_t content_num=0;
	for_each_sst_level(lev, rptr, ridx, sptr, sidx){
		content_num+=read_helper_get_cnt(sptr->_read_helper);
	}
	return content_num;
}

void level_print(level *lev){
#ifdef LSM_DEBUG
	uint32_t sidx=0, ridx=0;
	sst_file *sptr;
	run *rptr;
	uint32_t content_num=0;
	for_each_sst_level(lev, rptr, ridx, sptr, sidx){
		content_num+=read_helper_get_cnt(sptr->_read_helper);
	}
	if(lev->now_sst_num){
		if(lev->level_type==TIERING || lev->level_type==TIERING_WISCKEY){
			printf("level idx:%d run %u/%u content_num: %u (%.2lf %%)\n",
					lev->idx,
					lev->run_num, lev->max_run_num,
					content_num, (double)content_num/RANGE*100
				  );	
		}
		else{
			printf("level idx:%d sst:%u/%u start lba:%u~end lba:%u content_num:%u (%.2lf %%)\n", 
					lev->idx,
					lev->now_sst_num, lev->max_sst_num,
					FIRST_RUN_PTR(lev)->start_lba,
					LAST_RUN_PTR(lev)->end_lba,
					content_num, (double)content_num/RANGE*100
				  );
		}
	}
	else{
		printf("level idx:%d run:%u/%u sst:%u/%u\n", 
			lev->idx,
			lev->run_num, lev->max_run_num,
			lev->now_sst_num, lev->max_sst_num);
	}
#endif
}

void level_content_print(level *lev, bool print_sst){
	run *rptr;
	uint32_t ridx;
	sst_file *sptr;
	uint32_t sidx;
	level_print(lev);
	for_each_run(lev, rptr, ridx){
		printf("\t[%u] ",ridx);
		run_print(rptr);
		if(print_sst){
			for_each_sst(rptr, sptr, sidx){
				printf("\t\t[%u] ",sidx);
				sst_print(sptr);
			}
		}
	}
}

void level_run_reinit(level *lev, uint32_t idx){
	lev->run_num--;
	run_reinit(&lev->array[idx]);
}

void level_sptr_update_in_gc(level *lev, uint32_t ridx, uint32_t sptr_idx, sst_file *sptr){
	if(lev->level_type!=TIERING && lev->level_type!=TIERING_WISCKEY){
		EPRINT("only tier available", true);
	}
	
	sst_file *org_sptr=&lev->array[ridx].sst_set[sptr_idx];
	/*free(org_sptr->block_file_map);
	read_helper_free(org_sptr->_read_helper);*/
	sst_reinit(org_sptr);

	lev->array[ridx].sst_set[sptr_idx]=(*sptr);

	sst_file *nxt_sstfile, *prev_sstfile;
	if(sptr_idx==0){
		if(lev->array[ridx].now_sst_num -1 >=sptr_idx+1){
			nxt_sstfile=&lev->array[ridx].sst_set[sptr_idx+1];
			if(sptr->end_lba < nxt_sstfile->start_lba){}
			else{
				EPRINT("range error", true);
			}
		}
	}
	else{
		prev_sstfile=&lev->array[ridx].sst_set[sptr_idx-1];

		if(lev->array[ridx].now_sst_num -1 >= sptr_idx+1){
			nxt_sstfile=&lev->array[ridx].sst_set[sptr_idx+1];
			if(sptr->end_lba < nxt_sstfile->start_lba){}
			else{
				EPRINT("range error", true);
			}
		}

		if(prev_sstfile->end_lba < sptr->start_lba){}
		else{
			EPRINT("range error", true);
		}
	}
}

void level_sptr_add_at_in_gc(level *lev, uint32_t ridx, uint32_t sptr_idx, sst_file *sptr){
	if(lev->level_type!=TIERING && lev->level_type!=TIERING_WISCKEY){
		EPRINT("only tier available", true);
	}

	/*range check*/
	sst_file *nxt_sstfile, *prev_sstfile;
	if(sptr_idx==0){
		if(lev->array[ridx].now_sst_num -1 >= sptr_idx+1){
			nxt_sstfile=&lev->array[ridx].sst_set[sptr_idx+1];
			if(sptr->end_lba < nxt_sstfile->start_lba){}
			else{
				EPRINT("range error", true);
			}
		}
	}
	else{
		prev_sstfile=&lev->array[ridx].sst_set[sptr_idx-1];

		if(lev->array[ridx].now_sst_num-1 >= sptr_idx+1){
			nxt_sstfile=&lev->array[ridx].sst_set[sptr_idx+1];
			if(sptr->end_lba < nxt_sstfile->start_lba){}
			else{
				EPRINT("range error", true);
			}
		}
		else if(lev->array[ridx].now_sst_num == sptr_idx){
		
		}

		if(prev_sstfile->end_lba < sptr->start_lba){}
		else{
			EPRINT("range error", true);
		}
	}


	uint32_t target_num=lev->array[ridx].now_sst_num-sptr_idx;
	if(sptr_idx+1+target_num >= lev->array[ridx].max_sst_num){
		run *now_r=&lev->array[ridx];
		sst_file *new_sst_set;
		new_sst_set=(sst_file*)calloc(now_r->max_sst_num+1, sizeof(sst_file));
		memcpy(new_sst_set, now_r->sst_set, sizeof(sst_file)*now_r->max_sst_num);
		free(now_r->sst_set);

		now_r->max_sst_num++;
		now_r->sst_set=new_sst_set;
		//		run_print(&lev->array[ridx]);
		//		EPRINT("over run", true);
	}

	if(sptr_idx<=lev->array[ridx].now_sst_num-1){
		memmove(&lev->array[ridx].sst_set[sptr_idx+1],&lev->array[ridx].sst_set[sptr_idx], sizeof(sst_file) *target_num);
	}
	else if(sptr_idx!=lev->array[ridx].now_sst_num){
		EPRINT("how can't be", true);
	}

	//free(org_sptr->block_file_map);
	lev->array[ridx].sst_set[sptr_idx]=(*sptr);
	lev->array[ridx].now_sst_num++;
}

void level_sptr_remove_at_in_gc(level *lev, uint32_t ridx, uint32_t sptr_idx){
	if(lev->level_type!=TIERING && lev->level_type!=TIERING_WISCKEY){
		EPRINT("only tier available", true);
	}
	
	run *r=&lev->array[ridx];
	if(r->now_sst_num==0 || r->now_sst_num <=sptr_idx){
		EPRINT("?????", true);
	}

	uint32_t target_num_sst_file=r->now_sst_num-sptr_idx-1;
	memmove(&r->sst_set[sptr_idx], &r->sst_set[sptr_idx+1], sizeof(sst_file) * target_num_sst_file);
	memset(&r->sst_set[r->now_sst_num-1], 0, sizeof(sst_file));
	r->now_sst_num--;
}

run *level_LE_to_run(level *lev, bool move_originality){
	if(lev->level_type!=LEVELING && lev->level_type!=LEVELING_WISCKEY){
		EPRINT("the lev should be LEVELING", true);
	}

	run *res=run_init(lev->max_sst_num, UINT32_MAX, 0);

	run *rptr;
	sst_file *sptr;
	uint32_t ridx, sidx;
	for_each_sst_level(lev, rptr, ridx, sptr, sidx){
		if(move_originality){
			run_append_sstfile_move_originality(res, sptr);
		}
		else{
			run_append_sstfile(res, sptr);
		}
	}
	return res;
}
void level_tiering_sst_analysis(level *lev, blockmanager *bm, version *v, bool merge){
	run *rptr; 
	uint32_t ridx;
	/*print all sptr range*/
	printf("A: %u start sst analysis:\n",lev->idx);
	uint32_t total_invalidate_num=0;
	uint32_t total_num=0;
	uint32_t run_total_validate_number=0;
	for_each_run_max(lev, rptr, ridx){
		sst_file *sptr;
		uint32_t sidx;
		if(!rptr->now_sst_num && !merge){
			if(ridx >= lev->run_num){
				break;
			}
			else{
				printf("????\n");
				abort();
			}
		}
		printf("A: %u ridx:%u\n", lev->idx, ridx);

		uint32_t entry_num=0;
		uint32_t block_num=0;
		uint32_t start_lba=0;
		uint32_t end_lba=0;
		uint32_t invalidate_cnt=0;
		uint32_t run_version=version_level_to_start_version(v, lev->idx)+ridx;
		run_total_validate_number+=rptr->now_contents_num;
		for_each_sst(rptr, sptr, sidx){
			uint32_t midx;
			map_range *mptr;
	//		printf("\tA: %u , %u sidx:%u %u~%u\n", lev->idx, ridx, sidx, sptr->start_lba, sptr->end_lba);
			for_each_map_range(sptr, mptr, midx){
				char mapping_value[PAGESIZE];
				io_manager_test_read(mptr->ppa, mapping_value, TEST_IO);
				key_ptr_pair *kp_set=(key_ptr_pair*)mapping_value;
				for(uint32_t i=0; kp_set[i].lba!=UINT32_MAX && i<KP_IN_PAGE; i++){
					uint32_t now_version=version_map_lba(v, kp_set[i].lba);
					total_num++;
					if(version_compare(v, now_version,run_version) > 0){
						invalidate_cnt++;
						total_invalidate_num++;
					}

					if(entry_num==0) start_lba=kp_set[i].lba;
					entry_num++;
					end_lba=kp_set[i].lba;
					if(entry_num==_PPB*L2PGAP){
	//					printf("\t\tA: %u bidx:%u %u~%u %u/%u\n", lev->idx, block_num++, start_lba, end_lba,
	//							invalidate_cnt, _PPB*L2PGAP);
						entry_num=start_lba=end_lba=invalidate_cnt=0;
					}
				}
			}
		}
		if(entry_num){
			printf("\t\tA: %u bidx:%u %u~%u %u/%u\n",lev->idx, block_num++, start_lba, end_lba,
					invalidate_cnt, _PPB*L2PGAP);
		}
	}

	uint32_t version_inv_cnt=version_get_level_invalidation_cnt(v, lev->idx);
	if(version_inv_cnt!=total_invalidate_num){
		EPRINT("differ value", true);
	}
	printf("A: %u end sst analysis: total_invalidate_num:%u/%u (%.2f), version_inv_cnt:%u, run_now_contents%u\n", lev->idx, 
			total_invalidate_num, total_num,  (float)total_invalidate_num/total_num, 
			version_get_level_invalidation_cnt(v, lev->idx), run_total_validate_number);
}

extern lsmtree LSM;
uint32_t level_run_populate_analysis(run *r){
	sst_file *sptr;
	uint32_t sidx=0;
	char mapping[PAGESIZE];
	key_ptr_pair *kp_ptr;
	uint32_t contents_num=0;
	uint32_t cnt_sum=0;
	for_each_sst(r, sptr, sidx){
		if(sptr->type==PAGE_FILE){
			io_manager_test_read(sptr->file_addr.map_ppa, mapping, TEST_IO);
			kp_ptr=(key_ptr_pair*)mapping;
			for(uint32_t i=0; kp_ptr[i].piece_ppa!=UINT32_MAX && i<KP_IN_PAGE; i++){
				cnt_sum+=LSM.LBA_cnt[kp_ptr[i].lba];
				contents_num++;
			}
		}
		else{
			map_range *mptr;
			uint32_t midx;
			for_each_map_range(sptr, mptr, midx){
				io_manager_test_read(mptr->ppa, mapping, TEST_IO);
				kp_ptr=(key_ptr_pair*)mapping;
				for(uint32_t i=0; kp_ptr[i].piece_ppa!=UINT32_MAX && i<KP_IN_PAGE; i++){
					cnt_sum+=LSM.LBA_cnt[kp_ptr[i].lba];
					contents_num++;
				}		
			}
		}
	}
	if(cnt_sum==0) return 0;
	return cnt_sum/contents_num;
}
