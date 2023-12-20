#include "sftl_cache.h"
#include "bitmap_ops.h"
#include "../../demand_mapping.h"
#include "../../../../include/settings.h"
#include <stdio.h>
#include <stdlib.h>

extern uint32_t debug_lba;
extern uint32_t test_ppa;
//bool global_debug_flag=false;

my_cache sftl_cache_func{
	.init=sftl_init,
	.free=sftl_free,
	.is_needed_eviction=sftl_is_needed_eviction,
	.need_more_eviction=sftl_is_needed_eviction,
	.update_eviction_hint=sftl_update_eviction_hint,
	.is_hit_eviction=sftl_is_hit_eviction,
	.update_hit_eviction_hint=sftl_update_hit_eviction_hint,
	.is_eviction_hint_full=sftl_is_eviction_hint_full,
	.get_remain_space=sftl_get_remain_space,
	.update_entry=sftl_update_entry,
	.update_entry_gc=sftl_update_entry_gc,
	.force_put_mru=sftl_force_put_mru,
	.insert_entry_from_translation=sftl_insert_entry_from_translation,
	.update_from_translation_gc=sftl_update_from_translation_gc,
	.get_mapping=sftl_get_mapping,
	.get_eviction_GTD_entry=sftl_get_eviction_GTD_entry,
	.get_eviction_mapping_entry=NULL,
	.update_eviction_target_translation=sftl_update_eviction_target_translation,
	.evict_target=NULL,
	.update_dynamic_size=sftl_update_dynamic_size,
	.exist=sftl_exist,
};

sftl_cache_monitor scm;
extern demand_map_manager dmm;

uint32_t sftl_init(struct my_cache *mc, uint32_t total_caching_physical_pages){
	lru_init(&scm.lru, NULL, NULL);
	scm.max_caching_byte=total_caching_physical_pages * PAGESIZE;
	scm.now_caching_byte=0;
	mc->type=COARSE;
	mc->entry_type=DYNAMIC;
	mc->private_data=NULL;
	scm.gtd_size=(uint32_t*)malloc(GTDNUM *sizeof(uint32_t));
	for(uint32_t i=0; i<GTDNUM; i++){
		scm.gtd_size[i]=BITMAPSIZE+PAGESIZE;
	}

	printf("|\tcaching <min> percentage: %.2lf%%\n", (double) ((scm.max_caching_byte/(BITMAPSIZE+PAGESIZE)) * BITMAPMEMBER)/ RANGE *100);
	return (scm.max_caching_byte/(BITMAPSIZE+sizeof(uint32_t))) * BITMAPMEMBER;
}

uint32_t sftl_free(struct my_cache *mc){
	while(1){
		sftl_cache *sc=(sftl_cache*)lru_pop(scm.lru);
		if(!sc) break;
		free(sc->head_array);
		bitmap_free(sc->map);
		free(sc);
	}
	printf("now byte:%u max_byte:%u\n", scm.now_caching_byte, scm.max_caching_byte);
	lru_free(scm.lru);
	free(scm.gtd_size);
	return 1;
}

uint32_t sftl_is_needed_eviction(struct my_cache *mc, uint32_t lba, uint32_t *, uint32_t eviction_hint){
	uint32_t target_size=scm.gtd_size[GETGTDIDX(lba)];
	if(scm.max_caching_byte <= scm.now_caching_byte+target_size+sizeof(uint32_t)*2+(eviction_hint)){
		return scm.now_caching_byte==0? EMPTY_EVICTION : NORMAL_EVICTION;
	}

	/*sftl get eviction gtd entry*/
	/*
	lru_node *target;
	GTD_entry *etr=NULL;
	for_each_lru_backword(scm.lru, target){
		sftl_cache *sc=(sftl_cache*)target->data;
		etr=sc->etr;
		if(target_size > scm.gtd_size[etr->idx]){
			break;
		}
	}*/

	if(scm.max_caching_byte <= scm.now_caching_byte){
		printf("now caching byte bigger!!!! %s:%d\n", __FILE__, __LINE__);
		abort();
	}
	return HAVE_SPACE;
}

uint32_t sftl_update_eviction_hint(struct my_cache *, uint32_t lba, uint32_t * /*prefetching_info*/,uint32_t eviction_hint, 
		uint32_t *now_eviction_hint, bool increase){
	uint32_t target_size=scm.gtd_size[GETGTDIDX(lba)];
	if(increase){
		*now_eviction_hint=target_size+sizeof(uint32_t)*2;
		return eviction_hint+*now_eviction_hint;
	}
	else{
		return eviction_hint-*now_eviction_hint;
	}
}

