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

static WGPUBindGroupLayoutEntry g_Bindings[dmGraphics::MAX_SET_COUNT][dmGraphics::MAX_BINDINGS_PER_SET_COUNT];
static uint32_t g_LayoutCount;
static uint32_t g_PipelineLayoutCount;
static uint32_t g_ReleasedLayoutCount;
static uint32_t g_ReleasedPipelineLayoutCount;
static uint32_t g_GroupCount;
static uint32_t g_PipelineCount;
static bool g_NativeDevice;
#if defined(DM_GRAPHICS_DAWN)
static bool g_RejectStencilClearPipeline;
#endif

static WGPUBindGroupLayout CaptureBindGroupLayout(WGPUDevice device, const WGPUBindGroupLayoutDescriptor* desc)
{
    const uint32_t set = g_LayoutCount++ % dmGraphics::MAX_SET_COUNT;
    memset(g_Bindings[set], 0, sizeof(g_Bindings[set]));
    for (uint32_t i = 0; i < desc->entryCount; ++i)
    {
        g_Bindings[set][desc->entries[i].binding] = desc->entries[i];
    }

    if (g_NativeDevice)
    {
        return wgpuDeviceCreateBindGroupLayout(device, desc);
    }

    return (WGPUBindGroupLayout)(uintptr_t)g_LayoutCount;
}

static WGPUPipelineLayout CapturePipelineLayout(WGPUDevice device, const WGPUPipelineLayoutDescriptor* desc)
{
    ++g_PipelineLayoutCount;
    if (g_NativeDevice)
    {
        return wgpuDeviceCreatePipelineLayout(device, desc);
    }

    return (WGPUPipelineLayout)(uintptr_t)g_PipelineLayoutCount;
}

static void ReleaseBindGroupLayout(WGPUBindGroupLayout layout)
{
    ++g_ReleasedLayoutCount;
    if (g_NativeDevice)
    {
        wgpuBindGroupLayoutRelease(layout);
    }
}

static void ReleasePipelineLayout(WGPUPipelineLayout layout)
{
    ++g_ReleasedPipelineLayoutCount;
    if (g_NativeDevice)
    {
        wgpuPipelineLayoutRelease(layout);
    }
}

static WGPUBindGroup CaptureBindGroup(WGPUDevice device, const WGPUBindGroupDescriptor* desc)
{
    ++g_GroupCount;
    if (g_NativeDevice)
    {
        return wgpuDeviceCreateBindGroup(device, desc);
    }

    return (WGPUBindGroup)(uintptr_t)g_GroupCount;
}

static void ReleaseBindGroup(WGPUBindGroup group)
{
    if (g_NativeDevice)
    {
        wgpuBindGroupRelease(group);
    }
}

static WGPURenderPipeline CaptureRenderPipeline(WGPUDevice device, const WGPURenderPipelineDescriptor* desc)
{
    ++g_PipelineCount;
    if (g_NativeDevice)
    {
        return wgpuDeviceCreateRenderPipeline(device, desc);
    }

    return (WGPURenderPipeline)(uintptr_t)g_PipelineCount;
}

#if defined(DM_GRAPHICS_DAWN)
static WGPUFuture CaptureRenderPipelineAsync(WGPUDevice device, const WGPURenderPipelineDescriptor* desc, WGPUCreateRenderPipelineAsyncCallbackInfo callback)
{
    ++g_PipelineCount;
    if (g_RejectStencilClearPipeline)
    {
        WGPURenderPipelineDescriptor invalid_desc = *desc;
        invalid_desc.vertex.entryPoint.data = "missing";
        invalid_desc.vertex.entryPoint.length = 7;
        return wgpuDeviceCreateRenderPipelineAsync(device, &invalid_desc, callback);
    }

    return wgpuDeviceCreateRenderPipelineAsync(device, desc, callback);
}

#define wgpuDeviceCreateRenderPipelineAsync CaptureRenderPipelineAsync
#endif

// Capture the production descriptors without exposing an engine test API.
// The native test forwards layout, bind-group and pipeline creation to the real device.
#define wgpuDeviceCreateBindGroupLayout CaptureBindGroupLayout
#define wgpuDeviceCreatePipelineLayout CapturePipelineLayout
#define wgpuBindGroupLayoutRelease ReleaseBindGroupLayout
#define wgpuPipelineLayoutRelease ReleasePipelineLayout
#define wgpuDeviceCreateBindGroup CaptureBindGroup
#define wgpuBindGroupRelease ReleaseBindGroup
#define wgpuDeviceCreateRenderPipeline CaptureRenderPipeline
#include "../webgpu/graphics_webgpu.cpp"
#undef wgpuDeviceCreateBindGroupLayout
#undef wgpuDeviceCreatePipelineLayout
#undef wgpuBindGroupLayoutRelease
#undef wgpuPipelineLayoutRelease
#undef wgpuDeviceCreateBindGroup
#undef wgpuBindGroupRelease
#undef wgpuDeviceCreateRenderPipeline
#undef wgpuDeviceCreateRenderPipelineAsync

struct WebGPUTextureLayoutTest : jc_test_base_class
{
    dmGraphics::WebGPUContext*     m_Context;
    dmGraphics::WebGPUProgram      m_Program;
    dmGraphics::WebGPUTexture      m_Textures[4];
    dmGraphics::WebGPUShaderModule m_Vertex;
    dmGraphics::WebGPUShaderModule m_Fragment;
    dmGraphics::WebGPURenderTarget m_Target;
    WGPUInstance                   m_Instance;
    WGPUAdapter                    m_Adapter;
    WGPUBuffer                     m_ReadbackBuffer;

    void SetUp()
    {
        m_Instance = 0;
        m_Adapter = 0;
        m_ReadbackBuffer = 0;
        g_NativeDevice = false;
        g_LayoutCount = 0;
        g_PipelineLayoutCount = 0;
        g_ReleasedLayoutCount = 0;
        g_ReleasedPipelineLayoutCount = 0;
        g_GroupCount = 0;
        g_PipelineCount = 0;
#if defined(DM_GRAPHICS_DAWN)
        g_RejectStencilClearPipeline = false;
#endif
        memset(g_Bindings, 0, sizeof(g_Bindings));
        m_Context = new dmGraphics::WebGPUContext();
        g_WebGPUContext = m_Context;
        m_Context->m_CurrentProgram = &m_Program;
        m_Context->m_BindGroupCache.SetCapacity(8);
        m_Context->m_RenderPipelineCache.SetCapacity(8);
        m_Context->m_BindGroupPool = new dmObjectPool<dmGraphics::WebGPUBindGroup>();
        m_Context->m_BindGroupPool->SetCapacity(8);
#if defined(DM_GRAPHICS_WEBGPU2)
        m_Context->m_DeviceLimits.minUniformBufferOffsetAlignment = 256;
#else
        m_Context->m_DeviceLimits.limits.minUniformBufferOffsetAlignment = 256;
#endif
        m_Program.m_Hash = 1234;
        m_Program.m_BaseProgram.m_ShaderMeta.m_Textures.SetCapacity(8);
        AddTexture("bones", dmGraphics::ShaderDesc::SHADER_TYPE_TEXTURE2D, 0, 0, dmGraphics::SHADER_STAGE_FLAG_VERTEX, 0);
        AddTexture("bones_sampler", dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER, 1, 31, dmGraphics::SHADER_STAGE_FLAG_VERTEX, 0);
        AddTexture("color", dmGraphics::ShaderDesc::SHADER_TYPE_TEXTURE2D, 0, 1, dmGraphics::SHADER_STAGE_FLAG_FRAGMENT, 0);
        AddTexture("color_sampler", dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER, 1, 0, dmGraphics::SHADER_STAGE_FLAG_FRAGMENT, 2);
        for (uint32_t i = 0; i < 2; ++i)
        {
            m_Textures[i].m_Format = i == 0 ? WGPUTextureFormat_RGBA32Float : WGPUTextureFormat_RGBA8Unorm;
            m_Textures[i].m_TextureView = (WGPUTextureView)(uintptr_t)(i + 1);
            m_Textures[i].m_Sampler = (WGPUSampler)(uintptr_t)(i + 1);
            m_Context->m_CurrentTextureUnits[i] = &m_Textures[i];
        }
    }

