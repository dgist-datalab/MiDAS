#include "sst_file.h"
#include "lsmtree.h"
#include <stdlib.h>

extern lsmtree LSM;

sst_file *sst_init_empty(uint8_t type){
	sst_file *res=(sst_file*)calloc(1,sizeof(sst_file));
	res->type=type;
	return res;
}

sst_file *sst_pf_init(uint32_t ppa, uint32_t start_lba, uint32_t end_lba){
	sst_file *res=(sst_file*)calloc(1,sizeof(sst_file));
	res->file_addr.map_ppa=ppa;
	res->start_lba=start_lba;
	res->end_lba=end_lba;
	res->type=PAGE_FILE;
	return res;
}

sst_file *sst_bf_init(uint32_t ppa, uint32_t end_ppa, uint32_t start_lba, uint32_t end_lba){
	sst_file *res=(sst_file*)calloc(1,sizeof(sst_file));
	res->file_addr.piece_ppa=ppa;
	res->end_ppa=end_ppa;
	res->start_lba=start_lba;
	res->end_lba=end_lba;
	res->type=BLOCK_FILE;
	return res;
}

void sst_destroy_content(sst_file* sstfile, page_manager *pm){
	//free_sstfile_ readhelper...
	
	if(!sstfile->ismoved_originality && sstfile->type==BLOCK_FILE){
		if(sstfile->block_file_map){
			/* only invalidate in merge
			for(uint32_t i=0; i<sstfile->map_num; i++){
				invalidate_map_ppa(pm->bm, sstfile->block_file_map[i].ppa, true);
			}*/

			free(sstfile->block_file_map);
		}
	}
	if(sstfile->data){
		EPRINT("data not free", true);
	}
	if(!sstfile->ismoved_originality && sstfile->_read_helper){
		read_helper_free(sstfile->_read_helper);
	}

	return;
}

void sst_reinit(sst_file *sptr){
	sst_destroy_content(sptr, LSM.pm);
	memset(sptr, 0, sizeof(sst_file));
}

void sst_set_file_map(sst_file *sstfile, uint32_t map_num, map_range *map_range){
	if(sstfile->type!=BLOCK_FILE){
		EPRINT("cannot have map!", true);
	}
	sstfile->block_file_map=map_range;
	/*
	for(uint32_t i=0; i<map_num; i++){
		printf("[%p]s:%u e:%u p:%u\n", sstfile,
				sstfile->block_file_map[i].start_lba,
				sstfile->block_file_map[i].end_lba,
				sstfile->block_file_map[i].ppa);
	}*/
}

uint32_t sst_find_map_addr(sst_file *sstfile, uint32_t lba){
	if(sstfile->type!=BLOCK_FILE){
		EPRINT("cannot have map!", true);
	}
	uint32_t res=UINT32_MAX;

	int s=0, e=sstfile->map_num;
	while(s<=e){
		int mid=(s+e)/2;
		map_range *mr=&sstfile->block_file_map[mid];
		if(mr->start_lba<=lba && mr->end_lba>=lba){
			res=mr->ppa;
			break;
		}
		if(mr->start_lba > lba){
			e=mid-1;
		}
		else if(mr->end_lba<lba){
			s=mid+1;
		}
	}

	return res;
}

void sst_free(sst_file* sstfile, page_manager *pm){
	//free_sstfile
	if(!sstfile->ismoved_originality && sstfile->block_file_map){
		/* only invalidate in merge
		if(!sstfile->alread_invalidate){
			for(uint32_t i=0; i<sstfile->map_num; i++){
				invalidate_map_ppa(pm->bm, sstfile->block_file_map[i].ppa, true);
			}
		}*/
		free(sstfile->block_file_map);
	}
	if(sstfile->data){
		EPRINT("data not free", true);
	}
	if(!sstfile->ismoved_originality && sstfile->_read_helper){
		read_helper_free(sstfile->_read_helper);
	}
	free(sstfile);
}

void sst_deep_copy(sst_file *des, sst_file *src){
	*des=*src;
	if(src->block_file_map){
		des->map_num=src->map_num;
		des->block_file_map=(map_range*)malloc(sizeof(map_range) * des->map_num);
		memcpy(des->block_file_map, src->block_file_map, sizeof(map_range) * des->map_num);
	}

	if(src->_read_helper){
		des->_read_helper=read_helper_copy(src->_read_helper);
	}
	//des->_read_helper=src->_read_helper;
	/*
	src->block_file_map=NULL;
	src->_read_helper=NULL;
	 */
}


void sst_convert_seq_pf_to_bf(sst_file *src){
	if(src->type!=PAGE_FILE || !src->sequential_file){
		EPRINT("not allowed type", true);
	}

	src->type=BLOCK_FILE;
	map_range *mr=(map_range*)malloc(sizeof(map_range));
	mr->start_lba=src->start_lba;
	mr->end_lba=src->end_lba;
	mr->ppa=src->file_addr.map_ppa;
	
	src->map_num=1;
	src->block_file_map=mr;

	src->file_addr.piece_ppa=src->start_piece_ppa;
}

sst_file *sst_MR_to_sst_file(map_range *mr){
	sst_file *res=(sst_file*)calloc(1, sizeof(sst_file));
	res->file_addr.map_ppa=mr->ppa;
	res->start_lba=mr->start_lba;
	res->end_lba=mr->end_lba;
	res->type=PAGE_FILE;
	return res;
}

void sst_print(sst_file *sptr){
	printf("%s range:%u~%u ", sptr->type==PAGE_FILE?"PAGE_FILE":"BLOCK_FILE",
			sptr->start_lba, sptr->end_lba);
	if(sptr->type==PAGE_FILE){
		printf("map-ppa:%u\n",sptr->file_addr.map_ppa);
	}
	else{
		printf("file-ppa:%u~%u map-num:%u\n", sptr->file_addr.piece_ppa, 
				sptr->end_ppa*L2PGAP, sptr->map_num);
	}
}

void map_print(map_range *mr){
	printf("range:%u~%u ppa:%u\n", mr->start_lba, mr->end_lba, mr->ppa);
}
