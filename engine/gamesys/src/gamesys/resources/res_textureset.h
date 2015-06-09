#ifndef DMGAMESYSTEM_RES_TEXTURESET_H
#define DMGAMESYSTEM_RES_TEXTURESET_H

#include <stdint.h>

#include <dlib/hashtable.h>

#include <resource/resource.h>

#include <render/render.h>

#include <physics/physics.h>

#include "texture_set_ddf.h"

namespace dmGameSystem
{
    struct TextureSetResource
    {
        inline TextureSetResource()
        {
            m_Texture = 0;
            m_TextureSet = 0;
            m_HullSet = 0;
        }

        dmArray<dmhash_t>                   m_HullCollisionGroups;
        dmHashTable<dmhash_t, uint32_t>     m_AnimationIds;
        dmGraphics::HTexture                m_Texture;
        dmGameSystemDDF::TextureSet*        m_TextureSet;
        dmPhysics::HHullSet2D               m_HullSet;
    };

    dmResource::Result ResTextureSetPreload(dmResource::HFactory factory, dmResource::HPreloadHintInfo hint_info,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void** preload_data,
            const char* filename);

    dmResource::Result ResTextureSetCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void* preload_data,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::Result ResTextureSetDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::Result ResTextureSetRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DMGAMESYSTEM_RES_TEXTURESET_H
