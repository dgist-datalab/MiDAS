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

#include "bdbm_drv.h"
#include "umemory.h"
#include "params.h"
#include "debug.h"

#include "ftl_params.h"
#include "../devices/common/dev_params.h"

#include "llm_noq.h"
#include "llm_mq.h"
#include "hlm_nobuf.h"
#include "hlm_buf.h"
#include "hlm_dftl.h"
#include "hlm_rsd.h"
#include "devices.h"
#include "pmu.h"

#include "algo/no_ftl.h"
#include "algo/block_ftl.h"
#include "algo/page_ftl.h"
#include "algo/dftl.h"
#include "ufile.h"

/* TEMP */
//bdbm_ftl_inf_t _ftl_block_ftl, _ftl_dftl, _ftl_no_ftl;
bdbm_ftl_inf_t _ftl_dftl, _ftl_no_ftl;
bdbm_hlm_inf_t _hlm_dftl_inf, _hlm_buf_inf;
bdbm_llm_inf_t _llm_noq_inf;
/* TEMP */

/* It creates bdi and setups bdi with default parameters.  Users changes the
 * parameters before calling bdbm_drv_initialize () */
bdbm_drv_info_t* bdbm_drv_create (void)
{
	bdbm_drv_info_t* bdi = NULL;

	/* allocate the memory for bdbm_drv_info_t */
	if ((bdi = (bdbm_drv_info_t*)bdbm_malloc (sizeof (bdbm_drv_info_t))) == NULL) {
		bdbm_error ("[bdbm_drv_main] bdbm_malloc () failed");
		return NULL;
	}

	/* get default driver paramters */
	bdi->parm_ftl = get_default_ftl_params ();
	bdi->parm_dev = get_default_device_params ();

	return bdi;
}

int bdbm_drv_setup (
	bdbm_drv_info_t* bdi, 
	bdbm_host_inf_t* host_inf, 
	bdbm_dm_inf_t* dm_inf)
{
	if (bdi == NULL) {
		bdbm_warning ("oops! bdi is NULL");
		return 1;
	}

	/* setup host */
	bdi->ptr_host_inf = host_inf;

	/* setup device */
	bdi->ptr_dm_inf = dm_inf;

	/* setup ftl */
	switch (bdi->parm_ftl.hlm_type) {
	case HLM_NOT_SPECIFIED:
		bdi->ptr_hlm_inf = NULL;
		break;
	case HLM_NO_BUFFER:
		bdi->ptr_hlm_inf = &_hlm_nobuf_inf;
		break;
	case HLM_BUFFER:
		bdi->ptr_hlm_inf = &_hlm_buf_inf;
		break;
	case HLM_DFTL:
		bdi->ptr_hlm_inf = &_hlm_dftl_inf;
		break;
	default:
		bdbm_error ("invalid hlm type");
		bdbm_bug_on (1);
		break;
	}

	switch (bdi->parm_ftl.llm_type) {
	case LLM_NOT_SPECIFIED:
		bdi->ptr_llm_inf = NULL;
		break;
	case LLM_NO_QUEUE:
		bdi->ptr_llm_inf = &_llm_noq_inf;
		break;
	case LLM_MULTI_QUEUE:
		bdi->ptr_llm_inf = &_llm_mq_inf;
		break;
	default:
		bdbm_error ("invalid llm type");
		bdbm_bug_on (1);
		break;
	}

	switch (bdi->parm_ftl.mapping_type) {
	case MAPPING_POLICY_NOT_SPECIFIED:
		bdi->ptr_ftl_inf = NULL;
		break;
	case MAPPING_POLICY_NO_FTL:
		bdi->ptr_ftl_inf = &_ftl_no_ftl;
		break;
	case MAPPING_POLICY_BLOCK:
		bdi->ptr_ftl_inf = &_ftl_block_ftl;
		break;
	case MAPPING_POLICY_RSD:
		bdi->ptr_ftl_inf = &_ftl_block_ftl;
		break;
	case MAPPING_POLICY_PAGE:
		bdi->ptr_ftl_inf = &_ftl_page_ftl;
		break;
	case MAPPING_POLICY_DFTL:
		bdi->ptr_ftl_inf = &_ftl_dftl;
		break;
	default:
		bdbm_error ("invalid ftl type");
		bdbm_bug_on (1);
		break;
	}

	return 0;
}

