// Connectal DMA interface

#include "device_ifc/AmfIndication.h"
#include "device_ifc/AmfRequest.h"
#include "connectal_lib/cpp/DmaBuffer.h"

#include "DmaBuffer.h"

// Definition & other headers
#include "AmfManager.h"

// Page Size (Physical chip support up to 8224 bytes, but using 8192 bytes for now)
//  However, for DMA acks, we are using 8192*2 Bytes Buffer per TAG
#define FPAGE_SIZE (8192*2)
#define FPAGE_SIZE_VALID (8192)

#define TABLE_SZ ((sizeof(uint16_t)*NUM_SEGMENTS*NUM_VIRTBLKS))

#define DST_SZ FPAGE_SIZE*NUM_TAGS*sizeof(char)
#define SRC_SZ FPAGE_SIZE*NUM_TAGS*sizeof(char)

//
// USER INTERFACE
//
static AmfManager *_priv = NULL; // only visible in this file

AmfManager *AmfOpen(int mode) {
	if (_priv != NULL) {
		fprintf(stderr, "already open, so returning the same amfio_t\n");
		return _priv;
	}
	/*testtest*/
	AmfManager *tmp = new AmfManager(mode);
	return _priv; // _priv is registered automatically in Constructor
};

int __checkManager(AmfManager *am, const char* name) {
	if (am == NULL) {
		fprintf(stderr, "[%s] NULL pointer provided as a manager!\n", name);
		return -1;
	}
	else if (am != _priv) {
		fprintf(stderr, "[%s] Unknown Manager or Already Closed!\n", name);
		return -1;
	}
	return 0; // normal
}

int AmfClose(AmfManager *am) {
	if (__checkManager(am, "AmfClose")) { return -1; } // check am and _priv
	
	delete _priv; // closing the Manager;
	_priv = NULL;

	return 0;
}

bool IsAmfBusy(AmfManager *am) {
	return am->IsBusy();
}

int AmfRead(AmfManager* am, uint32_t lpa, char *data, void *req) {
	if (__checkManager(am, "AmfRead")) { return -1; } // check am and _priv

	am->Read(lpa, data, req);
	return 0;
}

int AmfWrite(AmfManager* am, uint32_t lpa, char *data, void *req) {
	if (__checkManager(am, "AmfWrite")) { return -1; } // check am and _priv

	am->Write(lpa, data, req);
	return 0;
}

int AmfErase(AmfManager* am, uint32_t lpa, void *req) {
	if (__checkManager(am, "AmfErase")) { return -1; } // check am and _priv

	am->Erase(lpa, req);
	return 0;
}

int SetReadCb(AmfManager* am,  void (*cbOk)(void*), void (*cbErr)(void*)) {
	if (__checkManager(am, "SetReadCb")) { return -1; } // check am and _priv

	am->SetReadCb(cbOk, cbErr);
	return 0;
}

int SetWriteCb(AmfManager* am, void (*cbOk)(void*), void (*cbErr)(void*)) {
	if (__checkManager(am, "SetWriteCb")) { return -1; } // check am and _priv

	am->SetWriteCb(cbOk, cbErr);
	return 0;
}

int SetEraseCb(AmfManager* am, void (*cbOk)(void*), void (*cbErr)(void*)) {
	if (__checkManager(am, "SetEraseCb")) { return -1; } // check am and _priv

	am->SetEraseCb(cbOk, cbErr);
	return 0;
}

//
// AmfDeviceAck (Device Ack Definition)
//

class AmfDeviceAck: public AmfIndicationWrapper {
	public:
		void debugDumpResp (uint32_t debug0, uint32_t debug1,  uint32_t debug2, uint32_t debug3, uint32_t debug4, uint32_t debug5) {
			fprintf(stderr, "LOG: DEBUG DUMP: gearSend = %u, gearRec = %u, aurSend = %u, aurRec = %u, readSend=%u, writeSend=%u\n"
					, debug0, debug1, debug2, debug3, debug4, debug5);
		}

		void readDone(uint8_t tag) {
			fprintf(stderr, "**ERROR: this readDone should have never come\n");
		}