    void TearDown()
    {
        // The fixture owns these modules; program cleanup must not delete them.
        m_Program.m_VertexModule = 0;
        m_Program.m_FragmentModule = 0;
        if (m_Program.m_Layouts.Size())
        {
            WebGPUDestroyProgram(m_Context, &m_Program);
        }

        dmGraphics::DestroyProgram(&m_Program.m_BaseProgram);
        delete m_Context->m_BindGroupPool;
        if (g_NativeDevice)
        {
            WebGPUDetachStencilClearPipelineRequest(m_Context);
            WebGPUEndRenderPass(m_Context);
            if (m_Context->m_CommandEncoder)
            {
                wgpuCommandEncoderRelease(m_Context->m_CommandEncoder);
            }

            m_Context->m_RenderPipelineCache.Iterate(WebGPUCleanupRenderPipelineCache, m_Context);
            for (uint32_t i = 0; i < DM_ARRAY_SIZE(m_Textures); ++i)
            {
                if (m_Textures[i].m_TextureView)
                {
                    wgpuTextureViewRelease(m_Textures[i].m_TextureView);
                }

                if (m_Textures[i].m_Texture)
                {
                    wgpuTextureRelease(m_Textures[i].m_Texture);
                }

                if (m_Textures[i].m_Sampler)
                {
                    wgpuSamplerRelease(m_Textures[i].m_Sampler);
                }
            }

            if (m_Vertex.m_Module)
            {
                wgpuShaderModuleRelease(m_Vertex.m_Module);
            }

            if (m_Fragment.m_Module)
            {
                wgpuShaderModuleRelease(m_Fragment.m_Module);
            }

            if (m_ReadbackBuffer)
            {
                wgpuBufferRelease(m_ReadbackBuffer);
            }

            if (m_Context->m_Queue)
            {
                wgpuQueueRelease(m_Context->m_Queue);
            }
        }

        if (m_Context->m_Device)
        {
            wgpuDeviceRelease(m_Context->m_Device);
        }

        if (m_Adapter)
        {
            wgpuAdapterRelease(m_Adapter);
        }

        if (m_Instance)
        {
            wgpuInstanceRelease(m_Instance);
        }

        delete m_Context;
        g_WebGPUContext = 0;
        g_NativeDevice = false;
    }

    void AddTexture(const char* name, dmGraphics::ShaderDesc::ShaderDataType type, uint16_t set, uint16_t binding, uint8_t stages, uint16_t texture_index)
    {
        dmGraphics::ShaderResourceBinding resource = {};
        resource.m_Name = strdup(name);
        resource.m_NameHash = dmHashString64(name);
        resource.m_BindingFamily = dmGraphics::BINDING_FAMILY_TEXTURE;
        resource.m_Type.m_ShaderType = type;
        resource.m_Set = set;
        resource.m_Binding = binding;
        resource.m_StageFlags = stages;
        resource.m_BindingInfo.m_SamplerTextureIndex = texture_index;
        m_Program.m_BaseProgram.m_ShaderMeta.m_Textures.Push(resource);
    }

    void Initialize()
    {
        WebGPUUpdateProgramLayouts(m_Context, &m_Program);
    }

};

// Bone data and its sampler must be unfilterable even when the sampler is in another set.
TEST_F(WebGPUTextureLayoutTest, BoneTextureWithoutFloat32Filtering)
{
    Initialize();
    WebGPUUpdateTextureBindingLayouts(m_Context);
    ASSERT_EQ(1u, m_Program.m_LayoutIndex);
    ASSERT_EQ(WGPUTextureSampleType_UnfilterableFloat, g_Bindings[2][0].texture.sampleType);
    ASSERT_EQ(WGPUSamplerBindingType_NonFiltering, g_Bindings[3][31].sampler.type);
    ASSERT_EQ(WGPUTextureSampleType_Float, g_Bindings[2][1].texture.sampleType);
    ASSERT_EQ(WGPUSamplerBindingType_Filtering, g_Bindings[3][0].sampler.type);
}

// Shared stage bindings must contribute visibility to one descriptor without consuming another texture unit.
TEST_F(WebGPUTextureLayoutTest, SharedBindingsAreInitializedOnce)
{
    AddTexture("bones_fragment", dmGraphics::ShaderDesc::SHADER_TYPE_TEXTURE2D, 0, 0, dmGraphics::SHADER_STAGE_FLAG_FRAGMENT, 0);
    Initialize();
    ASSERT_EQ(2u, m_Program.m_TextureSamplerCount);
    ASSERT_EQ(4u, m_Program.m_TotalResourcesCount);
    ASSERT_EQ(0u, m_Program.m_BaseProgram.m_ResourceBindings[0][0].m_TextureUnit);
    ASSERT_EQ(1u, m_Program.m_BaseProgram.m_ResourceBindings[0][1].m_TextureUnit);
    WebGPUUpdateTextureBindingLayouts(m_Context);
    ASSERT_EQ(WGPUShaderStage_Vertex | WGPUShaderStage_Fragment, g_Bindings[2][0].visibility);
    ASSERT_EQ(WGPUTextureSampleType_UnfilterableFloat, g_Bindings[2][0].texture.sampleType);
}

// Optional float32 filtering must preserve the default layouts and avoid extra variants.
TEST_F(WebGPUTextureLayoutTest, Float32FilteringFeaturePreservesDefaultLayout)
{
    m_Context->m_Float32Filterable = 1;
    Initialize();
    WebGPUUpdateTextureBindingLayouts(m_Context);
    ASSERT_EQ(0u, m_Program.m_LayoutIndex);
    ASSERT_EQ(1u, m_Program.m_Layouts.Size());
    ASSERT_EQ(WGPUTextureSampleType_Float, g_Bindings[0][0].texture.sampleType);
    ASSERT_EQ(WGPUSamplerBindingType_Filtering, g_Bindings[1][31].sampler.type);
}

// R32F and RG32F fragment textures need the same fallback as the RGBA32F vertex cache.
TEST_F(WebGPUTextureLayoutTest, Float32FormatsAndFragmentStage)
{
    m_Program.m_BaseProgram.m_ShaderMeta.m_Textures[0].m_StageFlags = dmGraphics::SHADER_STAGE_FLAG_FRAGMENT;
    m_Program.m_BaseProgram.m_ShaderMeta.m_Textures[1].m_StageFlags = dmGraphics::SHADER_STAGE_FLAG_FRAGMENT;
    Initialize();
    const WGPUTextureFormat formats[] = { WGPUTextureFormat_R32Float, WGPUTextureFormat_RG32Float, WGPUTextureFormat_RGBA32Float };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(formats); ++i)
    {
        m_Textures[0].m_Format = formats[i];
        WebGPUUpdateTextureBindingLayouts(m_Context);
        ASSERT_EQ(1u, m_Program.m_LayoutIndex);
        ASSERT_EQ(2u, m_Program.m_Layouts.Size());
        ASSERT_EQ(WGPUTextureSampleType_UnfilterableFloat, g_Bindings[2][0].texture.sampleType);
    }
}

