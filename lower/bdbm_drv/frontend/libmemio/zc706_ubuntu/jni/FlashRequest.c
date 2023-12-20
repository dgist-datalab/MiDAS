#include "GeneratedTypes.h"

int FlashRequest_readPage ( struct PortalInternal *p, const uint32_t tag, const uint32_t lpa, const uint32_t offset )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashRequest_readPage, 4);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashRequest_readPage, "FlashRequest_readPage")) return 1;
    p->transport->write(p, &temp_working_addr, tag);
    p->transport->write(p, &temp_working_addr, lpa);
    p->transport->write(p, &temp_working_addr, offset);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashRequest_readPage << 16) | 4, -1);
    return 0;
};

int FlashRequest_writePage ( struct PortalInternal *p, const uint32_t tag, const uint32_t lpa, const uint32_t offset )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashRequest_writePage, 4);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashRequest_writePage, "FlashRequest_writePage")) return 1;
    p->transport->write(p, &temp_working_addr, tag);
    p->transport->write(p, &temp_working_addr, lpa);
    p->transport->write(p, &temp_working_addr, offset);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashRequest_writePage << 16) | 4, -1);
    return 0;
};

int FlashRequest_eraseBlock ( struct PortalInternal *p, const uint32_t tag, const uint32_t lpa )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashRequest_eraseBlock, 3);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashRequest_eraseBlock, "FlashRequest_eraseBlock")) return 1;
    p->transport->write(p, &temp_working_addr, tag);
    p->transport->write(p, &temp_working_addr, lpa);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashRequest_eraseBlock << 16) | 3, -1);
    return 0;
};

int FlashRequest_setDmaReadRef ( struct PortalInternal *p, const uint32_t sgId )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashRequest_setDmaReadRef, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashRequest_setDmaReadRef, "FlashRequest_setDmaReadRef")) return 1;
    p->transport->write(p, &temp_working_addr, sgId);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashRequest_setDmaReadRef << 16) | 2, -1);
    return 0;
};

int FlashRequest_setDmaWriteRef ( struct PortalInternal *p, const uint32_t sgId )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashRequest_setDmaWriteRef, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashRequest_setDmaWriteRef, "FlashRequest_setDmaWriteRef")) return 1;
    p->transport->write(p, &temp_working_addr, sgId);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashRequest_setDmaWriteRef << 16) | 2, -1);
    return 0;
};

int FlashRequest_setDmaMapRef ( struct PortalInternal *p, const uint32_t sgId )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashRequest_setDmaMapRef, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashRequest_setDmaMapRef, "FlashRequest_setDmaMapRef")) return 1;
    p->transport->write(p, &temp_working_addr, sgId);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashRequest_setDmaMapRef << 16) | 2, -1);
    return 0;
};

int FlashRequest_downloadMap ( struct PortalInternal *p )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashRequest_downloadMap, 1);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashRequest_downloadMap, "FlashRequest_downloadMap")) return 1;
    p->transport->write(p, &temp_working_addr, 0);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashRequest_downloadMap << 16) | 1, -1);
    return 0;
};

int FlashRequest_uploadMap ( struct PortalInternal *p )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashRequest_uploadMap, 1);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashRequest_uploadMap, "FlashRequest_uploadMap")) return 1;
    p->transport->write(p, &temp_working_addr, 0);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashRequest_uploadMap << 16) | 1, -1);
    return 0;
};

int FlashRequest_start ( struct PortalInternal *p, const uint32_t dummy )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashRequest_start, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashRequest_start, "FlashRequest_start")) return 1;
    p->transport->write(p, &temp_working_addr, dummy);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashRequest_start << 16) | 2, -1);
    return 0;
};

int FlashRequest_debugDumpReq ( struct PortalInternal *p, const uint32_t dummy )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashRequest_debugDumpReq, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashRequest_debugDumpReq, "FlashRequest_debugDumpReq")) return 1;
    p->transport->write(p, &temp_working_addr, dummy);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashRequest_debugDumpReq << 16) | 2, -1);
    return 0;
};

int FlashRequest_setDebugVals ( struct PortalInternal *p, const uint32_t flag, const uint32_t debugDelay )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashRequest_setDebugVals, 3);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashRequest_setDebugVals, "FlashRequest_setDebugVals")) return 1;
    p->transport->write(p, &temp_working_addr, flag);
    p->transport->write(p, &temp_working_addr, debugDelay);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashRequest_setDebugVals << 16) | 3, -1);
    return 0;
};

FlashRequestCb FlashRequestProxyReq = {
    portal_disconnect,
    FlashRequest_readPage,
    FlashRequest_writePage,
    FlashRequest_eraseBlock,
    FlashRequest_setDmaReadRef,
    FlashRequest_setDmaWriteRef,
    FlashRequest_setDmaMapRef,
    FlashRequest_downloadMap,
    FlashRequest_uploadMap,
    FlashRequest_start,
    FlashRequest_debugDumpReq,
    FlashRequest_setDebugVals,
};
FlashRequestCb *pFlashRequestProxyReq = &FlashRequestProxyReq;

