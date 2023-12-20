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
#include <stdio.h>
#include <stdint.h>

#else
#error Invalid Platform (KERNEL_MODE or USER_MODE)
#endif

#include "debug.h"
#include "dm_nohost.h"
#include "dev_params.h"

#include "utime.h"
#include "umemory.h"


/***/
#include "FlashIndication.h"
#include "FlashRequest.h"
#include "DmaBuffer.h"

#define FPAGE_SIZE (8192)
#define FPAGE_SIZE_VALID (8192)
#define NUM_TAGS 128

// for Table
#define NUM_BLOCKS 4096
#define NUM_SEGMENTS NUM_BLOCKS
#define NUM_CHANNELS 8
#define NUM_CHIPS 8
#define NUM_LOGBLKS (NUM_CHANNELS*NUM_CHIPS)
#define NUM_PAGES_PER_BLK 256

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
size_t dstAlloc_sz = FPAGE_SIZE * NUM_TAGS *sizeof(unsigned char);
size_t srcAlloc_sz = FPAGE_SIZE * NUM_TAGS *sizeof(unsigned char);

int dstAlloc;
int srcAlloc;

unsigned int ref_dstAlloc;
unsigned int ref_srcAlloc;

unsigned int* dstBuffer;
unsigned int* srcBuffer;

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
} dm_nohost_private_t;

dm_nohost_private_t* _priv = NULL;

/* global data structure */
extern bdbm_drv_info_t* _bdi_dm;


class FlashIndication: public FlashIndicationWrapper {
	public:
		FlashIndication (unsigned int id) : FlashIndicationWrapper (id) { }

		virtual void readDone (unsigned int tag, unsigned int status) {
			//printf ("LOG: readdone: tag=%d/%d status=%d\n", tag, r->tag, status); fflush (stdout);
//			bdbm_sema_lock (&global_lock);
			bdbm_llm_req_t* r = _priv->llm_reqs[tag];
			_priv->llm_reqs[tag] = NULL;
//			bdbm_sema_unlock (&global_lock);
			if( r == NULL ){ printf("readDone: Ack Duplicate with tag=%d, status=%d\n", tag, status); fflush(stdout); return; }
			//else {  printf("readDone: Ack  with tag=%d, status=%d\n", tag, status); fflush(stdout); }
			dm_nohost_end_req (_bdi_dm, r);
		}

		virtual void writeDone (unsigned int tag, unsigned int status) {
			//printf ("LOG: writedone: tag=%d/%d status=%d\n", tag, r->tag, status); fflush (stdout);
//			bdbm_sema_lock (&global_lock);
			bdbm_llm_req_t* r = _priv->llm_reqs[tag];
			_priv->llm_reqs[tag] = NULL;
//			bdbm_sema_unlock (&global_lock);
			if( r == NULL ) { printf("writeDone: Ack Duplicate with tag=%d, status=%d\n", tag, status); fflush(stdout); return; }
			dm_nohost_end_req (_bdi_dm, r);
		}

		virtual void eraseDone (unsigned int tag, unsigned int status) {
			//printf ("LOG: eraseDone, tag=%d/%d, status=%d\n", tag, r->tag, status); fflush(stdout);
//			bdbm_sema_lock (&global_lock);
			bdbm_llm_req_t* r = _priv->llm_reqs[tag];
			_priv->llm_reqs[tag] = NULL;
//			bdbm_sema_unlock (&global_lock);
			if( r == NULL ) { printf("eraseDone: Ack Duplicate with tag=%d, status=%d\n", tag, status); fflush(stdout); return; }
			dm_nohost_end_req (_bdi_dm, r);
		}

		virtual void debugDumpResp (unsigned int debug0, unsigned int debug1,  unsigned int debug2, unsigned int debug3, unsigned int debug4, unsigned int debug5) {
			fprintf(stderr, "LOG: DEBUG DUMP: gearSend = %d, gearRec = %d, aurSend = %d, aurRec = %d, readSend=%d, writeSend=%d\n", debug0, debug1, debug2, debug3, debug4, debug5);
		}

