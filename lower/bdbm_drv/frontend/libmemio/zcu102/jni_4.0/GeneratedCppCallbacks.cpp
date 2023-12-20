#include "GeneratedTypes.h"

#ifndef NO_CPP_PORTAL_CODE
extern const uint32_t ifcNamesNone = IfcNamesNone;
extern const uint32_t platformIfcNames_MemServerRequestS2H = PlatformIfcNames_MemServerRequestS2H;
extern const uint32_t platformIfcNames_MMURequestS2H = PlatformIfcNames_MMURequestS2H;
extern const uint32_t platformIfcNames_MemServerIndicationH2S = PlatformIfcNames_MemServerIndicationH2S;
extern const uint32_t platformIfcNames_MMUIndicationH2S = PlatformIfcNames_MMUIndicationH2S;
extern const uint32_t ifcNames_FlashIndicationH2S = IfcNames_FlashIndicationH2S;
extern const uint32_t ifcNames_FlashRequestS2H = IfcNames_FlashRequestS2H;

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

/************** Start of FlashRequestWrapper CPP ***********/
#include "FlashRequest.h"
int FlashRequestdisconnect_cb (struct PortalInternal *p) {
    (static_cast<FlashRequestWrapper *>(p->parent))->disconnect();
    return 0;
};
int FlashRequestreadPage_cb (  struct PortalInternal *p, const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t page, const uint32_t tag, const uint32_t offset ) {
    (static_cast<FlashRequestWrapper *>(p->parent))->readPage ( bus, chip, block, page, tag, offset);
    return 0;
};
int FlashRequestwritePage_cb (  struct PortalInternal *p, const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t page, const uint32_t tag, const uint32_t offset ) {
    (static_cast<FlashRequestWrapper *>(p->parent))->writePage ( bus, chip, block, page, tag, offset);
    return 0;
};
int FlashRequesteraseBlock_cb (  struct PortalInternal *p, const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t tag ) {
    (static_cast<FlashRequestWrapper *>(p->parent))->eraseBlock ( bus, chip, block, tag);
    return 0;
};
int FlashRequestsetDmaReadRef_cb (  struct PortalInternal *p, const uint32_t sgId ) {
    (static_cast<FlashRequestWrapper *>(p->parent))->setDmaReadRef ( sgId);
    return 0;
};
int FlashRequestsetDmaWriteRef_cb (  struct PortalInternal *p, const uint32_t sgId ) {
    (static_cast<FlashRequestWrapper *>(p->parent))->setDmaWriteRef ( sgId);
    return 0;
};
int FlashRequeststartCompaction_cb (  struct PortalInternal *p, const uint32_t cntHigh, const uint32_t cntLow, const uint32_t destPpaFlag ) {
    (static_cast<FlashRequestWrapper *>(p->parent))->startCompaction ( cntHigh, cntLow, destPpaFlag);
    return 0;
};
int FlashRequestsetDmaKtPpaRef_cb (  struct PortalInternal *p, const uint32_t sgIdHigh, const uint32_t sgIdLow, const uint32_t sgIdRes1, const uint32_t sgIdRes2 ) {
    (static_cast<FlashRequestWrapper *>(p->parent))->setDmaKtPpaRef ( sgIdHigh, sgIdLow, sgIdRes1, sgIdRes2);
    return 0;
};
int FlashRequestsetDmaKtOutputRef_cb (  struct PortalInternal *p, const uint32_t sgIdKtBuf, const uint32_t sgIdInvalPPA ) {
    (static_cast<FlashRequestWrapper *>(p->parent))->setDmaKtOutputRef ( sgIdKtBuf, sgIdInvalPPA);
    return 0;
};
int FlashRequeststart_cb (  struct PortalInternal *p, const uint32_t dummy ) {
    (static_cast<FlashRequestWrapper *>(p->parent))->start ( dummy);
    return 0;
};
int FlashRequestdebugDumpReq_cb (  struct PortalInternal *p, const uint32_t dummy ) {
    (static_cast<FlashRequestWrapper *>(p->parent))->debugDumpReq ( dummy);
    return 0;
};
int FlashRequestsetDebugVals_cb (  struct PortalInternal *p, const uint32_t flag, const uint32_t debugDelay ) {
    (static_cast<FlashRequestWrapper *>(p->parent))->setDebugVals ( flag, debugDelay);
    return 0;
};
int FlashRequestsetDmaKtSearchRef_cb (  struct PortalInternal *p, const uint32_t sgId ) {
    (static_cast<FlashRequestWrapper *>(p->parent))->setDmaKtSearchRef ( sgId);
    return 0;
};
int FlashRequestfindKey_cb (  struct PortalInternal *p, const uint32_t ppa, const uint32_t keySz, const uint32_t tag ) {
    (static_cast<FlashRequestWrapper *>(p->parent))->findKey ( ppa, keySz, tag);
    return 0;
};
FlashRequestCb FlashRequest_cbTable = {
    FlashRequestdisconnect_cb,
    FlashRequestreadPage_cb,
    FlashRequestwritePage_cb,
    FlashRequesteraseBlock_cb,
    FlashRequestsetDmaReadRef_cb,
    FlashRequestsetDmaWriteRef_cb,
    FlashRequeststartCompaction_cb,
    FlashRequestsetDmaKtPpaRef_cb,
    FlashRequestsetDmaKtOutputRef_cb,
    FlashRequeststart_cb,
    FlashRequestdebugDumpReq_cb,
    FlashRequestsetDebugVals_cb,
    FlashRequestsetDmaKtSearchRef_cb,
    FlashRequestfindKey_cb,
};

