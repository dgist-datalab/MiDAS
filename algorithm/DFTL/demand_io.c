#include "demand_io.h"
#include "../../interface/interface.h"
#include <stdlib.h>

extern demand_map_manager dmm;

void *demand_end_req(algo_req * my_req){
	request *parents=my_req->parents;
	demand_param *dp;
	std::map<uint32_t, bool>::iterator iter;
	switch(my_req->type){
		case MAPPINGR:
			fdriver_lock(&dmm.flying_map_read_lock);
			dp=(demand_param *)my_req->param;
			iter=dmm.flying_map_read_flag_set->find(GETGTDIDX(dp->flying_map_read_key));
			if(iter==dmm.flying_map_read_flag_set->end()){
				printf("%u issued key is not inserted into flying set\n", iter->first);
				abort();
			}
			else{
	//			printf("iter->first:%u\n", iter->first);
				iter->second=true;
			}
			fdriver_unlock(&dmm.flying_map_read_lock);
		case MAPPINGW:
			if(!inf_assign_try(parents)){
				abort();
			}
			break;
		default:
			printf("unknown type :%d (%s:%d)\n",my_req->type, __FILE__, __LINE__);
			abort();
			break;
	}
	free(my_req);
	return NULL;
}

void *demand_inter_end_req(algo_req *my_req){
	gc_map_value *gmv=(gc_map_value*)my_req->param;
	switch(my_req->type){
		case GCMR_DGC:
			gmv->isdone=true;
			break;
		case GCMW_DGC:
			inf_free_valueset(gmv->value, FS_MALLOC_R);
			free(gmv);
			break;
	}
	free(my_req);
	return NULL;
}

algo_req* demand_mapping_read(uint32_t ppa, lower_info *li, request *req, void *param){
	algo_req *my_req=(algo_req*)malloc(sizeof(algo_req));
	my_req->type=MAPPINGR;
	my_req->parents=req;
	my_req->type_lower=0;
	my_req->end_req=demand_end_req;
	my_req->param=param;
	my_req->ppa=ppa;
	li->read(ppa, PAGESIZE, req->value, ASYNC, my_req);
	return my_req;
}

void demand_mapping_write(uint32_t ppa, lower_info *li, request *req, void *param){
	algo_req *my_req=(algo_req*)malloc(sizeof(algo_req));
	my_req->type=MAPPINGW;
	my_req->parents=req;
	my_req->type_lower=0;
	my_req->end_req=demand_end_req;
	my_req->param=param;
	my_req->ppa=ppa;
	li->write(ppa, PAGESIZE, req->value, ASYNC, my_req);
}

void demand_mapping_inter_read(uint32_t ppa, lower_info *li, gc_map_value *param){
	algo_req *my_req=(algo_req*)malloc(sizeof(algo_req));
	my_req->type=GCMR_DGC;
	my_req->parents=NULL;
	my_req->type_lower=0;
	my_req->end_req=demand_inter_end_req;
	my_req->param=(void *)param;
	my_req->ppa=ppa;

	li->read(ppa, PAGESIZE, param->value, ASYNC, my_req);
}

void demand_mapping_inter_write(uint32_t ppa, lower_info *li, gc_map_value *param){
	algo_req *my_req=(algo_req*)malloc(sizeof(algo_req));
	my_req->type=GCMW_DGC;
	my_req->parents=NULL;
	my_req->type_lower=0;
	my_req->end_req=demand_inter_end_req;
	my_req->param=(void *)param;
	my_req->ppa=ppa;

	li->write(ppa, PAGESIZE, param->value, ASYNC, my_req);
}
