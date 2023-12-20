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
#include <pthread.h>

#include "bdbm_drv.h"
#include "debug.h"
#include "umemory.h"
#include "params.h"
#include "utime.h"

#include "dm_dummy.h"
#include "dev_params.h"


bdbm_dm_inf_t _bdbm_dm_inf = {
	.ptr_private = NULL,
	.probe = dm_user_probe,
	.open = dm_user_open,
	.close = dm_user_close,
	.make_req = dm_user_make_req,
	.end_req = dm_user_end_req,
	.load = dm_user_load,
	.store = dm_user_store,
};

/* private data structure for dm */
struct dm_user_private {
	bdbm_spinlock_t dm_lock;
	uint64_t w_cnt;
	uint64_t w_cnt_done;
};

static void __dm_setup_device_params (bdbm_device_params_t* params)
{
#if 0
	/* user-specified parameters */
	params->nr_channels = _param_nr_channels;
	params->nr_chips_per_channel = _param_nr_chips_per_channel;
	params->nr_blocks_per_chip = _param_nr_blocks_per_chip;
	params->nr_pages_per_block = _param_nr_pages_per_block;
	params->page_main_size = _param_page_main_size;
	params->page_oob_size = _param_page_oob_size;
	params->device_type = _param_device_type;
	params->page_prog_time_us = _param_page_prog_time_us;
	params->page_read_time_us = _param_page_read_time_us;
	params->block_erase_time_us = _param_block_erase_time_us;
	/*params->timing_mode = _param_ramdrv_timing_mode;*/

	/* other parameters derived from user parameters */
	params->nr_blocks_per_channel = 
		params->nr_chips_per_channel * 
		params->nr_blocks_per_chip;

	params->nr_blocks_per_ssd = 
		params->nr_channels * 
		params->nr_chips_per_channel * 
		params->nr_blocks_per_chip;

	params->nr_chips_per_ssd =
		params->nr_channels * 
		params->nr_chips_per_channel;

	params->nr_pages_per_ssd =
		params->nr_pages_per_block * 
		params->nr_blocks_per_ssd;

	params->device_capacity_in_byte = 0;
	params->device_capacity_in_byte += params->nr_channels;
	params->device_capacity_in_byte *= params->nr_chips_per_channel;
	params->device_capacity_in_byte *= params->nr_blocks_per_chip;
	params->device_capacity_in_byte *= params->nr_pages_per_block;
	params->device_capacity_in_byte *= params->page_main_size;
#endif
	*params = get_default_device_params ();
}

uint32_t dm_user_probe (bdbm_drv_info_t* bdi, bdbm_device_params_t* params)
{
	struct dm_user_private* p = NULL;

	/* setup NAND parameters according to users' inputs */
	__dm_setup_device_params (params);

	/* create a private structure for ramdrive */
	if ((p = (struct dm_user_private*)bdbm_malloc_atomic
			(sizeof (struct dm_user_private))) == NULL) {
		bdbm_error ("bdbm_malloc_atomic failed");
		goto fail;
	}

	/* initialize some variables */
	bdbm_spin_lock_init (&p->dm_lock);
	p->w_cnt = 0;
	p->w_cnt_done = 0;

	/* OK! keep private info */
	bdi->ptr_dm_inf->ptr_private = (void*)p;

	return 0;

fail:
	return -1;
}

uint32_t dm_user_open (bdbm_drv_info_t* bdi)
{
	struct dm_user_private * p;

	p = (struct dm_user_private*)bdi->ptr_dm_inf->ptr_private;

	bdbm_msg ("dm_user_open is initialized");

	return 0;
}

void dm_user_close (bdbm_drv_info_t* bdi)
{
	struct dm_user_private* p; 

	p = (struct dm_user_private*)bdi->ptr_dm_inf->ptr_private;

	bdbm_msg ("dm_user: w_cnt = %llu, w_cnt_done = %llu", p->w_cnt, p->w_cnt_done);
	bdbm_msg ("dm_user_close is destroyed");

	bdbm_free_atomic (p);
}

uint32_t dm_user_make_req (bdbm_drv_info_t* bdi, bdbm_llm_req_t* ptr_llm_req)
{
	struct dm_user_private* p; 

	p = (struct dm_user_private*)bdi->ptr_dm_inf->ptr_private;

	/*TODO: do somthing */
	bdbm_spin_lock (&p->dm_lock);
	p->w_cnt++;
	bdbm_spin_unlock (&p->dm_lock);

	dm_user_end_req (bdi, ptr_llm_req);

	return 0;
}

void dm_user_end_req (bdbm_drv_info_t* bdi, bdbm_llm_req_t* ptr_llm_req)
{
	struct dm_user_private* p; 

	p = (struct dm_user_private*)bdi->ptr_dm_inf->ptr_private;

	bdbm_spin_lock (&p->dm_lock);
	p->w_cnt_done++;
	bdbm_spin_unlock (&p->dm_lock);

	bdi->ptr_llm_inf->end_req (bdi, ptr_llm_req);
}

/* for snapshot */
uint32_t dm_user_load (bdbm_drv_info_t* bdi, const char* fn)
{	
	struct dm_user_private * p = 
		(struct dm_user_private*)bdi->ptr_dm_inf->ptr_private;

	bdbm_msg ("loading a DRAM snapshot...");

	return 0;
}

uint32_t dm_user_store (bdbm_drv_info_t* bdi, const char* fn)
{
	struct dm_user_private * p = 
		(struct dm_user_private*)bdi->ptr_dm_inf->ptr_private;

	bdbm_msg ("storing a DRAM snapshot...");

	return 0;
}
