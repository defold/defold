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

#include "../webgpu/graphics_webgpu.cpp"

static const char* VERTEX_SOURCE =
    "@vertex fn main(@builtin(vertex_index) index: u32) -> @builtin(position) vec4<f32> {\n"
    "  let positions = array<vec2<f32>, 3>(vec2<f32>(-1, -1), vec2<f32>(3, -1), vec2<f32>(-1, 3));\n"
    "  return vec4<f32>(positions[index], 0.5, 1.0);\n"
    "}\n";

static const char* FRAGMENT_SOURCE =
    "@group(0) @binding(0) var color: texture_2d<f32>;\n"
    "@fragment fn main() -> @location(0) vec4<f32> {\n"
    "  return textureLoad(color, vec2<i32>(0, 0), 0);\n"
    "}\n";

static const char* UNIFORM_FRAGMENT_SOURCE =
    "struct Colors { color: vec4<f32>, tint: vec4<f32>, };\n"
    "@group(0) @binding(0) var<uniform> colors: Colors;\n"
    "@fragment fn main() -> @location(0) vec4<f32> {\n"
    "  return colors.color + colors.tint;\n"
    "}\n";

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

struct WebGPUDeviceLossTest : jc_test_base_class
{
    dmGraphics::WebGPUContext*              m_Context;
    dmGraphics::HContext                    m_GraphicsContext;
    dmGraphics::ShaderDesc::Shader          m_Shaders[2];
    dmGraphics::ShaderDesc::ResourceBinding m_Texture;
    dmGraphics::ShaderDesc                  m_ShaderDesc;
    bool                                    m_HasDevice;

    void SetUp() override
    {
        m_Context = new dmGraphics::WebGPUContext();
        g_WebGPUContext = m_Context;
        m_GraphicsContext = (dmGraphics::HContext) m_Context;
        m_Context->m_BindGroupPool = new dmObjectPool<dmGraphics::WebGPUBindGroup>();
        m_Context->m_BindGroupPool->SetCapacity(16);
        m_Context->m_BindGroupCache.SetCapacity(16);
        m_Context->m_SamplerCache.SetCapacity(8, 16);
        m_Context->m_RenderPipelineCache.SetCapacity(8, 16);
        m_Context->m_ComputePipelineCache.SetCapacity(8, 16);
        m_Context->m_CurrentPipelineState = dmGraphics::GetDefaultPipelineState();
        m_Context->m_CurrentPipelineState.m_PrimtiveType = dmGraphics::PRIMITIVE_TRIANGLES;

        WGPUInstanceDescriptor instance_desc = WGPU_INSTANCE_DESCRIPTOR_INIT;
        WGPUInstanceFeatureName instance_feature = WGPUInstanceFeatureName_TimedWaitAny;
        instance_desc.requiredFeatures = &instance_feature;
        instance_desc.requiredFeatureCount = 1;
        m_Context->m_Instance = wgpuCreateInstance(&instance_desc);

        // The device request and adoption of InitializeWebGPUContext, without a window surface
        WebGPURequestDevice(m_Context);
        dmGraphics::WebGPUDeviceRequest* request = m_Context->m_DeviceRequest;
        m_Context->m_DeviceRequest = NULL;
        m_HasDevice = request->m_Device != NULL;
        if (m_HasDevice)
        {
            WebGPUSetDevice(m_Context, request);
            WebGPUCreateDefaultTextures(m_Context);
        }

        WebGPUReleaseDeviceRequest(request);

        memset(m_Shaders, 0, sizeof(m_Shaders));
        const char* sources[] = { VERTEX_SOURCE, FRAGMENT_SOURCE };
        for (uint32_t i = 0; i < 2; ++i)
        {
            m_Shaders[i].m_ShaderType     = i == 0 ? dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX : dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT;
            m_Shaders[i].m_Language       = dmGraphics::ShaderDesc::LANGUAGE_WGSL;
            m_Shaders[i].m_Source.m_Data  = (uint8_t*) sources[i];
            m_Shaders[i].m_Source.m_Count = strlen(sources[i]);
        }

        memset(&m_Texture, 0, sizeof(m_Texture));
        m_Texture.m_Name                     = "color";
        m_Texture.m_NameHash                 = dmHashString64("color");
        m_Texture.m_Type.m_Type.m_ShaderType = dmGraphics::ShaderDesc::SHADER_TYPE_TEXTURE2D;
        m_Texture.m_StageFlags               = dmGraphics::SHADER_STAGE_FLAG_FRAGMENT;
        memset(&m_ShaderDesc, 0, sizeof(m_ShaderDesc));
        m_ShaderDesc.m_Shaders.m_Data             = m_Shaders;
        m_ShaderDesc.m_Shaders.m_Count            = 2;
        m_ShaderDesc.m_Reflection.m_Textures.m_Data  = &m_Texture;
        m_ShaderDesc.m_Reflection.m_Textures.m_Count = 1;
    }