// Normalized and half-float textures must retain their filtering layouts.
TEST_F(WebGPUTextureLayoutTest, FilterableFormatsPreserveDefaultLayout)
{
    Initialize();
    const WGPUTextureFormat formats[] = { WGPUTextureFormat_RGBA8Unorm, WGPUTextureFormat_RGBA16Float };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(formats); ++i)
    {
        m_Textures[0].m_Format = formats[i];
        WebGPUUpdateTextureBindingLayouts(m_Context);
        ASSERT_EQ(0u, m_Program.m_LayoutIndex);
        ASSERT_EQ(1u, m_Program.m_Layouts.Size());
    }
}

// Rebuilding layouts must retain SetSampler remapping for both the texture and its sampler.
TEST_F(WebGPUTextureLayoutTest, SamplerRemappingSurvivesLayoutCreation)
{
    Initialize();
    m_Context->m_CurrentTextureUnits[5] = &m_Textures[0];
    WebGPUSetSampler((dmGraphics::HContext)m_Context, 1 | ((uint64_t)31 << 16), 5);
    m_Context->m_CurrentTextureUnits[0] = &m_Textures[1];
    WebGPUUpdateTextureBindingLayouts(m_Context);
    ASSERT_EQ(5u, m_Program.m_BaseProgram.m_ResourceBindings[0][0].m_TextureUnit);
    ASSERT_EQ(1u << 0, WebGPUGetProgramLayout(&m_Program).m_UnfilterableBindings[0]);
    ASSERT_EQ(1u << 31, WebGPUGetProgramLayout(&m_Program).m_UnfilterableBindings[1]);
}

// Returning to an existing texture combination must reuse layouts and invalidate borrowed groups.
TEST_F(WebGPUTextureLayoutTest, LayoutVariantsAreReused)
{
    Initialize();
    const WGPUPipelineLayout original = WebGPUGetProgramLayout(&m_Program).m_PipelineLayout;
    WebGPUUpdateTextureBindingLayouts(m_Context);
    const WGPUPipelineLayout unfilterable = WebGPUGetProgramLayout(&m_Program).m_PipelineLayout;
    ASSERT_NE(original, unfilterable);
    for (uint32_t i = 0; i < 20; ++i)
    {
        m_Textures[0].m_Format = i % 2 ? WGPUTextureFormat_RGBA32Float : WGPUTextureFormat_RGBA8Unorm;
        m_Program.m_BindGroupIndices[0] = 123;
        WebGPUUpdateTextureBindingLayouts(m_Context);
        ASSERT_EQ(i % 2 ? unfilterable : original, WebGPUGetProgramLayout(&m_Program).m_PipelineLayout);
        ASSERT_EQ(dmGraphics::INVALID_BIND_GROUP_INDEX, m_Program.m_BindGroupIndices[0]);
    }

    ASSERT_EQ(2u, m_Program.m_Layouts.Size());
    ASSERT_EQ(2u, g_PipelineLayoutCount);
}

// Independent float32 bindings need distinct variants and must survive cache-array growth.
TEST_F(WebGPUTextureLayoutTest, MixedTextureCombinationsAreCached)
{
    Initialize();
    uint32_t groups[4] = {};
    for (uint32_t pass = 0; pass < 8; ++pass)
    {
        const uint32_t combination = pass % 4;
        m_Textures[0].m_Format = combination & 1 ? WGPUTextureFormat_RGBA32Float : WGPUTextureFormat_RGBA8Unorm;
        m_Textures[1].m_Format = combination & 2 ? WGPUTextureFormat_RGBA32Float : WGPUTextureFormat_RGBA8Unorm;
        WebGPUUpdateBindGroups(m_Context);
        const dmGraphics::WebGPUProgramLayout& layout = WebGPUGetProgramLayout(&m_Program);
        ASSERT_EQ(combination, layout.m_UnfilterableBindings[0]);
        ASSERT_EQ(((combination & 1) ? 1u << 31 : 0) | ((combination & 2) ? 1u : 0), layout.m_UnfilterableBindings[1]);
        if (pass < 4)
        {
            groups[combination] = m_Program.m_BindGroupIndices[0];
            for (uint32_t previous = 0; previous < combination; ++previous)
            {
                ASSERT_NE(groups[previous], groups[combination]);
            }
        }
        else
        {
            ASSERT_EQ(groups[combination], m_Program.m_BindGroupIndices[0]);
        }
    }

    ASSERT_EQ(4u, m_Program.m_Layouts.Size());
    ASSERT_EQ(4u, g_PipelineLayoutCount);
    ASSERT_EQ(8u, g_GroupCount);
}

// Pipeline and bind-group caches must distinguish layout variants and reuse each independently.
TEST_F(WebGPUTextureLayoutTest, CachesUseTheSelectedLayout)
{
    Initialize();
    m_Target.m_Base.m_Id = dmGraphics::DM_RENDERTARGET_BACKBUFFER_ID;
    m_Target.m_Multisample = 1;
    m_Context->m_CurrentRenderPass.m_Target = &m_Target;
    m_Context->m_CurrentPipelineState = dmGraphics::GetDefaultPipelineState();
    m_Program.m_VertexModule = &m_Vertex;
    m_Program.m_FragmentModule = &m_Fragment;
    WebGPUUpdateBindGroups(m_Context);
    const uint32_t first_group = m_Program.m_BindGroupIndices[0];
    const WGPURenderPipeline first_pipeline = WebGPUGetOrCreateRenderPipeline(m_Context);
    m_Textures[0].m_Format = WGPUTextureFormat_RGBA8Unorm;
    WebGPUUpdateBindGroups(m_Context);
    ASSERT_NE(first_group, m_Program.m_BindGroupIndices[0]);
    ASSERT_NE(first_pipeline, WebGPUGetOrCreateRenderPipeline(m_Context));
    m_Textures[0].m_Format = WGPUTextureFormat_RGBA32Float;
    WebGPUUpdateBindGroups(m_Context);
    ASSERT_EQ(first_group, m_Program.m_BindGroupIndices[0]);
    ASSERT_EQ(first_pipeline, WebGPUGetOrCreateRenderPipeline(m_Context));
    ASSERT_EQ(2u, g_PipelineCount);
}

