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

#if !defined (KERNEL_MODE)
#error "dm_stub only supports KERNEL_MODE"
#endif

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/poll.h> /* poll_table, etc. */
#include <linux/cdev.h> /* cdev_init, etc. */
#include <linux/device.h> /* class_create, device_create, etc */
#include <linux/mm.h>  /* mmap related stuff */

#include "bdbm_drv.h"
#include "debug.h"
#include "params.h"
#include "uthread.h"

#include "dev_params.h"
#include "dev_stub.h"
#include "umemory.h"


/* exported by the device implementation module */
extern bdbm_dm_inf_t _bdbm_dm_inf; 
extern bdbm_drv_info_t* _bdi_dm; 

void __dm_intr_handler (bdbm_drv_info_t* bdi, bdbm_llm_req_t* r);

typedef struct {
	wait_queue_head_t pollwq;
	bdbm_drv_info_t* bdi;
	bdbm_spinlock_t lock;
	uint32_t ref_cnt;

	uint64_t punit;
	uint8_t* punit_done;	/* punit_done is updated only in dm_fops_poll while poll () is calling */
	uint8_t** punit_main_pages;
	uint8_t** punit_oob_pages;

	/* shared by mmap */
	uint8_t* mmap_shared;
	uint64_t mmap_shared_size;

	/* keep the status of reqs */
	bdbm_spinlock_t lock_busy;
	uint8_t* punit_busy;
	bdbm_llm_req_t** kr;
	bdbm_llm_req_ioctl_t** ur;
} bdbm_dm_stub_t;

bdbm_llm_inf_t _bdbm_llm_inf = {
	.ptr_private = NULL,
	.create = NULL,
	.destroy = NULL,
	.make_req = NULL,
	.flush = NULL,
	.end_req = __dm_intr_handler, /* 'dm' automatically calls 'end_req' when it gets acks from devices */
};

void __dm_intr_handler (bdbm_drv_info_t* bdi, bdbm_llm_req_t* r)
{
	bdbm_dm_stub_t* s = (bdbm_dm_stub_t*)bdi->private_data;
	bdbm_phyaddr_t* p = &r->phyaddr;
	uint64_t punit_id = BDBM_GET_PUNIT_ID (bdi, p);

	bdbm_spin_lock (&s->lock_busy);
	if (s->punit_busy[punit_id] != 1) {
		bdbm_spin_unlock (&s->lock_busy);
		/* hmm... this is a serious bug... */
		bdbm_error ("s->punit_busy[punit_id] must be 1 (val = %u)", 
			s->punit_busy[punit_id]);
		bdbm_bug_on (1);
		return;
	}
	s->punit_busy[punit_id] = 2;	/* (2) busy to done */
	bdbm_spin_unlock (&s->lock_busy);

	wake_up_interruptible (&(s->pollwq));
}

/* TODO: We should improve it to use mmap to avoid useless malloc, memcpy, free, etc */
static bdbm_llm_req_t* __get_llm_req (
	bdbm_dm_stub_t* s,
	bdbm_llm_req_ioctl_t __user *ur)
{
	bdbm_device_params_t* np = BDBM_GET_DEVICE_PARAMS(s->bdi);
	bdbm_llm_req_ioctl_t kr;
	bdbm_llm_req_t* r = NULL;
	uint64_t nr_kpages = np->page_main_size / KPAGE_SIZE;
	uint64_t punit_id;
	uint64_t loop;

	if (ur == NULL) {
		bdbm_warning ("user-level llm_req is NULL");
		return NULL;
	}

 	/* is it accessable ?*/
	if (access_ok (VERIFY_READ, ur, sizeof (kr)) != 1) {
		bdbm_warning ("access_ok () failed");
		return NULL;
	}
	copy_from_user (&kr, ur, sizeof (kr));

	/* create bdbm_llm_req_t */
	if ((r = (bdbm_llm_req_t*)bdbm_malloc_atomic (sizeof (bdbm_llm_req_t))) == NULL) {
		bdbm_warning ("bdbm_zmalloc () failed");
		return NULL;
	}

	/* initialize it */
	r->req_type = kr.req_type;
	r->logaddr = kr.logaddr;
	r->phyaddr = kr.phyaddr;
	r->ret = 1;
  	punit_id = BDBM_GET_PUNIT_ID (s->bdi, (&r->phyaddr));
	for (loop = 0; loop < nr_kpages; loop++) {
		r->fmain.kp_stt[loop] = kr.kp_stt[loop];
		r->fmain.kp_ptr[loop] = s->punit_main_pages[punit_id] + (KERNEL_PAGE_SIZE * loop);
	}
	if (bdbm_is_write (r->req_type)) {
		bdbm_memcpy (r->foob.data, s->punit_oob_pages[punit_id], np->page_oob_size);
	}

	return r;
}

