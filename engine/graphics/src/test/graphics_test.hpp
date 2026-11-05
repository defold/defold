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

#ifndef DM_GRAPHICS_TEST_HPP
#define DM_GRAPHICS_TEST_HPP

#include <graphics/graphics.h>

// Test support for the null adapter. Handles and returned data are borrowed;
// uploads, deletion and subsequent draws may invalidate the returned data.
namespace dmGraphics
{
    struct TestBufferUploads
    {
        uint32_t m_DataSetCount;
        uint64_t m_DataSetBytes;
        uint32_t m_SubDataSetCount;
        uint64_t m_SubDataSetBytes;
    };

    int                     GetShaderStageFlags(ShaderDesc::ShaderType type);
    uint32_t                GetShaderTypeSize(ShaderDesc::ShaderDataType type);
    void                    SetForceVertexReloadFail(bool should_fail);
    void                    SetForceFragmentReloadFail(bool should_fail);
    void                    ResetDrawCount();
    uint64_t                GetDrawCount();
    const dmVMath::Vector4& GetConstantV4Ptr(HContext context, HUniformLocation location);
    void                    GetTextureFilters(HContext context, uint32_t unit, TextureFilter& min_filter, TextureFilter& mag_filter);
    void                    SetOverrideShaderLanguage(HContext context, ShaderDesc::ShaderType type, ShaderDesc::Language language);

    void                          TestSetContextSize(HContext context, uint32_t width, uint32_t height);
    void                          TestSetContextFeatures(HContext context, uint32_t features);
    uint64_t                      TestGetTextureFormats(HContext context);
    void                          TestSetTextureFormats(HContext context, uint64_t formats);
    void                          TestSetAsyncTextureLoad(HContext context, bool enabled);
    bool                          TestIsRenderTargetBound(HContext context, HRenderTarget target);
    TextureFormat                 TestGetTextureFormat(HContext context, HTexture texture);
    const uint8_t*                TestGetTextureData(HContext context, HTexture texture);
    int32_t                       TestGetTextureLastBoundUnit(HContext context, HTexture texture, uint32_t index);
    const char*                   TestGetVertexBufferData(HVertexBuffer buffer);
    const char*                   TestGetIndexBufferData(HIndexBuffer buffer);
    TestBufferUploads             TestGetVertexBufferUploads(HVertexBuffer buffer);
    HUniformBuffer                TestGetBoundUniformBuffer(HContext context, uint32_t set, uint32_t binding);
    const uint8_t*                TestGetUniformBufferData(HUniformBuffer buffer);
    uint32_t                      TestGetUniformBufferSize(HUniformBuffer buffer);
    bool                          TestWasUniformBufferUsed(HUniformBuffer buffer);
    const uint8_t*                TestGetUniformData(HProgram program, HUniformLocation location);
    const ShaderResourceTypeInfo* TestGetShaderTypes(HProgram program, uint32_t* count);
}

#endif // DM_GRAPHICS_TEST_HPP
