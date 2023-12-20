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

#ifndef _BLUEDBM_DRV_H
#define _BLUEDBM_DRV_H

/*** KERNEL_MODE ***/
#if defined(KERNEL_MODE)
#define KERNEL_PAGE_SIZE	PAGE_SIZE

/*** USER_MODE ***/
#elif defined(USER_MODE)
#include <stdint.h>
#include "uatomic.h"
#include "uatomic64.h"
#include "ulist.h"

#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"

#define module_param(a,b,c)
#define MODULE_PARM_DESC(a,b)
#define KERNEL_PAGE_SIZE	4096	/* a default page size */

/*** ELSE???? ***/
#else
#error Invalid Platform (KERNEL_MODE or USER_MODE)
#endif

/*** COMMON ***/
#include "params.h"
#include "utime.h"

#include "usync.h"

#define KPAGE_SIZE KERNEL_PAGE_SIZE

typedef struct _bdbm_drv_info_t bdbm_drv_info_t;

/* useful macros */
#define BDBM_KB (1024)
#define BDBM_MB (1024 * 1024)
#define BDBM_GB (1024 * 1024 * 1024)
#define BDBM_TB (1024 * 1024 * 1024 * 1024)

#define BDBM_SIZE_KB(size) (size/BDBM_KB)
#define BDBM_SIZE_MB(size) (size/BDBM_MB)
#define BDBM_SIZE_GB(size) (size/BDBM_GB)

#define BDBM_GET_HOST_INF(bdi) bdi->ptr_host_inf
#define BDBM_GET_DM_INF(bdi) bdi->ptr_dm_inf
#define BDBM_GET_HLM_INF(bdi) bdi->ptr_hlm_inf
#define BDBM_GET_LLM_INF(bdi) bdi->ptr_llm_inf
#define BDBM_GET_DEVICE_PARAMS(bdi) (&bdi->parm_dev)
#define BDBM_GET_DRIVER_PARAMS(bdi) (&bdi->parm_ftl)
#define BDBM_GET_FTL_INF(bdi) bdi->ptr_ftl_inf

#define BDBM_HOST_PRIV(bdi) bdi->ptr_host_inf->ptr_private
#define BDBM_DM_PRIV(bdi) bdi->ptr_dm_inf->ptr_private
#define BDBM_HLM_PRIV(bdi) bdi->ptr_hlm_inf->ptr_private
#define BDBM_LLM_PRIV(bdi) bdi->ptr_llm_inf->ptr_private
#define BDBM_FTL_PRIV(bdi) bdi->ptr_ftl_inf->ptr_private

#define BDBM_GET_NR_PUNITS(np) \
	(np.nr_channels * np.nr_chips_per_channel)
#define BDBM_GET_PUNIT_ID(bdi,p) \
	(p->channel_no * bdi->parm_dev.nr_chips_per_channel + p->chip_no)

/* request types */
enum BDBM_REQTYPE {
	REQTYPE_DONE			= 0xFF0000,
	REQTYPE_IO_READ 		= 0x000001,
	REQTYPE_IO_READ_DUMMY 	= 0x000002,
	REQTYPE_IO_WRITE 		= 0x000004,
	REQTYPE_IO_ERASE 		= 0x000008,
	REQTYPE_IO_TRIM 		= 0x000010,
	REQTYPE_NORNAL 			= 0x000100,
	REQTYPE_RMW 			= 0x000200,
	REQTYPE_GC 				= 0x000400,
	REQTYPE_META 			= 0x000800,

