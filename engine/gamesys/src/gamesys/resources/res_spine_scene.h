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
    };

    struct SpineSceneResource
    {
        dmArray<SpineBone>              m_BindPose;
        dmGameSystemDDF::SpineScene*    m_SpineScene;
        TextureSetResource*             m_TextureSet;
    };

    dmResource::Result ResSpineScenePreload(dmResource::HFactory factory,
            dmResource::HPreloadHintInfo hint_info,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void** preload_data,
            const char* filename);

    dmResource::Result ResSpineSceneCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void* preload_data,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::Result ResSpineSceneDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::Result ResSpineSceneRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DM_GAMESYS_RES_SPINE_SCENE_H
