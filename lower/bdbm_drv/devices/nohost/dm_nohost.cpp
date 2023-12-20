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

#elif defined (USER_MODE)
#include <stdint.h>

#else
#error Invalid Platform (KERNEL_MODE or USER_MODE)
#endif

#include <semaphore.h>

#include "sw_poller.h"
#include "debug.h"
#include "dm_nohost.h"
#include "dev_params.h"

#include "utime.h"
#include "umemory.h"


/***/
#include "FlashIndication.h"
#include "FlashRequest.h"
#include "DmaBuffer.h"

#include <queue> // koo
std::queue<int> *writeDmaQ = NULL;
std::queue<int> *readDmaQ = NULL;

#include <pthread.h>
pthread_mutex_t writeDmaQ_mutx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t readDmaQ_mutx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t writeDmaQ_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t readDmaQ_cond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t bus_lock=PTHREAD_MUTEX_INITIALIZER;


extern pthread_mutex_t endR;
struct timespec reqtime;

#define PPA_LIST_SIZE (200*1024)
#define MBYTE (1024*1024)

#define FPAGE_SIZE (8192)
#define FPAGE_SIZE_VALID (8192)
//#define NUM_TAGS 128 //koo
#define NUM_TAGS 128

// for Table
#define NUM_BLOCKS 4096
#define NUM_SEGMENTS NUM_BLOCKS
#define NUM_CHANNELS 8
#define NUM_CHIPS 8
#define NUM_LOGBLKS (NUM_CHANNELS*NUM_CHIPS)
#define NUM_PAGES_PER_BLK 256

//koo
#define DMASIZE (128*(1024/8))
#define METANUM 0
typedef enum {
	UNINIT,
	ERASED,
	WRITTEN
} FlashStatusT;

typedef struct {
	bool busy;
	int lpa;
} TagTableEntry;

FlashRequestProxy *device;

pthread_mutex_t flashReqMutex;
pthread_cond_t flashFreeTagCond;

//8k * 128
size_t dstAlloc_sz = FPAGE_SIZE * DMASIZE *sizeof(unsigned char);
size_t srcAlloc_sz = FPAGE_SIZE * DMASIZE *sizeof(unsigned char);

int dstAlloc;
int srcAlloc;

unsigned int ref_dstAlloc;
unsigned int ref_srcAlloc;
unsigned int ref_highPpaList;
unsigned int ref_lowPpaList;
unsigned int ref_resPpaList;
unsigned int ref_resPpaList2;
unsigned int ref_mergedKtBuf;
unsigned int ref_invalPpaList;
unsigned int ref_searchKeyBuf;

unsigned int* dstBuffer;
unsigned int* srcBuffer;
unsigned int* lowPpaList_Buffer;
unsigned int* highPpaList_Buffer;
unsigned int* resPpaList_Buffer;
unsigned int* resPpaList_Buffer2;
unsigned int* invPpaList_Buffer;
unsigned int* mergeKt_Buffer;
unsigned int* searchKey_Buffer;


unsigned int* readBuffers[NUM_TAGS];
unsigned int* writeBuffers[NUM_TAGS];

TagTableEntry readTagTable[NUM_TAGS];
TagTableEntry writeTagTable[NUM_TAGS];
TagTableEntry eraseTagTable[NUM_TAGS];
//FlashStatusT flashStatus[NUM_SEGMENTS*NUM_PAGES_PER_BLK*NUM_LOGBLKS];

size_t blkmapAlloc_sz = sizeof(uint16_t) * NUM_SEGMENTS * NUM_LOGBLKS;
int blkmapAlloc;
unsigned int ref_blkmapAlloc;
char *ftlPtr = NULL;
uint16_t (*blkmap)[NUM_CHANNELS*NUM_CHIPS]; // 4096*64
uint16_t (*blkmgr)[NUM_CHIPS][NUM_BLOCKS];  // 8*8*4096

// temp
bdbm_sema_t global_lock;
bdbm_sema_t ftl_table_lock;
/***/

uint32_t bus_history[128];
uint32_t bus_rqtype[128];

/* interface for dm */
bdbm_dm_inf_t _bdbm_dm_inf = {
	.ptr_private = NULL,
	.probe = dm_nohost_probe,
	.open = dm_nohost_open,
	.close = dm_nohost_close,
	.make_req = dm_nohost_make_req,
	.make_reqs = NULL,
	.end_req = dm_nohost_end_req,
	.load = NULL,
	.store = NULL,
};

