#include <gtest/gtest.h>

#include <assert.h>

#include <dlib/http_client.h>
#include <dlib/thread.h>
#include <dlib/dstrings.h>
#include "../engine.h"

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
    const char* argv[] = {"test_engine", "build/default/src/test/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(2, (char**)argv, 0, PostRunFrameCount, &frame_count));
    ASSERT_GT(frame_count, 5u);
}

TEST_F(EngineTest, GuiRenderCrash)
{
    uint32_t frame_count = 0;
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/gui_render_crash/gui_render_crash.collectionc", "build/default/src/test/game.projectc"};
    ASSERT_EQ(0, dmEngine::Launch(3, (char**)argv, 0, PostRunFrameCount, &frame_count));
    ASSERT_GT(frame_count, 5u);
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
    bool             m_PreCount;
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
        http_ctx->m_Thread = dmThread::New(HttpPostThread, 0x8000, http_ctx);
    }
    http_ctx->m_PreCount++;
}

TEST_F(EngineTest, HttpPost)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/http_post/http_post.collectionc", "build/default/src/test/game.projectc"};
    HttpTestContext ctx;
    ctx.m_Script = "post_exit.py";

    ASSERT_EQ(6, dmEngine::Launch(3, (char**)argv, PreRunHttpPort, 0, &ctx));
    dmThread::Join(ctx.m_Thread);
    ASSERT_EQ(0, g_PostExitResult);
}

TEST_F(EngineTest, Reboot)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/reboot/start.collectionc", "build/default/src/test/game.projectc"};
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

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
