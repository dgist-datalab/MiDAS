#ifndef __GENERATED_TYPES__
#define __GENERATED_TYPES__
#include "portal.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum AmfCmdTypes { AmfREAD=0, AmfWRITE, AmfERASE, AmfMARKBAD, AmfINVALID,  } AmfCmdTypes;
typedef struct AmfRequestT {
    AmfCmdTypes cmd;
    uint8_t tag : 7;
    uint32_t lpa : 27;
} AmfRequestT;
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
typedef enum IfcNames { IfcNamesNone=0, PlatformIfcNames_MemServerRequestS2H=1, PlatformIfcNames_MMURequestS2H=2, PlatformIfcNames_MemServerIndicationH2S=3, PlatformIfcNames_MMUIndicationH2S=4, IfcNames_AmfIndicationH2S=5, IfcNames_AmfRequestS2H=6,  } IfcNames;


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

typedef uint16_t bsvvector_Luint16_t_L8[8];
typedef uint16_t bsvvector_Luint16_t_L8[8];
int AmfRequest_makeReq ( struct PortalInternal *p, const AmfRequestT req );
int AmfRequest_debugDumpReq ( struct PortalInternal *p, const uint8_t card );
int AmfRequest_setDmaReadRef ( struct PortalInternal *p, const uint32_t sgId );
int AmfRequest_setDmaWriteRef ( struct PortalInternal *p, const uint32_t sgId );
int AmfRequest_askAftlLoaded ( struct PortalInternal *p );
int AmfRequest_setAftlLoaded ( struct PortalInternal *p );
int AmfRequest_updateMapping ( struct PortalInternal *p, const uint32_t seg_virtblk, const uint8_t allocated, const uint16_t mapped_block );
int AmfRequest_readMapping ( struct PortalInternal *p, const uint32_t seg_virtblk );
int AmfRequest_updateBlkInfo ( struct PortalInternal *p, const uint16_t phyaddr_upper, const bsvvector_Luint16_t_L8 blkinfo_vec );
int AmfRequest_readBlkInfo ( struct PortalInternal *p, const uint16_t phyaddr_upper );
int AmfRequest_eraseRawBlock ( struct PortalInternal *p, const uint8_t card, const uint8_t bus, const uint8_t chip, const uint16_t block, const uint8_t tag );
enum { CHAN_NUM_AmfRequest_makeReq,CHAN_NUM_AmfRequest_debugDumpReq,CHAN_NUM_AmfRequest_setDmaReadRef,CHAN_NUM_AmfRequest_setDmaWriteRef,CHAN_NUM_AmfRequest_askAftlLoaded,CHAN_NUM_AmfRequest_setAftlLoaded,CHAN_NUM_AmfRequest_updateMapping,CHAN_NUM_AmfRequest_readMapping,CHAN_NUM_AmfRequest_updateBlkInfo,CHAN_NUM_AmfRequest_readBlkInfo,CHAN_NUM_AmfRequest_eraseRawBlock};
extern const uint32_t AmfRequest_reqinfo;

