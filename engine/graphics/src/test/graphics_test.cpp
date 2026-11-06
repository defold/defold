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

#include "graphics_test.hpp"
#include "../graphics_private.h"
#include "../null/graphics_null_private.h"

namespace dmGraphics
{
    int GetShaderStageFlags(ShaderDesc::ShaderType type)
    {
        switch (type)
        {
            case ShaderDesc::SHADER_TYPE_VERTEX:
            {
                return SHADER_STAGE_FLAG_VERTEX;
            }
            case ShaderDesc::SHADER_TYPE_FRAGMENT:
            {
                return SHADER_STAGE_FLAG_FRAGMENT;
            }
            case ShaderDesc::SHADER_TYPE_COMPUTE:
            {
                return SHADER_STAGE_FLAG_COMPUTE;
            }
            default:
            {
                return 0;
            }
        }
    }

    void TestSetContextSize(HContext context, uint32_t width, uint32_t height)
    {
        context->m_Width = width;
        context->m_Height = height;
    }

    void TestSetContextFeatures(HContext context, uint32_t features)
    {
        context->m_ContextFeatureSupport = features;
    }

    uint64_t TestGetTextureFormats(HContext context)
    {
        return context->m_TextureFormatSupport;
    }

    void TestSetTextureFormats(HContext context, uint64_t formats)
    {
        context->m_TextureFormatSupport = formats;
    }

    void TestSetAsyncTextureLoad(HContext context, bool enabled)
    {
        ((NullContext*)context)->m_UseAsyncTextureLoad = enabled;
    }

    bool TestIsRenderTargetBound(HContext context, HRenderTarget target)
    {
        NullContext* null_context = (NullContext*)context;
        FrameBuffer* expected = &null_context->m_MainFrameBuffer;
        if (target)
        {
            expected = &GetAssetFromContainer<NullRenderTarget>(context->m_AssetHandleContainer, target)->m_FrameBuffer;
        }
        return expected == null_context->m_CurrentFrameBuffer;
    }

    TextureFormat TestGetTextureFormat(HContext context, HTexture texture)
    {
        return GetAssetFromContainer<Texture>(context->m_AssetHandleContainer, texture)->m_Format;
    }

    const uint8_t* TestGetTextureData(HContext context, HTexture texture)
    {
        return (const uint8_t*)GetAssetFromContainer<NullTexture>(context->m_AssetHandleContainer, texture)->m_Data;
    }

    int32_t TestGetTextureLastBoundUnit(HContext context, HTexture texture, uint32_t index)
    {
        return GetAssetFromContainer<NullTexture>(context->m_AssetHandleContainer, texture)->m_LastBoundUnit[index];
    }

    const char* TestGetVertexBufferData(HVertexBuffer buffer)
    {
        return ((VertexBuffer*)buffer)->m_Buffer;
    }

    const char* TestGetIndexBufferData(HIndexBuffer buffer)
    {
        return ((IndexBuffer*)buffer)->m_Buffer;
    }

    TestBufferUploads TestGetVertexBufferUploads(HVertexBuffer buffer)
    {
        VertexBuffer* native = (VertexBuffer*)buffer;
        TestBufferUploads uploads = { native->m_DataSetCount, native->m_DataSetBytes,
                                      native->m_SubDataSetCount, native->m_SubDataSetBytes };
        return uploads;
    }

    HUniformBuffer TestGetBoundUniformBuffer(HContext context, uint32_t set, uint32_t binding)
    {
        return (HUniformBuffer)((NullContext*)context)->m_UniformBuffers[set][binding];
    }

    const uint8_t* TestGetUniformBufferData(HUniformBuffer buffer)
    {
        return ((NullUniformBuffer*)buffer)->m_Buffer;
    }

    uint32_t TestGetUniformBufferSize(HUniformBuffer buffer)
    {
        return ((NullUniformBuffer*)buffer)->m_BufferSize;
    }

    bool TestWasUniformBufferUsed(HUniformBuffer buffer)
    {
        return ((NullUniformBuffer*)buffer)->m_UsedInDraw;
    }

    const uint8_t* TestGetUniformData(HProgram program, HUniformLocation location)
    {
        uint32_t set = UNIFORM_LOCATION_GET_OP0(location);
        uint32_t binding = UNIFORM_LOCATION_GET_OP1(location);
        uint32_t offset = UNIFORM_LOCATION_GET_OP2(location);
        return ((NullProgram*)program)->m_UniformData + ((Program*)program)->m_ResourceBindings[set][binding].m_UniformBufferOffset + offset;
    }

    const ShaderResourceTypeInfo* TestGetShaderTypes(HProgram program, uint32_t* count)
    {
        *count = ((Program*)program)->m_ShaderMeta.m_TypeInfos.Size();
        return ((Program*)program)->m_ShaderMeta.m_TypeInfos.Begin();
    }
}
