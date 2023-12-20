#include "GeneratedTypes.h"
#ifdef PORTAL_JSON
#include "jsoncpp/json/json.h"

int AmfIndicationJson_readDone ( struct PortalInternal *p, const uint8_t tag )
{
    Json::Value request;
    request.append(Json::Value("readDone"));
    request.append((Json::UInt64)tag);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_AmfIndication_readDone);
    return 0;
};

int AmfIndicationJson_writeDone ( struct PortalInternal *p, const uint8_t tag )
{
    Json::Value request;
    request.append(Json::Value("writeDone"));
    request.append((Json::UInt64)tag);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_AmfIndication_writeDone);
    return 0;
};

int AmfIndicationJson_eraseDone ( struct PortalInternal *p, const uint8_t tag, const uint8_t status )
{
    Json::Value request;
    request.append(Json::Value("eraseDone"));
    request.append((Json::UInt64)tag);
    request.append((Json::UInt64)status);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_AmfIndication_eraseDone);
    return 0;
};

int AmfIndicationJson_debugDumpResp ( struct PortalInternal *p, const uint32_t debug0, const uint32_t debug1, const uint32_t debug2, const uint32_t debug3, const uint32_t debug4, const uint32_t debug5 )
{
    Json::Value request;
    request.append(Json::Value("debugDumpResp"));
    request.append((Json::UInt64)debug0);
    request.append((Json::UInt64)debug1);
    request.append((Json::UInt64)debug2);
    request.append((Json::UInt64)debug3);
    request.append((Json::UInt64)debug4);
    request.append((Json::UInt64)debug5);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_AmfIndication_debugDumpResp);
    return 0;
};

int AmfIndicationJson_respAftlFailed ( struct PortalInternal *p, const AmfRequestT resp )
{
    Json::Value request;
    request.append(Json::Value("respAftlFailed"));
    Json::Value _respValue;
    _respValue["__type__"]="AmfRequestT";
    _respValue["cmd"] = (int)resp.cmd;
    _respValue["tag"] = (Json::UInt64)resp.tag;
    _respValue["lpa"] = (Json::UInt64)resp.lpa;
    request.append(_respValue);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_AmfIndication_respAftlFailed);
    return 0;
};

int AmfIndicationJson_respReadMapping ( struct PortalInternal *p, const uint8_t allocated, const uint16_t block_num )
{
    Json::Value request;
    request.append(Json::Value("respReadMapping"));
    request.append((Json::UInt64)allocated);
    request.append((Json::UInt64)block_num);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_AmfIndication_respReadMapping);
    return 0;
};

int AmfIndicationJson_respReadBlkInfo ( struct PortalInternal *p, const bsvvector_Luint16_t_L8 blkinfo_vec )
{
    Json::Value request;
    request.append(Json::Value("respReadBlkInfo"));
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
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_AmfIndication_respReadBlkInfo);
    return 0;
};

int AmfIndicationJson_respAftlLoaded ( struct PortalInternal *p, const uint8_t resp )
{
    Json::Value request;
    request.append(Json::Value("respAftlLoaded"));
    request.append((Json::UInt64)resp);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_AmfIndication_respAftlLoaded);
    return 0;
};

AmfIndicationCb AmfIndicationJsonProxyReq = {
    portal_disconnect,
    AmfIndicationJson_readDone,
    AmfIndicationJson_writeDone,
    AmfIndicationJson_eraseDone,
    AmfIndicationJson_debugDumpResp,
    AmfIndicationJson_respAftlFailed,
    AmfIndicationJson_respReadMapping,
    AmfIndicationJson_respReadBlkInfo,
    AmfIndicationJson_respAftlLoaded,
};
AmfIndicationCb *pAmfIndicationJsonProxyReq = &AmfIndicationJsonProxyReq;
const char * AmfIndicationJson_methodSignatures()
{
    return "{\"respReadBlkInfo\": [\"long\"], \"readDone\": [\"long\"], \"respAftlLoaded\": [\"long\"], \"eraseDone\": [\"long\", \"long\"], \"debugDumpResp\": [\"long\", \"long\", \"long\", \"long\", \"long\", \"long\"], \"respAftlFailed\": [\"long\"], \"writeDone\": [\"long\"], \"respReadMapping\": [\"long\", \"long\"]}";
}

int AmfIndicationJson_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd)
{
    static int runaway = 0;
    int   tmp __attribute__ ((unused));
    int tmpfd __attribute__ ((unused));
    AmfIndicationData tempdata __attribute__ ((unused));
    memset(&tempdata, 0, sizeof(tempdata));
    Json::Value msg = Json::Value(connectalJsonReceive(p));
    switch (channel) {
    case CHAN_NUM_AmfIndication_readDone: {
        ((AmfIndicationCb *)p->cb)->readDone(p, tempdata.readDone.tag);
      } break;
    case CHAN_NUM_AmfIndication_writeDone: {
        ((AmfIndicationCb *)p->cb)->writeDone(p, tempdata.writeDone.tag);
      } break;
    case CHAN_NUM_AmfIndication_eraseDone: {
        ((AmfIndicationCb *)p->cb)->eraseDone(p, tempdata.eraseDone.tag, tempdata.eraseDone.status);
      } break;
    case CHAN_NUM_AmfIndication_debugDumpResp: {
        ((AmfIndicationCb *)p->cb)->debugDumpResp(p, tempdata.debugDumpResp.debug0, tempdata.debugDumpResp.debug1, tempdata.debugDumpResp.debug2, tempdata.debugDumpResp.debug3, tempdata.debugDumpResp.debug4, tempdata.debugDumpResp.debug5);
      } break;
    case CHAN_NUM_AmfIndication_respAftlFailed: {
        ((AmfIndicationCb *)p->cb)->respAftlFailed(p, tempdata.respAftlFailed.resp);
      } break;
    case CHAN_NUM_AmfIndication_respReadMapping: {
        ((AmfIndicationCb *)p->cb)->respReadMapping(p, tempdata.respReadMapping.allocated, tempdata.respReadMapping.block_num);
      } break;
    case CHAN_NUM_AmfIndication_respReadBlkInfo: {
        ((AmfIndicationCb *)p->cb)->respReadBlkInfo(p, tempdata.respReadBlkInfo.blkinfo_vec);
      } break;
    case CHAN_NUM_AmfIndication_respAftlLoaded: {
        ((AmfIndicationCb *)p->cb)->respAftlLoaded(p, tempdata.respAftlLoaded.resp);
      } break;
    default:
        PORTAL_PRINTF("AmfIndicationJson_handleMessage: unknown channel 0x%x\n", channel);
        if (runaway++ > 10) {
            PORTAL_PRINTF("AmfIndicationJson_handleMessage: too many bogus indications, exiting\n");
#ifndef __KERNEL__
            exit(-1);
#endif
        }
        return 0;
    }
    return 0;
}
#endif /* PORTAL_JSON */