		void writeDone(uint8_t tag) {
			AmfManager::InternalReqT *cur_req = &_priv->reqs[tag];

			if (cur_req->busy == false || cur_req->cmd != AmfWRITE) {
				// something wrong
				fprintf(stderr, "**ERROR @ writeDone: tag not used or user for other cmd\n");
				return;
			}

			if (_priv->wCb) _priv->wCb(cur_req->user_req);

			cur_req->busy=false;

			pthread_mutex_lock(&_priv->tagMutex);
			// if(_priv->tagQ.empty()) pthread_cond_signal(&_priv->tagWaitCond);
			_priv->tagQ.push(tag);
			pthread_cond_broadcast(&_priv->tagWaitCond);
			pthread_mutex_unlock(&_priv->tagMutex);
		}

		void eraseDone(uint8_t tag, uint8_t status) {
			uint8_t isRawCmd = (status & 2)>>1;
			uint8_t isBadBlock = status & 1;

			AmfManager::InternalReqT *cur_req = &_priv->reqs[tag];

			if (cur_req->busy == false || cur_req->cmd != AmfERASE || cur_req->isRaw != (bool)isRawCmd) {
				// something wrong
				fprintf(stderr, "**ERROR @ eraseDone: tag not used or user for other cmd or raw cmd error\n");
				return;
			}

			if (isRawCmd) {
				// Raw Cmds for AFTL tables
				_priv->eRawCb(tag, isBadBlock?true:false);
			} else {
				// Normal IO by user
				if (isBadBlock) {
					AmfRequestT myReq;
					myReq.cmd = AmfMARKBAD;
					myReq.lpa = cur_req->lpa;
					pthread_mutex_lock(&_priv->deviceLock);
					_priv->dev->makeReq(myReq);
					pthread_mutex_unlock(&_priv->deviceLock);
					fprintf(stderr, "WARNING: ** normal->bad block detected **\n");
				}
				if (_priv->eCb) _priv->eCb(cur_req->user_req);
			}

			cur_req->busy=false;

			pthread_mutex_lock(&_priv->tagMutex);
			// if(_priv->tagQ.empty()) pthread_cond_signal(&_priv->tagWaitCond);
			_priv->tagQ.push(tag);
			pthread_cond_broadcast(&_priv->tagWaitCond);
			pthread_mutex_unlock(&_priv->tagMutex);
		}


		void respAftlFailed(AmfRequestT resp) {

			AmfManager::InternalReqT *cur_req = &_priv->reqs[resp.tag];
			if (cur_req->busy == false || cur_req->cmd != resp.cmd) {
				// something wrong
				fprintf(stderr, "**ERROR @ respAftlFailed: tag not used or user for other cmd\n");
				return;
			}

			if (resp.cmd == AmfREAD) {
				if (_priv->rErrCb) _priv->rErrCb(cur_req->user_req);
			}
			else if (resp.cmd ==  AmfWRITE) {
				if (_priv->wErrCb) _priv->wErrCb(cur_req->user_req);
			}
			else if (resp.cmd == AmfERASE) {
				if (_priv->eErrCb) _priv->eErrCb(cur_req->user_req);
			}
			else {
				fprintf(stderr, "**ERROR @ respAftlFailed: unknown AFTL Cmd\n");
			}

			cur_req->busy=false;

			pthread_mutex_lock(&_priv->tagMutex);
			// if(_priv->tagQ.empty()) pthread_cond_signal(&_priv->tagWaitCond);
			_priv->tagQ.push(resp.tag);
			pthread_cond_broadcast(&_priv->tagWaitCond);
			pthread_mutex_unlock(&_priv->tagMutex);
		}

		void respReadMapping(uint8_t allocated, uint16_t block_num) {
			// TODO
			int virt_blk = mappingReads%NUM_VIRTBLKS;
			int seg = mappingReads/NUM_VIRTBLKS;

			_priv->mapStatus[seg][virt_blk] = (allocated)?AmfManager::ALLOCATED: AmfManager::NOT_ALLOCATED;
			_priv->mappedBlock[seg][virt_blk] = block_num & 0x3fff;

			if (mappingReads >= NUM_SEGMENTS*NUM_VIRTBLKS-1) {
				mappingReads = 0;
				sem_post(&_priv->aftlReadSem);
			} else {
				mappingReads++;
			}
		}

