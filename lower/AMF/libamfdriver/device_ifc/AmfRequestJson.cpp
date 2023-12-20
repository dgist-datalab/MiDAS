#include "GeneratedTypes.h"
#ifdef PORTAL_JSON
#include "jsoncpp/json/json.h"

int AmfRequestJson_makeReq ( struct PortalInternal *p, const AmfRequestT req )
{
    Json::Value request;
    request.append(Json::Value("makeReq"));
    Json::Value _reqValue;
    _reqValue["__type__"]="AmfRequestT";
    _reqValue["cmd"] = (int)req.cmd;
    _reqValue["tag"] = (Json::UInt64)req.tag;
    _reqValue["lpa"] = (Json::UInt64)req.lpa;
    request.append(_reqValue);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_AmfRequest_makeReq);
    return 0;
};

int AmfRequestJson_debugDumpReq ( struct PortalInternal *p, const uint8_t card )
{
    Json::Value request;
    request.append(Json::Value("debugDumpReq"));
    request.append((Json::UInt64)card);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_AmfRequest_debugDumpReq);
    return 0;
};

int AmfRequestJson_setDmaReadRef ( struct PortalInternal *p, const uint32_t sgId )
{
    Json::Value request;
    request.append(Json::Value("setDmaReadRef"));
    request.append((Json::UInt64)sgId);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_AmfRequest_setDmaReadRef);
    return 0;
};

int AmfRequestJson_setDmaWriteRef ( struct PortalInternal *p, const uint32_t sgId )
{
    Json::Value request;
    request.append(Json::Value("setDmaWriteRef"));
    request.append((Json::UInt64)sgId);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_AmfRequest_setDmaWriteRef);
    return 0;
};

int AmfRequestJson_askAftlLoaded ( struct PortalInternal *p )
{
    Json::Value request;
    request.append(Json::Value("askAftlLoaded"));
    

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_AmfRequest_askAftlLoaded);
    return 0;
};

int AmfRequestJson_setAftlLoaded ( struct PortalInternal *p )
{
    Json::Value request;
    request.append(Json::Value("setAftlLoaded"));
    

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_AmfRequest_setAftlLoaded);
    return 0;
};

int AmfRequestJson_updateMapping ( struct PortalInternal *p, const uint32_t seg_virtblk, const uint8_t allocated, const uint16_t mapped_block )
{
    Json::Value request;
    request.append(Json::Value("updateMapping"));
    request.append((Json::UInt64)seg_virtblk);
    request.append((Json::UInt64)allocated);
    request.append((Json::UInt64)mapped_block);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_AmfRequest_updateMapping);
    return 0;
};

int AmfRequestJson_readMapping ( struct PortalInternal *p, const uint32_t seg_virtblk )
{
    Json::Value request;
    request.append(Json::Value("readMapping"));
    request.append((Json::UInt64)seg_virtblk);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_AmfRequest_readMapping);
    return 0;
};

int AmfRequestJson_updateBlkInfo ( struct PortalInternal *p, const uint16_t phyaddr_upper, const bsvvector_Luint16_t_L8 blkinfo_vec )
{
    Json::Value request;
    request.append(Json::Value("updateBlkInfo"));
    request.append((Json::UInt64)phyaddr_upper);
    Json::Value blkinfo_vecVector;
    blkinfo_vecVector.append((Json::UInt64)blkinfo_vec[0]);
    blkinfo_vecVector.append((Json::UInt64)blkinfo_vec[1]);
    blkinfo_vecVector.append((Json::UInt64)blkinfo_vec[2]);
    blkinfo_vecVector.append((Json::UInt64)blkinfo_vec[3]);
    blkinfo_vecVector.append((Json::UInt64)blkinfo_vec[4]);
    blkinfo_vecVector.append((Json::UInt64)blkinfo_vec[5]);
    blkinfo_vecVector.append((Json::UInt64)blkinfo_vec[6]);
    blkinfo_vecVector.append((Json::UInt64)blkinfo_vec[7]);
    request.append(blkinfo_vecVector);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_AmfRequest_updateBlkInfo);
    return 0;
};

int AmfRequestJson_readBlkInfo ( struct PortalInternal *p, const uint16_t phyaddr_upper )
{
    Json::Value request;
    request.append(Json::Value("readBlkInfo"));
    request.append((Json::UInt64)phyaddr_upper);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_AmfRequest_readBlkInfo);
    return 0;
};

int AmfRequestJson_eraseRawBlock ( struct PortalInternal *p, const uint8_t card, const uint8_t bus, const uint8_t chip, const uint16_t block, const uint8_t tag )
{
    Json::Value request;
    request.append(Json::Value("eraseRawBlock"));
    request.append((Json::UInt64)card);
    request.append((Json::UInt64)bus);
    request.append((Json::UInt64)chip);
    request.append((Json::UInt64)block);
    request.append((Json::UInt64)tag);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_AmfRequest_eraseRawBlock);
    return 0;
};

