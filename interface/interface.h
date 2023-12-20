#ifndef __H_INTERFACE_H
#define __H_INTERFACE_H
#include "../include/settings.h"
#include "../include/container.h"
#include "threading.h"

void inf_init(int apps_flag,int test_num, int argc, char **argv, bool checkingdata);
#ifdef USINGAPP
bool inf_make_req(const FSTYPE,const KEYT, char *);
#else
bool inf_make_req(const FSTYPE,const KEYT, char *,int len,int mark);
#endif

#ifdef KVSSD
bool inf_make_multi_set(const FSTYPE, KEYT *keys, char **values, int *lengths, int req_num, int makr);
//bool inf_make_range_query(const FSTYPE, KEYT start, char **values,)
bool inf_make_req_special(const FSTYPE type, const KEYT key, char* value, int len,uint32_t seq, void*(*special)(void*));
bool inf_make_req_fromApp(char type, KEYT key,uint32_t offset,uint32_t len,PTR value,void *req, void*(*end_func)(void*));

bool inf_iter_create(KEYT start,bool (*added_end)(struct request *const));
bool inf_iter_next(uint32_t iter_id, char **values,bool (*added_end)(struct request *const),bool withvalue);
bool inf_iter_release(uint32_t iter_id, bool (*added_end)(struct request *const));

bool inf_make_multi_req(char type, KEYT key,KEYT *keys,uint32_t iter_id,char **values,uint32_t lengths,bool (*added_end)(struct request *const));
bool inf_make_req_apps(char type, char *keys, uint8_t key_len,char *value,int value_len,int seq,void *req,void (*end_req)(uint32_t,uint32_t, void*));

bool inf_make_range_query_apps(char type, char *keys, uint8_t key_len,int seq, int length,void *req,void (*end_req)(uint32_t,uint32_t, void*));

bool inf_make_mreq_apps(char type, char **keys, uint8_t *key_len, char **values,int num, int seq, void *req,void (*end_req)(uint32_t,uint32_t, void*));
bool inf_iter_req_apps(char type, char *prefix, uint8_t key_len,char **value, int seq,void *req, void (*end_req)(uint32_t,uint32_t, void *));

bool inf_wait_background();
#endif

uint32_t inf_algorithm_caller(request *const);
bool inf_end_req(request*const);
bool inf_assign_try(request *req);
void inf_free();
void inf_print_debug();
void* qmanager_find_by_algo(KEYT key);
value_set *inf_get_valueset(PTR,int,uint32_t length);//NULL is uninitial, non-NULL is memcpy
void inf_free_valueset(value_set*, int);
void inf_lower_log_print();

#endif