/* private data structure for dm */
typedef struct {
	bdbm_spinlock_t lock;
	bdbm_llm_req_t** llm_reqs;
	bdbm_llm_req_t* merge_req;
} dm_nohost_private_t;

dm_nohost_private_t* _priv = NULL;

/* global data structure */
extern bdbm_drv_info_t* _bdi_dm;
typedef struct merge_argues{
	sem_t merge_lock;
	unsigned int kt_num;
	unsigned int inv_num;
}merge_argues;

static merge_argues merge_req;

class FlashIndication: public FlashIndicationWrapper {
	public:
		FlashIndication (unsigned int id) : FlashIndicationWrapper (id) { }

		virtual void mergeDone(unsigned int numMergedKt, uint32_t numInvalAddr, uint64_t counter) {
			merge_req.kt_num=numMergedKt;
			merge_req.inv_num=numInvalAddr;
		}

		virtual void mergeFlushDone1(unsigned int num) {
			// num does not mean anything
			sem_post(&merge_req.merge_lock);
			sem_destroy(&merge_req.merge_lock);
		}
		virtual void mergeFlushDone2(unsigned int num) {
		}
#if JNI==4
		virtual void findKeyDone ( const uint16_t tag, const uint16_t status, const uint32_t ppa ) {
			bdbm_llm_req_t* r = _priv->llm_reqs[tag];
			_priv->llm_reqs[tag] = NULL;
			
			r->req_type=UINT8_MAX;
			if(status){
				r->logaddr.lpa[0]=ppa;
			}
			else{
				r->logaddr.lpa[0]=UINT32_MAX;
			}
			dm_nohost_end_req(_bdi_dm,r);
		}
#endif
		virtual void readDone (unsigned int tag){ //, unsigned int status) {
			int status = 0;
			//printf ("LOG: readdone: tag=%d status=%d\n", tag, status); fflush (stdout);
			//			bdbm_sema_lock (&global_lock);
			bdbm_llm_req_t* r = _priv->llm_reqs[tag];
			_priv->llm_reqs[tag] = NULL;
			//			bdbm_sema_unlock (&global_lock);
			if( r == NULL ){ printf("readDone: Ack Duplicate with tag=%d, status=%d\n", tag, status); fflush(stdout); 
				device->debugDumpReq(0);
				return; }
			//else {  printf("readDone: Ack  with tag=%d, status=%d\n", tag, status); fflush(stdout); }

			//sw_poller_enqueue((void*)r);
			dm_nohost_end_req (_bdi_dm, r);
		}

		virtual void writeDone (unsigned int tag){ //, unsigned int status) {
			int status = 0;
			//printf ("LOG: writedone: tag=%d status=%d\n", tag, status); fflush (stdout);
			//			bdbm_sema_lock (&global_lock);
			bdbm_llm_req_t* r = _priv->llm_reqs[tag];
			_priv->llm_reqs[tag] = NULL;
			//			bdbm_sema_unlock (&global_lock);
			if( r == NULL ) { printf("writeDone: Ack Duplicate with tag=%d, status=%d\n", tag, status); fflush(stdout); return; }

			//sw_poller_enqueue((void*)r);
			dm_nohost_end_req (_bdi_dm, r);
		}

		virtual void eraseDone (unsigned int tag, unsigned int status) {
			//printf ("LOG: eraseDone, tag=%d, status=%d\n", tag, status); fflush(stdout);
			//			bdbm_sema_lock (&global_lock);
			bdbm_llm_req_t* r = _priv->llm_reqs[tag];
			_priv->llm_reqs[tag] = NULL;
			//			bdbm_sema_unlock (&global_lock);
			if( r == NULL ) { printf("eraseDone: Ack Duplicate with tag=%d, status=%d\n", tag, status); fflush(stdout); return; }
			if(status==1){
				r->segnum=r->logaddr.lpa[0]/(1<<14);
				r->isbad=1;
			}
			else{
				r->isbad=0;
			}

			//sw_poller_enqueue((void*)r);
			dm_nohost_end_req (_bdi_dm, r);
		}

		virtual void debugDumpResp (unsigned int debug0, unsigned int debug1,  unsigned int debug2, unsigned int debug3, unsigned int debug4, unsigned int debug5) {
			fprintf(stderr, "LOG: DEBUG DUMP: gearSend = %u, gearRec = %u, aurSend = %u, aurRec = %u, readSend=%u, writeSend=%u\n", debug0, debug1, debug2, debug3, debug4, debug5);
		}

};

