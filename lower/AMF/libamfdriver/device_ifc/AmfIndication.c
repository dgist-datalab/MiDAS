#include "GeneratedTypes.h"

int AmfIndication_readDone ( struct PortalInternal *p, const uint8_t tag )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_AmfIndication_readDone, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_AmfIndication_readDone, "AmfIndication_readDone")) return 1;
    p->transport->write(p, &temp_working_addr, tag);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_AmfIndication_readDone << 16) | 2, -1);
    return 0;
};

int AmfIndication_writeDone ( struct PortalInternal *p, const uint8_t tag )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_AmfIndication_writeDone, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_AmfIndication_writeDone, "AmfIndication_writeDone")) return 1;
    p->transport->write(p, &temp_working_addr, tag);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_AmfIndication_writeDone << 16) | 2, -1);
    return 0;
};

int AmfIndication_eraseDone ( struct PortalInternal *p, const uint8_t tag, const uint8_t status )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_AmfIndication_eraseDone, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_AmfIndication_eraseDone, "AmfIndication_eraseDone")) return 1;
    p->transport->write(p, &temp_working_addr, status|(((unsigned long)tag)<<2));
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_AmfIndication_eraseDone << 16) | 2, -1);
    return 0;
};

int AmfIndication_debugDumpResp ( struct PortalInternal *p, const uint32_t debug0, const uint32_t debug1, const uint32_t debug2, const uint32_t debug3, const uint32_t debug4, const uint32_t debug5 )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_AmfIndication_debugDumpResp, 7);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_AmfIndication_debugDumpResp, "AmfIndication_debugDumpResp")) return 1;
    p->transport->write(p, &temp_working_addr, debug0);
    p->transport->write(p, &temp_working_addr, debug1);
    p->transport->write(p, &temp_working_addr, debug2);
    p->transport->write(p, &temp_working_addr, debug3);
    p->transport->write(p, &temp_working_addr, debug4);
    p->transport->write(p, &temp_working_addr, debug5);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_AmfIndication_debugDumpResp << 16) | 7, -1);
    return 0;
};

int AmfIndication_respAftlFailed ( struct PortalInternal *p, const AmfRequestT resp )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_AmfIndication_respAftlFailed, 3);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_AmfIndication_respAftlFailed, "AmfIndication_respAftlFailed")) return 1;
    p->transport->write(p, &temp_working_addr, (resp.tag>>5)|(((unsigned long)resp.cmd)<<2));
    p->transport->write(p, &temp_working_addr, resp.lpa|(((unsigned long)resp.tag)<<27));
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_AmfIndication_respAftlFailed << 16) | 3, -1);
    return 0;
};

int AmfIndication_respReadMapping ( struct PortalInternal *p, const uint8_t allocated, const uint16_t block_num )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_AmfIndication_respReadMapping, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_AmfIndication_respReadMapping, "AmfIndication_respReadMapping")) return 1;
    p->transport->write(p, &temp_working_addr, block_num|(((unsigned long)allocated)<<14));
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_AmfIndication_respReadMapping << 16) | 2, -1);
    return 0;
};

int AmfIndication_respReadBlkInfo ( struct PortalInternal *p, const bsvvector_Luint16_t_L8 blkinfo_vec )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_AmfIndication_respReadBlkInfo, 5);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_AmfIndication_respReadBlkInfo, "AmfIndication_respReadBlkInfo")) return 1;
    p->transport->write(p, &temp_working_addr, blkinfo_vec[1]|(((unsigned long)blkinfo_vec[0])<<16));
    p->transport->write(p, &temp_working_addr, blkinfo_vec[3]|(((unsigned long)blkinfo_vec[2])<<16));
    p->transport->write(p, &temp_working_addr, blkinfo_vec[5]|(((unsigned long)blkinfo_vec[4])<<16));
    p->transport->write(p, &temp_working_addr, blkinfo_vec[7]|(((unsigned long)blkinfo_vec[6])<<16));
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_AmfIndication_respReadBlkInfo << 16) | 5, -1);
    return 0;
};

int AmfIndication_respAftlLoaded ( struct PortalInternal *p, const uint8_t resp )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_AmfIndication_respAftlLoaded, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_AmfIndication_respAftlLoaded, "AmfIndication_respAftlLoaded")) return 1;
    p->transport->write(p, &temp_working_addr, resp);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_AmfIndication_respAftlLoaded << 16) | 2, -1);
    return 0;
};

AmfIndicationCb AmfIndicationProxyReq = {
    portal_disconnect,
    AmfIndication_readDone,
    AmfIndication_writeDone,
    AmfIndication_eraseDone,
    AmfIndication_debugDumpResp,
    AmfIndication_respAftlFailed,
    AmfIndication_respReadMapping,
    AmfIndication_respReadBlkInfo,
    AmfIndication_respAftlLoaded,
};
AmfIndicationCb *pAmfIndicationProxyReq = &AmfIndicationProxyReq;

