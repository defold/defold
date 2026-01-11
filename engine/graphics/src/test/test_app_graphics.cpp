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

// Creating a small app test for initializing an running a small graphics app

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <dlib/log.h>
#include <dlib/time.h>

#include "test_app_graphics.h"

#include <dmsdk/graphics/graphics_vulkan.h>

#include "test_app_graphics_assets.h"

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

static dmGraphics::HUniformLocation GetUniformLocation(dmGraphics::HProgram program, const char* name)
{
    dmhash_t hash = dmHashString64(name);
    for (int i = 0; i < dmGraphics::GetUniformCount(program); ++i)
    {
        dmGraphics::Uniform uniform;
        dmGraphics::GetUniform(program, i, &uniform);

        if (uniform.m_NameHash == hash)
        {
            return uniform.m_Location;
        }
    }
    return dmGraphics::INVALID_UNIFORM_LOCATION;
}

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

struct EngineCtx;

struct ITest
{
    virtual void Initialize(EngineCtx*) {};
    virtual void Execute(EngineCtx*) {};
};

struct EngineCtx
{
    int m_WasCreated;
    int m_WasRun;
    int m_WasDestroyed;
    int m_WasResultCalled;
    int m_Running;

    uint64_t m_TimeStart;

    dmPlatform::HWindow   m_Window;
    dmGraphics::HContext  m_GraphicsContext;
    dmJobThread::HContext m_JobThread;

    ITest* m_Test;
    bool m_WindowClosed;

} g_EngineCtx;

struct ClearBackbufferTest : ITest
{
    void Initialize(EngineCtx* engine) override
    {
    }

    void Execute(EngineCtx* engine) override
    {
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
    }
};

struct ReadPixelsTest : ITest
{
    uint8_t m_Buffer[512 * 512 * 4];
    bool m_DidRead;

    void Initialize(EngineCtx* engine) override
    {
        m_DidRead = false;
        memset(m_Buffer, 0, sizeof(m_Buffer));
    }

    void Execute(EngineCtx* engine) override
    {
        static uint8_t color_r = 0;
        static uint8_t color_g = 80;
        static uint8_t color_b = 140;
        static uint8_t color_a = 255;

        dmGraphics::Clear(engine->m_GraphicsContext, dmGraphics::BUFFER_TYPE_COLOR0_BIT,
                                    (float) color_r,
                                    (float) color_g,
                                    (float) color_b,
                                    (float) color_a,
                                    1.0f, 0);

        int32_t x = 0, y = 0;
        uint32_t w = 0, h = 0;
        dmGraphics::GetViewport(engine->m_GraphicsContext, &x, &y, &w, &h);
        dmGraphics::ReadPixels(engine->m_GraphicsContext, x, y, w, h, m_Buffer, 512 * 512 * 4);
        dmLogInfo("%d, %d, %d, %d", m_Buffer[0], m_Buffer[1], m_Buffer[2], m_Buffer[3]);
    }
};

struct AsyncTextureUploadTest : ITest
{
    struct Texture
    {
        dmGraphics::HTexture m_Texture;
        dmGraphics::TextureParams m_Params;
    };

    dmArray<Texture> m_Textures;

    Texture NewTexture(dmGraphics::HContext context)
    {
        dmGraphics::TextureCreationParams creation_params;
        dmGraphics::TextureParams params;

        const uint32_t WIDTH = 128;
        const uint32_t HEIGHT = 128;

        creation_params.m_Width = WIDTH;
        creation_params.m_Height = HEIGHT;
        creation_params.m_OriginalWidth = WIDTH;
        creation_params.m_OriginalHeight = HEIGHT;

        params.m_DataSize = WIDTH * HEIGHT;
        params.m_Data = new char[params.m_DataSize];
        params.m_Width = WIDTH;
        params.m_Height = HEIGHT;
        params.m_Format = dmGraphics::TEXTURE_FORMAT_LUMINANCE;

        Texture texture = {};
        texture.m_Texture = dmGraphics::NewTexture(context, creation_params);
        texture.m_Params = params;

        return texture;
    }

    void CheckTexture(dmGraphics::HContext context, dmGraphics::HTexture texture)
    {
        dmGraphics::SetTextureParams(context, texture, dmGraphics::TEXTURE_FILTER_NEAREST, dmGraphics::TEXTURE_FILTER_NEAREST, dmGraphics::TEXTURE_WRAP_REPEAT, dmGraphics::TEXTURE_WRAP_REPEAT, 0.0f);
        dmGraphics::GetTextureResourceSize(context, texture);
        dmGraphics::GetTextureWidth(context, texture);
        dmGraphics::GetTextureHeight(context, texture);
        dmGraphics::GetTextureDepth(context, texture);
        dmGraphics::GetOriginalTextureWidth(context, texture);
        dmGraphics::GetOriginalTextureHeight(context, texture);
        dmGraphics::GetTextureMipmapCount(context, texture);
        dmGraphics::GetTextureType(context, texture);
        dmGraphics::GetNumTextureHandles(context, texture);
        dmGraphics::GetTextureUsageHintFlags(context, texture);

        dmGraphics::EnableTexture(context, 0, 0, texture);
        dmGraphics::DisableTexture(context, 0, texture);
    }