    void TearDown() override
    {
        if (m_HasDevice)
        {
            WebGPUSubmitCommandEncoder(m_Context);
        }

        m_Context->m_RenderPipelineCache.Iterate(WebGPUCleanupRenderPipelineCache, m_Context);
        m_Context->m_ComputePipelineCache.Iterate(WebGPUCleanupComputePipelineCache, m_Context);
        m_Context->m_SamplerCache.Iterate(WebGPUCleanupSamplerCache, m_Context);
        WebGPUDestroyContext(m_Context);
        delete m_Context;
        g_WebGPUContext = NULL;
    }

    void LoseAndRestoreDevice()
    {
        wgpuDeviceDestroy(m_Context->m_Device);
        ASSERT_EQ(dmGraphics::CONTEXT_EVENT_LOST, WebGPUPollContextEvent(m_GraphicsContext));
        ASSERT_EQ(dmGraphics::CONTEXT_EVENT_RESTORED, WebGPUPollContextEvent(m_GraphicsContext));
    }

    void PushValidationScope()
    {
        wgpuDevicePushErrorScope(m_Context->m_Device, WGPUErrorFilter_Validation);
    }

    void AssertNoValidationErrors()
    {
        ValidationResult result = {};
        WGPUPopErrorScopeCallbackInfo callback = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;
        callback.mode = WGPUCallbackMode_WaitAnyOnly;
        callback.callback = CaptureValidation;
        callback.userdata1 = &result;
        WGPUFutureWaitInfo wait = {};
        wait.future = wgpuDevicePopErrorScope(m_Context->m_Device, callback);
        ASSERT_EQ(WGPUWaitStatus_Success, wgpuInstanceWaitAny(m_Context->m_Instance, 1, &wait, UINT64_MAX));
        ASSERT_EQ(WGPUPopErrorScopeStatus_Success, result.m_Status);
        ASSERT_EQ(WGPUErrorType_NoError, result.m_Type);
    }

    dmGraphics::HRenderTarget NewRenderTarget()
    {
        dmGraphics::RenderTargetCreationParams params;
        params.m_ColorBufferCreationParams[0].m_Width  = 1;
        params.m_ColorBufferCreationParams[0].m_Height = 1;
        params.m_ColorBufferParams[0].m_Width          = 1;
        params.m_ColorBufferParams[0].m_Height         = 1;
        params.m_ColorBufferLoadOps[0]                 = dmGraphics::ATTACHMENT_OP_CLEAR;
        params.m_ColorBufferStoreOps[0]                = dmGraphics::ATTACHMENT_OP_STORE;
        params.m_ColorBufferClearValue[0][2]           = 1.0f;
        params.m_ColorBufferClearValue[0][3]           = 1.0f;
        dmGraphics::HRenderTarget rt = WebGPUNewRenderTarget(m_GraphicsContext, dmGraphics::BUFFER_TYPE_COLOR0_BIT, params);
        // Registered by dmGraphics::NewRenderTarget in the engine
        m_Context->m_BaseContext.m_RenderTargets.OffsetCapacity(1);
        m_Context->m_BaseContext.m_RenderTargets.Push(rt);
        return rt;
    }

