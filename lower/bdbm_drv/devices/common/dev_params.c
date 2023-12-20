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
#include <linux/moduleparam.h>
#include <linux/slab.h>

#elif defined (USER_MODE)

#include <stdio.h>
#include <stdint.h>

#else
#error Invalid Platform (KERNEL_MODE or USER_MODE)
#endif

#include "bdbm_drv.h"
#include "params.h"
#include "umemory.h"
#include "debug.h"

enum BDBM_DEFAULT_NAND_PARAMS {
	NAND_PAGE_SIZE = 4096*BDBM_MAX_PAGES,
	//NAND_PAGE_OOB_SIZE = 64, /* for bdbm hardware */
	NAND_PAGE_OOB_SIZE = 8*BDBM_MAX_PAGES,
	NR_PAGES_PER_BLOCK = 128,
	NR_BLOCKS_PER_CHIP = 192/BDBM_MAX_PAGES,
	//NR_BLOCKS_PER_CHIP = 8/BDBM_MAX_PAGES,
	NR_CHIPS_PER_CHANNEL = 4,
	//NR_CHIPS_PER_CHANNEL = 8,
	NR_CHANNELS = 8,
	NAND_HOST_BUS_TRANS_TIME_US = 0,	/* assume to be 0 */
	NAND_CHIP_BUS_TRANS_TIME_US = 100,	/* 100us */
	NAND_PAGE_PROG_TIME_US = 500,		/* 1.3ms */	
	NAND_PAGE_READ_TIME_US = 100,		/* 100us */
	NAND_BLOCK_ERASE_TIME_US = 3000,	/* 3ms */
};

int _param_nr_channels 				= NR_CHANNELS;
int _param_nr_chips_per_channel		= NR_CHIPS_PER_CHANNEL;
int _param_nr_blocks_per_chip 		= NR_BLOCKS_PER_CHIP;
int _param_nr_pages_per_block 		= NR_PAGES_PER_BLOCK;
int _param_page_main_size 			= NAND_PAGE_SIZE;
int _param_page_oob_size 			= NAND_PAGE_OOB_SIZE;
int _param_host_bus_trans_time_us	= NAND_HOST_BUS_TRANS_TIME_US;
int _param_chip_bus_trans_time_us	= NAND_CHIP_BUS_TRANS_TIME_US;
int _param_page_prog_time_us		= NAND_PAGE_PROG_TIME_US; 		
int _param_page_read_time_us		= NAND_PAGE_READ_TIME_US;
int _param_block_erase_time_us		= NAND_BLOCK_ERASE_TIME_US;

/* TODO: Hmm... there might be a more fancy way than this... */
#if defined (CONFIG_DEVICE_TYPE_RAMDRIVE)
int	_param_device_type = DEVICE_TYPE_RAMDRIVE;
#elif defined (CONFIG_DEVICE_TYPE_RAMDRIVE_INTR)
int	_param_device_type = DEVICE_TYPE_RAMDRIVE_INTR;
#elif defined (CONFIG_DEVICE_TYPE_RAMDRIVE_TIMING)
int _param_device_type = DEVICE_TYPE_RAMDRIVE_TIMING;
#elif defined (CONFIG_DEVICE_TYPE_BLUEDBM)
int _param_device_type = DEVICE_TYPE_BLUEDBM;
#elif defined (CONFIG_DEVICE_TYPE_USER_DUMMY)
int _param_device_type = DEVICE_TYPE_USER_DUMMY;
#elif defined (CONFIG_DEVICE_TYPE_USER_RAMDRIVE)
int _param_device_type = DEVICE_TYPE_USER_RAMDRIVE;
#else
int _param_device_type = DEVICE_TYPE_NOT_SPECIFIED;
#endif

#if defined (KERNEL_MODE)
module_param (_param_nr_channels, int, 0000);
module_param (_param_nr_chips_per_channel, int, 0000);
module_param (_param_nr_blocks_per_chip, int, 0000);
module_param (_param_nr_pages_per_block, int, 0000);
module_param (_param_page_main_size, int, 0000);
module_param (_param_page_oob_size, int, 0000);
module_param (_param_host_bus_trans_time_us, int, 0000);
module_param (_param_chip_bus_trans_time_us, int, 0000);
module_param (_param_page_prog_time_us, int, 0000);
module_param (_param_page_read_time_us, int, 0000);
module_param (_param_block_erase_time_us, int, 0000);
module_param (_param_device_type, int, 0000);

