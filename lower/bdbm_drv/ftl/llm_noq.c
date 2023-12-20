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

#if defined (KERNEL_MODE)
#include <linux/module.h>
#include <linux/slab.h>

#elif defined (USER_MODE)
#include <stdio.h>
#include <stdint.h>

#else
#error Invalid Platform (KERNEL_MODE or USER_MODE)
#endif

#include "debug.h"
#include "umemory.h"
#include "params.h"
#include "bdbm_drv.h"
#include "llm_noq.h"
#include "pmu.h"


/* llm interface */
bdbm_llm_inf_t _llm_noq_inf = {
	.ptr_private = NULL,
	.create = llm_noq_create,
	.destroy = llm_noq_destroy,
	.make_req = llm_noq_make_req,
	.make_reqs = llm_noq_make_reqs,
	.flush = llm_noq_flush,
	.end_req = llm_noq_end_req,
};

struct bdbm_llm_noq_private {
	uint32_t dummy;
};

uint32_t llm_noq_create (bdbm_drv_info_t* bdi)
{
	struct bdbm_llm_noq_private* p;
	uint64_t loop;

	/* create a private info for llm_nt */
	if ((p = (struct bdbm_llm_noq_private*)bdbm_malloc
			(sizeof (struct bdbm_llm_noq_private))) == NULL) {
		bdbm_error ("bdbm_malloc failed");
		return -1;
	}

	/* setup dummy */
	p->dummy = 0;

	/* keep the private structures for llm_nt */
	bdi->ptr_llm_inf->ptr_private = (void*)p;

	return 0;
}

/* NOTE: we assume that all of the host requests are completely served.
 * the host adapter must be first closed before this function is called.
 * if not, it would work improperly. */
void llm_noq_destroy (bdbm_drv_info_t* bdi)
{
	struct bdbm_llm_noq_private* p;
	uint64_t loop;

	p = (struct bdbm_llm_noq_private*)BDBM_LLM_PRIV(bdi);

	bdbm_free (p);
}

uint32_t llm_noq_make_req (bdbm_drv_info_t* bdi, bdbm_llm_req_t* llm_req)
{
	uint32_t ret;
	static uint64_t cnt = 0;

	/* just for display */
	if (cnt % 50000 == 0) bdbm_msg ("llm_noq_make_req: %llu", cnt);
	cnt++;

	/* update pmu */
	pmu_update_sw (bdi, llm_req);
	pmu_update_q (bdi, llm_req);

	/* send a request to a device manager */
	if ((ret = bdi->ptr_dm_inf->make_req (bdi, llm_req)) != 0) {
		/* handle error cases */
		bdbm_error ("llm_make_req failed");
	}

	return ret;
}

uint32_t llm_noq_make_reqs (bdbm_drv_info_t* bdi, bdbm_hlm_req_t* hr)
{
	uint32_t ret;

	/* send a request to a device manager */
	if ((ret = bdi->ptr_dm_inf->make_reqs (bdi, hr)) != 0) {
		/* handle error cases */
		bdbm_error ("llm_noq_make_reqs failed");
	}

	return ret;
}

void llm_noq_flush (bdbm_drv_info_t* bdi)
{
	//struct bdbm_llm_noq_private* p = (struct bdbm_llm_noq_private*)BDBM_LLM_PRIV(bdi);
}

void llm_noq_end_req (bdbm_drv_info_t* bdi, bdbm_llm_req_t* llm_req)
{
	/* update pmu */
	pmu_update_tot (bdi, llm_req);
	pmu_inc (bdi, llm_req);

	/* finish a request */
	bdi->ptr_hlm_inf->end_req (bdi, llm_req);
}