		void respReadBlkInfo(const uint16_t* blkinfo_vec ) {
			// TODO
			//fprintf(stderr, "respReadBlkInfo:\n");
			for (int i =0; i<8; i++) {
				uint8_t card = (blkInfoReads >> 15);
				uint8_t bus = (blkInfoReads >> 12) & 7;
				uint8_t chip = (blkInfoReads >> 9) & 7;
				uint16_t blk = (blkInfoReads & 511)*8+i;

				_priv->blockStatus[card][bus][chip][blk] = (AmfManager::BlockStatusT)(blkinfo_vec[i]>>14);
				_priv->blockPE[card][bus][chip][blk] = blkinfo_vec[i] & 0x3fff;

			}

			int maxBlkInfoReads = NUM_CARDS*NUM_BUSES*CHIPS_PER_BUS*BLOCKS_PER_CHIP/8;
			if (blkInfoReads >= maxBlkInfoReads-1) {
				blkInfoReads = 0;
				sem_post(&_priv->aftlReadSem);
			} else {
				blkInfoReads++;
			}
		}

		void respAftlLoaded(uint8_t resp) {
			_priv->aftlLoaded = (resp)?true:false;

			sem_post(&_priv->aftlStatusSem);
		}

		AmfDeviceAck(unsigned int id, PortalTransportFunctions *transport = 0, void *param = 0, PortalPoller *poller = 0) : AmfIndicationWrapper(id, transport, param, poller), mappingReads(0), blkInfoReads(0){}

	private:
		int mappingReads;
		int blkInfoReads;
};


// Static member funcion used by Read Done Thread 
void *AmfManager::PollReadBuffer(void *self) {
	uint16_t tag = NUM_TAGS-1;
	uint32_t flag_word_offset = FPAGE_SIZE_VALID/sizeof(uint32_t);

	AmfManager *am = (AmfManager*)self;

	while (!am->killChecker) {
		tag = (tag+1)%NUM_TAGS;
		InternalReqT *cur_req = &am->reqs[tag];

		if (am->flashReadBuf[tag][flag_word_offset] == (uint32_t)-1 ) {
			// Clear done flag
			am->flashReadBuf[tag][flag_word_offset] = 0;

			if(cur_req->busy == false || cur_req->cmd != AmfREAD) {
				// something wrong
				fprintf(stderr, "**ERROR @ readDone: tag not used or user for other cmd\n");
				continue;
			}

			memcpy(cur_req->data, am->flashReadBuf[tag], FPAGE_SIZE_VALID);

			// Read Callback if defined
			if (am->rCb) am->rCb(cur_req->user_req);

			// Collect tag & clear in-flight req
			cur_req->busy=false;

			pthread_mutex_lock(&am->tagMutex);
			// if(_priv->tagQ.empty()) pthread_cond_signal(&_priv->tagWaitCond);
			am->tagQ.push(tag);
			pthread_cond_broadcast(&am->tagWaitCond);
			pthread_mutex_unlock(&am->tagMutex);

		}
	}
	return NULL;
}

