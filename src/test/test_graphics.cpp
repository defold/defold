#include <stdint.h>
#include <gtest/gtest.h>

#include "graphics.h"
#include "null/null.h"

#define APP_TITLE "GraphicsTest"
#define WIDTH 8
#define HEIGHT 4

using namespace Vectormath::Aos;

class dmGraphicsTest : public ::testing::Test
{
protected:
    dmGraphics::HContext m_Context;
    dmGraphics::WindowResult m_WindowResult;

    virtual void SetUp()
    {
        m_Context = dmGraphics::NewContext();
        dmGraphics::WindowParams params;
        params.m_Title = APP_TITLE;
        params.m_Width = WIDTH;
        params.m_Height = HEIGHT;
        params.m_Fullscreen = false;
        params.m_PrintDeviceInfo = false;
        m_WindowResult = dmGraphics::OpenWindow(m_Context, &params);
    }

    virtual void TearDown()
    {
        dmGraphics::DeleteContext(m_Context);
    }
};

TEST_F(dmGraphicsTest, NewDeleteContext)
{
    ASSERT_NE((void*)0, m_Context);
    ASSERT_EQ(dmGraphics::WINDOW_RESULT_OK, m_WindowResult);
}

TEST_F(dmGraphicsTest, Flip)
{
    dmGraphics::Flip(m_Context);
}

TEST_F(dmGraphicsTest, Clear)
{
    const uint32_t flags = dmGraphics::BUFFER_TYPE_COLOR_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT | dmGraphics::BUFFER_TYPE_STENCIL_BIT;
    dmGraphics::Clear(m_Context, flags, 1, 1, 1, 1, 1.0f, 1);
    uint32_t data[WIDTH * HEIGHT];
    memset(data, 1, sizeof(data));
    ASSERT_EQ(0, memcmp(data, m_Context->m_CurrentFrameBuffer->m_ColorBuffer, sizeof(data)));
}

