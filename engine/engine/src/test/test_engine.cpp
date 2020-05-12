#include <assert.h>

#include <dlib/http_client.h>
#include <dlib/thread.h>
#include <dlib/dstrings.h>
#include <dlib/profile.h>
#include "test_engine.h"
#include "../../../graphics/src/graphics_private.h"
#include "../engine.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#if defined(__NX__)
    #define CONTENT_ROOT "host:/src/test/build/default"
#else
    #define CONTENT_ROOT "src/test/build/default"
#endif

typedef void (*PreRun)(dmEngine::HEngine engine, void* context);
typedef void (*PostRun)(dmEngine::HEngine engine, void* context);

PreRun g_PreRun = 0;
PostRun g_PostRun = 0;
void* g_TextCtx = 0;

static dmEngine::HEngine TestEngineCreate(int argc, char** argv)
{
    dmEngine::HEngine engine = dmEngineCreate(argc, argv);

    if (g_PreRun)
        g_PreRun(engine, g_TextCtx);

    return engine;
}

// Destroys the engine instance after finalizing each system
static void TestEngineDestroy(dmEngine::HEngine engine)
{
    dmEngineDestroy(engine);

    if (g_PostRun)
        g_PostRun(engine, g_TextCtx);
}

static dmEngineUpdateResult TestEngineUpdate(dmEngine::HEngine engine)
{
    return dmEngineUpdate(engine);
}

static void TestEngineGetResult(dmEngine::HEngine engine, int* run_action, int* exit_code, int* argc, char*** argv)
{
    dmEngineGetResult(engine, run_action, exit_code, argc, argv);
}

static int Launch(int argc, char *argv[], PreRun pre_run, PostRun post_run, void* context)
{
    g_PreRun = pre_run;
    g_PostRun = post_run;
    g_TextCtx = context;

    dmEngine::RunLoopParams params;
    params.m_Argc = argc;
    params.m_Argv = argv;
    params.m_AppCtx = 0;
    params.m_AppCreate = 0;
    params.m_AppDestroy = 0;
    params.m_EngineCreate = (dmEngine::EngineCreate)TestEngineCreate;
    params.m_EngineDestroy = (dmEngine::EngineDestroy)TestEngineDestroy;
    params.m_EngineUpdate = (dmEngine::EngineUpdate)TestEngineUpdate;
    params.m_EngineGetResult = (dmEngine::EngineGetResult)TestEngineGetResult;
    return dmEngine::RunLoop(&params);
}



/*
 * TODO:
 * We should add watchdog support that exists the application after N frames or similar.
 */

TEST_F(EngineTest, ProjectFail)
{
    const char* argv[] = {"test_engine", CONTENT_ROOT "/notexist.projectc"};
    ASSERT_NE(0, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
}

static void PostRunFrameCount(dmEngine::HEngine engine, void* ctx)
{
    *((uint32_t*) ctx) = dmEngine::GetFrameCount(engine);
}

TEST_F(EngineTest, Project)
{
    uint32_t frame_count = 0;
    const char* argv[] = {"test_engine", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, PostRunFrameCount, &frame_count));
    ASSERT_GT(frame_count, 5u);
}

TEST_F(EngineTest, SharedLuaState)
{
    uint32_t frame_count = 0;
    const char* argv[] = {"test_engine", "--config=script.shared_state=1", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, PostRunFrameCount, &frame_count));
    ASSERT_GT(frame_count, 5u);
}

TEST_F(EngineTest, ArchiveNotFound)
{
    uint32_t frame_count = 0;
    const char* argv[] = {"test_engine", "--config=resource.uri=arc:not_found.arc", CONTENT_ROOT "/game.projectc"};
    Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, PostRunFrameCount, &frame_count);
}

TEST_F(EngineTest, GuiRenderCrash)
{
    uint32_t frame_count = 0;
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/gui_render_crash/gui_render_crash.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, PostRunFrameCount, &frame_count));
    ASSERT_GT(frame_count, 5u);
}

TEST_F(EngineTest, CrossScriptMessaging)
{
    uint32_t frame_count = 0;
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/cross_script_messaging/main.collectionc", "--config=bootstrap.render=/cross_script_messaging/default.renderc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, PostRunFrameCount, &frame_count));
    ASSERT_EQ(frame_count, 1u);
}

TEST_F(EngineTest, RenderScript)
{
    uint32_t frame_count = 0;
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/render_script/main.collectionc", "--config=bootstrap.render=/render_script/default.renderc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, PostRunFrameCount, &frame_count));
    ASSERT_EQ(frame_count, 1u);
}

