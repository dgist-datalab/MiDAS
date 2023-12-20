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

#if defined(KERNEL_MODE)
#include <linux/module.h>
#include <linux/blkdev.h>

#elif defined(USER_MODE)
#include <stdio.h>
#include <stdint.h>

#else
#error Invalid Platform (KERNEL_MODE or USER_MODE)
#endif

#include "debug.h"
#include "params.h"
#include "bdbm_drv.h"
#include "hlm_nobuf.h"
#include "hlm_dftl.h"
#include "uthread.h"

#include "algo/no_ftl.h"
#include "algo/block_ftl.h"
#include "algo/page_ftl.h"
#include "queue/queue.h"


/*#define USE_THREAD*/

/* interface for hlm_dftl */
bdbm_hlm_inf_t _hlm_dftl_inf = {
	.ptr_private = NULL,
	.create = hlm_dftl_create,
	.destroy = hlm_dftl_destroy,
	.make_req = hlm_dftl_make_req,
	.end_req = hlm_dftl_end_req,
};

/* data structures for hlm_dftl */
typedef struct {
	bdbm_ftl_inf_t* ftl;	/* for hlm_nobuff (it must be on top of this structure) */

	/* for thread management */
	bdbm_queue_t* q;
#ifdef USE_THREAD
	bdbm_thread_t* hlm_thread;
	bdbm_sema_t ftl_lock;
#endif
} bdbm_hlm_dftl_private_t;

#ifdef USE_THREAD
/* kernel thread for _llm_q */
int __hlm_dftl_thread (void* arg)
{
	bdbm_drv_info_t* bdi = (bdbm_drv_info_t*)arg;
	bdbm_hlm_dftl_private_t* p = (bdbm_hlm_dftl_private_t*)BDBM_HLM_PRIV(bdi);
	bdbm_hlm_req_t* r = NULL;

	for (;;) {
		/* Go to sleep if there are not requests in Q */
		if (bdbm_queue_is_all_empty (p->q)) {
			bdbm_thread_schedule_setup (p->hlm_thread);
			if (bdbm_queue_is_all_empty (p->q)) {
				if (bdbm_thread_schedule_sleep (p->hlm_thread) == SIGKILL) 
					break;
			} else {
				bdbm_thread_schedule_cancel (p->hlm_thread);
			}
		}

		while (!bdbm_queue_is_empty (p->q, 0)) {
			if ((r = (bdbm_hlm_req_t*)bdbm_queue_dequeue (p->q, 0)) != NULL) {
			} else {
				bdbm_error ("r == NULL");
				bdbm_bug_on (1);
			}
		} 
	}

	return 0;
}
#endif

