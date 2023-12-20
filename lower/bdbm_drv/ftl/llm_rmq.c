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
#include "uthread.h"
#include "pmu.h"
#include "utime.h"

#include "queue/queue.h"
#include "queue/rd_prior_queue.h"

#include "llm_rmq.h"

/* NOTE: This serializes all of the requests from the host file system; 
 * it is useful for debugging */
/*#define ENABLE_SEQ_DBG*/

/* NOTE: This is just a quick fix to support RMW; 
 * it must be improved later for better performance */
#define QUICK_FIX_FOR_RWM	


/* llm interface */
bdbm_llm_inf_t _llm_rmq_inf = {
	.ptr_private = NULL,
	.create = llm_rmq_create,
	.destroy = llm_rmq_destroy,
	.make_req = llm_rmq_make_req,
	.flush = llm_rmq_flush,
	.end_req = llm_rmq_end_req,
};

/* private */
struct bdbm_llm_rmq_private {
	uint64_t nr_punits;
	bdbm_sema_t* punit_locks;
	bdbm_rd_prior_queue_t* q;

	/* for debugging */
#if defined(ENABLE_SEQ_DBG)
	bdbm_sema_t dbg_seq;
#endif	

	/* for thread management */
	bdbm_thread_t* llm_thread;
};

int __llm_rmq_thread (void* arg)
{
	bdbm_drv_info_t* bdi = (bdbm_drv_info_t*)arg;
	struct bdbm_llm_rmq_private* p = (struct bdbm_llm_rmq_private*)BDBM_LLM_PRIV(bdi);
	uint64_t loop;
	uint64_t cnt = 0;

	if (p == NULL || p->q == NULL || p->llm_thread == NULL) {
		bdbm_msg ("invalid parameters (p=%p, p->q=%p, p->llm_thread=%p",
			p, p->q, p->llm_thread);
		return 0;
	}

	for (;;) {
		/* give a chance to other processes if Q is empty */
		if (bdbm_rd_prior_queue_is_all_empty (p->q)) {
			bdbm_thread_schedule_setup (p->llm_thread);
			if (bdbm_rd_prior_queue_is_all_empty (p->q)) {
				/* ok... go to sleep */
				if (bdbm_thread_schedule_sleep (p->llm_thread) == SIGKILL)
					break;
			} else {
				/* there are items in Q; wake up */
				bdbm_thread_schedule_cancel (p->llm_thread);
			}
		}

		/* send reqs until Q becomes empty */
		for (loop = 0; loop < p->nr_punits; loop++) {
			bdbm_rd_prior_queue_item_t* qitem = NULL;
			bdbm_llm_req_t* r = NULL;

			/* if pu is busy, then go to the next pnit */
			if (!bdbm_sema_try_lock (&p->punit_locks[loop]))
				continue;
			
			if ((r = (bdbm_llm_req_t*)bdbm_rd_prior_queue_dequeue (p->q, loop, &qitem)) == NULL) {
				bdbm_sema_unlock (&p->punit_locks[loop]);
				continue;
			}

			r->ptr_qitem = qitem;

			pmu_update_q (bdi, r);

			if (cnt % 50000 == 0) {
				bdbm_msg ("llm_make_req: %llu, %llu", cnt, bdbm_rd_prior_queue_get_nr_items (p->q));
			}

			if (bdi->ptr_dm_inf->make_req (bdi, r)) {
				bdbm_sema_unlock (&p->punit_locks[loop]);

				/* TODO: I do not check whether it works well or not */
				bdi->ptr_llm_inf->end_req (bdi, r);
				bdbm_warning ("oops! make_req failed");
			}

			cnt++;
		}
	}

	return 0;
}