inline static sftl_cache* get_initial_state_cache(uint32_t gtd_idx, GTD_entry *etr){
	sftl_cache *res=(sftl_cache *)malloc(sizeof(sftl_cache));
	res->head_array=(uint32_t*)malloc(PAGESIZE);
	memset(res->head_array, -1, PAGESIZE);
	res->map=bitamp_set_init(BITMAPMEMBER);
	res->etr=etr;
	
	scm.gtd_size[gtd_idx]=PAGESIZE+BITMAPSIZE;
	
	return res;
}

inline static uint32_t get_ppa_from_sc(sftl_cache *sc, uint32_t lba){
	uint32_t head_offset=get_head_offset(sc->map, lba);
	uint32_t distance=get_distance(sc->map, lba);
	return sc->head_array[head_offset]+distance;
}

inline static bool is_sequential(sftl_cache *sc, uint32_t lba, uint32_t ppa){
	uint32_t offset=GETOFFSET(lba);
	if(offset==0) return false;

	uint32_t head_offset=get_head_offset(sc->map, lba-1);
	uint32_t distance=0;
	if(!bitmap_is_set(sc->map, GETOFFSET(lba-1))){
		distance=get_distance(sc->map, lba-1);
	}
	
	if(sc->head_array[head_offset]+distance+1==ppa){
		if(sc->head_array[head_offset]==UINT32_MAX) return false;
		return true;
	}
	else return false;
}

static inline void sftl_size_checker(uint32_t eviction_hint){
	if(scm.now_caching_byte+eviction_hint> scm.max_caching_byte/100*110){
		printf("n:%u m:%u e:%ucaching overflow! %s:%d\n", scm.now_caching_byte, scm.max_caching_byte, eviction_hint, __FILE__, __LINE__);
		abort();
	}
}

enum NEXT_STEP{
	DONE, SHRINK, EXPAND
};

inline static uint32_t shrink_cache(sftl_cache *sc, uint32_t lba, uint32_t ppa, char *should_more, uint32_t *more_ppa){

	bool is_next_do=false;
	uint32_t next_original_ppa;

	if(!ISLASTOFFSET(lba+1)){
		if(!bitmap_is_set(sc->map, GETOFFSET(lba+1))){
			is_next_do=true;
			next_original_ppa=get_ppa_from_sc(sc, lba+1);
		}
	}

	uint32_t old_ppa;
	if(bitmap_is_set(sc->map, GETOFFSET(lba))){

		uint32_t head_offset=get_head_offset(sc->map, lba);
		old_ppa=sc->head_array[head_offset];
		uint32_t total_head=(scm.gtd_size[GETGTDIDX(lba)]-BITMAPSIZE)/sizeof(uint32_t);
		uint32_t *new_head_array=(uint32_t*)malloc((total_head-1+(is_next_do?1:0))*sizeof(uint32_t));
	
		memcpy(new_head_array, sc->head_array, (head_offset)*sizeof(uint32_t));
		if(is_next_do){
			bitmap_set(sc->map, GETOFFSET(lba+1));
			new_head_array[head_offset]=next_original_ppa;
		}
		
		if(total_head!=head_offset+(is_next_do?1:0)){
			memcpy(&new_head_array[head_offset+(is_next_do?1:0)], &sc->head_array[(head_offset+1)], (total_head-1-head_offset)*sizeof(uint32_t));
		}
		free(sc->head_array);
		sc->head_array=new_head_array;
		scm.gtd_size[GETGTDIDX(lba)]=(total_head-1+(is_next_do?1:0))*sizeof(uint32_t)+BITMAPSIZE;

		bitmap_unset(sc->map, GETOFFSET(lba));
	}
	else{
		printf("it cannot be sequential with previous ppa %s:%d\n", __FILE__, __LINE__);
		abort();
	}

	if(!ISLASTOFFSET(lba)){
		if(GETOFFSET(lba+1)< PAGESIZE/sizeof(uint32_t) ){
			if(bitmap_is_set(sc->map, GETOFFSET(lba+1))){
				uint32_t next_ppa=get_ppa_from_sc(sc, lba+1);
				if(ppa+1==next_ppa){
					*should_more=SHRINK;
					*more_ppa=next_ppa;
				}
				else{
					*should_more=DONE;
				}
			}
			else{
				*should_more=EXPAND;
				*more_ppa=get_ppa_from_sc(sc, lba+1);
			}
		}
		else{
			*should_more=DONE;
		}
	}

	return old_ppa;
}

