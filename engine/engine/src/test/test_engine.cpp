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

int g_PostExitResult = -1;
void HttpPostThread(void* params)
{
    uint32_t* port = (uint32_t*) params;
    char cmd[256];
    DM_SNPRINTF(cmd, sizeof(cmd), "python src/test/post_exit.py %d", *port);
    g_PostExitResult = system(cmd);
}

struct HttpTestContext
{
    uint32_t m_Port;
    dmThread::Thread m_Thread;
};

static void PreRunHttpPort(dmEngine::HEngine engine, void* ctx)
{
    HttpTestContext* http_ctx = (HttpTestContext*) ctx;
    http_ctx->m_Port = dmEngine::GetHttpPort(engine);
    http_ctx->m_Thread = dmThread::New(HttpPostThread, 0x8000, &http_ctx->m_Port);
}

TEST_F(EngineTest, HttpPost)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/http_post/http_post.collectionc", "build/default/src/test/game.projectc"};
    HttpTestContext ctx;

    ASSERT_EQ(6, dmEngine::Launch(3, (char**)argv, PreRunHttpPort, 0, &ctx));
    dmThread::Join(ctx.m_Thread);
    ASSERT_EQ(0, g_PostExitResult);
}

TEST_F(EngineTest, Reboot)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/reboot/start.collectionc", "build/default/src/test/game.projectc"};
    ASSERT_EQ(7, dmEngine::Launch(3, (char**)argv, 0, 0, 0));
}

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
