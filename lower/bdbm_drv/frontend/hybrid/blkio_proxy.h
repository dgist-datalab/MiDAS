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

#ifndef _BLUEDBM_BLOCKIO_PROXY_PROXY_H
#define _BLUEDBM_BLOCKIO_PROXY_PROXY_H

/* All of them are fake interfaces */
extern bdbm_ftl_inf_t _ftl_block_ftl, _ftl_page_ftl, _ftl_dftl, _ftl_no_ftl;
extern bdbm_hlm_inf_t _hlm_dftl_inf, _hlm_buf_inf, _hlm_nobuf_inf, _hlm_rsd_inf;
extern bdbm_llm_inf_t _llm_mq_inf, _llm_noq_inf;

/* This is a real one */
extern bdbm_host_inf_t _blkio_proxy_inf;

uint32_t blkio_proxy_open (bdbm_drv_info_t* bdi);
void blkio_proxy_close (bdbm_drv_info_t* bdi);
void blkio_proxy_make_req (bdbm_drv_info_t* bdi, void* bio);
void blkio_proxy_end_req (bdbm_drv_info_t* bdi, bdbm_hlm_req_t* req);

#endif
