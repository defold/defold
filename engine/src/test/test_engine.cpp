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
        m_Engine = dmEngine::New();
    }

    virtual void TearDown()
    {
        dmEngine::Delete(m_Engine);
    }

    float m_DT;
    dmEngine::HEngine m_Engine;
};

/*
 * TODO:
 * We should add watchdog support that exists the application after N frames or similar.
 */

TEST_F(EngineTest, EmptyNewDelete)
{
}

TEST_F(EngineTest, ProjectFail)
{
    const char* argv[] = {"test_engine", "test.projectc"};
    ASSERT_FALSE(dmEngine::Init(m_Engine, 2, (char**)argv));
}

TEST_F(EngineTest, Project)
{
    const char* argv[] = {"test_engine", "build/default/src/test/test.projectc"};

    ASSERT_TRUE(dmEngine::Init(m_Engine, 2, (char**)argv));

    ASSERT_EQ(0, dmEngine::Run(m_Engine));

    ASSERT_GT(dmEngine::GetFrameCount(m_Engine), 5u);
}

TEST_F(EngineTest, GuiRenderCrash)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/gui_render_crash/gui_render_crash.collectionc", "build/default/src/test/test.projectc"};

    ASSERT_TRUE(dmEngine::Init(m_Engine, 3, (char**)argv));

    ASSERT_EQ(0, dmEngine::Run(m_Engine));

    ASSERT_GT(dmEngine::GetFrameCount(m_Engine), 5u);
}


int g_PostExitResult = -1;
void HtttPostThread(void* params)
{
    uint32_t* port = (uint32_t*) params;
    char cmd[256];
    DM_SNPRINTF(cmd, sizeof(cmd), "python src/test/post_exit.py %d", *port);
    g_PostExitResult = system(cmd);
}

TEST_F(EngineTest, HttpPost)
{
    const char* argv[] = {"test_engine", "--config=bootstrap.main_collection=/http_post/http_post.collectionc", "build/default/src/test/test.projectc"};

    ASSERT_TRUE(dmEngine::Init(m_Engine, 3, (char**)argv));
    uint32_t port = dmEngine::GetHttpPort(m_Engine);
    dmThread::Thread thread = dmThread::New(HtttPostThread, 0x8000, &port);
    ASSERT_EQ(6, dmEngine::Run(m_Engine));
    dmThread::Join(thread);
    ASSERT_EQ(0, g_PostExitResult);
}

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
