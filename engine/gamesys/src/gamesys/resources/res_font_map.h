#ifndef DM_GAMESYS_RES_FONT_MAP_H
#define DM_GAMESYS_RES_FONT_MAP_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::Result ResFontMapPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResFontMapCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResFontMapDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResFontMapRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResFontMapGetInfo(dmResource::ResourceGetInfoParams& params);
}

#endif // DM_GAMESYS_RES_FONT_MAP_H