int __readFTLfromFile (const char* path, void* ptr) {
	FILE *fp;
	fp = fopen(path, "r");

	if (fp) {
		size_t read_size = fread( ptr, blkmapAlloc_sz*2, 1, fp);
		fclose(fp);
		if (read_size == 0)
		{
			fprintf(stderr, "error reading %s\n", path);
			return -1;
		}
	} else {
		fprintf(stderr, "error reading %s: file not exist\n", path);
		return -1;
	}

	return 0; // success
}

int __writeFTLtoFile (const char* path, void* ptr) {
	FILE *fp;
	fp = fopen(path, "w");

	if (fp) {
		size_t write_size = fwrite( ptr, blkmapAlloc_sz*2, 1, fp);
		fclose(fp);
		if (write_size==0)
		{
			fprintf(stderr, "error writing %s\n", path);
			return -1;
		}
	} else {
		fprintf(stderr, "error writing %s: file not exist\n", path);
		return -1;
	}

	return 0; // success
}



FlashIndication *indication;
DmaBuffer *srcDmaBuffer, *dstDmaBuffer, *blkmapDmaBuffer;
DmaBuffer *highPpaList, *lowPpaList, *resPpaList, *resPpaList2; // PPA Lists
DmaBuffer *mergedKtBuf, *invalPpaList;
DmaBuffer *searchKeyBuf;

uint32_t get_ppa_list_size(){ return 4*PPA_LIST_SIZE;}
uint32_t get_result_ppa_list_size(){
	return 2*get_ppa_list_size();
}
uint32_t get_result_kt_size(){
	return (unsigned int)8192*PPA_LIST_SIZE;
}
uint32_t get_inv_ppa_list_size(){
	return 500*4*PPA_LIST_SIZE;
}

