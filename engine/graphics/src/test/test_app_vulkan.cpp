// Copyright 2020-2023 The Defold Foundation
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

// Creating a small app test for initializing an running a small graphics app

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <dlib/log.h>
#include <dlib/time.h>

#include "../graphics.h"
#include "../graphics_private.h"
#include "../vulkan/graphics_vulkan.h"

#include "test_app_vulkan_assets.h"

// From engine_private.h

enum UpdateResult
{
    RESULT_OK       =  0,
    RESULT_REBOOT   =  1,
    RESULT_EXIT     = -1,
};

typedef void* (*EngineCreateFn)(int argc, char** argv);
typedef void (*EngineDestroyFn)(void* engine);
typedef UpdateResult (*EngineUpdateFn)(void* engine);
typedef void (*EngineGetResultFn)(void* engine, int* run_action, int* exit_code, int* argc, char*** argv);

struct RunLoopParams
{
    int     m_Argc;
    char**  m_Argv;

    void*   m_AppCtx;
    void    (*m_AppCreate)(void* ctx);
    void    (*m_AppDestroy)(void* ctx);

    EngineCreateFn      m_EngineCreate;
    EngineDestroyFn     m_EngineDestroy;
    EngineUpdateFn      m_EngineUpdate;
    EngineGetResultFn   m_EngineGetResult;
};

// From engine_loop.cpp

static int RunLoop(const RunLoopParams* params)
{
    if (params->m_AppCreate)
        params->m_AppCreate(params->m_AppCtx);

    int argc = params->m_Argc;
    char** argv = params->m_Argv;
    int exit_code = 0;
    void* engine = 0;
    UpdateResult result = RESULT_OK;
    while (RESULT_OK == result)
    {
        if (engine == 0)
        {
            engine = params->m_EngineCreate(argc, argv);
            if (!engine)
            {
                exit_code = 1;
                break;
            }
        }

        result = params->m_EngineUpdate(engine);

        if (RESULT_OK != result)
        {
            int run_action = 0;
            params->m_EngineGetResult(engine, &run_action, &exit_code, &argc, &argv);

            params->m_EngineDestroy(engine);
            engine = 0;

            if (RESULT_REBOOT == result)
            {
                // allows us to reboot
                result = RESULT_OK;
            }
        }
    }

    if (params->m_AppDestroy)
        params->m_AppDestroy(params->m_AppCtx);

    return exit_code;
}


struct AppCtx
{
    int m_Created;
    int m_Destroyed;
};

static void AppCreate(void* _ctx)
{
    dmLog::LogParams params;
    dmLog::LogInitialize(&params);

    AppCtx* ctx = (AppCtx*)_ctx;
    ctx->m_Created++;
}

static void AppDestroy(void* _ctx)
{
    AppCtx* ctx = (AppCtx*)_ctx;
    ctx->m_Destroyed++;
}

struct EngineCtx
{
    int m_WasCreated;
    int m_WasRun;
    int m_WasDestroyed;
    int m_WasResultCalled;

    uint64_t m_TimeStart;

    dmGraphics::HContext                    m_GraphicsContext;
    dmGraphics::HRenderTarget               m_Rendertarget;
    dmGraphics::HProgram                    m_ShaderProgram;
    dmGraphics::HVertexDeclaration          m_VertexDeclaration;
    dmGraphics::HVertexBuffer               m_VertexBuffer;
    dmGraphics::ShaderDesc::ResourceBinding m_VertexAttributes[1];

    dmGraphics::HTexture                    m_CopyBufferToTextureTexture;
    dmGraphics::HVertexBuffer               m_CopyBufferToTextureBuffer;

} g_EngineCtx;