AmfRequestCb AmfRequestJsonProxyReq = {
    portal_disconnect,
    AmfRequestJson_makeReq,
    AmfRequestJson_debugDumpReq,
    AmfRequestJson_setDmaReadRef,
    AmfRequestJson_setDmaWriteRef,
    AmfRequestJson_askAftlLoaded,
    AmfRequestJson_setAftlLoaded,
    AmfRequestJson_updateMapping,
    AmfRequestJson_readMapping,
    AmfRequestJson_updateBlkInfo,
    AmfRequestJson_readBlkInfo,
    AmfRequestJson_eraseRawBlock,
};
AmfRequestCb *pAmfRequestJsonProxyReq = &AmfRequestJsonProxyReq;
const char * AmfRequestJson_methodSignatures()
{
    return "{\"updateMapping\": [\"long\", \"long\", \"long\"], \"setAftlLoaded\": [], \"askAftlLoaded\": [], \"setDmaWriteRef\": [\"long\"], \"readMapping\": [\"long\"], \"updateBlkInfo\": [\"long\", \"long\"], \"makeReq\": [\"long\"], \"readBlkInfo\": [\"long\"], \"debugDumpReq\": [\"long\"], \"setDmaReadRef\": [\"long\"], \"eraseRawBlock\": [\"long\", \"long\", \"long\", \"long\", \"long\"]}";
}

int AmfRequestJson_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd)
{
    static int runaway = 0;
    int   tmp __attribute__ ((unused));
    int tmpfd __attribute__ ((unused));
    AmfRequestData tempdata __attribute__ ((unused));
    memset(&tempdata, 0, sizeof(tempdata));
    Json::Value msg = Json::Value(connectalJsonReceive(p));
    switch (channel) {
    case CHAN_NUM_AmfRequest_makeReq: {
        ((AmfRequestCb *)p->cb)->makeReq(p, tempdata.makeReq.req);
      } break;
    case CHAN_NUM_AmfRequest_debugDumpReq: {
        ((AmfRequestCb *)p->cb)->debugDumpReq(p, tempdata.debugDumpReq.card);
      } break;
    case CHAN_NUM_AmfRequest_setDmaReadRef: {
        ((AmfRequestCb *)p->cb)->setDmaReadRef(p, tempdata.setDmaReadRef.sgId);
      } break;
    case CHAN_NUM_AmfRequest_setDmaWriteRef: {
        ((AmfRequestCb *)p->cb)->setDmaWriteRef(p, tempdata.setDmaWriteRef.sgId);
      } break;
    case CHAN_NUM_AmfRequest_askAftlLoaded: {
        ((AmfRequestCb *)p->cb)->askAftlLoaded(p);
      } break;
    case CHAN_NUM_AmfRequest_setAftlLoaded: {
        ((AmfRequestCb *)p->cb)->setAftlLoaded(p);
      } break;
    case CHAN_NUM_AmfRequest_updateMapping: {
        ((AmfRequestCb *)p->cb)->updateMapping(p, tempdata.updateMapping.seg_virtblk, tempdata.updateMapping.allocated, tempdata.updateMapping.mapped_block);
      } break;
    case CHAN_NUM_AmfRequest_readMapping: {
        ((AmfRequestCb *)p->cb)->readMapping(p, tempdata.readMapping.seg_virtblk);
      } break;
    case CHAN_NUM_AmfRequest_updateBlkInfo: {
        ((AmfRequestCb *)p->cb)->updateBlkInfo(p, tempdata.updateBlkInfo.phyaddr_upper, tempdata.updateBlkInfo.blkinfo_vec);
      } break;
    case CHAN_NUM_AmfRequest_readBlkInfo: {
        ((AmfRequestCb *)p->cb)->readBlkInfo(p, tempdata.readBlkInfo.phyaddr_upper);
      } break;
    case CHAN_NUM_AmfRequest_eraseRawBlock: {
        ((AmfRequestCb *)p->cb)->eraseRawBlock(p, tempdata.eraseRawBlock.card, tempdata.eraseRawBlock.bus, tempdata.eraseRawBlock.chip, tempdata.eraseRawBlock.block, tempdata.eraseRawBlock.tag);
      } break;
    default:
        PORTAL_PRINTF("AmfRequestJson_handleMessage: unknown channel 0x%x\n", channel);
        if (runaway++ > 10) {
            PORTAL_PRINTF("AmfRequestJson_handleMessage: too many bogus indications, exiting\n");
#ifndef __KERNEL__
            exit(-1);
#endif
        }
        return 0;
    }
    return 0;
}
#endif /* PORTAL_JSON */