TEST_F(dmGraphicsTest, VertexBuffer)
{
    char data[16];
    memset(data, 1, sizeof(data));
    dmGraphics::HVertexBuffer vertex_buffer = dmGraphics::NewVertexBuffer(m_Context, 16, data, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
    dmGraphics::VertexBuffer* vb = (dmGraphics::VertexBuffer*)vertex_buffer;
    ASSERT_EQ(0, memcmp(data, vb->m_Buffer, sizeof(data)));

    memset(data, 2, sizeof(data));
    dmGraphics::SetVertexBufferData(vertex_buffer, sizeof(data), data, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
    ASSERT_EQ(0, memcmp(data, vb->m_Buffer, sizeof(data)));

    memset(&data[4], 3, 4);
    dmGraphics::SetVertexBufferSubData(vertex_buffer, 4, 4, &data[4]);
    ASSERT_EQ(0, memcmp(data, vb->m_Buffer, sizeof(data)));

    memset(data, 4, 4);
    void* copy = dmGraphics::MapVertexBuffer(vertex_buffer, dmGraphics::BUFFER_ACCESS_READ_WRITE);
    memcpy(copy, data, sizeof(data));
    ASSERT_NE(0, memcmp(data, vb->m_Buffer, sizeof(data)));
    ASSERT_TRUE(dmGraphics::UnmapVertexBuffer(vertex_buffer));
    ASSERT_EQ(0, memcmp(data, vb->m_Buffer, sizeof(data)));

    dmGraphics::DeleteVertexBuffer(vertex_buffer);
}

TEST_F(dmGraphicsTest, IndexBuffer)
{
    char data[16];
    memset(data, 1, sizeof(data));
    dmGraphics::HIndexBuffer index_buffer = dmGraphics::NewIndexBuffer(m_Context, 16, data, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
    dmGraphics::IndexBuffer* ib = (dmGraphics::IndexBuffer*)index_buffer;
    ASSERT_EQ(0, memcmp(data, ib->m_Buffer, sizeof(data)));

    memset(data, 2, sizeof(data));
    dmGraphics::SetIndexBufferData(index_buffer, sizeof(data), data, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
    ASSERT_EQ(0, memcmp(data, ib->m_Buffer, sizeof(data)));

    memset(&data[4], 3, 4);
    dmGraphics::SetIndexBufferSubData(index_buffer, 4, 4, &data[4]);
    ASSERT_EQ(0, memcmp(data, ib->m_Buffer, sizeof(data)));

    memset(data, 4, 4);
    void* copy = dmGraphics::MapIndexBuffer(index_buffer, dmGraphics::BUFFER_ACCESS_READ_WRITE);
    memcpy(copy, data, sizeof(data));
    ASSERT_NE(0, memcmp(data, ib->m_Buffer, sizeof(data)));
    ASSERT_TRUE(dmGraphics::UnmapVertexBuffer(index_buffer));
    ASSERT_EQ(0, memcmp(data, ib->m_Buffer, sizeof(data)));

    dmGraphics::DeleteIndexBuffer(index_buffer);
}

TEST_F(dmGraphicsTest, VertexDeclaration)
{
    float v[] = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f };
    dmGraphics::HVertexBuffer vertex_buffer = dmGraphics::NewVertexBuffer(m_Context, sizeof(v), (void*)v, dmGraphics::BUFFER_USAGE_STREAM_DRAW);

    dmGraphics::VertexElement ve[2] =
    {
        {0, 3, dmGraphics::TYPE_FLOAT, 0, 0 },
        {1, 2, dmGraphics::TYPE_FLOAT, 0, 0 }
    };
    dmGraphics::HVertexDeclaration vertex_declaration = dmGraphics::NewVertexDeclaration(m_Context, ve, 2);

    dmGraphics::EnableVertexDeclaration(m_Context, vertex_declaration, vertex_buffer);

    float p[] = { 0.0f, 1.0f, 2.0f, 5.0f, 6.0f, 7.0f };
    ASSERT_EQ(sizeof(p) / 2, m_Context->m_VertexStreams[0].m_Size);
    ASSERT_EQ(0, memcmp(p, m_Context->m_VertexStreams[0].m_Source, 3 * sizeof(float)));
    float uv[] = { 3.0f, 4.0f, 8.0f, 9.0f };
    ASSERT_EQ(sizeof(uv) / 2, m_Context->m_VertexStreams[1].m_Size);
    ASSERT_EQ(0, memcmp(uv, m_Context->m_VertexStreams[1].m_Source, 2 * sizeof(float)));

    dmGraphics::DisableVertexDeclaration(m_Context, vertex_declaration);

    ASSERT_EQ(0u, m_Context->m_VertexStreams[0].m_Size);
    ASSERT_EQ(0u, m_Context->m_VertexStreams[1].m_Size);

    dmGraphics::DeleteVertexDeclaration(vertex_declaration);
    dmGraphics::DeleteVertexBuffer(vertex_buffer);
}

TEST_F(dmGraphicsTest, VertexStream)
{
    float v[] = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f };

    dmGraphics::SetVertexStream(m_Context, 0, 3, dmGraphics::TYPE_FLOAT, 20, v);
    dmGraphics::SetVertexStream(m_Context, 1, 2, dmGraphics::TYPE_FLOAT, 20, &v[3]);

    float p[] = { 0.0f, 1.0f, 2.0f, 5.0f, 6.0f, 7.0f };
    ASSERT_EQ(sizeof(p) / 2, m_Context->m_VertexStreams[0].m_Size);
    ASSERT_EQ(0, memcmp(p, m_Context->m_VertexStreams[0].m_Source, 3 * sizeof(float)));
    float uv[] = { 3.0f, 4.0f, 8.0f, 9.0f };
    ASSERT_EQ(sizeof(uv) / 2, m_Context->m_VertexStreams[1].m_Size);
    ASSERT_EQ(0, memcmp(uv, m_Context->m_VertexStreams[1].m_Source, 2 * sizeof(float)));

    dmGraphics::DisableVertexStream(m_Context, 0);
    dmGraphics::DisableVertexStream(m_Context, 1);

    ASSERT_EQ(0u, m_Context->m_VertexStreams[0].m_Size);
    ASSERT_EQ(0u, m_Context->m_VertexStreams[1].m_Size);
}

TEST_F(dmGraphicsTest, Drawing)
{
    float v[] = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f };
    uint32_t i[] = { 0, 1, 2 };

    dmGraphics::SetVertexStream(m_Context, 0, 3, dmGraphics::TYPE_FLOAT, 20, v);
    dmGraphics::SetVertexStream(m_Context, 1, 2, dmGraphics::TYPE_FLOAT, 20, &v[3]);

    dmGraphics::DrawElements(m_Context, dmGraphics::PRIMITIVE_TRIANGLES, 3, dmGraphics::TYPE_UNSIGNED_INT, i);

    float p[] = { 0.0f, 1.0f, 2.0f, 5.0f, 6.0f, 7.0f, 10.0f, 11.0f, 12.0f };
    ASSERT_EQ(0, memcmp(p, m_Context->m_VertexStreams[0].m_Buffer, sizeof(p)));
    float uv[] = { 3.0f, 4.0f, 8.0f, 9.0f, 13.0f, 14.0f };
    ASSERT_EQ(0, memcmp(uv, m_Context->m_VertexStreams[1].m_Buffer, sizeof(uv)));

    dmGraphics::DisableVertexStream(m_Context, 0);
    dmGraphics::DisableVertexStream(m_Context, 1);

    dmGraphics::VertexElement ve[] =
    {
        {0, 3, dmGraphics::TYPE_FLOAT, 0, 0},
        {1, 2, dmGraphics::TYPE_FLOAT, 0, 0}
    };
    dmGraphics::HVertexDeclaration vd = dmGraphics::NewVertexDeclaration(m_Context, ve, 2);
    dmGraphics::HVertexBuffer vb = dmGraphics::NewVertexBuffer(m_Context, sizeof(v), v, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
    dmGraphics::HIndexBuffer ib = dmGraphics::NewIndexBuffer(m_Context, sizeof(i), i, dmGraphics::BUFFER_USAGE_STREAM_DRAW);

    dmGraphics::EnableVertexDeclaration(m_Context, vd, vb);
    dmGraphics::DrawRangeElements(m_Context, dmGraphics::PRIMITIVE_TRIANGLES, 0, 3, dmGraphics::TYPE_UNSIGNED_INT, ib);
    dmGraphics::DisableVertexDeclaration(m_Context, vd);

    dmGraphics::EnableVertexDeclaration(m_Context, vd, vb);
    dmGraphics::Draw(m_Context, dmGraphics::PRIMITIVE_TRIANGLES, 0, 3);
    dmGraphics::DisableVertexDeclaration(m_Context, vd);

    dmGraphics::DeleteIndexBuffer(ib);
    dmGraphics::DeleteVertexBuffer(vb);
    dmGraphics::DeleteVertexDeclaration(vd);
}

TEST_F(dmGraphicsTest, TestVertexProgram)
{
    char* program_data = new char[1024];
    dmGraphics::HVertexProgram vp = dmGraphics::NewVertexProgram(m_Context, program_data, 1024);
    delete [] program_data;
    dmGraphics::SetVertexProgram(m_Context, vp);
    Vector4 constant(1.0f, 2.0f, 3.0f, 4.0f);
    dmGraphics::SetVertexConstantBlock(m_Context, &constant, 0, 1);
    program_data = new char[1024];
    dmGraphics::ReloadVertexProgram(vp, program_data, 1024);
    delete [] program_data;
    dmGraphics::DeleteVertexProgram(vp);
}

TEST_F(dmGraphicsTest, TestFragmentProgram)
{
    char* program_data = new char[1024];
    dmGraphics::HFragmentProgram fp = dmGraphics::NewFragmentProgram(m_Context, program_data, 1024);
    delete [] program_data;
    dmGraphics::SetFragmentProgram(m_Context, fp);
    Vector4 constant(1.0f, 2.0f, 3.0f, 4.0f);
    dmGraphics::SetFragmentConstantBlock(m_Context, &constant, 0, 1);
    program_data = new char[1024];
    dmGraphics::ReloadFragmentProgram(fp, program_data, 1024);
    delete [] program_data;
    dmGraphics::DeleteFragmentProgram(fp);
}

TEST_F(dmGraphicsTest, TestViewport)
{
    dmGraphics::SetViewport(m_Context, WIDTH*2, HEIGHT*2);
}

TEST_F(dmGraphicsTest, TestTexture)
{
    dmGraphics::TextureParams params;
    params.m_DataSize = WIDTH * HEIGHT;
    params.m_Data = new char[params.m_DataSize];
    params.m_Width = WIDTH;
    params.m_Height = HEIGHT;
    params.m_Format = dmGraphics::TEXTURE_FORMAT_LUMINANCE;
    dmGraphics::HTexture texture = dmGraphics::NewTexture(m_Context, params);
    delete [] (char*)params.m_Data;
    dmGraphics::SetTextureUnit(m_Context, 0, texture);
    dmGraphics::DeleteTexture(texture);
}

TEST_F(dmGraphicsTest, TestRenderTarget)
{
    dmGraphics::TextureParams params[4];
    for (uint32_t i = 0; i < dmGraphics::MAX_BUFFER_TYPE_COUNT; ++i)
    {
        params[i].m_Width = WIDTH;
        params[i].m_Height = HEIGHT;
        params[i].m_Format = dmGraphics::TEXTURE_FORMAT_LUMINANCE;
    }
    uint32_t flags = dmGraphics::BUFFER_TYPE_COLOR_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT | dmGraphics::BUFFER_TYPE_STENCIL_BIT;
    dmGraphics::HRenderTarget target = dmGraphics::NewRenderTarget(m_Context, flags, params);
    dmGraphics::EnableRenderTarget(m_Context, target);
    dmGraphics::Clear(m_Context, flags, 1, 1, 1, 1, 1.0f, 1);
    uint32_t data[WIDTH * HEIGHT];
    memset(data, 1, sizeof(data));
    ASSERT_EQ(0, memcmp(data, m_Context->m_CurrentFrameBuffer->m_ColorBuffer, sizeof(data)));
    dmGraphics::DisableRenderTarget(m_Context, target);
    dmGraphics::DeleteRenderTarget(target);
}

TEST_F(dmGraphicsTest, TestMasks)
{
    dmGraphics::SetColorMask(m_Context, false, false, false, false);
    ASSERT_FALSE(m_Context->m_RedMask);
    ASSERT_FALSE(m_Context->m_GreenMask);
    ASSERT_FALSE(m_Context->m_BlueMask);
    ASSERT_FALSE(m_Context->m_AlphaMask);
    dmGraphics::SetColorMask(m_Context, true, true, true, true);
    ASSERT_TRUE(m_Context->m_RedMask);
    ASSERT_TRUE(m_Context->m_GreenMask);
    ASSERT_TRUE(m_Context->m_BlueMask);
    ASSERT_TRUE(m_Context->m_AlphaMask);

    dmGraphics::SetDepthMask(m_Context, false);
    ASSERT_FALSE(m_Context->m_DepthMask);
    dmGraphics::SetDepthMask(m_Context, true);
    ASSERT_TRUE(m_Context->m_DepthMask);

    dmGraphics::SetStencilMask(m_Context, 0u);
    ASSERT_EQ(0u, m_Context->m_StencilMask);
    dmGraphics::SetStencilMask(m_Context, ~0u);
    ASSERT_EQ(~0u, m_Context->m_StencilMask);
}

TEST_F(dmGraphicsTest, TestWindowParams)
{
    ASSERT_TRUE(dmGraphics::GetWindowState(m_Context, dmGraphics::WINDOW_STATE_OPENED));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