//
// AmfManager
//
AmfManager::AmfManager(int mode) : killChecker(false), aftlLoaded(false), rCb(NULL), wCb(NULL), eCb(NULL), rErrCb(NULL), wErrCb(NULL), eErrCb(NULL) {

	if ( mode < 0 || mode >= 3) {
		fprintf(stderr, "[AmfManager] valid open mode: 0, 1, 2");
		exit(-1);
	}

	_priv = this;

	sem_init(&aftlStatusSem, 0, 0);
	sem_init(&aftlReadSem, 0, 0);

	fprintf(stderr, "Initializing Connectal & DMA...\n");

	// Device initialization
	dev = new AmfRequestProxy(IfcNames_AmfRequestS2H);
	ind = new AmfDeviceAck(IfcNames_AmfIndicationH2S);

	pthread_mutex_init(&deviceLock, NULL);

	// Memory-allocation for DMA
	dstDmaBuf = new DmaBuffer(DST_SZ);
	srcDmaBuf = new DmaBuffer(SRC_SZ);

	char *rBuf =  dstDmaBuf->buffer();
	char *wBuf = srcDmaBuf->buffer();

	for (int t = 0; t < NUM_TAGS; t++) {
		flashReadBuf[t] = (uint32_t*)(rBuf + t*FPAGE_SIZE);
		flashWriteBuf[t] = (uint32_t*)(wBuf + t*FPAGE_SIZE);
	}

	dstDmaBuf->cacheInvalidate(0, 1);
	srcDmaBuf->cacheInvalidate(0, 1);

	uint32_t ref_srcAlloc = srcDmaBuf->reference();
	uint32_t ref_dstAlloc = dstDmaBuf->reference();

	fprintf(stderr, "ref_dstAlloc = %x\n", ref_dstAlloc); 
	fprintf(stderr, "ref_srcAlloc = %x\n", ref_srcAlloc); 
	
	dev->setDmaWriteRef(ref_dstAlloc);
	dev->setDmaReadRef(ref_srcAlloc);

	// read done checker thread
	if(pthread_create(&readChecker, NULL, PollReadBuffer, this)) {
		fprintf(stderr, "Error creating read checker thread\n");

		delete dstDmaBuf;
		delete srcDmaBuf;
		delete dev;
		delete ind;
		exit(-1);
	}
	fprintf(stderr, "read checker thread created!\n"); 

	for (int t = 0; t < NUM_TAGS; t++) {
		reqs[t].busy = false;
		reqs[t].user_req = NULL;
		tagQ.push(t);
	}
	pthread_mutex_init(&tagMutex, NULL);
	pthread_cond_init(&tagWaitCond, 0);


	fprintf(stderr, "check aftl status and initilize the device\n"); 
	if (mode == 0) {
		if (!__isAftlTableLoaded()) {
			// if device table not programmed, must use local "aftl.bin"
			if (AftlFileToDev("aftl.bin")) {
				fprintf(stderr, "You must provide aftl.bin when mode=0 & device-aftl not programmed\n");

				delete dstDmaBuf;
				delete srcDmaBuf;
				delete dev;
				delete ind;
				exit(-1);
			}
			dev->setAftlLoaded();
			aftlLoaded=true;
		}
		fprintf(stderr, "AMF Ready to use\n"); 
	} else if (mode == 1) {
		// erase what is mapped
		if (!__isAftlTableLoaded()) {

			if (AftlFileToDev("aftl.bin")) {
				fprintf(stderr, "You must provide aftl.bin when mode=1 & device-aftl not programmed\n");

				delete dstDmaBuf;
				delete srcDmaBuf;
				delete dev;
				delete ind;
				exit(-1);
			}
			dev->setAftlLoaded();
			aftlLoaded=true;
		} else {
			__loadTableFromDev();
		}
		fprintf(stderr, "AFTL status OK & erasing mapped lpas\n"); 
		// erase only mapped info
		for (int seg = 0; seg < NUM_SEGMENTS; seg++) {
			for (int virt_blk = 0; virt_blk < NUM_VIRTBLKS; virt_blk++) {
				if (mapStatus[seg][virt_blk] == ALLOCATED) {
					uint32_t lpa = (virt_blk & (NUM_VIRTBLKS-1)) | (seg << 15); // reconstruct LPA
					Erase(lpa, NULL);
				}
			}
		}

		int elapsed = 10000;
		while (true) {
			usleep(100);
			if (elapsed == 0) {
				elapsed = 10000;
			} else {
				elapsed--;
			}
			if (tagQ.size() == NUM_TAGS) break;
		}
		fprintf(stderr, "Mapped entry erased!!\n");


	} else {
		fprintf(stderr, "Resetting device; erase all blocks\n"); 
		fprintf(stderr, "Existing AFTL & aftl.bin PE honored if exists\n"); 

		if (!__isAftlTableLoaded()) {
			__readTableFromFile("aftl.bin");
		} else {
			__loadTableFromDev();
		}

		// eraseAll & initialize no matter what
		for (int blk = 0; blk <  BLOCKS_PER_CHIP; blk++) {
			for (int chip = 0; chip < CHIPS_PER_BUS; chip++) {
				for (int bus = 0; bus < NUM_BUSES; bus++) {
					for (int card=0; card < NUM_CARDS; card++) {
						EraseRaw(card, bus, chip, blk);
					}
				}
			}
		}

		for (int seg=0; seg < NUM_SEGMENTS; seg++)  {
			for (int virt_blk = 0; virt_blk < NUM_VIRTBLKS; virt_blk++) {
				mapStatus[seg][virt_blk] = NOT_ALLOCATED;
			}
		}

		int elapsed = 10000;
		while (true) {
			usleep(100);
			if (elapsed == 0) {
				elapsed = 10000;
			} else {
				elapsed--;
			}
			if (tagQ.size() == NUM_TAGS) break;
		}
		fprintf(stderr, "All blocks erased!!\n");

		__pushTableToDev();
		dev->setAftlLoaded();
		aftlLoaded=true;
	}


	fprintf(stderr, "Debug Stats:\n" ); 
	dev->debugDumpReq(0);   // echo-back debug message
	dev->debugDumpReq(1);   // echo-back debug message
	sleep(1);

	fprintf(stderr, "Done initializing Hardware & DMA!\n" ); 
}

