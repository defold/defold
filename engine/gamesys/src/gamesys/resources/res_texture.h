// Copyright 2020-2026 The Defold Foundation
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

#ifndef DM_GAMESYS_RES_TEXTURE_H
#define DM_GAMESYS_RES_TEXTURE_H

#include <dmsdk/resource/resource.h>
#include <dmsdk/gamesys/resources/res_texture.h>
#include <graphics/graphics.h>

namespace dmGameSystem
{
    struct ResTextureUploadParams
    {
        uint16_t m_X;
        uint16_t m_Y;
        uint16_t m_Z;
        uint8_t  m_Page;
        uint8_t  m_MipMap               : 5;
        uint8_t  m_UploadSpecificMipmap : 1;
        uint8_t  m_SubUpdate            : 1;
        uint8_t                         : 1;
    };

    struct ResTextureReCreateParams
    {
        void*                  m_TextureImage;
        ResTextureUploadParams m_UploadParams;
    };

    dmGraphics::TextureType TextureImageToTextureType(dmGraphics::TextureImage::Type type);
    dmGraphics::TextureFormat TextureImageToTextureFormat(dmGraphics::TextureImage::TextureFormat format);

    dmResource::Result ResTexturePreload(const dmResource::ResourcePreloadParams* params);

    dmResource::Result ResTextureCreate(const dmResource::ResourceCreateParams* params);

    dmResource::Result ResTexturePostCreate(const dmResource::ResourcePostCreateParams* params);

    dmResource::Result ResTextureDestroy(const dmResource::ResourceDestroyParams* params);

    dmResource::Result ResTextureRecreate(const dmResource::ResourceRecreateParams* params);
}

#endif