	REQTYPE_READ 			= REQTYPE_NORNAL 	| REQTYPE_IO_READ,
	REQTYPE_READ_DUMMY 		= REQTYPE_NORNAL 	| REQTYPE_IO_READ_DUMMY,
	REQTYPE_WRITE 			= REQTYPE_NORNAL 	| REQTYPE_IO_WRITE,
	REQTYPE_TRIM 			= REQTYPE_NORNAL 	| REQTYPE_IO_TRIM,
	REQTYPE_RMW_READ 		= REQTYPE_RMW 		| REQTYPE_IO_READ,
	REQTYPE_RMW_WRITE 		= REQTYPE_RMW 		| REQTYPE_IO_WRITE,
	REQTYPE_GC_READ 		= REQTYPE_GC 		| REQTYPE_IO_READ,
	REQTYPE_GC_WRITE 		= REQTYPE_GC 		| REQTYPE_IO_WRITE,
	REQTYPE_GC_ERASE 		= REQTYPE_GC 		| REQTYPE_IO_ERASE,
	REQTYPE_META_READ 		= REQTYPE_META 		| REQTYPE_IO_READ,
	REQTYPE_META_WRITE 		= REQTYPE_META 		| REQTYPE_IO_WRITE,
};

#define bdbm_is_normal(type) (((type & REQTYPE_NORNAL) == REQTYPE_NORNAL) ? 1 : 0)
#define bdbm_is_rmw(type) (((type & REQTYPE_RMW) == REQTYPE_RMW) ? 1 : 0)
#define bdbm_is_gc(type) (((type & REQTYPE_GC) == REQTYPE_GC) ? 1 : 0)
#define bdbm_is_meta(type) (((type & REQTYPE_META) == REQTYPE_META) ? 1 : 0)
#define bdbm_is_read(type) (((type & REQTYPE_IO_READ) == REQTYPE_IO_READ) ? 1 : 0)
#define bdbm_is_write(type) (((type & REQTYPE_IO_WRITE) == REQTYPE_IO_WRITE) ? 1 : 0)
#define bdbm_is_erase(type) (((type & REQTYPE_IO_ERASE) == REQTYPE_IO_ERASE) ? 1 : 0)
#define bdbm_is_trim(type) (((type & REQTYPE_IO_TRIM) == REQTYPE_IO_TRIM) ? 1 : 0)


/* a physical address */
typedef struct {
	uint64_t punit_id;
	uint64_t channel_no;
	uint64_t chip_no;
	uint64_t block_no;
	uint64_t page_no;
} bdbm_phyaddr_t;

/* max kernel pages per physical flash page */
#define BDBM_MAX_PAGES 1

/* a bluedbm blockio request */
#define BDBM_BLKIO_MAX_VECS 256

typedef struct {
	uint64_t bi_rw; /* REQTYPE_WRITE or REQTYPE_READ */
	uint64_t bi_offset; /* unit: sector (512B) */
	uint64_t bi_size; /* unit: sector (512B) */
	uint64_t bi_bvec_cnt; /* unit: kernel-page (4KB); it must be equal to 'bi_size / 8' */
	uint8_t* bi_bvec_ptr[BDBM_BLKIO_MAX_VECS]; /* an array of 4 KB data for bvec */
	uint8_t ret; /* a return value will be kept here */
	void* bio; /* reserved for kernel's bio requests */
	void* user; /* keep user's data structure */
	void* user2; /* keep user's data structure */
	void (*cb_done) (void* req); /* call-back function which is called when a request is done */
} bdbm_blkio_req_t;

#define BDBM_ALIGN_UP(addr,size)		(((addr)+((size)-1))&(~((size)-1)))
#define BDBM_ALIGN_DOWN(addr,size)		((addr)&(~((size)-1)))
#define NR_KSECTORS_IN(size)			(size/KSECTOR_SIZE)
#define NR_KPAGES_IN(size)				(size/KPAGE_SIZE)

typedef enum {
	KP_STT_DONE = 0x0F,
	KP_STT_HOLE = 0x10,
	KP_STT_DATA = 0x20,
	KP_STT_HOLE_DONE = KP_STT_HOLE | KP_STT_DONE,
	KP_STT_DATA_DONE = KP_STT_DATA | KP_STT_DONE,
} kp_stt_t;

