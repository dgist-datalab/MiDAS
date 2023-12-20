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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <queue> 

#include "libmemio.h"
#include <stdlib.h>
#include<pthread.h>
#include <string.h>
//#include "../../devices/nohost/dm_nohost.h"
#include "dm_nohost.h"

#include "../../../../include/settings.h"
#ifndef BPS
#define BPS 64
#define _PPB 256
#define PAGESIZE 8129
#endif
#include "../../../../include/container.h"
//#include "../../../../bench/bench.h"

extern unsigned int* dstBuffer;
extern unsigned int* srcBuffer;
extern pthread_mutex_t endR;
MeasureTime bdbm_mt;
static void __dm_intr_handler (bdbm_drv_info_t* bdi, bdbm_llm_req_t* r);

bdbm_llm_inf_t _bdbm_llm_inf = {
	.ptr_private = NULL,
	.create = NULL,
	.destroy = NULL,
	.make_req = NULL,
	.make_reqs = NULL,
	.flush = NULL,
	/* 'dm' calls 'end_req' automatically
	 * when it gets acks from devices */
	.end_req = __dm_intr_handler, 
};

static bdbm_llm_req_t* __memio_alloc_llm_req (memio_t* mio,bool);
static void __memio_free_llm_req (memio_t* mio, bdbm_llm_req_t* r);

int req_cnt=0;
uint64_t dm_intr_cnt;
MeasureTime _mt;
static void __dm_intr_handler (
	bdbm_drv_info_t* bdi, 
	bdbm_llm_req_t* r)
{
	if(r->req_type!=REQTYPE_GC_ERASE){
		dm_intr_cnt++;
		algo_req *my_algo_req=(algo_req*)r->req;
		if(my_algo_req->parents){
			//bench_lower_end(my_algo_req->parents);
		}
		

		if(r->req_type==REQTYPE_READ){
			if(my_algo_req->type_lower<r->path_type)
				my_algo_req->type_lower=r->path_type;
			my_algo_req->end_req(my_algo_req);
		}
		else{
			if(r->req_type==UINT8_MAX){
				if(r->logaddr.lpa[0]==UINT32_MAX){
					my_algo_req->type=UINT8_MAX;
				}
				else{
					my_algo_req->ppa=r->logaddr.lpa[0];
				}
			}
			my_algo_req->end_req(my_algo_req);
		}
	}
	else{
		if(r->bad_seg_func!=NULL){
			r->bad_seg_func(r->segnum,r->isbad);
		}
	}
	__memio_free_llm_req((memio_t*)bdi->private_data,r);
}

static int __memio_init_llm_reqs (memio_t* mio)
{
	int ret = 0;
	if ((mio->rr = (bdbm_llm_req_t*)bdbm_zmalloc (
			sizeof (bdbm_llm_req_t) * mio->nr_tags)) == NULL) {
		bdbm_error ("bdbm_zmalloc () failed");
		ret = -1;
	} else {
		int i = 0;
		for (i = 0; i < mio->nr_tags; i++) {
			mio->tagQ->push(i);
			mio->rr[i].tag = i;
		}
		bdbm_mutex_init (&mio->tagQMutex);
		bdbm_cond_init (&mio->tagQCond);
	}
	return ret;
}

memio_t* memio_open ()
{
	measure_init(&bdbm_mt);
	bdbm_drv_info_t* bdi = NULL;
	bdbm_dm_inf_t* dm = NULL;
	memio_t* mio = NULL;
	int ret;
	
	//pthread_mutex_init(&proc,NULL);
	/* allocate a memio data structure */
	if ((mio = (memio_t*)bdbm_zmalloc (sizeof (memio_t))) == NULL) {
		bdbm_error ("bdbm_zmalloc() failed");
		return NULL;
	}
	bdi = &mio->bdi;

	/* initialize a device manager */
	if (bdbm_dm_init (bdi) != 0) {
		bdbm_error ("bdbm_dm_init() failed");
		goto fail;
	}

	/* get the device manager interface and assign it to bdi */
	if ((dm = bdbm_dm_get_inf (bdi)) == NULL) {
		bdbm_error ("bdbm_dm_get_inf() failed");
		goto fail;
	}
	bdi->ptr_dm_inf = dm;

	/* probe the device to see if it is working now */
	if ((ret = dm->probe (bdi, &bdi->parm_dev)) != 0) {
		bdbm_error ("dm->probe was NULL or probe() failed (%p, %d)", 
			dm->probe, ret);
		goto fail;
	}
	mio->nr_punits = 64; /* # of blocks that comprise one segment */
	mio->nr_tags = get_dev_tags(); /* # of request outstanding to HW possible */
	//mio->nr_tags = 512 * 1024 / 8; /* # of request outstanding to HW possible */
	mio->io_size = 8192;
	mio->trim_lbas = (1 << 14);
	mio->trim_size = mio->trim_lbas * mio->io_size;

	mio->tagQ = new std::queue<int>;

	/* setup some internal values according to 
	 * the device's organization */
	if ((ret = __memio_init_llm_reqs (mio)) != 0) {
		bdbm_error ("__memio_init_llm_reqs () failed (%d)", 
			ret);
		goto fail;
	}

	/* setup function points; this is just to handle responses from the device */
	bdi->ptr_llm_inf = &_bdbm_llm_inf;

	/* assign rf to bdi's private_data */
	bdi->private_data = (void*)mio;

	/* ok! open the device so that I/Os will be sent to it */
	if ((ret = dm->open (bdi)) != 0) {
		bdbm_error ("dm->open was NULL or open failed (%p. %d)", 
			dm->open, ret);
		goto fail;
	}

	bdbm_mutex_init(&mio->req_mutex);
	return mio;

fail:
	if (mio)
		bdbm_free (mio);
	

	return NULL;
}

