#ifndef DM_GAMESYS_RES_SPINE_SCENE_H
#define DM_GAMESYS_RES_SPINE_SCENE_H

#include <stdint.h>

#include <resource/resource.h>
#include "res_textureset.h"
#include "spine_ddf.h"

namespace dmGameSystem
{
    struct SpineBone
    {
        /// Local space transform
        dmTransform::Transform m_LocalToParent;
        /// Model space transform
        dmTransform::Transform m_LocalToModel;
        /// Inv model space transform
        dmTransform::Transform m_ModelToLocal;
        /// Index of parent bone, NOTE root bone has itself as parent
        uint32_t m_ParentIndex;
        /// Length of the bone
        float m_Length;
    };

    struct SpineSceneResource
    {
        dmArray<SpineBone>              m_BindPose;
        dmGameSystemDDF::SpineScene*    m_SpineScene;
        TextureSetResource*             m_TextureSet;
    };

    dmResource::Result ResSpineScenePreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResSpineSceneCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResSpineSceneDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResSpineSceneRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMESYS_RES_SPINE_SCENE_H
