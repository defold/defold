#ifndef DM_GAMESYS_RES_SPINE_SCENE_H
#define DM_GAMESYS_RES_SPINE_SCENE_H

#include <stdint.h>

#include <resource/resource.h>
#include "res_textureset.h"
#include "spine_ddf.h"

namespace dmGameSystem
{
    struct SpineSceneResource
    {
        dmGameSystemDDF::SpineScene*            m_SpineScene;
        TextureSetResource*                     m_TextureSet;
    };

    dmResource::Result ResSpineSceneCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
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