AmfManager::~AmfManager() {
	fprintf(stderr, "Debug Stats:\n" ); 
	dev->debugDumpReq(0);   // echo-back debug message
	dev->debugDumpReq(1);   // echo-back debug message
	sleep(1);

	if (AftlDevToFile("aftl.bin")) {
		fprintf(stderr, "[AmfManager] On close: failed to dump AFTL table data to aftl.bin\n");
	}

	killChecker = true;
	pthread_join(readChecker, NULL);

	pthread_mutex_destroy(&deviceLock);
	pthread_mutex_destroy(&tagMutex);
	pthread_cond_destroy(&tagWaitCond);

	sem_destroy(&aftlStatusSem);
	sem_destroy(&aftlReadSem);

	delete dstDmaBuf;
	delete srcDmaBuf;
	delete dev;
	delete ind;
}

void AmfManager::Read(uint32_t lpa, char *data, void *req) {
	int tag = __getTag();
	reqs[tag].busy = true;
	reqs[tag].cmd = AmfREAD;
	reqs[tag].isRaw = false;
	reqs[tag].user_req = req;
	reqs[tag].data = data;
	reqs[tag].lpa = lpa;

	AmfRequestT myReq; // request used for device
	myReq.tag = tag;
	myReq.cmd = AmfREAD;
	myReq.lpa = lpa;

	pthread_mutex_lock(&deviceLock);
	dev->makeReq(myReq);
	pthread_mutex_unlock(&deviceLock);
}

void AmfManager::Write(uint32_t lpa, char *data, void *req) {
	int tag = __getTag();
	reqs[tag].busy = true;
	reqs[tag].cmd = AmfWRITE;
	reqs[tag].isRaw = false;
	reqs[tag].user_req = req;
	reqs[tag].lpa = lpa;

	memcpy(flashWriteBuf[tag], data, FPAGE_SIZE_VALID);

	AmfRequestT myReq; // request used for device
	myReq.tag = tag;
	myReq.cmd = AmfWRITE;
	myReq.lpa = lpa;

	pthread_mutex_lock(&deviceLock);
	dev->makeReq(myReq);
	pthread_mutex_unlock(&deviceLock);
}

void AmfManager::Erase(uint32_t lpa, void *req) {
	int tag = __getTag();
	reqs[tag].busy = true;
	reqs[tag].cmd = AmfERASE;
	reqs[tag].isRaw = false;
	reqs[tag].user_req = req;
	reqs[tag].lpa = lpa;

	AmfRequestT myReq; // request used for device
	myReq.tag = tag;
	myReq.cmd = AmfERASE;
	myReq.lpa = lpa;

	pthread_mutex_lock(&deviceLock);
	dev->makeReq(myReq);
	pthread_mutex_unlock(&deviceLock);
}