MODULE_PARM_DESC (_param_nr_channels, "# of channels");
MODULE_PARM_DESC (_param_nr_chips_per_channel, "# of chips per channel");
MODULE_PARM_DESC (_param_nr_blocks_per_chip, "# of blocks per chip");
MODULE_PARM_DESC (_param_nr_pages_per_block, "# of pages per block");
MODULE_PARM_DESC (_param_page_main_size, "page main size");
MODULE_PARM_DESC (_param_page_oob_size, "page oob size");
MODULE_PARM_DESC (_param_ramdrv_timing_mode, "timing mode for ramdrive");
MODULE_PARM_DESC (_param_host_bus_trans_time_us, "host bus transfer time");
MODULE_PARM_DESC (_param_chip_bus_trans_time_us, "NAND bus transfer time");
MODULE_PARM_DESC (_param_page_prog_time_us, "page program time");
MODULE_PARM_DESC (_param_page_read_time_us, "page read time");
MODULE_PARM_DESC (_param_block_erase_time_us, "block erasure time");
MODULE_PARM_DESC (_param_device_type, "device type"); /* it must be reset when implementing actual device modules */
#endif

bdbm_device_params_t get_default_device_params (void)
{
	bdbm_device_params_t p;

	/* user-specified parameters */
	p.nr_channels = _param_nr_channels;
 	p.nr_chips_per_channel = _param_nr_chips_per_channel;
 	p.nr_blocks_per_chip = _param_nr_blocks_per_chip;
 	p.nr_pages_per_block = _param_nr_pages_per_block;
 	p.page_main_size = _param_page_main_size;
 	p.page_oob_size = _param_page_oob_size;
	p.device_type = _param_device_type;
 	p.page_prog_time_us = _param_page_prog_time_us;
 	p.page_read_time_us = _param_page_read_time_us;
 	p.block_erase_time_us = _param_block_erase_time_us;
 
 	/* other parameters derived from user parameters */
 	p.nr_blocks_per_channel = p.nr_chips_per_channel * p.nr_blocks_per_chip;
	p.nr_blocks_per_ssd = p.nr_channels * p.nr_chips_per_channel * p.nr_blocks_per_chip;
	p.nr_chips_per_ssd = p.nr_channels * p.nr_chips_per_channel;
	p.nr_pages_per_ssd = p.nr_pages_per_block * p.nr_blocks_per_ssd;
#if defined (USE_NEW_RMW)
	p.nr_subpages_per_page = (p.page_main_size / KERNEL_PAGE_SIZE);
	bdbm_bug_on (p.nr_subpages_per_page != BDBM_MAX_PAGES);
#else
	p.nr_subpages_per_page = 1;
#endif
	p.nr_subpages_per_block = (p.nr_subpages_per_page * p.nr_pages_per_block);
	p.nr_subpages_per_ssd = (p.nr_subpages_per_page * p.nr_pages_per_ssd);	/* the size of the subpage must be the same as the kernel-page size (4KB) */

	p.device_capacity_in_byte = 0;
	p.device_capacity_in_byte += p.nr_channels;
	p.device_capacity_in_byte *= p.nr_chips_per_channel;
	p.device_capacity_in_byte *= p.nr_blocks_per_chip;
	p.device_capacity_in_byte *= p.nr_pages_per_block;
	p.device_capacity_in_byte *= p.page_main_size;

	return p;
}

void display_device_params (bdbm_device_params_t* p)
{
    bdbm_msg ("=====================================================================");
    bdbm_msg ("DEVICE PARAMETERS");
    bdbm_msg ("=====================================================================");
    bdbm_msg ("# of channels = %llu", p->nr_channels);
    bdbm_msg ("# of chips per channel = %llu", p->nr_chips_per_channel);
    bdbm_msg ("# of blocks per chip = %llu", p->nr_blocks_per_chip);
    bdbm_msg ("# of pages per block = %llu", p->nr_pages_per_block);
	bdbm_msg ("# of subpages per page = %llu", p->nr_subpages_per_page);
    bdbm_msg ("page main size  = %llu bytes", p->page_main_size);
    bdbm_msg ("page oob size = %llu bytes", p->page_oob_size);
	bdbm_msg ("device type = %u (1: ramdrv, 2: ramdrive (intr), 3: ramdrive (timing), 4: BlueDBM, 5: libdummy, 6: libramdrive)", 
			p->device_type);
    bdbm_msg ("");
}

