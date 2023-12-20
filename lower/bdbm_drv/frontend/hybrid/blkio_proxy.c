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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/poll.h> /* poll_table, etc. */
#include <linux/cdev.h> /* cdev_init, etc. */
#include <linux/device.h> /* class_create, device_create, etc */
#include <linux/mm.h>  /* mmap related stuff */
#include <linux/delay.h> /* msleep */
#include <linux/sched.h> /* TASK_INTERRUPTIBLE */
#include <linux/blkdev.h> /* bio */
#include <asm/uaccess.h> /* ioctl */
#include <linux/workqueue.h> /* workqueue */
#include <linux/jiffies.h> /* jiffies */

#include "bdbm_drv.h"
#include "umemory.h"
#include "debug.h"

#include "blkio_proxy.h"
#include "blkio_proxy_ioctl.h"
#include "blkdev.h"
#include "blkio_proxy_reqs_pool.h"


static bdbm_drv_info_t* _bdi = NULL;

bdbm_host_inf_t _blkio_proxy_inf = {
	.ptr_private = NULL,
	.open = blkio_proxy_open,
	.close = blkio_proxy_close,
	.make_req = blkio_proxy_make_req,
	.end_req = blkio_proxy_end_req,
};

/* http://www.tune2wizard.com/kernel-programming-workqueue/ */
typedef struct {
	struct work_struct work; /* it must be at the end of structre */
	int id;
	bdbm_drv_info_t* bdi;
} bdbm_blkio_proxy_wq_t;

typedef struct {
	atomic_t ref_cnt; /* # of the user-level FTLs that are linked to the kernel */
	atomic_t nr_out_reqs; /* # of outstanding requests */

	wait_queue_head_t pollwq;
	bdbm_spinlock_t lock;
	bdbm_sema_t mutex;
	struct semaphore sem;

	/* for mmap management */
	int64_t mmap_nr_reqs;
	bdbm_blkio_proxy_req_t* mmap_reqs_buf;
	bdbm_proxy_reqs_pool_t* reqs_pool;

	/* workqueue */
	struct workqueue_struct *wq;
	bdbm_blkio_proxy_wq_t* works;
} bdbm_blkio_proxy_t;

static int blkio_proxy_ioctl_init (void);
static int blkio_proxy_ioctl_exit (void);


/* This is a call-back function invoked by a block-device layer */
static void __host_blkio_make_request_fn (
	struct request_queue *q, 
	struct bio *bio)
{
	blkio_proxy_make_req (_bdi, (void*)bio);
}

static void __blkio_proxy_fops_wq_handler (
	struct work_struct *w)
{
	bdbm_blkio_proxy_wq_t* work = (bdbm_blkio_proxy_wq_t*)w;
	bdbm_drv_info_t* bdi = work->bdi;
	bdbm_blkio_proxy_t* p = (bdbm_blkio_proxy_t*)BDBM_HOST_PRIV (bdi);
	bdbm_host_inf_t* h = (bdbm_host_inf_t*)BDBM_GET_HOST_INF (bdi);
	bdbm_blkio_proxy_req_t* r = NULL;

	/* get req from the pool with ID */
	r = &p->mmap_reqs_buf[work->id];
	bdbm_bug_on (r->stt != REQ_STT_USER_DONE);

	/* call end_req */
	h->end_req (bdi, (bdbm_hlm_req_t*)r);
}

static inline int __is_client_ready (bdbm_blkio_proxy_t* p)
{
	if (atomic_read (&p->ref_cnt) == 0)
		return 1;
	return 0;
}