    void CreateTextures(EngineCtx* engine)
    {
        const uint32_t TEXTURE_COUNT = 512;
        m_Textures.SetCapacity(TEXTURE_COUNT);

        for (int i = 0; i < TEXTURE_COUNT; ++i)
        {
            if (!m_Textures.Full())
            {
                Texture t = NewTexture(engine->m_GraphicsContext);
                m_Textures.Push(t);

                Texture& back = m_Textures.Back();

                dmGraphics::SetTextureAsync(engine->m_GraphicsContext, back.m_Texture, t.m_Params, 0, 0);

                CheckTexture(engine->m_GraphicsContext, back.m_Texture);

                // Immediately delete, so we simulate putting them on a post-delete-queue
                dmGraphics::DeleteTexture(engine->m_GraphicsContext, back.m_Texture);
            }
        }
    }

    void Initialize(EngineCtx* engine) override
    {
        CreateTextures(engine);
    }

    void Execute(EngineCtx* engine) override
    {
        for (int i = 0; i < m_Textures.Size(); ++i)
        {
            if (!dmGraphics::IsAssetHandleValid(engine->m_GraphicsContext, m_Textures[i].m_Texture))
            {
                // Texture deleted
                free((void*)m_Textures[i].m_Params.m_Data);
                m_Textures.EraseSwap(i);
            }
            else
            {
                CheckTexture(engine->m_GraphicsContext, m_Textures[i].m_Texture);
            }
        }

        CreateTextures(engine);
    }
};

struct ComputeTest : ITest
{
    dmGraphics::HProgram         m_Program;
    dmGraphics::HUniformLocation m_UniformLoc;

    struct buf
    {
        float color[4];
    };

    void Initialize(EngineCtx* engine) override
    {
        dmGraphics::ShaderDesc compute_desc = {};

        if (dmGraphics::GetInstalledAdapterFamily() == dmGraphics::ADAPTER_FAMILY_OPENGL)
        {
            AddShader(&compute_desc, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM430, (uint8_t*) graphics_assets::glsl_compute_program, sizeof(graphics_assets::glsl_compute_program));
        }
        else
        {
            AddShader(&compute_desc, dmGraphics::ShaderDesc::LANGUAGE_SPIRV, (uint8_t*) graphics_assets::spirv_compute_program, sizeof(graphics_assets::spirv_compute_program));
        }

        dmGraphics::ShaderDesc::ResourceTypeInfo* type_info = AddShaderType(&compute_desc, "buf");
        AddShaderTypeMember(&compute_desc, type_info, "color", dmGraphics::ShaderDesc::ShaderDataType::SHADER_TYPE_VEC4);
        AddShaderResource(&compute_desc, "buf", 0, 0, 0, BINDING_TYPE_UNIFORM_BUFFER);

        m_Program = dmGraphics::NewProgram(engine->m_GraphicsContext, &compute_desc, 0, 0);

        DeleteShaderDesc(&compute_desc);

        m_UniformLoc = GetUniformLocation(m_Program, "buf");
    }

    void Execute(EngineCtx* engine) override
    {
        dmVMath::Vector4 color(1.0f, 0.0f, 0.0f, 1.0f);

        dmGraphics::EnableProgram(engine->m_GraphicsContext, m_Program);
        dmGraphics::SetConstantV4(engine->m_GraphicsContext, &color, 1, m_UniformLoc);

        dmGraphics::DispatchCompute(engine->m_GraphicsContext, 1, 1, 1);
        dmGraphics::DisableProgram(engine->m_GraphicsContext);
    }
};

// Note: the vulkan dmsdk doens't contain these functions anymore, but since SSBOs is something we want eventually,
//       we can leave this test code here for later.
#if 0
struct StorageBufferTest : ITest
{
    dmGraphics::HProgram           m_Program;
    dmGraphics::HStorageBuffer     m_StorageBuffer;
    dmGraphics::HVertexDeclaration m_VertexDeclaration;
    dmGraphics::HVertexBuffer      m_VertexBuffer;