struct HttpTestContext
{
    HttpTestContext()
    {
        memset(this, 0, sizeof(*this));
    }
    const char*      m_Script;
    uint32_t         m_Port;
    dmThread::Thread m_Thread;
    int              m_PreCount;
};

int g_PostExitResult = -1;
void HttpPostThread(void* params)
{
    HttpTestContext* http_ctx = (HttpTestContext*) params;
    char cmd[256];
    dmSnPrintf(cmd, sizeof(cmd), "python src/test/%s %d", http_ctx->m_Script, http_ctx->m_Port);
#if !defined(DM_NO_SYSTEM_FUNCTION)
    g_PostExitResult = system(cmd);
#endif
}

static void PreRunHttpPort(dmEngine::HEngine engine, void* ctx)
{
    HttpTestContext* http_ctx = (HttpTestContext*) ctx;
    if (http_ctx->m_PreCount == 0)
    {
        // Only for first callback in order to avoid loops when testing reboot
        http_ctx->m_Port = dmEngine::GetHttpPort(engine);
        http_ctx->m_Thread = dmThread::New(HttpPostThread, 0x8000, http_ctx, "post");
    }
    http_ctx->m_PreCount++;
}

#if !defined(__NX__)
TEST_F(EngineTest, HttpPost)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/http_post/http_post.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    HttpTestContext ctx;
    ctx.m_Script = "post_exit.py";

    ASSERT_EQ(6, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, PreRunHttpPort, 0, &ctx));
    dmThread::Join(ctx.m_Thread);
    ASSERT_EQ(0, g_PostExitResult);
}

TEST_F(EngineTest, Reboot)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/reboot/start.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(7, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, ConnectionReboot)
{
    const char* argv[] = {"test_engine", "--config=dmengine.unload_builtins=0"};
    HttpTestContext ctx;
    ctx.m_Script = "post_reboot.py";

    ASSERT_EQ(0, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, PreRunHttpPort, 0, &ctx));
    dmThread::Join(ctx.m_Thread);
    ASSERT_EQ(0, g_PostExitResult);
}
#endif

TEST_F(EngineTest, DEF_841)
{
    // DEF-841: do not attempt to fire Lua animation end callbacks using deleted ScriptInstances as targets.
    // See first.script for test details.
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/def-841/def-841.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, DEF_1077)
{
    // DEF-1077: Crash triggered by gui scene containing a fully filled pie node with rectangular bounds that precisely fills up the remaining
    //           capacity in the vertex buffer, fails to allocate memory.
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/def-1077/def-1077.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, DEF_1480)
{
    // DEF-1480: Crash when too many collection proxies were loaded (crashed during cleanup)
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/def-1480/main.collectionc", "--config=collection_proxy.max_count=8", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, DEF_3086)
{
    // DEF-3086: Loading two collectionproxies asnyc with same texture might leak memory.
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/def-3086/main.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, DEF_3154)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/def-3154/def-3154.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, DEF_3456)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/def-3456/def-3456.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, DEF_3575)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/def-3575/def-3575.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, SpineAnim)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/spine_anim/spine.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, SpineIK)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/spine_ik/spine_ik.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
}

