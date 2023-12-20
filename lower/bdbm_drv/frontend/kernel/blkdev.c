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

#include <linux/module.h>
#include <linux/blkdev.h> /* bio */
#include <linux/hdreg.h>
#include <linux/kthread.h>
#include <linux/delay.h> /* mdelay */

#include "bdbm_drv.h"
#include "debug.h"
#include "blkdev.h"
#include "blkdev_ioctl.h"
#include "umemory.h"


int bdbm_blk_ioctl (struct block_device *bdev, fmode_t mode, unsigned cmd, unsigned long arg);
int bdbm_blk_getgeo (struct block_device *bdev, struct hd_geometry* geo);

static struct bdbm_device_t {
	struct gendisk *gd;
	struct request_queue *queue;
} bdbm_device;

static uint32_t bdbm_device_major_num = 0;
static struct block_device_operations bdops = {
	.owner = THIS_MODULE,
	.ioctl = bdbm_blk_ioctl,
	.getgeo = bdbm_blk_getgeo,
};

extern bdbm_drv_info_t* _bdi;

DECLARE_COMPLETION (task_completion);
static struct task_struct *task = NULL;


int badblock_scan_thread_fn (void* arg) 
{
	bdbm_ftl_inf_t* ftl = NULL;
	uint32_t ret;

	/* get the ftl */
	if ((ftl = _bdi->ptr_ftl_inf) == NULL) {
		bdbm_warning ("ftl is not created");
		goto exit;
	}

	/* run the bad-block scan */
	if ((ret = ftl->scan_badblocks (_bdi))) {
		bdbm_msg ("scan_badblocks failed (%u)", ret);
	}

exit:
	complete (&task_completion);
	return 0;
}

int bdbm_blk_getgeo (struct block_device *bdev, struct hd_geometry* geo)
{
	bdbm_device_params_t* np = BDBM_GET_DEVICE_PARAMS (_bdi);
	int nr_sectors = np->device_capacity_in_byte >> 9;

	/* NOTE: Heads * Cylinders * Sectors = # of sectors (512B) in SSDs */
	geo->heads = 16;
	geo->cylinders = 1024;
	geo->sectors = nr_sectors / (geo->heads * geo->cylinders);
	if (geo->heads * geo->cylinders * geo->sectors != nr_sectors) {
		bdbm_warning ("bdbm_blk_getgeo: heads=%d, cylinders=%d, sectors=%d (total sectors=%d)",
			geo->heads, 
			geo->cylinders, 
			geo->sectors,
			nr_sectors);
		return 1;
	}
	return 0;
}

int bdbm_blk_ioctl (
	struct block_device *bdev, 
	fmode_t mode, 
	unsigned cmd, 
	unsigned long arg)
{
	struct hd_geometry geo;
	struct gendisk *disk = bdev->bd_disk;
	int ret;

	switch (cmd) {
	case HDIO_GETGEO:
	case HDIO_GETGEO_BIG:
	case HDIO_GETGEO_BIG_RAW:
		if (!arg) {
			bdbm_warning ("invalid argument");
			return -EINVAL;
		}
		if (!disk->fops->getgeo) {
			bdbm_warning ("disk->fops->getgeo is NULL");
			return -ENOTTY;
		}

		bdbm_memset(&geo, 0, sizeof(geo));
		geo.start = get_start_sect(bdev);
		ret = disk->fops->getgeo(bdev, &geo);
		if (ret) {
			bdbm_warning ("disk->fops->getgeo returns (%d)", ret);
			return ret;
		}
		if (copy_to_user((struct hd_geometry __user *)arg, &geo, sizeof(geo))) {
			bdbm_warning ("copy_to_user failed");
			return -EFAULT;
		}
		break;

	case BDBM_BADBLOCK_SCAN:
		bdbm_msg ("Get a BDBM_BADBLOCK_SCAN command: %u (%X)", cmd, cmd);

		if (task != NULL) {
			bdbm_msg ("badblock_scan_thread is running");
		} else {
			/* create thread */
			if ((task = kthread_create (badblock_scan_thread_fn, NULL, "badblock_scan_thread")) == NULL) {
				bdbm_msg ("badblock_scan_thread failed to create");
			} else {
				wake_up_process (task);
			}
		}
		break;

	case BDBM_BADBLOCK_SCAN_CHECK:
		/* check the status of the thread */
		if (task == NULL) {
			bdbm_msg ("badblock_scan_thread is not created...");
			ret = 1; /* done */
			copy_to_user ((int*)arg, &ret, sizeof (int));
			break;
		}

		/* is it still running? */
		if (!bdbm_try_wait_for_completion (task_completion)) {
			ret = 0; /* still running */
			copy_to_user ((int*)arg, &ret, sizeof (int));
			break;
		}
		ret = 1; /* done */
		
		/* reinit some variables */
		task = NULL;
		copy_to_user ((int*)arg, &ret, sizeof (int));
		bdbm_reinit_completion (task_completion);
		break;

#if 0
	case BDBM_GET_PHYADDR:
		break;
#endif

	default:
		/*bdbm_msg ("unknown bdm_blk_ioctl: %u (%X)", cmd, cmd);*/
		break;
	}

	return 0;
}