// A texture-layout variant must not rebuild uniform layouts, offsets or stored constants.
TEST_F(WebGPUTextureLayoutTest, UniformStateSurvivesLayoutCreation)
{
    dmGraphics::ShaderMeta& meta = m_Program.m_BaseProgram.m_ShaderMeta;
    dmGraphics::ShaderResourceTypeInfo type = {};
    type.m_Name = strdup("Parameters");
    type.m_MemberCount = 1;
    type.m_Members = new dmGraphics::ShaderResourceMember[1]();
    type.m_Members[0].m_Name = strdup("tint");
    type.m_Members[0].m_NameHash = dmHashString64("tint");
    type.m_Members[0].m_Type.m_ShaderType = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
    type.m_Members[0].m_ElementCount = 1;
    meta.m_TypeInfos.SetCapacity(1);
    meta.m_TypeInfos.Push(type);
    meta.m_UniformBuffers.SetCapacity(2);
    for (uint32_t i = 0; i < 2; ++i)
    {
        dmGraphics::ShaderResourceBinding resource = {};
        resource.m_Name = strdup(i ? "second" : "first");
        resource.m_NameHash = dmHashString64(resource.m_Name);
        resource.m_BindingFamily = dmGraphics::BINDING_FAMILY_UNIFORM_BUFFER;
        resource.m_Type.m_UseTypeIndex = 1;
        resource.m_Type.m_TypeIndex = 0;
        resource.m_Set = 2;
        resource.m_Binding = i;
        resource.m_BindingInfo.m_BlockSize = 16;
        resource.m_StageFlags = dmGraphics::SHADER_STAGE_FLAG_VERTEX;
        meta.m_UniformBuffers.Push(resource);
    }

    Initialize();
    uint8_t expected[32];
    memset(expected, 0x5c, sizeof(expected));
    memcpy(m_Program.m_UniformData, expected, sizeof(expected));
    void* layout = m_Program.m_BaseProgram.m_ResourceBindings[2][1].m_BindingUserData;
    WebGPUUpdateTextureBindingLayouts(m_Context);
    ASSERT_EQ(2u, m_Program.m_BaseProgram.m_UniformBufferLayouts.Size());
    ASSERT_EQ(layout, m_Program.m_BaseProgram.m_ResourceBindings[2][1].m_BindingUserData);
    ASSERT_EQ(16u, m_Program.m_BaseProgram.m_ResourceBindings[2][1].m_UniformBufferOffset);
    ASSERT_EQ(0, memcmp(expected, m_Program.m_UniformData, sizeof(expected)));
}

// Changing a cube texture's sample type must preserve its cube view dimension.
TEST_F(WebGPUTextureLayoutTest, CubeViewDimensionIsPreserved)
{
    m_Program.m_BaseProgram.m_ShaderMeta.m_Textures[0].m_Type.m_ShaderType = dmGraphics::ShaderDesc::SHADER_TYPE_TEXTURE_CUBE;
    Initialize();
    WebGPUUpdateTextureBindingLayouts(m_Context);
    ASSERT_EQ(WGPUTextureViewDimension_Cube, g_Bindings[2][0].texture.viewDimension);
    ASSERT_EQ(WGPUTextureSampleType_UnfilterableFloat, g_Bindings[2][0].texture.sampleType);
}

// Changing an array texture's sample type must preserve its array view dimension.
TEST_F(WebGPUTextureLayoutTest, ArrayViewDimensionIsPreserved)
{
    m_Program.m_BaseProgram.m_ShaderMeta.m_Textures[0].m_Type.m_ShaderType = dmGraphics::ShaderDesc::SHADER_TYPE_TEXTURE2D_ARRAY;
    Initialize();
    WebGPUUpdateTextureBindingLayouts(m_Context);
    ASSERT_EQ(WGPUTextureViewDimension_2DArray, g_Bindings[2][0].texture.viewDimension);
    ASSERT_EQ(WGPUTextureSampleType_UnfilterableFloat, g_Bindings[2][0].texture.sampleType);
}

// Deleting or reloading a program must release every native layout exactly once.
TEST_F(WebGPUTextureLayoutTest, AllLayoutVariantsAreReleased)
{
    Initialize();
    WebGPUUpdateTextureBindingLayouts(m_Context);
    WebGPUDestroyProgram(m_Context, &m_Program);
    ASSERT_EQ(0u, m_Program.m_Layouts.Size());
    ASSERT_EQ(4u, g_ReleasedLayoutCount);
    ASSERT_EQ(2u, g_ReleasedPipelineLayoutCount);
}

// Integer and storage-image declarations must not become floating-point sampled bindings.
TEST_F(WebGPUTextureLayoutTest, IntegerAndStorageBindingsAreUnchanged)
{
    m_Program.m_BaseProgram.m_ShaderMeta.m_Textures[0].m_Type.m_ShaderType = dmGraphics::ShaderDesc::SHADER_TYPE_UTEXTURE2D;
    m_Program.m_BaseProgram.m_ShaderMeta.m_Textures[2].m_Type.m_ShaderType = dmGraphics::ShaderDesc::SHADER_TYPE_IMAGE2D;
    m_Textures[1].m_Format = WGPUTextureFormat_RGBA32Float;
    Initialize();
    WebGPUUpdateTextureBindingLayouts(m_Context);
    ASSERT_EQ(1u, m_Program.m_Layouts.Size());
    ASSERT_EQ(WGPUTextureSampleType_Uint, g_Bindings[0][0].texture.sampleType);
    ASSERT_EQ(WGPUTextureFormat_RGBA32Float, g_Bindings[0][1].storageTexture.format);
}

// Compute shaders must keep their existing non-filtering layouts without redundant variants.
TEST_F(WebGPUTextureLayoutTest, ComputeLayoutsAreUnchanged)
{
    dmArray<dmGraphics::ShaderResourceBinding>& resources = m_Program.m_BaseProgram.m_ShaderMeta.m_Textures;
    for (uint32_t i = 0; i < resources.Size(); ++i)
    {
        resources[i].m_StageFlags = dmGraphics::SHADER_STAGE_FLAG_COMPUTE;
    }

    Initialize();
    WebGPUUpdateTextureBindingLayouts(m_Context);
    ASSERT_EQ(1u, m_Program.m_Layouts.Size());
    ASSERT_EQ(WGPUTextureSampleType_UnfilterableFloat, g_Bindings[0][0].texture.sampleType);
    ASSERT_EQ(WGPUSamplerBindingType_NonFiltering, g_Bindings[1][31].sampler.type);
}

#if defined(DM_GRAPHICS_DAWN)
static void CaptureAdapter(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata, void*)
{
    *(WGPUAdapter*)userdata = adapter;
}

static void CaptureDevice(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata, void*)
{
    *(WGPUDevice*)userdata = device;
}

struct ValidationResult
{
    WGPUPopErrorScopeStatus m_Status;
    WGPUErrorType           m_Type;
};

static void CaptureValidation(WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView message, void* userdata, void*)
{
    ValidationResult* result = (ValidationResult*)userdata;
    result->m_Status = status;
    result->m_Type = type;
    if (type != WGPUErrorType_NoError)
    {
        fprintf(stderr, "WebGPU validation: %.*s\n", message.data ? (int)message.length : 0, message.data);
    }
}

static void ReadNativePixels(WebGPUTextureLayoutTest* fixture, WGPUCommandEncoder encoder, WGPUTexture texture,
                             WGPUTextureAspect aspect, uint32_t width, uint8_t* pixels, uint32_t size)
{
    WGPUTexelCopyTextureInfo source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    source.texture = texture;
    source.aspect = aspect;
    WGPUTexelCopyBufferInfo destination = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
    destination.buffer = fixture->m_ReadbackBuffer;
    destination.layout.bytesPerRow = 256;
    destination.layout.rowsPerImage = 1;
    WGPUExtent3D extent = { width, 1, 1 };
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &source, &destination, &extent);
    WGPUCommandBufferDescriptor command_desc = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &command_desc);
    wgpuCommandEncoderRelease(encoder);
    wgpuQueueSubmit(fixture->m_Context->m_Queue, 1, &command);
    wgpuCommandBufferRelease(command);

    WebGPUReadback readback = {};
    WGPUBufferMapCallbackInfo callback = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    callback.mode = WGPUCallbackMode_WaitAnyOnly;
    callback.callback = WebGPUReadbackMapped;
    callback.userdata1 = &readback;
    WGPUFutureWaitInfo wait = {};
    wait.future = wgpuBufferMapAsync(fixture->m_ReadbackBuffer, WGPUMapMode_Read, 0, 256, callback);
    ASSERT_EQ(WGPUWaitStatus_Success, wgpuInstanceWaitAny(fixture->m_Instance, 1, &wait, UINT64_MAX));
    ASSERT_TRUE(readback.m_Success);
    const void* mapped = wgpuBufferGetConstMappedRange(fixture->m_ReadbackBuffer, 0, 256);
    ASSERT_NE((const void*)0, mapped);
    memcpy(pixels, mapped, size);
    wgpuBufferUnmap(fixture->m_ReadbackBuffer);
}

