#ifndef DM_GAMESYS_RES_SPINE_MODEL_H
#define DM_GAMESYS_RES_SPINE_MODEL_H

#include <stdint.h>

#include <resource/resource.h>
#include "res_spine_scene.h"
#include "spine_ddf.h"

namespace dmGameSystem
{
    struct SpineModelResource
    {
        dmGameSystemDDF::SpineModelDesc*    m_Model;
        SpineSceneResource*                 m_Scene;
        dmRender::HMaterial                 m_Material;
    };

    dmResource::Result ResSpineModelPreload(dmResource::HFactory factory, dmResource::HPreloadHintInfo hint_info,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void** preload_data,
            const char* filename);

    dmResource::Result ResSpineModelCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void* preload_data,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::Result ResSpineModelDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::Result ResSpineModelRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DM_GAMESYS_RES_SPINE_MODEL_H
