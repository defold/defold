#ifndef DM_GAMESYS_RES_ANIMATIONSET_H
#define DM_GAMESYS_RES_ANIMATIONSET_H

#include <stdint.h>

#include <resource/resource.h>
#include "rig_ddf.h"

namespace dmGameSystem
{
    struct AnimationSetResource
    {
        dmGameSystemDDF::AnimationSet* m_AnimationSet;
    };

    dmResource::Result ResAnimationSetPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResAnimationSetCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResAnimationSetDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResAnimationSetRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMESYS_RES_ANIMATIONSET_H