uint32_t host_blkdev_register_device (bdbm_drv_info_t* bdi, make_request_fn* fn)
{
	/* create a blk queue */
	if (!(bdbm_device.queue = blk_alloc_queue (GFP_KERNEL))) {
		bdbm_error ("blk_alloc_queue failed");
		return -ENOMEM;
	}
	blk_queue_make_request (bdbm_device.queue, fn);
	blk_queue_logical_block_size (bdbm_device.queue, bdi->parm_ftl.kernel_sector_size);
	blk_queue_io_min (bdbm_device.queue, bdi->parm_dev.page_main_size);
	blk_queue_io_opt (bdbm_device.queue, bdi->parm_dev.page_main_size);
	/*blk_limits_max_hw_sectors (&bdbm_device.queue->limits, 16);*/

	/* see if a TRIM command is used or not */
	if (bdi->parm_ftl.trim == TRIM_ENABLE) {
		bdbm_device.queue->limits.discard_granularity = KERNEL_PAGE_SIZE;
		bdbm_device.queue->limits.max_discard_sectors = UINT_MAX;
		/*bdbm_device.queue->limits.discard_zeroes_data = 1;*/
		queue_flag_set_unlocked (QUEUE_FLAG_DISCARD, bdbm_device.queue);
		bdbm_msg ("TRIM is enabled");
	} else {
		bdbm_msg ("TRIM is disabled");
	}

	/* register a blk device */
	if ((bdbm_device_major_num = register_blkdev (bdbm_device_major_num, "blueDBM")) < 0) {
		bdbm_msg ("register_blkdev failed (%d)", bdbm_device_major_num);
		return bdbm_device_major_num;
	}
	if (!(bdbm_device.gd = alloc_disk (1))) {
		bdbm_msg ("alloc_disk failed");
		unregister_blkdev (bdbm_device_major_num, "blueDBM");
		return -ENOMEM;
	}
	bdbm_device.gd->major = bdbm_device_major_num;
	bdbm_device.gd->first_minor = 0;
	bdbm_device.gd->fops = &bdops;
	bdbm_device.gd->queue = bdbm_device.queue;
	bdbm_device.gd->private_data = NULL;
	strcpy (bdbm_device.gd->disk_name, "blueDBM");

	{
		uint64_t capacity;
		//capacity = bdi->parm_dev.device_capacity_in_byte * 0.9;
		/*capacity = bdi->parm_dev.device_capacity_in_byte;*/
	//	capacity = (capacity / KERNEL_PAGE_SIZE) * KERNEL_PAGE_SIZE;
		set_capacity (bdbm_device.gd, capacity / KERNEL_SECTOR_SIZE);
	}
	add_disk (bdbm_device.gd);

	return 0;
}

void host_blkdev_unregister_block_device (bdbm_drv_info_t* bdi)
{
	/* unregister a BlueDBM device driver */
	del_gendisk (bdbm_device.gd);
	put_disk (bdbm_device.gd);
	unregister_blkdev (bdbm_device_major_num, "blueDBM");
	blk_cleanup_queue (bdbm_device.queue);
}

