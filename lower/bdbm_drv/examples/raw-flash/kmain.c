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


/* DESCRIPTION: This is an example code that shows how a kernel-module uses the
 * bdbm devices directly with low-level flash commands (e.g, page-read,
 * page-write, and block-erase) without the FTL.
 * 
 * With the raw-flash interface, the end-users must handle everything about
 * flash management, including address remapping, bad-block management, garbage
 * collection, and wear-leveling */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "raw-flash.h"
#include "utime.h"


bdbm_raw_flash_t* rf = NULL;
uint8_t* temp_main = NULL;
uint8_t* temp_oob = NULL;

static void run_sync_test (bdbm_device_params_t* np)
{
	int channel = 0, chip = 0, block = 0, page = 0, lpa = 0, tagid = 0;
	uint8_t** main_page = NULL;
	uint8_t** oob_page = NULL;
	int64_t data_sent = 0;
	uint8_t ret = 0;
	bdbm_stopwatch_t sw;


	/* ---------------------------------------------------------------------------------------------- */
	/* alloc memory */
	printk (KERN_INFO "[run_async_test] alloc memory\n");
	main_page = (uint8_t**)vmalloc (np->nr_channels * np->nr_chips_per_channel * sizeof (uint8_t*));
	oob_page = (uint8_t**)vmalloc (np->nr_channels * np->nr_chips_per_channel * sizeof (uint8_t*));
	for (chip = 0; chip < np->nr_chips_per_channel; chip++) {
		for (channel = 0; channel < np->nr_channels; channel++) {
			tagid = np->nr_chips_per_channel * channel + chip;
			main_page[tagid] = (uint8_t*)vmalloc (np->page_main_size * sizeof (uint8_t));
			oob_page[tagid] = (uint8_t*)vmalloc (np->page_oob_size * sizeof (uint8_t));
		}
	}

	/* ---------------------------------------------------------------------------------------------- */
	/* send erase reqs */
	printk (KERN_INFO "[run_async_test] block-erasure test\n");
	data_sent = 0;
	for (block = 0; block < 1; block++)
	for (chip = 0; chip < np->nr_chips_per_channel; chip++)
	for (channel = 0; channel < np->nr_channels; channel++) {
		tagid = np->nr_chips_per_channel * channel + chip;

		/* sleep if it is bysy */
		if (bdbm_raw_flash_is_done (rf, channel, chip) != 0)
			bdbm_raw_flash_wait (rf, channel, chip, &ret);

		/* send a request */
		bdbm_stopwatch_start (&sw);
		if (bdbm_raw_flash_erase_block (rf, channel, chip, block, &ret) != 0) {
			printk (KERN_INFO "oops! %d %d %d %d (ret = %u) [%s:%d]\n", 
				channel, chip, block, page, ret, __FUNCTION__, __LINE__);
		}
		printk (KERN_INFO "erase elapsed time = %llu (usec), ret = %u\n", 
			bdbm_stopwatch_get_elapsed_time_us (&sw), ret);

		data_sent += np->nr_pages_per_block * np->page_main_size;
	}
	for (chip = 0; chip < np->nr_chips_per_channel; chip++)
	for (channel = 0; channel < np->nr_channels; channel++)
		bdbm_raw_flash_wait (rf, channel, chip, &ret);

	/* ---------------------------------------------------------------------------------------------- */
	/* send write reqs */
	data_sent = 0;
	bdbm_stopwatch_start (&sw);
	printk (KERN_INFO "[run_async_test] page-write test\n");
	for (page = 0; page < 128; page++)
	for (block = 0; block < 1; block++)
	for (chip = 0; chip < np->nr_chips_per_channel; chip++)
	for (channel = 0; channel < np->nr_channels; channel++) {
		uint8_t testbits;
		
		testbits = (page << 6 & 0xC0) | (block << 4 & 0x30) | (chip << 2 & 0x0C) | (channel & 0x03);
		tagid = np->nr_chips_per_channel * channel + chip;

		/* sleep if it is bysy */
		if (bdbm_raw_flash_is_done (rf, channel, chip) != 0)
			bdbm_raw_flash_wait (rf, channel, chip, &ret);

		/* send a request */
		bdbm_stopwatch_start (&sw);
		if (bdbm_raw_flash_write_page (
				rf, channel, chip, block, page, lpa,
				main_page[tagid],
				oob_page[tagid],
				&ret) != 0) {
			printk (KERN_INFO "oops! %d %d %d %d (ret = %u) [%s:%d]\n", 
				channel, chip, block, page, ret, __FUNCTION__, __LINE__);
		}
		printk (KERN_INFO "[%d] write elapsed time = %llu (usec), ret = %u\n", 
			page, bdbm_stopwatch_get_elapsed_time_us (&sw), ret);

		data_sent += np->page_main_size;
		lpa++;
	}
	for (chip = 0; chip < np->nr_chips_per_channel; chip++)
	for (channel = 0; channel < np->nr_channels; channel++)
		bdbm_raw_flash_wait (rf, channel, chip, &ret);

	/* ---------------------------------------------------------------------------------------------- */
	/* send read reqs */
	lpa = 0;
	data_sent = 0;
	bdbm_stopwatch_start (&sw);
	printk (KERN_INFO "[run_async_test] page-read test\n");
	for (page = 0; page < 128; page++)
	for (block = 0; block < 1; block++)
	for (chip = 0; chip < np->nr_chips_per_channel; chip++)
	for (channel = 0; channel < np->nr_channels; channel++) {
		uint8_t testbits;
		
		testbits = (page << 6 & 0xC0) | (block << 4 & 0x30) | (chip << 2 & 0x0C) | (channel & 0x03);

		tagid = np->nr_chips_per_channel * channel + chip;

		/* sleep if it is bysy */
		if (bdbm_raw_flash_is_done (rf, channel, chip) != 0)
			bdbm_raw_flash_wait (rf, channel, chip, &ret);

		/* send a request */
		bdbm_stopwatch_start (&sw);
		if (bdbm_raw_flash_read_page (
				rf, channel, chip, block, page, lpa,
				main_page[tagid],
				oob_page[tagid],
				&ret) != 0) {
			printk (KERN_INFO "oops! %d %d %d %d (ret = %u) [%s:%d]\n", 
				channel, chip, block, page, ret, __FUNCTION__, __LINE__);
		}
		printk (KERN_INFO "[%d] read elapsed time = %llu (usec), ret = %u\n", 
			page, bdbm_stopwatch_get_elapsed_time_us (&sw), ret);

		data_sent += np->page_main_size;
		lpa++;
	}
	for (chip = 0; chip < np->nr_chips_per_channel; chip++)
	for (channel = 0; channel < np->nr_channels; channel++)
		bdbm_raw_flash_wait (rf, channel, chip, &ret);

	/* ---------------------------------------------------------------------------------------------- */
	/* free memory */
	printk (KERN_INFO "[run_async_test] free memory\n");
	for (chip = 0; chip < np->nr_chips_per_channel; chip++) {
		for (channel = 0; channel < np->nr_channels; channel++) {
			tagid = np->nr_chips_per_channel * channel + chip;

			bdbm_raw_flash_wait (rf, channel, chip, &ret);

			vfree (main_page[tagid]);
			vfree (oob_page[tagid]);
		}
	}

	vfree (main_page);
	vfree (oob_page);
}

