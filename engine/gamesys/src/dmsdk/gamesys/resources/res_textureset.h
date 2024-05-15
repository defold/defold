
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

#ifndef DMSDK_RES_TEXTURESET_H
#define DMSDK_RES_TEXTURESET_H

#include <stdint.h>

#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/hashtable.h>
#include <dmsdk/gamesys/resources/res_texture.h>
#include <dmsdk/graphics/graphics.h>
#include <dmsdk/physics/physics.h>
#include <gamesys/texture_set_ddf.h>

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
        dmHashTable64<uint32_t>             m_AnimationIds; // Animation id to animation index
        dmHashTable64<uint32_t>             m_FrameIds;     // Animation id to frame index
        TextureResource*                    m_Texture;
        dmhash_t                            m_TexturePath;
        dmGameSystemDDF::TextureSet*        m_TextureSet;
        dmPhysics::HHullSet2D               m_HullSet;
    };
}

#endif // DMSDK_RES_TEXTURESET_H
