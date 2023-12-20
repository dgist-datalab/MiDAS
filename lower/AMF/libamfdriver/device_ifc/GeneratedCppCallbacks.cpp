#include "GeneratedTypes.h"

#ifndef NO_CPP_PORTAL_CODE
extern const uint32_t ifcNamesNone = IfcNamesNone;
extern const uint32_t platformIfcNames_MemServerRequestS2H = PlatformIfcNames_MemServerRequestS2H;
extern const uint32_t platformIfcNames_MMURequestS2H = PlatformIfcNames_MMURequestS2H;
extern const uint32_t platformIfcNames_MemServerIndicationH2S = PlatformIfcNames_MemServerIndicationH2S;
extern const uint32_t platformIfcNames_MMUIndicationH2S = PlatformIfcNames_MMUIndicationH2S;
extern const uint32_t ifcNames_AmfIndicationH2S = IfcNames_AmfIndicationH2S;
extern const uint32_t ifcNames_AmfRequestS2H = IfcNames_AmfRequestS2H;

/************** Start of MemServerRequestWrapper CPP ***********/
#include "MemServerRequest.h"
int MemServerRequestdisconnect_cb (struct PortalInternal *p) {
    (static_cast<MemServerRequestWrapper *>(p->parent))->disconnect();
    return 0;
};
int MemServerRequestaddrTrans_cb (  struct PortalInternal *p, const uint32_t sglId, const uint32_t offset ) {
    (static_cast<MemServerRequestWrapper *>(p->parent))->addrTrans ( sglId, offset);
    return 0;
};
int MemServerRequestsetTileState_cb (  struct PortalInternal *p, const TileControl tc ) {
    (static_cast<MemServerRequestWrapper *>(p->parent))->setTileState ( tc);
    return 0;
};
int MemServerRequeststateDbg_cb (  struct PortalInternal *p, const ChannelType rc ) {
    (static_cast<MemServerRequestWrapper *>(p->parent))->stateDbg ( rc);
    return 0;
};
int MemServerRequestmemoryTraffic_cb (  struct PortalInternal *p, const ChannelType rc ) {
    (static_cast<MemServerRequestWrapper *>(p->parent))->memoryTraffic ( rc);
    return 0;
};
MemServerRequestCb MemServerRequest_cbTable = {
    MemServerRequestdisconnect_cb,
    MemServerRequestaddrTrans_cb,
    MemServerRequestsetTileState_cb,
    MemServerRequeststateDbg_cb,
    MemServerRequestmemoryTraffic_cb,
};

/************** Start of MMURequestWrapper CPP ***********/
#include "MMURequest.h"
int MMURequestdisconnect_cb (struct PortalInternal *p) {
    (static_cast<MMURequestWrapper *>(p->parent))->disconnect();
    return 0;
};
int MMURequestsglist_cb (  struct PortalInternal *p, const uint32_t sglId, const uint32_t sglIndex, const uint64_t addr, const uint32_t len ) {
    (static_cast<MMURequestWrapper *>(p->parent))->sglist ( sglId, sglIndex, addr, len);
    return 0;
};
int MMURequestregion_cb (  struct PortalInternal *p, const uint32_t sglId, const uint64_t barr12, const uint32_t index12, const uint64_t barr8, const uint32_t index8, const uint64_t barr4, const uint32_t index4, const uint64_t barr0, const uint32_t index0 ) {
    (static_cast<MMURequestWrapper *>(p->parent))->region ( sglId, barr12, index12, barr8, index8, barr4, index4, barr0, index0);
    return 0;
};
int MMURequestidRequest_cb (  struct PortalInternal *p, const SpecialTypeForSendingFd fd ) {
    (static_cast<MMURequestWrapper *>(p->parent))->idRequest ( fd);
    return 0;
};
int MMURequestidReturn_cb (  struct PortalInternal *p, const uint32_t sglId ) {
    (static_cast<MMURequestWrapper *>(p->parent))->idReturn ( sglId);
    return 0;
};
int MMURequestsetInterface_cb (  struct PortalInternal *p, const uint32_t interfaceId, const uint32_t sglId ) {
    (static_cast<MMURequestWrapper *>(p->parent))->setInterface ( interfaceId, sglId);
    return 0;
};
MMURequestCb MMURequest_cbTable = {
    MMURequestdisconnect_cb,
    MMURequestsglist_cb,
    MMURequestregion_cb,
    MMURequestidRequest_cb,
    MMURequestidReturn_cb,
    MMURequestsetInterface_cb,
};

