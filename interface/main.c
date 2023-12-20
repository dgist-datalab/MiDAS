#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "../include/FS.h"
#include "../include/settings.h"
#include "../include/types.h"
#include "../bench/bench.h"
#include "interface.h"
#include "../include/utils/kvssd.h"

int main(int argc,char* argv[]){
	/*
	   to use the custom benchmark setting the first parameter of 'inf_init' set false
	   if not, set the parameter as true.
	   the second parameter is not used in anycase.
	 */
	inf_init(0,0,0,NULL);
	
	/*initialize the cutom bench mark*/
	bench_init();

	/*adding benchmark type for testing*/
	bench_add(RANDRW,0,RANGE,RANGE*2);

	bench_value *value;
	while((value=get_bench())){
		if(value->type==FS_SET_T){
			inf_make_req(value->type,value->key,NULL,value->length,value->mark);
		}
		else if(value->type==FS_GET_T){
			inf_make_req(value->type,value->key,NULL,value->length,value->mark);
		}
	}

	inf_free();
	return 0;
}
