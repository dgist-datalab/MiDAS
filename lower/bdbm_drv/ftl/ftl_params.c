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
#include <linux/moduleparam.h>
#include <linux/slab.h>

#elif defined(USER_MODE)
#include <stdint.h>
#include <stdio.h>

#else
#error Invalid Platform (KERNEL_MODE or USER_MODE)

#endif

#include "params.h"
#include "umemory.h"
#include "debug.h"
#include "bdbm_drv.h"

/* 
 * setup parameters according to user configurations 
 */
int _param_kernel_sector_size		= KERNEL_SECTOR_SIZE;	/* 512 Bytes */
int _param_gc_policy 				= GC_POLICY_GREEDY;
int _param_wl_policy 				= WL_POLICY_NONE;
int _param_queuing_policy			= QUEUE_POLICY_MULTI_FIFO;
int _param_trim						= TRIM_ENABLE;
int _param_snapshot					= SNAPSHOT_DISABLE;
int _param_mapping_type				= MAPPING_POLICY_PAGE;
/*int _param_mapping_type				= MAPPING_POLICY_RSD;*/
/*int _param_mapping_type				= MAPPING_POLICY_RSD;*/
int _param_llm_type					= LLM_MULTI_QUEUE;
/*int _param_llm_type					= LLM_NO_QUEUE;*/
int _param_hlm_type					= HLM_NO_BUFFER;

bdbm_ftl_params get_default_ftl_params (void)
{
	bdbm_ftl_params p;

	/* setup driver parameters */
	p.gc_policy = _param_gc_policy;
	p.wl_policy = _param_wl_policy;
	p.queueing_policy = _param_queuing_policy;
	p.kernel_sector_size = _param_kernel_sector_size;
	p.trim = _param_trim;
	p.snapshot = _param_snapshot;
	p.mapping_type = _param_mapping_type;
	p.llm_type = _param_llm_type;
	p.hlm_type = _param_hlm_type;

	return p;
}

void display_ftl_params (bdbm_ftl_params* p)
{
	if (p == NULL) {
		bdbm_msg ("oops! the parameters are not loaded properly");
		return;
	} 

	bdbm_msg ("=====================================================================");
	bdbm_msg ("FTL CONFIGURATION");
	bdbm_msg ("=====================================================================");
	bdbm_msg ("mapping type = %d (1: no ftl, 2: block-mapping, 3: RSD, 4: page-mapping, 5: dftl)", p->mapping_type);
	bdbm_msg ("gc policy = %d (1: merge 2: random, 3: greedy, 4: cost-benefit)", p->gc_policy);
	bdbm_msg ("wl policy = %d (1: none, 2: swap)", p->wl_policy);
	bdbm_msg ("trim mode = %d (1: enable, 2: disable)", p->trim);
	bdbm_msg ("kernel sector = %d bytes", p->kernel_sector_size);
	bdbm_msg ("");
}