    void DeleteRenderTarget(dmGraphics::HRenderTarget rt)
    {
        m_Context->m_BaseContext.m_RenderTargets.SetSize(0);
        WebGPUDeleteRenderTarget(m_GraphicsContext, rt);
    }

    dmGraphics::HTexture NewTexture(const uint8_t rgba[4])
    {
        dmGraphics::TextureCreationParams creation_params;
        creation_params.m_Width  = 1;
        creation_params.m_Height = 1;
        dmGraphics::HTexture texture = WebGPUNewTexture(m_GraphicsContext, creation_params);
        UploadTexture(texture, rgba);
        return texture;
    }

    void UploadTexture(dmGraphics::HTexture texture, const uint8_t rgba[4])
    {
        dmGraphics::TextureParams params;
        params.m_Data   = rgba;
        params.m_Width  = 1;
        params.m_Height = 1;
        params.m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
        WebGPUSetTexture(m_GraphicsContext, texture, params);
    }

    void DrawPixel(dmGraphics::HRenderTarget rt, dmGraphics::HProgram program, dmGraphics::HTexture texture, uint8_t bgra[4])
    {
        dmGraphics::RenderTargetBindingParams binding = {};
        WebGPUSetRenderTarget(m_GraphicsContext, rt, binding);
        WebGPUSetViewport(m_GraphicsContext, 0, 0, 1, 1);
        WebGPUEnableProgram(m_GraphicsContext, program);
        WebGPUEnableTexture(m_GraphicsContext, 0, 0, texture);
        WebGPUDraw(m_GraphicsContext, dmGraphics::PRIMITIVE_TRIANGLES, 0, 3, 1);
        WebGPUReadPixels(m_GraphicsContext, 0, 0, 1, 1, bgra, 4);
    }

    void DrawUniformPixel(dmGraphics::HRenderTarget rt, dmGraphics::HProgram program, dmGraphics::HUniformBuffer buffer, uint8_t bgra[4])
    {
        dmGraphics::RenderTargetBindingParams binding = {};
        WebGPUSetRenderTarget(m_GraphicsContext, rt, binding);
        WebGPUSetViewport(m_GraphicsContext, 0, 0, 1, 1);
        WebGPUEnableProgram(m_GraphicsContext, program);
        WebGPUEnableUniformBuffer(m_GraphicsContext, buffer, 0, 0);
        WebGPUDraw(m_GraphicsContext, dmGraphics::PRIMITIVE_TRIANGLES, 0, 3, 1);
        WebGPUReadPixels(m_GraphicsContext, 0, 0, 1, 1, bgra, 4);
    }
};

static dmGraphics::WebGPUTexture* GetTexture(dmGraphics::WebGPUContext* context, dmGraphics::HTexture texture)
{
    return dmGraphics::GetAssetFromContainer<dmGraphics::WebGPUTexture>(context->m_BaseContext.m_AssetHandleContainer, texture);
}

// A destroyed device is reported once as lost, kept until a new device replaces it on the next
// poll, and then reported as restored. Guards against rendering staying on a lost device.
TEST_F(WebGPUDeviceLossTest, LostDeviceIsReplaced)
{
    if (!m_HasDevice)
    {
        SKIP();
    }

    ASSERT_EQ(dmGraphics::CONTEXT_EVENT_NONE, WebGPUPollContextEvent(m_GraphicsContext));
    WGPUDevice lost_device = m_Context->m_Device;
    const uint32_t lost_generation = m_Context->m_DeviceGeneration;
    wgpuDeviceDestroy(lost_device);

    ASSERT_EQ(dmGraphics::CONTEXT_EVENT_LOST, WebGPUPollContextEvent(m_GraphicsContext));
    ASSERT_EQ(lost_device, m_Context->m_Device);

    ASSERT_EQ(dmGraphics::CONTEXT_EVENT_RESTORED, WebGPUPollContextEvent(m_GraphicsContext));
    ASSERT_NE(lost_device, m_Context->m_Device);
    ASSERT_NE(lost_generation, m_Context->m_DeviceGeneration);
    ASSERT_NE((WGPUQueue)0, m_Context->m_Queue);
    ASSERT_TRUE(WebGPUIsTextureBindable(m_Context->m_DefaultTexture2D));
    ASSERT_EQ(dmGraphics::CONTEXT_EVENT_NONE, WebGPUPollContextEvent(m_GraphicsContext));

    // A second loss of the replacement device is recovered the same way
    LoseAndRestoreDevice();
    ASSERT_EQ(dmGraphics::CONTEXT_EVENT_NONE, WebGPUPollContextEvent(m_GraphicsContext));
}

