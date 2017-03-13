#include <stdint.h>
#include <gtest/gtest.h>
#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/hash.h>
#include <dlib/math.h>

#include <script/script.h>

#include "render/render.h"
#include "render/render_private.h"
#include "render/font_renderer_private.h"

const static uint32_t WIDTH = 600;
const static uint32_t HEIGHT = 400;

using namespace Vectormath::Aos;

class dmRenderTest : public ::testing::Test
{
protected:
    dmRender::HRenderContext m_Context;
    dmGraphics::HContext m_GraphicsContext;
    dmScript::HContext m_ScriptContext;
    dmRender::HFontMap m_SystemFontMap;

    virtual void SetUp()
    {
        m_GraphicsContext = dmGraphics::NewContext(dmGraphics::ContextParams());
        dmRender::RenderContextParams params;
        m_ScriptContext = dmScript::NewContext(0, 0, true);
        params.m_MaxRenderTargets = 1;
        params.m_MaxInstances = 2;
        params.m_ScriptContext = m_ScriptContext;
        params.m_MaxDebugVertexCount = 256;
        m_Context = dmRender::NewRenderContext(m_GraphicsContext, params);

        dmRender::FontMapParams font_map_params;
        font_map_params.m_CacheWidth = 128;
        font_map_params.m_CacheHeight = 128;
        font_map_params.m_CacheCellWidth = 8;
        font_map_params.m_CacheCellHeight = 8;
        font_map_params.m_MaxAscent = 2;
        font_map_params.m_MaxDescent = 1;
        font_map_params.m_Glyphs.SetCapacity(128);
        font_map_params.m_Glyphs.SetSize(128);
        memset((void*)&font_map_params.m_Glyphs[0], 0, sizeof(dmRender::Glyph)*128);
        for (uint32_t i = 0; i < 128; ++i)
        {
            font_map_params.m_Glyphs[i].m_Character = i;
            font_map_params.m_Glyphs[i].m_Width = 1;
            font_map_params.m_Glyphs[i].m_LeftBearing = 1;
            font_map_params.m_Glyphs[i].m_Advance = 2;
            font_map_params.m_Glyphs[i].m_Ascent = 2;
            font_map_params.m_Glyphs[i].m_Descent = 1;
        }
        m_SystemFontMap = dmRender::NewFontMap(m_GraphicsContext, font_map_params);
    }

    virtual void TearDown()
    {
        dmRender::DeleteRenderContext(m_Context, 0);
        dmRender::DeleteFontMap(m_SystemFontMap);
        dmGraphics::DeleteContext(m_GraphicsContext);
        dmScript::DeleteContext(m_ScriptContext);
    }
};

TEST_F(dmRenderTest, TestContextNewDelete)
{

}