/************** Start of MemServerIndicationWrapper CPP ***********/
#include "MemServerIndication.h"
int MemServerIndicationdisconnect_cb (struct PortalInternal *p) {
    (static_cast<MemServerIndicationWrapper *>(p->parent))->disconnect();
    return 0;
};
int MemServerIndicationaddrResponse_cb (  struct PortalInternal *p, const uint64_t physAddr ) {
    (static_cast<MemServerIndicationWrapper *>(p->parent))->addrResponse ( physAddr);
    return 0;
};
int MemServerIndicationreportStateDbg_cb (  struct PortalInternal *p, const DmaDbgRec rec ) {
    (static_cast<MemServerIndicationWrapper *>(p->parent))->reportStateDbg ( rec);
    return 0;
};
int MemServerIndicationreportMemoryTraffic_cb (  struct PortalInternal *p, const uint64_t words ) {
    (static_cast<MemServerIndicationWrapper *>(p->parent))->reportMemoryTraffic ( words);
    return 0;
};
int MemServerIndicationerror_cb (  struct PortalInternal *p, const uint32_t code, const uint32_t sglId, const uint64_t offset, const uint64_t extra ) {
    (static_cast<MemServerIndicationWrapper *>(p->parent))->error ( code, sglId, offset, extra);
    return 0;
};
MemServerIndicationCb MemServerIndication_cbTable = {
    MemServerIndicationdisconnect_cb,
    MemServerIndicationaddrResponse_cb,
    MemServerIndicationreportStateDbg_cb,
    MemServerIndicationreportMemoryTraffic_cb,
    MemServerIndicationerror_cb,
};

/************** Start of MMUIndicationWrapper CPP ***********/
#include "MMUIndication.h"
int MMUIndicationdisconnect_cb (struct PortalInternal *p) {
    (static_cast<MMUIndicationWrapper *>(p->parent))->disconnect();
    return 0;
};
int MMUIndicationidResponse_cb (  struct PortalInternal *p, const uint32_t sglId ) {
    (static_cast<MMUIndicationWrapper *>(p->parent))->idResponse ( sglId);
    return 0;
};
int MMUIndicationconfigResp_cb (  struct PortalInternal *p, const uint32_t sglId ) {
    (static_cast<MMUIndicationWrapper *>(p->parent))->configResp ( sglId);
    return 0;
};
int MMUIndicationerror_cb (  struct PortalInternal *p, const uint32_t code, const uint32_t sglId, const uint64_t offset, const uint64_t extra ) {
    (static_cast<MMUIndicationWrapper *>(p->parent))->error ( code, sglId, offset, extra);
    return 0;
};
MMUIndicationCb MMUIndication_cbTable = {
    MMUIndicationdisconnect_cb,
    MMUIndicationidResponse_cb,
    MMUIndicationconfigResp_cb,
    MMUIndicationerror_cb,
};

/************** Start of AmfRequestWrapper CPP ***********/
#include "AmfRequest.h"
int AmfRequestdisconnect_cb (struct PortalInternal *p) {
    (static_cast<AmfRequestWrapper *>(p->parent))->disconnect();
    return 0;
};
int AmfRequestmakeReq_cb (  struct PortalInternal *p, const AmfRequestT req ) {
    (static_cast<AmfRequestWrapper *>(p->parent))->makeReq ( req);
    return 0;
};
int AmfRequestdebugDumpReq_cb (  struct PortalInternal *p, const uint8_t card ) {
    (static_cast<AmfRequestWrapper *>(p->parent))->debugDumpReq ( card);
    return 0;
};
int AmfRequestsetDmaReadRef_cb (  struct PortalInternal *p, const uint32_t sgId ) {
    (static_cast<AmfRequestWrapper *>(p->parent))->setDmaReadRef ( sgId);
    return 0;
};
int AmfRequestsetDmaWriteRef_cb (  struct PortalInternal *p, const uint32_t sgId ) {
    (static_cast<AmfRequestWrapper *>(p->parent))->setDmaWriteRef ( sgId);
    return 0;
};
int AmfRequestaskAftlLoaded_cb (  struct PortalInternal *p ) {
    (static_cast<AmfRequestWrapper *>(p->parent))->askAftlLoaded ( );
    return 0;
};
int AmfRequestsetAftlLoaded_cb (  struct PortalInternal *p ) {
    (static_cast<AmfRequestWrapper *>(p->parent))->setAftlLoaded ( );
    return 0;
};
int AmfRequestupdateMapping_cb (  struct PortalInternal *p, const uint32_t seg_virtblk, const uint8_t allocated, const uint16_t mapped_block ) {
    (static_cast<AmfRequestWrapper *>(p->parent))->updateMapping ( seg_virtblk, allocated, mapped_block);
    return 0;
};
int AmfRequestreadMapping_cb (  struct PortalInternal *p, const uint32_t seg_virtblk ) {
    (static_cast<AmfRequestWrapper *>(p->parent))->readMapping ( seg_virtblk);
    return 0;
};
int AmfRequestupdateBlkInfo_cb (  struct PortalInternal *p, const uint16_t phyaddr_upper, const bsvvector_Luint16_t_L8 blkinfo_vec ) {
    (static_cast<AmfRequestWrapper *>(p->parent))->updateBlkInfo ( phyaddr_upper, blkinfo_vec);
    return 0;
};
int AmfRequestreadBlkInfo_cb (  struct PortalInternal *p, const uint16_t phyaddr_upper ) {
    (static_cast<AmfRequestWrapper *>(p->parent))->readBlkInfo ( phyaddr_upper);
    return 0;
};
int AmfRequesteraseRawBlock_cb (  struct PortalInternal *p, const uint8_t card, const uint8_t bus, const uint8_t chip, const uint16_t block, const uint8_t tag ) {
    (static_cast<AmfRequestWrapper *>(p->parent))->eraseRawBlock ( card, bus, chip, block, tag);
    return 0;
};
AmfRequestCb AmfRequest_cbTable = {
    AmfRequestdisconnect_cb,
    AmfRequestmakeReq_cb,
    AmfRequestdebugDumpReq_cb,
    AmfRequestsetDmaReadRef_cb,
    AmfRequestsetDmaWriteRef_cb,
    AmfRequestaskAftlLoaded_cb,
    AmfRequestsetAftlLoaded_cb,
    AmfRequestupdateMapping_cb,
    AmfRequestreadMapping_cb,
    AmfRequestupdateBlkInfo_cb,
    AmfRequestreadBlkInfo_cb,
    AmfRequesteraseRawBlock_cb,
};

