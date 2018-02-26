#ifndef DMGAMESYSTEM_RES_RENDERSCRIPT_H
#define DMGAMESYSTEM_RES_RENDERSCRIPT_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::Result ResRenderScriptCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResRenderScriptDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResRenderScriptRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResRenderScriptGetInfo(dmResource::ResourceGetInfoParams& params);

}

#endif // DMGAMESYSTEM_RES_RENDERSCRIPT_H
