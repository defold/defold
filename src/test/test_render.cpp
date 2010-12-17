#include <stdint.h>
#include <gtest/gtest.h>

#include <dlib/hash.h>

#include <graphics/graphics_device.h>

#include "render/render.h"

class dmRenderTest : public ::testing::Test
{
protected:
    dmRender::HRenderContext m_Context;

    const static uint32_t WIDTH = 600;
    const static uint32_t HEIGHT = 400;

    virtual void SetUp()
    {
        dmRender::RenderContextParams params;
        params.m_DisplayWidth = WIDTH;
        params.m_DisplayHeight = HEIGHT;
        params.m_MaxRenderTargets = 1;
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

TEST_F(dmRenderTest, TestRenderTarget)
{
    dmGraphics::HRenderTarget target = dmGraphics::NewRenderTarget(WIDTH, HEIGHT, dmGraphics::TEXTURE_FORMAT_LUMINANCE);
    dmGraphics::DeleteRenderTarget(target);
    uint32_t hash = dmHashString32("rt");
    ASSERT_EQ(0x0, dmRender::GetRenderTarget(m_Context, hash));
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::RegisterRenderTarget(m_Context, target, hash));
    ASSERT_NE((void*)0x0, dmRender::GetRenderTarget(m_Context, hash));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