static void* EngineCreate(int argc, char** argv)
{
    if (!dmGraphics::Initialize())
    {
        return 0;
    }

    dmGraphics::ContextParams graphics_context_params;
    graphics_context_params.m_DefaultTextureMinFilter = dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
    graphics_context_params.m_DefaultTextureMagFilter = dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
    graphics_context_params.m_VerifyGraphicsCalls = 1;
    graphics_context_params.m_RenderDocSupport = 0;
    graphics_context_params.m_UseValidationLayers = 1;

    EngineCtx* engine = &g_EngineCtx;
    engine->m_GraphicsContext = dmGraphics::NewContext(graphics_context_params);

    dmGraphics::WindowParams window_params;
    window_params.m_ResizeCallback = 0;
    window_params.m_ResizeCallbackUserData = 0;
    window_params.m_CloseCallback = 0;
    window_params.m_CloseCallbackUserData = 0;
    window_params.m_FocusCallback = 0;
    window_params.m_FocusCallbackUserData = 0;
    window_params.m_IconifyCallback = 0;
    window_params.m_IconifyCallbackUserData = 0;
    window_params.m_Width = 512;
    window_params.m_Height = 512;
    window_params.m_Samples = 0;
    window_params.m_Title = "TestTitle!";
    window_params.m_Fullscreen = 0;
    window_params.m_PrintDeviceInfo = false;
    window_params.m_HighDPI = 0;

    (void)dmGraphics::OpenWindow(engine->m_GraphicsContext, &window_params);

    //////////// VERTEX BUFFER ////////////
    const float vertex_data_no_index[] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
        -0.5f,  0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f,
    };

    engine->m_VertexBuffer = dmGraphics::NewVertexBuffer(engine->m_GraphicsContext, sizeof(vertex_data_no_index), (void*) vertex_data_no_index, dmGraphics::BUFFER_USAGE_STATIC_DRAW);


    //////////// SHADER ////////////
    dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(engine->m_GraphicsContext);
    dmGraphics::AddVertexStream(stream_declaration, "pos", 2, dmGraphics::TYPE_FLOAT, false);

    dmGraphics::ShaderDesc::ResourceBinding& vx_attribute_position = engine->m_VertexAttributes[0];
    vx_attribute_position.m_Name         = "pos";
    vx_attribute_position.m_Type         = dmGraphics::ShaderDesc::ShaderDataType::SHADER_TYPE_VEC2;
    vx_attribute_position.m_ElementCount = 1;
    vx_attribute_position.m_Binding      = 0;

    dmGraphics::ShaderDesc::Shader vs_shader = {};
    vs_shader.m_Language       = dmGraphics::ShaderDesc::LANGUAGE_SPIRV;
    vs_shader.m_Source.m_Data  = (uint8_t*) vulkan_assets::vertex_program;
    vs_shader.m_Source.m_Count = sizeof(vulkan_assets::vertex_program);
    vs_shader.m_Inputs.m_Data  = engine->m_VertexAttributes;
    vs_shader.m_Inputs.m_Count = sizeof(engine->m_VertexAttributes) / sizeof(dmGraphics::ShaderDesc::ResourceBinding);

    dmGraphics::ShaderDesc::Shader fs_shader = {};
    fs_shader.m_Language       = dmGraphics::ShaderDesc::LANGUAGE_SPIRV;
    fs_shader.m_Source.m_Data  = (uint8_t*) vulkan_assets::fragment_program;
    fs_shader.m_Source.m_Count = sizeof(vulkan_assets::fragment_program);

    dmGraphics::ShaderDesc::ResourceBlock fs_uniform_block = {};

    fs_uniform_block.m_Name     = "inputColor";
    fs_uniform_block.m_NameHash = dmHashString64(fs_uniform_block.m_Name);
    fs_uniform_block.m_Type     = dmGraphics::ShaderDesc::SHADER_TYPE_RENDER_PASS_INPUT;
    fs_uniform_block.m_Set      = 0;
    fs_uniform_block.m_Binding  = 0;

    fs_shader.m_Resources.m_Data  = &fs_uniform_block;
    fs_shader.m_Resources.m_Count = 1;

    dmGraphics::HVertexProgram vs_program   = dmGraphics::NewVertexProgram(engine->m_GraphicsContext, &vs_shader);
    dmGraphics::HFragmentProgram fs_program = dmGraphics::NewFragmentProgram(engine->m_GraphicsContext, &fs_shader);

    engine->m_ShaderProgram     = dmGraphics::NewProgram(engine->m_GraphicsContext, vs_program, fs_program);
    engine->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(engine->m_GraphicsContext, stream_declaration);

    //////////// RENDER TARGET ////////////
    dmGraphics::RenderTargetCreationParams p = {};

    for (int i = 0; i < dmGraphics::MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
    {
        p.m_ColorBufferCreationParams[i].m_Type           = dmGraphics::TEXTURE_TYPE_2D;
        p.m_ColorBufferCreationParams[i].m_Width          = 512;
        p.m_ColorBufferCreationParams[i].m_Height         = 512;
        p.m_ColorBufferCreationParams[i].m_OriginalWidth  = 512;
        p.m_ColorBufferCreationParams[i].m_OriginalHeight = 512;

        p.m_ColorBufferParams[i].m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
        p.m_ColorBufferParams[i].m_Width  = 512;
        p.m_ColorBufferParams[i].m_Height = 512;
    }

    p.m_ColorBufferCreationParams[0].m_UsageHintBits = dmGraphics::TEXTURE_USAGE_HINT_INPUT | dmGraphics::TEXTURE_USAGE_HINT_MEMORYLESS;

    engine->m_Rendertarget = dmGraphics::NewRenderTarget(engine->m_GraphicsContext, dmGraphics::BUFFER_TYPE_COLOR0_BIT | dmGraphics::BUFFER_TYPE_COLOR1_BIT, p);

    //////////// RENDER PASS ////////////
    uint8_t sub_pass_0_color_attachments[] = { 0 };
    uint8_t sub_pass_1_color_attachments[] = { 1 };
    uint8_t sub_pass_1_input_attachments[] = { 0 };

    dmGraphics::CreateRenderPassParams rp = {};
    rp.m_SubPassCount = 2;

    rp.m_SubPasses[0].m_ColorAttachmentIndices      = sub_pass_0_color_attachments;
    rp.m_SubPasses[0].m_ColorAttachmentIndicesCount = DM_ARRAY_SIZE(sub_pass_0_color_attachments);

    rp.m_SubPasses[1].m_ColorAttachmentIndices      = sub_pass_1_color_attachments;
    rp.m_SubPasses[1].m_ColorAttachmentIndicesCount = DM_ARRAY_SIZE(sub_pass_1_color_attachments);

    rp.m_SubPasses[1].m_InputAttachmentIndices      = sub_pass_1_input_attachments;
    rp.m_SubPasses[1].m_InputAttachmentIndicesCount = DM_ARRAY_SIZE(sub_pass_1_input_attachments);

    rp.m_DependencyCount = 3;
    rp.m_Dependencies[0].m_Src = dmGraphics::SUBPASS_EXTERNAL;
    rp.m_Dependencies[0].m_Dst = 0;

    rp.m_Dependencies[1].m_Src = 0;
    rp.m_Dependencies[1].m_Dst = 1;

    rp.m_Dependencies[2].m_Src = 1;
    rp.m_Dependencies[2].m_Dst = dmGraphics::SUBPASS_EXTERNAL;

    dmGraphics::VulkanCreateRenderPass(engine->m_GraphicsContext, engine->m_Rendertarget, rp);

    //////////// COPY BUFFER TO TEXTURE ////////////
    {
        dmGraphics::TextureCreationParams tp = {};
        tp.m_Width                           = 128;
        tp.m_Height                          = 128;
        tp.m_OriginalWidth                   = 128;
        tp.m_OriginalHeight                  = 128;
        engine->m_CopyBufferToTextureTexture = dmGraphics::NewTexture(engine->m_GraphicsContext, tp);

        dmGraphics::TextureParams p = {};
        p.m_Width    = 128;
        p.m_Height   = 128;
        p.m_Format   = dmGraphics::TEXTURE_FORMAT_RGBA;
        dmGraphics::SetTexture(engine->m_CopyBufferToTextureTexture, p);

        engine->m_CopyBufferToTextureBuffer = dmGraphics::NewVertexBuffer(engine->m_GraphicsContext, 128 * 128 * 4, 0, dmGraphics::BUFFER_USAGE_TRANSFER);

        uint8_t* pixels = (uint8_t*) dmGraphics::MapVertexBuffer(engine->m_GraphicsContext, engine->m_CopyBufferToTextureBuffer, dmGraphics::BUFFER_ACCESS_READ_WRITE);

        for (int i = 0; i < 128; i++)
        {
            for (int j = 0; j < 128; j++)
            {
                float u = ((float) j) / (float) 128;
                float v = ((float) i) / (float) 128;

                uint32_t ix = (i * 128 + j) * 4;
                pixels[ix + 0] = (uint8_t) (u * 255.0f);
                pixels[ix + 1] = 0;
                pixels[ix + 2] = (uint8_t) (v * 255.0f);
                pixels[ix + 3] = 255;
            }
        }

        dmGraphics::UnmapVertexBuffer(engine->m_GraphicsContext, engine->m_CopyBufferToTextureBuffer);
    }

    engine->m_WasCreated++;
    engine->m_TimeStart = dmTime::GetTime();
    return &g_EngineCtx;
}

