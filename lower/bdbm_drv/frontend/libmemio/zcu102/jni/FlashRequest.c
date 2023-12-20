#include "GeneratedTypes.h"

int FlashRequest_readPage ( struct PortalInternal *p, const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t page, const uint32_t tag, const uint32_t offset )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashRequest_readPage, 7);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashRequest_readPage, "FlashRequest_readPage")) return 1;
    p->transport->write(p, &temp_working_addr, bus);
    p->transport->write(p, &temp_working_addr, chip);
    p->transport->write(p, &temp_working_addr, block);
    p->transport->write(p, &temp_working_addr, page);
    p->transport->write(p, &temp_working_addr, tag);
    p->transport->write(p, &temp_working_addr, offset);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashRequest_readPage << 16) | 7, -1);
    return 0;
};

int FlashRequest_writePage ( struct PortalInternal *p, const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t page, const uint32_t tag, const uint32_t offset )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashRequest_writePage, 7);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashRequest_writePage, "FlashRequest_writePage")) return 1;
    p->transport->write(p, &temp_working_addr, bus);
    p->transport->write(p, &temp_working_addr, chip);
    p->transport->write(p, &temp_working_addr, block);
    p->transport->write(p, &temp_working_addr, page);
    p->transport->write(p, &temp_working_addr, tag);
    p->transport->write(p, &temp_working_addr, offset);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashRequest_writePage << 16) | 7, -1);
    return 0;
};

int FlashRequest_eraseBlock ( struct PortalInternal *p, const uint32_t bus, const uint32_t chip, const uint32_t block, const uint32_t tag )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashRequest_eraseBlock, 5);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashRequest_eraseBlock, "FlashRequest_eraseBlock")) return 1;
    p->transport->write(p, &temp_working_addr, bus);
    p->transport->write(p, &temp_working_addr, chip);
    p->transport->write(p, &temp_working_addr, block);
    p->transport->write(p, &temp_working_addr, tag);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashRequest_eraseBlock << 16) | 5, -1);
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

int FlashRequest_startCompaction ( struct PortalInternal *p, const uint32_t cntHigh, const uint32_t cntLow, const uint32_t destPpaFlag )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashRequest_startCompaction, 4);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashRequest_startCompaction, "FlashRequest_startCompaction")) return 1;
    p->transport->write(p, &temp_working_addr, cntHigh);
    p->transport->write(p, &temp_working_addr, cntLow);
    p->transport->write(p, &temp_working_addr, destPpaFlag);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashRequest_startCompaction << 16) | 4, -1);
    return 0;
};

int FlashRequest_setDmaKtPpaRef ( struct PortalInternal *p, const uint32_t sgIdHigh, const uint32_t sgIdLow, const uint32_t sgIdRes1, const uint32_t sgIdRes2 )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashRequest_setDmaKtPpaRef, 5);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashRequest_setDmaKtPpaRef, "FlashRequest_setDmaKtPpaRef")) return 1;
    p->transport->write(p, &temp_working_addr, sgIdHigh);
    p->transport->write(p, &temp_working_addr, sgIdLow);
    p->transport->write(p, &temp_working_addr, sgIdRes1);
    p->transport->write(p, &temp_working_addr, sgIdRes2);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashRequest_setDmaKtPpaRef << 16) | 5, -1);
    return 0;
};

int FlashRequest_setDmaKtOutputRef ( struct PortalInternal *p, const uint32_t sgIdKtBuf, const uint32_t sgIdInvalPPA )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashRequest_setDmaKtOutputRef, 3);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashRequest_setDmaKtOutputRef, "FlashRequest_setDmaKtOutputRef")) return 1;
    p->transport->write(p, &temp_working_addr, sgIdKtBuf);
    p->transport->write(p, &temp_working_addr, sgIdInvalPPA);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashRequest_setDmaKtOutputRef << 16) | 3, -1);
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
    FlashRequest_startCompaction,
    FlashRequest_setDmaKtPpaRef,
    FlashRequest_setDmaKtOutputRef,
    FlashRequest_start,
    FlashRequest_debugDumpReq,
    FlashRequest_setDebugVals,
};
FlashRequestCb *pFlashRequestProxyReq = &FlashRequestProxyReq;

