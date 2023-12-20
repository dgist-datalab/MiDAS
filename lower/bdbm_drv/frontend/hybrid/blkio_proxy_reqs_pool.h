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

#ifndef _BDBM_PROXY_REQS_POOK_H
#define _BDBM_PROXY_REQS_POOK_H

#include "blkio_proxy_ioctl.h"

typedef struct {
	bdbm_spinlock_t lock;
	bdbm_blkio_proxy_req_t* mmap_reqs;
	struct list_head used_list;
	struct list_head free_list;
	int64_t nr_reqs;
} bdbm_proxy_reqs_pool_t;

bdbm_proxy_reqs_pool_t* bdbm_proxy_reqs_pool_create (int64_t nr_reqs, bdbm_blkio_proxy_req_t* reqs);
void bdbm_proxy_reqs_pool_destroy (bdbm_proxy_reqs_pool_t* pool);
bdbm_blkio_proxy_req_t* bdbm_proxy_reqs_pool_alloc_item (bdbm_proxy_reqs_pool_t* pool);
void bdbm_proxy_reqs_pool_free_item (bdbm_proxy_reqs_pool_t* pool, bdbm_blkio_proxy_req_t* req);

#endif