/************** Start of FlashIndicationWrapper CPP ***********/
#include "FlashIndication.h"
int FlashIndicationdisconnect_cb (struct PortalInternal *p) {
    (static_cast<FlashIndicationWrapper *>(p->parent))->disconnect();
    return 0;
};
int FlashIndicationreadDone_cb (  struct PortalInternal *p, const uint32_t tag ) {
    (static_cast<FlashIndicationWrapper *>(p->parent))->readDone ( tag);
    return 0;
};
int FlashIndicationwriteDone_cb (  struct PortalInternal *p, const uint32_t tag ) {
    (static_cast<FlashIndicationWrapper *>(p->parent))->writeDone ( tag);
    return 0;
};
int FlashIndicationeraseDone_cb (  struct PortalInternal *p, const uint32_t tag, const uint32_t status ) {
    (static_cast<FlashIndicationWrapper *>(p->parent))->eraseDone ( tag, status);
    return 0;
};
int FlashIndicationdebugDumpResp_cb (  struct PortalInternal *p, const uint32_t debug0, const uint32_t debug1, const uint32_t debug2, const uint32_t debug3, const uint32_t debug4, const uint32_t debug5 ) {
    (static_cast<FlashIndicationWrapper *>(p->parent))->debugDumpResp ( debug0, debug1, debug2, debug3, debug4, debug5);
    return 0;
};
int FlashIndicationmergeDone_cb (  struct PortalInternal *p, const uint32_t numGenKt, const uint32_t numInvalAddr, const uint64_t counter ) {
    (static_cast<FlashIndicationWrapper *>(p->parent))->mergeDone ( numGenKt, numInvalAddr, counter);
    return 0;
};
int FlashIndicationmergeFlushDone1_cb (  struct PortalInternal *p, const uint32_t num ) {
    (static_cast<FlashIndicationWrapper *>(p->parent))->mergeFlushDone1 ( num);
    return 0;
};
int FlashIndicationmergeFlushDone2_cb (  struct PortalInternal *p, const uint32_t num ) {
    (static_cast<FlashIndicationWrapper *>(p->parent))->mergeFlushDone2 ( num);
    return 0;
};
int FlashIndicationfindKeyDone_cb (  struct PortalInternal *p, const uint16_t tag, const uint16_t status, const uint32_t ppa ) {
    (static_cast<FlashIndicationWrapper *>(p->parent))->findKeyDone ( tag, status, ppa);
    return 0;
};
FlashIndicationCb FlashIndication_cbTable = {
    FlashIndicationdisconnect_cb,
    FlashIndicationreadDone_cb,
    FlashIndicationwriteDone_cb,
    FlashIndicationeraseDone_cb,
    FlashIndicationdebugDumpResp_cb,
    FlashIndicationmergeDone_cb,
    FlashIndicationmergeFlushDone1_cb,
    FlashIndicationmergeFlushDone2_cb,
    FlashIndicationfindKeyDone_cb,
};
#endif //NO_CPP_PORTAL_CODE