static bdbm_blkio_proxy_req_t* __get_blkio_proxy_req (
	bdbm_blkio_proxy_t* p)
{
	bdbm_blkio_proxy_req_t* proxy_req = NULL;
	int i, retry_cnt = 10;

	for (i = 0; i < retry_cnt; i++) {
		/* is an empty slot is available? */
		bdbm_spin_lock (&p->lock);
		if ((proxy_req = bdbm_proxy_reqs_pool_alloc_item 
				(p->reqs_pool)) == NULL) {
			bdbm_spin_unlock (&p->lock);
			/* wait until there is a new empty slot */
			msleep (1000);
			continue;
		}
		bdbm_spin_unlock (&p->lock);
		break;
	}

	return proxy_req;
}

static int __encode_bio_to_proxy_req (
	struct bio* bio, 
	bdbm_blkio_proxy_req_t* proxy_req)
{
	uint32_t loop = 0;
	struct bio_vec *bvec = NULL;
	bdbm_blkio_req_t* r = (bdbm_blkio_req_t*)&proxy_req->blkio_req;

	/* get the type of the bio request */
	if (bio->bi_rw & REQ_DISCARD)
		r->bi_rw = REQTYPE_TRIM;
	else if (bio_data_dir (bio) == READ || bio_data_dir (bio) == READA)
		r->bi_rw = REQTYPE_READ;
	else if (bio_data_dir (bio) == WRITE)
		r->bi_rw = REQTYPE_WRITE;
	else {
		bdbm_error ("oops! invalid request type (bi->bi_rw = %lx)", bio->bi_rw);
		return 1;
	}

	/* get the offset and the length of the bio */
	r->bi_offset = bio->bi_sector;
	r->bi_size = bio_sectors (bio);
	r->bi_bvec_cnt = 0;
	r->bio = (void*)bio;

#if 0
	if (r->bi_rw == REQTYPE_WRITE) {
		bdbm_msg ("proxy - [%llx] offset: %llu, size: %llu", r->bi_rw, r->bi_offset*512/4096, r->bi_size/8);
	}
#endif

	/* get the data from the bio */
	if (r->bi_rw != REQTYPE_TRIM) {
		bio_for_each_segment (bvec, bio, loop) {
			uint8_t* mmap_vec = (uint8_t*)proxy_req->bi_bvec_ptr[r->bi_bvec_cnt];
			uint8_t* page_vec = (uint8_t*)page_address (bvec->bv_page);
			bdbm_bug_on (mmap_vec == NULL);
			bdbm_bug_on (page_vec == NULL);
			if (r->bi_rw == REQTYPE_WRITE)
				bdbm_memcpy (mmap_vec, page_vec, KERNEL_PAGE_SIZE);
			r->bi_bvec_cnt++;
			if (r->bi_bvec_cnt >= BDBM_BLKIO_MAX_VECS) {
				/* NOTE: this is an impossible case unless kernel parameters are changed */
				bdbm_error ("oops! # of vectors in bio is larger than %u", 
					BDBM_BLKIO_MAX_VECS);
				break;
			}
		}
	}
/*
	bdbm_msg ("type: %x offset: %llu size: %llu bvec_cnt: %llu", 
		r->bi_rw, r->bi_offset, r->bi_size, r->bi_bvec_cnt);
*/
	return 0;
}

static inline void __free_block_io_proxy_req (
	bdbm_blkio_proxy_t* p, 
	bdbm_blkio_proxy_req_t* r)
{
	bdbm_spin_lock (&p->lock);
	bdbm_proxy_reqs_pool_free_item (p->reqs_pool, r);
	bdbm_spin_unlock (&p->lock);
}

