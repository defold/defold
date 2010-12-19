#include <stdint.h>
#include <gtest/gtest.h>
#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/hash.h>

#include <graphics/graphics_device.h>

#include "render/render.h"

const static uint32_t WIDTH = 600;
const static uint32_t HEIGHT = 400;

class dmRenderTest : public ::testing::Test
{
protected:
    dmRender::HRenderContext m_Context;

    virtual void SetUp()
    {
        dmRender::RenderContextParams params;
        params.m_DisplayWidth = WIDTH;
        params.m_DisplayHeight = HEIGHT;
        params.m_MaxRenderTargets = 1;
        params.m_MaxInstances = 1;
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

TEST_F(dmRenderTest, TestGraphicsContext)
{
    ASSERT_NE((void*)0x0, dmRender::GetGraphicsContext(m_Context));
}

TEST_F(dmRenderTest, TestViewProj)
{
    Vectormath::Aos::Matrix4 view = Vectormath::Aos::Matrix4::rotationX(M_PI);
    view.setTranslation(Vectormath::Aos::Vector3(1.0f, 2.0f, 3.0f));
    Vectormath::Aos::Matrix4 proj = Vectormath::Aos::Matrix4::orthographic(0.0f, WIDTH, HEIGHT, 0.0f, 1.0f, -1.0f);
    Vectormath::Aos::Matrix4 viewproj = proj * view;

    dmRender::SetViewMatrix(m_Context, view);
    dmRender::SetProjectionMatrix(m_Context, proj);
    Vectormath::Aos::Matrix4 test = *dmRender::GetViewProjectionMatrix(m_Context);

    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            ASSERT_EQ(viewproj.getElem(i, j), test.getElem(i, j));
}

TEST_F(dmRenderTest, TestScreenSize)
{
    ASSERT_EQ(WIDTH, dmRender::GetDisplayWidth(m_Context));
    ASSERT_EQ(HEIGHT, dmRender::GetDisplayHeight(m_Context));
}

TEST_F(dmRenderTest, TestRenderObjects)
{
    dmRender::RenderObject ro;
    ASSERT_EQ(dmRender::RESULT_OK, AddToRender(m_Context, &ro));
    ASSERT_NE(dmRender::RESULT_OK, AddToRender(m_Context, &ro));
    ASSERT_EQ(dmRender::RESULT_OK, ClearRenderObjects(m_Context));
    ASSERT_EQ(dmRender::RESULT_OK, AddToRender(m_Context, &ro));
}

TEST_F(dmRenderTest, TestConstants)
{
    dmRender::RenderObject ro;

    Vectormath::Aos::Vector4 val(1.0f, 2.0f, 3.0f, 4.0f);

    ASSERT_EQ(0, ro.m_VertexConstantMask & 1);
    SetVertexConstant(&ro, 0, val);
    ASSERT_EQ(val.getX(), ro.m_VertexConstants[0].getX());
    ASSERT_EQ(val.getY(), ro.m_VertexConstants[0].getY());
    ASSERT_EQ(val.getZ(), ro.m_VertexConstants[0].getZ());
    ASSERT_EQ(1, ro.m_VertexConstantMask & 1);
    ResetVertexConstant(&ro, 0);
    ASSERT_EQ(0, ro.m_VertexConstantMask & 1);

    ASSERT_EQ(0, ro.m_FragmentConstantMask & 1);
    SetFragmentConstant(&ro, 0, val);
    ASSERT_EQ(val.getX(), ro.m_FragmentConstants[0].getX());
    ASSERT_EQ(val.getY(), ro.m_FragmentConstants[0].getY());
    ASSERT_EQ(val.getZ(), ro.m_FragmentConstants[0].getZ());
    ASSERT_EQ(1, ro.m_FragmentConstantMask & 1);
    ResetFragmentConstant(&ro, 0);
    ASSERT_EQ(0, ro.m_FragmentConstantMask & 1);
}

TEST_F(dmRenderTest, TestSquare2d)
{
    Square2d(m_Context, 10.0f, 20.0f, 30.0f, 40.0f, Vector4(0.1f, 0.2f, 0.3f, 0.4f));
}

TEST_F(dmRenderTest, TestLine2d)
{
    Line2D(m_Context, 10.0f, 20.0f, 30.0f, 40.0f, Vector4(0.1f, 0.2f, 0.3f, 0.4f), Vector4(0.1f, 0.2f, 0.3f, 0.4f));
}

TEST_F(dmRenderTest, TestLine3d)
{
    Line3D(m_Context, Point3(10.0f, 20.0f, 30.0f), Point3(10.0f, 20.0f, 30.0f), Vector4(0.1f, 0.2f, 0.3f, 0.4f), Vector4(0.1f, 0.2f, 0.3f, 0.4f));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
