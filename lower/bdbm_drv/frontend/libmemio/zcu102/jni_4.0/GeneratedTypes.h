#ifndef __GENERATED_TYPES__
#define __GENERATED_TYPES__
#include "portal.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum CompResult { GT, LT, EQ,  } CompResult;
typedef enum SearchStatus { LOAD_KEY, FIND_KEY1, FIND_KEY2, FLUSH_ONE_ENTRY, FLUSH_TABLE,  } SearchStatus;
typedef uint8_t KeyBeatT;
typedef enum ChipState { ST_CMD, ST_OP_DELAY, ST_BUS_RESERVE, ST_WRITE_BUS_RESERVE, ST_WRITE_REQ, ST_ERASE, ST_ERROR, ST_READ_TRANSFER, ST_WRITE_DATA, ST_WRITE_ACK, ST_READ_DATA,  } ChipState;
typedef enum ChannelType { ChannelType_Read, ChannelType_Write,  } ChannelType;
typedef struct DmaDbgRec {
    uint32_t x : 32;
    uint32_t y : 32;
    uint32_t z : 32;
    uint32_t w : 32;
} DmaDbgRec;
typedef enum DmaErrorType { DmaErrorNone, DmaErrorSGLIdOutOfRange_r, DmaErrorSGLIdOutOfRange_w, DmaErrorMMUOutOfRange_r, DmaErrorMMUOutOfRange_w, DmaErrorOffsetOutOfRange, DmaErrorSGLIdInvalid, DmaErrorTileTagOutOfRange,  } DmaErrorType;
typedef uint32_t SpecialTypeForSendingFd;
typedef enum TileState { Idle, Stopped, Running,  } TileState;
typedef struct TileControl {
    uint8_t tile : 2;
    TileState state;
} TileControl;
typedef enum IfcNames { IfcNamesNone=0, PlatformIfcNames_MemServerRequestS2H=1, PlatformIfcNames_MMURequestS2H=2, PlatformIfcNames_MemServerIndicationH2S=3, PlatformIfcNames_MMUIndicationH2S=4, IfcNames_FlashIndicationH2S=5, IfcNames_FlashRequestS2H=6,  } IfcNames;


int MemServerRequest_addrTrans ( struct PortalInternal *p, const uint32_t sglId, const uint32_t offset );
int MemServerRequest_setTileState ( struct PortalInternal *p, const TileControl tc );
int MemServerRequest_stateDbg ( struct PortalInternal *p, const ChannelType rc );
int MemServerRequest_memoryTraffic ( struct PortalInternal *p, const ChannelType rc );
enum { CHAN_NUM_MemServerRequest_addrTrans,CHAN_NUM_MemServerRequest_setTileState,CHAN_NUM_MemServerRequest_stateDbg,CHAN_NUM_MemServerRequest_memoryTraffic};
extern const uint32_t MemServerRequest_reqinfo;

typedef struct {
    uint32_t sglId;
    uint32_t offset;
} MemServerRequest_addrTransData;
typedef struct {
    TileControl tc;
} MemServerRequest_setTileStateData;
typedef struct {
    ChannelType rc;
} MemServerRequest_stateDbgData;
typedef struct {
    ChannelType rc;
} MemServerRequest_memoryTrafficData;
typedef union {
    MemServerRequest_addrTransData addrTrans;
    MemServerRequest_setTileStateData setTileState;
    MemServerRequest_stateDbgData stateDbg;
    MemServerRequest_memoryTrafficData memoryTraffic;
} MemServerRequestData;
int MemServerRequest_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd);
typedef struct {
    PORTAL_DISCONNECT disconnect;
    int (*addrTrans) (  struct PortalInternal *p, const uint32_t sglId, const uint32_t offset );
    int (*setTileState) (  struct PortalInternal *p, const TileControl tc );
    int (*stateDbg) (  struct PortalInternal *p, const ChannelType rc );
    int (*memoryTraffic) (  struct PortalInternal *p, const ChannelType rc );
} MemServerRequestCb;
extern MemServerRequestCb MemServerRequestProxyReq;