// Render targets are recreated with the graphics handles, the stale program and texture are
// skipped or replaced by a default texture until the resource reload recreates them, and the
// result draws without validation errors on the new device. Guards against binding objects of
// the lost device to the new one.
TEST_F(WebGPUDeviceLossTest, ResourcesDrawOnTheRestoredDevice)
{
    if (!m_HasDevice)
    {
        SKIP();
    }

    const uint8_t red[4] = { 255, 0, 0, 255 };
    dmGraphics::HRenderTarget rt = NewRenderTarget();
    ASSERT_NE((dmGraphics::HRenderTarget)0, rt);
    dmGraphics::HProgram program = WebGPUNewProgram(m_GraphicsContext, &m_ShaderDesc, 0, 0);
    ASSERT_NE((dmGraphics::HProgram)0, program);
    dmGraphics::HTexture texture = NewTexture(red);

    uint8_t bgra[4] = {};
    DrawPixel(rt, program, texture, bgra);
    ASSERT_EQ(0u, bgra[0]);
    ASSERT_EQ(0u, bgra[1]);
    ASSERT_EQ(255u, bgra[2]);
    ASSERT_EQ(255u, bgra[3]);

    LoseAndRestoreDevice();
    PushValidationScope();

    dmGraphics::WebGPURenderTarget* target = dmGraphics::GetAssetFromContainer<dmGraphics::WebGPURenderTarget>(m_Context->m_BaseContext.m_AssetHandleContainer, rt);
    ASSERT_TRUE(WebGPUIsTextureStale(GetTexture(m_Context, target->m_TextureColor[0])));
    WebGPURecreateGraphicsHandles(m_GraphicsContext);
    ASSERT_FALSE(WebGPUIsTextureStale(GetTexture(m_Context, target->m_TextureColor[0])));

    // A program that the resource reload has not recreated yet does not draw
    dmGraphics::RenderTargetBindingParams binding = {};
    WebGPUSetRenderTarget(m_GraphicsContext, rt, binding);
    WebGPUEnableProgram(m_GraphicsContext, program);
    WebGPUDraw(m_GraphicsContext, dmGraphics::PRIMITIVE_TRIANGLES, 0, 3, 1);
    ASSERT_EQ((WGPURenderPassEncoder)0, m_Context->m_CurrentRenderPass.m_Encoder);

    ASSERT_TRUE(dmGraphics::ReloadProgram(m_GraphicsContext, program, &m_ShaderDesc, 0, 0));
    ASSERT_FALSE(WebGPUIsTextureBindable(GetTexture(m_Context, texture)));
    DrawPixel(rt, program, texture, bgra);
    ASSERT_EQ(0u, bgra[0]);
    ASSERT_EQ(0u, bgra[1]);
    ASSERT_EQ(0u, bgra[2]);
    ASSERT_EQ(0u, bgra[3]);

    UploadTexture(texture, red);
    ASSERT_TRUE(WebGPUIsTextureBindable(GetTexture(m_Context, texture)));
    DrawPixel(rt, program, texture, bgra);
    ASSERT_EQ(0u, bgra[0]);
    ASSERT_EQ(0u, bgra[1]);
    ASSERT_EQ(255u, bgra[2]);
    ASSERT_EQ(255u, bgra[3]);
    AssertNoValidationErrors();

    WebGPUDeleteTexture(m_GraphicsContext, texture);
    dmGraphics::DeleteProgram(m_GraphicsContext, program);
    DeleteRenderTarget(rt);
}

