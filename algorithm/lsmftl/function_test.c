#include "function_test.h"
#include "key_value_pair.h"
#include "io.h"
#include "lsmtree.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
extern lsmtree LSM;
static inline bool kp_data_check(char *data, uint32_t lev_idx, uint32_t run_idx, 
		uint32_t sst_idx, uint32_t find_lba, bool should_print){

	key_ptr_pair *kp_ptr; uint32_t kp_idx;
	uint32_t prev=UINT32_MAX;
	char value_data[PAGESIZE];
	for_each_kp(data, kp_ptr, kp_idx){
		if(should_print){
			printf("\t\t\tkp #%d: %u->%u\n", kp_idx, kp_ptr->lba, kp_ptr->piece_ppa);
		}


		if(prev==UINT32_MAX){
			prev=kp_ptr->lba;
		}
		else{
			if(prev>=kp_ptr->lba){
				EPRINT("sorting fail!", true);
			}
		}	

		if(kp_ptr->lba==UINT32_MAX) break;
		if(kp_ptr->lba==find_lba){
			printf("find_target:%u ppa:%u lev_idx:%u, run_idx:%u!\n", 
					find_lba, kp_ptr->piece_ppa, lev_idx, run_idx);
			return true;
		}

		io_manager_test_read(PIECETOPPA(kp_ptr->piece_ppa), value_data, TEST_IO);
		if(kp_ptr->lba!=*(uint32_t*)&value_data[LPAGESIZE*(kp_ptr->piece_ppa%L2PGAP)]){
			EPRINT("data fail", true);
		}

	}
	return false;
}

static inline bool kp_data_consistency_check(char *data, uint32_t version, bool version_check){

	key_ptr_pair *kp_ptr; uint32_t kp_idx;
	uint32_t prev=UINT32_MAX;
	char value_data[PAGESIZE];
	for_each_kp(data, kp_ptr, kp_idx){
		if(prev==UINT32_MAX){
			prev=kp_ptr->lba;
		}
		else{
			if(prev>=kp_ptr->lba){
				EPRINT("sorting fail!", true);
			}
		}	

		if(kp_ptr->lba==UINT32_MAX) break;

		io_manager_test_read(PIECETOPPA(kp_ptr->piece_ppa), value_data, TEST_IO);
		uint32_t temp_data=*(uint32_t*)&value_data[LPAGESIZE*(kp_ptr->piece_ppa%L2PGAP)];

		if(version_check && version_map_lba(LSM.last_run_version, kp_ptr->lba)!=version){
			EPRINT("version is not matched!", false);
		}

		if(kp_ptr->lba!=temp_data){
			printf("map data:%u,%u real data:%u\n", kp_ptr->lba, kp_ptr->piece_ppa, temp_data);
			EPRINT("data fail", true);
		}

	}
	return false;
}

void LSM_traversal(lsmtree *lsm){
	lsmtree_parameter *param=&lsm->param;
	for(uint32_t i=0; i<param->LEVELN-1; i++){
		level *disk=lsm->disk[i];
		run *rptr; uint32_t r_idx;
		printf("level #%d:\n", i);
		for_each_run(disk, rptr, r_idx){
			sst_file *sptr; uint32_t s_idx;
			printf("\trun #%d:\n", r_idx);
			for_each_sst(rptr, sptr, s_idx){
				printf("\t\tsstfile #%d:\n", s_idx);
				char map_data[PAGESIZE];
				if(sptr->type==PAGE_FILE){
					io_manager_test_read(sptr->file_addr.map_ppa, map_data, TEST_IO);
					kp_data_check(map_data, i, r_idx, s_idx, UINT32_MAX, false);
				}
				else{
					map_range *mr;
					for(uint32_t mr_idx=0; mr_idx<sptr->map_num; mr_idx++){
						mr=&sptr->block_file_map[mr_idx];
						io_manager_test_read(mr->ppa, map_data, TEST_IO);
						kp_data_check(map_data, i, r_idx, s_idx, UINT32_MAX, false);
					}
				}
			}
		}	
	}
}

