#include "GeneratedTypes.h"
#ifndef _AMFINDICATION_H_
#define _AMFINDICATION_H_
#include "portal.h"

class AmfIndicationProxy : public Portal {
    AmfIndicationCb *cb;
public:
    AmfIndicationProxy(int id, int tile = DEFAULT_TILE, AmfIndicationCb *cbarg = &AmfIndicationProxyReq, int bufsize = AmfIndication_reqinfo, PortalPoller *poller = 0) :
        Portal(id, tile, bufsize, NULL, NULL, this, poller), cb(cbarg) {};
    AmfIndicationProxy(int id, PortalTransportFunctions *transport, void *param, AmfIndicationCb *cbarg = &AmfIndicationProxyReq, int bufsize = AmfIndication_reqinfo, PortalPoller *poller = 0) :
        Portal(id, DEFAULT_TILE, bufsize, NULL, NULL, transport, param, this, poller), cb(cbarg) {};
    AmfIndicationProxy(int id, PortalPoller *poller) :
        Portal(id, DEFAULT_TILE, AmfIndication_reqinfo, NULL, NULL, NULL, NULL, this, poller), cb(&AmfIndicationProxyReq) {};
    int readDone ( const uint8_t tag ) { return cb->readDone (&pint, tag); };
    int writeDone ( const uint8_t tag ) { return cb->writeDone (&pint, tag); };
    int eraseDone ( const uint8_t tag, const uint8_t status ) { return cb->eraseDone (&pint, tag, status); };
    int debugDumpResp ( const uint32_t debug0, const uint32_t debug1, const uint32_t debug2, const uint32_t debug3, const uint32_t debug4, const uint32_t debug5 ) { return cb->debugDumpResp (&pint, debug0, debug1, debug2, debug3, debug4, debug5); };
    int respAftlFailed ( const AmfRequestT resp ) { return cb->respAftlFailed (&pint, resp); };
    int respReadMapping ( const uint8_t allocated, const uint16_t block_num ) { return cb->respReadMapping (&pint, allocated, block_num); };
    int respReadBlkInfo ( const bsvvector_Luint16_t_L8 blkinfo_vec ) { return cb->respReadBlkInfo (&pint, blkinfo_vec); };
    int respAftlLoaded ( const uint8_t resp ) { return cb->respAftlLoaded (&pint, resp); };
};

extern AmfIndicationCb AmfIndication_cbTable;
class AmfIndicationWrapper : public Portal {
public:
    AmfIndicationWrapper(int id, int tile = DEFAULT_TILE, PORTAL_INDFUNC cba = AmfIndication_handleMessage, int bufsize = AmfIndication_reqinfo, PortalPoller *poller = 0) :
           Portal(id, tile, bufsize, cba, (void *)&AmfIndication_cbTable, this, poller) {
    };
    AmfIndicationWrapper(int id, PortalTransportFunctions *transport, void *param, PORTAL_INDFUNC cba = AmfIndication_handleMessage, int bufsize = AmfIndication_reqinfo, PortalPoller *poller=0):
           Portal(id, DEFAULT_TILE, bufsize, cba, (void *)&AmfIndication_cbTable, transport, param, this, poller) {
    };
    AmfIndicationWrapper(int id, PortalPoller *poller) :
           Portal(id, DEFAULT_TILE, AmfIndication_reqinfo, AmfIndication_handleMessage, (void *)&AmfIndication_cbTable, this, poller) {
    };
    AmfIndicationWrapper(int id, PortalTransportFunctions *transport, void *param, PortalPoller *poller):
           Portal(id, DEFAULT_TILE, AmfIndication_reqinfo, AmfIndication_handleMessage, (void *)&AmfIndication_cbTable, transport, param, this, poller) {
    };
    virtual void disconnect(void) {
        printf("AmfIndicationWrapper.disconnect called %d\n", pint.client_fd_number);
    };
    virtual void readDone ( const uint8_t tag ) = 0;
    virtual void writeDone ( const uint8_t tag ) = 0;
    virtual void eraseDone ( const uint8_t tag, const uint8_t status ) = 0;
    virtual void debugDumpResp ( const uint32_t debug0, const uint32_t debug1, const uint32_t debug2, const uint32_t debug3, const uint32_t debug4, const uint32_t debug5 ) = 0;
    virtual void respAftlFailed ( const AmfRequestT resp ) = 0;
    virtual void respReadMapping ( const uint8_t allocated, const uint16_t block_num ) = 0;
    virtual void respReadBlkInfo ( const bsvvector_Luint16_t_L8 blkinfo_vec ) = 0;
    virtual void respAftlLoaded ( const uint8_t resp ) = 0;
};
#endif // _AMFINDICATION_H_
