#include <gtest/gtest.h>

#include <assert.h>

#include <dlib/http_client.h>
#include <dlib/thread.h>
#include <dlib/dstrings.h>
#include <dlib/profile.h>
#include "test_engine.h"
#include "../../../graphics/src/graphics_private.h"

#define CONTENT_ROOT "src/test/build/default"


/*
 * TODO:
 * We should add watchdog support that exists the application after N frames or similar.
 */

TEST_F(EngineTest, ProjectFail)
{
    const char* argv[] = {"test_engine", "game.projectc"};
    ASSERT_NE(0, dmEngine::Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
}

static void PostRunFrameCount(dmEngine::HEngine engine, void* ctx)
{
    *((uint32_t*) ctx) = dmEngine::GetFrameCount(engine);
}

TEST_F(EngineTest, Project)
{
    uint32_t frame_count = 0;
    const char* argv[] = {"test_engine", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, PostRunFrameCount, &frame_count));
    ASSERT_GT(frame_count, 5u);
}

TEST_F(EngineTest, SharedLuaState)
{
    uint32_t frame_count = 0;
    const char* argv[] = {"test_engine", "--config=script.shared_state=1", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, PostRunFrameCount, &frame_count));
    ASSERT_GT(frame_count, 5u);
}

TEST_F(EngineTest, ArchiveNotFound)
{
    uint32_t frame_count = 0;
    const char* argv[] = {"test_engine", "--config=resource.uri=arc:not_found.arc", CONTENT_ROOT "/game.projectc"};
    dmEngine::Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, PostRunFrameCount, &frame_count);
}

TEST_F(EngineTest, GuiRenderCrash)
{
    uint32_t frame_count = 0;
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/gui_render_crash/gui_render_crash.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, PostRunFrameCount, &frame_count));
    ASSERT_GT(frame_count, 5u);
}

TEST_F(EngineTest, CrossScriptMessaging)
{
    uint32_t frame_count = 0;
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/cross_script_messaging/main.collectionc", "--config=bootstrap.render=/cross_script_messaging/default.renderc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, PostRunFrameCount, &frame_count));
    ASSERT_EQ(frame_count, 1u);
}

TEST_F(EngineTest, RenderScript)
{
    uint32_t frame_count = 0;
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/render_script/main.collectionc", "--config=bootstrap.render=/render_script/default.renderc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, PostRunFrameCount, &frame_count));
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
    DM_SNPRINTF(cmd, sizeof(cmd), "python src/test/%s %d", http_ctx->m_Script, http_ctx->m_Port);
    g_PostExitResult = system(cmd);
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

TEST_F(EngineTest, HttpPost)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/http_post/http_post.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    HttpTestContext ctx;
    ctx.m_Script = "post_exit.py";

    ASSERT_EQ(6, dmEngine::Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, PreRunHttpPort, 0, &ctx));
    dmThread::Join(ctx.m_Thread);
    ASSERT_EQ(0, g_PostExitResult);
}

TEST_F(EngineTest, Reboot)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/reboot/start.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(7, dmEngine::Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, ConnectionReboot)
{
    const char* argv[] = {"test_engine", "--config=dmengine.unload_builtins=0"};
    HttpTestContext ctx;
    ctx.m_Script = "post_reboot.py";

    ASSERT_EQ(0, dmEngine::Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, PreRunHttpPort, 0, &ctx));
    dmThread::Join(ctx.m_Thread);
    ASSERT_EQ(0, g_PostExitResult);
}

TEST_F(EngineTest, DEF_841)
{
    // DEF-841: do not attempt to fire Lua animation end callbacks using deleted ScriptInstances as targets.
    // See first.script for test details.
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/def-841/def-841.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, DEF_1077)
{
    // DEF-1077: Crash triggered by gui scene containing a fully filled pie node with rectangular bounds that precisely fills up the remaining
    //           capacity in the vertex buffer, fails to allocate memory.
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/def-1077/def-1077.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, DEF_1480)
{
    // DEF-1480: Crash when too many collection proxies were loaded (crashed during cleanup)
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/def-1480/main.collectionc", "--config=collection_proxy.max_count=8", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, DEF_3086)
{
    // DEF-3086: Loading two collectionproxies asnyc with same texture might leak memory.
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/def-3086/main.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, SpineAnim)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/spine_anim/spine.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, MemCpuProfiler)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/profiler/profiler.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, 0, 0, 0));
}