typedef struct {
    AmfRequestT req;
} AmfRequest_makeReqData;
typedef struct {
    uint8_t card;
} AmfRequest_debugDumpReqData;
typedef struct {
    uint32_t sgId;
} AmfRequest_setDmaReadRefData;
typedef struct {
    uint32_t sgId;
} AmfRequest_setDmaWriteRefData;
typedef struct {
        int padding;

} AmfRequest_askAftlLoadedData;
typedef struct {
        int padding;

} AmfRequest_setAftlLoadedData;
typedef struct {
    uint32_t seg_virtblk;
    uint8_t allocated;
    uint16_t mapped_block;
} AmfRequest_updateMappingData;
typedef struct {
    uint32_t seg_virtblk;
} AmfRequest_readMappingData;
typedef struct {
    uint16_t phyaddr_upper;
    bsvvector_Luint16_t_L8 blkinfo_vec;
} AmfRequest_updateBlkInfoData;
typedef struct {
    uint16_t phyaddr_upper;
} AmfRequest_readBlkInfoData;
typedef struct {
    uint8_t card;
    uint8_t bus;
    uint8_t chip;
    uint16_t block;
    uint8_t tag;
} AmfRequest_eraseRawBlockData;
typedef union {
    AmfRequest_makeReqData makeReq;
    AmfRequest_debugDumpReqData debugDumpReq;
    AmfRequest_setDmaReadRefData setDmaReadRef;
    AmfRequest_setDmaWriteRefData setDmaWriteRef;
    AmfRequest_askAftlLoadedData askAftlLoaded;
    AmfRequest_setAftlLoadedData setAftlLoaded;
    AmfRequest_updateMappingData updateMapping;
    AmfRequest_readMappingData readMapping;
    AmfRequest_updateBlkInfoData updateBlkInfo;
    AmfRequest_readBlkInfoData readBlkInfo;
    AmfRequest_eraseRawBlockData eraseRawBlock;
} AmfRequestData;
int AmfRequest_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd);
typedef struct {
    PORTAL_DISCONNECT disconnect;
    int (*makeReq) (  struct PortalInternal *p, const AmfRequestT req );
    int (*debugDumpReq) (  struct PortalInternal *p, const uint8_t card );
    int (*setDmaReadRef) (  struct PortalInternal *p, const uint32_t sgId );
    int (*setDmaWriteRef) (  struct PortalInternal *p, const uint32_t sgId );
    int (*askAftlLoaded) (  struct PortalInternal *p );
    int (*setAftlLoaded) (  struct PortalInternal *p );
    int (*updateMapping) (  struct PortalInternal *p, const uint32_t seg_virtblk, const uint8_t allocated, const uint16_t mapped_block );
    int (*readMapping) (  struct PortalInternal *p, const uint32_t seg_virtblk );
    int (*updateBlkInfo) (  struct PortalInternal *p, const uint16_t phyaddr_upper, const bsvvector_Luint16_t_L8 blkinfo_vec );
    int (*readBlkInfo) (  struct PortalInternal *p, const uint16_t phyaddr_upper );
    int (*eraseRawBlock) (  struct PortalInternal *p, const uint8_t card, const uint8_t bus, const uint8_t chip, const uint16_t block, const uint8_t tag );
} AmfRequestCb;
extern AmfRequestCb AmfRequestProxyReq;

int AmfRequestJson_makeReq ( struct PortalInternal *p, const AmfRequestT req );
int AmfRequestJson_debugDumpReq ( struct PortalInternal *p, const uint8_t card );
int AmfRequestJson_setDmaReadRef ( struct PortalInternal *p, const uint32_t sgId );
int AmfRequestJson_setDmaWriteRef ( struct PortalInternal *p, const uint32_t sgId );
int AmfRequestJson_askAftlLoaded ( struct PortalInternal *p );
int AmfRequestJson_setAftlLoaded ( struct PortalInternal *p );
int AmfRequestJson_updateMapping ( struct PortalInternal *p, const uint32_t seg_virtblk, const uint8_t allocated, const uint16_t mapped_block );
int AmfRequestJson_readMapping ( struct PortalInternal *p, const uint32_t seg_virtblk );
int AmfRequestJson_updateBlkInfo ( struct PortalInternal *p, const uint16_t phyaddr_upper, const bsvvector_Luint16_t_L8 blkinfo_vec );
int AmfRequestJson_readBlkInfo ( struct PortalInternal *p, const uint16_t phyaddr_upper );
int AmfRequestJson_eraseRawBlock ( struct PortalInternal *p, const uint8_t card, const uint8_t bus, const uint8_t chip, const uint16_t block, const uint8_t tag );
int AmfRequestJson_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd);
extern AmfRequestCb AmfRequestJsonProxyReq;

typedef uint16_t bsvvector_Luint16_t_L8[8];
typedef uint16_t bsvvector_Luint16_t_L8[8];
int AmfIndication_readDone ( struct PortalInternal *p, const uint8_t tag );
int AmfIndication_writeDone ( struct PortalInternal *p, const uint8_t tag );
int AmfIndication_eraseDone ( struct PortalInternal *p, const uint8_t tag, const uint8_t status );
int AmfIndication_debugDumpResp ( struct PortalInternal *p, const uint32_t debug0, const uint32_t debug1, const uint32_t debug2, const uint32_t debug3, const uint32_t debug4, const uint32_t debug5 );
int AmfIndication_respAftlFailed ( struct PortalInternal *p, const AmfRequestT resp );
int AmfIndication_respReadMapping ( struct PortalInternal *p, const uint8_t allocated, const uint16_t block_num );
int AmfIndication_respReadBlkInfo ( struct PortalInternal *p, const bsvvector_Luint16_t_L8 blkinfo_vec );
int AmfIndication_respAftlLoaded ( struct PortalInternal *p, const uint8_t resp );
enum { CHAN_NUM_AmfIndication_readDone,CHAN_NUM_AmfIndication_writeDone,CHAN_NUM_AmfIndication_eraseDone,CHAN_NUM_AmfIndication_debugDumpResp,CHAN_NUM_AmfIndication_respAftlFailed,CHAN_NUM_AmfIndication_respReadMapping,CHAN_NUM_AmfIndication_respReadBlkInfo,CHAN_NUM_AmfIndication_respAftlLoaded};
extern const uint32_t AmfIndication_reqinfo;