uint32_t __dm_nohost_init_device (
		bdbm_drv_info_t* bdi, 
		bdbm_device_params_t* params)
{
	//sw_poller_init();
	fprintf(stderr, "Initializing Connectal & DMA...\n");

	device = new FlashRequestProxy(IfcNames_FlashRequestS2H);
	indication = new FlashIndication(IfcNames_FlashIndicationH2S);

	fprintf(stderr, "Main::allocating memory...\n");

	// Memory for DMA
#if defined(USE_ACP)
	fprintf(stderr, "USE_ACP = TRUE\n");
	srcDmaBuffer = new DmaBuffer(srcAlloc_sz);
	dstDmaBuffer = new DmaBuffer(dstAlloc_sz);

	highPpaList = new DmaBuffer(get_ppa_list_size());
	lowPpaList = new DmaBuffer(get_ppa_list_size());
	resPpaList = new DmaBuffer(get_result_ppa_list_size());
	resPpaList2 = new DmaBuffer(get_result_ppa_list_size());

	mergedKtBuf = new DmaBuffer(get_result_kt_size());
	invalPpaList = new DmaBuffer(get_inv_ppa_list_size());
#if JNI==4
	searchKeyBuf = new DmaBuffer(256*128);
#endif

#else
	fprintf(stderr, "USE_ACP = FALSE\n");
	srcDmaBuffer = new DmaBuffer(srcAlloc_sz, false);
	dstDmaBuffer = new DmaBuffer(dstAlloc_sz, false);

	highPpaList = new DmaBuffer(get_ppa_list_size(),false);
	lowPpaList = new DmaBuffer(get_ppa_list_size(),false);
	resPpaList = new DmaBuffer(get_result_ppa_list_size(),false);
	resPpaList2 = new DmaBuffer(get_result_ppa_list_size(),false);

	mergedKtBuf = new DmaBuffer(get_result_kt_size(),false);
	invalPpaList = new DmaBuffer(get_inv_ppa_list_size(),false);
#if JNI==4
	searchKeyBuf = new DmaBuffer(256*128,false);
#endif

#endif
	srcBuffer = (unsigned int*)srcDmaBuffer->buffer();
	dstBuffer = (unsigned int*)dstDmaBuffer->buffer();

	highPpaList_Buffer=(unsigned int*)highPpaList->buffer();
	lowPpaList_Buffer=(unsigned int*)lowPpaList->buffer();
	resPpaList_Buffer=(unsigned int*)resPpaList->buffer();
	resPpaList_Buffer2=(unsigned int*)resPpaList2->buffer();
	invPpaList_Buffer=(unsigned int*)invalPpaList->buffer();
	mergeKt_Buffer=(unsigned int*)mergedKtBuf->buffer();
#if JNI==4
	searchKey_Buffer=(unsigned int*)searchKeyBuf->buffer();
#endif


	fprintf(stderr, "USE_ACP = FALSE\n");

	// Memory for FTL
//#if defined(USE_ACP)
//	blkmapDmaBuffer = new DmaBuffer(blkmapAlloc_sz * 2);
//#else
//	blkmapDmaBuffer = new DmaBuffer(blkmapAlloc_sz * 2, false);
//#endif
//	ftlPtr = blkmapDmaBuffer->buffer();
//	blkmap = (uint16_t(*)[NUM_LOGBLKS]) (ftlPtr);  // blkmap[Seg#][LogBlk#]
//	blkmgr = (uint16_t(*)[NUM_CHIPS][NUM_BLOCKS])  (ftlPtr+blkmapAlloc_sz); // blkmgr[Bus][Chip][Block]

	fprintf(stderr, "Main::allocating memory finished!\n");

	dstDmaBuffer->cacheInvalidate(0, 1);
	srcDmaBuffer->cacheInvalidate(0, 1);
//	blkmapDmaBuffer->cacheInvalidate(0, 1);

	highPpaList->cacheInvalidate(0, 1);
	lowPpaList->cacheInvalidate(0, 1);
	resPpaList->cacheInvalidate(0, 1);
	resPpaList2->cacheInvalidate(0, 1);
	mergedKtBuf->cacheInvalidate(0, 1);
	invalPpaList->cacheInvalidate(0, 1);
#if JNI==4
	searchKeyBuf->cacheInvalidate(0,1);
#endif

	ref_dstAlloc = dstDmaBuffer->reference();
	fprintf(stderr,"dest %d\n",ref_dstAlloc);
	ref_srcAlloc = srcDmaBuffer->reference();
	fprintf(stderr,"src %d\n",ref_srcAlloc);

	ref_highPpaList = highPpaList->reference();
	fprintf(stderr,"highPpa %d size:%d\n",ref_highPpaList,get_ppa_list_size());
	ref_lowPpaList = lowPpaList->reference();
	fprintf(stderr,"lowPpa %d size:%d\n",ref_lowPpaList, get_ppa_list_size());
	ref_resPpaList = resPpaList->reference();
	fprintf(stderr,"resPpa %d size %d\n",ref_resPpaList,get_result_ppa_list_size());
	ref_resPpaList2 = resPpaList2->reference();
	fprintf(stderr,"resPpa2 %d size %d\n",ref_resPpaList2,get_result_ppa_list_size());

	ref_mergedKtBuf = mergedKtBuf->reference();
	fprintf(stderr,"mergeResult %d size %u page %d\n",ref_mergedKtBuf,get_result_kt_size(),get_result_kt_size()/8192);
	ref_invalPpaList = invalPpaList->reference();
	fprintf(stderr,"inv %d size %d\n",ref_invalPpaList,get_inv_ppa_list_size());
#if JNI==4
	ref_searchKeyBuf = searchKeyBuf->reference();
	fprintf(stderr,"key buf:%d size:%d\n",ref_searchKeyBuf,256*128);
#endif

	fprintf(stderr,"total memory:%d MB, %d MB\n",(srcAlloc_sz+dstAlloc_sz)/MBYTE,(get_ppa_list_size()*2+2*get_result_ppa_list_size()+get_result_kt_size()+get_inv_ppa_list_size())/MBYTE);

	device->setDmaWriteRef(ref_dstAlloc);
	device->setDmaReadRef(ref_srcAlloc);

	device->setDmaKtPpaRef(ref_highPpaList, ref_lowPpaList, ref_resPpaList, ref_resPpaList2);
	device->setDmaKtOutputRef(ref_mergedKtBuf, ref_invalPpaList);
#if JNI==4
	device->setDmaKtSearchRef(ref_searchKeyBuf);
#endif

	for (int t = 0; t < NUM_TAGS; t++) {
		readTagTable[t].busy = false;
		writeTagTable[t].busy = false;
		eraseTagTable[t].busy = false;

		int byteOffset = ( DMASIZE - NUM_TAGS + t ) * FPAGE_SIZE;
		//		int byteOffset = t * FPAGE_SIZE;
		readBuffers[t] = dstBuffer + byteOffset/sizeof(unsigned int);
		writeBuffers[t] = srcBuffer + byteOffset/sizeof(unsigned int);

	}

	//for (int lpa=0; lpa < NUM_SEGMENTS*NUM_LOGBLKS*NUM_PAGES_PER_BLK; lpa++) {
	//	flashStatus[lpa] = UNINIT;
	//}

	for (int t = 0; t < NUM_TAGS; t++) {
		for ( unsigned int i = 0; i < FPAGE_SIZE/sizeof(unsigned int); i++ ) {
			readBuffers[t][i] = 0xDEADBEEF;
			writeBuffers[t][i] = 0xBEEFDEAD;
		}
	}

	//#define MainClockPeriod 6 // Already defined in ConnectalProjectConfig.h:20 
	long actualFrequency=0;
	long requestedFrequency=1e9/MainClockPeriod;
	int status = setClockFrequency(0, requestedFrequency, &actualFrequency);
	fprintf(stderr, "Requested Freq: %5.2f, Actual Freq: %5.2f, status=%d\n"
			,(double)requestedFrequency*1.0e-6
			,(double)actualFrequency*1.0e-6,status);

	device->start(0);
	device->setDebugVals(0,0); //flag, delay

	device->debugDumpReq(0);
	sleep(1);

	return 0;
}

