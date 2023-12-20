#include "GeneratedTypes.h"
#ifndef _FLASHREQUEST_H_
#define _FLASHREQUEST_H_
#include "portal.h"

class FlashRequestProxy : public Portal {
    FlashRequestCb *cb;
public:
    FlashRequestProxy(int id, int tile = DEFAULT_TILE, FlashRequestCb *cbarg = &FlashRequestProxyReq, int bufsize = FlashRequest_reqinfo, PortalPoller *poller = 0) :
        Portal(id, tile, bufsize, NULL, NULL, this, poller), cb(cbarg) {};
    FlashRequestProxy(int id, PortalTransportFunctions *transport, void *param, FlashRequestCb *cbarg = &FlashRequestProxyReq, int bufsize = FlashRequest_reqinfo, PortalPoller *poller = 0) :
        Portal(id, DEFAULT_TILE, bufsize, NULL, NULL, transport, param, this, poller), cb(cbarg) {};
    FlashRequestProxy(int id, PortalPoller *poller) :
        Portal(id, DEFAULT_TILE, FlashRequest_reqinfo, NULL, NULL, NULL, NULL, this, poller), cb(&FlashRequestProxyReq) {};
    int readPage ( const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t page, const uint32_t tag, const uint32_t offset ) { return cb->readPage (&pint, bus, chip, block, page, tag, offset); };
    int writePage ( const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t page, const uint32_t tag, const uint32_t offset ) { return cb->writePage (&pint, bus, chip, block, page, tag, offset); };
    int eraseBlock ( const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t tag ) { return cb->eraseBlock (&pint, bus, chip, block, tag); };
    int setDmaReadRef ( const uint32_t sgId ) { return cb->setDmaReadRef (&pint, sgId); };
    int setDmaWriteRef ( const uint32_t sgId ) { return cb->setDmaWriteRef (&pint, sgId); };
    int startCompaction ( const uint32_t cntHigh, const uint32_t cntLow, const uint32_t destPpaFlag ) { return cb->startCompaction (&pint, cntHigh, cntLow, destPpaFlag); };
    int setDmaKtPpaRef ( const uint32_t sgIdHigh, const uint32_t sgIdLow, const uint32_t sgIdRes1, const uint32_t sgIdRes2 ) { return cb->setDmaKtPpaRef (&pint, sgIdHigh, sgIdLow, sgIdRes1, sgIdRes2); };
    int setDmaKtOutputRef ( const uint32_t sgIdKtBuf, const uint32_t sgIdInvalPPA ) { return cb->setDmaKtOutputRef (&pint, sgIdKtBuf, sgIdInvalPPA); };
    int start ( const uint32_t dummy ) { return cb->start (&pint, dummy); };
    int debugDumpReq ( const uint32_t dummy ) { return cb->debugDumpReq (&pint, dummy); };
    int setDebugVals ( const uint32_t flag, const uint32_t debugDelay ) { return cb->setDebugVals (&pint, flag, debugDelay); };
    int setDmaKtSearchRef ( const uint32_t sgId ) { return cb->setDmaKtSearchRef (&pint, sgId); };
    int findKey ( const uint32_t ppa, const uint32_t keySz, const uint32_t tag ) { return cb->findKey (&pint, ppa, keySz, tag); };
};

extern FlashRequestCb FlashRequest_cbTable;
class FlashRequestWrapper : public Portal {
public:
    FlashRequestWrapper(int id, int tile = DEFAULT_TILE, PORTAL_INDFUNC cba = FlashRequest_handleMessage, int bufsize = FlashRequest_reqinfo, PortalPoller *poller = 0) :
           Portal(id, tile, bufsize, cba, (void *)&FlashRequest_cbTable, this, poller) {
    };
    FlashRequestWrapper(int id, PortalTransportFunctions *transport, void *param, PORTAL_INDFUNC cba = FlashRequest_handleMessage, int bufsize = FlashRequest_reqinfo, PortalPoller *poller=0):
           Portal(id, DEFAULT_TILE, bufsize, cba, (void *)&FlashRequest_cbTable, transport, param, this, poller) {
    };
    FlashRequestWrapper(int id, PortalPoller *poller) :
           Portal(id, DEFAULT_TILE, FlashRequest_reqinfo, FlashRequest_handleMessage, (void *)&FlashRequest_cbTable, this, poller) {
    };
    FlashRequestWrapper(int id, PortalTransportFunctions *transport, void *param, PortalPoller *poller):
           Portal(id, DEFAULT_TILE, FlashRequest_reqinfo, FlashRequest_handleMessage, (void *)&FlashRequest_cbTable, transport, param, this, poller) {
    };
    virtual void disconnect(void) {
        printf("FlashRequestWrapper.disconnect called %d\n", pint.client_fd_number);
    };
    virtual void readPage ( const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t page, const uint32_t tag, const uint32_t offset ) = 0;
    virtual void writePage ( const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t page, const uint32_t tag, const uint32_t offset ) = 0;
    virtual void eraseBlock ( const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t tag ) = 0;
    virtual void setDmaReadRef ( const uint32_t sgId ) = 0;
    virtual void setDmaWriteRef ( const uint32_t sgId ) = 0;
    virtual void startCompaction ( const uint32_t cntHigh, const uint32_t cntLow, const uint32_t destPpaFlag ) = 0;
    virtual void setDmaKtPpaRef ( const uint32_t sgIdHigh, const uint32_t sgIdLow, const uint32_t sgIdRes1, const uint32_t sgIdRes2 ) = 0;
    virtual void setDmaKtOutputRef ( const uint32_t sgIdKtBuf, const uint32_t sgIdInvalPPA ) = 0;
    virtual void start ( const uint32_t dummy ) = 0;
    virtual void debugDumpReq ( const uint32_t dummy ) = 0;
    virtual void setDebugVals ( const uint32_t flag, const uint32_t debugDelay ) = 0;
    virtual void setDmaKtSearchRef ( const uint32_t sgId ) = 0;
    virtual void findKey ( const uint32_t ppa, const uint32_t keySz, const uint32_t tag ) = 0;
};
#endif // _FLASHREQUEST_H_
