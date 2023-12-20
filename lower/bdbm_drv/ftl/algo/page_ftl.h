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

#ifndef _BLUEDBM_FTL_PAGEFTL_H
#define _BLUEDBM_FTL_PAGEFTL_H

extern bdbm_ftl_inf_t _ftl_page_ftl;

#if 0
uint32_t bdbm_page_ftl_create (bdbm_drv_info_t* bdi);
void bdbm_page_ftl_destroy (bdbm_drv_info_t* bdi);
uint32_t bdbm_page_ftl_get_free_ppa (bdbm_drv_info_t* bdi, uint64_t lpa, bdbm_phyaddr_t* ppa);
uint32_t bdbm_page_ftl_get_ppa (bdbm_drv_info_t* bdi, uint64_t lpa, bdbm_phyaddr_t* ppa);
uint32_t bdbm_page_ftl_map_lpa_to_ppa (bdbm_drv_info_t* bdi, uint64_t lpa, bdbm_phyaddr_t* ptr_phyaddr);
uint32_t bdbm_page_ftl_invalidate_lpa (bdbm_drv_info_t* bdi, uint64_t lpa, uint64_t len);
uint8_t bdbm_page_ftl_is_gc_needed (bdbm_drv_info_t* bdi);
uint32_t bdbm_page_ftl_do_gc (bdbm_drv_info_t* bdi);
uint32_t bdbm_page_badblock_scan (bdbm_drv_info_t* bdi);
uint32_t bdbm_page_ftl_load (bdbm_drv_info_t* bdi, const char* fn);
uint32_t bdbm_page_ftl_store (bdbm_drv_info_t* bdi, const char* fn);
#endif

uint32_t bdbm_page_ftl_create (bdbm_drv_info_t* bdi);
void bdbm_page_ftl_destroy (bdbm_drv_info_t* bdi);
uint32_t bdbm_page_ftl_get_free_ppa (bdbm_drv_info_t* bdi, int64_t lpa, bdbm_phyaddr_t* ppa);
uint32_t bdbm_page_ftl_get_ppa (bdbm_drv_info_t* bdi, int64_t lpa, bdbm_phyaddr_t* ppa, uint64_t* sp_off);
uint32_t bdbm_page_ftl_map_lpa_to_ppa (bdbm_drv_info_t* bdi, bdbm_logaddr_t* logaddr, bdbm_phyaddr_t* ppa);
uint32_t bdbm_page_ftl_invalidate_lpa (bdbm_drv_info_t* bdi, int64_t lpa, uint64_t len);
uint8_t bdbm_page_ftl_is_gc_needed (bdbm_drv_info_t* bdi, int64_t lpa);
uint32_t bdbm_page_ftl_do_gc (bdbm_drv_info_t* bdi, int64_t lpa);
uint32_t bdbm_page_badblock_scan (bdbm_drv_info_t* bdi);
uint32_t bdbm_page_ftl_load (bdbm_drv_info_t* bdi, const char* fn);
uint32_t bdbm_page_ftl_store (bdbm_drv_info_t* bdi, const char* fn);


#endif /* _BLUEDBM_FTL_BLOCKFTL_H */