uint32_t dm_nohost_probe (
		bdbm_drv_info_t* bdi, 
		bdbm_device_params_t* params)
{
	dm_nohost_private_t* p = NULL;

	bdbm_msg ("[dm_nohost_probe] PROBE STARTED");

	/* setup NAND parameters according to users' inputs */
	*params = get_default_device_params ();

	/* create a private structure for ramdrive */
	if ((p = (dm_nohost_private_t*)bdbm_malloc
				(sizeof (dm_nohost_private_t))) == NULL) {
		bdbm_error ("bdbm_malloc failed");
		goto fail;
	}

	/* initialize the nohost device */
	if (__dm_nohost_init_device (bdi, params) != 0) {
		bdbm_error ("__dm_nohost_init_device() failed");
		bdbm_free (p);
		goto fail;
	}

	bdbm_sema_init (&global_lock);
	bdbm_sema_init (&ftl_table_lock);
	bdbm_sema_lock (&ftl_table_lock); // initially lock=0 to be used for waiting

	if ((p->llm_reqs = (bdbm_llm_req_t**)bdbm_zmalloc (
					sizeof (bdbm_llm_req_t*) * get_dev_tags())) == NULL) {
		bdbm_warning ("bdbm_zmalloc failed");
		goto fail;
	}

	/* OK! keep private info */
	bdi->ptr_dm_inf->ptr_private = (void*)p;
	_priv = p;

	bdbm_msg ("[dm_nohost_probe] PROBE DONE!");

//	if (__readFTLfromFile ("table.dump.0", ftlPtr) == 0) { //if exists, read from table.dump.0
//		bdbm_msg ("[dm_nohost_probe] MAP Upload to HW!" ); 
//	fflush(stdout);
//		device->uploadMap();
//		bdbm_sema_lock (&ftl_table_lock); // wait until Ack comes
//	} else {
//		bdbm_msg ("[dm_nohost_probe] MAP file not found" ); 
//		fflush(stdout);
//		goto fail;
//	}

	//koo
	if ( (writeDmaQ = new std::queue<int> ) == NULL ) {
		bdbm_msg ("[dm_nohost_probe] write dmaQ create failed" ); 
		fflush(stdout);
		goto fail;
	}

	if ( (readDmaQ = new std::queue<int> ) == NULL ) {
		bdbm_msg ("[dm_nohost_probe] read dmaQ create failed" ); 
		fflush(stdout);
		goto fail;
	}

	init_dmaQ(writeDmaQ);
	init_dmaQ(readDmaQ);

	//
	reqtime.tv_sec = 0;
	reqtime.tv_nsec = 1;
	return 0;

fail:
	return -1;
}

uint32_t dm_nohost_open (bdbm_drv_info_t* bdi)
{
	dm_nohost_private_t * p = (dm_nohost_private_t*)BDBM_DM_PRIV (bdi);

	bdbm_msg ("[dm_nohost_open] open done!");

	return 0;
}