static void EngineDestroy(void* _engine)
{
    EngineCtx* engine = (EngineCtx*)_engine;
    dmGraphics::CloseWindow(engine->m_GraphicsContext);
    dmGraphics::DeleteContext(engine->m_GraphicsContext);
    dmGraphics::Finalize();
    engine->m_WasDestroyed++;
}

static UpdateResult EngineUpdate(void* _engine)
{
    EngineCtx* engine = (EngineCtx*)_engine;
    engine->m_WasRun++;
    uint64_t t = dmTime::GetTime();
    float elapsed = (t - engine->m_TimeStart) / 1000000.0f;
    /*
    if (elapsed > 3.0f)
        return RESULT_EXIT;
    */

    dmGraphics::BeginFrame(engine->m_GraphicsContext);

    //////////// COPY BUFFER TO TEXTURE TEST ////////////
#if 1
    dmGraphics::TextureParams copy_params = {};
    copy_params.m_Width                   = dmGraphics::GetTextureWidth(engine->m_CopyBufferToTextureTexture);
    copy_params.m_Height                  = dmGraphics::GetTextureHeight(engine->m_CopyBufferToTextureTexture);
    dmGraphics::VulkanCopyBufferToTexture(engine->m_GraphicsContext, engine->m_CopyBufferToTextureBuffer, engine->m_CopyBufferToTextureTexture, copy_params);
#endif

    //////////// SUBPASS TEST ////////////
#if 0
    dmGraphics::HTexture sub_pass_0_color = dmGraphics::GetRenderTargetTexture(engine->m_Rendertarget, dmGraphics::BUFFER_TYPE_COLOR0_BIT);

    dmGraphics::SetRenderTarget(engine->m_GraphicsContext, engine->m_Rendertarget, 0);

    dmGraphics::SetViewport(engine->m_GraphicsContext, 0, 0, 512, 512);

    static uint8_t color_r = 0;
    static uint8_t color_g = 80;
    static uint8_t color_b = 140;
    static uint8_t color_a = 255;

    dmGraphics::Clear(engine->m_GraphicsContext, dmGraphics::BUFFER_TYPE_COLOR0_BIT,
                                (float)color_r,
                                (float)color_g,
                                (float)color_b,
                                (float)color_a,
                                1.0f, 0);

    dmGraphics::VulkanNextRenderPass(engine->m_GraphicsContext, engine->m_Rendertarget);

    dmGraphics::EnableProgram(engine->m_GraphicsContext, engine->m_ShaderProgram);
    dmGraphics::DisableState(engine->m_GraphicsContext, dmGraphics::STATE_DEPTH_TEST);

    dmGraphics::EnableTexture(engine->m_GraphicsContext, 0, 0, sub_pass_0_color);

    dmGraphics::EnableVertexDeclaration(engine->m_GraphicsContext, engine->m_VertexDeclaration, engine->m_VertexBuffer);
    dmGraphics::Draw(engine->m_GraphicsContext, dmGraphics::PRIMITIVE_TRIANGLES, 0, 6);

    dmGraphics::SetRenderTarget(engine->m_GraphicsContext, 0, 0);
#endif

    dmGraphics::Flip(engine->m_GraphicsContext);

    // color_b += 2;
    // color_g += 1;

    return RESULT_OK;
}