const uint32_t FlashRequest_reqinfo = 0xb001c;
const char * FlashRequest_methodSignatures()
{
    return "{\"setDebugVals\": [\"long\", \"long\"], \"writePage\": [\"long\", \"long\", \"long\", \"long\", \"long\", \"long\"], \"eraseBlock\": [\"long\", \"long\", \"long\", \"long\"], \"debugDumpReq\": [\"long\"], \"setDmaWriteRef\": [\"long\"], \"setDmaKtOutputRef\": [\"long\", \"long\"], \"setDmaKtPpaRef\": [\"long\", \"long\", \"long\", \"long\"], \"startCompaction\": [\"long\", \"long\", \"long\"], \"setDmaReadRef\": [\"long\"], \"start\": [\"long\"], \"readPage\": [\"long\", \"long\", \"long\", \"long\", \"long\", \"long\"]}";
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
        
        p->transport->recv(p, temp_working_addr, 6, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.readPage.bus = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.readPage.chip = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.readPage.block = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.readPage.page = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.readPage.tag = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.readPage.offset = (uint32_t)(((tmp)&0xfffffffful));
        ((FlashRequestCb *)p->cb)->readPage(p, tempdata.readPage.bus, tempdata.readPage.chip, tempdata.readPage.block, tempdata.readPage.page, tempdata.readPage.tag, tempdata.readPage.offset);
      } break;
    case CHAN_NUM_FlashRequest_writePage: {
        
        p->transport->recv(p, temp_working_addr, 6, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.writePage.bus = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.writePage.chip = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.writePage.block = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.writePage.page = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.writePage.tag = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.writePage.offset = (uint32_t)(((tmp)&0xfffffffful));
        ((FlashRequestCb *)p->cb)->writePage(p, tempdata.writePage.bus, tempdata.writePage.chip, tempdata.writePage.block, tempdata.writePage.page, tempdata.writePage.tag, tempdata.writePage.offset);
      } break;
    case CHAN_NUM_FlashRequest_eraseBlock: {
        
        p->transport->recv(p, temp_working_addr, 4, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.eraseBlock.bus = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.eraseBlock.chip = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.eraseBlock.block = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.eraseBlock.tag = (uint32_t)(((tmp)&0xfffffffful));
        ((FlashRequestCb *)p->cb)->eraseBlock(p, tempdata.eraseBlock.bus, tempdata.eraseBlock.chip, tempdata.eraseBlock.block, tempdata.eraseBlock.tag);
      } break;
    case CHAN_NUM_FlashRequest_setDmaReadRef: {
        
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.setDmaReadRef.sgId = (uint32_t)(((tmp)&0xfffffffful));
        ((FlashRequestCb *)p->cb)->setDmaReadRef(p, tempdata.setDmaReadRef.sgId);
      } break;
    case CHAN_NUM_FlashRequest_setDmaWriteRef: {
        
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.setDmaWriteRef.sgId = (uint32_t)(((tmp)&0xfffffffful));
        ((FlashRequestCb *)p->cb)->setDmaWriteRef(p, tempdata.setDmaWriteRef.sgId);
      } break;
    case CHAN_NUM_FlashRequest_startCompaction: {
        
        p->transport->recv(p, temp_working_addr, 3, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.startCompaction.cntHigh = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.startCompaction.cntLow = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.startCompaction.destPpaFlag = (uint32_t)(((tmp)&0xfffffffful));
        ((FlashRequestCb *)p->cb)->startCompaction(p, tempdata.startCompaction.cntHigh, tempdata.startCompaction.cntLow, tempdata.startCompaction.destPpaFlag);
      } break;
    case CHAN_NUM_FlashRequest_setDmaKtPpaRef: {
        
        p->transport->recv(p, temp_working_addr, 4, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.setDmaKtPpaRef.sgIdHigh = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.setDmaKtPpaRef.sgIdLow = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.setDmaKtPpaRef.sgIdRes1 = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.setDmaKtPpaRef.sgIdRes2 = (uint32_t)(((tmp)&0xfffffffful));
        ((FlashRequestCb *)p->cb)->setDmaKtPpaRef(p, tempdata.setDmaKtPpaRef.sgIdHigh, tempdata.setDmaKtPpaRef.sgIdLow, tempdata.setDmaKtPpaRef.sgIdRes1, tempdata.setDmaKtPpaRef.sgIdRes2);
      } break;
    case CHAN_NUM_FlashRequest_setDmaKtOutputRef: {
        
        p->transport->recv(p, temp_working_addr, 2, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.setDmaKtOutputRef.sgIdKtBuf = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.setDmaKtOutputRef.sgIdInvalPPA = (uint32_t)(((tmp)&0xfffffffful));
        ((FlashRequestCb *)p->cb)->setDmaKtOutputRef(p, tempdata.setDmaKtOutputRef.sgIdKtBuf, tempdata.setDmaKtOutputRef.sgIdInvalPPA);
      } break;
    case CHAN_NUM_FlashRequest_start: {
        
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.start.dummy = (uint32_t)(((tmp)&0xfffffffful));
        ((FlashRequestCb *)p->cb)->start(p, tempdata.start.dummy);
      } break;
    case CHAN_NUM_FlashRequest_debugDumpReq: {
        
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.debugDumpReq.dummy = (uint32_t)(((tmp)&0xfffffffful));
        ((FlashRequestCb *)p->cb)->debugDumpReq(p, tempdata.debugDumpReq.dummy);
      } break;
    case CHAN_NUM_FlashRequest_setDebugVals: {
        
        p->transport->recv(p, temp_working_addr, 2, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.setDebugVals.flag = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.setDebugVals.debugDelay = (uint32_t)(((tmp)&0xfffffffful));
        ((FlashRequestCb *)p->cb)->setDebugVals(p, tempdata.setDebugVals.flag, tempdata.setDebugVals.debugDelay);
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
