#ifndef DMGAMESYSTEM_RES_RENDER_PROTOTYPE_H
#define DMGAMESYSTEM_RES_RENDER_PROTOTYPE_H

#include <stdint.h>

#include <resource/resource.h>

#include <render/render.h>

namespace dmGameSystem
{
    dmResource::Result ResRenderPrototypeCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResRenderPrototypeDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResRenderPrototypeRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResRenderPrototypeGetInfo(dmResource::ResourceGetInfoParams& params);

}

#endif // DMGAMESYSTEM_RES_RENDER_PROTOTYPE_H
