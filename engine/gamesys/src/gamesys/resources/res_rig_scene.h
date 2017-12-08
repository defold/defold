#ifndef DM_GAMESYS_RES_SPINE_SCENE_H
#define DM_GAMESYS_RES_SPINE_SCENE_H

#include <stdint.h>

#include <resource/resource.h>
#include "res_skeleton.h"
#include "res_animationset.h"
#include "res_meshset.h"
#include "res_textureset.h"
#include "spine_ddf.h"
#include <rig/rig.h>
#include <rig/rig_ddf.h>

namespace dmGameSystem
{

    struct RigSceneResource
    {
        dmArray<dmRig::RigBone> m_BindPose;
        dmRigDDF::RigScene*     m_RigScene;
        SkeletonResource*       m_SkeletonRes;
        MeshSetResource*        m_MeshSetRes;
        AnimationSetResource*   m_AnimationSetRes;
        TextureSetResource*     m_TextureSet;
        dmGraphics::HTexture    m_Textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        dmArray<uint32_t>       m_PoseIdxToInfluence;
        dmArray<uint32_t>       m_TrackIdxToPose;
    };

    dmResource::Result ResRigScenePreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResRigSceneCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResRigSceneDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResRigSceneRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMESYS_RES_SPINE_SCENE_H
