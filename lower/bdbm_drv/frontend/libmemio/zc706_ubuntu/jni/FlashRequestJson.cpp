#include "GeneratedTypes.h"
#ifdef PORTAL_JSON
#include "jsoncpp/json/json.h"

int FlashRequestJson_readPage ( struct PortalInternal *p, const uint32_t tag, const uint32_t lpa, const uint32_t offset )
{
    Json::Value request;
    request.append(Json::Value("readPage"));
    request.append((Json::UInt64)tag);
    request.append((Json::UInt64)lpa);
    request.append((Json::UInt64)offset);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_FlashRequest_readPage);
    return 0;
};

int FlashRequestJson_writePage ( struct PortalInternal *p, const uint32_t tag, const uint32_t lpa, const uint32_t offset )
{
    Json::Value request;
    request.append(Json::Value("writePage"));
    request.append((Json::UInt64)tag);
    request.append((Json::UInt64)lpa);
    request.append((Json::UInt64)offset);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_FlashRequest_writePage);
    return 0;
};

int FlashRequestJson_eraseBlock ( struct PortalInternal *p, const uint32_t tag, const uint32_t lpa )
{
    Json::Value request;
    request.append(Json::Value("eraseBlock"));
    request.append((Json::UInt64)tag);
    request.append((Json::UInt64)lpa);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_FlashRequest_eraseBlock);
    return 0;
};

int FlashRequestJson_setDmaReadRef ( struct PortalInternal *p, const uint32_t sgId )
{
    Json::Value request;
    request.append(Json::Value("setDmaReadRef"));
    request.append((Json::UInt64)sgId);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_FlashRequest_setDmaReadRef);
    return 0;
};

int FlashRequestJson_setDmaWriteRef ( struct PortalInternal *p, const uint32_t sgId )
{
    Json::Value request;
    request.append(Json::Value("setDmaWriteRef"));
    request.append((Json::UInt64)sgId);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_FlashRequest_setDmaWriteRef);
    return 0;
};

int FlashRequestJson_setDmaMapRef ( struct PortalInternal *p, const uint32_t sgId )
{
    Json::Value request;
    request.append(Json::Value("setDmaMapRef"));
    request.append((Json::UInt64)sgId);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_FlashRequest_setDmaMapRef);
    return 0;
};

int FlashRequestJson_downloadMap ( struct PortalInternal *p )
{
    Json::Value request;
    request.append(Json::Value("downloadMap"));
    

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_FlashRequest_downloadMap);
    return 0;
};

int FlashRequestJson_uploadMap ( struct PortalInternal *p )
{
    Json::Value request;
    request.append(Json::Value("uploadMap"));
    

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_FlashRequest_uploadMap);
    return 0;
};

int FlashRequestJson_start ( struct PortalInternal *p, const uint32_t dummy )
{
    Json::Value request;
    request.append(Json::Value("start"));
    request.append((Json::UInt64)dummy);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_FlashRequest_start);
    return 0;
};

int FlashRequestJson_debugDumpReq ( struct PortalInternal *p, const uint32_t dummy )
{
    Json::Value request;
    request.append(Json::Value("debugDumpReq"));
    request.append((Json::UInt64)dummy);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_FlashRequest_debugDumpReq);
    return 0;
};

int FlashRequestJson_setDebugVals ( struct PortalInternal *p, const uint32_t flag, const uint32_t debugDelay )
{
    Json::Value request;
    request.append(Json::Value("setDebugVals"));
    request.append((Json::UInt64)flag);
    request.append((Json::UInt64)debugDelay);

    std::string requestjson = Json::FastWriter().write(request);;
    connectalJsonSend(p, requestjson.c_str(), (int)CHAN_NUM_FlashRequest_setDebugVals);
    return 0;
};