void dm_nohost_close (bdbm_drv_info_t* bdi)
{
	dm_nohost_private_t* p = (dm_nohost_private_t*)BDBM_DM_PRIV (bdi);

	/* before closing, dump the table to table.dump.0 */
	bdbm_msg ("[dm_nohost_close] MAP Download from HW!" ); 
//	device->downloadMap();
//	bdbm_sema_lock (&ftl_table_lock); // wait until Ack comes
//
//	if(__writeFTLtoFile ("table.dump.0", ftlPtr) == 0) {
//		bdbm_msg("[dm_nohost_close] MAP successfully dumped to table.dump.0!");
//		fflush(stdout);
//	} else {
//		bdbm_msg("[dm_nohost_close] Error dumping FTL map to table.dump.0!");
//		fflush(stdout);
//	}
//
	bdbm_msg ("[dm_nohost_close] closed!");

	bdbm_free (p);

	bdbm_sema_free(&global_lock);
	bdbm_sema_free(&ftl_table_lock);

	delete device;
	delete srcDmaBuffer;
	delete dstDmaBuffer;
	delete blkmapDmaBuffer;
	//koo
	delete writeDmaQ;
	delete readDmaQ; 
	pthread_cond_destroy(&writeDmaQ_cond);
	pthread_cond_destroy(&readDmaQ_cond);	
	pthread_mutex_destroy(&writeDmaQ_mutx);	
	pthread_mutex_destroy(&readDmaQ_mutx);	
	//sw_poller_destroy();
	//
}
int readlockbywrite;
uint32_t dm_nohost_make_req (
		bdbm_drv_info_t* bdi, 
		bdbm_llm_req_t* r) 
{
	uint32_t punit_id, ret, i;
	dm_nohost_private_t* priv = (dm_nohost_private_t*)BDBM_DM_PRIV (bdi);
	//	bdbm_sema_lock (&global_lock);
	if (priv->llm_reqs[r->tag] == r) {
		// timeout & send the request again
		fprintf(stderr,"time out!\n");
	} 
	else if (priv->llm_reqs[r->tag] != NULL) {
		// busy tag error
		//		bdbm_sema_unlock (&global_lock);
		bdbm_error ("tag (%u) is busy...", r->tag);
		bdbm_bug_on (1);
		return -1;
	} else {
		priv->llm_reqs[r->tag] = r;
	}

	//	bdbm_sema_unlock (&global_lock);
	//usleep(1);


	//nanosleep(&reqtime, NULL);
	/* submit reqs to the device */

	//pthread_mutex_lock(&endR);
	//printf("lock!!\n");
	uint32_t bus, chip, block, page;
	/*bus check*/
	bus  = r->logaddr.lpa[0] & 0x7;

	pthread_mutex_lock(&bus_lock);
	if(bus_history[bus]){
		r->path_type+=2;
		if(r->req_type==REQTYPE_READ &&bus_rqtype[bus]==REQTYPE_WRITE){
			r->path_type+=4;
		}
	}
	bus_history[bus]=r->logaddr.lpa[0];
	bus_rqtype[bus]=r->req_type;
	pthread_mutex_unlock(&bus_lock);

	switch (r->req_type) {
		case REQTYPE_WRITE:
		case REQTYPE_RMW_WRITE:
		case REQTYPE_GC_WRITE:
			bus  = r->logaddr.lpa[0] & 0x7;
			chip = (r->logaddr.lpa[0] >> 3) & 0x7;
			page = (r->logaddr.lpa[0] >> 6) & 0xFF;
			block = (r->logaddr.lpa[0] >> 14);
			device->writePage(bus, chip, block, page, r->tag, r->dmaTag * FPAGE_SIZE);
			//pthread_mutex_unlock(&endR);
			//
			break;

		case REQTYPE_META_WRITE:
			//printf ("LOG: device->writePage, tag=%d lpa=%d\n", r->tag, r->logaddr.lpa[0]); fflush(stdout);
			bdbm_memcpy (writeBuffers[r->tag], r->fmain.kp_ptr[0], 8192);
			//device->writePage (r->tag, r->logaddr.lpa[0], (DMASIZE - NUM_TAGS + r->tag) * FPAGE_SIZE);

			bus  = r->logaddr.lpa[0] & 0x7;
			chip = (r->logaddr.lpa[0] >> 3) & 0x7;
			page = (r->logaddr.lpa[0] >> 6) & 0xFF;
			block = (r->logaddr.lpa[0] >> 14);
			device->writePage(bus, chip, block, page, r->tag, r->dmaTag * FPAGE_SIZE);
			//printf ("WRITE-LOG: %c %c\n", r->fmain.kp_ptr[0][0], r->fmain.kp_ptr[0][8191]); fflush(stdout);
			break;

		case REQTYPE_READ:
		case REQTYPE_READ_DUMMY:
		case REQTYPE_RMW_READ:
		case REQTYPE_GC_READ:
			//printf ("LOG: device->readPage, tag=%d lpa=%d\n", r->tag, r->logaddr.lpa[0]); fflush(stdout);
			//device->readPage (r->tag, r->logaddr.lpa[0], r->dmaTag * FPAGE_SIZE);

			bus  = r->logaddr.lpa[0] & 0x7;
			chip = (r->logaddr.lpa[0] >> 3) & 0x7;
			page = (r->logaddr.lpa[0] >> 6) & 0xFF;
			block = (r->logaddr.lpa[0] >> 14);
			device->readPage(bus, chip, block, page, r->tag, r->dmaTag * FPAGE_SIZE);
			break;

		case REQTYPE_META_READ:
			//printf ("LOG: device->readPage, tag=%d lpa=%d\n", r->tag, r->logaddr.lpa[0]); fflush(stdout);
			//device->readPage (r->tag, r->logaddr.lpa[0], (DMASIZE - NUM_TAGS + r->tag) * FPAGE_SIZE);

			bus  = r->logaddr.lpa[0] & 0x7;
			chip = (r->logaddr.lpa[0] >> 3) & 0x7;
			page = (r->logaddr.lpa[0] >> 6) & 0xFF;
			block = (r->logaddr.lpa[0] >> 14);
			device->readPage(bus, chip, block, page, r->tag, r->dmaTag * FPAGE_SIZE);
			break;

		case REQTYPE_GC_ERASE:
			//printf ("LOG: device->eraseBlock, tag=%d lpa=%d\n", r->tag, r->logaddr.lpa[0]); fflush(stdout);
			//device->eraseBlock (r->tag, r->logaddr.lpa[0]);
			bus  = r->logaddr.lpa[0] & 0x7;
			chip = (r->logaddr.lpa[0] >> 3) & 0x7;
			//page = (r->logaddr.lpa[0] >> 6) & 0xFF;
			block = (r->logaddr.lpa[0] >> 14);
			device->eraseBlock(bus, chip, block, r->tag);
			break;
		default:
			//		bdbm_sema_unlock (&global_lock);
			break;
	}
	//printf("unlock!\n");
	return 0;
}
struct timeval max_time1;
struct timeval adding;
int big_time_check1;
int end_counter;
void dm_nohost_end_req (
		bdbm_drv_info_t* bdi, 
		bdbm_llm_req_t* r)
{
	bdbm_bug_on (r == NULL);
	
	pthread_mutex_lock(&bus_lock);
	if(bus_history[r->logaddr.lpa[0]&0x7] == r->logaddr.lpa[0]){
		bus_history[r->logaddr.lpa[0]&0x7]=0;
		bus_rqtype[r->logaddr.lpa[0]&0x7]=0;
	}
	pthread_mutex_unlock(&bus_lock);

	if (r->req_type == REQTYPE_META_READ) {
		bdbm_memcpy (r->fmain.kp_ptr[0], readBuffers[r->tag], 8192);
		//printf ("READ-LOG: %c %c\n", r->fmain.kp_ptr[0][0], r->fmain.kp_ptr[0][8191]); fflush(stdout);
	}
	bdi->ptr_llm_inf->end_req (bdi, r);
}