inline static uint32_t expand_cache(sftl_cache *sc, uint32_t lba, uint32_t ppa, char *should_more, uint32_t *more_ppa){
	uint32_t old_ppa;	
	uint32_t head_offset;
	uint32_t distance=0;
	head_offset=get_head_offset(sc->map, lba);

	bool is_next_do=false;
	uint32_t next_original_ppa;
	if(ISLASTOFFSET(lba+1)){}
	else{
		if(!bitmap_is_set(sc->map, GETOFFSET(lba+1))){
			is_next_do=true;
			next_original_ppa=get_ppa_from_sc(sc, lba+1);
		}
	}

	if(!bitmap_is_set(sc->map, GETOFFSET(lba))){

		distance=get_distance(sc->map, lba);
		old_ppa=sc->head_array[head_offset]+distance;

		uint32_t total_head=(scm.gtd_size[GETGTDIDX(lba)]-BITMAPSIZE)/sizeof(uint32_t);
		uint32_t *new_head_array=(uint32_t*)malloc((total_head+1+(is_next_do?1:0))*sizeof(uint32_t));

		memcpy(new_head_array, sc->head_array, (head_offset+1) * sizeof(uint32_t));
		new_head_array[head_offset+1]=ppa;

		if(is_next_do){
			bitmap_set(sc->map, GETOFFSET(lba+1));
			new_head_array[head_offset+2]=next_original_ppa;
			if(total_head!=head_offset+1){
				memcpy(&new_head_array[head_offset+3], &sc->head_array[head_offset+1], (total_head-(head_offset+1)) * sizeof(uint32_t));
			}
		}
		else{
			if(total_head!=head_offset+1){
				memcpy(&new_head_array[head_offset+2], &sc->head_array[head_offset+1], (total_head-(head_offset+1)) * sizeof(uint32_t));
			}
		}
		free(sc->head_array);
		sc->head_array=new_head_array;
		scm.gtd_size[GETGTDIDX(lba)]=(total_head+1+(is_next_do?1:0))*sizeof(uint32_t)+BITMAPSIZE;
		bitmap_set(sc->map, GETOFFSET(lba));
	//	sftl_print_mapping(sc);
	}
	else{
		old_ppa=sc->head_array[head_offset];
		sc->head_array[head_offset]=ppa;
	
		if(is_next_do){
			uint32_t total_head=(scm.gtd_size[GETGTDIDX(lba)]-BITMAPSIZE)/sizeof(uint32_t);
			uint32_t *new_head_array=(uint32_t*)malloc((total_head+1)*sizeof(uint32_t));

			memcpy(new_head_array, sc->head_array, (head_offset+1) * sizeof(uint32_t));
			new_head_array[head_offset+1]=next_original_ppa;
			bitmap_set(sc->map, GETOFFSET(lba+1));

			if(total_head!=head_offset+1){
				memcpy(&new_head_array[head_offset+2], &sc->head_array[head_offset+1], (total_head-(head_offset+1)) * sizeof(uint32_t));
			}
			free(sc->head_array);
			sc->head_array=new_head_array;
			scm.gtd_size[GETGTDIDX(lba)]=(total_head+1)*sizeof(uint32_t)+BITMAPSIZE;
			bitmap_set(sc->map, GETOFFSET(lba));
		}
		else{
			bitmap_set(sc->map, GETOFFSET(lba));
		}
	}

	if(GETOFFSET(lba+1)< PAGESIZE/sizeof(uint32_t)){
		if(bitmap_is_set(sc->map, GETOFFSET(lba+1))){
			//check if it can be compressed
			*more_ppa=get_ppa_from_sc(sc, lba+1);
			if(ppa+1==(*more_ppa)){
				*should_more=SHRINK;
			}
			else{
				*should_more=DONE;
			}
		}
		else{
			*more_ppa=get_ppa_from_sc(sc, lba+1);
			*should_more=EXPAND;	
		}
	}

	return old_ppa;
}


