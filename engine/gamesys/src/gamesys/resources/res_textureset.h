#ifndef DMGAMESYSTEM_RES_TEXTURESET_H
#define DMGAMESYSTEM_RES_TEXTURESET_H

#include <stdint.h>

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
            memset(this, 0, sizeof(*this));
        }

        dmArray<dmhash_t>           m_HullCollisionGroups;
        dmArray<dmhash_t>           m_AnimationIds;
        dmGraphics::HTexture        m_Texture;
        dmGameSystemDDF::TextureSet*   m_TextureSet;
        dmPhysics::HHullSet2D       m_HullSet;
    };

    dmResource::Result ResTextureSetCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
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
