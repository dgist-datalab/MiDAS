#include "GeneratedTypes.h"
#ifndef _FLASHINDICATION_H_
#define _FLASHINDICATION_H_
#include "portal.h"

class FlashIndicationProxy : public Portal {
    FlashIndicationCb *cb;
public:
    FlashIndicationProxy(int id, int tile = DEFAULT_TILE, FlashIndicationCb *cbarg = &FlashIndicationProxyReq, int bufsize = FlashIndication_reqinfo, PortalPoller *poller = 0) :
        Portal(id, tile, bufsize, NULL, NULL, this, poller), cb(cbarg) {};
    FlashIndicationProxy(int id, PortalTransportFunctions *transport, void *param, FlashIndicationCb *cbarg = &FlashIndicationProxyReq, int bufsize = FlashIndication_reqinfo, PortalPoller *poller = 0) :
        Portal(id, DEFAULT_TILE, bufsize, NULL, NULL, transport, param, this, poller), cb(cbarg) {};
    FlashIndicationProxy(int id, PortalPoller *poller) :
        Portal(id, DEFAULT_TILE, FlashIndication_reqinfo, NULL, NULL, NULL, NULL, this, poller), cb(&FlashIndicationProxyReq) {};
    int readDone ( const uint32_t tag ) { return cb->readDone (&pint, tag); };
    int writeDone ( const uint32_t tag ) { return cb->writeDone (&pint, tag); };
    int eraseDone ( const uint32_t tag, const uint32_t status ) { return cb->eraseDone (&pint, tag, status); };
    int debugDumpResp ( const uint32_t debug0, const uint32_t debug1, const uint32_t debug2, const uint32_t debug3, const uint32_t debug4, const uint32_t debug5 ) { return cb->debugDumpResp (&pint, debug0, debug1, debug2, debug3, debug4, debug5); };
    int mergeDone ( const uint32_t numGenKt, const uint32_t numInvalAddr, const uint64_t counter ) { return cb->mergeDone (&pint, numGenKt, numInvalAddr, counter); };
    int mergeFlushDone1 ( const uint32_t num ) { return cb->mergeFlushDone1 (&pint, num); };
    int mergeFlushDone2 ( const uint32_t num ) { return cb->mergeFlushDone2 (&pint, num); };
    int findKeyDone ( const uint16_t tag, const uint16_t status, const uint32_t ppa ) { return cb->findKeyDone (&pint, tag, status, ppa); };
};

extern FlashIndicationCb FlashIndication_cbTable;
class FlashIndicationWrapper : public Portal {
public:
    FlashIndicationWrapper(int id, int tile = DEFAULT_TILE, PORTAL_INDFUNC cba = FlashIndication_handleMessage, int bufsize = FlashIndication_reqinfo, PortalPoller *poller = 0) :
           Portal(id, tile, bufsize, cba, (void *)&FlashIndication_cbTable, this, poller) {
    };
    FlashIndicationWrapper(int id, PortalTransportFunctions *transport, void *param, PORTAL_INDFUNC cba = FlashIndication_handleMessage, int bufsize = FlashIndication_reqinfo, PortalPoller *poller=0):
           Portal(id, DEFAULT_TILE, bufsize, cba, (void *)&FlashIndication_cbTable, transport, param, this, poller) {
    };
    FlashIndicationWrapper(int id, PortalPoller *poller) :
           Portal(id, DEFAULT_TILE, FlashIndication_reqinfo, FlashIndication_handleMessage, (void *)&FlashIndication_cbTable, this, poller) {
    };
    FlashIndicationWrapper(int id, PortalTransportFunctions *transport, void *param, PortalPoller *poller):
           Portal(id, DEFAULT_TILE, FlashIndication_reqinfo, FlashIndication_handleMessage, (void *)&FlashIndication_cbTable, transport, param, this, poller) {
    };
    virtual void disconnect(void) {
        printf("FlashIndicationWrapper.disconnect called %d\n", pint.client_fd_number);
    };
    virtual void readDone ( const uint32_t tag ) = 0;
    virtual void writeDone ( const uint32_t tag ) = 0;
    virtual void eraseDone ( const uint32_t tag, const uint32_t status ) = 0;
    virtual void debugDumpResp ( const uint32_t debug0, const uint32_t debug1, const uint32_t debug2, const uint32_t debug3, const uint32_t debug4, const uint32_t debug5 ) = 0;
    virtual void mergeDone ( const uint32_t numGenKt, const uint32_t numInvalAddr, const uint64_t counter ) = 0;
    virtual void mergeFlushDone1 ( const uint32_t num ) = 0;
    virtual void mergeFlushDone2 ( const uint32_t num ) = 0;
    virtual void findKeyDone ( const uint16_t tag, const uint16_t status, const uint32_t ppa ) = 0;
};
#endif // _FLASHINDICATION_H_
