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

#include "test_app_graphics_texture_upload.h"

namespace dmGraphicsTest
{
#if defined(DM_TEST_APP_GRAPHICS_HAS_METAL)
    bool ReadMetalTextureUpload(dmGraphics::HContext, dmGraphics::HTexture, const TextureUploadReadback&, dmArray<uint8_t>&);
#endif
#if defined(DM_TEST_APP_GRAPHICS_HAS_OPENGL)
    bool ReadOpenGLTextureUpload(dmGraphics::HContext, dmGraphics::HTexture, const TextureUploadReadback&, dmArray<uint8_t>&);
#endif
#if defined(DM_TEST_APP_GRAPHICS_HAS_VULKAN)
    void PrepareVulkanTextureUploadReadback(dmGraphics::HContext, dmGraphics::HTexture);
    bool ReadVulkanTextureUpload(dmGraphics::HContext, dmGraphics::HTexture, const TextureUploadReadback&, dmArray<uint8_t>&);
#endif
#if defined(DM_TEST_APP_GRAPHICS_HAS_WEBGPU)
    void PrepareWebGPUTextureUploadReadback(dmGraphics::HContext, dmGraphics::HTexture);
    bool ReadWebGPUTextureUpload(dmGraphics::HContext, dmGraphics::HTexture, const TextureUploadReadback&, dmArray<uint8_t>&);
#endif
#if defined(DM_TEST_APP_GRAPHICS_HAS_DX12)
    bool ReadDX12TextureUpload(dmGraphics::HContext, dmGraphics::HTexture, const TextureUploadReadback&, dmArray<uint8_t>&);
    uint64_t DX12TextureUploadExpectedBytes(dmGraphics::HContext, dmGraphics::TextureType, const dmGraphics::TextureParams&, const dmGraphics::TextureParams&, uint32_t, uint32_t);
#endif
    void SetTextureUploadObserver(dmGraphics::HContext context, dmGraphics::TextureUploadMemoryObserver observer, void* user_data)
    {
        dmGraphics::TextureUploadMemoryBudget& budget = context->m_TextureUploadMemoryBudget;
        DM_MUTEX_SCOPED_LOCK(budget.m_Mutex);
        budget.m_Observer = observer;
        budget.m_ObserverData = user_data;
    }

    void PrepareTextureUploadReadback(dmGraphics::HContext context, dmGraphics::HTexture texture)
    {
        switch (dmGraphics::GetInstalledAdapterFamily())
        {
#if defined(DM_TEST_APP_GRAPHICS_HAS_VULKAN)
            case dmGraphics::ADAPTER_FAMILY_VULKAN:
                PrepareVulkanTextureUploadReadback(context, texture);
                break;
#endif
#if defined(DM_TEST_APP_GRAPHICS_HAS_WEBGPU)
            case dmGraphics::ADAPTER_FAMILY_WEBGPU:
                PrepareWebGPUTextureUploadReadback(context, texture);
                break;
#endif
            default:
                break;
        }
    }

    dmGraphics::TextureFormat GetTextureUploadStorageFormat(dmGraphics::AdapterFamily family, dmGraphics::TextureFormat source)
    {
        using namespace dmGraphics;
        if (family == ADAPTER_FAMILY_OPENGL || family == ADAPTER_FAMILY_OPENGLES)
            return source;
        if (source == TEXTURE_FORMAT_RGB)
            return TEXTURE_FORMAT_RGBA;
        if (family == ADAPTER_FAMILY_METAL || family == ADAPTER_FAMILY_DIRECTX)
        {
            if (source == TEXTURE_FORMAT_RGB16F)
                return TEXTURE_FORMAT_RGBA16F;
            if (family == ADAPTER_FAMILY_METAL && source == TEXTURE_FORMAT_RGB32F)
                return TEXTURE_FORMAT_RGBA32F;
        }
        return source;
    }

    bool ReadTextureUpload(dmGraphics::HContext context, dmGraphics::HTexture texture, const TextureUploadReadback& params, dmArray<uint8_t>& pixels)
    {
        uint32_t size = params.m_RowBytes * params.m_Rows * params.m_Depth * params.m_Layers;
        pixels.SetCapacity(size);
        pixels.SetSize(size);
        switch (dmGraphics::GetInstalledAdapterFamily())
        {
#if defined(DM_TEST_APP_GRAPHICS_HAS_METAL)
            case dmGraphics::ADAPTER_FAMILY_METAL:
                return ReadMetalTextureUpload(context, texture, params, pixels);
#endif
#if defined(DM_TEST_APP_GRAPHICS_HAS_OPENGL)
            case dmGraphics::ADAPTER_FAMILY_OPENGL:
                return ReadOpenGLTextureUpload(context, texture, params, pixels);
#endif
#if defined(DM_TEST_APP_GRAPHICS_HAS_VULKAN)
            case dmGraphics::ADAPTER_FAMILY_VULKAN:
                return ReadVulkanTextureUpload(context, texture, params, pixels);
#endif
#if defined(DM_TEST_APP_GRAPHICS_HAS_WEBGPU)
            case dmGraphics::ADAPTER_FAMILY_WEBGPU:
                return ReadWebGPUTextureUpload(context, texture, params, pixels);
#endif
#if defined(DM_TEST_APP_GRAPHICS_HAS_DX12)
            case dmGraphics::ADAPTER_FAMILY_DIRECTX:
                return ReadDX12TextureUpload(context, texture, params, pixels);
#endif
            default:
                return false;
        }
    }

    uint64_t GetDX12TextureUploadExpectedBytes(dmGraphics::HContext context, dmGraphics::TextureType type, const dmGraphics::TextureParams& base, const dmGraphics::TextureParams& mip, uint32_t mip_count, uint32_t layers)
    {
#if defined(DM_TEST_APP_GRAPHICS_HAS_DX12)
        return DX12TextureUploadExpectedBytes(context, type, base, mip, mip_count, layers);
#else
        return 0;
#endif
    }
} // namespace dmGraphicsTest
