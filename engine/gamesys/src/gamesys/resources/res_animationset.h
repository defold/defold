#ifndef DM_GAMESYS_RES_ANIMATIONSET_H
#define DM_GAMESYS_RES_ANIMATIONSET_H

#include <stdint.h>

#include <resource/resource.h>
#include <rig/rig_ddf.h>

namespace dmGameSystem
{
    struct AnimationSetResource
    {
        dmRigDDF::AnimationSet* m_AnimationSet;
        uint32_t                m_DDFSize;
    };

    dmResource::Result ResAnimationSetPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResAnimationSetCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResAnimationSetDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResAnimationSetRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResAnimationSetGetInfo(dmResource::ResourceGetInfoParams& params);
}

#endif // DM_GAMESYS_RES_ANIMATIONSET_H