typedef struct {
	int64_t lpa[BDBM_MAX_PAGES];
	int32_t ofs;	/* only used for reads */
} bdbm_logaddr_t;

typedef struct {
	kp_stt_t kp_stt[BDBM_MAX_PAGES];
	uint8_t* kp_ptr[BDBM_MAX_PAGES];
	uint8_t* kp_pad[BDBM_MAX_PAGES];
} bdbm_flash_page_main_t;

typedef struct {
	uint8_t* data;
} bdbm_flash_page_oob_t;

typedef struct {
	uint32_t req_type; /* read, write, or trim */
	uint8_t ret;	/* old for GC */
	void* ptr_hlm_req;
	void* ptr_qitem;
	bdbm_sema_t* done;	/* maybe used by applications that require direct notifications from an interrupt handler */

	/* logical / physical info */
	bdbm_logaddr_t logaddr;
	bdbm_phyaddr_t phyaddr;
	bdbm_phyaddr_t phyaddr_src;
	bdbm_phyaddr_t phyaddr_dst;

	/* physical layout */
	bdbm_flash_page_main_t fmain;
	bdbm_flash_page_oob_t foob;

	/* extension for nohost */
	int tag;
	bdbm_cond_t* cond;
	int* counter;
} bdbm_llm_req_t;

typedef struct {
	struct list_head list;	/* for hlm_reqs_pool */
	uint32_t req_type; /* read, write, or trim */
	bdbm_stopwatch_t sw;

	union {
		/* for rw ops */
		struct {
			uint64_t nr_llm_reqs;
			atomic64_t nr_llm_reqs_done;
			bdbm_llm_req_t llm_reqs[BDBM_BLKIO_MAX_VECS];
			bdbm_sema_t done;
		};
		/* for trim ops */
		struct {
			uint64_t lpa;
			uint64_t len;
		};
		/* for gc??? */
		struct {
		};
	};

	void* blkio_req;
	uint8_t ret;
} bdbm_hlm_req_t;

#define bdbm_hlm_for_each_llm_req(r, h, i) \
	for (i = 0, r = &h->llm_reqs[i]; i < h->nr_llm_reqs; r = &h->llm_reqs[++i]) 
#define bdbm_llm_set_logaddr(lr, l) lr->logaddr = l
#define bdbm_llm_get_logaddr(lr) &lr->logaddr
#define bdbm_llm_set_phyaddr(lr, p) lr->phyaddr = p
#define bdbm_llm_get_phyaddr(lr) &lr->phyaddr

/* a high-level request for gc */
typedef struct {
	uint32_t req_type;
	uint64_t nr_llm_reqs;
	atomic64_t nr_llm_reqs_done;
	bdbm_llm_req_t* llm_reqs;
	bdbm_sema_t done;
} bdbm_hlm_req_gc_t;

/* a generic host interface */
typedef struct {
	void* ptr_private;
	uint32_t (*open) (bdbm_drv_info_t* bdi);
	void (*close) (bdbm_drv_info_t* bdi);
	void (*make_req) (bdbm_drv_info_t* bdi, void* req);
	void (*end_req) (bdbm_drv_info_t* bdi, bdbm_hlm_req_t* req);
} bdbm_host_inf_t;

/* a generic high-level memory manager interface */
typedef struct {
	void* ptr_private;
	uint32_t (*create) (bdbm_drv_info_t* bdi);
	void (*destroy) (bdbm_drv_info_t* bdi);
	uint32_t (*make_req) (bdbm_drv_info_t* bdi, bdbm_hlm_req_t* req);
	void (*end_req) (bdbm_drv_info_t* bdi, bdbm_llm_req_t* req);
} bdbm_hlm_inf_t;