static void DrawNativePixel(WebGPUTextureLayoutTest* fixture, WGPURenderPipeline pipeline, uint8_t pixel[4])
{
    WGPUCommandEncoderDescriptor encoder_desc = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(fixture->m_Context->m_Device, &encoder_desc);
    WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    color.view = fixture->m_Textures[3].m_TextureView;
    color.loadOp = WGPULoadOp_Clear;
    color.storeOp = WGPUStoreOp_Store;
    WGPURenderPassDescriptor pass_desc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    pass_desc.colorAttachmentCount = 1;
    pass_desc.colorAttachments = &color;
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &pass_desc);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    for (uint32_t set = 0; set < fixture->m_Program.m_BaseProgram.m_MaxSet; ++set)
    {
        const uint32_t index = fixture->m_Program.m_BindGroupIndices[set];
        wgpuRenderPassEncoderSetBindGroup(pass, set, fixture->m_Context->m_BindGroupPool->Get(index).m_BindGroup, 0, 0);
    }

    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    ReadNativePixels(fixture, encoder, fixture->m_Textures[3].m_Texture, WGPUTextureAspect_All, 1, pixel, 4);
}

static void InitializeNativeDevice(WebGPUTextureLayoutTest* fixture)
{
    WGPUInstanceDescriptor instance_desc = WGPU_INSTANCE_DESCRIPTOR_INIT;
    WGPUInstanceFeatureName instance_feature = WGPUInstanceFeatureName_TimedWaitAny;
    instance_desc.requiredFeatures = &instance_feature;
    instance_desc.requiredFeatureCount = 1;
    fixture->m_Instance = wgpuCreateInstance(&instance_desc);
    ASSERT_NE((WGPUInstance)0, fixture->m_Instance);
    WGPURequestAdapterCallbackInfo adapter_callback = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    adapter_callback.mode = WGPUCallbackMode_WaitAnyOnly;
    adapter_callback.callback = CaptureAdapter;
    adapter_callback.userdata1 = &fixture->m_Adapter;
    WGPUFutureWaitInfo wait = {};
    wait.future = wgpuInstanceRequestAdapter(fixture->m_Instance, 0, adapter_callback);
    ASSERT_EQ(WGPUWaitStatus_Success, wgpuInstanceWaitAny(fixture->m_Instance, 1, &wait, UINT64_MAX));
    if (!fixture->m_Adapter)
    {
        return;
    }

    WGPUDeviceDescriptor device_desc = WGPU_DEVICE_DESCRIPTOR_INIT;
    WGPURequestDeviceCallbackInfo device_callback = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    device_callback.mode = WGPUCallbackMode_WaitAnyOnly;
    device_callback.callback = CaptureDevice;
    dmGraphics::WebGPUContext* context = fixture->m_Context;
    context->m_Instance = fixture->m_Instance;
    device_callback.userdata1 = &context->m_Device;
    wait.future = wgpuAdapterRequestDevice(fixture->m_Adapter, &device_desc, device_callback);
    ASSERT_EQ(WGPUWaitStatus_Success, wgpuInstanceWaitAny(fixture->m_Instance, 1, &wait, UINT64_MAX));
    ASSERT_NE((WGPUDevice)0, context->m_Device);

    for (uint32_t i = 0; i < DM_ARRAY_SIZE(fixture->m_Textures); ++i)
    {
        fixture->m_Textures[i].m_TextureView = 0;
        fixture->m_Textures[i].m_Sampler = 0;
    }

    g_NativeDevice = true;
    context->m_Queue = wgpuDeviceGetQueue(context->m_Device);
    wgpuDevicePushErrorScope(context->m_Device, WGPUErrorFilter_Validation);
    WGPUBufferDescriptor buffer_desc = WGPU_BUFFER_DESCRIPTOR_INIT;
    buffer_desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    buffer_desc.size = 256;
    fixture->m_ReadbackBuffer = wgpuDeviceCreateBuffer(context->m_Device, &buffer_desc);
}

static void AssertNoNativeValidationErrors(WebGPUTextureLayoutTest* fixture)
{
    ValidationResult result = {};
    WGPUPopErrorScopeCallbackInfo callback = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;
    callback.mode = WGPUCallbackMode_WaitAnyOnly;
    callback.callback = CaptureValidation;
    callback.userdata1 = &result;
    WGPUFutureWaitInfo wait = {};
    wait.future = wgpuDevicePopErrorScope(fixture->m_Context->m_Device, callback);
    ASSERT_EQ(WGPUWaitStatus_Success, wgpuInstanceWaitAny(fixture->m_Instance, 1, &wait, UINT64_MAX));
    ASSERT_EQ(WGPUPopErrorScopeStatus_Success, result.m_Status);
    ASSERT_EQ(WGPUErrorType_NoError, result.m_Type);
}

static WGPUShaderModule CreateNativeShader(WGPUDevice device, const char* source)
{
    WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgsl.code.data = source;
    wgsl.code.length = strlen(source);
    WGPUShaderModuleDescriptor desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    desc.nextInChain = &wgsl.chain;
    return wgpuDeviceCreateShaderModule(device, &desc);
}