/************** Start of AmfIndicationWrapper CPP ***********/
#include "AmfIndication.h"
int AmfIndicationdisconnect_cb (struct PortalInternal *p) {
    (static_cast<AmfIndicationWrapper *>(p->parent))->disconnect();
    return 0;
};
int AmfIndicationreadDone_cb (  struct PortalInternal *p, const uint8_t tag ) {
    (static_cast<AmfIndicationWrapper *>(p->parent))->readDone ( tag);
    return 0;
};
int AmfIndicationwriteDone_cb (  struct PortalInternal *p, const uint8_t tag ) {
    (static_cast<AmfIndicationWrapper *>(p->parent))->writeDone ( tag);
    return 0;
};
int AmfIndicationeraseDone_cb (  struct PortalInternal *p, const uint8_t tag, const uint8_t status ) {
    (static_cast<AmfIndicationWrapper *>(p->parent))->eraseDone ( tag, status);
    return 0;
};
int AmfIndicationdebugDumpResp_cb (  struct PortalInternal *p, const uint32_t debug0, const uint32_t debug1, const uint32_t debug2, const uint32_t debug3, const uint32_t debug4, const uint32_t debug5 ) {
    (static_cast<AmfIndicationWrapper *>(p->parent))->debugDumpResp ( debug0, debug1, debug2, debug3, debug4, debug5);
    return 0;
};
int AmfIndicationrespAftlFailed_cb (  struct PortalInternal *p, const AmfRequestT resp ) {
    (static_cast<AmfIndicationWrapper *>(p->parent))->respAftlFailed ( resp);
    return 0;
};
int AmfIndicationrespReadMapping_cb (  struct PortalInternal *p, const uint8_t allocated, const uint16_t block_num ) {
    (static_cast<AmfIndicationWrapper *>(p->parent))->respReadMapping ( allocated, block_num);
    return 0;
};
int AmfIndicationrespReadBlkInfo_cb (  struct PortalInternal *p, const bsvvector_Luint16_t_L8 blkinfo_vec ) {
    (static_cast<AmfIndicationWrapper *>(p->parent))->respReadBlkInfo ( blkinfo_vec);
    return 0;
};
int AmfIndicationrespAftlLoaded_cb (  struct PortalInternal *p, const uint8_t resp ) {
    (static_cast<AmfIndicationWrapper *>(p->parent))->respAftlLoaded ( resp);
    return 0;
};
AmfIndicationCb AmfIndication_cbTable = {
    AmfIndicationdisconnect_cb,
    AmfIndicationreadDone_cb,
    AmfIndicationwriteDone_cb,
    AmfIndicationeraseDone_cb,
    AmfIndicationdebugDumpResp_cb,
    AmfIndicationrespAftlFailed_cb,
    AmfIndicationrespReadMapping_cb,
    AmfIndicationrespReadBlkInfo_cb,
    AmfIndicationrespAftlLoaded_cb,
};
#endif //NO_CPP_PORTAL_CODE
