// Creating a small app test for initializing an running a small graphics app

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <dlib/log.h>
#include <dlib/time.h>
#include "../graphics.h"

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
    dmLogParams params;
    dmLogInitialize(&params);

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

    dmGraphics::HContext m_GraphicsContext;
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
    graphics_context_params.m_VerifyGraphicsCalls = 0;
    graphics_context_params.m_RenderDocSupport = 0;

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
    window_params.m_Width = 0;
    window_params.m_Height = 0;
    window_params.m_Samples = 0;
    window_params.m_Title = "TestTitle!";
    window_params.m_Fullscreen = 1;
    window_params.m_PrintDeviceInfo = false;
    window_params.m_HighDPI = 0;

    (void)dmGraphics::OpenWindow(engine->m_GraphicsContext, &window_params);

    g_EngineCtx.m_WasCreated++;
    g_EngineCtx.m_TimeStart = dmTime::GetTime();
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
    if (elapsed > 3.0f)
        return RESULT_EXIT;

    static uint8_t color_r = 0;
    static uint8_t color_g = 80;
    static uint8_t color_b = 140;
    static uint8_t color_a = 255;
    dmGraphics::BeginFrame(engine->m_GraphicsContext);
    dmGraphics::SetViewport(engine->m_GraphicsContext, 0, 0, dmGraphics::GetWindowWidth(engine->m_GraphicsContext), dmGraphics::GetWindowHeight(engine->m_GraphicsContext));
    dmGraphics::Clear(engine->m_GraphicsContext, dmGraphics::BUFFER_TYPE_COLOR_BIT,
                                (float)color_r,
                                (float)color_g,
                                (float)color_b,
                                (float)color_a,
                                1.0f, 0);
    dmGraphics::Flip(engine->m_GraphicsContext);

    color_b += 2;
    color_g += 1;

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