// Real vertex float32 sampling and linear color filtering must draw correctly and reuse pipelines across layout switches.
TEST_F(WebGPUTextureLayoutTest, NativeDeviceWithoutFloat32Filtering)
{
    InitializeNativeDevice(this);
    if (!m_Adapter)
    {
        SKIP();
    }

    ASSERT_FALSE(wgpuDeviceHasFeature(m_Context->m_Device, WGPUFeatureName_Float32Filterable));

    const float float_bone[] = { 0.25f, 1.0f, 1.0f, 1.0f };
    const uint8_t normalized_bone[] = { 128, 255, 255, 255 };
    const uint8_t colors[] = { 0, 0, 0, 255, 255, 0, 0, 255 };
    const void* texture_data[] = { float_bone, colors, normalized_bone };
    m_Textures[2].m_Format = WGPUTextureFormat_RGBA8Unorm;
    for (uint32_t i = 0; i < 3; ++i)
    {
        WGPUTextureDescriptor texture_desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
        texture_desc.format = m_Textures[i].m_Format;
        texture_desc.size.width = i == 1 ? 2 : 1;
        texture_desc.size.height = 1;
        texture_desc.size.depthOrArrayLayers = 1;
        texture_desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
        m_Textures[i].m_Texture = wgpuDeviceCreateTexture(m_Context->m_Device, &texture_desc);
        m_Textures[i].m_TextureView = wgpuTextureCreateView(m_Textures[i].m_Texture, 0);
        WGPUSamplerDescriptor sampler_desc = WGPU_SAMPLER_DESCRIPTOR_INIT;
        sampler_desc.minFilter = i == 1 ? WGPUFilterMode_Linear : WGPUFilterMode_Nearest;
        sampler_desc.magFilter = i == 1 ? WGPUFilterMode_Linear : WGPUFilterMode_Nearest;
        m_Textures[i].m_Sampler = wgpuDeviceCreateSampler(m_Context->m_Device, &sampler_desc);

        WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
        destination.texture = m_Textures[i].m_Texture;
        WGPUTexelCopyBufferLayout layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
        layout.bytesPerRow = i == 0 ? sizeof(float_bone) : texture_desc.size.width * 4;
        layout.rowsPerImage = 1;
        wgpuQueueWriteTexture(m_Context->m_Queue, &destination, texture_data[i], layout.bytesPerRow, &layout, &texture_desc.size);
    }

    Initialize();
    WebGPUUpdateBindGroups(m_Context);

    const char* vertex_source =
        "@group(0) @binding(0) var bones: texture_2d<f32>;\n"
        "@group(1) @binding(31) var bones_sampler: sampler;\n"
        "struct Output { @builtin(position) position: vec4<f32>, @location(0) bone: vec4<f32> };\n"
        "@vertex fn main(@builtin(vertex_index) index: u32) -> Output {\n"
        "  let positions = array<vec2<f32>, 3>(vec2<f32>(-1, -1), vec2<f32>(3, -1), vec2<f32>(-1, 3));\n"
        "  var output: Output;\n"
        "  output.position = vec4<f32>(positions[index], 0, 1);\n"
        "  output.bone = textureSampleLevel(bones, bones_sampler, vec2<f32>(0.5), 0.0);\n"
        "  return output;\n"
        "}\n";
    const char* fragment_source =
        "@group(0) @binding(1) var color: texture_2d<f32>;\n"
        "@group(1) @binding(0) var color_sampler: sampler;\n"
        "@fragment fn main(@location(0) bone: vec4<f32>) -> @location(0) vec4<f32> {\n"
        "  return bone * textureSampleLevel(color, color_sampler, vec2<f32>(0.5), 0.0);\n"
        "}\n";
    m_Vertex.m_Module = CreateNativeShader(m_Context->m_Device, vertex_source);
    m_Fragment.m_Module = CreateNativeShader(m_Context->m_Device, fragment_source);
    m_Program.m_VertexModule = &m_Vertex;
    m_Program.m_FragmentModule = &m_Fragment;

    WGPUTextureDescriptor output_desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
    output_desc.format = WGPUTextureFormat_RGBA8Unorm;
    output_desc.size.width = 1;
    output_desc.size.height = 1;
    output_desc.size.depthOrArrayLayers = 1;
    output_desc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    m_Textures[3].m_Format = output_desc.format;
    m_Textures[3].m_Texture = wgpuDeviceCreateTexture(m_Context->m_Device, &output_desc);
    m_Textures[3].m_TextureView = wgpuTextureCreateView(m_Textures[3].m_Texture, 0);
    m_Target.m_Base.m_Id = dmGraphics::DM_RENDERTARGET_BACKBUFFER_ID;
    m_Target.m_Base.m_ColorAttachmentCount = 1;
    m_Target.m_Multisample = 1;
    m_Target.m_TextureColor[0] = dmGraphics::StoreAssetInContainer(m_Context->m_BaseContext.m_AssetHandleContainer, &m_Textures[3], dmGraphics::ASSET_TYPE_TEXTURE);
    m_Context->m_CurrentRenderPass.m_Target = &m_Target;
    m_Context->m_CurrentPipelineState = dmGraphics::GetDefaultPipelineState();
    m_Context->m_CurrentPipelineState.m_PrimtiveType = dmGraphics::PRIMITIVE_TRIANGLES;
    uint8_t pixels[3][4];
    WGPURenderPipeline pipelines[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        m_Context->m_CurrentTextureUnits[0] = &m_Textures[i == 1 ? 0 : 2];
        WebGPUUpdateBindGroups(m_Context);
        pipelines[i] = WebGPUGetOrCreateRenderPipeline(m_Context);
        DrawNativePixel(this, pipelines[i], pixels[i]);
    }

    AssertNoNativeValidationErrors(this);
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_NEAR(i == 1 ? 32 : 64, pixels[i][0], 1);
        ASSERT_EQ(0u, pixels[i][1]);
        ASSERT_EQ(0u, pixels[i][2]);
        ASSERT_EQ(255u, pixels[i][3]);
    }

    ASSERT_NE(pipelines[0], pipelines[1]);
    ASSERT_EQ(pipelines[0], pipelines[2]);
    ASSERT_EQ(2u, g_PipelineCount);
}

static dmGraphics::HContext BeginNativeBackbuffer(WebGPUTextureLayoutTest* fixture, uint32_t width, uint8_t sample_count)
{
    dmGraphics::WebGPUContext* context = fixture->m_Context;
    const WGPUTextureFormat formats[] = { WGPUTextureFormat_RGBA8Unorm, WGPUTextureFormat_Depth24PlusStencil8, WGPUTextureFormat_RGBA8Unorm };
    const uint32_t texture_count = sample_count > 1 ? 3 : 2;
    for (uint32_t i = 0; i < texture_count; ++i)
    {
        WGPUTextureDescriptor desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
        desc.format = formats[i];
        desc.size = { width, 1, 1 };
        desc.sampleCount = i < 2 ? sample_count : 1;
        desc.usage = WGPUTextureUsage_RenderAttachment;
        if (desc.sampleCount == 1)
        {
            desc.usage |= WGPUTextureUsage_CopySrc;
        }

        dmGraphics::WebGPUTexture& texture = fixture->m_Textures[i];
        texture.m_Format = desc.format;
        texture.m_Texture = wgpuDeviceCreateTexture(context->m_Device, &desc);
        texture.m_TextureView = wgpuTextureCreateView(texture.m_Texture, 0);
    }

    const uint32_t attachments = dmGraphics::BUFFER_TYPE_COLOR0_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT | dmGraphics::BUFFER_TYPE_STENCIL_BIT;
    dmGraphics::WebGPURenderTarget& target = fixture->m_Target;
    target.m_Base.m_Id = dmGraphics::DM_RENDERTARGET_BACKBUFFER_ID;
    target.m_Base.m_ColorAttachmentCount = 1;
    target.m_Base.m_SampleCount = sample_count;
    target.m_BufferTypeFlags = attachments;
    target.m_Multisample = sample_count;
    target.m_Width = width;
    target.m_Height = 1;
    target.m_ColorBufferTypes[0] = dmGraphics::BUFFER_TYPE_COLOR0_BIT;
    target.m_TextureColor[0] = dmGraphics::StoreAssetInContainer(context->m_BaseContext.m_AssetHandleContainer, &fixture->m_Textures[0], dmGraphics::ASSET_TYPE_TEXTURE);
    target.m_TextureDepthStencil = dmGraphics::StoreAssetInContainer(context->m_BaseContext.m_AssetHandleContainer, &fixture->m_Textures[1], dmGraphics::ASSET_TYPE_TEXTURE);
    if (sample_count > 1)
    {
        target.m_TextureResolve[0] = dmGraphics::StoreAssetInContainer(context->m_BaseContext.m_AssetHandleContainer, &fixture->m_Textures[2], dmGraphics::ASSET_TYPE_TEXTURE);
    }

    context->m_Format = formats[0];
    context->m_MainRenderTarget = &target;
    context->m_CurrentRenderTarget = &target;
    context->m_ApplyRenderTargetLoadOps = 1;
    context->m_CurrentPipelineState = dmGraphics::GetDefaultPipelineState();
    dmGraphics::HContext graphics_context = &context->m_BaseContext;
    WebGPUSetViewport(graphics_context, 0, 0, width, 1);
    WebGPUClear(graphics_context, attachments, 16, 32, 64, 255, 0.75f, 3);
    return graphics_context;
}