uint32_t llm_rmq_create (bdbm_drv_info_t* bdi)
{
	struct bdbm_llm_rmq_private* p;
	uint64_t loop;

	/* create a private info for llm_nt */
	if ((p = (struct bdbm_llm_rmq_private*)bdbm_malloc_atomic
			(sizeof (struct bdbm_llm_rmq_private))) == NULL) {
		bdbm_error ("bdbm_malloc_atomic failed");
		return -1;
	}

	/* get the total number of parallel units */
	p->nr_punits = BDBM_GET_NR_PUNITS (bdi->parm_dev);

	/* create queue */
	if ((p->q = bdbm_rd_prior_queue_create (p->nr_punits, INFINITE_QUEUE)) == NULL) {
		bdbm_error ("bdbm_prior_queue_create failed");
		goto fail;
	}

	/* create completion locks for parallel units */
	if ((p->punit_locks = (bdbm_sema_t*)bdbm_malloc_atomic
			(sizeof (bdbm_sema_t) * p->nr_punits)) == NULL) {
		bdbm_error ("bdbm_malloc_atomic failed");
		goto fail;
	}
	for (loop = 0; loop < p->nr_punits; loop++) {
		bdbm_sema_init (&p->punit_locks[loop]);
	}

	/* keep the private structures for llm_nt */
	bdi->ptr_llm_inf->ptr_private = (void*)p;

	/* create & run a thread */
	if ((p->llm_thread = bdbm_thread_create (
			__llm_rmq_thread, bdi, "__llm_rmq_thread")) == NULL) {
		bdbm_error ("kthread_create failed");
		goto fail;
	}
	bdbm_thread_run (p->llm_thread);

#if defined(ENABLE_SEQ_DBG)
	bdbm_sema_init (&p->dbg_seq);
#endif

	return 0;

fail:
	if (p->punit_locks)
		bdbm_free_atomic (p->punit_locks);
	if (p->q)
		bdbm_rd_prior_queue_destroy (p->q);
	if (p)
		bdbm_free_atomic (p);
	return -1;
}

/* NOTE: we assume that all of the host requests are completely served.
 * the host adapter must be first closed before this function is called.
 * if not, it would work improperly. */
void llm_rmq_destroy (bdbm_drv_info_t* bdi)
{
	uint64_t loop;
	struct bdbm_llm_rmq_private* p = (struct bdbm_llm_rmq_private*)BDBM_LLM_PRIV(bdi);

	if (p == NULL)
		return;

	/* wait until Q becomes empty */
	while (!bdbm_rd_prior_queue_is_all_empty (p->q)) {
		bdbm_msg ("llm items = %llu", bdbm_rd_prior_queue_get_nr_items (p->q));
		bdbm_thread_msleep (1);
	}

	/* kill kthread */
	bdbm_thread_stop (p->llm_thread);

	for (loop = 0; loop < p->nr_punits; loop++) {
		bdbm_sema_lock (&p->punit_locks[loop]);
	}

	/* release all the relevant data structures */
	if (p->q)
		bdbm_rd_prior_queue_destroy (p->q);
	if (p) 
		bdbm_free_atomic (p);
	bdbm_msg ("done");
}

uint32_t llm_rmq_make_req (bdbm_drv_info_t* bdi, bdbm_llm_req_t* r)
{
	uint32_t ret;
	uint64_t punit_id;
	struct bdbm_llm_rmq_private* p = (struct bdbm_llm_rmq_private*)BDBM_LLM_PRIV(bdi);

	/* FIXME: ugry */
	rd_prior_iotype_t type;
	if (r->req_type == REQTYPE_READ ||
		r->req_type == REQTYPE_READ_DUMMY ||
		r->req_type == REQTYPE_RMW_READ || 
		r->req_type == REQTYPE_GC_READ ||
		r->req_type == REQTYPE_META_READ) {
		type = RD_PRIORITY_READ;
	} else {
		type = RD_PRIORITY_WRITE;
	}

#if defined(ENABLE_SEQ_DBG)
	bdbm_sema_lock (&p->dbg_seq);
#endif

	/* obtain the elapsed time taken by FTL algorithms */
	pmu_update_sw (bdi, r);

	/* get a parallel unit ID */
	punit_id = r->phyaddr->punit_id;

	while (bdbm_rd_prior_queue_get_nr_items (p->q) >= 96) {
		/*yield ();*/
		bdbm_thread_yield ();
	}

	/* put a request into Q */
#if defined(QUICK_FIX_FOR_RWM)
	if (r->req_type == REQTYPE_RMW_READ) {
		/* FIXME: this is a quick fix to support RMW; it must be improved later */
		/* step 1: put READ first */
		if ((ret = bdbm_rd_prior_queue_enqueue (p->q, punit_id, r->lpa, (void*)r, type))) {
			bdbm_msg ("bdbm_prior_queue_enqueue failed");
		}

		/* step 2: put WRITE second with the same LPA */
		punit_id = r->phyaddr_w.punit_id;
		if ((ret = bdbm_rd_prior_queue_enqueue (p->q, punit_id, r->lpa, (void*)r, type))) {
			bdbm_msg ("bdbm_prior_queue_enqueue failed");
		}
	} else {
		if ((ret = bdbm_rd_prior_queue_enqueue (p->q, punit_id, r->lpa, (void*)r, type))) {
			bdbm_msg ("bdbm_prior_queue_enqueue failed");
		}
	}
#else
	if ((ret = bdbm_rd_prior_queue_enqueue (p->q, punit_id, r->lpa, (void*)r, type))) {
		bdbm_msg ("bdbm_prior_queue_enqueue failed");
	}
#endif

	/* wake up thread if it sleeps */
	bdbm_thread_wakeup (p->llm_thread);

	return ret;
}

