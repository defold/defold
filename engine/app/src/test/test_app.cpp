#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <app/app.h>

struct AppCtx
{
    int m_Created;
    int m_Destroyed;
};

static void AppCreate(void* _ctx)
{
    AppCtx* ctx = (AppCtx*)_ctx;
    ctx->m_Created = 1;
}

static void AppDestroy(void* _ctx)
{
    AppCtx* ctx = (AppCtx*)_ctx;
    ctx->m_Destroyed = 1;
}

struct EngineCtx
{
    int m_WasCreated;
    int m_WasRun;
    int m_WasDestroyed;
    int m_WasResultCalled;
} g_EngineCtx;

static void* EngineCreate(int argc, char** argv)
{
    g_EngineCtx.m_WasCreated++;
    return &g_EngineCtx;
}

static void EngineDestroy(void* _engine)
{
    EngineCtx* ctx = (EngineCtx*)_engine;
    ctx->m_WasDestroyed++;
}

static dmApp::Result EngineUpdate(void* _engine)
{
    EngineCtx* ctx = (EngineCtx*)_engine;
    ctx->m_WasRun++;
    if (ctx->m_WasRun==5)
        return dmApp::RESULT_REBOOT;
    if (ctx->m_WasRun==10)
        return dmApp::RESULT_EXIT;
    return dmApp::RESULT_OK;
}

static void EngineGetResult(void* _engine, int* run_action, int* exit_code, int* argc, char*** argv)
{
    EngineCtx* ctx = (EngineCtx*)_engine;
    ctx->m_WasResultCalled++;
}

TEST(dmApp, Run)
{
    AppCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    memset(&g_EngineCtx, 0, sizeof(g_EngineCtx));

    dmApp::Params params;
    params.m_AppCtx = &ctx;
    params.m_AppCreate = AppCreate;
    params.m_AppDestroy = AppDestroy;
    params.m_EngineCreate = EngineCreate;
    params.m_EngineDestroy = EngineDestroy;
    params.m_EngineUpdate = EngineUpdate;
    params.m_EngineGetResult = EngineGetResult;

    int ret = dmApp::Run(&params);
    ASSERT_EQ(0, ret);

    ASSERT_EQ(1, ctx.m_Created);
    ASSERT_EQ(1, ctx.m_Destroyed);
    ASSERT_EQ(2, g_EngineCtx.m_WasCreated);
    ASSERT_EQ(10, g_EngineCtx.m_WasRun);
    ASSERT_EQ(2, g_EngineCtx.m_WasDestroyed);
    ASSERT_EQ(2, g_EngineCtx.m_WasResultCalled);
}


int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