static void InitializeNativeDrawProgram(WebGPUTextureLayoutTest* fixture, const char* fragment_source)
{
    static const char vertex_source[] =
        "@vertex fn main(@builtin(vertex_index) index: u32) -> @builtin(position) vec4<f32> {\n"
        "  let positions = array<vec2<f32>, 3>(vec2<f32>(-1, -1), vec2<f32>(3, -1), vec2<f32>(-1, 3));\n"
        "  return vec4<f32>(positions[index], 0.5, 1.0);\n"
        "}\n";
    fixture->m_Vertex.m_Module = CreateNativeShader(fixture->m_Context->m_Device, vertex_source);
    fixture->m_Fragment.m_Module = CreateNativeShader(fixture->m_Context->m_Device, fragment_source);
    fixture->m_Program.m_VertexModule = &fixture->m_Vertex;
    fixture->m_Program.m_FragmentModule = &fixture->m_Fragment;
    fixture->Initialize();
}

static bool WaitForStencilClearPipeline(dmGraphics::WebGPUContext* context)
{
    WGPUFutureWaitInfo wait = { context->m_StencilClearPipelineRequest->m_Future, 0 };
    return wgpuInstanceWaitAny(context->m_Instance, 1, &wait, UINT64_MAX) == WGPUWaitStatus_Success && wait.completed;
}

static WGPUCommandEncoder EndNativeBackbuffer(dmGraphics::WebGPUContext* context)
{
    WebGPUEndRenderPass(context);
    WGPUCommandEncoder encoder = context->m_CommandEncoder;
    context->m_CommandEncoder = 0;
    return encoder;
}

// Verify full-target stencil replacement and that the next draw restores its state in the same pass.
TEST_F(WebGPUTextureLayoutTest, StencilOnlyClearReusesBackbufferPass)
{
    InitializeNativeDevice(this);
    if (!m_Adapter)
    {
        SKIP();
    }

    dmGraphics::HContext context = BeginNativeBackbuffer(this, 4, 1);
    ASSERT_EQ((WGPURenderPipeline)0, WebGPUGetOrCreateStencilClearPipeline(m_Context, m_Textures[1].m_Format, 1));
    ASSERT_NE((dmGraphics::WebGPUStencilClearPipelineRequest*)0, m_Context->m_StencilClearPipelineRequest);
    ASSERT_TRUE(WaitForStencilClearPipeline(m_Context));
    ASSERT_NE((WGPURenderPipeline)0, WebGPUGetOrCreateStencilClearPipeline(m_Context, m_Textures[1].m_Format, 1));

    dmGraphics::DestroyProgram(&m_Program.m_BaseProgram);
    InitializeNativeDrawProgram(this, "@fragment fn main() -> @location(0) vec4<f32> { return vec4<f32>(1, 0, 0, 1); }");
    // The viewport and scissor overlap only on the second pixel.
    WebGPUSetViewport(context, 0, 0, 2, 1);
    m_Target.m_Scissor[0] = 1;
    m_Target.m_Scissor[2] = 2;
    WebGPUEnableState(context, dmGraphics::STATE_STENCIL_TEST);
    WebGPUSetStencilFunc(context, dmGraphics::COMPARE_FUNC_NOTEQUAL, 3, 0xff);
    WebGPUSetStencilMask(context, 0);
    WebGPUEnableState(context, dmGraphics::STATE_DEPTH_TEST);
    WebGPUSetDepthMask(context, false);
    WebGPUDraw(context, dmGraphics::PRIMITIVE_TRIANGLES, 0, 3, 1);

    const WGPURenderPassEncoder pass = m_Context->m_CurrentRenderPass.m_Encoder;
    const WGPURenderPipeline draw_pipeline = m_Context->m_CurrentRenderPass.m_Pipeline;
    const uint32_t pass_count = m_Context->m_RenderPasses;
    ASSERT_NE((WGPURenderPassEncoder)0, pass);
    WebGPUClear(context, dmGraphics::BUFFER_TYPE_STENCIL_BIT, 0, 0, 0, 0, 0.0f, 19);
    WebGPUClear(context, dmGraphics::BUFFER_TYPE_STENCIL_BIT, 0, 0, 0, 0, 0.0f, 0x19d);
    WebGPUDraw(context, dmGraphics::PRIMITIVE_TRIANGLES, 0, 3, 1);
    ASSERT_EQ(pass, m_Context->m_CurrentRenderPass.m_Encoder);
    ASSERT_EQ(pass_count, m_Context->m_RenderPasses);
    ASSERT_EQ(draw_pipeline, m_Context->m_CurrentRenderPass.m_Pipeline);
    ASSERT_EQ(2u, g_PipelineCount);

    uint8_t pixels[16] = {};
    ReadNativePixels(this, EndNativeBackbuffer(m_Context), m_Textures[0].m_Texture, WGPUTextureAspect_All, 4, pixels, sizeof(pixels));
    uint8_t stencil[4] = {};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_Context->m_Device, 0);
    ReadNativePixels(this, encoder, m_Textures[1].m_Texture, WGPUTextureAspect_StencilOnly, 4, stencil, sizeof(stencil));
    AssertNoNativeValidationErrors(this);
    const uint8_t expected_color[] = { 16, 32, 64, 255, 255, 0, 0, 255, 16, 32, 64, 255, 16, 32, 64, 255 };
    const uint8_t expected_stencil[] = { 157, 157, 157, 157 };
    ASSERT_EQ(0, memcmp(expected_color, pixels, sizeof(expected_color)));
    ASSERT_EQ(0, memcmp(expected_stencil, stencil, sizeof(expected_stencil)));
}

// Verify MSAA clears all four stencil samples in the same pass with a separate cached pipeline.
TEST_F(WebGPUTextureLayoutTest, StencilOnlyClearReusesMultisampledBackbufferPass)
{
    InitializeNativeDevice(this);
    if (!m_Adapter)
    {
        SKIP();
    }

    dmGraphics::HContext context = BeginNativeBackbuffer(this, 2, 4);
    const uint32_t sample_counts[] = { 1, 4 };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(sample_counts); ++i)
    {
        const uint32_t sample_count = sample_counts[i];
        ASSERT_EQ((WGPURenderPipeline)0, WebGPUGetOrCreateStencilClearPipeline(m_Context, m_Textures[1].m_Format, sample_count));
        ASSERT_NE((dmGraphics::WebGPUStencilClearPipelineRequest*)0, m_Context->m_StencilClearPipelineRequest);
        ASSERT_TRUE(WaitForStencilClearPipeline(m_Context));
        ASSERT_NE((WGPURenderPipeline)0, WebGPUGetOrCreateStencilClearPipeline(m_Context, m_Textures[1].m_Format, sample_count));
    }

    dmGraphics::DestroyProgram(&m_Program.m_BaseProgram);
    InitializeNativeDrawProgram(this, "@fragment fn main() -> @location(0) vec4<f32> { return vec4<f32>(1, 0, 0, 1); }");
    WebGPUSetViewport(context, 1, 0, 1, 1);
    WebGPUEnableState(context, dmGraphics::STATE_STENCIL_TEST);
    WebGPUSetStencilFunc(context, dmGraphics::COMPARE_FUNC_EQUAL, 23, 0xff);
    WebGPUSetStencilMask(context, 0);
    WebGPUEnableState(context, dmGraphics::STATE_DEPTH_TEST);
    WebGPUSetDepthMask(context, false);

    const WGPURenderPassEncoder pass = m_Context->m_CurrentRenderPass.m_Encoder;
    const uint32_t pass_count = m_Context->m_RenderPasses;
    WebGPUClear(context, dmGraphics::BUFFER_TYPE_STENCIL_BIT, 0, 0, 0, 0, 0.0f, 23);
    WebGPUDraw(context, dmGraphics::PRIMITIVE_TRIANGLES, 0, 3, 1);
    ASSERT_EQ(pass, m_Context->m_CurrentRenderPass.m_Encoder);
    ASSERT_EQ(pass_count, m_Context->m_RenderPasses);
    ASSERT_EQ(3u, g_PipelineCount);

    // The resolve is solid red only if every sample in the second pixel passed.
    uint8_t pixels[8] = {};
    ReadNativePixels(this, EndNativeBackbuffer(m_Context), m_Textures[2].m_Texture, WGPUTextureAspect_All, 2, pixels, sizeof(pixels));
    AssertNoNativeValidationErrors(this);
    const uint8_t expected_color[] = { 16, 32, 64, 255, 255, 0, 0, 255 };
    ASSERT_EQ(0, memcmp(expected_color, pixels, sizeof(expected_color)));
}