/* run all the layers related to bdbm_drv */
int bdbm_drv_run (bdbm_drv_info_t* bdi)
{
	bdbm_host_inf_t* host = NULL; 
	bdbm_hlm_inf_t* hlm = NULL;
	bdbm_llm_inf_t* llm = NULL;
	bdbm_ftl_inf_t* ftl = NULL;
	bdbm_dm_inf_t* dm = NULL;
	uint32_t load = 0;

	/* run setup functions */
	if (bdi->ptr_dm_inf) {
		dm = bdi->ptr_dm_inf;

		/* get the device information */
		if (dm->probe == NULL || dm->probe (bdi, &bdi->parm_dev) != 0) {
			bdbm_error ("[bdbm_drv_main] failed to probe a flash device");
			goto fail;
		}
		/* open a flash device */
		if (dm->open == NULL || dm->open (bdi) != 0) {
			bdbm_error ("[bdbm_drv_main] failed to open a flash device");
			goto fail;
		}
		/* do we need to read a snapshot? */
		if (bdi->parm_ftl.snapshot == SNAPSHOT_ENABLE &&
			dm->load != NULL) {
			if (dm->load (bdi, "/usr/share/bdbm_drv/dm.dat") != 0) {
				bdbm_msg ("[bdbm_drv_main] loading 'dm.dat' failed");
				load = 0;
			} else 
				load = 1;
		}
	}

	/* create a low-level memory manager */
	if (bdi->ptr_llm_inf) {
		llm = bdi->ptr_llm_inf;
		if (llm->create == NULL || llm->create (bdi) != 0) {
			bdbm_error ("[bdbm_drv_main] failed to create llm");
			goto fail;
		}
	}

	/* create a logical-to-physical mapping manager */
	if (bdi->ptr_ftl_inf) {
		ftl = bdi->ptr_ftl_inf;
		if (ftl->create == NULL || ftl->create (bdi) != 0) {
			bdbm_error ("[bdbm_drv_main] failed to create ftl");
			goto fail;
		}
		if (bdi->parm_ftl.snapshot == SNAPSHOT_ENABLE &&
			load == 1 && ftl->load != NULL) {
			if (ftl->load (bdi, "/usr/share/bdbm_drv/ftl.dat") != 0) {
				bdbm_msg ("[bdbm_drv_main] loading 'ftl.dat' failed");
			}
		}
	}

	/* create a high-level memory manager */
	if (bdi->ptr_hlm_inf) {
		hlm = bdi->ptr_hlm_inf;
		if (hlm->create == NULL || hlm->create (bdi) != 0) {
			bdbm_error ("[bdbm_drv_main] failed to create hlm");
			goto fail;
		}
	}

	/* create a host interface */
	if (bdi->ptr_host_inf) {
		host = bdi->ptr_host_inf;
		if (host->open == NULL || host->open (bdi) != 0) {
			bdbm_error ("[bdbm_drv_main] failed to open a host interface");
			goto fail;
		}
	}

	/* display default parameters */
	display_device_params (&bdi->parm_dev);
	display_ftl_params (&bdi->parm_ftl);

	/* init performance monitor */
	pmu_create (bdi);

	bdbm_msg ("[bdbm_drv_main] bdbm_drv is registered!");

	return 0;

fail:
	if (host && host->close)
		host->close (bdi);
	if (hlm && hlm->destroy)
		hlm->destroy (bdi);
	if (ftl && ftl->destroy)
		ftl->destroy (bdi);
	if (llm && llm->destroy)
		llm->destroy (bdi);
	if (dm && dm->close)
		dm->close (bdi);
	if (bdi)
		bdbm_free (bdi);
	
	bdbm_error ("[bdbm_drv_main] bdbm_drv failed!");

	return 1;
}

void bdbm_drv_close (bdbm_drv_info_t* bdi)
{
	/* is bdi valid? */
	if (bdi == NULL) {
		bdbm_error ("[bdbm_drv_main] bdi is NULL");
		return;
	}

	/* display performance results */
	pmu_display (bdi);

	if (bdi->ptr_host_inf)
		bdi->ptr_host_inf->close (bdi);

	if (bdi->ptr_hlm_inf)
		bdi->ptr_hlm_inf->destroy (bdi);

	if (bdi->ptr_ftl_inf) {
		if (bdi->parm_ftl.snapshot == SNAPSHOT_ENABLE && bdi->ptr_ftl_inf->store) {
			bdbm_msg ("[bdbm_drv_main] storing ftl tables to '/usr/share/bdbm_drv/ftl.dat'");
			bdi->ptr_ftl_inf->store (bdi, "/usr/share/bdbm_drv/ftl.dat");
		}
		bdi->ptr_ftl_inf->destroy (bdi);
	}

	if (bdi->ptr_llm_inf)
		bdi->ptr_llm_inf->destroy (bdi);

	if (bdi->ptr_dm_inf) {
		if (bdi->parm_ftl.snapshot == SNAPSHOT_ENABLE && bdi->ptr_dm_inf->store) {
			bdbm_msg ("[bdbm_drv_main] storing dm to '/usr/share/bdbm_drv/dm.dat'");
			bdi->ptr_dm_inf->store (bdi, "/usr/share/bdbm_drv/dm.dat");
		}
		bdi->ptr_dm_inf->close (bdi);
	}

	pmu_destory (bdi);

	bdbm_msg ("[bdbm_drv_main] bdbm_drv is closed");
}

void bdbm_drv_destroy (bdbm_drv_info_t* bdi)
{
	bdbm_free (bdi);
	bdbm_msg ("[bdbm_drv_main] bdbm_drv is removed");
}