static int __kill_pending_proxy_reqs (bdbm_drv_info_t *bdi)
{
	bdbm_blkio_proxy_t* p = (bdbm_blkio_proxy_t*)BDBM_HOST_PRIV (bdi);
	bdbm_blkio_proxy_req_t* r = NULL;
	int i = 0, nr_cancel = 0, nr_to_be_killed;

	nr_to_be_killed = atomic_read (&p->nr_out_reqs);

	if (nr_to_be_killed > 0) {
		bdbm_warning ("# of requests to be killed: %d", nr_to_be_killed);
	}

	for (i = 0; i < p->mmap_nr_reqs; i++) {
		r = &p->mmap_reqs_buf[i];
		if (r->stt != REQ_STT_FREE) {
			if (p->reqs_pool) {
				__free_block_io_proxy_req (p, r);
			} else {
				bdbm_warning ("hmm.. p->reqs_pool is NULL");
				r->stt = REQ_STT_FREE;
			}
			bio_endio (r->blkio_req.bio, -EIO);
			atomic_dec (&p->nr_out_reqs);
			up (&p->sem);
			nr_cancel++;
		}
	}

	if (nr_cancel > 0) {
		bdbm_warning ("# of cancelled requests: %d", nr_cancel);
	}

	return nr_cancel;
}

/* The implement of blkio_proxy */
uint32_t blkio_proxy_open (bdbm_drv_info_t* bdi)
{
	bdbm_blkio_proxy_t* p = NULL;
	int32_t size, i;

	/* see if hlm_user_proxy is already created */
	if (_bdi) {
		bdbm_error ("blkio_proxy is already created");
		return -EIO;
	}

	/* create bdbm_blkio_proxy_t with zeros */
	if ((p = (bdbm_blkio_proxy_t*)bdbm_zmalloc (sizeof (bdbm_blkio_proxy_t))) == NULL) {
		bdbm_error ("bdbm_malloc failed");
		return -EIO;
	}

	/* initialize some variables */
	init_waitqueue_head (&p->pollwq);
	bdbm_spin_lock_init (&p->lock);
	bdbm_sema_init (&p->mutex);
	atomic_set (&p->nr_out_reqs, 0);
	atomic_set (&p->ref_cnt, 0);
	p->mmap_nr_reqs = BDBM_PROXY_MAX_REQS;	/* just large enough number */
	sema_init (&p->sem, p->mmap_nr_reqs);

	/* create workqueues */
	p->wq = create_singlethread_workqueue ("blkio_proxy_wq");
	p->works = bdbm_malloc (sizeof (bdbm_blkio_proxy_wq_t) *  p->mmap_nr_reqs);
	for (i = 0; i < p->mmap_nr_reqs; i++) {
		INIT_WORK (&p->works[i].work, __blkio_proxy_fops_wq_handler); 
		p->works[i].id = i;
	}

	/* create requests */
	size = PAGE_ALIGN (sizeof (bdbm_blkio_proxy_req_t) * p->mmap_nr_reqs);
	if ((p->mmap_reqs_buf = (bdbm_blkio_proxy_req_t*)bdbm_malloc (size)) == NULL) {
		bdbm_error ("kmalloc () failed (%d)", size);
		goto fail;
	}
	for (i = 0; i < size; i+=PAGE_SIZE) {
		SetPageReserved (vmalloc_to_page (
			(void*)(((unsigned long)p->mmap_reqs_buf) + i))
		);
	}
	memset (p->mmap_reqs_buf, 0x00, size);

	/* create a request pool */
	if ((p->reqs_pool = bdbm_proxy_reqs_pool_create
			(p->mmap_nr_reqs, p->mmap_reqs_buf)) == NULL) {
		bdbm_error ("bdbm_proxy_reqs_pool_create () failed");
		goto fail;
	}

	/* assign p to bdi */
	bdi->ptr_host_inf->ptr_private = (void*)p;
	_bdi = bdi;

	/* register a character device (for user-level FTL) */
	if (blkio_proxy_ioctl_init () != 0) {
		bdbm_error ("failed to register a character device");
		goto fail;
	}

	/* register a block device (for applications) */
	if (host_blkdev_register_device	(bdi, __host_blkio_make_request_fn) != 0) {
		bdbm_error ("failed to register blueDBM");
		blkio_proxy_ioctl_exit ();
		goto fail;
	}

	return 0;

fail:
	if (p->reqs_pool)
		bdbm_proxy_reqs_pool_destroy (p->reqs_pool);
	if (p->mmap_reqs_buf) {
		for (i = 0; i < size; i+=PAGE_SIZE) {
			ClearPageReserved (vmalloc_to_page (
				(void*)(((unsigned long)p->mmap_reqs_buf) + i))
			);
		}
		bdbm_free (p->mmap_reqs_buf);
	}
	if (p->works)
		bdbm_free (p->works);
	if (p->wq) 
		destroy_workqueue (p->wq);
	bdbm_sema_free (&p->mutex);
	bdbm_spin_lock_destory (&p->lock);
	init_waitqueue_head (&p->pollwq);
	bdbm_free (p);
	_bdi = NULL;

	return 1;
}