int MemServerRequestJson_addrTrans ( struct PortalInternal *p, const uint32_t sglId, const uint32_t offset );
int MemServerRequestJson_setTileState ( struct PortalInternal *p, const TileControl tc );
int MemServerRequestJson_stateDbg ( struct PortalInternal *p, const ChannelType rc );
int MemServerRequestJson_memoryTraffic ( struct PortalInternal *p, const ChannelType rc );
int MemServerRequestJson_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd);
extern MemServerRequestCb MemServerRequestJsonProxyReq;

int MMURequest_sglist ( struct PortalInternal *p, const uint32_t sglId, const uint32_t sglIndex, const uint64_t addr, const uint32_t len );
int MMURequest_region ( struct PortalInternal *p, const uint32_t sglId, const uint64_t barr12, const uint32_t index12, const uint64_t barr8, const uint32_t index8, const uint64_t barr4, const uint32_t index4, const uint64_t barr0, const uint32_t index0 );
int MMURequest_idRequest ( struct PortalInternal *p, const SpecialTypeForSendingFd fd );
int MMURequest_idReturn ( struct PortalInternal *p, const uint32_t sglId );
int MMURequest_setInterface ( struct PortalInternal *p, const uint32_t interfaceId, const uint32_t sglId );
enum { CHAN_NUM_MMURequest_sglist,CHAN_NUM_MMURequest_region,CHAN_NUM_MMURequest_idRequest,CHAN_NUM_MMURequest_idReturn,CHAN_NUM_MMURequest_setInterface};
extern const uint32_t MMURequest_reqinfo;

typedef struct {
    uint32_t sglId;
    uint32_t sglIndex;
    uint64_t addr;
    uint32_t len;
} MMURequest_sglistData;
typedef struct {
    uint32_t sglId;
    uint64_t barr12;
    uint32_t index12;
    uint64_t barr8;
    uint32_t index8;
    uint64_t barr4;
    uint32_t index4;
    uint64_t barr0;
    uint32_t index0;
} MMURequest_regionData;
typedef struct {
    SpecialTypeForSendingFd fd;
} MMURequest_idRequestData;
typedef struct {
    uint32_t sglId;
} MMURequest_idReturnData;
typedef struct {
    uint32_t interfaceId;
    uint32_t sglId;
} MMURequest_setInterfaceData;
typedef union {
    MMURequest_sglistData sglist;
    MMURequest_regionData region;
    MMURequest_idRequestData idRequest;
    MMURequest_idReturnData idReturn;
    MMURequest_setInterfaceData setInterface;
} MMURequestData;
int MMURequest_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd);
typedef struct {
    PORTAL_DISCONNECT disconnect;
    int (*sglist) (  struct PortalInternal *p, const uint32_t sglId, const uint32_t sglIndex, const uint64_t addr, const uint32_t len );
    int (*region) (  struct PortalInternal *p, const uint32_t sglId, const uint64_t barr12, const uint32_t index12, const uint64_t barr8, const uint32_t index8, const uint64_t barr4, const uint32_t index4, const uint64_t barr0, const uint32_t index0 );
    int (*idRequest) (  struct PortalInternal *p, const SpecialTypeForSendingFd fd );
    int (*idReturn) (  struct PortalInternal *p, const uint32_t sglId );
    int (*setInterface) (  struct PortalInternal *p, const uint32_t interfaceId, const uint32_t sglId );
} MMURequestCb;
extern MMURequestCb MMURequestProxyReq;

