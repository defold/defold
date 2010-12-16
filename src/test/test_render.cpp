#include <stdint.h>
#include <gtest/gtest.h>
#include "render/render.h"

class dmRenderTest : public ::testing::Test
{
protected:
    dmRender::HRenderContext m_Context;

    virtual void SetUp()
    {
        dmRender::RenderContextParams params;
        params.m_DisplayWidth = 600;
        params.m_DisplayHeight = 400;
        m_Context = dmRender::NewRenderContext(params);
    }

    virtual void TearDown()
    {
        dmRender::DeleteRenderContext(m_Context);
    }
};

TEST_F(dmRenderTest, TestContextNewDelete)
{

}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
