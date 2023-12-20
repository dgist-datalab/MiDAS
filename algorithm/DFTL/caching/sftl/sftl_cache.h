#ifndef __SFTL_CACHE_HEADER__
#define __SFTL_CACHE_HEADER__
#include "../../my_cache.h"
#include "../../demand_mapping.h"
#include "../../../../include/data_struct/lru_list.h"
#include "../../../../include/data_struct/bitmap.h"

#define BITMAPFROMLN(ln) (((sftl_cache*)((ln)->data))->map)
#define HEADARRFROMLN(ln) (((sftl_cache*)((ln)->data))->head_array)
#define GETSCFROMETR(etr) ((sftl_cache*)((lru_node*)etr->private_data)->data)
#define BITMAPMEMBER (PAGESIZE/sizeof(DMF))
#define BITMAPSIZE (BITMAPMEMBER/8+(BITMAPMEMBER%8?1:0))

#define GETOFFSET(lba) TRANSOFFSET(lba)
#define ISLASTOFFSET(lba) ((GETOFFSET(lba))==PAGESIZE/sizeof(uint32_t))

typedef struct sftl_cache{
	uint32_t *head_array;
	bitmap *map;
	GTD_entry *etr;
}sftl_cache;

typedef struct sftl_cache_monitor{
	uint32_t max_caching_byte;
	uint32_t now_caching_byte;
	uint32_t *gtd_size;
	LRU *lru;
}sftl_cache_monitor;

uint32_t sftl_init(struct my_cache *, uint32_t total_caching_physical_pages);
uint32_t sftl_free(struct my_cache *);
uint32_t sftl_is_needed_eviction(struct my_cache *, uint32_t lba, uint32_t *, uint32_t eviction_hint);
uint32_t sftl_update_eviction_hint(struct my_cache *, uint32_t lba, uint32_t *prefetching_info, uint32_t eviction_hint, 
		uint32_t *now_eviction_hint, bool increase);

uint32_t sftl_update_hit_eviction_hint(struct my_cache *, uint32_t lba, uint32_t *prefetching_info, uint32_t eviction_hint, 
		uint32_t *now_eviction_hint, bool increase);
uint32_t sftl_update_entry(struct my_cache *, GTD_entry *, uint32_t lba, uint32_t ppa, uint32_t *eviction_hint);
uint32_t sftl_update_entry_gc(struct my_cache *, GTD_entry *, uint32_t lba, uint32_t ppa);
uint32_t sftl_insert_entry_from_translation(struct my_cache *, GTD_entry *, uint32_t lba, char *data, uint32_t *eviction_hint, uint32_t org_eviction_hint);
uint32_t sftl_update_from_translation_gc(struct my_cache *, char *data, uint32_t lba, uint32_t ppa);
uint32_t sftl_get_mapping(struct my_cache *, uint32_t lba);
struct GTD_entry *sftl_get_eviction_GTD_entry(struct my_cache *, uint32_t lba);//if return value is NULL, it is clean eviction.
bool sftl_update_eviction_target_translation(struct my_cache* , uint32_t, GTD_entry *etr, mapping_entry * map, char *data, void *);
bool sftl_exist(struct my_cache *, uint32_t lba);
void sftl_update_dynamic_size(struct my_cache *, uint32_t lba,char *data);

void sftl_mapping_verify(sftl_cache* sc);
void sftl_print_mapping(sftl_cache* sc);
uint32_t sftl_print_mapping_target(sftl_cache *sc, uint32_t lba);

bool sftl_is_hit_eviction(struct my_cache *, GTD_entry *,uint32_t lba, uint32_t ppa, uint32_t total_hit_eviction);
void sftl_force_put_mru(struct my_cache *, GTD_entry *, mapping_entry *, uint32_t);
bool sftl_is_eviction_hint_full(struct my_cache *, uint32_t eviction_hint);
int32_t sftl_get_remain_space(struct my_cache *, uint32_t total_eviction_hint);
#endif
