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

#ifndef _BLUEDBM_DEV_RAMSSD_H
#define _BLUEDBM_DEV_RAMSSD_H

#if defined (KERNEL_MODE)
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

#elif defined (USER_MODE)
#include <stdio.h>
#include <stdint.h>

#else
#error Invalid Platform (KERNEL_MODE or USER_MODE)
#endif

#include "bdbm_drv.h"
#include "params.h"
#include "utime.h"


typedef struct {
	void* ptr_req;
	int64_t target_elapsed_time_us;
	bdbm_stopwatch_t sw;
} dev_ramssd_punit_t;

#if defined (KERNEL_MODE)
typedef struct {
	struct work_struct work; /* it must be at the end of structre */
	void* ri;
} dev_ramssd_wq_t;
#endif

typedef struct {
	uint8_t is_init; /* 0: not initialized, 1: initialized */
	uint8_t emul_mode;
	bdbm_device_params_t* np;
	void* ptr_ssdram; /* DRAM memory for SSD */
	dev_ramssd_punit_t* ptr_punits;	/* parallel units */
	bdbm_spinlock_t ramssd_lock;
	void (*intr_handler) (void*);

#if defined (KERNEL_MODE)
	struct hrtimer hrtimer;	/* hrtimer must be at the end of the structure */
	struct workqueue_struct *wq;
	dev_ramssd_wq_t works;
#endif
} dev_ramssd_info_t;

dev_ramssd_info_t* dev_ramssd_create (bdbm_device_params_t* np, void (*intr_handler)(void*));
void dev_ramssd_destroy (dev_ramssd_info_t* ptr_ramssd_info);
uint32_t dev_ramssd_send_cmd (dev_ramssd_info_t* ptr_ramssd_info, bdbm_llm_req_t* ptr_llm_req );

/* for snapshot */
uint32_t dev_ramssd_load (dev_ramssd_info_t* ptr_ramssd_info, const char* fn);
uint32_t dev_ramssd_store (dev_ramssd_info_t* ptr_ramssd_info, const char* fn);

/* some inline functions */
inline static 
uint64_t dev_ramssd_get_page_size_main (dev_ramssd_info_t* ptr_ramssd_info) {
	return ptr_ramssd_info->np->page_main_size;
}

inline static 
uint64_t dev_ramssd_get_page_size_oob (dev_ramssd_info_t* ptr_ramssd_info) {
	return ptr_ramssd_info->np->page_oob_size;
}

inline static 
uint64_t dev_ramssd_get_page_size (dev_ramssd_info_t* ptr_ramssd_info) {
	return ptr_ramssd_info->np->page_main_size +
		ptr_ramssd_info->np->page_oob_size;
}

inline static 
uint64_t dev_ramssd_get_block_size (dev_ramssd_info_t* ptr_ramssd_info) {
	return dev_ramssd_get_page_size (ptr_ramssd_info) *	
		ptr_ramssd_info->np->nr_pages_per_block;
}

inline static 
uint64_t dev_ramssd_get_chip_size (dev_ramssd_info_t* ptr_ramssd_info) {
	return dev_ramssd_get_block_size (ptr_ramssd_info) * 
		ptr_ramssd_info->np->nr_blocks_per_chip;
}

inline static 
uint64_t dev_ramssd_get_channel_size (dev_ramssd_info_t* ptr_ramssd_info) {
	return dev_ramssd_get_chip_size (ptr_ramssd_info) * 
		ptr_ramssd_info->np->nr_chips_per_channel;
}

inline static 
uint64_t dev_ramssd_get_ssd_size (dev_ramssd_info_t* ptr_ramssd_info) {
	return dev_ramssd_get_channel_size (ptr_ramssd_info) * 
		ptr_ramssd_info->np->nr_channels;
}

inline static 
uint64_t dev_ramssd_get_pages_per_block (dev_ramssd_info_t* ptr_ramssd_info) {
	return ptr_ramssd_info->np->nr_pages_per_block;
}

inline static 
uint64_t dev_ramssd_get_blocks_per_chips (dev_ramssd_info_t* ptr_ramssd_info) {
	return ptr_ramssd_info->np->nr_blocks_per_chip;
}

inline static 
uint64_t dev_ramssd_get_chips_per_channel (dev_ramssd_info_t* ptr_ramssd_info) {
	return ptr_ramssd_info->np->nr_chips_per_channel;
}

inline static 
uint64_t dev_ramssd_get_channles_per_ssd (dev_ramssd_info_t* ptr_ramssd_info) {
	return ptr_ramssd_info->np->nr_channels;
}

inline static 
uint64_t dev_ramssd_get_chips_per_ssd (dev_ramssd_info_t* ptr_ramssd_info) {
	return ptr_ramssd_info->np->nr_channels *
		ptr_ramssd_info->np->nr_chips_per_channel;
}

inline static 
uint64_t dev_ramssd_get_blocks_per_ssd (dev_ramssd_info_t* ptr_ramssd_info) {
	return dev_ramssd_get_chips_per_ssd (ptr_ramssd_info) *
		ptr_ramssd_info->np->nr_blocks_per_chip;
}

inline static 
uint64_t dev_ramssd_get_pages_per_ssd (dev_ramssd_info_t* ptr_ramssd_info) {
	return dev_ramssd_get_blocks_per_ssd (ptr_ramssd_info) *
		ptr_ramssd_info->np->nr_pages_per_block;
}

inline static 
uint8_t dev_ramssd_is_init (dev_ramssd_info_t* ptr_ramssd_info) {
	if (ptr_ramssd_info->is_init == 1)
		return 0;
	else 
		return 1;
}

#endif /* _BLUEDBM_DEV_RAMSSD_H */
