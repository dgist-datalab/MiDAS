#ifndef __MY_CACHE_H__
#define __MY_CACHE_H__
#include <stdint.h>

typedef enum {
	COARSE, FINE
} CACHE_TYPE;

typedef enum{
	STATIC, DYNAMIC, 
}ENTRY_SIZE_TYPE;

typedef struct my_cache{
	uint32_t (*init)(struct my_cache *, uint32_t total_caching_physical_pages);
	uint32_t (*free)(struct my_cache *);
	uint32_t (*is_needed_eviction)(struct my_cache *, uint32_t lba, uint32_t* prefetching_num, uint32_t eviction_hint);
	uint32_t (*need_more_eviction)(struct my_cache *, uint32_t lba, uint32_t *prefetching_num, uint32_t eviction_hint);//it is valid if type is VARIABLE
	uint32_t (*update_eviction_hint)(struct my_cache *, uint32_t lba, uint32_t *prefetching_num,
			uint32_t total_eviction_hint, uint32_t* now_eviction_hint, bool increase);
	bool (*is_hit_eviction)(struct my_cache *, struct GTD_entry*, uint32_t lba, uint32_t ppa, uint32_t eviction_hint);
	uint32_t (*update_hit_eviction_hint)(struct my_cache *, uint32_t lba, uint32_t *prefetching_num,
			uint32_t total_eviction_hint, uint32_t* now_eviction_hint, bool increase);
	bool (*is_eviction_hint_full)(struct my_cache *, uint32_t eviction_hint);
	int32_t (*get_remain_space)(struct my_cache *, uint32_t total_eviction_hint);
	uint32_t (*update_entry)(struct my_cache *, struct GTD_entry *, uint32_t lba, 
			uint32_t ppa, uint32_t *eviction_hint);
	uint32_t (*update_entry_gc)(struct my_cache *, struct GTD_entry *, uint32_t lba, uint32_t ppa);
	void (*force_put_mru)(struct my_cache *, struct GTD_entry *, struct mapping_entry *,uint32_t lba);
	uint32_t (*insert_entry_from_translation)(struct my_cache *, GTD_entry *, uint32_t lba, char *data, uint32_t *eviction_hint, uint32_t org_eviction_hint);
	uint32_t (*update_from_translation_gc)(struct my_cache *, char *data, uint32_t lba, uint32_t ppa);
	uint32_t (*get_mapping)(struct my_cache *, uint32_t lba);
	struct GTD_entry *(*get_eviction_GTD_entry)(struct my_cache *, uint32_t lba);//if return value is NULL, it is clean eviction.
	struct mapping_entry *(*get_eviction_mapping_entry)(struct my_cache *, uint32_t lba, uint32_t now_eviction_hint, void **tp_data);//if return value is NULL, it is clean eviction.
	bool (*update_eviction_target_translation)(struct my_cache* , uint32_t lba, GTD_entry *etr, mapping_entry *map, char *data, void *additional_data);
	bool (*evict_target)(struct my_cache *,GTD_entry *, mapping_entry *);
	void (*update_dynamic_size)(struct my_cache *, uint32_t lba, char *data);
	bool (*exist)(struct my_cache *, uint32_t lba);
	CACHE_TYPE type;
	ENTRY_SIZE_TYPE entry_type;
	void *private_data;
}my_cache;

#endif