int __fetch_me_and_make_req (bdbm_drv_info_t* bdi, bdbm_hlm_req_t* r)
{
	bdbm_hlm_dftl_private_t* p = (bdbm_hlm_dftl_private_t*)BDBM_HLM_PRIV(bdi);
	int i = 0, nr_missed_dir = 0;

	/* see if foreground GC is needed or not */
	for (i = 0; i < 10; i++) {
		if ((r->req_type == REQTYPE_WRITE || r->req_type == REQTYPE_READ) &&
			 p->ftl->is_gc_needed != NULL && 
			 p->ftl->is_gc_needed (bdi)) {
			/* perform GC before sending requests */ 
			p->ftl->do_gc (bdi);
		} else
			break;
	}
	if (i > 1) {
		bdbm_msg ("GC invokation: %d", i);
	}

	/* STEP1: read missing mapping entries */
	nr_missed_dir = r->len;
	{
		bdbm_llm_req_t** rr = (bdbm_llm_req_t**)bdbm_malloc (sizeof (bdbm_llm_req_t*) * nr_missed_dir);

		/* FIXME: need to improve to exploit parallelism */
		memset (rr, 0x00, sizeof (bdbm_llm_req_t*) * nr_missed_dir);
		for (i = 0; i < nr_missed_dir; i++) {
			/* check the availability of mapping entries again */
			if (p->ftl->check_mapblk (bdi, r->lpa + i) == 0)
				continue;

			/* fetch mapping entries to DRAM from Flash */
			if ((rr[i] = p->ftl->prepare_mapblk_load (bdi, r->lpa + i)) == NULL)
				continue;

			/* send read requets to llm */
			bdbm_sema_lock (rr[i]->done);
			bdi->ptr_llm_inf->make_req (bdi, rr[i]);
		}
		for (i = 0; i < nr_missed_dir; i++) {
			if (rr[i]) {
				bdbm_sema_lock (rr[i]->done);
				p->ftl->finish_mapblk_load (bdi, rr[i]);
			}
		}
		bdbm_free (rr);
	}

	/* STEP2: send origianl requests to llm */
	if (hlm_nobuf_make_req (bdi, r)) {
		/* if it failed, we directly call 'ptr_host_inf->end_req' */
		bdi->ptr_host_inf->end_req (bdi, r);
		bdbm_warning ("oops! make_req failed");
		/* [CAUTION] r is now NULL */
	}

	/* STEP4: evict mapping entries if there is not enough DRAM space */
	{
		bdbm_llm_req_t** rr = (bdbm_llm_req_t**)bdbm_malloc (sizeof (bdbm_llm_req_t*) * nr_missed_dir);

		memset (rr, 0x00, sizeof (bdbm_llm_req_t*) * nr_missed_dir);
		for (i = 0; i < nr_missed_dir; i++) {
#include "algo/dftl_map.h"
			directory_slot_t* ds;

			/* drop mapping enries to Flash */
			if ((rr[i] = p->ftl->prepare_mapblk_eviction (bdi)) == NULL)
				break;

			/* send a req to llm */
			ds = (directory_slot_t*)rr[i]->ds;
			if (ds->status != DFTL_DIR_CLEAN) {
				bdbm_sema_lock (rr[i]->done);
				bdi->ptr_llm_inf->make_req (bdi, rr[i]);
			}
		}

		for (i = 0; i < nr_missed_dir; i++) {
			if (rr[i]) {
				bdbm_sema_lock (rr[i]->done);
				p->ftl->finish_mapblk_eviction (bdi, rr[i]);
			}
		}

		bdbm_free (rr);
	}

	return 0;
}

/* interface functions for hlm_dftl */
uint32_t hlm_dftl_create (bdbm_drv_info_t* bdi)
{
	bdbm_hlm_dftl_private_t* p;

	/* create private */
	if ((p = (bdbm_hlm_dftl_private_t*)bdbm_malloc_atomic
			(sizeof(bdbm_hlm_dftl_private_t))) == NULL) {
		bdbm_error ("bdbm_malloc_atomic failed");
		return 1;
	}

	/* setup FTL function pointers */
	if ((p->ftl= BDBM_GET_FTL_INF (bdi)) == NULL) {
		bdbm_error ("ftl is not valid");
		return 1;
	}

	/* create a single queue */
	if ((p->q = bdbm_queue_create (1, INFINITE_QUEUE)) == NULL) {
		bdbm_error ("bdbm_queue_create failed");
		return -1;
	}

	/* keep the private structure */
	bdi->ptr_hlm_inf->ptr_private = (void*)p;

#ifdef USE_THREAD
	bdbm_sema_init (&p->ftl_lock);

	/* create & run a thread */
	if ((p->hlm_thread = bdbm_thread_create (
			__hlm_dftl_thread, bdi, "__hlm_dftl_thread")) == NULL) {
		bdbm_error ("kthread_create failed");
		return -1;
	}
	bdbm_thread_run (p->hlm_thread);
#endif

	return 0;
}