bool LSM_find_lba(lsmtree *lsm, uint32_t lba){
	lsmtree_parameter *param=&lsm->param;
	for(uint32_t i=0; i<=param->LEVELN-1; i++){
		level *disk=lsm->disk[i];
		run *rptr; uint32_t r_idx;
	//	printf("level #%d:\n", i);
		for_each_run(disk, rptr, r_idx){
			sst_file *sptr; uint32_t s_idx;
	//		printf("\trun #%d:\n", r_idx);
			for_each_sst(rptr, sptr, s_idx){
	//			printf("\t\tsstfile #%d:\n", s_idx);
				char map_data[PAGESIZE];
				if(sptr->type==PAGE_FILE){
					io_manager_test_read(sptr->file_addr.map_ppa, map_data, TEST_IO);
					if(kp_data_check(map_data, i, r_idx, s_idx, lba, false)){
						printf("level:%u run:%u s_idx:%u\n", i, r_idx, s_idx);
						return true;
					}
				}
				else{
					map_range *mr;
					for(uint32_t mr_idx=0; mr_idx<sptr->map_num; mr_idx++){
						mr=&sptr->block_file_map[mr_idx];
						io_manager_test_read(mr->ppa, map_data, TEST_IO);
						if(kp_data_check(map_data, i, r_idx, s_idx, lba, false)){
							printf("level:%u run:%u s_idx:%u\n", i, r_idx, s_idx);
							return true;
						}
					}
				}
			}
		}	
	}
	return false;
}

bool LSM_level_find_lba(level *lev, uint32_t lba){
	run *rptr; uint32_t r_idx;
	//	printf("level #%d:\n", i);
	for_each_run(lev, rptr, r_idx){
		sst_file *sptr; uint32_t s_idx;
		//		printf("\trun #%d:\n", r_idx);
		for_each_sst(rptr, sptr, s_idx){
			//			printf("\t\tsstfile #%d:\n", s_idx);
			char map_data[PAGESIZE];
			if(sptr->type==PAGE_FILE){
				io_manager_test_read(sptr->file_addr.map_ppa, map_data, TEST_IO);
				if(kp_data_check(map_data, lev->idx, r_idx, s_idx, lba, false))
					return true;
			}
			else{
				map_range *mr;
				for(uint32_t mr_idx=0; mr_idx<sptr->map_num; mr_idx++){
					mr=&sptr->block_file_map[mr_idx];
					io_manager_test_read(mr->ppa, map_data, TEST_IO);
					if(kp_data_check(map_data, lev->idx, r_idx, s_idx, lba, false))
						return true;
				}
			}
		}
	}
	return false;
}

void level_consistency_check(level *lev, bool version_check){
	run *rptr; uint32_t r_idx;

	for_each_run_max(lev, rptr, r_idx){
		sst_file *sptr; uint32_t s_idx;
		uint32_t prev_sst_start=UINT32_MAX;
		uint32_t prev_sst_end=UINT32_MAX;
		printf("ridx:%u testing\n", r_idx);
		uint32_t start_version=version_level_to_start_version(LSM.last_run_version, lev->idx);
		for_each_sst(rptr, sptr, s_idx){
			printf("\t sidx:%u testing\n",s_idx);
			uint32_t start_lba=UINT32_MAX, end_lba=0;
			char map_data[PAGESIZE];
			if(sptr->type==PAGE_FILE){
				io_manager_test_read(sptr->file_addr.map_ppa, map_data, TEST_IO);
				
				uint32_t temp_lba=((key_ptr_pair*)map_data)[0].lba;
				start_lba=start_lba>temp_lba?temp_lba:start_lba;

				temp_lba=kp_get_end_lba(map_data);
				end_lba=end_lba<temp_lba?temp_lba:end_lba;

				kp_data_consistency_check(map_data, start_version+r_idx, version_check);
			}
			else{
				map_range *mr;
				for(uint32_t mr_idx=0; mr_idx<sptr->map_num; mr_idx++){
					mr=&sptr->block_file_map[mr_idx];
					io_manager_test_read(mr->ppa, map_data, TEST_IO);

					uint32_t temp_lba=((key_ptr_pair*)map_data)[0].lba;
					start_lba=start_lba>temp_lba?temp_lba:start_lba;

					temp_lba=kp_get_end_lba(map_data);
					end_lba=end_lba<temp_lba?temp_lba:end_lba;

					kp_data_consistency_check(map_data, start_version+r_idx, version_check);
				}
			}

			if(prev_sst_start!=UINT32_MAX){
				if(!(start_lba>prev_sst_end)){
					EPRINT("error!", true);
				}
			}
			prev_sst_start=start_lba;
			prev_sst_end=end_lba;

		}
	}	

}
