#include "debug_tools.h"
#include <stdlib.h>
#include <stdio.h>
#include <execinfo.h>
#include <stdint.h>
void print_trace_step(int a){
	void **array=(void**)malloc(sizeof(void*)*a);
	size_t size;
	char **strings;
	size=backtrace(array,a);
	strings=backtrace_symbols(array,size);

	for(uint32_t i=0; i<size; i++){
		printf("\n\t%s\n",strings[i]);
	}
	free(array);
}