static void EngineGetResult(void* _engine, int* run_action, int* exit_code, int* argc, char*** argv)
{
    EngineCtx* ctx = (EngineCtx*)_engine;
    ctx->m_WasResultCalled++;
}

TEST(App, Run)
{
    AppCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    memset(&g_EngineCtx, 0, sizeof(g_EngineCtx));

    RunLoopParams params;
    params.m_AppCtx = &ctx;
    params.m_AppCreate = AppCreate;
    params.m_AppDestroy = AppDestroy;
    params.m_EngineCreate = EngineCreate;
    params.m_EngineDestroy = EngineDestroy;
    params.m_EngineUpdate = EngineUpdate;
    params.m_EngineGetResult = EngineGetResult;

    int ret = RunLoop(&params);
    ASSERT_EQ(0, ret);


    uint64_t t = dmTime::GetTime();
    float elapsed = (t - g_EngineCtx.m_TimeStart) / 1000000.0f;
    (void)elapsed;

    ASSERT_EQ(1, ctx.m_Created);
    ASSERT_EQ(1, ctx.m_Destroyed);
    ASSERT_EQ(1, g_EngineCtx.m_WasCreated);
    //ASSERT_EQ(200, g_EngineCtx.m_WasRun);
    ASSERT_EQ(1, g_EngineCtx.m_WasDestroyed);
    ASSERT_EQ(1, g_EngineCtx.m_WasResultCalled);
}


int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
