#include "GeneratedTypes.h"

int FlashIndication_readDone ( struct PortalInternal *p, const uint32_t tag )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashIndication_readDone, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashIndication_readDone, "FlashIndication_readDone")) return 1;
    p->transport->write(p, &temp_working_addr, tag);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashIndication_readDone << 16) | 2, -1);
    return 0;
};

int FlashIndication_writeDone ( struct PortalInternal *p, const uint32_t tag )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashIndication_writeDone, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashIndication_writeDone, "FlashIndication_writeDone")) return 1;
    p->transport->write(p, &temp_working_addr, tag);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashIndication_writeDone << 16) | 2, -1);
    return 0;
};

int FlashIndication_eraseDone ( struct PortalInternal *p, const uint32_t tag, const uint32_t status )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashIndication_eraseDone, 3);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashIndication_eraseDone, "FlashIndication_eraseDone")) return 1;
    p->transport->write(p, &temp_working_addr, tag);
    p->transport->write(p, &temp_working_addr, status);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashIndication_eraseDone << 16) | 3, -1);
    return 0;
};

int FlashIndication_debugDumpResp ( struct PortalInternal *p, const uint32_t debug0, const uint32_t debug1, const uint32_t debug2, const uint32_t debug3, const uint32_t debug4, const uint32_t debug5 )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashIndication_debugDumpResp, 7);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashIndication_debugDumpResp, "FlashIndication_debugDumpResp")) return 1;
    p->transport->write(p, &temp_working_addr, debug0);
    p->transport->write(p, &temp_working_addr, debug1);
    p->transport->write(p, &temp_working_addr, debug2);
    p->transport->write(p, &temp_working_addr, debug3);
    p->transport->write(p, &temp_working_addr, debug4);
    p->transport->write(p, &temp_working_addr, debug5);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashIndication_debugDumpResp << 16) | 7, -1);
    return 0;
};

int FlashIndication_mergeDone ( struct PortalInternal *p, const uint32_t numGenKt, const uint32_t numInvalAddr, const uint64_t counter )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashIndication_mergeDone, 5);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashIndication_mergeDone, "FlashIndication_mergeDone")) return 1;
    p->transport->write(p, &temp_working_addr, numGenKt);
    p->transport->write(p, &temp_working_addr, numInvalAddr);
    p->transport->write(p, &temp_working_addr, (counter>>32));
    p->transport->write(p, &temp_working_addr, counter);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashIndication_mergeDone << 16) | 5, -1);
    return 0;
};

int FlashIndication_mergeFlushDone1 ( struct PortalInternal *p, const uint32_t num )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashIndication_mergeFlushDone1, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashIndication_mergeFlushDone1, "FlashIndication_mergeFlushDone1")) return 1;
    p->transport->write(p, &temp_working_addr, num);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashIndication_mergeFlushDone1 << 16) | 2, -1);
    return 0;
};

int FlashIndication_mergeFlushDone2 ( struct PortalInternal *p, const uint32_t num )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashIndication_mergeFlushDone2, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashIndication_mergeFlushDone2, "FlashIndication_mergeFlushDone2")) return 1;
    p->transport->write(p, &temp_working_addr, num);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashIndication_mergeFlushDone2 << 16) | 2, -1);
    return 0;
};

int FlashIndication_findKeyDone ( struct PortalInternal *p, const uint16_t tag, const uint16_t status, const uint32_t ppa )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_FlashIndication_findKeyDone, 3);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_FlashIndication_findKeyDone, "FlashIndication_findKeyDone")) return 1;
    p->transport->write(p, &temp_working_addr, status|(((unsigned long)tag)<<16));
    p->transport->write(p, &temp_working_addr, ppa);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_FlashIndication_findKeyDone << 16) | 3, -1);
    return 0;
};

FlashIndicationCb FlashIndicationProxyReq = {
    portal_disconnect,
    FlashIndication_readDone,
    FlashIndication_writeDone,
    FlashIndication_eraseDone,
    FlashIndication_debugDumpResp,
    FlashIndication_mergeDone,
    FlashIndication_mergeFlushDone1,
    FlashIndication_mergeFlushDone2,
    FlashIndication_findKeyDone,
};
FlashIndicationCb *pFlashIndicationProxyReq = &FlashIndicationProxyReq;