    void Initialize(EngineCtx* engine) override
    {
        const float vertex_data_no_index[] = {
            -0.5f, -0.5f,
             0.5f, -0.5f,
            -0.5f,  0.5f,
             0.5f, -0.5f,
             0.5f,  0.5f,
            -0.5f,  0.5f,
        };

        m_VertexBuffer = dmGraphics::NewVertexBuffer(engine->m_GraphicsContext, sizeof(vertex_data_no_index), (void*) vertex_data_no_index, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

        dmGraphics::ShaderDesc vs_desc = {};
        dmGraphics::ShaderDesc fs_desc = {};

        if (dmGraphics::GetInstalledAdapterFamily() == dmGraphics::ADAPTER_FAMILY_OPENGL)
        {
            assert(false && "TODO: storage buffers are only supported on vulkan currently");
            AddShader(&vs_desc, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM430, (uint8_t*) graphics_assets::glsl_vertex_program, sizeof(graphics_assets::glsl_vertex_program));
            AddShader(&fs_desc, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM430, (uint8_t*) graphics_assets::glsl_fragment_program_ssbo, sizeof(graphics_assets::glsl_fragment_program_ssbo));
        }
        else
        {
            AddShader(&vs_desc, dmGraphics::ShaderDesc::LANGUAGE_SPIRV, (uint8_t*) graphics_assets::spirv_vertex_program, sizeof(graphics_assets::spirv_vertex_program));
            AddShader(&fs_desc, dmGraphics::ShaderDesc::LANGUAGE_SPIRV, (uint8_t*) graphics_assets::spirv_fragment_program_ssbo, sizeof(graphics_assets::spirv_fragment_program_ssbo));
        }

        AddShaderResource(&vs_desc, "pos", dmGraphics::ShaderDesc::ShaderDataType::SHADER_TYPE_VEC2, 0, 0, BINDING_TYPE_INPUT);
        AddShaderResource(&fs_desc, "Test", dmGraphics::ShaderDesc::SHADER_TYPE_STORAGE_BUFFER, 0, 1, BINDING_TYPE_STORAGE_BUFFER);

        dmGraphics::HVertexProgram vs_program   = dmGraphics::NewVertexProgram(engine->m_GraphicsContext, &vs_desc, 0, 0);
        dmGraphics::HFragmentProgram fs_program = dmGraphics::NewFragmentProgram(engine->m_GraphicsContext, &fs_desc, 0, 0);

        DeleteShaderDesc(&vs_desc);
        DeleteShaderDesc(&fs_desc);

        m_Program = dmGraphics::NewProgram(engine->m_GraphicsContext, vs_program, fs_program);

        dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(engine->m_GraphicsContext);
        dmGraphics::AddVertexStream(stream_declaration, "pos", 2, dmGraphics::TYPE_FLOAT, false);
        m_VertexDeclaration = dmGraphics::NewVertexDeclaration(engine->m_GraphicsContext, stream_declaration);

        struct StorageBuffer_Data
        {
            float m_Member1[4];
        };

        StorageBuffer_Data storage_data[16] = {};

        for (int i = 0; i < DM_ARRAY_SIZE(storage_data); ++i)
        {
            storage_data[i].m_Member1[0] = (float) (10 * i + 0);
            storage_data[i].m_Member1[1] = (float) (10 * i + 1);
            storage_data[i].m_Member1[2] = (float) (10 * i + 2);
            storage_data[i].m_Member1[3] = (float) (10 * i + 3);
        }

        m_StorageBuffer = dmGraphics::VulkanNewStorageBuffer(engine->m_GraphicsContext, sizeof(storage_data));
        dmGraphics::VulkanSetStorageBufferData(engine->m_GraphicsContext, m_StorageBuffer, sizeof(storage_data), (void*) storage_data);
    }

    void Execute(EngineCtx* engine) override
    {
        dmGraphics::EnableProgram(engine->m_GraphicsContext, m_Program);
        dmGraphics::EnableVertexBuffer(engine->m_GraphicsContext, m_VertexBuffer, 0);
        dmGraphics::EnableVertexDeclaration(engine->m_GraphicsContext, m_VertexDeclaration, 0, 0, m_Program);

        dmGraphics::HUniformLocation loc = GetUniformLocation(m_Program, "Test");
        dmGraphics::VulkanSetStorageBuffer(engine->m_GraphicsContext, m_StorageBuffer, 0, 0, loc);

        dmGraphics::Draw(engine->m_GraphicsContext, dmGraphics::PRIMITIVE_TRIANGLES, 0, 6, 1);
    }
};
#endif

static bool OnWindowClose(void* user_data)
{
    EngineCtx* engine = (EngineCtx*) user_data;
    engine->m_WindowClosed = 1;
    return true;
}

static void* EngineCreate(int argc, char** argv)
{
    EngineCtx* engine = &g_EngineCtx;
    engine->m_Window = dmPlatform::NewWindow();

    dmPlatform::WindowParams window_params = {};
    window_params.m_Width                  = 512;
    window_params.m_Height                 = 512;
    window_params.m_Title                  = "Graphics Test App";
    window_params.m_GraphicsApi            = dmPlatform::PLATFORM_GRAPHICS_API_VULKAN;
    window_params.m_CloseCallback          = OnWindowClose;
    window_params.m_CloseCallbackUserData  = (void*) engine;

    if (dmGraphics::GetInstalledAdapterFamily() == dmGraphics::ADAPTER_FAMILY_OPENGL)
    {
        window_params.m_GraphicsApi = dmPlatform::PLATFORM_GRAPHICS_API_OPENGL;
    }
    else if (dmGraphics::GetInstalledAdapterFamily() == dmGraphics::ADAPTER_FAMILY_OPENGLES)
    {
        window_params.m_GraphicsApi = dmPlatform::PLATFORM_GRAPHICS_API_OPENGLES;
    }

    dmPlatform::OpenWindow(engine->m_Window, window_params);
    dmPlatform::ShowWindow(engine->m_Window);

    dmJobThread::JobThreadCreationParams job_thread_create_param = {0};
    job_thread_create_param.m_ThreadCount = 1;
    engine->m_JobThread = dmJobThread::Create(job_thread_create_param);

    dmGraphics::ContextParams graphics_context_params = {};
    graphics_context_params.m_DefaultTextureMinFilter = dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
    graphics_context_params.m_DefaultTextureMagFilter = dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
    graphics_context_params.m_VerifyGraphicsCalls     = 1;
    graphics_context_params.m_UseValidationLayers     = 1;
    graphics_context_params.m_Window                  = engine->m_Window;
    graphics_context_params.m_Width                   = 512;
    graphics_context_params.m_Height                  = 512;
    graphics_context_params.m_JobThread               = engine->m_JobThread;

    engine->m_GraphicsContext = dmGraphics::NewContext(graphics_context_params);

    //engine->m_Test = new ComputeTest();
    //engine->m_Test = new StorageBufferTest();
    //engine->m_Test = new ReadPixelsTest();
    engine->m_Test = new AsyncTextureUploadTest();
    // engine->m_Test = new ClearBackbufferTest();
    engine->m_Test->Initialize(engine);

    engine->m_WasCreated++;
    engine->m_Running = 1;
    engine->m_TimeStart = dmTime::GetMonotonicTime();

    return &g_EngineCtx;
}

static void EngineDestroy(void* _engine)
{
    EngineCtx* engine = (EngineCtx*)_engine;
    dmGraphics::CloseWindow(engine->m_GraphicsContext);
    dmGraphics::DeleteContext(engine->m_GraphicsContext);
    dmGraphics::Finalize();

    if (engine->m_JobThread)
        dmJobThread::Destroy(engine->m_JobThread);

    engine->m_WasDestroyed++;
}

static UpdateResult EngineUpdate(void* _engine)
{
    EngineCtx* engine = (EngineCtx*)_engine;
    engine->m_WasRun++;
    uint64_t t = dmTime::GetMonotonicTime();
    /*
    float elapsed = (t - engine->m_TimeStart) / 1000000.0f;
    if (elapsed > 3.0f)
        return RESULT_EXIT;
    */

    if (!engine->m_Running)
    {
        return RESULT_EXIT;
    }

    dmPlatform::PollEvents(engine->m_Window);

    if (engine->m_WindowClosed)
    {
        return RESULT_EXIT;
    }

    dmJobThread::Update(engine->m_JobThread, 0);

    dmGraphics::BeginFrame(engine->m_GraphicsContext);

    engine->m_Test->Execute(engine);

    dmGraphics::Flip(engine->m_GraphicsContext);

    return RESULT_OK;
}

static void EngineGetResult(void* _engine, int* run_action, int* exit_code, int* argc, char*** argv)
{
    EngineCtx* ctx = (EngineCtx*)_engine;
    ctx->m_WasResultCalled++;
}

static void InstallAdapter(int argc, char **argv)
{
    dmGraphics::AdapterFamily family = dmGraphics::ADAPTER_FAMILY_VULKAN;

    for (int i = 0; i < argc; ++i)
    {
        if (strcmp(argv[i], "opengl") == 0)
        {
            family = dmGraphics::ADAPTER_FAMILY_OPENGL;
        }
    }

    dmGraphics::InstallAdapter(family);
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


    uint64_t t = dmTime::GetMonotonicTime();
    float elapsed = (t - g_EngineCtx.m_TimeStart) / 1000000.0f;
    (void)elapsed;

    ASSERT_EQ(1, ctx.m_Created);
    ASSERT_EQ(1, ctx.m_Destroyed);
    ASSERT_EQ(1, g_EngineCtx.m_WasCreated);
    //ASSERT_EQ(200, g_EngineCtx.m_WasRun);
    ASSERT_EQ(1, g_EngineCtx.m_WasDestroyed);
    ASSERT_EQ(1, g_EngineCtx.m_WasResultCalled);
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    InstallAdapter(argc, argv);
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