// Verify that project.dependencies config entry is stripped during build.
TEST_F(EngineTest, ProjectDependency)
{
    // Regular project.dependencies entry
    const char* argv1[] = {"test_engine", "--config=bootstrap.main_collection=/project_conf/project_conf.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/project_conf/test1.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(sizeof(argv1)/sizeof(argv1[0]), (char**)argv1, 0, 0, 0));

    // project.dependencies entry without spaces
    const char* argv2[] = {"test_engine", "--config=bootstrap.main_collection=/project_conf/project_conf.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/project_conf/test2.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(sizeof(argv2)/sizeof(argv2[0]), (char**)argv2, 0, 0, 0));

    // Multiple project.dependencies entries
    const char* argv3[] = {"test_engine", "--config=bootstrap.main_collection=/project_conf/project_conf.collectionc", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/project_conf/test3.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(sizeof(argv3)/sizeof(argv3[0]), (char**)argv3, 0, 0, 0));
}

// Verify that the engine runs the init script at startup
TEST_F(EngineTest, RunScript)
{
    // Regular project.dependencies entry
    const char* argv1[] = {"test_engine", "--config=script.shared_state=1", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/init_script/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(sizeof(argv1)/sizeof(argv1[0]), (char**)argv1, 0, 0, 0));

    // Two files in the same property "file1,file2"
    const char* argv2[] = {"test_engine", "--config=script.shared_state=1", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/init_script/game1.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(sizeof(argv2)/sizeof(argv2[0]), (char**)argv2, 0, 0, 0));

    // Command line property
    // An init script that all it does is post an exit
    const char* argv3[] = {"test_engine", "--config=script.shared_state=1", "--config=bootstrap.debug_init_script=/init_script/init2.luac", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/init_script/game2.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(sizeof(argv3)/sizeof(argv3[0]), (char**)argv3, 0, 0, 0));

    // Trying a non existing file
    const char* argv4[] = {"test_engine", "--config=script.shared_state=1", "--config=bootstrap.debug_init_script=/init_script/doesnt_exist.luac", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/init_script/game2.projectc"};
    ASSERT_NE(0, dmEngine::Launch(sizeof(argv4)/sizeof(argv4[0]), (char**)argv4, 0, 0, 0));

    // With a non shared context
    const char* argv5[] = {"test_engine", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/init_script/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(sizeof(argv5)/sizeof(argv5[0]), (char**)argv5, 0, 0, 0));
}

TEST_F(EngineTest, ConnectionRunScript)
{
    const char* argv[] = {"test_engine", "--config=script.shared_state=1", "--config=dmengine.unload_builtins=0", CONTENT_ROOT "/init_script/game_connection.projectc"};
    HttpTestContext ctx;
    ctx.m_Script = "post_runscript.py";

    ASSERT_EQ(42, dmEngine::Launch(sizeof(argv)/sizeof(argv[0]), (char**)argv, PreRunHttpPort, 0, &ctx));
    dmThread::Join(ctx.m_Thread);
    ASSERT_EQ(0, g_PostExitResult);
}

/* Draw Count */

TEST_P(DrawCountTest, DrawCount)
{
    const DrawCountParams& p = GetParam();

    char name[] = {"dmengine"};
    char state[] = {"--config=script.shared_state=1"};
    char dont_unload[] = {"--config=dmengine.unload_builtins=0"};
    char project[512];
    DM_SNPRINTF(project, sizeof(project), "%s%s", CONTENT_ROOT, p.m_ProjectPath);
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
INSTANTIATE_TEST_CASE_P(DrawCount, DrawCountTest, ::testing::ValuesIn(draw_count_params));


int main(int argc, char **argv)
{
    dmProfile::Initialize(256, 1024 * 16, 128);
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    dmProfile::Finalize();
    return ret;
}
