#include <gtest/gtest.h>

#include <assert.h>

#include <dlib/http_client.h>
#include <dlib/thread.h>
#include <dlib/dstrings.h>
#include <dlib/profile.h>
#include "../engine.h"

#define CONTENT_ROOT "src/test/build/default"

class EngineTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_DT = 1.0f / 60.0f;
    }

    virtual void TearDown()
    {
    }

    float m_DT;
};

/*
 * TODO:
 * We should add watchdog support that exists the application after N frames or similar.
 */

TEST_F(EngineTest, ProjectFail)
{
    const char* argv[] = {"test_engine", "game.projectc"};
    ASSERT_NE(0, dmEngine::Launch(2, (char**)argv, 0, 0, 0));
}

static void PostRunFrameCount(dmEngine::HEngine engine, void* ctx)
{
    *((uint32_t*) ctx) = dmEngine::GetFrameCount(engine);
}

TEST_F(EngineTest, Project)
{
    uint32_t frame_count = 0;
    const char* argv[] = {"test_engine", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(2, (char**)argv, 0, PostRunFrameCount, &frame_count));
    ASSERT_GT(frame_count, 5u);
}

TEST_F(EngineTest, SharedLuaState)
{
    uint32_t frame_count = 0;
    const char* argv[] = {"test_engine", "--config=script.shared_state=1", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(3, (char**)argv, 0, PostRunFrameCount, &frame_count));
    ASSERT_GT(frame_count, 5u);
}

TEST_F(EngineTest, ArchiveNotFound)
{
    uint32_t frame_count = 0;
    const char* argv[] = {"test_engine", "--config=resource.uri=arc:not_found.arc", CONTENT_ROOT "/game.projectc"};
    dmEngine::Launch(3, (char**)argv, 0, PostRunFrameCount, &frame_count);
}

TEST_F(EngineTest, GuiRenderCrash)
{
    uint32_t frame_count = 0;
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/gui_render_crash/gui_render_crash.collectionc", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(3, (char**)argv, 0, PostRunFrameCount, &frame_count));
    ASSERT_GT(frame_count, 5u);
}

TEST_F(EngineTest, CrossScriptMessaging)
{
    uint32_t frame_count = 0;
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/cross_script_messaging/main.collectionc", "--config=bootstrap.render=/cross_script_messaging/default.renderc", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(4, (char**)argv, 0, PostRunFrameCount, &frame_count));
    ASSERT_EQ(frame_count, 1u);
}

TEST_F(EngineTest, RenderScript)
{
    uint32_t frame_count = 0;
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/render_script/main.collectionc", "--config=bootstrap.render=/render_script/default.renderc", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(4, (char**)argv, 0, PostRunFrameCount, &frame_count));
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
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/http_post/http_post.collectionc", CONTENT_ROOT "/game.projectc"};
    HttpTestContext ctx;
    ctx.m_Script = "post_exit.py";

    ASSERT_EQ(6, dmEngine::Launch(3, (char**)argv, PreRunHttpPort, 0, &ctx));
    dmThread::Join(ctx.m_Thread);
    ASSERT_EQ(0, g_PostExitResult);
}

TEST_F(EngineTest, Reboot)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/reboot/start.collectionc", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(7, dmEngine::Launch(3, (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, ConnectionReboot)
{
    const char* argv[] = {"test_engine"};
    HttpTestContext ctx;
    ctx.m_Script = "post_reboot.py";

    ASSERT_EQ(0, dmEngine::Launch(1, (char**)argv, PreRunHttpPort, 0, &ctx));
    dmThread::Join(ctx.m_Thread);
    ASSERT_EQ(0, g_PostExitResult);
}