// Only for initializing device
void AmfManager::EraseRaw(int card, int bus, int chip, int block) {
	int tag = __getTag();
	reqs[tag].busy = true;
	reqs[tag].cmd = AmfERASE;
	reqs[tag].isRaw = true;
	reqs[tag].user_req = NULL;

	eraseRawTable[tag].card = card;
	eraseRawTable[tag].bus = bus;
	eraseRawTable[tag].chip = chip;
	eraseRawTable[tag].block = block;

	pthread_mutex_lock(&deviceLock);
	dev->eraseRawBlock(card, bus, chip, block, tag);
	pthread_mutex_unlock(&deviceLock);
}


void AmfManager::eRawCb(int tag, bool isBadBlock) {
	TagTableEntry entry = eraseRawTable[tag];
	blockStatus[entry.card][entry.bus][entry.chip][entry.block] = isBadBlock? BAD: FREE;
	blockPE[entry.card][entry.bus][entry.chip][entry.block]++;
}

bool AmfManager::IsBusy() {
	pthread_mutex_lock(&tagMutex);
	bool busy = tagQ.size() != NUM_TAGS;
	//printf("tag Q: %d - NumTags:%d\n",tagQ.size(), NUM_TAGS);
	pthread_mutex_unlock(&tagMutex);

	return (tagQ.size() != NUM_TAGS);
}

int AmfManager::AftlFileToDev(const char *path) {
	if(__readTableFromFile(path)){
		return -1;
	}
	__pushTableToDev();
	return 0;
}


int AmfManager::AftlDevToFile(const char *path) {

	__loadTableFromDev();
	if(__writeTableToFile(path)) {
		return -1;
	}
	return 0;
}


bool AmfManager::__isAftlTableLoaded() {
	dev->askAftlLoaded();
	sem_wait(&aftlStatusSem);

	return aftlLoaded;
}

void AmfManager::__loadTableFromDev() {
	int map_cnt = 0;
	for (int seg=0; seg < NUM_SEGMENTS; seg++)  {
		for (int virt_blk = 0; virt_blk < NUM_VIRTBLKS; virt_blk++) {
			dev->readMapping(map_cnt);
			map_cnt++;
		}
	}
	sem_wait(&aftlReadSem);

	int blk_cnt = 0;
	for (int card=0; card < NUM_CARDS; card++) {
		for (int bus = 0; bus < NUM_BUSES; bus++) {
			for (int chip = 0; chip < CHIPS_PER_BUS; chip++) {
				for (int blk = 0; blk < BLOCKS_PER_CHIP; blk++) {

					int idx = blk % 8;
					if (idx == 7) {
						dev->readBlkInfo((uint16_t)(blk_cnt>>3));
					}

					blk_cnt++;
				}
			}
		}
	}
	sem_wait(&aftlReadSem);
}

void AmfManager::__pushTableToDev() {
	uint32_t map_cnt = 0;
	for (int seg=0; seg < NUM_SEGMENTS; seg++)  {
		for (int virt_blk = 0; virt_blk < NUM_VIRTBLKS; virt_blk++) {
			dev->updateMapping(map_cnt, (mapStatus[seg][virt_blk]==ALLOCATED)?1:0, mappedBlock[seg][virt_blk]);
			map_cnt++;
		}
	}

	uint32_t blk_cnt = 0;
	for (int card=0; card < NUM_CARDS; card++)  {
		for (int bus = 0; bus < NUM_BUSES; bus++) {
			for (int chip = 0; chip < CHIPS_PER_BUS; chip++) {
				uint16_t entry_vec[8];

				for (int blk = 0; blk < BLOCKS_PER_CHIP; blk++) {

					int idx = blk % 8;
					entry_vec[idx] = (uint16_t)(blockStatus[card][bus][chip][blk] << 14);

					if (idx == 7) {
						dev->updateBlkInfo((uint16_t)(blk_cnt>>3), entry_vec);
					}

					blk_cnt++;
				}
			}
		}
	}
}

