#include "sftl_cache.h"
#include "bitmap_ops.h"

extern sftl_cache_monitor scm;
void sftl_mapping_verify(sftl_cache* sc){
	bool isset;
	int32_t max=PAGESIZE/sizeof(uint32_t);
	uint32_t offset=0;
	//uint32_t ppa=sc->head_array[0];
	uint32_t i=0;
	uint32_t max_head=(scm.gtd_size[sc->etr->idx]-BITMAPSIZE)/sizeof(uint32_t);
	uint32_t ppa_list_idx=0;
	for_each_bitmap_forward(sc->map, offset, isset, max){
		if(isset){
			ppa_list_idx++;
			if(ppa_list_idx > max_head){
				printf("head num and gtd_size is different %s:%d\n", __FILE__, __LINE__);
				abort();
			}
		}
		else{
			if(i==0){
				printf("first bit is not set %s:%d\n", __FILE__, __LINE__);
				abort();
			}
		}
		i++;
	}
	if(ppa_list_idx!=max_head){
		sftl_print_mapping(sc);
		abort();
	}
}



void sftl_print_mapping(sftl_cache *sc){
	bool isset;
	int32_t max=PAGESIZE/sizeof(uint32_t);
	uint32_t offset=0;
	uint32_t ppa_list_idx=0;
	uint32_t now_ppa=0;
	uint32_t seq_cnt=0;
	uint32_t i=0;
	bool isstart=true;
	printf("--------%u print_sc: size:%u max_offset:%u\n",sc->etr->idx, scm.gtd_size[sc->etr->idx], max);

	bool debug_flag=false;
	for_each_bitmap_forward(sc->map, offset, isset, max){
		if(isset){
			now_ppa=sc->head_array[ppa_list_idx++];
			if(now_ppa==UINT32_MAX) continue;
			if(!isstart){
				printf("seq: %d\n", seq_cnt);
				if(debug_flag && seq_cnt!=0){
					printf("break!\n");
				}
				debug_flag=false;
			}
			else isstart=false;
			seq_cnt=0;
			printf("%d\t (LBA:%lu head-%d, idx:%d) ", ppa_list_idx,sc->etr->idx*PAGESIZE/sizeof(uint32_t)+offset, now_ppa, i);
			if(sc->etr->idx==483 && now_ppa > 1070000){
				debug_flag=true;
			}
		}
		else{
			seq_cnt++;
			/*
			printf("(%d:%d) ", i, now_ppa+seq_cnt);
			if((seq_cnt)%10==0){
				printf("\n\t");
			}*/
		}
		i++;
	}
	printf("seq: %d\n",seq_cnt);
	if(debug_flag && seq_cnt!=0){
		printf("break!\n");
	}
}

uint32_t sftl_print_mapping_target(sftl_cache *sc, uint32_t lba){
	bool isset;
	int32_t max=PAGESIZE/sizeof(uint32_t);
	uint32_t offset=0;
	uint32_t ppa_list_idx=0;
	uint32_t now_ppa=0;
	uint32_t seq_cnt=0;
	uint32_t i=0;
	bool isstart=true;

	for_each_bitmap_forward(sc->map, offset, isset, max){
		if(isset){
			now_ppa=sc->head_array[ppa_list_idx++];
			if(now_ppa==UINT32_MAX) continue;
			else isstart=false;
			seq_cnt=0;
		}
		else{
			seq_cnt++;
		}
		if(GETOFFSET(lba)==i){
			printf("target: %u -> %u\n", lba, now_ppa+seq_cnt);
			return now_ppa+seq_cnt;
		}
		if(now_ppa==0  && i!=0){
			abort();
		}
		i++;
	}
	if(GETOFFSET(lba)==i){
		printf("target: %u -> %u\n", lba, now_ppa==UINT32_MAX? UINT32_MAX:now_ppa+seq_cnt);
		return now_ppa==UINT32_MAX? UINT32_MAX:now_ppa+seq_cnt;
	}
	return -1;
}