//koo
void init_dmaQ (std::queue<int> *q) {
	for (int i = METANUM; i <DMASIZE-1; i++) {
		//for (int i = 0; i < DMASIZE; i++) {
		q->push(i);
	}
	return;
}

	//int alloc_cnt;
int alloc_dmaQ_tag (int type) {
	int dmaTag;
	// byteOffset;
	//uint8_t *buf;
	if (type == 1) { 		// write
		pthread_mutex_lock(&writeDmaQ_mutx);
		//while (writeDmaQ->size() < (1 << 14)) {
		while (writeDmaQ->empty()) {
			pthread_cond_wait(&writeDmaQ_cond, &writeDmaQ_mutx);
		}
		dmaTag = writeDmaQ->front();
		writeDmaQ->pop();
		pthread_mutex_unlock(&writeDmaQ_mutx);
		//	byteOffset = tag * FPAGE_SIZE;
		//	buf = (uint8_t*)(srcBuffer + byteOffset/sizeof(unsigned int));
	} 
	else if (type == 2) {		// read
		pthread_mutex_lock(&readDmaQ_mutx);
		while (readDmaQ->empty()) {
			pthread_cond_wait(&readDmaQ_cond, &readDmaQ_mutx);
		}
		dmaTag = readDmaQ->front();
		readDmaQ->pop();
		//	alloc_cnt++;
		//	printf("alloc cnt %d\n",alloc_cnt);
		//	pthread_mutex_unlock(&endR);
		pthread_mutex_unlock(&readDmaQ_mutx);
		//	byteOffset = tag * FPAGE_SIZE;
		//	buf = (uint8_t*)(dstBuffer + byteOffset/sizeof(unsigned int);
	}
	else if (type == 4) {		//compaction write
		}
	else if (type == 5) {		//compaction read
	}
	return dmaTag;
}
void free_dmaQ_tag (int type, int dmaTag) {
	switch(type) {
		case 1:
			pthread_mutex_lock(&writeDmaQ_mutx);
			writeDmaQ->push(dmaTag);
			pthread_cond_broadcast(&writeDmaQ_cond);
			pthread_mutex_unlock(&writeDmaQ_mutx);
			break;
		case 2:
			pthread_mutex_lock(&readDmaQ_mutx);
			readDmaQ->push(dmaTag);
			pthread_cond_broadcast(&readDmaQ_cond);
			pthread_mutex_unlock(&readDmaQ_mutx);
			break;
		case 4:
			break;
		case 5:
			break;
	}
	/*
	   if (type == 1 || type == 4) { 		// write / compaction write
	   pthread_mutex_lock(&writeDmaQ_mutx);
	   writeDmaQ->push(dmaTag);
	   pthread_cond_broadcast(&writeDmaQ_cond);
	   pthread_mutex_unlock(&writeDmaQ_mutx);
	   } 
	   else if (type == 2 || type == 5) {		// read / compaction read
	   pthread_mutex_lock(&readDmaQ_mutx);
	   readDmaQ->push(dmaTag);
	   pthread_cond_broadcast(&readDmaQ_cond);
	   pthread_mutex_unlock(&readDmaQ_mutx);
	   }
	 */
	return;
}