static bdbm_llm_req_t* __memio_alloc_llm_req (memio_t* mio, bool read)
{
	int i = 0;
	bdbm_llm_req_t* r = NULL;
	// using std::queue & event
	bdbm_mutex_lock(&mio->tagQMutex);
	while (mio->tagQ->empty()) {
		mio->req_flag+=1;
		bdbm_cond_wait(&mio->tagQCond, &mio->tagQMutex);
	}
	r = (bdbm_llm_req_t*)&mio->rr[mio->tagQ->front()];
	mio->tagQ->pop();
	bdbm_mutex_unlock(&mio->tagQMutex);
	return r;
}

static void __memio_free_llm_req (memio_t* mio, bdbm_llm_req_t* r)
{
	bdbm_mutex_lock(&mio->tagQMutex);
	r->req=NULL;
	mio->tagQ->push(r->tag);
	bdbm_cond_broadcast(&mio->tagQCond); // wakes up threads waiting for a tag
	bdbm_mutex_unlock(&mio->tagQMutex);
}

static void __memio_check_alignment (uint64_t length, uint64_t alignment)
{
	if ((length % alignment) != 0) {
		bdbm_error ("alignment error occurs (length = %d, alignment = %d)",
			length, alignment);
		exit (-1);
	}
}
uint64_t do_io_cnt;
bool flag=false;
static int __memio_do_io (memio_t* mio, int dir, uint32_t lba, uint64_t len, uint8_t* data, int async, void *req, int dmaTag) // async == 0 : sync,  == 1 : async

{
	bdbm_mutex_lock(&mio->req_mutex);
	do_io_cnt++;
	bdbm_llm_req_t* r = NULL;
	bdbm_dm_inf_t* dm = mio->bdi.ptr_dm_inf;
	uint8_t* cur_buf = data;
	uint64_t cur_lba = lba;
	uint64_t sent = 0;
	int dmatag = dmaTag;
	int ret, num_lbas;

	/* read wait variables */
	int counter = (int)(len/mio->io_size);
	bdbm_cond_t readCond = PTHREAD_COND_INITIALIZER;

	/* see if LBA alignment is correct */
	__memio_check_alignment (len, mio->io_size);

	mio->req_flag=0;
	/* fill up logaddr; note that phyaddr is not used here */
	while (cur_lba < lba + (len/mio->io_size)) {
		/* get an empty llm_req */
		r = __memio_alloc_llm_req (mio,dir==0);

		bdbm_bug_on (!r);
		r->path_type=0;
		r->path_type+=mio->req_flag;
		/* setup llm_req */
		switch(dir) {
		case 0:
			r->req_type = REQTYPE_READ;
			break;
		case 1:
			r->req_type = REQTYPE_WRITE;
			break;
		case 2:
			r->req_type = REQTYPE_META_READ;
			break;
		case 3:
			r->req_type = REQTYPE_META_WRITE;
		}
		//r->req_type = (dir == 0) ? REQTYPE_READ : REQTYPE_WRITE;
		r->logaddr.lpa[0] = cur_lba;
		r->fmain.kp_ptr[0] = cur_buf;
		r->async = async;
		/*kukania*/
		algo_req *my_algo_req=(algo_req*)req;
		if(my_algo_req->parents){
			//bench_lower_start(my_algo_req->parents);
		}
		/*before path type*/
		

		r->req = req;
		r->dmaTag = dmatag;
		if (dir==0) {
			r->cond = &readCond;
			r->counter = &counter;
		}
		
		/* send I/O requets to the device */
		if(r->req_type==REQTYPE_READ){
			if(my_algo_req->type_lower!=0){
				if(my_algo_req->type_lower>10){
					printf("wtf!\n");
				}
			}
		}
		if ((ret = dm->make_req (&mio->bdi, r)) != 0) {
			bdbm_error ("dm->make_req() failed (ret = %d)", ret);
			bdbm_bug_on (1);
		}

		/* go the next */
		cur_lba += 1;
		cur_buf += mio->io_size;
		sent += mio->io_size;
	}
	bdbm_mutex_unlock(&mio->req_mutex);
	return sent;
}

