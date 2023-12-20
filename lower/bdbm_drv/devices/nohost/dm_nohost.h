/*
The MIT License (MIT)

Copyright (c) 2014-2015 CSAIL, MIT

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef _BLUEDBM_DEV_RAMDRV_H
#define _BLUEDBM_DEV_RAMDRV_H

#include "bdbm_drv.h"
#include "params.h"
#include <queue> //koo
#include <pthread.h> //koo

extern bdbm_dm_inf_t _dm_nohost_inf;

uint32_t dm_nohost_probe (bdbm_drv_info_t* bdi, bdbm_device_params_t* param);
uint32_t dm_nohost_open (bdbm_drv_info_t* bdi);
void dm_nohost_close (bdbm_drv_info_t* bdi);
uint32_t dm_nohost_make_req (bdbm_drv_info_t* bdi, bdbm_llm_req_t* ptr_llm_req);
uint32_t dm_nohost_make_reqs (bdbm_drv_info_t* bdi, bdbm_hlm_req_t* ptr_hlm_req);
void dm_nohost_end_req (bdbm_drv_info_t* bdi, bdbm_llm_req_t* ptr_llm_req);

uint32_t dm_nohost_load (bdbm_drv_info_t* bdi, const char* fn);
uint32_t dm_nohost_store (bdbm_drv_info_t* bdi, const char* fn);

int dm_do_merge(unsigned int ht_num, unsigned int lt_num, unsigned int *kt_num, unsigned int *inv_num,uint32_t ppa_dma);

int dm_do_hw_find(uint32_t ppa, uint32_t size, bdbm_llm_req_t* r);

uint32_t get_dev_tags();
unsigned int *get_low_ppali();
unsigned int *get_high_ppali();
unsigned int *get_res_ppali();
unsigned int *get_res_ppali2();
unsigned int *get_inv_ppali();
unsigned int *get_merged_kt();
unsigned int *get_findKey_dma();

void init_dmaQ (std::queue<int>*);
int alloc_dmaQ_tag (int);
void free_dmaQ_tag (int, int);

#endif