// A storage-capable texture can be sampled before the clear and bound for writing afterward.
TEST_F(WebGPUTextureLayoutTest, StencilClearSeparatesStorageTextureUsages)
{
    InitializeNativeDevice(this);
    if (!m_Adapter)
    {
        SKIP();
    }

    dmGraphics::HContext context = BeginNativeBackbuffer(this, 1, 1);
    ASSERT_EQ((WGPURenderPipeline)0, WebGPUGetOrCreateStencilClearPipeline(m_Context, m_Textures[1].m_Format, 1));
    ASSERT_NE((dmGraphics::WebGPUStencilClearPipelineRequest*)0, m_Context->m_StencilClearPipelineRequest);
    ASSERT_TRUE(WaitForStencilClearPipeline(m_Context));
    ASSERT_NE((WGPURenderPipeline)0, WebGPUGetOrCreateStencilClearPipeline(m_Context, m_Textures[1].m_Format, 1));

    dmGraphics::WebGPUTexture& texture = m_Textures[2];
    WGPUTextureDescriptor desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
    desc.format = WGPUTextureFormat_RGBA32Float;
    desc.size = { 1, 1, 1 };
    desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_StorageBinding;
    texture.m_Format = desc.format;
    texture.m_UsageFlags = desc.usage;
    texture.m_Texture = wgpuDeviceCreateTexture(m_Context->m_Device, &desc);
    texture.m_TextureView = wgpuTextureCreateView(texture.m_Texture, 0);

    dmGraphics::DestroyProgram(&m_Program.m_BaseProgram);
    m_Program.m_BaseProgram.m_ShaderMeta.m_Textures.SetCapacity(1);
    AddTexture("source", dmGraphics::ShaderDesc::SHADER_TYPE_TEXTURE2D, 0, 0, dmGraphics::SHADER_STAGE_FLAG_FRAGMENT, 0);
    m_Context->m_CurrentTextureUnits[0] = &texture;
    InitializeNativeDrawProgram(this, "@group(0) @binding(0) var source: texture_2d<f32>;\n"
                                "@fragment fn main() -> @location(0) vec4<f32> { return textureLoad(source, vec2<i32>(0), 0); }");
    WebGPUDraw(context, dmGraphics::PRIMITIVE_TRIANGLES, 0, 3, 1);
    const uint32_t pass_count = m_Context->m_RenderPasses;
    WebGPUClear(context, dmGraphics::BUFFER_TYPE_STENCIL_BIT, 0, 0, 0, 0, 0.0f, 11);

    // WebGPU tracks the storage usage of bindings even without a draw using them.
    WGPUBindGroupLayoutEntry layout_entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    layout_entry.visibility = WGPUShaderStage_Fragment;
    layout_entry.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
    layout_entry.storageTexture.format = desc.format;
    layout_entry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;
    WGPUBindGroupLayoutDescriptor layout_desc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    layout_desc.entryCount = 1;
    layout_desc.entries = &layout_entry;
    WGPUBindGroupLayout layout = wgpuDeviceCreateBindGroupLayout(m_Context->m_Device, &layout_desc);
    WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
    entry.textureView = texture.m_TextureView;
    WGPUBindGroupDescriptor group_desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    group_desc.layout = layout;
    group_desc.entryCount = 1;
    group_desc.entries = &entry;
    WGPUBindGroup group = wgpuDeviceCreateBindGroup(m_Context->m_Device, &group_desc);
    wgpuRenderPassEncoderSetBindGroup(m_Context->m_CurrentRenderPass.m_Encoder, 0, group, 0, 0);
    wgpuBindGroupRelease(group);
    wgpuBindGroupLayoutRelease(layout);

    uint8_t stencil = 0;
    ReadNativePixels(this, EndNativeBackbuffer(m_Context), m_Textures[1].m_Texture, WGPUTextureAspect_StencilOnly, 1, &stencil, sizeof(stencil));
    AssertNoNativeValidationErrors(this);
    ASSERT_EQ(pass_count + 1, m_Context->m_RenderPasses);
    ASSERT_EQ(11u, stencil);
}

// Verify both fallback clears write stencil and the rejection is cached.
TEST_F(WebGPUTextureLayoutTest, StencilClearFallsBackWhenPipelineCreationFails)
{
    InitializeNativeDevice(this);
    if (!m_Adapter)
    {
        SKIP();
    }

    dmGraphics::HContext context = BeginNativeBackbuffer(this, 1, 1);
    dmGraphics::DestroyProgram(&m_Program.m_BaseProgram);
    InitializeNativeDrawProgram(this, "@fragment fn main() -> @location(0) vec4<f32> { return vec4<f32>(1, 0, 0, 1); }");
    g_RejectStencilClearPipeline = true;
    const uint32_t pass_count = m_Context->m_RenderPasses;
    WebGPUClear(context, dmGraphics::BUFFER_TYPE_STENCIL_BIT, 0, 0, 0, 0, 0.0f, 17);
    ASSERT_EQ(pass_count + 1, m_Context->m_RenderPasses);
    ASSERT_NE((dmGraphics::WebGPUStencilClearPipelineRequest*)0, m_Context->m_StencilClearPipelineRequest);

    // Keep the first clear observable in color while leaving the restarted pass active.
    WebGPUEnableState(context, dmGraphics::STATE_STENCIL_TEST);
    WebGPUSetStencilFunc(context, dmGraphics::COMPARE_FUNC_EQUAL, 17, 0xff);
    WebGPUSetStencilMask(context, 0);
    WebGPUDraw(context, dmGraphics::PRIMITIVE_TRIANGLES, 0, 3, 1);

    ASSERT_TRUE(WaitForStencilClearPipeline(m_Context));
    WebGPUClear(context, dmGraphics::BUFFER_TYPE_STENCIL_BIT, 0, 0, 0, 0, 0.0f, 23);

    uint8_t color[4] = {};
    ReadNativePixels(this, EndNativeBackbuffer(m_Context), m_Textures[0].m_Texture, WGPUTextureAspect_All, 1, color, sizeof(color));
    const uint8_t expected_color[] = { 255, 0, 0, 255 };
    ASSERT_EQ(0, memcmp(expected_color, color, sizeof(expected_color)));

    uint8_t stencil = 0;
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_Context->m_Device, 0);
    ReadNativePixels(this, encoder, m_Textures[1].m_Texture, WGPUTextureAspect_StencilOnly, 1, &stencil, sizeof(stencil));
    AssertNoNativeValidationErrors(this);
    ASSERT_EQ(pass_count + 2, m_Context->m_RenderPasses);
    ASSERT_EQ(2u, g_PipelineCount);
    ASSERT_EQ(23u, stencil);
}
#endif

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