FlashRequestCb FlashRequestJsonProxyReq = {
    portal_disconnect,
    FlashRequestJson_readPage,
    FlashRequestJson_writePage,
    FlashRequestJson_eraseBlock,
    FlashRequestJson_setDmaReadRef,
    FlashRequestJson_setDmaWriteRef,
    FlashRequestJson_setDmaMapRef,
    FlashRequestJson_downloadMap,
    FlashRequestJson_uploadMap,
    FlashRequestJson_start,
    FlashRequestJson_debugDumpReq,
    FlashRequestJson_setDebugVals,
};
FlashRequestCb *pFlashRequestJsonProxyReq = &FlashRequestJsonProxyReq;
const char * FlashRequestJson_methodSignatures()
{
    return "{\"setDebugVals\": [\"long\", \"long\"], \"setDmaMapRef\": [\"long\"], \"writePage\": [\"long\", \"long\", \"long\"], \"eraseBlock\": [\"long\", \"long\"], \"debugDumpReq\": [\"long\"], \"setDmaWriteRef\": [\"long\"], \"start\": [\"long\"], \"uploadMap\": [], \"setDmaReadRef\": [\"long\"], \"downloadMap\": [], \"readPage\": [\"long\", \"long\", \"long\"]}";
}

int FlashRequestJson_handleMessage(struct PortalInternal *p, unsigned int channel, int messageFd)
{
    static int runaway = 0;
    int   tmp __attribute__ ((unused));
    int tmpfd __attribute__ ((unused));
    FlashRequestData tempdata __attribute__ ((unused));
    memset(&tempdata, 0, sizeof(tempdata));
    Json::Value msg = Json::Value(connectalJsonReceive(p));
    switch (channel) {
    case CHAN_NUM_FlashRequest_readPage: {
        ((FlashRequestCb *)p->cb)->readPage(p, tempdata.readPage.tag, tempdata.readPage.lpa, tempdata.readPage.offset);
      } break;
    case CHAN_NUM_FlashRequest_writePage: {
        ((FlashRequestCb *)p->cb)->writePage(p, tempdata.writePage.tag, tempdata.writePage.lpa, tempdata.writePage.offset);
      } break;
    case CHAN_NUM_FlashRequest_eraseBlock: {
        ((FlashRequestCb *)p->cb)->eraseBlock(p, tempdata.eraseBlock.tag, tempdata.eraseBlock.lpa);
      } break;
    case CHAN_NUM_FlashRequest_setDmaReadRef: {
        ((FlashRequestCb *)p->cb)->setDmaReadRef(p, tempdata.setDmaReadRef.sgId);
      } break;
    case CHAN_NUM_FlashRequest_setDmaWriteRef: {
        ((FlashRequestCb *)p->cb)->setDmaWriteRef(p, tempdata.setDmaWriteRef.sgId);
      } break;
    case CHAN_NUM_FlashRequest_setDmaMapRef: {
        ((FlashRequestCb *)p->cb)->setDmaMapRef(p, tempdata.setDmaMapRef.sgId);
      } break;
    case CHAN_NUM_FlashRequest_downloadMap: {
        ((FlashRequestCb *)p->cb)->downloadMap(p);
      } break;
    case CHAN_NUM_FlashRequest_uploadMap: {
        ((FlashRequestCb *)p->cb)->uploadMap(p);
      } break;
    case CHAN_NUM_FlashRequest_start: {
        ((FlashRequestCb *)p->cb)->start(p, tempdata.start.dummy);
      } break;
    case CHAN_NUM_FlashRequest_debugDumpReq: {
        ((FlashRequestCb *)p->cb)->debugDumpReq(p, tempdata.debugDumpReq.dummy);
      } break;
    case CHAN_NUM_FlashRequest_setDebugVals: {
        ((FlashRequestCb *)p->cb)->setDebugVals(p, tempdata.setDebugVals.flag, tempdata.setDebugVals.debugDelay);
      } break;
    default:
        PORTAL_PRINTF("FlashRequestJson_handleMessage: unknown channel 0x%x\n", channel);
        if (runaway++ > 10) {
            PORTAL_PRINTF("FlashRequestJson_handleMessage: too many bogus indications, exiting\n");
#ifndef __KERNEL__
            exit(-1);
#endif
        }
        return 0;
    }
    return 0;
}
#endif /* PORTAL_JSON */