const uint32_t FlashRequest_reqinfo = 0xb0010;
const char * FlashRequest_methodSignatures()
{
    return "{\"setDebugVals\": [\"long\", \"long\"], \"setDmaMapRef\": [\"long\"], \"writePage\": [\"long\", \"long\", \"long\"], \"eraseBlock\": [\"long\", \"long\"], \"debugDumpReq\": [\"long\"], \"setDmaWriteRef\": [\"long\"], \"start\": [\"long\"], \"uploadMap\": [], \"setDmaReadRef\": [\"long\"], \"downloadMap\": [], \"readPage\": [\"long\", \"long\", \"long\"]}";
}

int FlashRequest_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd)
{
    static int runaway = 0;
    int   tmp __attribute__ ((unused));
    int tmpfd __attribute__ ((unused));
    FlashRequestData tempdata __attribute__ ((unused));
    memset(&tempdata, 0, sizeof(tempdata));
    volatile unsigned int* temp_working_addr = p->transport->mapchannelInd(p, channel);
    switch (channel) {
    case CHAN_NUM_FlashRequest_readPage: {
        
        p->transport->recv(p, temp_working_addr, 3, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.readPage.tag = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.readPage.lpa = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.readPage.offset = (uint32_t)(((tmp)&0xfffffffful));((FlashRequestCb *)p->cb)->readPage(p, tempdata.readPage.tag, tempdata.readPage.lpa, tempdata.readPage.offset);
      } break;
    case CHAN_NUM_FlashRequest_writePage: {
        
        p->transport->recv(p, temp_working_addr, 3, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.writePage.tag = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.writePage.lpa = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.writePage.offset = (uint32_t)(((tmp)&0xfffffffful));((FlashRequestCb *)p->cb)->writePage(p, tempdata.writePage.tag, tempdata.writePage.lpa, tempdata.writePage.offset);
      } break;
    case CHAN_NUM_FlashRequest_eraseBlock: {
        
        p->transport->recv(p, temp_working_addr, 2, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.eraseBlock.tag = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.eraseBlock.lpa = (uint32_t)(((tmp)&0xfffffffful));((FlashRequestCb *)p->cb)->eraseBlock(p, tempdata.eraseBlock.tag, tempdata.eraseBlock.lpa);
      } break;
    case CHAN_NUM_FlashRequest_setDmaReadRef: {
        
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.setDmaReadRef.sgId = (uint32_t)(((tmp)&0xfffffffful));((FlashRequestCb *)p->cb)->setDmaReadRef(p, tempdata.setDmaReadRef.sgId);
      } break;
    case CHAN_NUM_FlashRequest_setDmaWriteRef: {
        
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.setDmaWriteRef.sgId = (uint32_t)(((tmp)&0xfffffffful));((FlashRequestCb *)p->cb)->setDmaWriteRef(p, tempdata.setDmaWriteRef.sgId);
      } break;
    case CHAN_NUM_FlashRequest_setDmaMapRef: {
        
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.setDmaMapRef.sgId = (uint32_t)(((tmp)&0xfffffffful));((FlashRequestCb *)p->cb)->setDmaMapRef(p, tempdata.setDmaMapRef.sgId);
      } break;
    case CHAN_NUM_FlashRequest_downloadMap: {
        
        p->transport->recv(p, temp_working_addr, 0, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);((FlashRequestCb *)p->cb)->downloadMap(p);
      } break;
    case CHAN_NUM_FlashRequest_uploadMap: {
        
        p->transport->recv(p, temp_working_addr, 0, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);((FlashRequestCb *)p->cb)->uploadMap(p);
      } break;
    case CHAN_NUM_FlashRequest_start: {
        
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.start.dummy = (uint32_t)(((tmp)&0xfffffffful));((FlashRequestCb *)p->cb)->start(p, tempdata.start.dummy);
      } break;
    case CHAN_NUM_FlashRequest_debugDumpReq: {
        
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.debugDumpReq.dummy = (uint32_t)(((tmp)&0xfffffffful));((FlashRequestCb *)p->cb)->debugDumpReq(p, tempdata.debugDumpReq.dummy);
      } break;
    case CHAN_NUM_FlashRequest_setDebugVals: {
        
        p->transport->recv(p, temp_working_addr, 2, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.setDebugVals.flag = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.setDebugVals.debugDelay = (uint32_t)(((tmp)&0xfffffffful));((FlashRequestCb *)p->cb)->setDebugVals(p, tempdata.setDebugVals.flag, tempdata.setDebugVals.debugDelay);
      } break;
    default:
        PORTAL_PRINTF("FlashRequest_handleMessage: unknown channel 0x%x\n", channel);
        if (runaway++ > 10) {
            PORTAL_PRINTF("FlashRequest_handleMessage: too many bogus indications, exiting\n");
#ifndef __KERNEL__
            exit(-1);
#endif
        }
        return 0;
    }
    return 0;
}