const uint32_t AmfIndication_reqinfo = 0x8001c;
const char * AmfIndication_methodSignatures()
{
    return "{\"respReadBlkInfo\": [\"long\"], \"readDone\": [\"long\"], \"respAftlLoaded\": [\"long\"], \"eraseDone\": [\"long\", \"long\"], \"debugDumpResp\": [\"long\", \"long\", \"long\", \"long\", \"long\", \"long\"], \"respAftlFailed\": [\"long\"], \"writeDone\": [\"long\"], \"respReadMapping\": [\"long\", \"long\"]}";
}

int AmfIndication_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd)
{
    static int runaway = 0;
    int   tmp __attribute__ ((unused));
    int tmpfd __attribute__ ((unused));
    AmfIndicationData tempdata __attribute__ ((unused));
    memset(&tempdata, 0, sizeof(tempdata));
    volatile unsigned int* temp_working_addr = p->transport->mapchannelInd(p, channel);
    switch (channel) {
    case CHAN_NUM_AmfIndication_readDone: {
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.readDone.tag = (uint8_t)(((tmp)&0x7ful));
        ((AmfIndicationCb *)p->cb)->readDone(p, tempdata.readDone.tag);
      } break;
    case CHAN_NUM_AmfIndication_writeDone: {
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.writeDone.tag = (uint8_t)(((tmp)&0x7ful));
        ((AmfIndicationCb *)p->cb)->writeDone(p, tempdata.writeDone.tag);
      } break;
    case CHAN_NUM_AmfIndication_eraseDone: {
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.eraseDone.status = (uint8_t)(((tmp)&0x3ul));
        tempdata.eraseDone.tag = (uint8_t)(((tmp>>2)&0x7ful));
        ((AmfIndicationCb *)p->cb)->eraseDone(p, tempdata.eraseDone.tag, tempdata.eraseDone.status);
      } break;
    case CHAN_NUM_AmfIndication_debugDumpResp: {
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
        ((AmfIndicationCb *)p->cb)->debugDumpResp(p, tempdata.debugDumpResp.debug0, tempdata.debugDumpResp.debug1, tempdata.debugDumpResp.debug2, tempdata.debugDumpResp.debug3, tempdata.debugDumpResp.debug4, tempdata.debugDumpResp.debug5);
      } break;
    case CHAN_NUM_AmfIndication_respAftlFailed: {
        p->transport->recv(p, temp_working_addr, 2, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.respAftlFailed.resp.tag = (uint8_t)(((uint8_t)(((tmp)&0x3ul))<<5));
        tempdata.respAftlFailed.resp.cmd = (AmfCmdTypes)(((tmp>>2)&0x7ul));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.respAftlFailed.resp.lpa = (uint32_t)(((tmp)&0x7fffffful));
        tempdata.respAftlFailed.resp.tag |= (uint8_t)(((tmp>>27)&0x1ful));
        ((AmfIndicationCb *)p->cb)->respAftlFailed(p, tempdata.respAftlFailed.resp);
      } break;
    case CHAN_NUM_AmfIndication_respReadMapping: {
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.respReadMapping.block_num = (uint16_t)(((tmp)&0x3ffful));
        tempdata.respReadMapping.allocated = (uint8_t)(((tmp>>14)&0x1ul));
        ((AmfIndicationCb *)p->cb)->respReadMapping(p, tempdata.respReadMapping.allocated, tempdata.respReadMapping.block_num);
      } break;
    case CHAN_NUM_AmfIndication_respReadBlkInfo: {
        p->transport->recv(p, temp_working_addr, 4, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.respReadBlkInfo.blkinfo_vec[1] = (uint16_t)(((tmp)&0xfffful));
        tempdata.respReadBlkInfo.blkinfo_vec[0] = (uint16_t)(((tmp>>16)&0xfffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.respReadBlkInfo.blkinfo_vec[3] = (uint16_t)(((tmp)&0xfffful));
        tempdata.respReadBlkInfo.blkinfo_vec[2] = (uint16_t)(((tmp>>16)&0xfffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.respReadBlkInfo.blkinfo_vec[5] = (uint16_t)(((tmp)&0xfffful));
        tempdata.respReadBlkInfo.blkinfo_vec[4] = (uint16_t)(((tmp>>16)&0xfffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.respReadBlkInfo.blkinfo_vec[7] = (uint16_t)(((tmp)&0xfffful));
        tempdata.respReadBlkInfo.blkinfo_vec[6] = (uint16_t)(((tmp>>16)&0xfffful));
        ((AmfIndicationCb *)p->cb)->respReadBlkInfo(p, tempdata.respReadBlkInfo.blkinfo_vec);
      } break;
    case CHAN_NUM_AmfIndication_respAftlLoaded: {
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.respAftlLoaded.resp = (uint8_t)(((tmp)&0x1ul));
        ((AmfIndicationCb *)p->cb)->respAftlLoaded(p, tempdata.respAftlLoaded.resp);
      } break;
    default:
        PORTAL_PRINTF("AmfIndication_handleMessage: unknown channel 0x%x\n", channel);
        if (runaway++ > 10) {
            PORTAL_PRINTF("AmfIndication_handleMessage: too many bogus indications, exiting\n");
#ifndef __KERNEL__
            exit(-1);
#endif
        }
        return 0;
    }
    return 0;
}