TEST_F(dmRenderTest, TestRenderTarget)
{
    dmGraphics::TextureCreationParams creation_params[dmGraphics::MAX_BUFFER_TYPE_COUNT];
    dmGraphics::TextureParams params[dmGraphics::MAX_BUFFER_TYPE_COUNT];

    creation_params[0].m_Width = WIDTH;
    creation_params[0].m_Height = HEIGHT;
    creation_params[1].m_Width = WIDTH;
    creation_params[1].m_Height = HEIGHT;

    params[0].m_Width = WIDTH;
    params[0].m_Height = HEIGHT;
    params[0].m_Format = dmGraphics::TEXTURE_FORMAT_LUMINANCE;
    params[1].m_Width = WIDTH;
    params[1].m_Height = HEIGHT;
    params[1].m_Format = dmGraphics::TEXTURE_FORMAT_DEPTH;
    uint32_t flags = dmGraphics::BUFFER_TYPE_COLOR_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT;
    dmGraphics::HRenderTarget target = dmGraphics::NewRenderTarget(m_GraphicsContext, flags, creation_params, params);
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

struct TestDrawDispatchCtx
{
    int m_BeginCalls;
    int m_BatchCalls;
    int m_EndCalls;
    int m_EntriesRendered;
    int m_Order;
    float m_Z;
    int m_DrawnBefore;
    int m_DrawnWorld;
    int m_DrawnAfter;
};

static void TestDrawDispatch(dmRender::RenderListDispatchParams const & params)
{
    TestDrawDispatchCtx *ctx = (TestDrawDispatchCtx*) params.m_UserData;
    uint32_t* i;

    switch (params.m_Operation)
    {
        case dmRender::RENDER_LIST_OPERATION_BEGIN:
            ASSERT_EQ(ctx->m_BatchCalls, 0);
            ASSERT_EQ(ctx->m_EndCalls, 0);
            ctx->m_BeginCalls++;
            break;
        case dmRender::RENDER_LIST_OPERATION_BATCH:
            ASSERT_EQ(ctx->m_BeginCalls, 1);
            ASSERT_EQ(ctx->m_EndCalls, 0);
            ctx->m_BatchCalls++;
            for (i=params.m_Begin;i!=params.m_End;i++)
            {
                if (params.m_Buf[*i].m_MajorOrder != dmRender::RENDER_ORDER_WORLD)
                {
                    // verify strictly increasing order
                    ASSERT_GT(params.m_Buf[*i].m_Order, ctx->m_Order);
                    ctx->m_Order = params.m_Buf[*i].m_Order;
                }
                else
                {
                    // verify strictly increasing z for world entries.
                    ASSERT_GT(params.m_Buf[*i].m_WorldPosition.getZ(), ctx->m_Z);
                    ctx->m_Z = params.m_Buf[*i].m_WorldPosition.getZ();
                    // reset order for the _AFTER_WORLD batch
                    ctx->m_Order = 0;
                }

                ctx->m_EntriesRendered++;

                // verify sequence of things.
                switch (params.m_Buf[*i].m_MajorOrder)
                {
                    case dmRender::RENDER_ORDER_BEFORE_WORLD:
                        ctx->m_DrawnBefore = 1;
                        ASSERT_EQ(ctx->m_DrawnWorld, 0);
                        ASSERT_EQ(ctx->m_DrawnAfter, 0);
                        break;
                    case dmRender::RENDER_ORDER_WORLD:
                        ctx->m_DrawnWorld = 1;
                        ASSERT_EQ(ctx->m_DrawnBefore, 1);
                        ASSERT_EQ(ctx->m_DrawnAfter, 0);
                        break;
                    case dmRender::RENDER_ORDER_AFTER_WORLD:
                        ctx->m_DrawnAfter = 1;
                        ASSERT_EQ(ctx->m_DrawnBefore, 1);
                        ASSERT_EQ(ctx->m_DrawnWorld, 1);
                        break;
                }
            }
            break;
        default:
            ASSERT_EQ(params.m_Operation, dmRender::RENDER_LIST_OPERATION_END);
            ASSERT_EQ(ctx->m_BeginCalls, 1);
            ASSERT_EQ(ctx->m_EndCalls, 0);
            ctx->m_EndCalls++;
            break;
    }
}

TEST_F(dmRenderTest, TestRenderListDraw)
{
    TestDrawDispatchCtx ctx;
    memset(&ctx, 0x00, sizeof(TestDrawDispatchCtx));

    Vectormath::Aos::Matrix4 view = Vectormath::Aos::Matrix4::identity();
    Vectormath::Aos::Matrix4 proj = Vectormath::Aos::Matrix4::orthographic(0.0f, WIDTH, HEIGHT, 0.0f, 0.1f, 1.0f);
    dmRender::SetViewMatrix(m_Context, view);
    dmRender::SetProjectionMatrix(m_Context, proj);

    dmRender::RenderListBegin(m_Context);

    uint8_t dispatch = dmRender::RenderListMakeDispatch(m_Context, TestDrawDispatch, &ctx);

    const uint32_t n = 32;

    const uint32_t orders[n] = {
        99999,99998,99997,57734,75542,86333,64399,20415,15939,58565,34577,9813,3428,5503,49328,
        25189,24801,18298,83657,55459,27204,69430,72376,37545,43725,54023,68259,85984,6852,34106,
        37169,55555,
    };

    const dmRender::RenderOrder majors[3] = {
        dmRender::RENDER_ORDER_BEFORE_WORLD,
        dmRender::RENDER_ORDER_WORLD,
        dmRender::RENDER_ORDER_AFTER_WORLD
    };

    dmRender::RenderListEntry* out = dmRender::RenderListAlloc(m_Context, n);

    for (uint32_t i=0;i!=n;i++)
    {
        dmRender::RenderListEntry & entry = out[i];
        entry.m_WorldPosition = Point3(0,0,orders[i]);
        entry.m_MajorOrder = majors[i % 3];
        entry.m_TagMask = 0;
        entry.m_Order = orders[i];
        entry.m_BatchKey = i & 3; // no particular system
        entry.m_Dispatch = dispatch;
        entry.m_UserData = 0;
    }

    dmRender::RenderListSubmit(m_Context, out, out + n);
    dmRender::RenderListEnd(m_Context);

    dmRender::DrawRenderList(m_Context, 0, 0);

    ASSERT_EQ(ctx.m_BeginCalls, 1);
    ASSERT_GT(ctx.m_BatchCalls, 1);
    ASSERT_EQ(ctx.m_EntriesRendered, n);
    ASSERT_EQ(ctx.m_EndCalls, 1);
    ASSERT_EQ(ctx.m_Order, orders[2]); // highest after world entry
    ASSERT_EQ(ctx.m_Z, orders[1]);
}

TEST_F(dmRenderTest, TestRenderListDebug)
{
    // Test submitting debug drawing when there is no other drawing going on

    // See DEF-1475 Crash: Physics debug with one GO containing collision object crashes engine
    //
    //   Reason: Render system failed to set Capacity properly to account for debug
    //           render objects being flushed in during render.

    Vectormath::Aos::Matrix4 view = Vectormath::Aos::Matrix4::identity();
    Vectormath::Aos::Matrix4 proj = Vectormath::Aos::Matrix4::orthographic(0.0f, WIDTH, HEIGHT, 0.0f, 0.1f, 1.0f);
    dmRender::SetViewMatrix(m_Context, view);
    dmRender::SetProjectionMatrix(m_Context, proj);

    dmRender::RenderListBegin(m_Context);
    dmRender::Square2d(m_Context, 0, 0, 100, 100, Vector4(0,0,0,0));
    dmRender::RenderListEnd(m_Context);

    dmRender::DrawRenderList(m_Context, 0, 0);
    dmRender::DrawDebug2d(m_Context);
    dmRender::DrawDebug3d(m_Context);
}

static float Metric(const char* text, int n)
{
    return n * 4;
}

#define ASSERT_LINE(index, count, lines, i)\
    ASSERT_EQ(char_width * count, lines[i].m_Width);\
    ASSERT_EQ(index, lines[i].m_Index);\
    ASSERT_EQ(count, lines[i].m_Count);

TEST(dmFontRenderer, Layout)
{
    const uint32_t lines_count = 256;
    dmRender::TextLine lines[lines_count];
    int total_lines;
    const float char_width = 4;
    float w;
    total_lines = dmRender::Layout("", 100, lines, lines_count, &w, Metric);
    ASSERT_EQ(0, total_lines);
    ASSERT_EQ(0, w);

    total_lines = dmRender::Layout("x", 100, lines, lines_count, &w, Metric);
    ASSERT_EQ(1, total_lines);
    ASSERT_LINE(0, 1, lines, 0);
    ASSERT_EQ(char_width * 1, w);

    total_lines = dmRender::Layout("x\x00 123", 100, lines, lines_count, &w, Metric);
    ASSERT_EQ(1, total_lines);
    ASSERT_LINE(0, 1, lines, 0);
    ASSERT_EQ(char_width * 1, w);

    total_lines = dmRender::Layout("x", 0, lines, lines_count, &w, Metric);
    ASSERT_EQ(1, total_lines);
    ASSERT_LINE(0, 1, lines, 0);
    ASSERT_EQ(char_width * 1, w);

    total_lines = dmRender::Layout("foo", 3 * char_width, lines, lines_count, &w, Metric);
    ASSERT_EQ(1, total_lines);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_EQ(char_width * 3, w);

    total_lines = dmRender::Layout("foo", 3 * char_width - 1, lines, lines_count, &w, Metric);
    ASSERT_EQ(1, total_lines);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_EQ(char_width * 3, w);

    total_lines = dmRender::Layout("foo bar", 3 * char_width, lines, lines_count, &w, Metric);
    ASSERT_EQ(2, total_lines);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_LINE(4, 3, lines, 1);
    ASSERT_EQ(char_width * 3, w);

    total_lines = dmRender::Layout("foo bar", 1000, lines, lines_count, &w, Metric);
    ASSERT_EQ(1, total_lines);
    ASSERT_LINE(0, 7, lines, 0);
    ASSERT_EQ(char_width * 7, w);

    total_lines = dmRender::Layout("foo  bar", 1000, lines, lines_count, &w, Metric);
    ASSERT_EQ(1, total_lines);
    ASSERT_LINE(0, 8, lines, 0);
    ASSERT_EQ(char_width * 8, w);

    total_lines = dmRender::Layout("foo\n\nbar", 3 * char_width, lines, lines_count, &w, Metric);
    ASSERT_EQ(3, total_lines);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_LINE(4, 0, lines, 1);
    ASSERT_LINE(5, 3, lines, 2);
    ASSERT_EQ(char_width * 3, w);

    // åäö
    total_lines = dmRender::Layout("\xc3\xa5\xc3\xa4\xc3\xb6", 3 * char_width, lines, lines_count, &w, Metric);
    ASSERT_EQ(1, total_lines);
    ASSERT_EQ(char_width * 3, lines[0].m_Width);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_EQ(char_width * 3, w);

    total_lines = dmRender::Layout("Welcome to the Kingdom of Games...", 0, lines, lines_count, &w, Metric);
    ASSERT_EQ(6, total_lines);
    ASSERT_LINE(0, 7, lines, 0);
    ASSERT_LINE(8, 2, lines, 1);
    ASSERT_LINE(11, 3, lines, 2);
    ASSERT_LINE(15, 7, lines, 3);
    ASSERT_LINE(23, 2, lines, 4);
    ASSERT_LINE(26, 8, lines, 5);
    ASSERT_EQ(char_width * 8, w);
}

static inline float ExpectedHeight(float line_height, float num_lines, float leading)
{
    return num_lines * (line_height * fabsf(leading)) - line_height * (fabsf(leading) - 1.0f);
}

TEST_F(dmRenderTest, GetTextMetrics)
{
    dmRender::TextMetrics metrics;

    const int charwidth     = 2;
    const int ascent        = 2;
    const int descent       = 1;
    const int lineheight    = ascent + descent;

    dmRender::GetTextMetrics(m_SystemFontMap, "Hello World", 0, false, 1.0f, 0.0f, &metrics);
    ASSERT_EQ(ascent, metrics.m_MaxAscent);
    ASSERT_EQ(descent, metrics.m_MaxDescent);
    ASSERT_EQ(charwidth*11, metrics.m_Width);
    ASSERT_EQ(lineheight*1, metrics.m_Height);

    // line break in the middle of the sentence
    int numlines = 2;

    dmRender::GetTextMetrics(m_SystemFontMap, "Hello World", 8*charwidth, true, 1.0f, 0.0f, &metrics);
    ASSERT_EQ(ascent, metrics.m_MaxAscent);
    ASSERT_EQ(descent, metrics.m_MaxDescent);
    ASSERT_EQ(charwidth*5, metrics.m_Width);
    ASSERT_EQ(lineheight*numlines, metrics.m_Height);

    float leading;
    float tracking;

    leading = 2.0f;
    tracking = 0.0f;
    dmRender::GetTextMetrics(m_SystemFontMap, "Hello World", 8*charwidth, true, leading, tracking, &metrics);
    ASSERT_EQ(ascent, metrics.m_MaxAscent);
    ASSERT_EQ(descent, metrics.m_MaxDescent);
    ASSERT_EQ(charwidth*5, metrics.m_Width);
    ASSERT_EQ(ExpectedHeight(lineheight, numlines, leading), metrics.m_Height);

    leading = 0.0f;
    tracking = 0.0f;
    dmRender::GetTextMetrics(m_SystemFontMap, "Hello World", 8*charwidth, true, leading, tracking, &metrics);
    ASSERT_EQ(ascent, metrics.m_MaxAscent);
    ASSERT_EQ(descent, metrics.m_MaxDescent);
    ASSERT_EQ(charwidth*5, metrics.m_Width);
    ASSERT_EQ(ExpectedHeight(lineheight, numlines, leading), metrics.m_Height);

    leading = 1.0f;
    tracking = 0.0f;
    numlines = 3;
    dmRender::GetTextMetrics(m_SystemFontMap, "Hello World Bonanza", 8*charwidth, true, leading, tracking, &metrics);
    ASSERT_EQ(ascent, metrics.m_MaxAscent);
    ASSERT_EQ(descent, metrics.m_MaxDescent);
    ASSERT_EQ(charwidth*7, metrics.m_Width);
    ASSERT_EQ(ExpectedHeight(lineheight, numlines, leading), metrics.m_Height);
}

TEST_F(dmRenderTest, TextAlignment)
{
    dmRender::TextMetrics metrics;

    const int charwidth     = 2;
    const int ascent        = 2;
    const int descent       = 1;
    const int lineheight    = ascent + descent;

    float tracking;
    int numlines;

    float leadings[] = { 1.0f, 2.0f, 0.5f };
    for( int i = 0; i < sizeof(leadings)/sizeof(leadings[0]); ++i )
    {
        float leading = leadings[i];
        tracking = 0.0f;
        numlines = 3;
        dmRender::GetTextMetrics(m_SystemFontMap, "Hello World Bonanza", 8*charwidth, true, leading, tracking, &metrics);
        ASSERT_EQ(ascent, metrics.m_MaxAscent);
        ASSERT_EQ(descent, metrics.m_MaxDescent);
        ASSERT_EQ(charwidth*7, metrics.m_Width);
        ASSERT_EQ(ExpectedHeight(lineheight, numlines, leading), metrics.m_Height);


        float offset;
        offset = dmRender::OffsetX(dmRender::TEXT_ALIGN_LEFT, metrics.m_Width);
        ASSERT_EQ( 0.0f, offset );
        offset = dmRender::OffsetX(dmRender::TEXT_ALIGN_CENTER, metrics.m_Width);
        ASSERT_EQ( metrics.m_Width * 0.5f, offset );
        offset = dmRender::OffsetX(dmRender::TEXT_ALIGN_RIGHT, metrics.m_Width);
        ASSERT_EQ( metrics.m_Width, offset );

        offset = OffsetY(dmRender::TEXT_VALIGN_TOP, metrics.m_Height, ascent, descent, leading, numlines);
        ASSERT_EQ( metrics.m_Height - ascent, offset );

        offset = OffsetY(dmRender::TEXT_VALIGN_MIDDLE, metrics.m_Height, ascent, descent, leading, numlines);
        ASSERT_EQ( metrics.m_Height * 0.5f + ExpectedHeight(lineheight, numlines, leading) * 0.5f - ascent, offset );

        offset = OffsetY(dmRender::TEXT_VALIGN_BOTTOM, metrics.m_Height, ascent, descent, leading, numlines);
        ASSERT_EQ( lineheight * leading * (numlines - 1) + descent, offset );
    }
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