int MMURequestJson_sglist ( struct PortalInternal *p, const uint32_t sglId, const uint32_t sglIndex, const uint64_t addr, const uint32_t len );
int MMURequestJson_region ( struct PortalInternal *p, const uint32_t sglId, const uint64_t barr12, const uint32_t index12, const uint64_t barr8, const uint32_t index8, const uint64_t barr4, const uint32_t index4, const uint64_t barr0, const uint32_t index0 );
int MMURequestJson_idRequest ( struct PortalInternal *p, const SpecialTypeForSendingFd fd );
int MMURequestJson_idReturn ( struct PortalInternal *p, const uint32_t sglId );
int MMURequestJson_setInterface ( struct PortalInternal *p, const uint32_t interfaceId, const uint32_t sglId );
int MMURequestJson_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd);
extern MMURequestCb MMURequestJsonProxyReq;

int MemServerIndication_addrResponse ( struct PortalInternal *p, const uint64_t physAddr );
int MemServerIndication_reportStateDbg ( struct PortalInternal *p, const DmaDbgRec rec );
int MemServerIndication_reportMemoryTraffic ( struct PortalInternal *p, const uint64_t words );
int MemServerIndication_error ( struct PortalInternal *p, const uint32_t code, const uint32_t sglId, const uint64_t offset, const uint64_t extra );
enum { CHAN_NUM_MemServerIndication_addrResponse,CHAN_NUM_MemServerIndication_reportStateDbg,CHAN_NUM_MemServerIndication_reportMemoryTraffic,CHAN_NUM_MemServerIndication_error};
extern const uint32_t MemServerIndication_reqinfo;

typedef struct {
    uint64_t physAddr;
} MemServerIndication_addrResponseData;
typedef struct {
    DmaDbgRec rec;
} MemServerIndication_reportStateDbgData;
typedef struct {
    uint64_t words;
} MemServerIndication_reportMemoryTrafficData;
typedef struct {
    uint32_t code;
    uint32_t sglId;
    uint64_t offset;
    uint64_t extra;
} MemServerIndication_errorData;
typedef union {
    MemServerIndication_addrResponseData addrResponse;
    MemServerIndication_reportStateDbgData reportStateDbg;
    MemServerIndication_reportMemoryTrafficData reportMemoryTraffic;
    MemServerIndication_errorData error;
} MemServerIndicationData;
int MemServerIndication_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd);
typedef struct {
    PORTAL_DISCONNECT disconnect;
    int (*addrResponse) (  struct PortalInternal *p, const uint64_t physAddr );
    int (*reportStateDbg) (  struct PortalInternal *p, const DmaDbgRec rec );
    int (*reportMemoryTraffic) (  struct PortalInternal *p, const uint64_t words );
    int (*error) (  struct PortalInternal *p, const uint32_t code, const uint32_t sglId, const uint64_t offset, const uint64_t extra );
} MemServerIndicationCb;
extern MemServerIndicationCb MemServerIndicationProxyReq;

int MemServerIndicationJson_addrResponse ( struct PortalInternal *p, const uint64_t physAddr );
int MemServerIndicationJson_reportStateDbg ( struct PortalInternal *p, const DmaDbgRec rec );
int MemServerIndicationJson_reportMemoryTraffic ( struct PortalInternal *p, const uint64_t words );
int MemServerIndicationJson_error ( struct PortalInternal *p, const uint32_t code, const uint32_t sglId, const uint64_t offset, const uint64_t extra );
int MemServerIndicationJson_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd);
extern MemServerIndicationCb MemServerIndicationJsonProxyReq;

int MMUIndication_idResponse ( struct PortalInternal *p, const uint32_t sglId );
int MMUIndication_configResp ( struct PortalInternal *p, const uint32_t sglId );
int MMUIndication_error ( struct PortalInternal *p, const uint32_t code, const uint32_t sglId, const uint64_t offset, const uint64_t extra );
enum { CHAN_NUM_MMUIndication_idResponse,CHAN_NUM_MMUIndication_configResp,CHAN_NUM_MMUIndication_error};
extern const uint32_t MMUIndication_reqinfo;

