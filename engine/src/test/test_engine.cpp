#include <gtest/gtest.h>

#include <assert.h>

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

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
