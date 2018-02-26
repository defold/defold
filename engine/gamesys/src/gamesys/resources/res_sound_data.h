#ifndef DM_GAMESYS_SOUND_DATA_H
#define DM_GAMESYS_SOUND_DATA_H

#include <resource/resource.h>
#include <physics/physics.h>

namespace dmGameSystem
{
    dmResource::Result ResSoundDataCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResSoundDataDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResSoundDataRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResSoundDataGetInfo(dmResource::ResourceGetInfoParams& params);
}

#endif
