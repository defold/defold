#ifndef DM_GAMESYS_RES_SKELETON_H
#define DM_GAMESYS_RES_SKELETON_H

#include <stdint.h>

#include <resource/resource.h>
#include <rig/rig_ddf.h>

namespace dmGameSystem
{
    struct SkeletonResource
    {
        dmRigDDF::Skeleton* m_Skeleton;
    };

    dmResource::Result ResSkeletonPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResSkeletonCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResSkeletonDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResSkeletonRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMESYS_RES_SKELETON_H