inline static uint32_t __update_entry(GTD_entry *etr, uint32_t lba, uint32_t ppa, bool isgc, uint32_t *eviction_hint){
	sftl_cache *sc;

	uint32_t old_ppa;
	uint32_t gtd_idx=GETGTDIDX(lba);
	int32_t prev_gtd_size;
	int32_t changed_gtd_size;
	lru_node *ln;

	if(etr->status==EMPTY){
		sc=get_initial_state_cache(gtd_idx, etr);
		ln=lru_push(scm.lru, sc);
		etr->private_data=(void*)ln;
	}else{
		if(scm.now_caching_byte <= scm.gtd_size[gtd_idx]){
			scm.now_caching_byte=0;
		}
		else{
			scm.now_caching_byte-=scm.gtd_size[gtd_idx];
		}
		if(etr->private_data==NULL){
			printf("insert translation page before cache update! %s:%d\n",__FILE__, __LINE__);
			print_stacktrace();
			abort();
		}
		ln=(lru_node*)etr->private_data;
		sc=(sftl_cache*)(ln->data);
	}

	prev_gtd_size=scm.gtd_size[gtd_idx];
/*
	if(lba==2129921){//GETGTDIDX(lba)==520){
		printf("prev %u-%u : ", lba, ppa);
		sftl_print_mapping(sc);
	}


	if(etr->idx==535){
		sftl_mapping_verify(sc);
		sftl_print_mapping(sc);
		printf("pair: %u, %u physical:%u\n", lba, ppa, etr->physical_address);
	}
*/

	uint32_t more_lba=lba;
	uint32_t more_ppa;
	char should_more=false;
	if(is_sequential(sc, lba, ppa)){
		old_ppa=shrink_cache(sc, lba, ppa, &should_more, &more_ppa);
	}
	else{
		old_ppa=expand_cache(sc, lba, ppa, &should_more, &more_ppa);
	}

	while(should_more!=DONE){
		more_lba++;
		switch(should_more){
			case SHRINK:
				shrink_cache(sc, more_lba, more_ppa, &should_more, &more_ppa);
				break;
			case EXPAND:
				expand_cache(sc, more_lba, more_ppa, &should_more, &more_ppa);
				break;
			default:
				break;
		}
	}
/*
	if(etr->idx==535){
		sftl_mapping_verify(sc);
		sftl_print_mapping(sc);
		printf("pair: %u, %u physical:%u\n", lba, ppa, etr->physical_address);
	}
	
*/
	//sftl_mapping_verify(sc);

	changed_gtd_size=scm.gtd_size[gtd_idx];
	if(changed_gtd_size - prev_gtd_size > (int)sizeof(uint32_t)*2){
		printf("what happen???\n");
		abort();
	}
/*
	if(lba==2129921){//GETGTDIDX(lba)==520){
		printf("after %u-%u:", lba, ppa);
		sftl_print_mapping(sc);
		sftl_mapping_verify(sc);
	}
*/
	if((scm.gtd_size[gtd_idx]-BITMAPSIZE)/sizeof(uint32_t) > PAGESIZE/sizeof(uint32_t)){
		printf("oversize!\n");
		sftl_print_mapping(sc);
		abort();
	}
	scm.now_caching_byte+=scm.gtd_size[gtd_idx];
	
	if(eviction_hint){
		sftl_size_checker(*eviction_hint);
	}
	else{
		sftl_size_checker(0);
	}

	if(!isgc){
		lru_update(scm.lru, ln);
	}
	etr->status=DIRTY;
	return old_ppa;
}
extern uint32_t test_ppa;
uint32_t sftl_update_entry(struct my_cache *, GTD_entry *etr, uint32_t lba, uint32_t ppa, uint32_t *eviction_hint){
	return __update_entry(etr, lba, ppa, false, eviction_hint);
}

uint32_t sftl_update_entry_gc(struct my_cache *, GTD_entry *etr, uint32_t lba, uint32_t ppa){
	return __update_entry(etr, lba, ppa, true, NULL);
}