typedef struct {
    uint32_t sglId;
} MMUIndication_idResponseData;
typedef struct {
    uint32_t sglId;
} MMUIndication_configRespData;
typedef struct {
    uint32_t code;
    uint32_t sglId;
    uint64_t offset;
    uint64_t extra;
} MMUIndication_errorData;
typedef union {
    MMUIndication_idResponseData idResponse;
    MMUIndication_configRespData configResp;
    MMUIndication_errorData error;
} MMUIndicationData;
int MMUIndication_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd);
typedef struct {
    PORTAL_DISCONNECT disconnect;
    int (*idResponse) (  struct PortalInternal *p, const uint32_t sglId );
    int (*configResp) (  struct PortalInternal *p, const uint32_t sglId );
    int (*error) (  struct PortalInternal *p, const uint32_t code, const uint32_t sglId, const uint64_t offset, const uint64_t extra );
} MMUIndicationCb;
extern MMUIndicationCb MMUIndicationProxyReq;

int MMUIndicationJson_idResponse ( struct PortalInternal *p, const uint32_t sglId );
int MMUIndicationJson_configResp ( struct PortalInternal *p, const uint32_t sglId );
int MMUIndicationJson_error ( struct PortalInternal *p, const uint32_t code, const uint32_t sglId, const uint64_t offset, const uint64_t extra );
int MMUIndicationJson_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd);
extern MMUIndicationCb MMUIndicationJsonProxyReq;

int FlashRequest_readPage ( struct PortalInternal *p, const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t page, const uint32_t tag, const uint32_t offset );
int FlashRequest_writePage ( struct PortalInternal *p, const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t page, const uint32_t tag, const uint32_t offset );
int FlashRequest_eraseBlock ( struct PortalInternal *p, const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t tag );
int FlashRequest_setDmaReadRef ( struct PortalInternal *p, const uint32_t sgId );
int FlashRequest_setDmaWriteRef ( struct PortalInternal *p, const uint32_t sgId );
int FlashRequest_startCompaction ( struct PortalInternal *p, const uint32_t cntHigh, const uint32_t cntLow, const uint32_t destPpaFlag );
int FlashRequest_setDmaKtPpaRef ( struct PortalInternal *p, const uint32_t sgIdHigh, const uint32_t sgIdLow, const uint32_t sgIdRes1, const uint32_t sgIdRes2 );
int FlashRequest_setDmaKtOutputRef ( struct PortalInternal *p, const uint32_t sgIdKtBuf, const uint32_t sgIdInvalPPA );
int FlashRequest_start ( struct PortalInternal *p, const uint32_t dummy );
int FlashRequest_debugDumpReq ( struct PortalInternal *p, const uint32_t dummy );
int FlashRequest_setDebugVals ( struct PortalInternal *p, const uint32_t flag, const uint32_t debugDelay );
int FlashRequest_setDmaKtSearchRef ( struct PortalInternal *p, const uint32_t sgId );
int FlashRequest_findKey ( struct PortalInternal *p, const uint32_t ppa, const uint32_t keySz, const uint32_t tag );
enum { CHAN_NUM_FlashRequest_readPage,CHAN_NUM_FlashRequest_writePage,CHAN_NUM_FlashRequest_eraseBlock,CHAN_NUM_FlashRequest_setDmaReadRef,CHAN_NUM_FlashRequest_setDmaWriteRef,CHAN_NUM_FlashRequest_startCompaction,CHAN_NUM_FlashRequest_setDmaKtPpaRef,CHAN_NUM_FlashRequest_setDmaKtOutputRef,CHAN_NUM_FlashRequest_start,CHAN_NUM_FlashRequest_debugDumpReq,CHAN_NUM_FlashRequest_setDebugVals,CHAN_NUM_FlashRequest_setDmaKtSearchRef,CHAN_NUM_FlashRequest_findKey};
extern const uint32_t FlashRequest_reqinfo;

