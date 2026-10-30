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

#ifndef DM_TEST_TEXTURE_UPLOAD_H
#define DM_TEST_TEXTURE_UPLOAD_H

#include "graphics_private.h"
#include "graphics_adapter.h"

namespace dmGraphicsTest
{
    struct TextureUploadReadback
    {
        dmGraphics::TextureFormat m_Format;
        uint32_t m_Mip;
        uint32_t m_Width;
        uint32_t m_Height;
        uint32_t m_Depth;
        uint32_t m_Layers;
        uint32_t m_RowBytes;
        uint32_t m_Rows;
    };

    void SetTextureUploadObserver(dmGraphics::HContext context, dmGraphics::TextureUploadMemoryObserver observer, void* user_data);
    void PrepareTextureUploadReadback(dmGraphics::HContext context, dmGraphics::HTexture texture);
    dmGraphics::TextureFormat GetTextureUploadStorageFormat(dmGraphics::AdapterFamily family, dmGraphics::TextureFormat source);
    bool ReadTextureUpload(dmGraphics::HContext context, dmGraphics::HTexture texture, const TextureUploadReadback& params, dmArray<uint8_t>& pixels);
    uint64_t GetDX12TextureUploadExpectedBytes(dmGraphics::HContext context, dmGraphics::TextureType type, const dmGraphics::TextureParams& base, const dmGraphics::TextureParams& mip, uint32_t mip_count, uint32_t layers);
} // namespace dmGraphicsTest

#endif
