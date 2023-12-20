#include <stdio.h>
#include "../../include/container.h"
#include "../../include/types.h"
#include "../../include/utils.h"
#include "frontend/libmemio/libmemio.h"
#include "../../include/FS.h"
#include "../../include/settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
memio_t *mio;
#define TESTSET 1000000
#define A_SYNC 1
typedef struct temp_params{
	int type;
	value_set *value;
}tp;


value_set *inf_get_valueset(PTR in_v, int type, uint32_t length){
	value_set *res=(value_set*)malloc(sizeof(value_set));
	//check dma alloc type
	if(length==8192)
		res->dmatag=F_malloc((void**)&(res->value),8192,type);
	else{
		res->dmatag=-1;
		res->value=(PTR)malloc(length);
	}
	res->length=length;

	if(in_v){
		memcpy(res->value,in_v,length);
	}
	else
		memset(res->value,0,length);
	return res;
}

void inf_free_valueset(value_set *in, int type){
	if(in->dmatag==-1){
		free(in->value);
	}
	else{
		F_free((void*)in->value,in->dmatag,type);
	}
	free(in);
}

void *t_end_req(struct algo_req *const t){
	tp *params=(tp*)t->params;
	inf_free_valueset(params->value,params->type);
	free(params);
	free(t);
	return NULL;
}

MeasureTime tt;
int main(){
	measure_init(&tt);
	mio=memio_open();
	MS(&tt);
	for(int i=0; i<TESTSET; i++){
		algo_req *req=(algo_req*)malloc(sizeof(algo_req));
		tp *params=(tp*)malloc(sizeof(tp));
		params->type=FS_MALLOC_W;
		params->value=inf_get_valueset(NULL,FS_MALLOC_W,8192);
		req->params=params;
		req->end_req=t_end_req;
		memio_write(mio,i,8192,(uint8_t *)params->value->value,A_SYNC,req,params->value->dmatag);
	}
	MC(&tt);
	printf("test:%lu\n",tt.micro_time);
}