static void __return_llm_req (
	bdbm_dm_stub_t* s,
	bdbm_llm_req_ioctl_t* ur,
	bdbm_llm_req_t* kr)
{
	/* copy a retun value */
	if (access_ok (VERIFY_WRITE, ur, sizeof (*ur)) != 1) {
		bdbm_warning ("access_ok () failed");
	} else {
		copy_to_user (&ur->ret, &kr->ret, sizeof (uint8_t));
	}
}

static void __free_llm_req (bdbm_llm_req_t* kr)
{
	bdbm_free_atomic (kr);
}

static int dm_stub_probe (bdbm_dm_stub_t* s)
{
	bdbm_drv_info_t* bdi = s->bdi;

	if (bdi->ptr_dm_inf->probe == NULL) {
		bdbm_warning ("ptr_dm_inf->probe is NULL");
		return -ENOTTY;
	}

	/* call probe; just get NAND parameters from the device */
	if (bdi->ptr_dm_inf->probe (bdi, &bdi->parm_dev) != 0) {
		bdbm_warning ("dm->probe () failed");
		return -EIO;
	} 

	return 0;
}

static int dm_stub_open (bdbm_dm_stub_t* s)
{
	bdbm_drv_info_t* bdi = s->bdi;
	uint64_t mmap_ofs, i;

	if (bdi->ptr_dm_inf->open == NULL) {
		bdbm_warning ("ptr_dm_inf->open is NULL");
		return -ENOTTY;
	}

	/* a big spin-lock; but not a problem with open */
	bdbm_spin_lock (&s->lock);
	bdbm_spin_lock (&s->lock_busy);

	/* are there any other clients? */
	if (s->ref_cnt > 0) {
		bdbm_spin_unlock (&s->lock_busy);
		bdbm_spin_unlock (&s->lock);
		bdbm_warning ("dm_stub is already open for other clients (%u)", s->ref_cnt);
		return -EBUSY;
	} 

	/* initialize internal variables */
	s->punit = bdi->parm_dev.nr_chips_per_channel * bdi->parm_dev.nr_channels;
	s->mmap_shared_size = KPAGE_SIZE + PAGE_ALIGN (
		s->punit * (bdi->parm_dev.page_main_size + bdi->parm_dev.page_oob_size));

	bdbm_msg ("s->punit=%llu, s->mmap_shared_size=%llu", s->punit, s->mmap_shared_size);

	s->kr = (bdbm_llm_req_t**)bdbm_zmalloc (s->punit * sizeof (bdbm_llm_req_t*));
	s->ur = (bdbm_llm_req_ioctl_t**)bdbm_zmalloc (s->punit * sizeof (bdbm_llm_req_ioctl_t*));
	s->punit_busy = (uint8_t*)bdbm_malloc_atomic (s->punit * sizeof (uint8_t));
	s->punit_main_pages = (uint8_t**)bdbm_zmalloc (s->punit * sizeof (uint8_t*));
	s->punit_oob_pages = (uint8_t**)bdbm_zmalloc (s->punit * sizeof (uint8_t*));
	s->mmap_shared = (uint8_t*)bdbm_zmalloc (s->mmap_shared_size);
	for (i = 0; i < s->mmap_shared_size; i+=KPAGE_SIZE)
		SetPageReserved (vmalloc_to_page ((void*)(((unsigned long)s->mmap_shared)+i)));

	/* setup other stuffs */
	mmap_ofs = 0;
	s->punit_done = s->mmap_shared;
	mmap_ofs += KPAGE_SIZE;
	for (i = 0; i < s->punit; i++) {
		s->punit_main_pages[i] = s->mmap_shared + mmap_ofs;
		mmap_ofs += bdi->parm_dev.page_main_size;
		s->punit_oob_pages[i] = s->mmap_shared + mmap_ofs;
		mmap_ofs += bdi->parm_dev.page_oob_size;
	}

	bdbm_msg ("mmap_shared_size = %llu, mmap_ofs = %llu", 
		s->mmap_shared_size, mmap_ofs);

	/* are there any errors? */
	if (s->kr == NULL || 
		s->ur == NULL || 
		s->mmap_shared == NULL ||
		s->punit_busy == NULL || 
		s->punit_main_pages == NULL ||
		s->punit_oob_pages == NULL) {

		if (s->punit_oob_pages) bdbm_free (s->punit_oob_pages);
		if (s->punit_main_pages) bdbm_free (s->punit_main_pages);
		if (s->punit_busy) bdbm_free_atomic (s->punit_busy);
		if (s->kr) bdbm_free (s->kr);
		if (s->ur) bdbm_free (s->ur);
		if (s->mmap_shared) {
			for (i = 0; i < s->mmap_shared_size; i+=KPAGE_SIZE)
				ClearPageReserved (vmalloc_to_page ((void*)(((unsigned long)s->mmap_shared)+i)));
			bdbm_free (s->mmap_shared);
		}

		bdbm_spin_unlock (&s->lock_busy);
		bdbm_spin_unlock (&s->lock);

		bdbm_warning ("bdbm_malloc failed \
			(kr=%p, ur=%p, s->mmap_shared=%p, punit_busy=%p, punit_main_pages=%p, punit_oob_pages=%p)", 
			s->kr, s->ur, s->mmap_shared, 
			s->punit_busy, s->punit_main_pages, s->punit_oob_pages);

		return -EIO;
	}

	/* increase ref_cnt */
	s->ref_cnt = 1;

	bdbm_spin_unlock (&s->lock_busy);
	bdbm_spin_unlock (&s->lock);

	/* call open */
	if (bdi->ptr_dm_inf->open (bdi) != 0) {
		bdbm_warning ("dm->open () failed");
		return -EIO;
	}

	return 0;
}