static inline sftl_cache *make_sc_from_translation(GTD_entry *etr, char *data){
	sftl_cache *sc=(sftl_cache*)malloc(sizeof(sftl_cache));
	sc->map=bitmap_init(BITMAPMEMBER);
	sc->etr=etr;

	uint32_t total_head=(scm.gtd_size[etr->idx]-BITMAPSIZE)/sizeof(uint32_t);
	uint32_t *new_head_array=(uint32_t*)malloc(total_head * sizeof(uint32_t));
	uint32_t *ppa_list=(uint32_t*)data;
	uint32_t last_ppa, new_head_idx=0;
	bool sequential_flag=false;

	for(uint32_t i=0; i<PAGESIZE/sizeof(uint32_t); i++){
		if(new_head_idx>total_head){
			printf("total_head cannot smaller then new_head_idx %s:%d\n", __FILE__, __LINE__);
			abort();
		}
		if(i==0){
			last_ppa=ppa_list[i];
			new_head_array[new_head_idx++]=last_ppa;
			bitmap_set(sc->map, i);
		}
		else{
			sequential_flag=false;
			if((last_ppa!=UINT32_MAX) && last_ppa+1==ppa_list[i]){
				sequential_flag=true;
			}

			if(sequential_flag){
				bitmap_unset(sc->map, i);
				last_ppa++;
			}
			else{
				bitmap_set(sc->map, i);
				last_ppa=ppa_list[i];
				new_head_array[new_head_idx++]=last_ppa;
			}
		}
		
	}
	if(new_head_idx<total_head){
		sftl_print_mapping(sc);
		printf("etr->idx:%u making error:%u\n",etr->idx, etr->physical_address);
		abort();
	}
	sc->head_array=new_head_array;
	return sc;
}

uint32_t sftl_insert_entry_from_translation(struct my_cache *, GTD_entry *etr, uint32_t /*lba*/, char *data, uint32_t *eviction_hint, uint32_t org_eviction_hint){
	if(etr->private_data){
		printf("already lru node exists! %s:%d\n", __FILE__, __LINE__);
		abort();
	}
/*
	bool test=false;
	if(etr->idx==711){
		static uint32_t cnt=0;
		if(cnt++==106){
			printf("break! %d\n",cnt++);
		}
	
		test=true;
	}*/
	sftl_cache *sc=make_sc_from_translation(etr, data);
	/*
	if(test){
		sftl_print_mapping(sc);
	}*/

	etr->private_data=(void *)lru_push(scm.lru, (void*)sc);
	etr->status=CLEAN;
	scm.now_caching_byte+=scm.gtd_size[etr->idx];

	uint32_t target_size=scm.gtd_size[etr->idx];
	(*eviction_hint)-=org_eviction_hint;
/*
	if(target_size+sizeof(uint32_t)*2!=org_eviction_hint){
		printf("changed_size\n");
		abort();
	}
*/
	sftl_size_checker(*eviction_hint);
	return 1;
}

uint32_t sftl_update_from_translation_gc(struct my_cache *, char *data, uint32_t lba, uint32_t ppa){
	uint32_t *ppa_list=(uint32_t*)data;
	uint32_t old_ppa=ppa_list[GETOFFSET(lba)];
	ppa_list[GETOFFSET(lba)]=ppa;
	return old_ppa;
}

void sftl_update_dynamic_size(struct my_cache *, uint32_t lba, char *data){
	uint32_t total_head=0;
	uint32_t last_ppa=0;
	bool sequential_flag=false;
	uint32_t *ppa_list=(uint32_t*)data;
	for(uint32_t i=0; i<PAGESIZE/sizeof(uint32_t); i++){
		if(i==0){
			last_ppa=ppa_list[i];
			total_head++;
		}
		else{
			sequential_flag=false;
			if(last_ppa+1==ppa_list[i]){
				sequential_flag=true;
			}

			if(sequential_flag){
				last_ppa++;
			}
			else{
				last_ppa=ppa_list[i];
				total_head++;
			}
		}
	}
	if(total_head<1 || total_head>PAGESIZE/sizeof(uint32_t)){
		printf("total_head over or small %s:%d\n", __FILE__, __LINE__);
		abort();
	}
	scm.gtd_size[GETGTDIDX(lba)]=(total_head*sizeof(uint32_t)+BITMAPSIZE);
	//sftl_mapping_verify(sc);
}

uint32_t sftl_get_mapping(struct my_cache *, uint32_t lba){
	uint32_t gtd_idx=GETGTDIDX(lba);
	GTD_entry *etr=&dmm.GTD[gtd_idx];
	if(!etr->private_data){
		printf("insert data before pick mapping! %s:%d\n", __FILE__, __LINE__);
		abort();
	}

	return get_ppa_from_sc((sftl_cache*)((lru_node*)etr->private_data)->data, lba);
}