void blkio_proxy_close (bdbm_drv_info_t* bdi)
{
	bdbm_blkio_proxy_t* p = NULL;
	
	if (!bdi || !_bdi) 
		return;

	if (!(p = (bdbm_blkio_proxy_t*)BDBM_HOST_PRIV (bdi)))
		return;

	bdbm_sema_lock (&p->mutex);

	/* is there the user-level FTL which is attached to the kernel? */
	if (atomic_read (&p->ref_cnt) > 0) {
		bdbm_sema_unlock (&p->mutex);
		return;
	}
	_bdi = NULL;

	/* before closing it, we must wait until all the on-gonging requests are
	 * finished */
	while (atomic_read (&p->nr_out_reqs) > 0) {
		static int retry = 0;
		bdbm_msg ("blkio_proxy is busy... (cnt: %d)", retry);
		msleep (1000);
		retry++;
		if (retry > 3) {
			__kill_pending_proxy_reqs (bdi);
			bdbm_warning ("blkio_proxy is not nicely closed (too many retries)");
			break;
		}
	}

	/* destroy the block device */
	host_blkdev_unregister_block_device (bdi);

	/* destroy the character device */
	blkio_proxy_ioctl_exit ();

	/* free all variables related to blkio_proxy */
	if (p->mmap_reqs_buf) {
		int32_t i = 0;
		int32_t size = PAGE_ALIGN (sizeof (bdbm_blkio_proxy_req_t) * p->mmap_nr_reqs);
		for (i = 0; i < size; i+=PAGE_SIZE) {
			ClearPageReserved (vmalloc_to_page (
				(void*)(((unsigned long)p->mmap_reqs_buf) + i))
			);
		}
		bdbm_free (p->mmap_reqs_buf);
	}
	if (p->reqs_pool)
		bdbm_proxy_reqs_pool_destroy (p->reqs_pool);
	bdbm_spin_lock_destory (&p->lock);
	init_waitqueue_head (&p->pollwq);

	bdbm_sema_unlock (&p->mutex);
	bdbm_sema_free (&p->mutex);
	bdbm_free (p);
}

