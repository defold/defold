#ifndef DM_GAMESYS_RES_MATERIAL_H
#define DM_GAMESYS_RES_MATERIAL_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::Result ResMaterialCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResMaterialDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResMaterialRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResMaterialGetInfo(dmResource::ResourceGetInfoParams& params);
}

#endif