// Below should not be used by external functions
void memio_wait (memio_t* mio)
{
	int i = 0;
	do {
		bdbm_mutex_lock(&mio->tagQMutex);
		i = mio->tagQ->size();
		bdbm_mutex_unlock(&mio->tagQMutex);
		printf("i:%d\n",i);
	} while ( i != mio->nr_tags-1 );

}
#if JNI==4
int memio_do_hw_read (memio_t* mio, uint32_t lba, char *key,uint16_t key_len, uint8_t* data, int async, void *req,int dmatag)
{
	bdbm_mutex_lock(&mio->req_mutex);
	do_io_cnt++;
	bdbm_llm_req_t* r = NULL;
	bdbm_dm_inf_t* dm = mio->bdi.ptr_dm_inf;
	uint8_t* cur_buf = data;
	uint64_t cur_lba = lba;
	uint64_t sent = 0;
	int dmaTag = dmatag;
	int ret, num_lbas;

	/* read wait variables */
	int counter = 1;
	bdbm_cond_t readCond = PTHREAD_COND_INITIALIZER;

	/* see if LBA alignment is correct */
	//__memio_check_alignment (len, mio->io_size);

	mio->req_flag=0;

	r = __memio_alloc_llm_req (mio,true);
	bdbm_bug_on (!r);
	r->path_type=0;
	r->path_type+=mio->req_flag;
	r->req_type=REQTYPE_READ;
	r->logaddr.lpa[0]=cur_lba;
	r->fmain.kp_ptr[0]=cur_buf;
	r->async=async;
	r->req=req;
	r->dmaTag=dmatag;

	r->cond = &readCond;
	r->counter = &counter;
	char *target_buf=(char*)get_findKey_dma();
	memcpy(&target_buf[r->tag*256+4],key,key_len);
	dm_do_hw_find(cur_lba,(key_len+4)/16,r);
	bdbm_mutex_unlock(&mio->req_mutex);
	return 1;
}
#else
int memio_do_hw_read (memio_t* mio, uint32_t lba, char *key,uint16_t key_len, uint8_t* data, int async, void *req,int dmatag){
	abort();
	return 1;
}
#endif


int memio_read (memio_t* mio, uint32_t lba, uint64_t len, uint8_t* data, int async, void *req, int dmaTag)
{
	return __memio_do_io (mio, 0, lba, len, data, async, req, dmaTag);
}

int memio_comp_read (memio_t* mio, uint32_t lba, uint64_t len, uint8_t* data, int async, void *req, int dmaTag)
{

	return __memio_do_io (mio, 2, lba, len, data, async, req, dmaTag);
}

int memio_write (memio_t* mio, uint32_t lba, uint64_t len, uint8_t* data, int async, void *req, int dmaTag)
{
	return __memio_do_io (mio, 1, lba, len, data, async, req, dmaTag);
}

int memio_comp_write (memio_t* mio, uint32_t lba, uint64_t len, uint8_t* data, int async, void *req, int dmaTag)
{
	return __memio_do_io (mio, 3, lba, len, data, async, req, dmaTag);
}

int memio_trim (memio_t* mio, uint32_t lba, uint64_t len, void *(*end_req)(uint64_t,uint8_t))
{
	bdbm_mutex_lock(&mio->req_mutex);
	bdbm_llm_req_t* r = NULL;
	bdbm_dm_inf_t* dm = mio->bdi.ptr_dm_inf;
	uint64_t cur_lba = lba;
	uint64_t sent = 0;
	int ret, i;
	

	/* see if LBA alignment is correct */
	__memio_check_alignment (lba, mio->trim_lbas);
	__memio_check_alignment (len, mio->trim_size);

	/* fill up logaddr; note that phyaddr is not used here */
	while (cur_lba < lba + (len/mio->io_size)) {
		//bdbm_msg ("segment #: %d", cur_lba / mio->trim_lbas);
		for (i = 0; i < mio->nr_punits; i++) {
			/* get an empty llm_req */
			r = __memio_alloc_llm_req (mio,false);
			r->bad_seg_func=end_req;

			bdbm_bug_on (!r);

			/* setup llm_req */
			//bdbm_msg ("  -- blk #: %d", i);
			r->req_type = REQTYPE_GC_ERASE;
			r->logaddr.lpa[0] = cur_lba + i;
	//		r->logaddr.lpa[0] = cur_lba + ( (mio->trim_lbas / mio->nr_punits) * i );
			r->fmain.kp_ptr[0] = NULL;	/* no data; it must be NULL */

			/* send I/O requets to the device */
			if ((ret = dm->make_req (&mio->bdi, r)) != 0) {
				bdbm_error ("dm->make_req() failed (ret = %d)", ret);
				bdbm_bug_on (1);
			}
		}

		/* go the next */
		cur_lba += mio->trim_lbas;
		sent += mio->trim_size;
	}

	/* return the length of bytes transferred */
	bdbm_mutex_unlock(&mio->req_mutex);
	return sent;
}


