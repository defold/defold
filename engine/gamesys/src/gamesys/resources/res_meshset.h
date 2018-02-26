#ifndef DM_GAMESYS_RES_MESHSET_H
#define DM_GAMESYS_RES_MESHSET_H

#include <stdint.h>

#include <resource/resource.h>
#include <rig/rig_ddf.h>

namespace dmGameSystem
{
    struct MeshSetResource
    {
        dmRigDDF::MeshSet* m_MeshSet;
        uint32_t           m_DDFSize;
   };

    dmResource::Result ResMeshSetPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResMeshSetCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResMeshSetDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResMeshSetRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResMeshSetGetInfo(dmResource::ResourceGetInfoParams& params);
}

#endif // DM_GAMESYS_RES_MESHSET_H
