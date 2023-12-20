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
#include <linux/list.h>

#elif defined (USER_MODE)
#include <stdio.h>
#include <stdint.h>

#else
#error Invalid Platform (KERNEL_MODE or USER_MODE)
#endif

#include "bdbm_drv.h"
#include "debug.h"
#include "umemory.h"
#include "queue.h"


static int64_t max_queue_items = 0;

bdbm_queue_t* bdbm_queue_create (uint64_t nr_queues, int64_t max_size)
{
	bdbm_queue_t* mq;
	uint64_t loop;

	/* create a private structure */
	if ((mq = (bdbm_queue_t*)bdbm_malloc_atomic (sizeof (bdbm_queue_t))) == NULL) {
		bdbm_msg ("bdbm_malloc_alloc failed");
		return NULL;
	}
	mq->nr_queues = nr_queues;
	mq->max_size = max_size;
	mq->qic = 0;
	bdbm_spin_lock_init (&mq->lock);

	/* create linked-lists */
	if ((mq->qlh = (struct list_head*)bdbm_malloc_atomic (sizeof (struct list_head) * mq->nr_queues)) == NULL) {
		bdbm_msg ("bdbm_malloc_alloc failed");
		bdbm_free_atomic (mq);
		return NULL;
	}
	for (loop = 0; loop < mq->nr_queues; loop++) {
		INIT_LIST_HEAD (&mq->qlh[loop]);
	}

	return mq;
}

/* NOTE: it must be called when mq is empty.
 * If not, we would work improperly (e.g., freeing while locking)
 */
void bdbm_queue_destroy (bdbm_queue_t* mq)
{
	bdbm_free_atomic (mq->qlh);
	bdbm_free_atomic (mq);
}

uint8_t bdbm_queue_enqueue (bdbm_queue_t* mq, uint64_t qid, void* req)
{
	uint32_t ret = 0;
	unsigned long flags;

	if (qid >= mq->nr_queues) {
		bdbm_error ("qid is invalid (%llu)", qid);
		return 1;
	}

	ret = 1;
	bdbm_spin_lock_irqsave (&mq->lock, flags);
	if (mq->max_size == INFINITE_QUEUE || mq->qic < mq->max_size) {
		bdbm_queue_item_t* q = NULL;
		if ((q = (bdbm_queue_item_t*)bdbm_malloc_atomic 
				(sizeof (bdbm_queue_item_t))) == NULL) {
			bdbm_error ("bdbm_malloc_atomic failed");
		} else {
			q->ptr_req = (void*)req;
			list_add_tail (&q->list, &mq->qlh[qid]);	/* add to tail */
			mq->qic++;
			ret = 0;

			if (mq->qic > max_queue_items) {
				max_queue_items = mq->qic;
				/*bdbm_msg ("max queue items: %llu", max_queue_items);*/
			}
		}
	}
	bdbm_spin_unlock_irqrestore (&mq->lock, flags);

	return ret;
}

uint8_t bdbm_queue_enqueue_top (bdbm_queue_t* mq, uint64_t qid, void* req)
{
	uint32_t ret = 0;
	unsigned long flags;

	if (qid >= mq->nr_queues) {
		bdbm_error ("qid is invalid (%llu)", qid);
		return 1;
	}

	ret = 1;
	bdbm_spin_lock_irqsave (&mq->lock, flags);
	if (mq->max_size == INFINITE_QUEUE || mq->qic < mq->max_size) {
		bdbm_queue_item_t* q = NULL;
		if ((q = (bdbm_queue_item_t*)bdbm_malloc_atomic 
				(sizeof (bdbm_queue_item_t))) == NULL) {
			bdbm_error ("bdbm_malloc_atomic failed");
		} else {
			q->ptr_req = (void*)req;
			list_add (&q->list, &mq->qlh[qid]);	/* add to tail */
			mq->qic++;
			ret = 0;

			if (mq->qic > max_queue_items) {
				max_queue_items = mq->qic;
				/*bdbm_msg ("max queue items: %llu", max_queue_items);*/
			}
		}
	}
	bdbm_spin_unlock_irqrestore (&mq->lock, flags);

	return ret;
}

uint8_t bdbm_queue_is_empty (bdbm_queue_t* mq, uint64_t qid)
{
	unsigned long flags;
	struct list_head* pos = NULL;
	bdbm_queue_item_t* q = NULL;
	uint8_t ret = 1;

	bdbm_spin_lock_irqsave (&mq->lock, flags);
	if (mq->qic > 0) {
		list_for_each (pos, &mq->qlh[qid]) {
			q = list_entry (pos, bdbm_queue_item_t, list);
			break;
		}
		if (q)
			ret = 0;
	}
	bdbm_spin_unlock_irqrestore (&mq->lock, flags);

	return ret;
}

void* bdbm_queue_dequeue (bdbm_queue_t* mq, uint64_t qid)
{
	unsigned long flags;
	struct list_head* pos = NULL;
	bdbm_queue_item_t* q = NULL;
	void* req = NULL;

	bdbm_spin_lock_irqsave (&mq->lock, flags);
	if (mq->qic > 0) {
		list_for_each (pos, &mq->qlh[qid]) {
			q = list_entry (pos, bdbm_queue_item_t, list);
			break;
		}
		if (q) {
			req = q->ptr_req;
			list_del (&q->list); /* remove from q */
			bdbm_free_atomic (q); /* free q */
			mq->qic--;
		}
	}
	bdbm_spin_unlock_irqrestore (&mq->lock, flags);

	return req;
}

uint8_t bdbm_queue_is_full (bdbm_queue_t* mq)
{
	uint8_t ret = 0;
	unsigned long flags;

	bdbm_spin_lock_irqsave (&mq->lock, flags);
	if (mq->max_size != INFINITE_QUEUE) {
		bdbm_bug_on (mq->qic > mq->max_size);
		if (mq->qic == mq->max_size)
			ret = 1;
	} else
		ret = 0;
	bdbm_spin_unlock_irqrestore (&mq->lock, flags);

	return ret;
}

uint8_t bdbm_queue_is_all_empty (bdbm_queue_t* mq)
{
	uint8_t ret = 0;
	unsigned long flags;

	bdbm_spin_lock_irqsave (&mq->lock, flags);
	if (mq->qic == 0)
		ret = 1;	/* q is empty */
	bdbm_spin_unlock_irqrestore (&mq->lock, flags);

	return ret;
}

uint64_t bdbm_queue_get_nr_items (bdbm_queue_t* mq)
{
	uint64_t nr_items = 0;
	unsigned long flags;

	bdbm_spin_lock_irqsave (&mq->lock, flags);
	nr_items = mq->qic;
	bdbm_spin_unlock_irqrestore (&mq->lock, flags);

	return nr_items;
}