void hlm_dftl_destroy (bdbm_drv_info_t* bdi)
{
	bdbm_hlm_dftl_private_t* p = (bdbm_hlm_dftl_private_t*)bdi->ptr_hlm_inf->ptr_private;

	/* wait until Q becomes empty */
	while (!bdbm_queue_is_all_empty (p->q)) {
		bdbm_msg ("hlm items = %llu", bdbm_queue_get_nr_items (p->q));
		bdbm_thread_msleep (1);
	}

#ifdef USE_THREAD
	bdbm_sema_free (&p->ftl_lock);

	/* kill kthread */
	bdbm_thread_stop (p->hlm_thread);
#endif

	/* destroy queue */
	bdbm_queue_destroy (p->q);

	/* free priv */
	bdbm_free_atomic (p);
}

uint32_t hlm_dftl_make_req (
	bdbm_drv_info_t* bdi, 
	bdbm_hlm_req_t* r)
{
	uint32_t ret, i, loop;
	uint32_t avail = 0;
	bdbm_hlm_dftl_private_t* p = (bdbm_hlm_dftl_private_t*)BDBM_HLM_PRIV(bdi);

	/*bdbm_stopwatch_t sw;*/
	/*bdbm_stopwatch_start (&sw);*/

	if (bdbm_queue_is_full (p->q)) {
		/* FIXME: wait unti queue has a enough room */
		bdbm_error ("it should not be happened!");
		bdbm_bug_on (1);
	} 

	/*bdbm_msg ("%llu %llu", r->lpa, r->len);*/

	/* see if mapping entries for hlm_req are available */
#ifdef USE_THREAD
	bdbm_sema_lock (&p->ftl_lock);
#endif

	/* see if foreground GC is needed or not */
	for (loop = 0; loop < 10; loop++) {
		if ((r->req_type == REQTYPE_WRITE || r->req_type == REQTYPE_READ) &&
			 p->ftl->is_gc_needed != NULL && 
			 p->ftl->is_gc_needed (bdi)) {
			p->ftl->do_gc (bdi);
		} else
			break;
	}

	/* see if there are missing entries */
	if (r->req_type == REQTYPE_WRITE ||
		r->req_type == REQTYPE_READ) {
		for (i = 0; i < r->len; i++) {
			if ((avail = p->ftl->check_mapblk (bdi, r->lpa + i)) == 1)
				break;
		}
	} else if (r->req_type == REQTYPE_TRIM) {
		/* don't fetch mapping entries for TRIM */
	} else {
		bdbm_msg ("oops! invalid req_type (%d)", r->req_type);
		bdbm_bug_on (1);
	}

	/* handle hlm_req */
	if (avail == 0) {
		/* If all of the mapping entries are available, send a hlm_req to llm directly */
		/*bdbm_msg ("main-3");*/
		if ((ret = hlm_nobuf_make_req (bdi, r))) {
			/* if it failed, we directly call 'ptr_host_inf->end_req' */
			bdi->ptr_host_inf->end_req (bdi, r);
			bdbm_warning ("oops! make_req failed");
			/* [CAUTION] r is now NULL */
		}
	} else {
#ifdef USE_THREAD
		/* If some of the mapping entries are *not* available, put a hlm_req to queue. 
		 * This allows other incoming requests not to be affected by a hlm_req
		 * with missing mapping entries */
		bdbm_msg ("put queue");
		if ((ret = bdbm_queue_enqueue (p->q, 0, (void*)r))) {
			bdbm_msg ("bdbm_queue_enqueue failed");
		}
#endif
		ret = __fetch_me_and_make_req (bdi, r);
	}

#ifdef USE_THREAD
	bdbm_sema_unlock (&p->ftl_lock);

	/* wake up thread if it sleeps */
	bdbm_thread_wakeup (p->hlm_thread);
#endif

	/*bdbm_msg ("%llu us +", */
	/*bdbm_stopwatch_get_elapsed_time_us (&sw));*/

	return ret;
}

void hlm_dftl_end_req (
	bdbm_drv_info_t* bdi, 
	bdbm_llm_req_t* r)
{
	if (r->done && r->ds) {
		/* FIXME: r->done is set to not NULL for mapblk */
		bdbm_sema_unlock (r->done);
		return;
	}
	hlm_nobuf_end_req (bdi, r);
}