void blkio_proxy_make_req (bdbm_drv_info_t* bdi, void* req)
{
	struct bio* bio = (struct bio*)req;
	bdbm_blkio_proxy_t* p = (bdbm_blkio_proxy_t*)BDBM_HOST_PRIV (bdi);
	bdbm_blkio_proxy_req_t* proxy_req = NULL;

	bdbm_sema_lock (&p->mutex);

	/* blkio_proxy was closed */
	if (!_bdi) {
		bio_endio (bio, -EIO);
		bdbm_sema_unlock (&p->mutex);
		return;
	}

	/* see if the user-level FTL is connected to the kernel */
	if (__is_client_ready (p) != 0) {
		/*bdbm_warning ("oops! the user-level FTL is not ready");*/
		bio_endio (bio, -EIO);
		bdbm_sema_unlock (&p->mutex);
		return;
	}

	/* down the semaphore; it takes long time in the case where the user-level FTL died */
	if (down_timeout (&p->sem, msecs_to_jiffies (5000)) != 0) {
		bdbm_warning ("oops! the user-level FTL is not responding...");
		bio_endio (bio, -EIO);
		bdbm_sema_unlock (&p->mutex);
		return;
	}

	/* send an incoming request to the user-level FTL */
	/* (1) get an empty mmap_req slot */
	if ((proxy_req = __get_blkio_proxy_req (p)) == NULL) {
		bdbm_warning ("oops! mmap_reqs is full");
		bio_endio (bio, -EIO);
		bdbm_sema_unlock (&p->mutex);
		up (&p->sem);
		return;
	}

	/* (2) encode it to mapped-memory */
	if (__encode_bio_to_proxy_req (bio, proxy_req) != 0) {
		__free_block_io_proxy_req (p, proxy_req);
		bdbm_sema_unlock (&p->mutex);
		up (&p->sem);
		return;
	}

	proxy_req->stt = REQ_STT_KERN_INIT;
	proxy_req->blkio_req.bio = (void*)bio;

	atomic_inc (&p->nr_out_reqs);
	if (atomic_read (&p->nr_out_reqs) > BDBM_PROXY_MAX_REQS) {
		bdbm_warning ("oops! # of out-reqs > %d (%d)", 
			atomic_read (&p->nr_out_reqs),
			BDBM_PROXY_MAX_REQS);
	}

	/* trigger a poller */
	wake_up_interruptible (&(p->pollwq));

	bdbm_sema_unlock (&p->mutex);
}

void blkio_proxy_end_req (bdbm_drv_info_t* bdi, bdbm_hlm_req_t* req)
{
	bdbm_blkio_proxy_t* p = (bdbm_blkio_proxy_t*)BDBM_HOST_PRIV (bdi);
	bdbm_blkio_proxy_req_t* proxy_req = (bdbm_blkio_proxy_req_t*)req;
	struct bio* bio = (struct bio*)proxy_req->blkio_req.bio;

	/* copy mmap to the bio if it is REQTYPE_READ */
	if (proxy_req->blkio_req.bi_rw == REQTYPE_READ) {
		struct bio_vec *bvec = NULL;
		uint32_t loop = 0, i = 0;
		bio_for_each_segment (bvec, bio, loop) {
			uint8_t* mmap_vec = (uint8_t*)proxy_req->bi_bvec_ptr[i];
			uint8_t* page_vec = (uint8_t*)page_address (bvec->bv_page);
			bdbm_bug_on (mmap_vec == NULL);
			bdbm_bug_on (page_vec == NULL);
			bdbm_memcpy (page_vec, mmap_vec, KERNEL_PAGE_SIZE);
			if (loop >= BDBM_BLKIO_MAX_VECS) {
				/* NOTE: this is an impossible case unless kernel parameters are changed */
				bdbm_error ("oops! # of vectors in bio is larger than %u", 
					BDBM_BLKIO_MAX_VECS);
				break;
			}
			i++;
		}
	}

	/* end bio */
	if (proxy_req->blkio_req.ret == 0)
		bio_endio (bio, 0);
	else {
		bdbm_warning ("oops! make_req () failed with %d", proxy_req->blkio_req.ret);
		bio_endio (bio, -EIO);
	}

	/* free pool */
	__free_block_io_proxy_req (p, proxy_req);
	proxy_req->stt = REQ_STT_FREE;

	atomic_dec (&p->nr_out_reqs);
	if (atomic_read (&p->nr_out_reqs) < 0)
		bdbm_warning ("oops! p->nr_out_reqs is negative (%d)", atomic_read (&p->nr_out_reqs));

	up (&p->sem);
}


/*
 * For the interaction with user-level application
 */
static long blkio_proxy_fops_ioctl (struct file *filp, unsigned int cmd, unsigned long arg);
static unsigned int blkio_proxy_fops_poll (struct file *filp, poll_table *poll_table);
static void blkio_proxy_mmap_open (struct vm_area_struct *vma);
static void blkio_proxy_mmap_close (struct vm_area_struct *vma);
static int blkio_proxy_fops_mmap (struct file *filp, struct vm_area_struct *vma);
static int blkio_proxy_fops_create (struct inode *inode, struct file *filp);
static int blkio_proxy_fops_release (struct inode *inode, struct file *filp);