static int dm_stub_close (bdbm_dm_stub_t* s)
{
	bdbm_drv_info_t* bdi = s->bdi;
	long i;

	if (bdi->ptr_dm_inf->close == NULL) {
		bdbm_warning ("ptr_dm_inf->close is NULL");
		return -ENOTTY;
	}

	bdbm_spin_lock (&s->lock);
	if (s->ref_cnt == 0) {
		bdbm_warning ("oops! bdbm_dm_stub is not open or was already closed");
		bdbm_spin_unlock (&s->lock);
		return -ENOTTY;
	}
	s->ref_cnt = 0;
	bdbm_spin_unlock (&s->lock);

	/* call close */
	bdi->ptr_dm_inf->close (bdi);

	/* free llm_reqs for bdi */
	if (s->punit_oob_pages) bdbm_free (s->punit_oob_pages);
	if (s->punit_main_pages) bdbm_free (s->punit_main_pages);
	if (s->punit_busy) bdbm_free_atomic (s->punit_busy);
	if (s->mmap_shared) {
		for (i = 0; i < s->mmap_shared_size; i+=KPAGE_SIZE)
			ClearPageReserved (vmalloc_to_page ((void*)(((unsigned long)s->mmap_shared)+i)));
		bdbm_free (s->mmap_shared);
	}
	if (s->kr) bdbm_free (s->kr);
	if (s->ur) bdbm_free (s->ur);

	s->mmap_shared = NULL;
	s->punit_done = NULL;
	s->punit_busy = NULL;
	s->kr = NULL;
	s->ur = NULL;

	return 0;
}

