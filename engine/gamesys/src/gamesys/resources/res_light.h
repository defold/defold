#ifndef DM_GAMESYS_RES_LIGHT_H
#define DM_GAMESYS_RES_LIGHT_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::Result ResLightCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResLightDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResLightRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResLightGetInfo(dmResource::ResourceGetInfoParams& params);
}

#endif // DM_GAMESYS_RES_LIGHT_H
