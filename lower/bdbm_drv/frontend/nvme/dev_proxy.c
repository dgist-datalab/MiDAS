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
#include <errno.h>	/* strerr, errno */
#include <fcntl.h> /* O_RDWR */
#include <unistd.h> /* close */
#include <poll.h> /* poll */
#include <sys/mman.h> /* mmap */

#include "bdbm_drv.h"
#include "debug.h"
#include "umemory.h"
#include "userio.h"
#include "params.h"

#include "dev_proxy.h"

#include "utime.h"
#include "uthread.h"

#include "dm_df.h"


/* interface for dm */
bdbm_dm_inf_t _bdbm_dm_inf = {
	.ptr_private = NULL,
	.probe = dm_proxy_probe,
	.open = dm_proxy_open,
	.close = dm_proxy_close,
	.make_req = dm_proxy_make_req,
	.make_reqs = dm_proxy_make_reqs,
	.end_req = dm_proxy_end_req,
	.load = NULL,
	.store = NULL,
};

typedef struct {
	/* nothing now */
	int test;
} bdbm_dm_proxy_t;


uint32_t dm_proxy_probe (
	bdbm_drv_info_t* bdi, 
	bdbm_device_params_t* params)
{
	bdbm_dm_proxy_t* p = (bdbm_dm_proxy_t*)BDBM_DM_PRIV(bdi);
	int ret;

	bdbm_msg ("dm_proxy_probe is called");

	ret = dm_df_probe (bdi, params);

	bdbm_msg ("--------------------------------");
	bdbm_msg ("probe (): ret = %d", ret);
	bdbm_msg ("nr_channels: %u", (uint32_t)params->nr_channels);
	bdbm_msg ("nr_chips_per_channel: %u", (uint32_t)params->nr_chips_per_channel);
	bdbm_msg ("nr_blocks_per_chip: %u", (uint32_t)params->nr_blocks_per_chip);
	bdbm_msg ("nr_pages_per_block: %u", (uint32_t)params->nr_pages_per_block);
	bdbm_msg ("page_main_size: %u", (uint32_t)params->page_main_size);
	bdbm_msg ("page_oob_size: %u", (uint32_t)params->page_oob_size);
	bdbm_msg ("device_type: %u", (uint32_t)params->device_type);
	bdbm_msg ("");

	return ret;
}

uint32_t dm_proxy_open (bdbm_drv_info_t* bdi)
{
	bdbm_msg ("dm_proxy_open is called");
	return dm_df_open (bdi);
}

void dm_proxy_close (bdbm_drv_info_t* bdi)
{
	bdbm_dm_proxy_t* p = (bdbm_dm_proxy_t*)BDBM_DM_PRIV(bdi);

	bdbm_msg ("dm_proxy_close is called");
	return dm_df_close (bdi);
}

uint32_t dm_proxy_make_req (bdbm_drv_info_t* bdi, bdbm_llm_req_t* r)
{
	bdbm_dm_proxy_t* p = (bdbm_dm_proxy_t*)BDBM_DM_PRIV(bdi);

	if (bdbm_is_read (r->req_type)) {
		bdbm_msg ("dm_proxy_make_req: \t[R] logical:%llu <= physical:%llu %llu %llu %llu",
			r->logaddr.lpa[0], 
			r->phyaddr.channel_no, 
			r->phyaddr.chip_no, 
			r->phyaddr.block_no, 
			r->phyaddr.page_no);
	} else if (bdbm_is_write (r->req_type)) {
		bdbm_msg ("dm_proxy_make_req: \t[W] logical:%llu => physical:%llu %llu %llu %llu",
			r->logaddr.lpa[0], 
			r->phyaddr.channel_no, 
			r->phyaddr.chip_no, 
			r->phyaddr.block_no, 
			r->phyaddr.page_no);
	} else {
		bdbm_msg ("invalid req_type");
	}

	return dm_df_make_req (bdi, r);
}

uint32_t dm_proxy_make_reqs (bdbm_drv_info_t* bdi, bdbm_hlm_req_t* r)
{
	bdbm_msg ("dm_proxy_make_reqs ()");
	return dm_df_make_reqs (bdi, r);
}

void dm_proxy_end_req (bdbm_drv_info_t* bdi, bdbm_llm_req_t* r)
{
	bdbm_dm_proxy_t* p = (bdbm_dm_proxy_t*)BDBM_DM_PRIV(bdi);

	bdbm_msg ("dm_proxy_end_req is called");
	bdi->ptr_llm_inf->end_req (bdi, r);
}


/* functions exported to clients */
int bdbm_dm_init (bdbm_drv_info_t* bdi)
{
	bdbm_dm_proxy_t* p = NULL;

	if (_bdbm_dm_inf.ptr_private != NULL) {
		bdbm_warning ("_bdbm_dm_inf is already open");
		return 1;
	}

	/* create and initialize private variables */
	if ((p = (bdbm_dm_proxy_t*)bdbm_zmalloc 
			(sizeof (bdbm_dm_proxy_t))) == NULL) {
		bdbm_error ("bdbm_malloc failed");
		return 1;
	}

	_bdbm_dm_inf.ptr_private = (void*)p;

	bdbm_msg ("%p", p);

	return 0;
}

bdbm_dm_inf_t* bdbm_dm_get_inf (bdbm_drv_info_t* bdi)
{
	return &_bdbm_dm_inf;
}

void bdbm_dm_exit (bdbm_drv_info_t* bdi) 
{
	bdbm_dm_proxy_t* p = (bdbm_dm_proxy_t*)BDBM_DM_PRIV(bdi);

	if (p == NULL) {
		bdbm_warning ("_bdbm_dm_inf is already closed");
		return;
	}

	bdbm_free (p);
}