typedef struct {
    uint32_t bus;
    uint32_t chip;
    uint32_t block;
    uint32_t page;
    uint32_t tag;
    uint32_t offset;
} FlashRequest_readPageData;
typedef struct {
    uint32_t bus;
    uint32_t chip;
    uint32_t block;
    uint32_t page;
    uint32_t tag;
    uint32_t offset;
} FlashRequest_writePageData;
typedef struct {
    uint32_t bus;
    uint32_t chip;
    uint32_t block;
    uint32_t tag;
} FlashRequest_eraseBlockData;
typedef struct {
    uint32_t sgId;
} FlashRequest_setDmaReadRefData;
typedef struct {
    uint32_t sgId;
} FlashRequest_setDmaWriteRefData;
typedef struct {
    uint32_t cntHigh;
    uint32_t cntLow;
    uint32_t destPpaFlag;
} FlashRequest_startCompactionData;
typedef struct {
    uint32_t sgIdHigh;
    uint32_t sgIdLow;
    uint32_t sgIdRes1;
    uint32_t sgIdRes2;
} FlashRequest_setDmaKtPpaRefData;
typedef struct {
    uint32_t sgIdKtBuf;
    uint32_t sgIdInvalPPA;
} FlashRequest_setDmaKtOutputRefData;
typedef struct {
    uint32_t dummy;
} FlashRequest_startData;
typedef struct {
    uint32_t dummy;
} FlashRequest_debugDumpReqData;
typedef struct {
    uint32_t flag;
    uint32_t debugDelay;
} FlashRequest_setDebugValsData;
typedef struct {
    uint32_t sgId;
} FlashRequest_setDmaKtSearchRefData;
typedef struct {
    uint32_t ppa;
    uint32_t keySz;
    uint32_t tag;
} FlashRequest_findKeyData;
typedef union {
    FlashRequest_readPageData readPage;
    FlashRequest_writePageData writePage;
    FlashRequest_eraseBlockData eraseBlock;
    FlashRequest_setDmaReadRefData setDmaReadRef;
    FlashRequest_setDmaWriteRefData setDmaWriteRef;
    FlashRequest_startCompactionData startCompaction;
    FlashRequest_setDmaKtPpaRefData setDmaKtPpaRef;
    FlashRequest_setDmaKtOutputRefData setDmaKtOutputRef;
    FlashRequest_startData start;
    FlashRequest_debugDumpReqData debugDumpReq;
    FlashRequest_setDebugValsData setDebugVals;
    FlashRequest_setDmaKtSearchRefData setDmaKtSearchRef;
    FlashRequest_findKeyData findKey;
} FlashRequestData;
int FlashRequest_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd);
typedef struct {
    PORTAL_DISCONNECT disconnect;
    int (*readPage) (  struct PortalInternal *p, const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t page, const uint32_t tag, const uint32_t offset );
    int (*writePage) (  struct PortalInternal *p, const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t page, const uint32_t tag, const uint32_t offset );
    int (*eraseBlock) (  struct PortalInternal *p, const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t tag );
    int (*setDmaReadRef) (  struct PortalInternal *p, const uint32_t sgId );
    int (*setDmaWriteRef) (  struct PortalInternal *p, const uint32_t sgId );
    int (*startCompaction) (  struct PortalInternal *p, const uint32_t cntHigh, const uint32_t cntLow, const uint32_t destPpaFlag );
    int (*setDmaKtPpaRef) (  struct PortalInternal *p, const uint32_t sgIdHigh, const uint32_t sgIdLow, const uint32_t sgIdRes1, const uint32_t sgIdRes2 );
    int (*setDmaKtOutputRef) (  struct PortalInternal *p, const uint32_t sgIdKtBuf, const uint32_t sgIdInvalPPA );
    int (*start) (  struct PortalInternal *p, const uint32_t dummy );
    int (*debugDumpReq) (  struct PortalInternal *p, const uint32_t dummy );
    int (*setDebugVals) (  struct PortalInternal *p, const uint32_t flag, const uint32_t debugDelay );
    int (*setDmaKtSearchRef) (  struct PortalInternal *p, const uint32_t sgId );
    int (*findKey) (  struct PortalInternal *p, const uint32_t ppa, const uint32_t keySz, const uint32_t tag );
} FlashRequestCb;
extern FlashRequestCb FlashRequestProxyReq;

