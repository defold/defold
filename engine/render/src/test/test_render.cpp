#include <stdint.h>
#include <gtest/gtest.h>
#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/hash.h>
#include <dlib/math.h>

#include "render/render.h"
#include "render/render_private.h"

const static uint32_t WIDTH = 600;
const static uint32_t HEIGHT = 400;

using namespace Vectormath::Aos;

class dmRenderTest : public ::testing::Test
{
protected:
    dmRender::HRenderContext m_Context;
    dmGraphics::HContext m_GraphicsContext;

    virtual void SetUp()
    {
        m_GraphicsContext = dmGraphics::NewContext(dmGraphics::ContextParams());
        dmRender::RenderContextParams params;
        params.m_MaxRenderTargets = 1;
        params.m_MaxInstances = 2;
        m_Context = dmRender::NewRenderContext(m_GraphicsContext, params);
    }

    virtual void TearDown()
    {
        dmRender::DeleteRenderContext(m_Context, 0);
        dmGraphics::DeleteContext(m_GraphicsContext);
    }
};

TEST_F(dmRenderTest, TestContextNewDelete)
{

}

TEST_F(dmRenderTest, TestRenderTarget)
{
    dmGraphics::TextureParams params[dmGraphics::MAX_BUFFER_TYPE_COUNT];
    params[0].m_Width = WIDTH;
    params[0].m_Height = HEIGHT;
    params[0].m_Format = dmGraphics::TEXTURE_FORMAT_LUMINANCE;
    params[1].m_Width = WIDTH;
    params[1].m_Height = HEIGHT;
    params[1].m_Format = dmGraphics::TEXTURE_FORMAT_DEPTH;
    uint32_t flags = dmGraphics::BUFFER_TYPE_COLOR_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT;
    dmGraphics::HRenderTarget target = dmGraphics::NewRenderTarget(m_GraphicsContext, flags, params);
    dmGraphics::DeleteRenderTarget(target);
    dmhash_t hash = dmHashString64("rt");
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
    const Vectormath::Aos::Matrix4& test = dmRender::GetViewProjectionMatrix(m_Context);

    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            ASSERT_EQ(viewproj.getElem(i, j), test.getElem(i, j));
}

TEST_F(dmRenderTest, TestRenderObjects)
{
    dmRender::RenderObject ro;
    ASSERT_EQ(dmRender::RESULT_OK, AddToRender(m_Context, &ro));
    ASSERT_EQ(dmRender::RESULT_OK, AddToRender(m_Context, &ro));
    ASSERT_NE(dmRender::RESULT_OK, AddToRender(m_Context, &ro));
    ASSERT_EQ(dmRender::RESULT_OK, ClearRenderObjects(m_Context));
    ASSERT_EQ(dmRender::RESULT_OK, AddToRender(m_Context, &ro));
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

TEST_F(dmRenderTest, TestDraw)
{
    dmRender::RenderObject ro_neg_z;
    ro_neg_z.m_RenderKey.m_Depth = 1;
    dmRender::RenderObject ro_pos_z;
    ro_pos_z.m_RenderKey.m_Depth = 0;

    ASSERT_EQ(0u, m_Context->m_RenderObjects.Size());

    dmRender::AddToRender(m_Context, &ro_neg_z);
    dmRender::AddToRender(m_Context, &ro_pos_z);
    ASSERT_LT(0u, m_Context->m_RenderObjects.Size());
    ASSERT_EQ((void*)&ro_neg_z, (void*)m_Context->m_RenderObjects[0]);
    ASSERT_EQ((void*)&ro_pos_z, (void*)m_Context->m_RenderObjects[1]);

    dmRender::Draw(m_Context, 0x0, 0);

    ASSERT_EQ((void*)&ro_pos_z, (void*)m_Context->m_RenderObjects[0]);
    ASSERT_EQ((void*)&ro_neg_z, (void*)m_Context->m_RenderObjects[1]);

    // Change draw-order
    ro_neg_z.m_RenderKey.m_Depth = 0;
    ro_pos_z.m_RenderKey.m_Depth = 1;

    dmRender::Draw(m_Context, 0x0, 0);

    ASSERT_EQ((void*)&ro_neg_z, (void*)m_Context->m_RenderObjects[0]);
    ASSERT_EQ((void*)&ro_pos_z, (void*)m_Context->m_RenderObjects[1]);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