int AmfManager::__readTableFromFile (const char *path) {
	char *filebuf = new char[2*TABLE_SZ];

	uint16_t (*mapRaw)[NUM_VIRTBLKS] = (uint16_t(*)[NUM_VIRTBLKS])(filebuf);
	uint16_t (*blkInfoRaw)[NUM_BUSES][CHIPS_PER_BUS][BLOCKS_PER_CHIP] = (uint16_t(*)[NUM_BUSES][CHIPS_PER_BUS][BLOCKS_PER_CHIP])(filebuf+TABLE_SZ);

	FILE *fp = fopen(path, "r");
	if(fp) {
		size_t rsz = fread(filebuf, 2*TABLE_SZ, 1, fp);
		fclose(fp);

		if (rsz == 0) {
			fprintf(stderr, "[FileToAftl] error reading %s (size?)\n", path);
			delete [] filebuf;
			return -1;
		}
	} else {
		fprintf(stderr, "[FileToAftl] %s does not exist\n", path);
		delete [] filebuf;
		return -1;
	}

	for (int i = 0; i < NUM_SEGMENTS; i++) {
		for (int j = 0; j < NUM_VIRTBLKS; j++) {
			mapStatus[i][j] = (MapStatusT)(mapRaw[i][j] >> 14);
			mappedBlock[i][j] = mapRaw[i][j] & 0x3fff;
		}
	}

	for (int i = 0; i < NUM_CARDS; i++) {
		for (int j = 0; j < NUM_BUSES; j++) {
			for (int k = 0; k < CHIPS_PER_BUS; k++) {
				for (int l = 0; l < BLOCKS_PER_CHIP; l++) {
					blockStatus[i][j][k][l] = (BlockStatusT)(blkInfoRaw[i][j][k][l] >> 14);
					blockPE[i][j][k][l] = blkInfoRaw[i][j][k][l] & 0x3fff;
				}
			}
		}
	}

	delete [] filebuf;
	return 0;
}

int AmfManager::__writeTableToFile (const char* path) {
	char *filebuf = new char[2*TABLE_SZ];

	uint16_t (*mapRaw)[NUM_VIRTBLKS] = (uint16_t(*)[NUM_VIRTBLKS])(filebuf);
	uint16_t (*blkInfoRaw)[NUM_BUSES][CHIPS_PER_BUS][BLOCKS_PER_CHIP] = (uint16_t(*)[NUM_BUSES][CHIPS_PER_BUS][BLOCKS_PER_CHIP])(filebuf+TABLE_SZ);

	for (int i = 0; i < NUM_SEGMENTS; i++) {
		for (int j = 0; j < NUM_VIRTBLKS; j++) {
			mapRaw[i][j] = (mapStatus[i][j] << 14) | (mappedBlock[i][j] & 0x3fff);
		}
	}

	for (int i = 0; i < NUM_CARDS; i++) {
		for (int j = 0; j < NUM_BUSES; j++) {
			for (int k = 0; k < CHIPS_PER_BUS; k++) {
				for (int l = 0; l < BLOCKS_PER_CHIP; l++) {
					blkInfoRaw[i][j][k][l] = (blockStatus[i][j][k][l] << 14) | (blockPE[i][j][k][l] & 0x3fff);
				}
			}
		}
	}

	FILE *fp = fopen(path, "w");
	if(fp) {
		size_t wsz = fwrite(filebuf, 2*TABLE_SZ, 1, fp);
		fclose(fp);

		if (wsz == 0) {
			fprintf(stderr, "[AftlToFile] Error writing %s\n", path);
			delete [] filebuf;
			return -1;
		}
	} else {
		fprintf(stderr, "[AftlToFile] %s could not be open or created\n", path);
		delete [] filebuf;
		return -1;
	}


	delete [] filebuf;
	return 0;
}

int AmfManager::__getTag () {
	int tag = -1;
	pthread_mutex_lock(&tagMutex);
	while (tagQ.empty()) {
		pthread_cond_wait(&tagWaitCond, &tagMutex);
	}
	tag = tagQ.front();
	tagQ.pop();
	pthread_mutex_unlock(&tagMutex);

	return tag;
}
