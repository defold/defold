#include <stdint.h>
#include <gtest/gtest.h>

#include <graphics/graphics_device.h>

#include "render/render.h"

class dmRenderTest : public ::testing::Test
{
protected:
    dmRender::HRenderContext m_Context;

    const static uint32_t WIDTH = 600;
    const static uint32_t HEIGHT = 600;

    virtual void SetUp()
    {
        dmRender::RenderContextParams params;
        params.m_DisplayWidth = WIDTH;
        params.m_DisplayHeight = HEIGHT;
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
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
