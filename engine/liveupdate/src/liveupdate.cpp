#include "liveupdate.h"
#include "liveupdate_private.h"
#include <extension/extension.h>

namespace dmLiveUpdate
{


};

static dmExtension::Result AppInitializeLiveUpdate(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeLiveUpdate(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeLiveUpdate(dmExtension::Params* params)
{
    dmLiveUpdate::LuaInit(params->m_L);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeLiveUpdate(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(LiveUpdateExt, "LiveUpdate", AppInitializeLiveUpdate, AppFinalizeLiveUpdate, InitializeLiveUpdate, 0, 0, FinalizeLiveUpdate)