/* a generic low-level memory manager interface */
typedef struct {
	void* ptr_private;
	uint32_t (*create) (bdbm_drv_info_t* bdi);
	void (*destroy) (bdbm_drv_info_t* bdi);
	uint32_t (*make_req) (bdbm_drv_info_t* bdi, bdbm_llm_req_t* req);
	uint32_t (*make_reqs) (bdbm_drv_info_t* bdi, bdbm_hlm_req_t* req);
	void (*flush) (bdbm_drv_info_t* bdi);
	void (*end_req) (bdbm_drv_info_t* bdi, bdbm_llm_req_t* req);
} bdbm_llm_inf_t;

/* a generic device interface */
typedef struct {
	void* ptr_private;
	uint32_t (*probe) (bdbm_drv_info_t* bdi, bdbm_device_params_t* param);
	uint32_t (*open) (bdbm_drv_info_t* bdi);
	void (*close) (bdbm_drv_info_t* bdi);
	uint32_t (*make_req) (bdbm_drv_info_t* bdi, bdbm_llm_req_t* req);
	uint32_t (*make_reqs) (bdbm_drv_info_t* bdi, bdbm_hlm_req_t* req);
	void (*end_req) (bdbm_drv_info_t* bdi, bdbm_llm_req_t* req);
	uint32_t (*load) (bdbm_drv_info_t* bdi, const char* fn);
	uint32_t (*store) (bdbm_drv_info_t* bdi, const char* fn);
} bdbm_dm_inf_t;

/* a generic FTL interface */
#if 0
typedef struct {
	void* ptr_private;
	uint32_t (*create) (bdbm_drv_info_t* bdi);
	void (*destroy) (bdbm_drv_info_t* bdi);
	uint32_t (*get_free_ppa) (bdbm_drv_info_t* bdi, uint64_t lpa, bdbm_phyaddr_t* ppa);
	uint32_t (*get_ppa) (bdbm_drv_info_t* bdi, uint64_t lpa, bdbm_phyaddr_t* ppa);
	uint32_t (*map_lpa_to_ppa) (bdbm_drv_info_t* bdi, uint64_t lpa, bdbm_phyaddr_t* ppa);
	uint32_t (*invalidate_lpa) (bdbm_drv_info_t* bdi, uint64_t lpa, uint64_t len);
	uint32_t (*do_gc) (bdbm_drv_info_t* bdi);
	uint8_t (*is_gc_needed) (bdbm_drv_info_t* bdi);

	/* interfaces for intialization */
	uint32_t (*scan_badblocks) (bdbm_drv_info_t* bdi);
	uint32_t (*load) (bdbm_drv_info_t* bdi, const char* fn);
	uint32_t (*store) (bdbm_drv_info_t* bdi, const char* fn);
	
	/* interfaces for RSD */
	uint64_t (*get_segno) (bdbm_drv_info_t* bdi, uint64_t lpa);

	/* interfaces for DFTL */
	uint8_t (*check_mapblk) (bdbm_drv_info_t* bdi, uint64_t lpa);
	bdbm_llm_req_t* (*prepare_mapblk_eviction) (bdbm_drv_info_t* bdi);
	void (*finish_mapblk_eviction) (bdbm_drv_info_t* bdi, bdbm_llm_req_t* r);
	bdbm_llm_req_t* (*prepare_mapblk_load) (bdbm_drv_info_t* bdi, uint64_t lpa);
	void (*finish_mapblk_load) (bdbm_drv_info_t* bdi, bdbm_llm_req_t* r);
} bdbm_ftl_inf_t;
#endif

