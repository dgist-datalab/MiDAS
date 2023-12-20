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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/slab.h>

#elif defined(USER_MODE)
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#endif

#include "bdbm_drv.h"
#include "umemory.h"
#include "params.h"
#include "ftl_params.h"
#include "debug.h"
#include "userio.h"
#include "ufile.h"

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

/* main data structure */
bdbm_drv_info_t* _bdi = NULL;

/*#define NUM_THREADS	20*/
/*#define NUM_THREADS	20*/
/*#define NUM_THREADS	10*/
#define NUM_THREADS	1

#include "bdbm_drv.h"
#include "uatomic64.h"

void nvme_cb_done (void* req)
{
	bdbm_blkio_req_t* r = (bdbm_blkio_req_t*)req;
	int i = 0;

	for (i = 0; i < r->bi_bvec_cnt; i++) {
		if (bdbm_is_read (r->bi_rw)) {
		if (r->bi_bvec_ptr[i][0] != 0x0A ||
			r->bi_bvec_ptr[i][1] != 0x0B ||
			r->bi_bvec_ptr[i][2] != 0x0C) {
#if 0
			bdbm_msg ("[%llu] data corruption: %X %X %X",
				r->bi_offset,
				r->bi_bvec_ptr[i][0],
				r->bi_bvec_ptr[i][1],
				r->bi_bvec_ptr[i][2]);
#endif
		}
		}
		bdbm_free (r->bi_bvec_ptr[i]);
	}
	bdbm_free (r);
}

void host_thread_fn_write (void *data) 
{
	int i = 0, j = 0;
	int offset = 0; /* sector (512B) */
	int size = 8 * 32; /* 512B * 8 * 32 = 128 KB */

	for (i = 0; i < 2; i++) {
		bdbm_blkio_req_t* blkio_req = (bdbm_blkio_req_t*)bdbm_malloc (sizeof (bdbm_blkio_req_t));

		/* build blkio req */
		blkio_req->bi_rw = REQTYPE_WRITE;
		blkio_req->bi_offset = offset;
		blkio_req->bi_size = size;
		blkio_req->bi_bvec_cnt = size / 8;
		blkio_req->cb_done = nvme_cb_done;
		for (j = 0; j < blkio_req->bi_bvec_cnt; j++) {
			blkio_req->bi_bvec_ptr[j] = (uint8_t*)bdbm_malloc (4096);
			blkio_req->bi_bvec_ptr[j][0] = 0x0A;
			blkio_req->bi_bvec_ptr[j][1] = 0x0B;
			blkio_req->bi_bvec_ptr[j][2] = 0x0C;
		}

		/* send req to ftl */
		_bdi->ptr_host_inf->make_req (_bdi, blkio_req);

		/* increase offset */
		offset += size;
	}

	pthread_exit (0);
}

void host_thread_fn_read (void *data) 
{
	int i = 0, j = 0;
	int offset = 0; /* sector (512B) */
	int size = 8 * 32; /* 512B * 8 * 32 = 128 KB */

	for (i = 0; i < 2; i++) {
		bdbm_blkio_req_t* blkio_req = (bdbm_blkio_req_t*)bdbm_malloc (sizeof (bdbm_blkio_req_t));

		/* build blkio req */
		blkio_req->bi_rw = REQTYPE_READ;
		blkio_req->bi_offset = offset;
		blkio_req->bi_size = size;
		blkio_req->bi_bvec_cnt = size / 8;
		blkio_req->cb_done = nvme_cb_done;
		for (j = 0; j < blkio_req->bi_bvec_cnt; j++) {
			blkio_req->bi_bvec_ptr[j] = (uint8_t*)bdbm_malloc (4096);
			if (blkio_req->bi_bvec_ptr[j] == NULL) {
				bdbm_msg ("bdbm_malloc () failed");
				exit (-1);
			}
		}

		/* send req to ftl */
		_bdi->ptr_host_inf->make_req (_bdi, blkio_req);

		/* increase offset */
		offset += size;
	}

	pthread_exit (0);
}

int main (int argc, char** argv)
{
	int loop_thread;

	pthread_t thread[NUM_THREADS];
	int thread_args[NUM_THREADS];

	bdbm_msg ("[main] run ftlib... (%d)", sizeof (bdbm_llm_req_t));

	bdbm_msg ("[nvme-main] initialize bdbm_drv");
	if ((_bdi = bdbm_drv_create ()) == NULL) {
		bdbm_error ("[main] bdbm_drv_create () failed");
		return -1;
	}

	if (bdbm_dm_init (_bdi) != 0) {
		bdbm_error ("[main] bdbm_dm_init () failed");
		return -1;
	}

	if (bdbm_drv_setup (_bdi, &_userio_inf, bdbm_dm_get_inf (_bdi)) != 0) {
		bdbm_error ("[main] bdbm_drv_setup () failed");
		return -1;
	}

	if (bdbm_drv_run (_bdi) != 0) {
		bdbm_error ("[main] bdbm_drv_run () failed");
		return -1;
	}

	do {
		bdbm_msg ("[main] start writes");
		for (loop_thread = 0; loop_thread < NUM_THREADS; loop_thread++) {
			thread_args[loop_thread] = loop_thread;
			pthread_create (&thread[loop_thread], NULL, 
				(void*)&host_thread_fn_write, 
				(void*)&thread_args[loop_thread]);
		}

		bdbm_msg ("[main] wait for threads to end...");
		for (loop_thread = 0; loop_thread < NUM_THREADS; loop_thread++) {
			pthread_join (thread[loop_thread], NULL);
		}

		bdbm_msg ("[main] start reads");
		for (loop_thread = 0; loop_thread < NUM_THREADS; loop_thread++) {
			thread_args[loop_thread] = loop_thread;
			pthread_create (&thread[loop_thread], NULL, 
				(void*)&host_thread_fn_read, 
				(void*)&thread_args[loop_thread]);
		}

		bdbm_msg ("[main] wait for threads to end...");
		for (loop_thread = 0; loop_thread < NUM_THREADS; loop_thread++) {
			pthread_join (thread[loop_thread], NULL);
		}

	} while (0);

	sleep (1);
	bdbm_msg ("[main] destroy bdbm_drv");
	bdbm_drv_close (_bdi);
	bdbm_dm_exit (_bdi);
	bdbm_drv_destroy (_bdi);

	bdbm_msg ("[main] done");

	return 0;
}

