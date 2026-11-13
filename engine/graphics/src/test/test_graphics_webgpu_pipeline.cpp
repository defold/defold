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

#include "../webgpu/graphics_webgpu_private.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

struct CapturedColorTarget
{
    WGPUTextureFormat m_Format;
    uint64_t          m_WriteMask;
    bool              m_HasBlend;
    WGPUBlendState    m_Blend;
};

static CapturedColorTarget g_Targets[dmGraphics::MAX_BUFFER_COLOR_ATTACHMENTS];
static uint32_t            g_TargetCount;
static uint32_t            g_CreateCount;

// Keep the API consumer instrumented too: packaged Dawn does not check these reads with ASan.
static __attribute__((noinline)) WGPURenderPipeline CaptureRenderPipeline(WGPUDevice, const WGPURenderPipelineDescriptor* desc)
{
    ++g_CreateCount;
    g_TargetCount = desc->fragment->targetCount;
    for (uint32_t i = 0; i < g_TargetCount; ++i)
    {
        const WGPUColorTargetState& target = desc->fragment->targets[i];
        g_Targets[i].m_Format = target.format;
        g_Targets[i].m_WriteMask = target.writeMask;
        g_Targets[i].m_HasBlend = target.blend != NULL;
        if (target.blend)
        {
            g_Targets[i].m_Blend = *target.blend;
        }
    }
    // The cache stores this token, but the fixture never sends it to the GPU or releases it.
    return (WGPURenderPipeline)g_Targets;
}

// Exercise the production pipeline builder without adding a test hook to the engine API.
#define wgpuDeviceCreateRenderPipeline CaptureRenderPipeline
#include "../webgpu/graphics_webgpu.cpp"
#undef wgpuDeviceCreateRenderPipeline

struct WebGPUPipelineTest : jc_test_base_class
{
    dmGraphics::WebGPUContext*     m_Context;
    dmGraphics::WebGPUProgram      m_Program;
    dmGraphics::WebGPUShaderModule m_Vertex;
    dmGraphics::WebGPUShaderModule m_Fragment;
    dmGraphics::WebGPURenderTarget m_Target;
    dmGraphics::WebGPUTexture      m_Textures[2];

    void SetUp()
    {
        m_Context = new dmGraphics::WebGPUContext();
        g_WebGPUContext = m_Context;
        m_Context->m_RenderPipelineCache.SetCapacity(8);
        m_Context->m_CurrentPipelineState = dmGraphics::GetDefaultPipelineState();
        m_Context->m_CurrentProgram = &m_Program;
        m_Context->m_CurrentRenderPass.m_Target = &m_Target;
        m_Program.m_VertexModule = &m_Vertex;
        m_Program.m_FragmentModule = &m_Fragment;
        m_Target.m_Base.m_Id = dmGraphics::DM_RENDERTARGET_BACKBUFFER_ID;
        m_Target.m_Base.m_ColorAttachmentCount = 2;
        m_Target.m_Multisample = 1;
        for (uint32_t i = 0; i < 2; ++i)
        {
            m_Textures[i].m_Format = i == 0 ? WGPUTextureFormat_RGBA8Unorm : WGPUTextureFormat_BGRA8Unorm;
            m_Target.m_TextureColor[i] = dmGraphics::StoreAssetInContainer(m_Context->m_BaseContext.m_AssetHandleContainer,
                                                                           &m_Textures[i], dmGraphics::ASSET_TYPE_TEXTURE);
        }
        memset(g_Targets, 0, sizeof(g_Targets));
        g_TargetCount = 0;
        g_CreateCount = 0;
    }

    void TearDown()
    {
        g_WebGPUContext = NULL;
        delete m_Context;
    }
};

// ASan must reject color-target descriptors whose block scope ends before pipeline creation.
TEST_F(WebGPUPipelineTest, ColorTargetsAliveAtPipelineCreation)
{
    m_Context->m_CurrentPipelineState.m_BlendEnabled = 0;
    WebGPUGetOrCreateRenderPipeline(m_Context);
    ASSERT_EQ(1u, g_CreateCount);
    ASSERT_EQ(2u, g_TargetCount);
    for (uint32_t i = 0; i < 2; ++i)
    {
        ASSERT_EQ(m_Textures[i].m_Format, g_Targets[i].m_Format);
        ASSERT_EQ((uint64_t)WGPUColorWriteMask_All, g_Targets[i].m_WriteMask);
        ASSERT_FALSE(g_Targets[i].m_HasBlend);
    }
}

// ASan must reject blend descriptors whose block scope ends before the API reads their factors.
TEST_F(WebGPUPipelineTest, BlendStateAliveAtPipelineCreation)
{
    dmGraphics::PipelineState& state = m_Context->m_CurrentPipelineState;
    state.m_BlendEnabled = 1;
    state.m_BlendSrcFactor = dmGraphics::BLEND_FACTOR_SRC_ALPHA;
    state.m_BlendDstFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    state.m_BlendSrcFactorAlpha = dmGraphics::BLEND_FACTOR_ONE;
    state.m_BlendDstFactorAlpha = dmGraphics::BLEND_FACTOR_ZERO;
    state.m_BlendEquationColor = dmGraphics::BLEND_EQUATION_ADD;
    state.m_BlendEquationAlpha = dmGraphics::BLEND_EQUATION_REVERSE_SUBTRACT;
    WebGPUGetOrCreateRenderPipeline(m_Context);
    ASSERT_EQ(1u, g_CreateCount);
    ASSERT_EQ(2u, g_TargetCount);
    for (uint32_t i = 0; i < 2; ++i)
    {
        ASSERT_TRUE(g_Targets[i].m_HasBlend);
        ASSERT_EQ(WGPUBlendFactor_SrcAlpha, g_Targets[i].m_Blend.color.srcFactor);
        ASSERT_EQ(WGPUBlendFactor_OneMinusSrcAlpha, g_Targets[i].m_Blend.color.dstFactor);
        ASSERT_EQ(WGPUBlendOperation_Add, g_Targets[i].m_Blend.color.operation);
        ASSERT_EQ(WGPUBlendFactor_One, g_Targets[i].m_Blend.alpha.srcFactor);
        ASSERT_EQ(WGPUBlendFactor_Zero, g_Targets[i].m_Blend.alpha.dstFactor);
        ASSERT_EQ(WGPUBlendOperation_ReverseSubtract, g_Targets[i].m_Blend.alpha.operation);
    }
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
