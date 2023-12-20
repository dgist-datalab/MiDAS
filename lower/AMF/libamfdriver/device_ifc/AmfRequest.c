#include "GeneratedTypes.h"

int AmfRequest_makeReq ( struct PortalInternal *p, const AmfRequestT req )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_AmfRequest_makeReq, 3);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_AmfRequest_makeReq, "AmfRequest_makeReq")) return 1;
    p->transport->write(p, &temp_working_addr, (req.tag>>5)|(((unsigned long)req.cmd)<<2));
    p->transport->write(p, &temp_working_addr, req.lpa|(((unsigned long)req.tag)<<27));
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_AmfRequest_makeReq << 16) | 3, -1);
    return 0;
};

int AmfRequest_debugDumpReq ( struct PortalInternal *p, const uint8_t card )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_AmfRequest_debugDumpReq, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_AmfRequest_debugDumpReq, "AmfRequest_debugDumpReq")) return 1;
    p->transport->write(p, &temp_working_addr, card);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_AmfRequest_debugDumpReq << 16) | 2, -1);
    return 0;
};

int AmfRequest_setDmaReadRef ( struct PortalInternal *p, const uint32_t sgId )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_AmfRequest_setDmaReadRef, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_AmfRequest_setDmaReadRef, "AmfRequest_setDmaReadRef")) return 1;
    p->transport->write(p, &temp_working_addr, sgId);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_AmfRequest_setDmaReadRef << 16) | 2, -1);
    return 0;
};

int AmfRequest_setDmaWriteRef ( struct PortalInternal *p, const uint32_t sgId )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_AmfRequest_setDmaWriteRef, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_AmfRequest_setDmaWriteRef, "AmfRequest_setDmaWriteRef")) return 1;
    p->transport->write(p, &temp_working_addr, sgId);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_AmfRequest_setDmaWriteRef << 16) | 2, -1);
    return 0;
};

int AmfRequest_askAftlLoaded ( struct PortalInternal *p )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_AmfRequest_askAftlLoaded, 1);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_AmfRequest_askAftlLoaded, "AmfRequest_askAftlLoaded")) return 1;
    p->transport->write(p, &temp_working_addr, 0);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_AmfRequest_askAftlLoaded << 16) | 1, -1);
    return 0;
};

int AmfRequest_setAftlLoaded ( struct PortalInternal *p )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_AmfRequest_setAftlLoaded, 1);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_AmfRequest_setAftlLoaded, "AmfRequest_setAftlLoaded")) return 1;
    p->transport->write(p, &temp_working_addr, 0);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_AmfRequest_setAftlLoaded << 16) | 1, -1);
    return 0;
};

int AmfRequest_updateMapping ( struct PortalInternal *p, const uint32_t seg_virtblk, const uint8_t allocated, const uint16_t mapped_block )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_AmfRequest_updateMapping, 3);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_AmfRequest_updateMapping, "AmfRequest_updateMapping")) return 1;
    p->transport->write(p, &temp_working_addr, (seg_virtblk>>17));
    p->transport->write(p, &temp_working_addr, mapped_block|(((unsigned long)allocated)<<14)|(((unsigned long)seg_virtblk)<<15));
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_AmfRequest_updateMapping << 16) | 3, -1);
    return 0;
};

int AmfRequest_readMapping ( struct PortalInternal *p, const uint32_t seg_virtblk )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_AmfRequest_readMapping, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_AmfRequest_readMapping, "AmfRequest_readMapping")) return 1;
    p->transport->write(p, &temp_working_addr, seg_virtblk);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_AmfRequest_readMapping << 16) | 2, -1);
    return 0;
};

int AmfRequest_updateBlkInfo ( struct PortalInternal *p, const uint16_t phyaddr_upper, const bsvvector_Luint16_t_L8 blkinfo_vec )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_AmfRequest_updateBlkInfo, 6);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_AmfRequest_updateBlkInfo, "AmfRequest_updateBlkInfo")) return 1;
    p->transport->write(p, &temp_working_addr, phyaddr_upper);
    p->transport->write(p, &temp_working_addr, blkinfo_vec[1]|(((unsigned long)blkinfo_vec[0])<<16));
    p->transport->write(p, &temp_working_addr, blkinfo_vec[3]|(((unsigned long)blkinfo_vec[2])<<16));
    p->transport->write(p, &temp_working_addr, blkinfo_vec[5]|(((unsigned long)blkinfo_vec[4])<<16));
    p->transport->write(p, &temp_working_addr, blkinfo_vec[7]|(((unsigned long)blkinfo_vec[6])<<16));
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_AmfRequest_updateBlkInfo << 16) | 6, -1);
    return 0;
};

