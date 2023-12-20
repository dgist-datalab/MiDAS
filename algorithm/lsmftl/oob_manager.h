#ifndef OOB_MANAGER
#define OOB_MANAGER
#include "../../include/container.h"
#include "page_manager.h"
#include "lsmtree.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct oob_manager{
	uint32_t lba[L2PGAP];
	uint32_t version[L2PGAP];
}oob_manager;

oob_manager *get_oob_manager(uint32_t ppa);
uint32_t get_version_from_piece_ppa(uint32_t piece_ppa);
static inline uint32_t get_lba_from_oob_manager(oob_manager *oob, uint32_t idx){
	if(idx>=L2PGAP){
		abort();
	}
	return oob->lba[idx];
}

static inline uint32_t get_version_from_oob_manager(oob_manager *oob, uint32_t idx){
	if(idx>=L2PGAP){
		abort();
	}
	return oob->version[idx];
}

#endif