int dm_do_merge(unsigned int ht_num, unsigned int lt_num, unsigned int *kt_num, unsigned int *inv_num, uint32_t ppa_dma_num){
	if(ht_num==0 || lt_num==0){
		fprintf(stderr,"l:%d h:%d\n",lt_num,ht_num);
		abort();
	}

	if((lt_num + ht_num)*8*1024 >=get_result_kt_size()){
		fprintf(stderr,"over kt aborting %d\n",lt_num+ht_num);
		abort();	
	}
	if(lt_num > get_ppa_list_size() || ht_num > get_ppa_list_size()){
		fprintf(stderr,"over size %d %d\n",lt_num,ht_num);
		abort();
	}
	sem_init(&merge_req.merge_lock,0,0);

	device->startCompaction(ht_num,lt_num,ppa_dma_num);

	sem_wait(&merge_req.merge_lock);
	*kt_num=merge_req.kt_num;
	*inv_num=merge_req.inv_num;
	return 1;
}
#if JNI==4
int dm_do_hw_find(uint32_t ppa, uint32_t size, bdbm_llm_req_t* r){
	_priv->llm_reqs[r->tag]=r;
	return device->findKey(ppa,size,r->tag);
}
#endif

unsigned int *get_low_ppali(){
	return lowPpaList_Buffer;
}

unsigned int *get_high_ppali(){
	return highPpaList_Buffer;
}

unsigned int *get_res_ppali(){
	return resPpaList_Buffer;
}
unsigned int *get_res_ppali2(){
	return resPpaList_Buffer2;
}
unsigned int *get_inv_ppali(){
	return invPpaList_Buffer;
}

unsigned int *get_merged_kt(){
	return mergeKt_Buffer;
}
#if JNI==4
unsigned int *get_findKey_dma(){
	return searchKey_Buffer;
}
#endif

uint32_t get_dev_tags(){
	static const uint32_t tag=64;
	printf("my_tag:%d\n",tag);
	return tag;
}
