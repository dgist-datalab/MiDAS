#include "GeneratedTypes.h"
#ifdef PORTAL_JSON
#include "jsoncpp/json/json.h"

int FlashIndicationJson_readDone ( struct PortalInternal *p, const uint32_t tag, const uint32_t status )
{
    Json::Value request;
    request.append(Json::Value("readDone"));
    request.append((Json::UInt64)tag);
    request.append((Json::UInt64)status);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_FlashIndication_readDone);
    return 0;
};

int FlashIndicationJson_writeDone ( struct PortalInternal *p, const uint32_t tag, const uint32_t status )
{
    Json::Value request;
    request.append(Json::Value("writeDone"));
    request.append((Json::UInt64)tag);
    request.append((Json::UInt64)status);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_FlashIndication_writeDone);
    return 0;
};

int FlashIndicationJson_eraseDone ( struct PortalInternal *p, const uint32_t tag, const uint32_t status )
{
    Json::Value request;
    request.append(Json::Value("eraseDone"));
    request.append((Json::UInt64)tag);
    request.append((Json::UInt64)status);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_FlashIndication_eraseDone);
    return 0;
};

int FlashIndicationJson_uploadDone ( struct PortalInternal *p )
{
    Json::Value request;
    request.append(Json::Value("uploadDone"));
    

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_FlashIndication_uploadDone);
    return 0;
};

int FlashIndicationJson_downloadDone ( struct PortalInternal *p )
{
    Json::Value request;
    request.append(Json::Value("downloadDone"));
    

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_FlashIndication_downloadDone);
    return 0;
};

int FlashIndicationJson_debugDumpResp ( struct PortalInternal *p, const uint32_t debug0, const uint32_t debug1, const uint32_t debug2, const uint32_t debug3, const uint32_t debug4, const uint32_t debug5 )
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
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_FlashIndication_debugDumpResp);
    return 0;
};

FlashIndicationCb FlashIndicationJsonProxyReq = {
    portal_disconnect,
    FlashIndicationJson_readDone,
    FlashIndicationJson_writeDone,
    FlashIndicationJson_eraseDone,
    FlashIndicationJson_uploadDone,
    FlashIndicationJson_downloadDone,
    FlashIndicationJson_debugDumpResp,
};
FlashIndicationCb *pFlashIndicationJsonProxyReq = &FlashIndicationJsonProxyReq;
const char * FlashIndicationJson_methodSignatures()
{
    return "{\"readDone\": [\"long\", \"long\"], \"uploadDone\": [], \"eraseDone\": [\"long\", \"long\"], \"debugDumpResp\": [\"long\", \"long\", \"long\", \"long\", \"long\", \"long\"], \"downloadDone\": [], \"writeDone\": [\"long\", \"long\"]}";
}

int FlashIndicationJson_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd)
{
    static int runaway = 0;
    int   tmp __attribute__ ((unused));
    int tmpfd __attribute__ ((unused));
    FlashIndicationData tempdata __attribute__ ((unused));
    memset(&tempdata, 0, sizeof(tempdata));
    Json::Value msg = Json::Value(connectalJsonReceive(p));
    switch (channel) {
    case CHAN_NUM_FlashIndication_readDone: {
        ((FlashIndicationCb *)p->cb)->readDone(p, tempdata.readDone.tag, tempdata.readDone.status);
      } break;
    case CHAN_NUM_FlashIndication_writeDone: {
        ((FlashIndicationCb *)p->cb)->writeDone(p, tempdata.writeDone.tag, tempdata.writeDone.status);
      } break;
    case CHAN_NUM_FlashIndication_eraseDone: {
        ((FlashIndicationCb *)p->cb)->eraseDone(p, tempdata.eraseDone.tag, tempdata.eraseDone.status);
      } break;
    case CHAN_NUM_FlashIndication_uploadDone: {
        ((FlashIndicationCb *)p->cb)->uploadDone(p);
      } break;
    case CHAN_NUM_FlashIndication_downloadDone: {
        ((FlashIndicationCb *)p->cb)->downloadDone(p);
      } break;
    case CHAN_NUM_FlashIndication_debugDumpResp: {
        ((FlashIndicationCb *)p->cb)->debugDumpResp(p, tempdata.debugDumpResp.debug0, tempdata.debugDumpResp.debug1, tempdata.debugDumpResp.debug2, tempdata.debugDumpResp.debug3, tempdata.debugDumpResp.debug4, tempdata.debugDumpResp.debug5);
      } break;
    default:
        PORTAL_PRINTF("FlashIndicationJson_handleMessage: unknown channel 0x%x\n", channel);
        if (runaway++ > 10) {
            PORTAL_PRINTF("FlashIndicationJson_handleMessage: too many bogus indications, exiting\n");
#ifndef __KERNEL__
            exit(-1);
#endif
        }
        return 0;
    }
    return 0;
}
#endif /* PORTAL_JSON */