#if !defined(__NX__) // until we've added suport for it
TEST_F(EngineTest, MemCpuProfiler)
{
    #ifndef SANITIZE_ADDRESS
        // DEF-3677
        // DE 20181217
        // When ASAN is enabled the amount of memory used (resident_size) actually increases after
        // the test collection is loaded. This is likley caused by the OS shuffling memory around
        // when ASAN is enabled since it add some overhead. Workaround is to disable this test.
        // Tried adding a big OGG file to the test data set but still the same result. The difference
        // between amount of allocated memory is over 20Mb less than before loading when ASAN is enabled.
        const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/profiler/profiler.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
        ASSERT_EQ(0, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
    #endif
}
#endif

// Verify that project.dependencies config entry is stripped during build.
TEST_F(EngineTest, ProjectDependency)
{
    // Regular project.dependencies entry
    const char* argv1[] = {"test_engine", "--config=bootstrap.main_collection=/project_conf/project_conf.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/project_conf/test1.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv1)/sizeof(argv1[0]), (char**)argv1, 0, 0, 0));

    // project.dependencies entry without spaces
    const char* argv2[] = {"test_engine", "--config=bootstrap.main_collection=/project_conf/project_conf.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/project_conf/test2.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv2)/sizeof(argv2[0]), (char**)argv2, 0, 0, 0));

    // Multiple project.dependencies entries
    const char* argv3[] = {"test_engine", "--config=bootstrap.main_collection=/project_conf/project_conf.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/project_conf/test3.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv3)/sizeof(argv3[0]), (char**)argv3, 0, 0, 0));
}

// Verify that the engine runs the init script at startup
TEST_F(EngineTest, RunScript)
{
    // Regular project.dependencies entry
    const char* argv1[] = {"test_engine", "--config=script.shared_state=1", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/init_script/game.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv1)/sizeof(argv1[0]), (char**)argv1, 0, 0, 0));

    // Two files in the same property "file1,file2"
    const char* argv2[] = {"test_engine", "--config=script.shared_state=1", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/init_script/game1.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv2)/sizeof(argv2[0]), (char**)argv2, 0, 0, 0));

    // Command line property
    // An init script that all it does is post an exit
    const char* argv3[] = {"test_engine", "--config=script.shared_state=1", "--config=bootstrap.debug_init_script=/init_script/init2.luac", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/init_script/game2.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv3)/sizeof(argv3[0]), (char**)argv3, 0, 0, 0));

    // Trying a non existing file
    const char* argv4[] = {"test_engine", "--config=script.shared_state=1", "--config=bootstrap.debug_init_script=/init_script/doesnt_exist.luac", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/init_script/game2.projectc"};
    ASSERT_NE(0, Launch(sizeof(argv4)/sizeof(argv4[0]), (char**)argv4, 0, 0, 0));

    // With a non shared context
    const char* argv5[] = {"test_engine", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/init_script/game.projectc"};
    ASSERT_EQ(0, Launch(sizeof(argv5)/sizeof(argv5[0]), (char**)argv5, 0, 0, 0));
}

#if !defined(__NX__) // until we support connections
TEST_F(EngineTest, ConnectionRunScript)
{
    const char* argv[] = {"test_engine", "--config=script.shared_state=1", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/init_script/game_connection.projectc"};
    HttpTestContext ctx;
    ctx.m_Script = "post_runscript.py";

    ASSERT_EQ(42, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, PreRunHttpPort, 0, &ctx));
    dmThread::Join(ctx.m_Thread);
    ASSERT_EQ(0, g_PostExitResult);
}
#endif

/* Draw Count */

TEST_P(DrawCountTest, DrawCount)
{
    const DrawCountParams& p = GetParam();

    char name[] = {"dmengine"};
    char state[] = {"--config=script.shared_state=1"};
    char dont_unload[] = {"--config=dmengine.unload_builtins=0"};
    char project[512];
    dmSnPrintf(project, sizeof(project), "%s%s", CONTENT_ROOT, p.m_ProjectPath);
    char* argv[] = {name, state, dont_unload, project};

    ASSERT_TRUE(dmEngine::Init(m_Engine, sizeof(argv)/sizeof(argv[0]), argv));

    for( int i = 0; i < p.m_NumSkipFrames; ++i )
    {
        dmEngine::Step(m_Engine);
    }

    dmEngine::Step(m_Engine);
    ASSERT_EQ(p.m_ExpectedDrawCount, dmGraphics::GetDrawCount());
}

DrawCountParams draw_count_params[] =
{
    {"/render/drawcall.projectc", 2, 2},    // 1 draw call for sprite, 1 for debug physics
};
INSTANTIATE_TEST_CASE_P(DrawCount, DrawCountTest, jc_test_values_in(draw_count_params));

#if !defined(__NX__) // until we support connections
// Test that we can reload a full collection containing a spine scene
// while the first gameobject has been already been deleted (marked for
// deletion through a `go.delete()` call, invalidating any "delayed delete"
// list entries).
TEST_F(EngineTest, DEF_3652)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/def-3652/def-3652.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    HttpTestContext ctx;
    ctx.m_Script = "/def-3652/post_reload_collection.py";

    ASSERT_EQ(0, Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, PreRunHttpPort, 0, &ctx));
    dmThread::Join(ctx.m_Thread);
    ASSERT_EQ(0, g_PostExitResult);
}
#endif

int main(int argc, char **argv)
{
    dmProfile::Initialize(256, 1024 * 16, 128);
    dmDDF::RegisterAllTypes();
    jc_test_init(&argc, argv);
    dmHashEnableReverseHash(true);
    dmGraphics::Initialize();

    int ret = jc_test_run_all();
    dmProfile::Finalize();
    return ret;
}
