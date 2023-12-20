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
#elif defined (USER_MODE)
#error "USER_MODE is not supported yet"
#else
#error "Invalid Platform (KERNEL_MODE or USER_MODE)"
#endif

#include "bdbm_drv.h"

typedef struct {
	bdbm_drv_info_t bdi; 
	bdbm_llm_req_t* rr;
	bdbm_device_params_t* np;
	uint64_t nr_punits;
	atomic_t* punit_status;
	uint64_t nr_kp_per_fp;
} bdbm_raw_flash_t;

bdbm_raw_flash_t* bdbm_raw_flash_init (void);
int bdbm_raw_flash_open (bdbm_raw_flash_t* rf);
bdbm_device_params_t* bdbm_raw_flash_get_nand_params (bdbm_raw_flash_t* rf);
void bdbm_raw_flash_exit (bdbm_raw_flash_t* rf);

int bdbm_raw_flash_wait (bdbm_raw_flash_t* rf, uint64_t channel, uint64_t chip, uint8_t* ret);
int bdbm_raw_flash_is_done (bdbm_raw_flash_t* rf, uint64_t channel, uint64_t chip); /* return 0 if it is done; otherwise return 1 */
int bdbm_raw_flash_read_page_async (bdbm_raw_flash_t* rf, uint64_t channel, uint64_t chip, uint64_t block, uint64_t page, uint64_t lpa, uint8_t* ptr_data, uint8_t* ptr_oob);
int bdbm_raw_flash_read_page (bdbm_raw_flash_t* rf, uint64_t channel, uint64_t chip, uint64_t block, uint64_t page, uint64_t lpa, uint8_t* ptr_data, uint8_t* ptr_oob, uint8_t* ret);
int bdbm_raw_flash_write_page_async (bdbm_raw_flash_t* rf, uint64_t channel, uint64_t chip, uint64_t block, uint64_t page, uint64_t lpa, uint8_t* ptr_data, uint8_t* ptr_oob);
int bdbm_raw_flash_write_page (bdbm_raw_flash_t* rf, uint64_t channel, uint64_t chip, uint64_t block, uint64_t page, uint64_t lpa, uint8_t* ptr_data, uint8_t* ptr_oob, uint8_t* ret);
int bdbm_raw_flash_erase_block_async (bdbm_raw_flash_t* rf, uint64_t channel, uint64_t chip, uint64_t block);
int bdbm_raw_flash_erase_block (bdbm_raw_flash_t* rf, uint64_t channel, uint64_t chip, uint64_t block, uint8_t* ret);
