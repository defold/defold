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
    // NOTE: This structure has a matching .sol implementation which needs to be the same size
    struct TextureSetResource
    {
        inline TextureSetResource()
        {
            m_Texture = 0;
            m_TextureSet = 0;
            m_HullSet = 0;
        }
        dmGraphics::HTexture                m_Texture;
        dmGameSystemDDF::TextureSet*        m_TextureSet;

        dmArray<dmhash_t>                   m_HullCollisionGroups;
        dmHashTable<dmhash_t, uint32_t>     m_AnimationIds;
        dmPhysics::HHullSet2D               m_HullSet;
        uint8_t                             m_Valid;
    };

    dmResource::Result ResTextureSetPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResTextureSetCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResTextureSetDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResTextureSetRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif // DMGAMESYSTEM_RES_TEXTURESET_H
