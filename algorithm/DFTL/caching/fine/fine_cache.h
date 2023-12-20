#include "../../my_cache.h"
#include "../../../../include/data_struct/lru_list.h"
#include "../../../../include/dftl_settings.h"
#include "../../../../include/data_struct/bitmap.h"

#define GETOFFSET(lba) TRANSOFFSET(lba)
#define get_fcn(a) ((fine_cache_node*)((a)->private_data))
#define get_ln(a) ((fine_cache_node*)((a)->private_data))->lru_node
#define get_flag(a) ((fine_cache_node*)((a)->private_data))->dirty_bit
#define set_flag(a, b) ((fine_cache_node*)((a)->private_data))->dirty_bit=(b)
#define init_fcn(a) ((a)->private_data)=malloc(sizeof(fine_cache_node))

#define FINECACHEENT_SZ (sizeof(uint32_t)+sizeof(uint32_t))

typedef mapping_entry fine_cache;

#ifdef SEARCHSPEEDUP
typedef struct cache_node_lru_mapping{
	void **fc_array;
}cache_node_lru_mapping;
#endif

typedef struct fine_cache_node{
	struct lru_node *lru_node;
	char dirty_bit;
}fine_cache_node;

typedef struct fine_cache_monitor{
	uint32_t max_caching_map;
	uint32_t now_caching_map;
	bitmap *populated_cache_entry;
	LRU *lru;
	char *GTD_internal_state;
#ifdef SEARCHSPEEDUP
	cache_node_lru_mapping *cl_mapping;
#endif
}fine_cache_monitor;

uint32_t fine_init(struct my_cache *, uint32_t total_caching_physical_pages);
uint32_t fine_free(struct my_cache *);
uint32_t fine_is_needed_eviction(struct my_cache *, uint32_t , uint32_t *, uint32_t eviction_hint);
uint32_t fine_update_eviction_hint(struct my_cache *, uint32_t lba, uint32_t *prefetching_info, 
		uint32_t evicition_hint, uint32_t *now_eviction_hint, bool increase);
uint32_t fine_update_entry(struct my_cache *, GTD_entry *, uint32_t lba, uint32_t ppa, uint32_t *eviction_hint);
uint32_t fine_update_entry_gc(struct my_cache *, GTD_entry *, uint32_t lba, uint32_t ppa);
uint32_t fine_insert_entry_from_translation(struct my_cache *, GTD_entry *, uint32_t lba, char *data, uint32_t *eviction_hint, uint32_t);
uint32_t fine_update_from_translation_gc(struct my_cache *, char *data, uint32_t lba, uint32_t ppa);
uint32_t fine_get_mapping(struct my_cache *, uint32_t lba);
mapping_entry *fine_get_eviction_entry(struct my_cache *, uint32_t lba, uint32_t now_eviction_hint, void **);
bool fine_update_eviction_target_translation(struct my_cache* ,uint32_t, GTD_entry *etr, mapping_entry *map, char *data, void *);
bool fine_evict_target(struct my_cache *, GTD_entry *, mapping_entry *etr);
bool fine_exist(struct my_cache *, uint32_t lba);
void fine_force_put_mru(struct my_cache *, GTD_entry *, mapping_entry *, uint32_t lba);
bool fine_is_eviction_hint_full(struct my_cache *, uint32_t eviction_hint);
int32_t fine_get_remain_space(struct my_cache *, uint32_t total_eviction_hint);