int AmfRequest_readBlkInfo ( struct PortalInternal *p, const uint16_t phyaddr_upper )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_AmfRequest_readBlkInfo, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_AmfRequest_readBlkInfo, "AmfRequest_readBlkInfo")) return 1;
    p->transport->write(p, &temp_working_addr, phyaddr_upper);
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_AmfRequest_readBlkInfo << 16) | 2, -1);
    return 0;
};

int AmfRequest_eraseRawBlock ( struct PortalInternal *p, const uint8_t card, const uint8_t bus, const uint8_t chip, const uint16_t block, const uint8_t tag )
{
    volatile unsigned int* temp_working_addr_start = p->transport->mapchannelReq(p, CHAN_NUM_AmfRequest_eraseRawBlock, 2);
    volatile unsigned int* temp_working_addr = temp_working_addr_start;
    if (p->transport->busywait(p, CHAN_NUM_AmfRequest_eraseRawBlock, "AmfRequest_eraseRawBlock")) return 1;
    p->transport->write(p, &temp_working_addr, tag|(((unsigned long)block)<<7)|(((unsigned long)chip)<<19)|(((unsigned long)bus)<<22)|(((unsigned long)card)<<25));
    p->transport->send(p, temp_working_addr_start, (CHAN_NUM_AmfRequest_eraseRawBlock << 16) | 2, -1);
    return 0;
};

AmfRequestCb AmfRequestProxyReq = {
    portal_disconnect,
    AmfRequest_makeReq,
    AmfRequest_debugDumpReq,
    AmfRequest_setDmaReadRef,
    AmfRequest_setDmaWriteRef,
    AmfRequest_askAftlLoaded,
    AmfRequest_setAftlLoaded,
    AmfRequest_updateMapping,
    AmfRequest_readMapping,
    AmfRequest_updateBlkInfo,
    AmfRequest_readBlkInfo,
    AmfRequest_eraseRawBlock,
};
AmfRequestCb *pAmfRequestProxyReq = &AmfRequestProxyReq;

const uint32_t AmfRequest_reqinfo = 0xb0018;
const char * AmfRequest_methodSignatures()
{
    return "{\"updateMapping\": [\"long\", \"long\", \"long\"], \"setAftlLoaded\": [], \"askAftlLoaded\": [], \"setDmaWriteRef\": [\"long\"], \"readMapping\": [\"long\"], \"updateBlkInfo\": [\"long\", \"long\"], \"makeReq\": [\"long\"], \"readBlkInfo\": [\"long\"], \"debugDumpReq\": [\"long\"], \"setDmaReadRef\": [\"long\"], \"eraseRawBlock\": [\"long\", \"long\", \"long\", \"long\", \"long\"]}";
}