TEST_F(EngineTest, DEF_841)
{
    // DEF-841: do not attempt to fire Lua animation end callbacks using deleted ScriptInstances as targets.
    // See first.script for test details.
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/def-841/def-841.collectionc", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(3, (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, DEF_1077)
{
    // DEF-1077: Crash triggered by gui scene containing a fully filled pie node with rectangular bounds that precisely fills up the remaining
    //           capacity in the vertex buffer, fails to allocate memory.
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/def-1077/def-1077.collectionc", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(3, (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, DEF_1480)
{
    // DEF-1480: Crash when too many collection proxies were loaded (crashed during cleanup)
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/def-1480/main.collectionc", "--config=collection_proxy.max_count=8", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(4, (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, SpineAnim)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/spine_anim/spine.collectionc", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(3, (char**)argv, 0, 0, 0));
}

TEST_F(EngineTest, MemCpuProfiler)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/profiler/profiler.collectionc", CONTENT_ROOT "/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(3, (char**)argv, 0, 0, 0));
}

// Verify that project.dependencies config entry is stripped during build.
TEST_F(EngineTest, ProjectDependency)
{
    // Regular project.dependencies entry
    const char* argv1[] = {"test_engine", "--config=bootstrap.main_collection=/project_conf/project_conf.collectionc", CONTENT_ROOT "/project_conf/test1.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(3, (char**)argv1, 0, 0, 0));

    // project.dependencies entry without spaces
    const char* argv2[] = {"test_engine", "--config=bootstrap.main_collection=/project_conf/project_conf.collectionc", CONTENT_ROOT "/project_conf/test2.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(3, (char**)argv2, 0, 0, 0));

    // Multiple project.dependencies entries
    const char* argv3[] = {"test_engine", "--config=bootstrap.main_collection=/project_conf/project_conf.collectionc", CONTENT_ROOT "/project_conf/test3.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(3, (char**)argv3, 0, 0, 0));
}

// Verify that the engine runs the init script at startup
TEST_F(EngineTest, InitScript)
{
    // write a plain lua source file
    {
        FILE* f = fopen(CONTENT_ROOT "/init_script/init.lua", "wb");
        ASSERT_NE( (uintptr_t)0, (uintptr_t)f );
        const char* data = "globalvar = 1";
        fwrite(data, strlen(data), 1, f);
        fclose(f);
    }
    {
        FILE* f = fopen(CONTENT_ROOT "/init_script/init1.lua", "wb");
        ASSERT_NE( (uintptr_t)0, (uintptr_t)f );
        const char* data = "globalvar1 = 2";
        fwrite(data, strlen(data), 1, f);
        fclose(f);
    }
    {
        FILE* f = fopen(CONTENT_ROOT "/init_script/init2.lua", "wb");
        ASSERT_NE( (uintptr_t)0, (uintptr_t)f );
        const char* data = "globalvar2 = 3";
        fwrite(data, strlen(data), 1, f);
        fclose(f);
    }

    // Regular project.dependencies entry
    const char* argv1[] = {"test_engine", CONTENT_ROOT "/init_script/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(2, (char**)argv1, 0, 0, 0));

    // Two files in the same property "file1,file2"
    const char* argv2[] = {"test_engine", CONTENT_ROOT "/init_script/game1.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(2, (char**)argv2, 0, 0, 0));

    // Command line property
    // An init script that all it does is post an exit
    const char* argv3[] = {"test_engine", "--config=bootstrap.debug_init_script=/init_script/init2.lua", CONTENT_ROOT "/init_script/game2.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(3, (char**)argv3, 0, 0, 0));

    // Trying a non existing file
    const char* argv4[] = {"test_engine", "--config=bootstrap.debug_init_script=/init_script/doesnt_exist.lua", CONTENT_ROOT "/init_script/game2.projectc"};
    ASSERT_NE(0, dmEngine::Launch(3, (char**)argv4, 0, 0, 0));
}

int main(int argc, char **argv)
{
    dmProfile::Initialize(256, 1024 * 16, 128);
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    dmProfile::Finalize();
    return ret;
}