void llm_rmq_flush (bdbm_drv_info_t* bdi)
{
	struct bdbm_llm_rmq_private* p = (struct bdbm_llm_rmq_private*)BDBM_LLM_PRIV(bdi);

	while (bdbm_rd_prior_queue_is_all_empty (p->q) != 1) {
		/*cond_resched ();*/
		bdbm_thread_yield ();
	}
}

void llm_rmq_end_req (bdbm_drv_info_t* bdi, bdbm_llm_req_t* r)
{
	struct bdbm_llm_rmq_private* p = (struct bdbm_llm_rmq_private*)BDBM_LLM_PRIV(bdi);
	bdbm_rd_prior_queue_item_t* qitem = (bdbm_rd_prior_queue_item_t*)r->ptr_qitem;

	switch (r->req_type) {
	case REQTYPE_RMW_READ:
		/* get a parallel unit ID */
		bdbm_sema_unlock (&p->punit_locks[r->phyaddr->punit_id]);

		/* change its type to WRITE if req_type is RMW */
		r->phyaddr = &r->phyaddr_w;
		r->req_type = REQTYPE_RMW_WRITE;

#ifdef QUICK_FIX_FOR_RWM
		bdbm_rd_prior_queue_remove (p->q, qitem);
#else
		/* put it to Q again */
		if (bdbm_rd_prior_queue_move (p->q, r->phyaddr->punit_id, qitem)) {
			bdbm_msg ("bdbm_prior_queue_enqueue failed");
			bdbm_bug_on (1);
		}
#endif

		pmu_inc (bdi, r);

		/* wake up thread if it sleeps */
		bdbm_thread_wakeup (p->llm_thread);
		break;

	case REQTYPE_META_WRITE:
	case REQTYPE_META_READ:
	case REQTYPE_READ:
	case REQTYPE_READ_DUMMY:
	case REQTYPE_WRITE:
	case REQTYPE_RMW_WRITE:
	case REQTYPE_GC_READ:
	case REQTYPE_GC_WRITE:
	case REQTYPE_GC_ERASE:
	case REQTYPE_TRIM:
		/* get a parallel unit ID */
		bdbm_rd_prior_queue_remove (p->q, qitem);

		/*
		if (r->req_type == REQTYPE_GC_WRITE && (int64_t)r->lpa == -2LL) {
			bdbm_msg ("done - writing mapping pages: llu phy: %lld %lld %lld %lld, oob: %lld %lld", 
				r->phyaddr->channel_no, 
				r->phyaddr->chip_no,
				r->phyaddr->block_no,
				r->phyaddr->page_no,
				((uint64_t*)r->ptr_oob)[0],
				((uint64_t*)r->ptr_oob)[1]);
		}
		*/

		/* complete a lock */
		bdbm_sema_unlock (&p->punit_locks[r->phyaddr->punit_id]);

		/* update the elapsed time taken by NAND devices */
		pmu_update_tot (bdi, r);
		pmu_inc (bdi, r);

		/* finish a request */
		bdi->ptr_hlm_inf->end_req (bdi, r);

#if defined(ENABLE_SEQ_DBG)
		bdbm_sema_unlock (&p->dbg_seq);
#endif
		break;

	default:
		bdbm_error ("invalid req-type (%u)", r->req_type);
		break;
	}
}