static struct vm_operations_struct mmap_vm_ops = {
	.open = blkio_proxy_mmap_open,
	.close = blkio_proxy_mmap_close,
};

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.mmap = blkio_proxy_fops_mmap, 
	.open = blkio_proxy_fops_create,
	.release = blkio_proxy_fops_release,
	.poll = blkio_proxy_fops_poll,
	.unlocked_ioctl = blkio_proxy_fops_ioctl,
	.compat_ioctl = blkio_proxy_fops_ioctl,
};

void blkio_proxy_mmap_open (struct vm_area_struct *vma)
{
#if 0
	bdbm_msg ("blkio_proxy_mmap_open: virt %lx, phys %lx", vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
#endif
}

void blkio_proxy_mmap_close (struct vm_area_struct *vma)
{
}

static int blkio_proxy_fops_mmap (struct file *filp, struct vm_area_struct *vma)
{
	bdbm_drv_info_t* bdi = (bdbm_drv_info_t*)filp->private_data;
	bdbm_blkio_proxy_t* p = (bdbm_blkio_proxy_t*)BDBM_HOST_PRIV (bdi);
	uint64_t size = vma->vm_end - vma->vm_start;
 	unsigned long pfn, start = vma->vm_start;
	char *vmalloc_addr = (char *)p->mmap_reqs_buf;

	if (p == NULL) {
		bdbm_warning ("blkio_proxy is not created yet");
		return -EINVAL;
	}

	if (size != PAGE_ALIGN (p->mmap_nr_reqs * sizeof (bdbm_blkio_proxy_req_t))) {
		bdbm_warning ("size > p->mmap_nr_reqs: %llu > %llu (%llu / %ld)", 
			size, 
			p->mmap_nr_reqs * sizeof (bdbm_blkio_proxy_req_t), 
			p->mmap_nr_reqs,
			sizeof (bdbm_blkio_proxy_req_t));
		return -EINVAL;
	} else {
		bdbm_msg ("blkio_proxy_fops_mmap: %lld MB (max_reqs: %d, max_vecs: %d)",
			size/(1024*1024), BDBM_PROXY_MAX_REQS, BDBM_BLKIO_MAX_VECS);
	}

	vma->vm_page_prot = pgprot_noncached (vma->vm_page_prot);
	while (size > 0) {
		pfn = vmalloc_to_pfn (vmalloc_addr);
		if (remap_pfn_range (vma, start, pfn, PAGE_SIZE, PAGE_SHARED) < 0) {
			return -EAGAIN;
		}
		start += PAGE_SIZE;
		vmalloc_addr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}

	vma->vm_ops = &mmap_vm_ops;
	vma->vm_private_data = p;	/* bdbm_blkio_proxy_t */
	blkio_proxy_mmap_open (vma);

	bdbm_msg ("blkio_proxy_fops_mmap is called (%lu)", vma->vm_end - vma->vm_start);

	return 0;
}

static int blkio_proxy_fops_create (struct inode *inode, struct file *filp)
{
	bdbm_drv_info_t* bdi = _bdi;
	bdbm_blkio_proxy_t* p = NULL;

	/* see if other FTL are already connected to the kernel */
	if (filp->private_data != NULL) {
		bdbm_error ("filp->private_data is *NOT* NULL");
		return -EBUSY;
	}

	/* is bdbm_blkio_proxy_t created? */
	if (bdi == NULL) {
		bdbm_error ("the kernel is not initialized yet");
		return -EBUSY;
	}

	p = (bdbm_blkio_proxy_t*)BDBM_HOST_PRIV (bdi);

	/* check # of ref_cnt */
	if (atomic_read (&p->ref_cnt) > 0) {
		bdbm_error ("The user-level FTL is already attached to the kernel (ref_cnt: %u)", 
			atomic_read (&p->ref_cnt));
		return -EBUSY;
	}
	atomic_inc (&p->ref_cnt);

	/* ok! assign bdbm_blkio_proxy_ioctl to private_data */
	filp->private_data = (void *)_bdi;
	filp->f_mode |= FMODE_WRITE;

	bdbm_msg ("[%s] The user-level FTL is attached to the kernel succesfully (%u)", 
		__FUNCTION__, atomic_read (&p->ref_cnt));

	return 0;
}

static int blkio_proxy_fops_release (struct inode *inode, struct file *filp)
{
	bdbm_drv_info_t* bdi = (bdbm_drv_info_t*)filp->private_data;
	bdbm_blkio_proxy_t* p = (bdbm_blkio_proxy_t*)BDBM_HOST_PRIV (bdi);

	bdbm_sema_lock (&p->mutex);

	/* bdbm_blkio_proxy_ioctl is not open before */
	if (p == NULL) {
		bdbm_warning ("oops! attempt to close blkio_proxy which was closed or not opened before");
		bdbm_sema_unlock (&p->mutex);
		return 0;
	}

	if (atomic_read (&p->ref_cnt) == 0) {
		bdbm_warning ("oops! ref_cnt is 0");
		bdbm_sema_unlock (&p->mutex);
		return 0;
	}
		
	__kill_pending_proxy_reqs (bdi);

	/* reset private_data */
	filp->private_data = (void *)NULL;

	atomic_dec (&p->ref_cnt);

	bdbm_sema_unlock (&p->mutex);

	return 0;
}

static unsigned int blkio_proxy_fops_poll (struct file *filp, poll_table *poll_table)
{
	bdbm_drv_info_t* bdi = (bdbm_drv_info_t*)filp->private_data;
	bdbm_blkio_proxy_t* p = (bdbm_blkio_proxy_t*)BDBM_HOST_PRIV (bdi);
	unsigned int mask = 0;

	if (p == NULL) {
		bdbm_error ("bdbm_blkio_proxy_ioctl is not created");
		return 0;
	}

	poll_wait (filp, &p->pollwq, poll_table);

	/* are there any new requests? */
	if (atomic_read (&p->nr_out_reqs) > 0) {
		int i = 0;
		bdbm_blkio_proxy_req_t* r = NULL;
		for (i = 0; i < p->mmap_nr_reqs; i++) {
			r = &p->mmap_reqs_buf[i];
			if (r->stt == REQ_STT_KERN_INIT) {
				r->stt = REQ_STT_KERN_SENT;
				mask |= POLLIN | POLLRDNORM; 
			}
		}
	}

	return mask;
}

static long blkio_proxy_fops_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
	bdbm_drv_info_t* bdi = (bdbm_drv_info_t*)filp->private_data;
	bdbm_blkio_proxy_t* p = (bdbm_blkio_proxy_t*)BDBM_HOST_PRIV (bdi);
	bdbm_host_inf_t* h = (bdbm_host_inf_t*)BDBM_GET_HOST_INF (bdi);
	int ret = 0;

	/* see if blkio_proxy is valid or not */
	if (p == NULL) {
		bdbm_error ("bdbm_blkio_proxy_ioctl is not created");
		return -ENOTTY;
	}

	/* handle a command from user applications */
	switch (cmd) {
	case BDBM_BLOCKIO_PROXY_IOCTL_DONE:
		if (h != NULL && h->end_req != NULL) {
			int req_id = -1;
			int __user* req_id_ur = (int __user*)arg;

			/* copy ur_id from user-level FTL */
			copy_from_user (&req_id, req_id_ur, sizeof (int));
			bdbm_bug_on (req_id < 0);
			bdbm_bug_on (req_id >= p->mmap_nr_reqs);

			/* run workqueue */
			p->works[req_id].bdi = bdi;
			p->works[req_id].id = req_id;
			queue_work (p->wq, &p->works[req_id].work);
		}
		break;

	default:
		bdbm_warning ("invalid command code");
		ret = -ENOTTY;
	}

	return ret;
}