typedef struct {
    uint8_t tag;
} AmfIndication_readDoneData;
typedef struct {
    uint8_t tag;
} AmfIndication_writeDoneData;
typedef struct {
    uint8_t tag;
    uint8_t status;
} AmfIndication_eraseDoneData;
typedef struct {
    uint32_t debug0;
    uint32_t debug1;
    uint32_t debug2;
    uint32_t debug3;
    uint32_t debug4;
    uint32_t debug5;
} AmfIndication_debugDumpRespData;
typedef struct {
    AmfRequestT resp;
} AmfIndication_respAftlFailedData;
typedef struct {
    uint8_t allocated;
    uint16_t block_num;
} AmfIndication_respReadMappingData;
typedef struct {
    bsvvector_Luint16_t_L8 blkinfo_vec;
} AmfIndication_respReadBlkInfoData;
typedef struct {
    uint8_t resp;
} AmfIndication_respAftlLoadedData;
typedef union {
    AmfIndication_readDoneData readDone;
    AmfIndication_writeDoneData writeDone;
    AmfIndication_eraseDoneData eraseDone;
    AmfIndication_debugDumpRespData debugDumpResp;
    AmfIndication_respAftlFailedData respAftlFailed;
    AmfIndication_respReadMappingData respReadMapping;
    AmfIndication_respReadBlkInfoData respReadBlkInfo;
    AmfIndication_respAftlLoadedData respAftlLoaded;
} AmfIndicationData;
int AmfIndication_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd);
typedef struct {
    PORTAL_DISCONNECT disconnect;
    int (*readDone) (  struct PortalInternal *p, const uint8_t tag );
    int (*writeDone) (  struct PortalInternal *p, const uint8_t tag );
    int (*eraseDone) (  struct PortalInternal *p, const uint8_t tag, const uint8_t status );
    int (*debugDumpResp) (  struct PortalInternal *p, const uint32_t debug0, const uint32_t debug1, const uint32_t debug2, const uint32_t debug3, const uint32_t debug4, const uint32_t debug5 );
    int (*respAftlFailed) (  struct PortalInternal *p, const AmfRequestT resp );
    int (*respReadMapping) (  struct PortalInternal *p, const uint8_t allocated, const uint16_t block_num );
    int (*respReadBlkInfo) (  struct PortalInternal *p, const bsvvector_Luint16_t_L8 blkinfo_vec );
    int (*respAftlLoaded) (  struct PortalInternal *p, const uint8_t resp );
} AmfIndicationCb;
extern AmfIndicationCb AmfIndicationProxyReq;

int AmfIndicationJson_readDone ( struct PortalInternal *p, const uint8_t tag );
int AmfIndicationJson_writeDone ( struct PortalInternal *p, const uint8_t tag );
int AmfIndicationJson_eraseDone ( struct PortalInternal *p, const uint8_t tag, const uint8_t status );
int AmfIndicationJson_debugDumpResp ( struct PortalInternal *p, const uint32_t debug0, const uint32_t debug1, const uint32_t debug2, const uint32_t debug3, const uint32_t debug4, const uint32_t debug5 );
int AmfIndicationJson_respAftlFailed ( struct PortalInternal *p, const AmfRequestT resp );
int AmfIndicationJson_respReadMapping ( struct PortalInternal *p, const uint8_t allocated, const uint16_t block_num );
int AmfIndicationJson_respReadBlkInfo ( struct PortalInternal *p, const bsvvector_Luint16_t_L8 blkinfo_vec );
int AmfIndicationJson_respAftlLoaded ( struct PortalInternal *p, const uint8_t resp );
int AmfIndicationJson_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd);
extern AmfIndicationCb AmfIndicationJsonProxyReq;
#ifdef __cplusplus
}
#endif
#endif //__GENERATED_TYPES__