struct GTD_entry *sftl_get_eviction_GTD_entry(struct my_cache *, uint32_t lba){
	lru_node *target;
	GTD_entry *etr=NULL;
	for_each_lru_backword(scm.lru, target){
		sftl_cache *sc=(sftl_cache*)target->data;
		etr=sc->etr;
		/*
		if(sc->etr->idx==535){
			printf("start %u eviction \n", sc->etr->physical_address);
			sftl_print_mapping(sc);
			printf("end: %u eviction \n", sc->etr->physical_address);
		}*/
		if(etr->status==FLYING || etr->status==EVICTING){
			continue;
		}
		if(etr->status==CLEAN){
			etr->private_data=NULL;
			sc=(sftl_cache*)target->data;
			free(sc->head_array);
			bitmap_free(sc->map);
			lru_delete(scm.lru, target);
			scm.now_caching_byte-=scm.gtd_size[etr->idx];
			return NULL;
		}

		if(etr->status!=DIRTY){
			printf("can't be status %s:%d\n", __FILE__, __LINE__);
			abort();
		}
		return etr;
	}
	return NULL;
}


bool sftl_update_eviction_target_translation(struct my_cache* ,uint32_t,  GTD_entry *etr,mapping_entry *map, char *data, void *){
	sftl_cache *sc=(sftl_cache*)((lru_node*)etr->private_data)->data;

	bool target;
	uint32_t max=PAGESIZE/sizeof(uint32_t);
	uint32_t last_ppa=0;
	uint32_t head_idx=0;
	uint32_t *ppa_array=(uint32_t*)data;
	uint32_t ppa_array_idx=0;
	uint32_t offset=0;
	uint32_t total_head=0;
	/*
	if(etr->idx==535){
		total_head=(scm.gtd_size[etr->idx]-BITMAPSIZE)/sizeof(uint32_t);
		sftl_mapping_verify(sc);
		printf("eviction start!\n");
		sftl_print_mapping(sc);
		printf("print done!\n");
	}*/

	for_each_bitmap_forward(sc->map, offset, target, max){	
		if(target){
			last_ppa=sc->head_array[head_idx++];
			ppa_array[ppa_array_idx++]=last_ppa;	
		}
		else{
			ppa_array[ppa_array_idx++]=++last_ppa;
		}
	}
/*
	if(etr->idx==535){
		sftl_cache *temp=make_sc_from_translation(etr, data);
		printf("eviction end!\n");
		free(temp->head_array);
		free(temp);
	}
*/
	free(sc->head_array);
	bitmap_free(sc->map);
	lru_delete(scm.lru, (lru_node*)etr->private_data);
	etr->private_data=NULL;
	scm.now_caching_byte-=scm.gtd_size[etr->idx];
	return true;
}

bool sftl_exist(struct my_cache *, uint32_t lba){
	return dmm.GTD[GETGTDIDX(lba)].private_data!=NULL;
}

bool sftl_is_hit_eviction(struct my_cache *, GTD_entry *etr, uint32_t lba, uint32_t ppa, uint32_t total_hit_eviction){
	if(!etr->private_data) return false;
	sftl_cache *sc=GETSCFROMETR(etr);
	if(is_sequential(sc, lba, ppa)) return false;

	if(scm.now_caching_byte+total_hit_eviction+sizeof(uint32_t)*2 > scm.max_caching_byte){
		return true;
	}
	return false;
}

void sftl_force_put_mru(struct my_cache *, GTD_entry *etr,mapping_entry *map, uint32_t lba){
	lru_update(scm.lru, (lru_node*)etr->private_data);
}

bool sftl_is_eviction_hint_full(struct my_cache *, uint32_t eviction_hint){
	return scm.max_caching_byte <= eviction_hint;
}

uint32_t sftl_update_hit_eviction_hint(struct my_cache *, uint32_t lba, uint32_t *prefetching_info, uint32_t eviction_hint, 
		uint32_t *now_eviction_hint, bool increase){
	if(increase){
		*now_eviction_hint=sizeof(uint32_t)*2;
		return eviction_hint+*now_eviction_hint;
	}else{
		return eviction_hint-*now_eviction_hint;
	}
}

int32_t sftl_get_remain_space(struct my_cache *, uint32_t total_eviction_hint){
	return scm.max_caching_byte-scm.now_caching_byte-total_eviction_hint;
}