static int dm_stub_make_req (
	bdbm_dm_stub_t* s, 
	bdbm_llm_req_ioctl_t __user *ur)
{
	bdbm_drv_info_t* bdi = s->bdi;
	bdbm_llm_req_t* kr = NULL;
	uint32_t punit_id;

	if (bdi->ptr_dm_inf->make_req == NULL) {
		bdbm_warning ("ptr_dm_inf->make_req is NULL");
		return -ENOTTY;
	} 

	/* the current implementation of bdbm_dm stub only supports a single client */
	bdbm_spin_lock (&s->lock);
	if (s->ref_cnt == 0) {
		bdbm_warning ("oops! bdbm_dm_stub is not open");
		bdbm_spin_unlock (&s->lock);
		return -EBUSY;
	}
	bdbm_spin_unlock (&s->lock);

	/* copy user-level llm_req to kernel-level */
	if ((kr = __get_llm_req (s, ur)) == NULL) {
		bdbm_warning ("__get_llm_req () failed (ur=%p, kr=%p)", ur, kr);
		return -EIO;
	}

	/* get punit_id */
	punit_id = BDBM_GET_PUNIT_ID (bdi, (&kr->phyaddr));

	/* see if there is an on-going request */
	bdbm_spin_lock (&s->lock_busy);
	if (s->punit_busy[punit_id] != 0 ||
		s->ur[punit_id] != NULL || 
		s->kr[punit_id] != NULL) {
		bdbm_spin_unlock (&s->lock_busy);
		bdbm_warning ("oops! the punit for the request is busy (punit_id = %u)", punit_id);
		__free_llm_req (kr);
		return -EBUSY;
	}
	s->punit_busy[punit_id] = 1; /* (1) idle to busy */
	s->ur[punit_id] = ur;
	s->kr[punit_id] = kr;
	bdbm_spin_unlock (&s->lock_busy);

	/* call make_req */ 
	if (bdi->ptr_dm_inf->make_req (bdi, kr) != 0) {
		return -EIO;
	}

	return 0;
}

static int dm_stub_end_req (bdbm_dm_stub_t* s)
{
	bdbm_llm_req_t* kr = NULL;
	bdbm_llm_req_ioctl_t* ur = NULL;
	uint64_t i;
	int ret = 1;

	/* see if there are available punits */
	for (i = 0; i < s->punit; i++) {
		/* see if there is a request that ends */
		bdbm_spin_lock (&s->lock_busy);
		if (s->punit_busy[i] != 2) {
			bdbm_spin_unlock (&s->lock_busy);
			continue;
		}
		kr = s->kr[i];
		ur = s->ur[i];
		s->kr[i] = NULL;
		s->ur[i] = NULL;
		s->punit_busy[i] = 0;	/* (3) done to idle */
		bdbm_spin_unlock (&s->lock_busy);
		
		/* let's finish it */
		if (ur != NULL && kr != NULL) {
			/* copy oob; setup results; and destroy a kernel copy */
			if (bdbm_is_read (kr->req_type))
				bdbm_memcpy (s->punit_oob_pages[i], kr->foob.data, s->bdi->parm_dev.page_oob_size);
			__return_llm_req (s, ur, kr);
			__free_llm_req (kr);

			/* done */
			s->punit_done[i] = 1; /* don't need to use a lock for this */
			ret = 0;
		} else {
			bdbm_error ("hmm... this is impossible");
		}
	}

	return ret;
}


/*
 * For the interaction with user-level application
 */
static long dm_fops_ioctl (struct file *filp, unsigned int cmd, unsigned long arg);
static unsigned int dm_fops_poll (struct file *filp, poll_table *poll_table);
static void mmap_open (struct vm_area_struct *vma);
static void mmap_close (struct vm_area_struct *vma);
static int dm_fops_mmap (struct file *filp, struct vm_area_struct *vma);
static int dm_fops_create (struct inode *inode, struct file *filp);
static int dm_fops_release (struct inode *inode, struct file *filp);

/* Linux Device Drivers, 3rd Edition, (CHAP 15 / SEC 2): http://www.makelinux.net/ldd3/chp-15-sect-2 */
/* arch/powerpc/kernel/proc_powerpc.c */
/* arch/powerpc/kernel/rtas_flash.c */
struct vm_operations_struct mmap_vm_ops = {
	.open = mmap_open,
	.close = mmap_close,
};

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.mmap = dm_fops_mmap, /* TODO: implement mmap later to avoid usless operations */
	.open = dm_fops_create,
	.release = dm_fops_release,
	.poll = dm_fops_poll,
	.unlocked_ioctl = dm_fops_ioctl,
	.compat_ioctl = dm_fops_ioctl,
};

