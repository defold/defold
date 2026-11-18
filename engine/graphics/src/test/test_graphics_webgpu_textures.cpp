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
    return (WGPURenderPipeline)(uintptr_t)g_PipelineCount;
}

// Capture the production descriptors without exposing an engine test API.
// The native test forwards layout and bind-group creation to the real device.
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

struct WebGPUTextureLayoutTest : jc_test_base_class
{
    dmGraphics::WebGPUContext*     m_Context;
    dmGraphics::WebGPUProgram      m_Program;
    dmGraphics::WebGPUTexture      m_Textures[2];
    dmGraphics::WebGPUShaderModule m_Vertex;
    dmGraphics::WebGPUShaderModule m_Fragment;
    dmGraphics::WebGPURenderTarget m_Target;
    WGPUInstance                   m_Instance;
    WGPUAdapter                    m_Adapter;

    void SetUp()
    {
        m_Instance = 0;
        m_Adapter = 0;
        g_NativeDevice = false;
        g_LayoutCount = 0;
        g_PipelineLayoutCount = 0;
        g_ReleasedLayoutCount = 0;
        g_ReleasedPipelineLayoutCount = 0;
        g_GroupCount = 0;
        g_PipelineCount = 0;
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
        // These modules are fixture-owned placeholders used only for cache keys.
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
            for (uint32_t i = 0; i < 2; ++i)
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

// A real device with the optional feature disabled must accept the mixed float32/linear bindings.
TEST_F(WebGPUTextureLayoutTest, NativeDeviceWithoutFloat32Filtering)
{
    WGPUInstanceDescriptor instance_desc = WGPU_INSTANCE_DESCRIPTOR_INIT;
    WGPUInstanceFeatureName instance_feature = WGPUInstanceFeatureName_TimedWaitAny;
    instance_desc.requiredFeatures = &instance_feature;
    instance_desc.requiredFeatureCount = 1;
    m_Instance = wgpuCreateInstance(&instance_desc);
    ASSERT_NE((WGPUInstance)0, m_Instance);
    WGPURequestAdapterCallbackInfo adapter_callback = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    adapter_callback.mode = WGPUCallbackMode_WaitAnyOnly;
    adapter_callback.callback = CaptureAdapter;
    adapter_callback.userdata1 = &m_Adapter;
    WGPUFutureWaitInfo wait = {};
    wait.future = wgpuInstanceRequestAdapter(m_Instance, 0, adapter_callback);
    ASSERT_EQ(WGPUWaitStatus_Success, wgpuInstanceWaitAny(m_Instance, 1, &wait, UINT64_MAX));
    if (!m_Adapter)
    {
        SKIP();
    }

    WGPUDeviceDescriptor device_desc = WGPU_DEVICE_DESCRIPTOR_INIT;
    WGPURequestDeviceCallbackInfo device_callback = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    device_callback.mode = WGPUCallbackMode_WaitAnyOnly;
    device_callback.callback = CaptureDevice;
    device_callback.userdata1 = &m_Context->m_Device;
    wait.future = wgpuAdapterRequestDevice(m_Adapter, &device_desc, device_callback);
    ASSERT_EQ(WGPUWaitStatus_Success, wgpuInstanceWaitAny(m_Instance, 1, &wait, UINT64_MAX));
    ASSERT_NE((WGPUDevice)0, m_Context->m_Device);
    ASSERT_FALSE(wgpuDeviceHasFeature(m_Context->m_Device, WGPUFeatureName_Float32Filterable));

    for (uint32_t i = 0; i < 2; ++i)
    {
        m_Textures[i].m_TextureView = 0;
        m_Textures[i].m_Sampler = 0;
    }

    g_NativeDevice = true;
    wgpuDevicePushErrorScope(m_Context->m_Device, WGPUErrorFilter_Validation);
    for (uint32_t i = 0; i < 2; ++i)
    {
        WGPUTextureDescriptor texture_desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
        texture_desc.format = m_Textures[i].m_Format;
        texture_desc.size.width = 1;
        texture_desc.size.height = 1;
        texture_desc.size.depthOrArrayLayers = 1;
        texture_desc.usage = WGPUTextureUsage_TextureBinding;
        m_Textures[i].m_Texture = wgpuDeviceCreateTexture(m_Context->m_Device, &texture_desc);
        m_Textures[i].m_TextureView = wgpuTextureCreateView(m_Textures[i].m_Texture, 0);
        WGPUSamplerDescriptor sampler_desc = WGPU_SAMPLER_DESCRIPTOR_INIT;
        sampler_desc.minFilter = i ? WGPUFilterMode_Linear : WGPUFilterMode_Nearest;
        sampler_desc.magFilter = i ? WGPUFilterMode_Linear : WGPUFilterMode_Nearest;
        m_Textures[i].m_Sampler = wgpuDeviceCreateSampler(m_Context->m_Device, &sampler_desc);
    }

    Initialize();
    WebGPUUpdateBindGroups(m_Context);
    ValidationResult result = {};
    WGPUPopErrorScopeCallbackInfo error_callback = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;
    error_callback.mode = WGPUCallbackMode_WaitAnyOnly;
    error_callback.callback = CaptureValidation;
    error_callback.userdata1 = &result;
    wait.future = wgpuDevicePopErrorScope(m_Context->m_Device, error_callback);
    ASSERT_EQ(WGPUWaitStatus_Success, wgpuInstanceWaitAny(m_Instance, 1, &wait, UINT64_MAX));
    ASSERT_EQ(WGPUPopErrorScopeStatus_Success, result.m_Status);
    ASSERT_EQ(WGPUErrorType_NoError, result.m_Type);
    ASSERT_EQ(1u, m_Program.m_LayoutIndex);
}
#endif

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
