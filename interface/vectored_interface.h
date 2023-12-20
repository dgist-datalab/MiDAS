#ifndef VECTORED_INTERFACE
#define VECTORED_INTERFACE
#include "interface.h"
#include "../include/settings.h"

uint32_t inf_vector_make_req(char *buf, void * (*end_req)(void*), uint32_t mark);
void *vectored_main(void *);
void inf_algorithm_testing();
void assign_vectored_req(vec_request *txn);
void release_each_req(request *req);
#endif
