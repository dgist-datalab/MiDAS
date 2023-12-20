#include "../../my_cache.h"
#include "../../../../include/data_struct/lru_list.h"
#include "../../../../include/dftl_settings.h"
#include "../../../../include/data_struct/bitmap.h"
#include <map>

#define GETOFFSET(lba) TRANSOFFSET(lba)
#define GETTPLRU(etr) ((LRU*)((etr)->private_data))
#define GETLBA(tn, tc) ((tn->idx*(PAGESIZE/sizeof(uint32_t)))+tc->offset)
#define MAXOFFSET ((PAGESIZE/sizeof(uint32_t))-1)
#define PREFETCHINGTH -3

typedef struct tp_entry{
	uint16_t offset;
	uint32_t ppa;
	bool dirty_bit;
	struct lru_node *lru_node;
}tp_cache_node; 

typedef struct tp_node{
	struct lru_node *lru_node;
	LRU *tp_lru;
	uint32_t idx; //transaction page number
}tp_node;

typedef struct tp_cache_monitor{
	uint32_t max_caching_byte;
	uint32_t now_caching_byte;
	bitmap *populated_cache_entry;
	char *GTD_internal_state;
	int8_t tp_node_change_cnt;
	//bool prefetching_mode;
	LRU *lru;
}tp_cache_monitor;

typedef struct tp_evicting_entry_set{
	uint32_t evicting_num;
	mapping_entry *map;
}tp_evicting_entry_set;

uint32_t tp_init(struct my_cache *, uint32_t total_caching_physical_pages);
uint32_t tp_free(struct my_cache *);
uint32_t tp_is_needed_eviction(struct my_cache *, uint32_t , uint32_t *, uint32_t eviction_hint);
uint32_t tp_update_eviction_hint(struct my_cache *, uint32_t lba, uint32_t *prefetching_info, uint32_t eviction_hint, uint32_t *now_eviciton_hint,bool increase);
uint32_t tp_update_hit_eviction_hint(struct my_cache *, uint32_t lba, uint32_t *prefetching_info, uint32_t eviction_hint, uint32_t *now_eviciton_hint,bool increase);
uint32_t tp_update_entry(struct my_cache *, GTD_entry *, uint32_t lba, uint32_t ppa, uint32_t *eviction_hint);
uint32_t tp_update_entry_gc(struct my_cache *, GTD_entry *, uint32_t lba, uint32_t ppa);
uint32_t tp_insert_entry_from_translation(struct my_cache *, GTD_entry *, uint32_t lba, char *data, uint32_t *eviction_hint, uint32_t now_eviction_hint);
uint32_t tp_update_from_translation_gc(struct my_cache *, char *data, uint32_t lba, uint32_t ppa);
uint32_t tp_get_mapping(struct my_cache *, uint32_t lba);
mapping_entry *tp_get_eviction_entry(struct my_cache *, uint32_t lba, uint32_t now_eviction_hint, void **);
bool tp_update_eviction_target_translation(struct my_cache* , uint32_t lba, GTD_entry *etr, mapping_entry *map, char *data, void *);
bool tp_evict_target(struct my_cache *, GTD_entry *, mapping_entry *etr);
bool tp_exist(struct my_cache *, uint32_t lba);
void tp_force_put_mru(struct my_cache *, GTD_entry *, mapping_entry *,uint32_t);
bool tp_is_hit_eviction(struct my_cache *, GTD_entry *etr, uint32_t lba, uint32_t ppa, uint32_t total_hit_eviction);
void tp_update_dynamic_size(struct my_cache*, uint32_t lba, char*data);
bool tp_is_eviction_hint_full(struct my_cache*, uint32_t eviction_hint);
int32_t tp_get_remain_space(struct my_cache *,uint32_t total_eviction_hint);