int AmfRequest_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd)
{
    static int runaway = 0;
    int   tmp __attribute__ ((unused));
    int tmpfd __attribute__ ((unused));
    AmfRequestData tempdata __attribute__ ((unused));
    memset(&tempdata, 0, sizeof(tempdata));
    volatile unsigned int* temp_working_addr = p->transport->mapchannelInd(p, channel);
    switch (channel) {
    case CHAN_NUM_AmfRequest_makeReq: {
        p->transport->recv(p, temp_working_addr, 2, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.makeReq.req.tag = (uint8_t)(((uint8_t)(((tmp)&0x3ul))<<5));
        tempdata.makeReq.req.cmd = (AmfCmdTypes)(((tmp>>2)&0x7ul));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.makeReq.req.lpa = (uint32_t)(((tmp)&0x7fffffful));
        tempdata.makeReq.req.tag |= (uint8_t)(((tmp>>27)&0x1ful));
        ((AmfRequestCb *)p->cb)->makeReq(p, tempdata.makeReq.req);
      } break;
    case CHAN_NUM_AmfRequest_debugDumpReq: {
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.debugDumpReq.card = (uint8_t)(((tmp)&0xfful));
        ((AmfRequestCb *)p->cb)->debugDumpReq(p, tempdata.debugDumpReq.card);
      } break;
    case CHAN_NUM_AmfRequest_setDmaReadRef: {
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.setDmaReadRef.sgId = (uint32_t)(((tmp)&0xfffffffful));
        ((AmfRequestCb *)p->cb)->setDmaReadRef(p, tempdata.setDmaReadRef.sgId);
      } break;
    case CHAN_NUM_AmfRequest_setDmaWriteRef: {
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.setDmaWriteRef.sgId = (uint32_t)(((tmp)&0xfffffffful));
        ((AmfRequestCb *)p->cb)->setDmaWriteRef(p, tempdata.setDmaWriteRef.sgId);
      } break;
    case CHAN_NUM_AmfRequest_askAftlLoaded: {
        p->transport->recv(p, temp_working_addr, 0, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        ((AmfRequestCb *)p->cb)->askAftlLoaded(p);
      } break;
    case CHAN_NUM_AmfRequest_setAftlLoaded: {
        p->transport->recv(p, temp_working_addr, 0, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        ((AmfRequestCb *)p->cb)->setAftlLoaded(p);
      } break;
    case CHAN_NUM_AmfRequest_updateMapping: {
        p->transport->recv(p, temp_working_addr, 2, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.updateMapping.seg_virtblk = (uint32_t)(((uint32_t)(((tmp)&0x3ul))<<17));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.updateMapping.mapped_block = (uint16_t)(((tmp)&0x3ffful));
        tempdata.updateMapping.allocated = (uint8_t)(((tmp>>14)&0x1ul));
        tempdata.updateMapping.seg_virtblk |= (uint32_t)(((tmp>>15)&0x1fffful));
        ((AmfRequestCb *)p->cb)->updateMapping(p, tempdata.updateMapping.seg_virtblk, tempdata.updateMapping.allocated, tempdata.updateMapping.mapped_block);
      } break;
    case CHAN_NUM_AmfRequest_readMapping: {
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.readMapping.seg_virtblk = (uint32_t)(((tmp)&0x7fffful));
        ((AmfRequestCb *)p->cb)->readMapping(p, tempdata.readMapping.seg_virtblk);
      } break;
    case CHAN_NUM_AmfRequest_updateBlkInfo: {
        p->transport->recv(p, temp_working_addr, 5, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.updateBlkInfo.phyaddr_upper = (uint16_t)(((tmp)&0xfffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.updateBlkInfo.blkinfo_vec[1] = (uint16_t)(((tmp)&0xfffful));
        tempdata.updateBlkInfo.blkinfo_vec[0] = (uint16_t)(((tmp>>16)&0xfffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.updateBlkInfo.blkinfo_vec[3] = (uint16_t)(((tmp)&0xfffful));
        tempdata.updateBlkInfo.blkinfo_vec[2] = (uint16_t)(((tmp>>16)&0xfffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.updateBlkInfo.blkinfo_vec[5] = (uint16_t)(((tmp)&0xfffful));
        tempdata.updateBlkInfo.blkinfo_vec[4] = (uint16_t)(((tmp>>16)&0xfffful));
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.updateBlkInfo.blkinfo_vec[7] = (uint16_t)(((tmp)&0xfffful));
        tempdata.updateBlkInfo.blkinfo_vec[6] = (uint16_t)(((tmp>>16)&0xfffful));
        ((AmfRequestCb *)p->cb)->updateBlkInfo(p, tempdata.updateBlkInfo.phyaddr_upper, tempdata.updateBlkInfo.blkinfo_vec);
      } break;
    case CHAN_NUM_AmfRequest_readBlkInfo: {
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.readBlkInfo.phyaddr_upper = (uint16_t)(((tmp)&0xfffful));
        ((AmfRequestCb *)p->cb)->readBlkInfo(p, tempdata.readBlkInfo.phyaddr_upper);
      } break;
    case CHAN_NUM_AmfRequest_eraseRawBlock: {
        p->transport->recv(p, temp_working_addr, 1, &tmpfd);
        tmp = p->transport->read(p, &temp_working_addr);
        tempdata.eraseRawBlock.tag = (uint8_t)(((tmp)&0x7ful));
        tempdata.eraseRawBlock.block = (uint16_t)(((tmp>>7)&0xffful));
        tempdata.eraseRawBlock.chip = (uint8_t)(((tmp>>19)&0x7ul));
        tempdata.eraseRawBlock.bus = (uint8_t)(((tmp>>22)&0x7ul));
        tempdata.eraseRawBlock.card = (uint8_t)(((tmp>>25)&0x1ul));
        ((AmfRequestCb *)p->cb)->eraseRawBlock(p, tempdata.eraseRawBlock.card, tempdata.eraseRawBlock.bus, tempdata.eraseRawBlock.chip, tempdata.eraseRawBlock.block, tempdata.eraseRawBlock.tag);
      } break;
    default:
        PORTAL_PRINTF("AmfRequest_handleMessage: unknown channel 0x%x\n", channel);
        if (runaway++ > 10) {
            PORTAL_PRINTF("AmfRequest_handleMessage: too many bogus indications, exiting\n");
#ifndef __KERNEL__
            exit(-1);
#endif
        }
        return 0;
    }
    return 0;
}
