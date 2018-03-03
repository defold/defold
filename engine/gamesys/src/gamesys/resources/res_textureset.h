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
        dmhash_t                            m_TexturePath;
        dmGameSystemDDF::TextureSet*        m_TextureSet;
        dmPhysics::HHullSet2D               m_HullSet;
    };

    dmResource::Result ResTextureSetPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResTextureSetCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResTextureSetDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResTextureSetRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResTextureSetGetInfo(dmResource::ResourceGetInfoParams& params);
}

#endif // DMGAMESYSTEM_RES_TEXTURESET_H
