#ifndef __DEMAND_IO_H__
#define __DEMAND_IO_H__
#include "demand_mapping.h"
#include "../../include/container.h"

algo_req* demand_mapping_read(uint32_t ppa, lower_info *li, request *req, void *params);
void demand_mapping_write(uint32_t ppa, lower_info *li, request *req, void *params);

void demand_mapping_fine_type_evict_write(uint32_t ppa, lower_info *li, request *req);
//for gc
void demand_mapping_inter_read(uint32_t ppa, lower_info *li, struct gc_map_value *);
void demand_mapping_inter_write(uint32_t ppa, lower_info *li, struct gc_map_value *);
#endif
