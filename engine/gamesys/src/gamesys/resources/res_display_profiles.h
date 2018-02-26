#ifndef DM_GAMESYS_RES_DISPLAY_PROFILES_H
#define DM_GAMESYS_RES_DISPLAY_PROFILES_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::Result ResDisplayProfilesCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResDisplayProfilesDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResDisplayProfilesRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResDisplayProfilesGetInfo(dmResource::ResourceGetInfoParams& params);
}

#endif