		virtual void uploadDone () {
			bdbm_msg ("[dm_nohost_probe] Map Upload(Host->FPGA) done!\n");
			bdbm_sema_unlock (&ftl_table_lock);
		}

		virtual void downloadDone () {
			bdbm_msg ("[dm_nohost_close] Map Download(FPGA->Host) done!\n");
			bdbm_sema_unlock (&ftl_table_lock);
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

uint32_t __dm_nohost_init_device (
	bdbm_drv_info_t* bdi, 
	bdbm_device_params_t* params)
{
	fprintf(stderr, "Initializing Connectal & DMA...\n");

	device = new FlashRequestProxy(IfcNames_FlashRequestS2H);
	indication = new FlashIndication(IfcNames_FlashIndicationH2S);

	fprintf(stderr, "Main::allocating memory...\n");
	
	// Memory for DMA
#if defined(USE_ACP)
	fprintf(stderr, "USE_ACP = TRUE\n");
	srcDmaBuffer = new DmaBuffer(srcAlloc_sz);
	dstDmaBuffer = new DmaBuffer(dstAlloc_sz);
#else
	fprintf(stderr, "USE_ACP = FALSE\n");
	srcDmaBuffer = new DmaBuffer(srcAlloc_sz, false);
	dstDmaBuffer = new DmaBuffer(dstAlloc_sz, false);
#endif
	srcBuffer = (unsigned int*)srcDmaBuffer->buffer();
	dstBuffer = (unsigned int*)dstDmaBuffer->buffer();


	// Memory for FTL
#if defined(USE_ACP)
	blkmapDmaBuffer = new DmaBuffer(blkmapAlloc_sz * 2);
#else
	blkmapDmaBuffer = new DmaBuffer(blkmapAlloc_sz * 2, false);
#endif
	ftlPtr = blkmapDmaBuffer->buffer();
	blkmap = (uint16_t(*)[NUM_LOGBLKS]) (ftlPtr);  // blkmap[Seg#][LogBlk#]
	blkmgr = (uint16_t(*)[NUM_CHIPS][NUM_BLOCKS])  (ftlPtr+blkmapAlloc_sz); // blkmgr[Bus][Chip][Block]
	
	fprintf(stderr, "Main::allocating memory finished!\n");
	
	dstDmaBuffer->cacheInvalidate(0, 1);
	srcDmaBuffer->cacheInvalidate(0, 1);
	blkmapDmaBuffer->cacheInvalidate(0, 1);

	ref_dstAlloc = dstDmaBuffer->reference();
	ref_srcAlloc = srcDmaBuffer->reference();
	ref_blkmapAlloc = blkmapDmaBuffer->reference();

	device->setDmaWriteRef(ref_dstAlloc);
	device->setDmaReadRef(ref_srcAlloc);
	device->setDmaMapRef(ref_blkmapAlloc);

	for (int t = 0; t < NUM_TAGS; t++) {
		readTagTable[t].busy = false;
		writeTagTable[t].busy = false;
		eraseTagTable[t].busy = false;

		int byteOffset = t * FPAGE_SIZE;
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
	uint32_t nr_punit;

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

	nr_punit = 128;
	if ((p->llm_reqs = (bdbm_llm_req_t**)bdbm_zmalloc (
			sizeof (bdbm_llm_req_t*) * nr_punit)) == NULL) {
		bdbm_warning ("bdbm_zmalloc failed");
		goto fail;
	}

	/* OK! keep private info */
	bdi->ptr_dm_inf->ptr_private = (void*)p;
	_priv = p;

	bdbm_msg ("[dm_nohost_probe] PROBE DONE!");

	if (__readFTLfromFile ("table.dump.0", ftlPtr) == 0) { //if exists, read from table.dump.0
		bdbm_msg ("[dm_nohost_probe] MAP Upload to HW!" ); 
		fflush(stdout);
		device->uploadMap();
		bdbm_sema_lock (&ftl_table_lock); // wait until Ack comes
	} else {
		bdbm_msg ("[dm_nohost_probe] MAP file not found" ); 
		fflush(stdout);
		goto fail;
	}

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
	device->downloadMap();
	bdbm_sema_lock (&ftl_table_lock); // wait until Ack comes
	
	if(__writeFTLtoFile ("table.dump.0", ftlPtr) == 0) {
		bdbm_msg("[dm_nohost_close] MAP successfully dumped to table.dump.0!");
		fflush(stdout);
	} else {
		bdbm_msg("[dm_nohost_close] Error dumping FTL map to table.dump.0!");
		fflush(stdout);
	}

	bdbm_msg ("[dm_nohost_close] closed!");

	bdbm_free (p);

	bdbm_sema_free(&global_lock);
	bdbm_sema_free(&ftl_table_lock);

	delete device;
	delete srcDmaBuffer;
	delete dstDmaBuffer;
	delete blkmapDmaBuffer;
}

uint32_t dm_nohost_make_req (
	bdbm_drv_info_t* bdi, 
	bdbm_llm_req_t* r)
{
	uint32_t punit_id, ret, i;
	dm_nohost_private_t* priv = (dm_nohost_private_t*)BDBM_DM_PRIV (bdi);

//	bdbm_sema_lock (&global_lock);
	if (priv->llm_reqs[r->tag] == r) {
		// timeout & send the request again
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

	/* submit reqs to the device */
	switch (r->req_type) {
	case REQTYPE_WRITE:
	case REQTYPE_RMW_WRITE:
	case REQTYPE_GC_WRITE:
	case REQTYPE_META_WRITE:
		//printf ("LOG: device->writePage, tag=%d lpa=%d\n", r->tag, r->logaddr.lpa[0]); fflush(stdout);
		bdbm_memcpy (writeBuffers[r->tag], r->fmain.kp_ptr[0], 8192);
		//printf ("WRITE-LOG: %c %c\n", r->fmain.kp_ptr[0][0], r->fmain.kp_ptr[0][8191]); fflush(stdout);
		device->writePage (r->tag, r->logaddr.lpa[0], r->tag * FPAGE_SIZE);
		break;

	case REQTYPE_READ:
	case REQTYPE_READ_DUMMY:
	case REQTYPE_RMW_READ:
	case REQTYPE_GC_READ:
	case REQTYPE_META_READ:
		//printf ("LOG: device->readPage, tag=%d lpa=%d\n", r->tag, r->logaddr.lpa[0]); fflush(stdout);
		device->readPage (r->tag, r->logaddr.lpa[0], r->tag * FPAGE_SIZE);
		break;

	case REQTYPE_GC_ERASE:
		//printf ("LOG: device->eraseBlock, tag=%d lpa=%d\n", r->tag, r->logaddr.lpa[0]); fflush(stdout);
		device->eraseBlock (r->tag, r->logaddr.lpa[0]);
		break;

	default:
//		bdbm_sema_unlock (&global_lock);
		break;
	}

	return 0;
}

void dm_nohost_end_req (
	bdbm_drv_info_t* bdi, 
	bdbm_llm_req_t* r)
{
	bdbm_bug_on (r == NULL);

	if (r->req_type == REQTYPE_READ || r->req_type == REQTYPE_GC_READ) {
		bdbm_memcpy (r->fmain.kp_ptr[0], readBuffers[r->tag], 8192);
		//printf ("READ-LOG: %c %c\n", r->fmain.kp_ptr[0][0], r->fmain.kp_ptr[0][8191]); fflush(stdout);
	}

	//bdbm_msg ("dm_nohost_end_req done");
	bdi->ptr_llm_inf->end_req (bdi, r);
}