// Buffers keep their capacity and are recreated on the new device by their next write, also by a
// partial one. Guards against writes into, and bindings of, buffers of the lost device.
TEST_F(WebGPUDeviceLossTest, BuffersAreRecreatedOnWrite)
{
    if (!m_HasDevice)
    {
        SKIP();
    }

    const uint32_t data[4] = { 1, 2, 3, 4 };
    dmGraphics::HVertexBuffer vertex_buffer = WebGPUNewVertexBuffer(m_GraphicsContext, sizeof(data), data, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
    dmGraphics::HUniformBuffer uniform_buffer = WebGPUNewUniformBuffer(m_GraphicsContext, 0, sizeof(data));
    dmGraphics::HStorageBuffer storage_buffer = WebGPUNewStorageBuffer(m_GraphicsContext, sizeof(data), data, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
    dmGraphics::WebGPUBuffer* vertex = (dmGraphics::WebGPUBuffer*) vertex_buffer;
    dmGraphics::WebGPUUniformBuffer* uniform = (dmGraphics::WebGPUUniformBuffer*) uniform_buffer;
    dmGraphics::WebGPUStorageBuffer* storage = (dmGraphics::WebGPUStorageBuffer*) storage_buffer;

    LoseAndRestoreDevice();
    PushValidationScope();
    ASSERT_TRUE(WebGPUIsStale(vertex->m_DeviceGeneration));
    ASSERT_TRUE(WebGPUIsStale(uniform->m_DeviceGeneration));
    ASSERT_TRUE(WebGPUIsStale(storage->m_DeviceGeneration));

    WebGPUSetVertexBufferSubData(vertex_buffer, 4, 4, &data[1]);
    ASSERT_FALSE(WebGPUIsStale(vertex->m_DeviceGeneration));
    ASSERT_EQ(sizeof(data), vertex->m_Base.m_Size);
    ASSERT_EQ(sizeof(data), vertex->m_Used);
    WebGPUSetUniformBuffer(m_GraphicsContext, uniform_buffer, 0, sizeof(data), data);
    ASSERT_FALSE(WebGPUIsStale(uniform->m_DeviceGeneration));
    WebGPUSetStorageBufferSubData(m_GraphicsContext, storage_buffer, 0, sizeof(data), data);
    ASSERT_FALSE(WebGPUIsStale(storage->m_DeviceGeneration));
    AssertNoValidationErrors();

    WebGPUDeleteStorageBuffer(m_GraphicsContext, storage_buffer);
    WebGPUDeleteUniformBuffer(m_GraphicsContext, uniform_buffer);
    WebGPUDeleteVertexBuffer(vertex_buffer);
}

// Uniform buffers recreated on the new device keep the content written before the loss: one is
// never written again (as material buffers), the other gets only a partial write (as the light
// buffer). Guards against restored draws reading zeros from uniform buffers.
TEST_F(WebGPUDeviceLossTest, UniformBuffersKeepContent)
{
    if (!m_HasDevice)
    {
        SKIP();
    }

    dmGraphics::ShaderDesc::ResourceMember members[2];
    memset(members, 0, sizeof(members));
    const char* member_names[] = { "color", "tint" };
    for (uint32_t i = 0; i < 2; ++i)
    {
        members[i].m_Name                     = member_names[i];
        members[i].m_NameHash                 = dmHashString64(member_names[i]);
        members[i].m_Type.m_Type.m_ShaderType = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
        members[i].m_ElementCount             = 1;
        members[i].m_Offset                   = i * sizeof(dmVMath::Vector4);
    }

    dmGraphics::ShaderDesc::ResourceTypeInfo type;
    memset(&type, 0, sizeof(type));
    type.m_Name            = "Colors";
    type.m_NameHash        = dmHashString64("Colors");
    type.m_Members.m_Data  = members;
    type.m_Members.m_Count = 2;
    dmGraphics::ShaderDesc::ResourceBinding colors;
    memset(&colors, 0, sizeof(colors));
    colors.m_Name                    = "colors";
    colors.m_NameHash                = dmHashString64("colors");
    colors.m_Type.m_Type.m_TypeIndex = 0;
    colors.m_Type.m_UseTypeIndex     = 1;
    colors.m_StageFlags              = dmGraphics::SHADER_STAGE_FLAG_FRAGMENT;
    colors.m_Bindinginfo.m_BlockSize = 2 * sizeof(dmVMath::Vector4);
    dmGraphics::ShaderDesc::Shader shaders[2];
    memcpy(shaders, m_Shaders, sizeof(shaders));
    shaders[1].m_Source.m_Data  = (uint8_t*) UNIFORM_FRAGMENT_SOURCE;
    shaders[1].m_Source.m_Count = strlen(UNIFORM_FRAGMENT_SOURCE);
    dmGraphics::ShaderDesc desc;
    memset(&desc, 0, sizeof(desc));
    desc.m_Shaders.m_Data                       = shaders;
    desc.m_Shaders.m_Count                      = 2;
    desc.m_Reflection.m_UniformBuffers.m_Data   = &colors;
    desc.m_Reflection.m_UniformBuffers.m_Count  = 1;
    desc.m_Reflection.m_Types.m_Data            = &type;
    desc.m_Reflection.m_Types.m_Count           = 1;

    dmGraphics::HRenderTarget rt = NewRenderTarget();
    dmGraphics::HProgram program = WebGPUNewProgram(m_GraphicsContext, &desc, 0, 0);
    ASSERT_NE((dmGraphics::HProgram)0, program);
    dmGraphics::WebGPUProgram* webgpu_program = (dmGraphics::WebGPUProgram*) program;
    const dmGraphics::UniformBufferLayout layout = *(dmGraphics::UniformBufferLayout*) webgpu_program->m_BaseProgram.m_ResourceBindings[0][0].m_BindingUserData;
    const dmVMath::Vector4 contents[2][2] = {
        { dmVMath::Vector4(1, 0, 0, 1), dmVMath::Vector4(0, 0, 0, 0) },
        { dmVMath::Vector4(0, 1, 0, 1), dmVMath::Vector4(0, 0, 0, 0) },
    };
    dmGraphics::HUniformBuffer buffers[2];
    for (uint32_t i = 0; i < 2; ++i)
    {
        buffers[i] = WebGPUNewUniformBuffer(m_GraphicsContext, layout, sizeof(contents[i]));
        WebGPUSetUniformBuffer(m_GraphicsContext, buffers[i], 0, sizeof(contents[i]), contents[i]);
    }

    // BGRA, as returned by ReadPixels()
    const uint8_t before[2][4] = { { 0, 0, 255, 255 }, { 0, 255, 0, 255 } };
    for (uint32_t i = 0; i < 2; ++i)
    {
        uint8_t bgra[4] = {};
        DrawUniformPixel(rt, program, buffers[i], bgra);
        ASSERT_EQ(0, memcmp(before[i], bgra, sizeof(bgra)));
    }

    LoseAndRestoreDevice();
    PushValidationScope();
    WebGPURecreateGraphicsHandles(m_GraphicsContext);
    ASSERT_TRUE(dmGraphics::ReloadProgram(m_GraphicsContext, program, &desc, 0, 0));
    const dmVMath::Vector4 tint(0, 0, 1, 0);
    WebGPUSetUniformBuffer(m_GraphicsContext, buffers[1], sizeof(dmVMath::Vector4), sizeof(tint), &tint);

    const uint8_t after[2][4] = { { 0, 0, 255, 255 }, { 255, 255, 0, 255 } };
    for (uint32_t i = 0; i < 2; ++i)
    {
        uint8_t bgra[4] = {};
        DrawUniformPixel(rt, program, buffers[i], bgra);
        ASSERT_EQ(0, memcmp(after[i], bgra, sizeof(bgra)));
    }

    AssertNoValidationErrors();

    for (uint32_t i = 0; i < 2; ++i)
    {
        WebGPUDeleteUniformBuffer(m_GraphicsContext, buffers[i]);
    }

    dmGraphics::DeleteProgram(m_GraphicsContext, program);
    DeleteRenderTarget(rt);
}

int main(int argc, char** argv)
{
    // Program creation selects shaders through the installed function table
    GraphicsAdapterWebGPU();
    dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_WEBGPU);
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