int memio_trim_a_block (memio_t* mio, uint32_t lba)
{

	bdbm_mutex_lock(&mio->req_mutex);
	bdbm_llm_req_t* r = NULL;
	bdbm_dm_inf_t* dm = mio->bdi.ptr_dm_inf;
	uint64_t cur_lba = lba;
	uint64_t sent = 0;
	int ret, i;

	/* fill up logaddr; note that phyaddr is not used here */
	r = __memio_alloc_llm_req (mio,false);
	r->bad_seg_func=NULL;

	bdbm_bug_on (!r);

	/* setup llm_req */
	r->req_type = REQTYPE_GC_ERASE;
	r->logaddr.lpa[0] = cur_lba;
	r->fmain.kp_ptr[0] = NULL;	/* no data; it must be NULL */

	if ((ret = dm->make_req (&mio->bdi, r)) != 0) {
		bdbm_error ("dm->make_req() failed (ret = %d)", ret);
		bdbm_bug_on (1);
	}
	/* return the length of bytes transferred */
	bdbm_mutex_unlock(&mio->req_mutex);
	return sent;
}

int memio_do_merge(unsigned int ht_num, unsigned lt_num, unsigned int *kt_num, unsigned int *inv_num,uint32_t ppa_dma_num){
	return dm_do_merge(ht_num,lt_num,kt_num,inv_num,ppa_dma_num);
}

void memio_close (memio_t* mio)
{

	printf("bdbm_mt max: %lu\n",bdbm_mt.max);
	bdbm_drv_info_t* bdi = NULL;
	bdbm_dm_inf_t* dm = NULL;
	int i;

	/* mio is available? */
	if (!mio) return;

	delete mio->tagQ;

	/* get pointers for dm and bdi */
	bdi = &mio->bdi;
	dm = bdi->ptr_dm_inf;

	/* wait for all the on-going jobs to finish */
	bdbm_msg ("Wait for all the on-going jobs to finish...");
	if (mio->rr) {
		for (i = 0; i < mio->nr_tags; i++)
			if (mio->rr[i].done)
				bdbm_sema_lock (mio->rr[i].done);
	}

	/* close the device interface */
	bdi->ptr_dm_inf->close (bdi);

	/* close the device module */
	bdbm_dm_exit (&mio->bdi);

	/* free allocated memory */
	if (mio->rr) {
		for (i = 0; i < mio->nr_tags; i++)
			if (mio->rr[i].done)
				bdbm_free (mio->rr[i].done);
		bdbm_free (mio->rr);
	}
	bdbm_free (mio);
}

int memio_alloc_dma (int type, char** buf) {
	int dmaTag, byteOffset;
	dmaTag = alloc_dmaQ_tag(type);
	byteOffset = dmaTag * 8192;
//	printf("buf pointer : %p\n", *buf);
	switch (type) {
	case 1:
		*buf  = (char*)(srcBuffer + byteOffset/sizeof(unsigned int));
		break;
	case 2:
		*buf  = (char*)(dstBuffer + byteOffset/sizeof(unsigned int));
		break;
	case 3:
	case 4:
		*buf  = (char*)(srcBuffer + byteOffset/sizeof(unsigned int));
	case 5:
		*buf  = (char*)(dstBuffer + byteOffset/sizeof(unsigned int));
		break;
	}
//	printf("buf pointer : %p\n", *buf);
	return dmaTag;
}

void memio_free_dma (int type, int dmaTag) {
	free_dmaQ_tag(type,dmaTag);
	return ;
}

bool memio_is_clean(memio_t *mio){
	if(mio->tagQ->size()==(uint32_t)mio->nr_tags)
		return true;
	else
		return false;
}
void memio_show_info(){
	printf("m\n");
}