void mmap_open (struct vm_area_struct *vma)
{
	bdbm_msg ("mmap_open: virt %lx, phys %lx",
		vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

void mmap_close (struct vm_area_struct *vma)
{
	bdbm_msg ("mmap_close");
}

static int dm_fops_mmap (struct file *filp, struct vm_area_struct *vma)
{
	bdbm_dm_stub_t* s = filp->private_data;
	uint64_t size = vma->vm_end - vma->vm_start;
 	unsigned long pfn, start = vma->vm_start;
	char *vmalloc_addr = (char *)s->mmap_shared;

	if (s == NULL) {
		bdbm_warning ("dm_stub is not created yet");
		return -EINVAL;
	}

	if (size > s->mmap_shared_size) {
		bdbm_warning ("size > s->mmap_shared_size: %llu > %llu", size, s->mmap_shared_size);
		return -EINVAL;
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
	vma->vm_private_data = s;	/* bdbm_dm_stub_t */
	mmap_open (vma);

	bdbm_msg ("dm_fops_mmap is called (%lu)", vma->vm_end - vma->vm_start);

	return 0;
}


static int dm_fops_create (struct inode *inode, struct file *filp)
{
	bdbm_dm_stub_t* s = (bdbm_dm_stub_t*)filp->private_data;

	/* see if bdbm_dm is already used by other applications */
	if (_bdi_dm != NULL) {
		bdbm_error ("bdbm_dm is already used by other applications (_bdi_dm = %p)", _bdi_dm);
		return -EBUSY;
	}

	/* see if bdbm_dm_stub is already created */
	if (s != NULL) {
		bdbm_error ("bdbm_dm_stub is already created; duplicate open is not allowed (s = %p)", s);
		return -EBUSY;
	}

	/* create bdbm_dm_stub with zeros */
	if ((s = (bdbm_dm_stub_t*)bdbm_zmalloc (sizeof (bdbm_dm_stub_t))) == NULL) {
		bdbm_error ("bdbm_malloc failed");
		return -EIO;
	}
	init_waitqueue_head (&s->pollwq);
	bdbm_spin_lock_init (&s->lock);
	bdbm_spin_lock_init (&s->lock_busy);
	s->ref_cnt = 0;

	/* create bdi with zeros */
	if ((s->bdi = (bdbm_drv_info_t*)bdbm_zmalloc 
			(sizeof (bdbm_drv_info_t))) == NULL) {
		bdbm_error ("bdbm_malloc failed");
		return -EIO;
	} 
	s->bdi->private_data = (void*)s;
	s->bdi->ptr_llm_inf = &_bdbm_llm_inf;	/* register interrupt handler */
	s->bdi->ptr_dm_inf = &_bdbm_dm_inf;	/* register dm handler */

	/* assign bdbm_dm_stub to private_data */
	filp->private_data = (void *)s;
	filp->f_mode |= FMODE_WRITE;

	_bdi_dm = s->bdi;

	bdbm_msg ("dm_fops_create is done");

	return 0;
}

static int dm_fops_release (struct inode *inode, struct file *filp)
{
	bdbm_dm_stub_t* s = (bdbm_dm_stub_t*)filp->private_data;

	/* bdbm_dm_stub is not open before */
	if (s == NULL) {
		bdbm_warning ("attempt to close dm_stub which was not open");
		return 0;
	}

	/* it is not necessary when dm_stub is nicely closed,
	 * bit it is required when a client is crashed */
	dm_stub_close (s);

	/* free some variables */
	bdbm_spin_lock_destory (&s->lock_busy);
	bdbm_spin_lock_destory (&s->lock);
	init_waitqueue_head (&s->pollwq);
	if (s->bdi != NULL) {
		s->bdi->ptr_dm_inf = NULL;
		bdbm_free (s->bdi);
	}
	bdbm_free (s);

	/* reset private_data */
	filp->private_data = (void *)NULL;
	_bdi_dm = NULL;

	bdbm_msg ("dm_fops_release is done");

	return 0;
}

static unsigned int dm_fops_poll (struct file *filp, poll_table *poll_table)
{
	bdbm_dm_stub_t* s = (bdbm_dm_stub_t*)filp->private_data;
	unsigned int mask = 0;

	if (s == NULL) {
		bdbm_error ("bdbm_dm_stub is not created");
		return 0;
	}

	/*bdbm_msg ("dm_fops_poll is called");*/

	poll_wait (filp, &s->pollwq, poll_table);

	/* see if there are requests that already finished */
	if (dm_stub_end_req (s) == 0) {
		mask |= POLLIN | POLLRDNORM; 
	}

	/*bdbm_msg ("dm_fops_poll is finished");*/

	return mask;
}

static long dm_fops_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
	bdbm_dm_stub_t* s = (bdbm_dm_stub_t*)filp->private_data;
	int ret = 0;

	/* see if bdbm_dm_study is valid or not */
	if (s == NULL) {
		bdbm_error ("bdbm_dm_stub is not created");
		return -ENOTTY;
	}

	/* handle a command from user applications */
	switch (cmd) {
	case BDBM_DM_IOCTL_PROBE:
		if ((ret = dm_stub_probe (s)) == 0)
			copy_to_user ((bdbm_device_params_t*)arg, &s->bdi->parm_dev, sizeof (bdbm_device_params_t));
		break;
	case BDBM_DM_IOCTL_OPEN:
		ret = dm_stub_open (s);
		break;
	case BDBM_DM_IOCTL_CLOSE:
		ret = dm_stub_close (s);
		break;
	case BDBM_DM_IOCTL_MAKE_REQ:
		ret = dm_stub_make_req (s, (bdbm_llm_req_ioctl_t __user*)arg);
		break;
	case BDBM_DM_IOCTL_END_REQ:
		bdbm_warning ("Hmm... dm_stub_end_req () cannot be directly called by user applications");
		ret = -ENOTTY;
		break;
	case BDBM_DM_IOCTL_LOAD:
		bdbm_warning ("dm_stub_load () is not implemented yet");
		ret = -ENOTTY;
		break;
	case BDBM_DM_IOCTL_STORE:
		bdbm_warning ("dm_stub_store () is not implemented yet");
		ret = -ENOTTY;
		break;
	default:
		bdbm_warning ("invalid command code");
		ret = -ENOTTY;
	}

	return ret;
}


/*
 * For the registration of dm_stub as a character device
 */
static dev_t devnum = 0; 
static struct cdev c_dev;
static struct class *cl = NULL;
static int FIRST_MINOR = 0;
static int MINOR_CNT = 1;

/* register a bdbm_dm_stub driver */
int bdbm_dm_stub_init (void)
{
	int ret = -1;
	struct device *dev_ret = NULL;

	if ((ret = alloc_chrdev_region (&devnum, FIRST_MINOR, MINOR_CNT, BDBM_DM_IOCTL_NAME)) != 0) {
		bdbm_error ("bdbm_dm_stub registration failed: %d\n", ret);
		return ret;
	}
	cdev_init (&c_dev, &fops);

	if ((ret = cdev_add (&c_dev, devnum, MINOR_CNT)) < 0) {
		bdbm_error ("bdbm_dm_stub registration failed: %d\n", ret);
		return ret;
	}

	if (IS_ERR (cl = class_create (THIS_MODULE, "char"))) {
		bdbm_error ("bdbm_dm_stub registration failed: %d\n", MAJOR(devnum));
		cdev_del (&c_dev);
		unregister_chrdev_region (devnum, MINOR_CNT);
		return PTR_ERR (cl);
	}

	if (IS_ERR (dev_ret = device_create (cl, NULL, devnum, NULL, BDBM_DM_IOCTL_NAME))) {
		bdbm_error ("bdbm_dm_stub registration failed: %d\n", MAJOR(devnum));
		class_destroy (cl);
		cdev_del (&c_dev);
		unregister_chrdev_region (devnum, MINOR_CNT);
		return PTR_ERR (dev_ret);
	}
	
	bdbm_msg ("bdbm_dm_stub is installed: %s (major:%d minor:%d)", 
		BDBM_DM_IOCTL_DEVNAME, 
		MAJOR(devnum), MINOR(devnum));

	return 0;
}

/* remove a bdbm_db_stub driver */
void bdbm_dm_stub_exit (void)
{
	if (cl == NULL || devnum == 0) {
		bdbm_warning ("bdbm_dm_stub is not installed yet");
		return;
	}

	/* get rid of bdbm_dm_stub */
	device_destroy (cl, devnum);
    class_destroy (cl);
    cdev_del (&c_dev);
    unregister_chrdev_region (devnum, MINOR_CNT);

	bdbm_msg ("bdbm_dm_stub is removed: %s (%d %d)", 
		BDBM_DM_IOCTL_DEVNAME, 
		MAJOR(devnum), MINOR(devnum));
}

