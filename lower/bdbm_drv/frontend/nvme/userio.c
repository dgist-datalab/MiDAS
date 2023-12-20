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

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "bdbm_drv.h"
#include "debug.h"
#include "umemory.h"
#include "userio.h"
#include "params.h"

#include "utime.h"
#include "uthread.h"
#include "hlm_reqs_pool.h"

bdbm_host_inf_t _userio_inf = {
	.ptr_private = NULL,
	.open = userio_open,
	.close = userio_close,
	.make_req = userio_make_req,
	.end_req = userio_end_req,
};

typedef struct {
	atomic_t nr_host_reqs;
	bdbm_sema_t host_lock;
	bdbm_hlm_reqs_pool_t* hlm_reqs_pool;
} bdbm_userio_private_t;


uint32_t userio_open (bdbm_drv_info_t* bdi)
{
	uint32_t ret;
	bdbm_userio_private_t* p;
	int mapping_unit_size;

	/* create a private data structure */
	if ((p = (bdbm_userio_private_t*)bdbm_malloc_atomic
			(sizeof (bdbm_userio_private_t))) == NULL) {
		bdbm_error ("bdbm_malloc_atomic failed");
		return 1;
	}
	atomic_set (&p->nr_host_reqs, 0);
	bdbm_sema_init (&p->host_lock);

	/* create hlm_reqs pool */
	if (bdi->parm_dev.nr_subpages_per_page == 1)
		mapping_unit_size = bdi->parm_dev.page_main_size;
	else
		mapping_unit_size = KERNEL_PAGE_SIZE;

	if ((p->hlm_reqs_pool = bdbm_hlm_reqs_pool_create (
			mapping_unit_size,	/* mapping unit */
			bdi->parm_dev.page_main_size	/* io unit */	
			)) == NULL) {
		bdbm_warning ("bdbm_hlm_reqs_pool_create () failed");
		return 1;
	}

	bdi->ptr_host_inf->ptr_private = (void*)p;

	return 0;
}

void userio_close (bdbm_drv_info_t* bdi)
{
	bdbm_userio_private_t* p = NULL;

	p = (bdbm_userio_private_t*)BDBM_HOST_PRIV(bdi);

	/* wait for host reqs to finish */
	bdbm_msg ("wait for host reqs to finish");
	for (;;) {
		if (atomic_read (&p->nr_host_reqs) == 0)
			break;
		bdbm_msg ("p->nr_host_reqs = %llu", p->nr_host_reqs);
		bdbm_thread_msleep (1);
	}

	if (p->hlm_reqs_pool) {
		bdbm_hlm_reqs_pool_destroy (p->hlm_reqs_pool);
	}

	bdbm_sema_free (&p->host_lock);

	/* free private */
	bdbm_free_atomic (p);
}

void userio_make_req (bdbm_drv_info_t* bdi, void *bio)
{
	bdbm_userio_private_t* p = (bdbm_userio_private_t*)BDBM_HOST_PRIV(bdi);
	bdbm_blkio_req_t* br = (bdbm_blkio_req_t*)bio;
	bdbm_hlm_req_t* hr = NULL;

	/* get a free hlm_req from the hlm_reqs_pool */
	if ((hr = bdbm_hlm_reqs_pool_get_item (p->hlm_reqs_pool)) == NULL) {
		bdbm_error ("bdbm_hlm_reqs_pool_get_item () failed");
		bdbm_bug_on (1);
		return;
	}

	/* build hlm_req with bio */
	if (bdbm_hlm_reqs_pool_build_req (p->hlm_reqs_pool, hr, br) != 0) {
		bdbm_error ("bdbm_hlm_reqs_pool_build_req () failed");
		bdbm_bug_on (1);
		return;
	}

	/* if success, increase # of host reqs */
	atomic_inc (&p->nr_host_reqs);

	bdbm_sema_lock (&p->host_lock);

	/* NOTE: it would be possible that 'hlm_req' becomes NULL 
	 * if 'bdi->ptr_hlm_inf->make_req' is success. */
	if (bdi->ptr_hlm_inf->make_req (bdi, hr) != 0) {
		/* oops! something wrong */
		bdbm_error ("'bdi->ptr_hlm_inf->make_req' failed");

		/* cancel the request */
		atomic_dec (&p->nr_host_reqs);
		bdbm_hlm_reqs_pool_free_item (p->hlm_reqs_pool, hr);
	}

	bdbm_sema_unlock (&p->host_lock);
}

void userio_end_req (bdbm_drv_info_t* bdi, bdbm_hlm_req_t* req)
{
	bdbm_userio_private_t* p = (bdbm_userio_private_t*)BDBM_HOST_PRIV(bdi);
	bdbm_blkio_req_t* r = (bdbm_blkio_req_t*)req->blkio_req;

	/* destroy hlm_req */
	bdbm_hlm_reqs_pool_free_item (p->hlm_reqs_pool, req);

	/* decreate # of reqs */
	atomic_dec (&p->nr_host_reqs);

	/* call call-back function */
	if (r->cb_done)
		r->cb_done (r);
}

