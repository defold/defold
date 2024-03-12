// Copyright 2020-2024 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DMSDK_GAMESYS_RES_SPRITE_H
#define DMSDK_GAMESYS_RES_SPRITE_H

#include <stdint.h>

#include <gamesys/sprite_ddf.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/render/render.h>

namespace dmGameSystem
{
    struct MaterialResource;
    struct TextureSetResource;

    struct SpriteTexture
    {
        dmhash_t            m_SamplerNameHash;
        TextureSetResource* m_TextureSet;
    };

    struct SpriteResource
    {
        dmGameSystemDDF::SpriteDesc* m_DDF;
        MaterialResource*           m_Material;
        dmhash_t                    m_DefaultAnimation;

        // Sorted by the order of occurrance of samplers in the .material file
        SpriteTexture* m_Textures;
        uint32_t       m_NumTextures;
    };
}

#endif // DMSDK_GAMESYS_RES_SPRITE_H