typedef struct {
	void* ptr_private;
	uint32_t (*create) (bdbm_drv_info_t* bdi);
	void (*destroy) (bdbm_drv_info_t* bdi);
	uint32_t (*get_free_ppa) (bdbm_drv_info_t* bdi, int64_t lpa, bdbm_phyaddr_t* ppa);
	uint32_t (*get_ppa) (bdbm_drv_info_t* bdi, int64_t lpa, bdbm_phyaddr_t* ppa, uint64_t* sp_off);
	uint32_t (*map_lpa_to_ppa) (bdbm_drv_info_t* bdi, bdbm_logaddr_t* logaddr, bdbm_phyaddr_t* ppa);
	uint32_t (*invalidate_lpa) (bdbm_drv_info_t* bdi, int64_t lpa, uint64_t len);
	uint32_t (*do_gc) (bdbm_drv_info_t* bdi, int64_t lpa);
	uint8_t (*is_gc_needed) (bdbm_drv_info_t* bdi, int64_t lpa);

	/* interfaces for intialization */
	uint32_t (*scan_badblocks) (bdbm_drv_info_t* bdi);
	uint32_t (*load) (bdbm_drv_info_t* bdi, const char* fn);
	uint32_t (*store) (bdbm_drv_info_t* bdi, const char* fn);
	
	/* interfaces for RSD */
	uint64_t (*get_segno) (bdbm_drv_info_t* bdi, uint64_t lpa);

	/* interfaces for DFTL */
	uint8_t (*check_mapblk) (bdbm_drv_info_t* bdi, uint64_t lpa);
	bdbm_llm_req_t* (*prepare_mapblk_eviction) (bdbm_drv_info_t* bdi);
	void (*finish_mapblk_eviction) (bdbm_drv_info_t* bdi, bdbm_llm_req_t* r);
	bdbm_llm_req_t* (*prepare_mapblk_load) (bdbm_drv_info_t* bdi, uint64_t lpa);
	void (*finish_mapblk_load) (bdbm_drv_info_t* bdi, bdbm_llm_req_t* r);
} bdbm_ftl_inf_t;


/* for performance monitoring */
typedef struct {
	bdbm_spinlock_t pmu_lock;
	bdbm_stopwatch_t exetime;
	atomic64_t page_read_cnt;
	atomic64_t page_write_cnt;
	atomic64_t rmw_read_cnt;
	atomic64_t rmw_write_cnt;
	atomic64_t gc_cnt;
	atomic64_t gc_erase_cnt;
	atomic64_t gc_read_cnt;
	atomic64_t gc_write_cnt;
	atomic64_t meta_read_cnt;
	atomic64_t meta_write_cnt;
	uint64_t time_r_sw;
	uint64_t time_r_q;
	uint64_t time_r_tot;
	uint64_t time_w_sw;
	uint64_t time_w_q;
	uint64_t time_w_tot;
	uint64_t time_rmw_sw;
	uint64_t time_rmw_q;
	uint64_t time_rmw_tot;
	uint64_t time_gc_sw;
	uint64_t time_gc_q;
	uint64_t time_gc_tot;
	atomic64_t* util_r;
	atomic64_t* util_w;
} bdbm_perf_monitor_t;

/* the main data-structure for bdbm_drv */
struct _bdbm_drv_info_t {
	void* private_data;
	bdbm_ftl_params parm_ftl;
	bdbm_device_params_t parm_dev;
	bdbm_host_inf_t* ptr_host_inf; 
	bdbm_dm_inf_t* ptr_dm_inf;
	bdbm_hlm_inf_t* ptr_hlm_inf;
	bdbm_llm_inf_t* ptr_llm_inf;
	bdbm_ftl_inf_t* ptr_ftl_inf;
	bdbm_perf_monitor_t pm;
};

/* functions for bdi creation, setup, run, and remove */
bdbm_drv_info_t* bdbm_drv_create (void);
int bdbm_drv_setup (bdbm_drv_info_t* bdi, bdbm_host_inf_t* host_inf, bdbm_dm_inf_t* dm_inf);
int bdbm_drv_run (bdbm_drv_info_t* bdi);
void bdbm_drv_close (bdbm_drv_info_t* bdi);
void bdbm_drv_destroy (bdbm_drv_info_t* bdi);

#endif /* _BLUEDBM_DRV_H */
