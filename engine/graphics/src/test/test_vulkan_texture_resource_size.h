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

#ifndef DM_TEST_VULKAN_TEXTURE_RESOURCE_SIZE_H
#define DM_TEST_VULKAN_TEXTURE_RESOURCE_SIZE_H

// Verifies full mip-chain accounting for synchronous and asynchronous uploads,
// including RGB expansion, compressed mips, sub-updates and texture recreation.
struct VulkanTextureResourceSizeTest : ITest
{
    enum
    {
        INITIAL_TEXTURE_SIZE = 256,
        RESIZED_TEXTURE_SIZE = 512,
        MIP_COUNT            = 9,
        SUB_UPDATE_STEP      = MIP_COUNT,
        RESIZE_STEP,
        STEP_COUNT,
        CASE_COUNT = 6
    };

    struct Case
    {
        dmGraphics::TextureFormat m_Format;
        uint32_t                  m_MinimumMipChainBytes;
        bool                      m_Asynchronous;
    };

    const Case& GetCase() const
    {
        // Fixed expected payload sizes for 256x256 through 1x1. RGB expands to RGBA.
        static const Case cases[CASE_COUNT] = {
            { dmGraphics::TEXTURE_FORMAT_RGBA,    349524, false },
            { dmGraphics::TEXTURE_FORMAT_RGB,     349524, false },
            { dmGraphics::TEXTURE_FORMAT_RGB_BC1,   43704, false },
            { dmGraphics::TEXTURE_FORMAT_RGBA,    349524, true  },
            { dmGraphics::TEXTURE_FORMAT_RGB,     349524, true  },
            { dmGraphics::TEXTURE_FORMAT_RGB_BC1,   43704, true  }
        };
        return cases[m_Case];
    }

    dmGraphics::HTexture m_Texture = 0;
    uint8_t*             m_Data = 0;
    uint32_t             m_Case = 0;
    uint32_t             m_Step = 0;
    uint32_t             m_OriginalSize = 0;
    bool                 m_Pending = false;
    bool                 m_CheckUpload = false;

    static void UploadComplete(dmGraphics::HTexture, void* user_data)
    {
        ((VulkanTextureResourceSizeTest*) user_data)->m_Pending = false;
    }

    void Check(EngineCtx* engine, bool condition, const char* message)
    {
        if (!condition)
        {
            dmLogError("Texture resource size case %u step %u: %s", m_Case, m_Step, message);
            engine->m_Failed = true;
        }
    }

    void Initialize(EngineCtx* engine) override
    {
        Check(engine, dmGraphics::GetInstalledAdapterFamily() == dmGraphics::ADAPTER_FAMILY_VULKAN, "requires Vulkan");
        m_Data = (uint8_t*) calloc(RESIZED_TEXTURE_SIZE * RESIZED_TEXTURE_SIZE, 4);
    }

    void CheckCompletedUpload(EngineCtx* engine)
    {
        using namespace dmGraphics;
        VulkanContext* context = (VulkanContext*) engine->m_GraphicsContext;
        VulkanTexture* texture = GetAssetFromContainer<VulkanTexture>(context->m_BaseContext.m_AssetHandleContainer, m_Texture);

        VkMemoryRequirements requirements;
        vkGetImageMemoryRequirements(context->m_LogicalDevice.m_Device, texture->m_Handle.m_Image, &requirements);
        uint32_t reported = GetTextureResourceSize(engine->m_GraphicsContext, m_Texture);
        Check(engine, reported == requirements.size + sizeof(VulkanTexture), "reported size must cover the entire image allocation");

        if (m_Step == 0)
        {
            Check(engine, requirements.size >= GetCase().m_MinimumMipChainBytes, "allocation must contain every mip level");
            m_OriginalSize = reported;
        }
        else if (m_Step < RESIZE_STEP)
        {
            Check(engine, reported == m_OriginalSize, "mips and sub-updates must not change allocation accounting");
        }
        else
        {
            Check(engine, reported > m_OriginalSize, "recreation must account for the larger allocation");
        }
    }

    void UploadNextStep(EngineCtx* engine)
    {
        using namespace dmGraphics;
        if (!m_Texture)
        {
            TextureCreationParams creation;
            creation.m_Width = INITIAL_TEXTURE_SIZE;
            creation.m_Height = INITIAL_TEXTURE_SIZE;
            creation.m_MipMapCount = MIP_COUNT;
            m_Texture = NewTexture(engine->m_GraphicsContext, creation);
        }

        // Upload every mip, modify a small region, then recreate a larger image.
        TextureParams params;
        if (m_Step < MIP_COUNT)
        {
            params.m_MipMap = m_Step;
            params.m_Width = INITIAL_TEXTURE_SIZE >> m_Step;
        }
        else if (m_Step == SUB_UPDATE_STEP)
        {
            params.m_SubUpdate = true;
            params.m_Width = 4;
        }
        else
        {
            params.m_Width = RESIZED_TEXTURE_SIZE;
        }
        params.m_Height = params.m_Width;
        params.m_Format = GetCase().m_Format;
        params.m_Data = m_Data;
        params.m_DataSize = GetTextureFormatDataSize(params.m_Format, params.m_Width, params.m_Height);

        m_CheckUpload = true;
        if (GetCase().m_Asynchronous)
        {
            m_Pending = true;
            SetTextureAsync(engine->m_GraphicsContext, m_Texture, params, UploadComplete, this);
        }
        else
        {
            SetTexture(engine->m_GraphicsContext, m_Texture, params);
        }
    }

    void Execute(EngineCtx* engine) override
    {
        // Completion callbacks are pumped by the harness before each frame.
        if (m_Pending)
        {
            return;
        }
        if (m_CheckUpload)
        {
            CheckCompletedUpload(engine);
            m_CheckUpload = false;
            ++m_Step;
            if (m_Step == STEP_COUNT)
            {
                dmGraphics::DeleteTexture(engine->m_GraphicsContext, m_Texture);
                m_Texture = 0;
                m_Step = 0;
                ++m_Case;
            }
        }

        if (engine->m_Failed || m_Case == CASE_COUNT)
        {
            if (!engine->m_Failed)
            {
                dmLogInfo("Texture resource size: PASS (sync/async RGBA, RGB, BC1 mip chains, sub-updates and recreation)");
            }
            engine->m_Running = 0;
            return;
        }
        UploadNextStep(engine);
    }

    void OnGraphicsClosing(EngineCtx* engine) override
    {
        if (m_Texture)
        {
            dmGraphics::DeleteTexture(engine->m_GraphicsContext, m_Texture);
        }
    }

    void OnGraphicsDeleted(EngineCtx*) override
    {
        free(m_Data);
    }
};

#endif