int FlashRequestJson_readPage ( struct PortalInternal *p, const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t page, const uint32_t tag, const uint32_t offset );
int FlashRequestJson_writePage ( struct PortalInternal *p, const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t page, const uint32_t tag, const uint32_t offset );
int FlashRequestJson_eraseBlock ( struct PortalInternal *p, const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t tag );
int FlashRequestJson_setDmaReadRef ( struct PortalInternal *p, const uint32_t sgId );
int FlashRequestJson_setDmaWriteRef ( struct PortalInternal *p, const uint32_t sgId );
int FlashRequestJson_startCompaction ( struct PortalInternal *p, const uint32_t cntHigh, const uint32_t cntLow, const uint32_t destPpaFlag );
int FlashRequestJson_setDmaKtPpaRef ( struct PortalInternal *p, const uint32_t sgIdHigh, const uint32_t sgIdLow, const uint32_t sgIdRes1, const uint32_t sgIdRes2 );
int FlashRequestJson_setDmaKtOutputRef ( struct PortalInternal *p, const uint32_t sgIdKtBuf, const uint32_t sgIdInvalPPA );
int FlashRequestJson_start ( struct PortalInternal *p, const uint32_t dummy );
int FlashRequestJson_debugDumpReq ( struct PortalInternal *p, const uint32_t dummy );
int FlashRequestJson_setDebugVals ( struct PortalInternal *p, const uint32_t flag, const uint32_t debugDelay );
int FlashRequestJson_setDmaKtSearchRef ( struct PortalInternal *p, const uint32_t sgId );
int FlashRequestJson_findKey ( struct PortalInternal *p, const uint32_t ppa, const uint32_t keySz, const uint32_t tag );
int FlashRequestJson_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd);
extern FlashRequestCb FlashRequestJsonProxyReq;

int FlashIndication_readDone ( struct PortalInternal *p, const uint32_t tag );
int FlashIndication_writeDone ( struct PortalInternal *p, const uint32_t tag );
int FlashIndication_eraseDone ( struct PortalInternal *p, const uint32_t tag, const uint32_t status );
int FlashIndication_debugDumpResp ( struct PortalInternal *p, const uint32_t debug0, const uint32_t debug1, const uint32_t debug2, const uint32_t debug3, const uint32_t debug4, const uint32_t debug5 );
int FlashIndication_mergeDone ( struct PortalInternal *p, const uint32_t numGenKt, const uint32_t numInvalAddr, const uint64_t counter );
int FlashIndication_mergeFlushDone1 ( struct PortalInternal *p, const uint32_t num );
int FlashIndication_mergeFlushDone2 ( struct PortalInternal *p, const uint32_t num );
int FlashIndication_findKeyDone ( struct PortalInternal *p, const uint16_t tag, const uint16_t status, const uint32_t ppa );
enum { CHAN_NUM_FlashIndication_readDone,CHAN_NUM_FlashIndication_writeDone,CHAN_NUM_FlashIndication_eraseDone,CHAN_NUM_FlashIndication_debugDumpResp,CHAN_NUM_FlashIndication_mergeDone,CHAN_NUM_FlashIndication_mergeFlushDone1,CHAN_NUM_FlashIndication_mergeFlushDone2,CHAN_NUM_FlashIndication_findKeyDone};
extern const uint32_t FlashIndication_reqinfo;