static void run_async_test (bdbm_device_params_t* np, int check_value)
{
	int channel = 0, chip = 0, block = 0, page = 0, lpa = 0, tagid = 0;
	uint8_t** main_page = NULL;
	uint8_t** oob_page = NULL;
	int64_t data_sent = 0;
	uint8_t ret = 0;
	bdbm_stopwatch_t sw;


	/* ---------------------------------------------------------------------------------------------- */
	/* alloc memory */
	printk (KERN_INFO "[run_async_test] alloc memory\n");
	main_page = (uint8_t**)vmalloc (np->nr_channels * np->nr_chips_per_channel * sizeof (uint8_t*));
	oob_page = (uint8_t**)vmalloc (np->nr_channels * np->nr_chips_per_channel * sizeof (uint8_t*));
	for (chip = 0; chip < np->nr_chips_per_channel; chip++) {
		for (channel = 0; channel < np->nr_channels; channel++) {
			tagid = np->nr_chips_per_channel * channel + chip;
			main_page[tagid] = (uint8_t*)vmalloc (np->page_main_size * sizeof (uint8_t));
			oob_page[tagid] = (uint8_t*)vmalloc (np->page_oob_size * sizeof (uint8_t));
		}
	}
	if (check_value == 1) {
		temp_main = (uint8_t*)vmalloc (np->page_main_size * sizeof (uint8_t));
		temp_oob = (uint8_t*)vmalloc (np->page_oob_size * sizeof (uint8_t));
	}


	/* ---------------------------------------------------------------------------------------------- */
	/* send erase reqs */
	printk (KERN_INFO "[run_async_test] block-erasure test\n");
	data_sent = 0;
	bdbm_stopwatch_start (&sw);
	for (block = 0; block < np->nr_blocks_per_chip; block++)
	for (chip = 0; chip < np->nr_chips_per_channel; chip++)
	for (channel = 0; channel < np->nr_channels; channel++) {
		tagid = np->nr_chips_per_channel * channel + chip;

		/* sleep if it is bysy */
		bdbm_raw_flash_wait (rf, channel, chip, &ret);

		/* send a request */
		bdbm_raw_flash_erase_block_async (rf, channel, chip, block);
		/*bdbm_raw_flash_erase_block (rf, channel, chip, block, &ret);*/

		if (ret == 1) {
			printk (KERN_INFO "bad block: %d %d %d\n",
				channel, chip, block);
		}

		data_sent += np->nr_pages_per_block * np->page_main_size;
	}
	for (chip = 0; chip < np->nr_chips_per_channel; chip++)
	for (channel = 0; channel < np->nr_channels; channel++)
		bdbm_raw_flash_wait (rf, channel, chip, &ret);

	if (check_value == 0) {
		printk (" - erase throughput: %llu (MB/s) (%llu B, %llu ms)", 
			(data_sent/1024) / bdbm_stopwatch_get_elapsed_time_ms (&sw),
			data_sent, bdbm_stopwatch_get_elapsed_time_ms (&sw));
	}


	/* ---------------------------------------------------------------------------------------------- */
	/* send write reqs */
	data_sent = 0;
	bdbm_stopwatch_start (&sw);
	printk (KERN_INFO "[run_async_test] page-write test\n");
	for (page = 0; page < np->nr_pages_per_block; page++)
	for (block = 0; block < np->nr_blocks_per_chip; block++)
	for (chip = 0; chip < np->nr_chips_per_channel; chip++)
	for (channel = 0; channel < np->nr_channels; channel++) {
		uint8_t testbits;
		
		testbits = (page << 6 & 0xC0) | (block << 4 & 0x30) | (chip << 2 & 0x0C) | (channel & 0x03);
		tagid = np->nr_chips_per_channel * channel + chip;

		/* sleep if it is bysy */
		bdbm_raw_flash_wait (rf, channel, chip, &ret);

		if (check_value == 1) {
			memset (main_page[tagid], testbits, np->page_main_size);
			memset (oob_page[tagid], testbits, np->page_oob_size);
		}

		/* send a request */
		bdbm_raw_flash_write_page_async (
				rf, 
				channel, chip, block, page, lpa,
				main_page[tagid],
				oob_page[tagid]);

		data_sent += np->page_main_size;
		lpa++;
	}
	for (chip = 0; chip < np->nr_chips_per_channel; chip++)
	for (channel = 0; channel < np->nr_channels; channel++)
		bdbm_raw_flash_wait (rf, channel, chip, &ret);

	if (check_value == 0) {
		printk (" - write throughput: %llu (MB/s) (%llu B, %llu ms)", 
			(data_sent/1024) / (bdbm_stopwatch_get_elapsed_time_ms (&sw)),
			data_sent, bdbm_stopwatch_get_elapsed_time_ms (&sw));
	}


	/* ---------------------------------------------------------------------------------------------- */
	/* send read reqs */
	lpa = 0;
	data_sent = 0;
	bdbm_stopwatch_start (&sw);
	printk (KERN_INFO "[run_async_test] page-read test\n");
	for (page = 0; page < np->nr_pages_per_block; page++)
	for (block = 0; block < np->nr_blocks_per_chip; block++)
	for (chip = 0; chip < np->nr_chips_per_channel; chip++)
	for (channel = 0; channel < np->nr_channels; channel++) {
		uint8_t testbits;
		
		testbits = (page << 6 & 0xC0) | (block << 4 & 0x30) | (chip << 2 & 0x0C) | (channel & 0x03);

		tagid = np->nr_chips_per_channel * channel + chip;

		/* sleep if it is bysy */
		bdbm_raw_flash_wait (rf, channel, chip, &ret);

		/* send a request */
		bdbm_raw_flash_read_page_async (
				rf, 
				channel, chip, block, page, lpa,
				main_page[tagid],
				oob_page[tagid]);

		if (check_value == 1) {
			memset (temp_main, testbits, np->page_main_size);
			memset (temp_oob, testbits, np->page_oob_size);

			bdbm_raw_flash_wait (rf, channel, chip, &ret);

			if (memcmp (main_page[tagid], temp_main, np->page_main_size) != 0) {
				printk ("OOPS! found mismatch in the main (%u %u %u %u ... != %u %u %u %u ...)\n",
						main_page[tagid][0], main_page[tagid][1], main_page[tagid][2], main_page[tagid][3],
						temp_main[0], temp_main[1], temp_main[2], temp_main[3]);
			}

			if (memcmp (oob_page[tagid], temp_oob, np->page_oob_size) != 0) {
				printk ("OOPS! found mismatch in the oob (%u %u %u %u ... != %u %u %u %u ...)\n",
						oob_page[tagid][0], oob_page[tagid][1], oob_page[tagid][2], oob_page[tagid][3],
						temp_oob[0], temp_oob[1], temp_oob[2], temp_oob[3]);
			}
		}

		data_sent += np->page_main_size;
		lpa++;
	}
	for (chip = 0; chip < np->nr_chips_per_channel; chip++)
	for (channel = 0; channel < np->nr_channels; channel++)
		bdbm_raw_flash_wait (rf, channel, chip, &ret);

	if (check_value == 0) {
		printk (" - read throughput: %llu (MB/s) (%llu B, %llu ms)", 
			(data_sent/1024) / (bdbm_stopwatch_get_elapsed_time_ms (&sw)),
			data_sent, bdbm_stopwatch_get_elapsed_time_ms (&sw));
	}

	/* ---------------------------------------------------------------------------------------------- */
	/* free memory */
	printk (KERN_INFO "[run_async_test] free memory\n");
	for (chip = 0; chip < np->nr_chips_per_channel; chip++) {
		for (channel = 0; channel < np->nr_channels; channel++) {
			tagid = np->nr_chips_per_channel * channel + chip;

			bdbm_raw_flash_wait (rf, channel, chip, &ret);

			vfree (main_page[tagid]);
			vfree (oob_page[tagid]);
		}
	}

	if (check_value == 1) {
		vfree (temp_main);
		vfree (temp_oob);
	}

	vfree (main_page);
	vfree (oob_page);
}

static int __init raw_flash_init (void)
{
	bdbm_device_params_t* np;

	if ((rf = bdbm_raw_flash_init ()) == NULL)
		return -1;

	if (bdbm_raw_flash_open (rf) != 0)
		return -1;

	if ((np = bdbm_raw_flash_get_nand_params (rf)) == NULL)
		return -1;

	/* do a test */
	run_async_test (np, 0); /* don't check values; throughput test */
	/*run_async_test (np, 1); *//* check values */
	/*run_sync_test (np);*/
	/* done */

	return 0;
}

static void __exit raw_flash_exit (void)
{
	bdbm_raw_flash_exit (rf);
}

MODULE_AUTHOR ("Sungjin Lee <chamdoo@csail.mit.edu>");
MODULE_DESCRIPTION ("BlueDBM Manage-Flash Example");
MODULE_LICENSE ("GPL");

module_init (raw_flash_init);
module_exit (raw_flash_exit);