/*
 * For the registration of blkio_proxy as a character device
 */
static dev_t devnum = 0; 
static struct cdev c_dev;
static struct class *cl = NULL;
static int FIRST_MINOR = 0;
static int MINOR_CNT = 1;

/* register a bdbm_blkio_proxy_ioctl driver */
static int blkio_proxy_ioctl_init (void)
{
	int ret = -1;
	struct device *dev_ret = NULL;

	if ((ret = alloc_chrdev_region (&devnum, FIRST_MINOR, MINOR_CNT, BDBM_BLOCKIO_PROXY_IOCTL_NAME)) != 0) {
		bdbm_error ("bdbm_blkio_proxy_ioctl registration failed: %d\n", ret);
		return ret;
	}
	cdev_init (&c_dev, &fops);

	if ((ret = cdev_add (&c_dev, devnum, MINOR_CNT)) < 0) {
		bdbm_error ("bdbm_blkio_proxy_ioctl registration failed: %d\n", ret);
		return ret;
	}

	if (IS_ERR (cl = class_create (THIS_MODULE, "char"))) {
		bdbm_error ("bdbm_blkio_proxy_ioctl registration failed: %d\n", MAJOR(devnum));
		cdev_del (&c_dev);
		unregister_chrdev_region (devnum, MINOR_CNT);
		return PTR_ERR (cl);
	}

	if (IS_ERR (dev_ret = device_create (cl, NULL, devnum, NULL, BDBM_BLOCKIO_PROXY_IOCTL_NAME))) {
		bdbm_error ("bdbm_blkio_proxy_ioctl registration failed: %d\n", MAJOR(devnum));
		class_destroy (cl);
		cdev_del (&c_dev);
		unregister_chrdev_region (devnum, MINOR_CNT);
		return PTR_ERR (dev_ret);
	}

	bdbm_msg ("bdbm_blkio_proxy_ioctl is installed: %s (major:%d minor:%d)", 
		BDBM_BLOCKIO_PROXY_IOCTL_DEVNAME, 
		MAJOR(devnum), MINOR(devnum));

	return 0;
}