typedef struct {
    uint32_t tag;
} FlashIndication_readDoneData;
typedef struct {
    uint32_t tag;
} FlashIndication_writeDoneData;
typedef struct {
    uint32_t tag;
    uint32_t status;
} FlashIndication_eraseDoneData;
typedef struct {
    uint32_t debug0;
    uint32_t debug1;
    uint32_t debug2;
    uint32_t debug3;
    uint32_t debug4;
    uint32_t debug5;
} FlashIndication_debugDumpRespData;
typedef struct {
    uint32_t numGenKt;
    uint32_t numInvalAddr;
    uint64_t counter;
} FlashIndication_mergeDoneData;
typedef struct {
    uint32_t num;
} FlashIndication_mergeFlushDone1Data;
typedef struct {
    uint32_t num;
} FlashIndication_mergeFlushDone2Data;
typedef struct {
    uint16_t tag;
    uint16_t status;
    uint32_t ppa;
} FlashIndication_findKeyDoneData;
typedef union {
    FlashIndication_readDoneData readDone;
    FlashIndication_writeDoneData writeDone;
    FlashIndication_eraseDoneData eraseDone;
    FlashIndication_debugDumpRespData debugDumpResp;
    FlashIndication_mergeDoneData mergeDone;
    FlashIndication_mergeFlushDone1Data mergeFlushDone1;
    FlashIndication_mergeFlushDone2Data mergeFlushDone2;
    FlashIndication_findKeyDoneData findKeyDone;
} FlashIndicationData;
int FlashIndication_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd);
typedef struct {
    PORTAL_DISCONNECT disconnect;
    int (*readDone) (  struct PortalInternal *p, const uint32_t tag );
    int (*writeDone) (  struct PortalInternal *p, const uint32_t tag );
    int (*eraseDone) (  struct PortalInternal *p, const uint32_t tag, const uint32_t status );
    int (*debugDumpResp) (  struct PortalInternal *p, const uint32_t debug0, const uint32_t debug1, const uint32_t debug2, const uint32_t debug3, const uint32_t debug4, const uint32_t debug5 );
    int (*mergeDone) (  struct PortalInternal *p, const uint32_t numGenKt, const uint32_t numInvalAddr, const uint64_t counter );
    int (*mergeFlushDone1) (  struct PortalInternal *p, const uint32_t num );
    int (*mergeFlushDone2) (  struct PortalInternal *p, const uint32_t num );
    int (*findKeyDone) (  struct PortalInternal *p, const uint16_t tag, const uint16_t status, const uint32_t ppa );
} FlashIndicationCb;
extern FlashIndicationCb FlashIndicationProxyReq;

int FlashIndicationJson_readDone ( struct PortalInternal *p, const uint32_t tag );
int FlashIndicationJson_writeDone ( struct PortalInternal *p, const uint32_t tag );
int FlashIndicationJson_eraseDone ( struct PortalInternal *p, const uint32_t tag, const uint32_t status );
int FlashIndicationJson_debugDumpResp ( struct PortalInternal *p, const uint32_t debug0, const uint32_t debug1, const uint32_t debug2, const uint32_t debug3, const uint32_t debug4, const uint32_t debug5 );
int FlashIndicationJson_mergeDone ( struct PortalInternal *p, const uint32_t numGenKt, const uint32_t numInvalAddr, const uint64_t counter );
int FlashIndicationJson_mergeFlushDone1 ( struct PortalInternal *p, const uint32_t num );
int FlashIndicationJson_mergeFlushDone2 ( struct PortalInternal *p, const uint32_t num );
int FlashIndicationJson_findKeyDone ( struct PortalInternal *p, const uint16_t tag, const uint16_t status, const uint32_t ppa );
int FlashIndicationJson_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd);
extern FlashIndicationCb FlashIndicationJsonProxyReq;
#ifdef __cplusplus
}
#endif
#endif //__GENERATED_TYPES__