const uint32_t FlashIndication_reqinfo = 0x8001c;
const char * FlashIndication_methodSignatures()
{
    return "{\"readDone\": [\"long\"], \"findKeyDone\": [\"long\", \"long\", \"long\"], \"eraseDone\": [\"long\", \"long\"], \"debugDumpResp\": [\"long\", \"long\", \"long\", \"long\", \"long\", \"long\"], \"writeDone\": [\"long\"], \"mergeFlushDone1\": [\"long\"], \"mergeFlushDone2\": [\"long\"], \"mergeDone\": [\"long\", \"long\", \"long\"]}";
}

int FlashIndication_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd)
{
    static int runaway = 0;
    int   tmp __attribute__ ((unused));
    int tmpfd __attribute__ ((unused));
    FlashIndicationData tempdata __attribute__ ((unused));
    memset(&tempdata, 0, sizeof(tempdata));
    volatile unsigned int* temp_working_addr = p->transport->mapchannelInd(p, channel);
    switch (channel) {
    case CHAN_NUM_FlashIndication_readDone: {
        
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.readDone.tag = (uint32_t)(((tmp)&0xfffffffful));
        ((FlashIndicationCb *)p->cb)->readDone(p, tempdata.readDone.tag);
      } break;
    case CHAN_NUM_FlashIndication_writeDone: {
        
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.writeDone.tag = (uint32_t)(((tmp)&0xfffffffful));
        ((FlashIndicationCb *)p->cb)->writeDone(p, tempdata.writeDone.tag);
      } break;
    case CHAN_NUM_FlashIndication_eraseDone: {
        
        p->transport->recv(p, temp_working_addr, 2, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.eraseDone.tag = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.eraseDone.status = (uint32_t)(((tmp)&0xfffffffful));
        ((FlashIndicationCb *)p->cb)->eraseDone(p, tempdata.eraseDone.tag, tempdata.eraseDone.status);
      } break;
    case CHAN_NUM_FlashIndication_debugDumpResp: {
        
        p->transport->recv(p, temp_working_addr, 6, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.debugDumpResp.debug0 = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.debugDumpResp.debug1 = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.debugDumpResp.debug2 = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.debugDumpResp.debug3 = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.debugDumpResp.debug4 = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.debugDumpResp.debug5 = (uint32_t)(((tmp)&0xfffffffful));
        ((FlashIndicationCb *)p->cb)->debugDumpResp(p, tempdata.debugDumpResp.debug0, tempdata.debugDumpResp.debug1, tempdata.debugDumpResp.debug2, tempdata.debugDumpResp.debug3, tempdata.debugDumpResp.debug4, tempdata.debugDumpResp.debug5);
      } break;
    case CHAN_NUM_FlashIndication_mergeDone: {
        
        p->transport->recv(p, temp_working_addr, 4, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.mergeDone.numGenKt = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.mergeDone.numInvalAddr = (uint32_t)(((tmp)&0xfffffffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.mergeDone.counter = (uint64_t)(((uint64_t)(((tmp)&0xfffffffful))<<32));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.mergeDone.counter |= (uint64_t)(((tmp)&0xfffffffful));
        ((FlashIndicationCb *)p->cb)->mergeDone(p, tempdata.mergeDone.numGenKt, tempdata.mergeDone.numInvalAddr, tempdata.mergeDone.counter);
      } break;
    case CHAN_NUM_FlashIndication_mergeFlushDone1: {
        
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.mergeFlushDone1.num = (uint32_t)(((tmp)&0xfffffffful));
        ((FlashIndicationCb *)p->cb)->mergeFlushDone1(p, tempdata.mergeFlushDone1.num);
      } break;
    case CHAN_NUM_FlashIndication_mergeFlushDone2: {
        
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.mergeFlushDone2.num = (uint32_t)(((tmp)&0xfffffffful));
        ((FlashIndicationCb *)p->cb)->mergeFlushDone2(p, tempdata.mergeFlushDone2.num);
      } break;
    case CHAN_NUM_FlashIndication_findKeyDone: {
        
        p->transport->recv(p, temp_working_addr, 2, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.findKeyDone.status = (uint16_t)(((tmp)&0xfffful));
        tempdata.findKeyDone.tag = (uint16_t)(((tmp>>16)&0xfffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.findKeyDone.ppa = (uint32_t)(((tmp)&0xfffffffful));
        ((FlashIndicationCb *)p->cb)->findKeyDone(p, tempdata.findKeyDone.tag, tempdata.findKeyDone.status, tempdata.findKeyDone.ppa);
      } break;
    default:
        PORTAL_PRINTF("FlashIndication_handleMessage: unknown channel 0x%x\n", channel);
        if (runaway++ > 10) {
            PORTAL_PRINTF("FlashIndication_handleMessage: too many bogus indications, exiting\n");
#ifndef __KERNEL__
            exit(-1);
#endif
        }
        return 0;
    }
    return 0;
}