/* remove a bdbm_db_stub driver */
static int blkio_proxy_ioctl_exit (void)
{
	if (cl == NULL || devnum == 0) {
		bdbm_warning ("bdbm_blkio_proxy_ioctl is not installed yet");
		return 1;
	}

	/* get rid of bdbm_blkio_proxy_ioctl */
	device_destroy (cl, devnum);
    class_destroy (cl);
    cdev_del (&c_dev);
    unregister_chrdev_region (devnum, MINOR_CNT);

	bdbm_msg ("bdbm_blkio_proxy_ioctl is removed: %s (%d %d)", 
		BDBM_BLOCKIO_PROXY_IOCTL_DEVNAME, 
		MAJOR(devnum), MINOR(devnum));

	return 0;
}

/* NOTE: We create fake interfaces to avoid compile errors. Do not use them for
 * any other purposes! */
/*bdbm_ftl_inf_t _ftl_block_ftl, _ftl_page_ftl, _ftl_dftl, _ftl_no_ftl;*/
/*bdbm_hlm_inf_t _hlm_dftl_inf, _hlm_buf_inf, _hlm_nobuf_inf, _hlm_rsd_inf;*/
/*bdbm_llm_inf_t _llm_mq_inf, _llm_noq_inf;*/
/*bdbm_host_inf_t _blkio_inf;*/

bdbm_hlm_inf_t _hlm_nobuf_inf;
bdbm_ftl_inf_t _ftl_page_ftl, _ftl_block_ftl;
bdbm_llm_inf_t _llm_mq_inf;
