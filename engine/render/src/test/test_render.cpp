// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <stdint.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dmsdk/dlib/intersection.h>

#include <testmain/testmain.h>
#include <dlib/hash.h>
#include <dlib/math.h>
#include <dlib/dstrings.h>
#include <script/script.h>
#include <font/font.h>
#include <font/fontcollection.h>
#include <font/text_layout.h>
#include <platform/window.hpp>

#include <algorithm> // std::stable_sort

#include "../../../graphics/src/graphics_private.h"
#include "../../../graphics/src/null/graphics_null_private.h"
#include "../../../graphics/src/test/test_graphics_util.h"

#include "render/render.h"
#include "render/render_private.h"
#include "render/font/fontmap.h"
#include "render/font/font_glyphbank.h"
#include "render/font/font_renderer_api.h"
#include "render/font/font_renderer_private.h"

#include "render/font_ddf.h"

const static uint32_t WIDTH = 600;
const static uint32_t HEIGHT = 400;
const static uint32_t COLOR_WHITE_RGBA = 0xffffffffu;
const static uint32_t COLOR_TRANSPARENT_RGBA = 0x00000000u;

#define EPSILON 0.0001f
#define ASSERT_VEC4(exp, act)\
    ASSERT_NEAR(exp.getX(), act.getX(), EPSILON);\
    ASSERT_NEAR(exp.getY(), act.getY(), EPSILON);\
    ASSERT_NEAR(exp.getZ(), act.getZ(), EPSILON);\
    ASSERT_NEAR(exp.getW(), act.getW(), EPSILON);

using namespace dmVMath;

static dmRenderDDF::GlyphBank* CreateGlyphBank(uint32_t max_ascent, uint32_t max_descent, uint32_t glyph_count)
{
    dmRenderDDF::GlyphBank* bank = new dmRenderDDF::GlyphBank;
    memset(bank, 0, sizeof(*bank));

    bank->m_Glyphs.m_Count = glyph_count;
    bank->m_Glyphs.m_Data = new dmRenderDDF::GlyphBank::Glyph[glyph_count];

    memset(bank->m_Glyphs.m_Data, 0, sizeof(dmRenderDDF::GlyphBank::Glyph) * glyph_count);
    for (uint32_t i = 0; i < glyph_count; ++i)
    {
        bank->m_Glyphs[i].m_Character = i;
        bank->m_Glyphs[i].m_Width = 1;
        bank->m_Glyphs[i].m_LeftBearing = 1;
        bank->m_Glyphs[i].m_Advance = 2;
        bank->m_Glyphs[i].m_Ascent = 2;
        bank->m_Glyphs[i].m_Descent = 1;
    }

    bank->m_MaxAscent = max_ascent;
    bank->m_MaxDescent = max_descent;

    return bank;
}

static void DestroyGlyphBank(dmRenderDDF::GlyphBank* bank)
{
    delete[] bank->m_Glyphs.m_Data;
    delete bank;
}

static HTextLayout CreateTextLayout(dmRender::HFontMap font_map, const char* text, const TextLayoutSettings& settings)
{
    dmArray<uint32_t> codepoints;
    TextToCodePoints(text, codepoints);

    TextLayoutSettings layout_settings = settings;
    HTextLayout layout = 0;
    TextResult r = TextLayoutCreate(dmRender::GetFontCollection(font_map), codepoints.Begin(), codepoints.Size(), &layout_settings, &layout);
    EXPECT_EQ(TEXT_RESULT_OK, r);
    EXPECT_NE((HTextLayout)0, layout);
    return layout;
}

static uint32_t QueueTextAndCopyVertices(dmRender::HRenderContext render_context, dmRender::HFontMap font_map, const dmRender::DrawTextParams& params, dmArray<uint8_t>& out_vertices, HTextLayout* out_layout, float* out_radius_sq)
{
    dmRender::RenderListBegin(render_context);
    dmRender::DrawText(render_context, font_map, 0, 0, params);

    dmRender::TextContext& text_context = render_context->m_TextContext;
    if (out_layout)
    {
        *out_layout = text_context.m_TextEntries.Size() > 0 ? text_context.m_TextEntries[0].m_TextLayout : 0;
    }
    if (out_radius_sq)
    {
        *out_radius_sq = text_context.m_TextEntries.Size() > 0 ? text_context.m_TextEntries[0].m_FrustumCullingRadiusSq : 0.0f;
    }

    dmRender::FlushTexts(render_context, dmRender::RENDER_ORDER_AFTER_WORLD, true);
    dmRender::RenderListEnd(render_context);
    dmRender::DrawRenderList(render_context, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);

    uint32_t vertex_count = text_context.m_VertexIndex;
    uint32_t vertex_stride = dmRender::GetFontVertexSize(text_context.m_FontRenderBackend);
    uint32_t byte_count = vertex_count * vertex_stride;

    out_vertices.SetCapacity(byte_count);
    out_vertices.SetSize(byte_count);
    if (byte_count > 0)
    {
        memcpy(out_vertices.Begin(), text_context.m_ClientBuffer, byte_count);
    }

    return vertex_count;
}

struct TestGlyphVertex
{
    float m_Position[4];
};


class dmRenderTest : public jc_test_base_class
{
protected:
    HWindow                     m_Window;
    dmRender::HRenderContext    m_Context;
    dmGraphics::HContext        m_GraphicsContext;
    dmScript::HContext          m_ScriptContext;
    dmRender::HFontMap          m_SystemFontMap;

    HFont                   m_Font;
    dmRenderDDF::GlyphBank* m_GlyphBank;

    void SetUp() override
    {
        dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_NONE);

        WindowCreateParams win_params;
        WindowCreateParamsInitialize(&win_params);
        win_params.m_Width = 20;
        win_params.m_Height = 10;
        win_params.m_ContextAlphabits = 8;

        m_Window = dmPlatform::NewWindow();
        dmPlatform::OpenWindow(m_Window, win_params);

        dmGraphics::ContextParams graphics_context_params = {};
        graphics_context_params.m_Window                  = m_Window;
        graphics_context_params.m_DefaultTextureMinFilter = dmGraphics::TEXTURE_FILTER_DEFAULT;
        graphics_context_params.m_DefaultTextureMagFilter = dmGraphics::TEXTURE_FILTER_DEFAULT;

        m_GraphicsContext = dmGraphics::NewContext(graphics_context_params);
        dmRender::RenderContextParams params;
        dmScript::ContextParams script_context_params = {};
        script_context_params.m_GraphicsContext = m_GraphicsContext;
        m_ScriptContext = dmScript::NewContext(script_context_params);
        params.m_MaxRenderTargets = 1;
        params.m_MaxInstances = 2;
        params.m_ScriptContext = m_ScriptContext;
        params.m_MaxDebugVertexCount = 256;
        params.m_MaxCharacters = 256;
        params.m_MaxBatches = 128;
        m_Context = dmRender::NewRenderContext(m_GraphicsContext, params);
        dmRender::SetLightBufferCount(m_Context, 32);

        m_GlyphBank = CreateGlyphBank(2, 1, 128);
        m_Font = CreateGlyphBankFont("test.glyph_bankc", m_GlyphBank);

        printf("%s : %d\n", __FUNCTION__, __LINE__);
        HFontCollection font_collection = FontCollectionCreate();
        FontCollectionAddFont(font_collection, m_Font);

        dmRender::FontMapParams font_map_params;
        font_map_params.m_CacheWidth = 128;
        font_map_params.m_CacheHeight = 128;
        font_map_params.m_CacheCellWidth = 8;
        font_map_params.m_CacheCellHeight = 8;
        font_map_params.m_MaxAscent = 2;
        font_map_params.m_MaxDescent = 1;
        font_map_params.m_FontCollection = font_collection;

        printf("%s : %d\n", __FUNCTION__, __LINE__);
        m_SystemFontMap = dmRender::NewFontMap(m_Context, m_GraphicsContext, font_map_params);
    }

    void TearDown() override
    {
        dmRender::DeleteRenderContext(m_Context, 0);
        dmRender::DeleteFontMap(m_SystemFontMap);

        DestroyGlyphBank(m_GlyphBank);
        FontDestroy(m_Font);

        dmGraphics::CloseWindow(m_GraphicsContext);
        dmGraphics::DeleteContext(m_GraphicsContext);
        dmScript::DeleteContext(m_ScriptContext);

        dmPlatform::CloseWindow(m_Window);
        dmPlatform::DeleteWindow(m_Window);
    }
};

TEST_F(dmRenderTest, TestFontMapTextureFiltering)
{
    dmRender::HFontMap bitmap_font_map;
    dmRender::FontMapParams bitmap_font_map_params;
    bitmap_font_map_params.m_CacheWidth = 8;
    bitmap_font_map_params.m_CacheHeight = 8;
    bitmap_font_map_params.m_CacheCellWidth = 8;
    bitmap_font_map_params.m_CacheCellHeight = 8;
    bitmap_font_map_params.m_MaxAscent = 2;
    bitmap_font_map_params.m_MaxDescent = 1;
    bitmap_font_map_params.m_ImageFormat = dmRenderDDF::TYPE_BITMAP;

    bitmap_font_map = dmRender::NewFontMap(m_Context, m_GraphicsContext, bitmap_font_map_params);
    ASSERT_TRUE(VerifyFontMapMinFilter(bitmap_font_map, dmGraphics::TEXTURE_FILTER_LINEAR));
    ASSERT_TRUE(VerifyFontMapMagFilter(bitmap_font_map, dmGraphics::TEXTURE_FILTER_LINEAR));
    dmRender::DeleteFontMap(bitmap_font_map);
}

TEST_F(dmRenderTest, TestGraphicsContext)
{
    ASSERT_NE((void*)0x0, dmRender::GetGraphicsContext(m_Context));
}

TEST_F(dmRenderTest, TestViewProj)
{
    dmVMath::Matrix4 view = dmVMath::Matrix4::rotationX(M_PI);
    view.setTranslation(dmVMath::Vector3(1.0f, 2.0f, 3.0f));
    dmVMath::Matrix4 proj = dmVMath::Matrix4::orthographic(0.0f, WIDTH, 0.0f, HEIGHT, 1.0f, -1.0f);
    dmVMath::Matrix4 viewproj = proj * view;

    dmRender::SetViewMatrix(m_Context, view);
    dmRender::SetProjectionMatrix(m_Context, proj);
    const dmVMath::Matrix4& test = dmRender::GetViewProjectionMatrix(m_Context);

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

TEST_F(dmRenderTest, TestRenderCamera)
{
    dmRender::HRenderCamera camera = dmRender::NewRenderCamera(m_Context);

    dmMessage::URL camera_url = {};
    camera_url.m_Socket   = dmHashString64("socket");
    camera_url.m_Path     = dmHashString64("my_go");
    camera_url.m_Fragment = dmHashString64("camera");

    dmRender::SetRenderCameraURL(m_Context, camera, &camera_url);

    dmRender::RenderCameraData data = {};
    data.m_Viewport               = dmVMath::Vector4(0.0f, 0.0f, WIDTH, HEIGHT);
    data.m_AspectRatio            = WIDTH / HEIGHT;
    data.m_Fov                    = M_PI / 4.0f;
    data.m_NearZ                  = 0.1f;
    data.m_FarZ                   = 1000.0f;
    data.m_OrthographicZoom       = 1.0f;
    data.m_AutoAspectRatio        = true;
    data.m_OrthographicProjection = true;

    dmRender::SetRenderCameraData(m_Context, camera, &data);

    dmRender::RenderCameraData data_other = {};
    dmRender::GetRenderCameraData(m_Context, camera, &data_other);

    ASSERT_VEC4(data.m_Viewport, data_other.m_Viewport);
    ASSERT_NEAR(data.m_AspectRatio, data_other.m_AspectRatio, EPSILON);
    ASSERT_NEAR(data.m_Fov, data_other.m_Fov, EPSILON);
    ASSERT_NEAR(data.m_NearZ, data_other.m_NearZ, EPSILON);
    ASSERT_NEAR(data.m_FarZ, data_other.m_FarZ, EPSILON);
    ASSERT_NEAR(data.m_OrthographicZoom, data_other.m_OrthographicZoom, EPSILON);
    ASSERT_EQ(data.m_AutoAspectRatio, data_other.m_AutoAspectRatio);
    ASSERT_EQ(data.m_OrthographicProjection, data_other.m_OrthographicProjection);

    dmRender::DeleteRenderCamera(m_Context, camera);
}

TEST_F(dmRenderTest, TestRenderCameraEffectiveAspectRatio)
{
    dmRender::HRenderCamera camera = dmRender::NewRenderCamera(m_Context);
    dmRender::RenderCameraData data = {};

    // Test 1: Auto aspect ratio disabled - should return stored aspect ratio
    data.m_AspectRatio        = 19.75f;  // Set specific aspect ratio
    data.m_AutoAspectRatio    = false; // Disable auto mode
    data.m_Fov                = M_PI / 4.0f;
    data.m_NearZ              = 0.1f;
    data.m_FarZ               = 1000.0f;
    data.m_OrthographicZoom   = 1.0f;
    data.m_OrthographicProjection = false;

    dmRender::SetRenderCameraData(m_Context, camera, &data);

    float effective_aspect_ratio = dmRender::GetRenderCameraEffectiveAspectRatio(m_Context, camera);
    ASSERT_NEAR(19.75f, effective_aspect_ratio, EPSILON);

    // Test 2: Auto aspect ratio enabled - should calculate from window dimensions
    data.m_AutoAspectRatio = true; // Enable auto mode
    dmRender::SetRenderCameraData(m_Context, camera, &data);

    effective_aspect_ratio = dmRender::GetRenderCameraEffectiveAspectRatio(m_Context, camera);

    // Window dimensions from SetUp(): width=20, height=10, so expected ratio = 20/10 = 2.0
    float expected_auto_ratio = 20.0f / 10.0f;
    ASSERT_NEAR(expected_auto_ratio, effective_aspect_ratio, EPSILON);

    // Test 3: Change stored aspect ratio with auto mode - should still use calculated ratio
    data.m_AspectRatio = 5.0f; // Different stored value
    dmRender::SetRenderCameraData(m_Context, camera, &data);

    effective_aspect_ratio = dmRender::GetRenderCameraEffectiveAspectRatio(m_Context, camera);
    ASSERT_NEAR(expected_auto_ratio, effective_aspect_ratio, EPSILON); // Should still be 2.0, not 5.0

    // Test 4: Switch back to manual mode - should use stored aspect ratio again
    data.m_AutoAspectRatio = false;
    dmRender::SetRenderCameraData(m_Context, camera, &data);

    effective_aspect_ratio = dmRender::GetRenderCameraEffectiveAspectRatio(m_Context, camera);
    ASSERT_NEAR(5.0f, effective_aspect_ratio, EPSILON); // Should use stored value now

    dmRender::DeleteRenderCamera(m_Context, camera);
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

    dmVMath::Matrix4 view = dmVMath::Matrix4::identity();
    dmVMath::Matrix4 proj = dmVMath::Matrix4::orthographic(0.0f, WIDTH, 0.0f, HEIGHT, 0.1f, 1.0f);
    dmRender::SetViewMatrix(m_Context, view);
    dmRender::SetProjectionMatrix(m_Context, proj);

    dmRender::RenderListBegin(m_Context);

    uint8_t dispatch = dmRender::RenderListMakeDispatch(m_Context, TestDrawDispatch, 0, &ctx);

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
        entry.m_MinorOrder = 0;
        entry.m_TagListKey = 0;
        entry.m_Order = orders[i];
        entry.m_BatchKey = i & 3; // no particular system
        entry.m_Dispatch = dispatch;
        entry.m_UserData = 0;
    }

    dmRender::RenderListSubmit(m_Context, out, out + n);
    dmRender::RenderListEnd(m_Context);

    dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);

    ASSERT_EQ(ctx.m_BeginCalls, 1);
    ASSERT_GT(ctx.m_BatchCalls, 1);
    ASSERT_EQ(ctx.m_EntriesRendered, n);
    ASSERT_EQ(ctx.m_EndCalls, 1);
    ASSERT_EQ(ctx.m_Order, orders[2]); // highest after world entry
    ASSERT_EQ(ctx.m_Z, orders[1]);
}

struct TestDrawStateDispatchCtx
{
    dmRender::HRenderContext       m_Context;
    dmRender::HMaterial            m_Material;
    dmRender::RenderObject         m_RenderObjects[2];
    dmGraphics::HVertexDeclaration m_VertexDeclaration;
    dmGraphics::HVertexBuffer      m_VertexBuffer;
};

static void TestDrawStateDispatch(dmRender::RenderListDispatchParams const & params)
{
    if (params.m_Operation == dmRender::RENDER_LIST_OPERATION_BATCH)
    {
        TestDrawStateDispatchCtx* user_ctx = (TestDrawStateDispatchCtx*) params.m_UserData;
        dmRender::RenderObject* ro_0       = &user_ctx->m_RenderObjects[0];

        ro_0->Init();
        ro_0->m_Material = user_ctx->m_Material;
        ro_0->m_VertexCount = 1;
        ro_0->m_VertexDeclaration = user_ctx->m_VertexDeclaration;
        ro_0->m_VertexBuffer      = user_ctx->m_VertexBuffer;

        // Override blending factors
        ro_0->m_SetBlendFactors        = true;
        ro_0->m_SourceBlendFactor      = dmGraphics::BLEND_FACTOR_ONE;
        ro_0->m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;

        // Override face winding
        ro_0->m_SetFaceWinding = true;
        ro_0->m_FaceWinding    = dmGraphics::FACE_WINDING_CW;

        ro_0->m_SetStencilTest                      = true;
        ro_0->m_StencilTestParams.m_Front.m_Func    = dmGraphics::COMPARE_FUNC_EQUAL;
        ro_0->m_StencilTestParams.m_Ref             = 16; // expected: 0xff after render
        ro_0->m_StencilTestParams.m_RefMask         = 22; // expected: 0x0f after render
        ro_0->m_StencilTestParams.m_ColorBufferMask = dmGraphics::DM_GRAPHICS_STATE_WRITE_R | dmGraphics::DM_GRAPHICS_STATE_WRITE_A;
        ro_0->m_StencilTestParams.m_BufferMask      = 1;

        dmRender::RenderObject* ro_1 = &user_ctx->m_RenderObjects[1];
        ro_1->Init();
        ro_1->m_Material          = user_ctx->m_Material;
        ro_1->m_VertexCount       = 1;
        ro_1->m_VertexDeclaration = user_ctx->m_VertexDeclaration;
        ro_1->m_VertexBuffer      = user_ctx->m_VertexBuffer;

        ro_1->m_SetStencilTest                         = true;
        ro_1->m_StencilTestParams.m_SeparateFaceStates = 1;

        // Set some non-specific state values
        ro_1->m_StencilTestParams.m_Front.m_Func     = dmGraphics::COMPARE_FUNC_NOTEQUAL;
        ro_1->m_StencilTestParams.m_Front.m_OpSFail  = dmGraphics::STENCIL_OP_INCR_WRAP;
        ro_1->m_StencilTestParams.m_Front.m_OpDPFail = dmGraphics::STENCIL_OP_DECR;
        ro_1->m_StencilTestParams.m_Front.m_OpDPPass = dmGraphics::STENCIL_OP_DECR_WRAP;

        ro_1->m_StencilTestParams.m_Back.m_Func     = dmGraphics::COMPARE_FUNC_GEQUAL;
        ro_1->m_StencilTestParams.m_Back.m_OpSFail  = dmGraphics::STENCIL_OP_REPLACE;
        ro_1->m_StencilTestParams.m_Back.m_OpDPFail = dmGraphics::STENCIL_OP_INVERT;
        ro_1->m_StencilTestParams.m_Back.m_OpDPPass = dmGraphics::STENCIL_OP_INCR_WRAP;

        ro_1->m_StencilTestParams.m_Ref     = 127; // expected: 0xff after render
        ro_1->m_StencilTestParams.m_RefMask = 11; // expected: 0x0f after render

        AddToRender(user_ctx->m_Context, ro_0);
        AddToRender(user_ctx->m_Context, ro_1);
    }
}

TEST_F(dmRenderTest, TestRenderListDrawState)
{
    dmRender::RenderListBegin(m_Context);

    dmGraphics::ShaderDescBuilder shader_desc_builder;
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, "foo", 3);
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, "foo", 3);

    dmGraphics::HProgram program = dmGraphics::NewProgram(m_GraphicsContext, shader_desc_builder.Get(), 0, 0);
    dmRender::HMaterial material = dmRender::NewMaterial(m_Context, program);
    dmhash_t tag = dmHashString64("tag");
    dmRender::SetMaterialTags(material, 1, &tag);

    dmGraphics::HVertexDeclaration vx_decl = dmGraphics::NewVertexDeclaration(m_GraphicsContext, 0, 0);
    dmGraphics::HVertexBuffer vx_buffer = dmGraphics::NewVertexBuffer(m_GraphicsContext, 0, 0, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

    TestDrawStateDispatchCtx user_ctx;
    user_ctx.m_Context           = m_Context;
    user_ctx.m_Material          = material;
    user_ctx.m_VertexDeclaration = vx_decl;
    user_ctx.m_VertexBuffer      = vx_buffer;

    uint8_t dispatch = dmRender::RenderListMakeDispatch(m_Context, TestDrawStateDispatch, 0, &user_ctx);

    dmRender::RenderListEntry* out = dmRender::RenderListAlloc(m_Context, 1);

    dmRender::RenderListEntry & entry = out[0];
    entry.m_WorldPosition             = Point3(0,0,0);
    entry.m_MajorOrder                = 0;
    entry.m_MinorOrder                = 0;
    entry.m_TagListKey                = 0;
    entry.m_Order                     = 1;
    entry.m_BatchKey                  = 0;
    entry.m_Dispatch                  = dispatch;
    entry.m_UserData                  = 0;

    dmGraphics::EnableState(m_GraphicsContext, dmGraphics::STATE_BLEND);
    dmGraphics::EnableState(m_GraphicsContext, dmGraphics::STATE_STENCIL_TEST);

    dmGraphics::SetBlendFunc(m_GraphicsContext, dmGraphics::BLEND_FACTOR_ZERO, dmGraphics::BLEND_FACTOR_ZERO);
    dmGraphics::SetStencilFunc(m_GraphicsContext, dmGraphics::COMPARE_FUNC_NEVER, 0xff, 0xf);
    dmGraphics::SetFaceWinding(m_GraphicsContext, dmGraphics::FACE_WINDING_CCW);
    dmGraphics::SetStencilOp(m_GraphicsContext, dmGraphics::STENCIL_OP_ZERO, dmGraphics::STENCIL_OP_ZERO, dmGraphics::STENCIL_OP_ZERO);

    dmGraphics::SetColorMask(m_GraphicsContext, 0, 0, 0, 0);
    dmGraphics::SetStencilMask(m_GraphicsContext, 0);

    dmGraphics::PipelineState ps_before = dmGraphics::GetPipelineState(m_GraphicsContext);

    dmRender::RenderListSubmit(m_Context, out, out + 1);
    dmRender::RenderListEnd(m_Context);
    dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);

    dmGraphics::PipelineState ps_after = dmGraphics::GetPipelineState(m_GraphicsContext);

    ASSERT_EQ(0, memcmp(&ps_before, &ps_after, sizeof(dmGraphics::PipelineState)));

    dmRender::DeleteMaterial(m_Context, material);
    dmGraphics::DeleteProgram(m_GraphicsContext, program);

    dmGraphics::DeleteVertexBuffer(vx_buffer);
    printf("DeleteVertexDeclaration; %p\n", vx_decl);

    dmGraphics::DeleteVertexDeclaration(vx_decl);
}

struct TestEnableTextureByHashDispatchCtx
{
    dmRender::HRenderContext        m_Context;
    dmRender::RenderObject          m_RenderObject;
    dmGraphics::HTexture*           m_Textures;

    dmRender::HMaterial             m_Material;
    dmGraphics::HVertexDeclaration  m_VertexDeclaration;
    dmGraphics::HVertexBuffer       m_VertexBuffer;
};

static void TestEnableTextureByHashDispatch(dmRender::RenderListDispatchParams const & params)
{
    if (params.m_Operation == dmRender::RENDER_LIST_OPERATION_BATCH)
    {
        TestEnableTextureByHashDispatchCtx* user_ctx = (TestEnableTextureByHashDispatchCtx*) params.m_UserData;
        dmRender::RenderObject* ro_0                 = &user_ctx->m_RenderObject;

        ro_0->m_Material          = user_ctx->m_Material;
        ro_0->m_VertexCount       = 1;
        ro_0->m_VertexDeclaration = user_ctx->m_VertexDeclaration;
        ro_0->m_VertexBuffer      = user_ctx->m_VertexBuffer;
        memcpy(ro_0->m_Textures, user_ctx->m_Textures, sizeof(ro_0->m_Textures));

        AddToRender(user_ctx->m_Context, ro_0);
    }
}

static dmGraphics::HTexture MakeDummyTexture(dmGraphics::HContext context, uint32_t depth = 1)
{
    dmGraphics::TextureCreationParams creation_params;
    creation_params.m_Type           = depth > 1 ? dmGraphics::TEXTURE_TYPE_2D_ARRAY : dmGraphics::TEXTURE_TYPE_2D;
    creation_params.m_Width          = 2;
    creation_params.m_Height         = 2;
    creation_params.m_OriginalWidth  = 2;
    creation_params.m_OriginalHeight = 2;
    creation_params.m_Depth          = depth;

    uint8_t tex_data[2 * 2];
    dmGraphics::TextureParams params;
    params.m_DataSize  = sizeof(tex_data);
    params.m_Data      = tex_data;
    params.m_Width     = creation_params.m_Width;
    params.m_Height    = creation_params.m_Height;
    params.m_Format    = dmGraphics::TEXTURE_FORMAT_LUMINANCE;
    params.m_MinFilter = dmGraphics::TEXTURE_FILTER_DEFAULT;
    params.m_MagFilter = dmGraphics::TEXTURE_FILTER_DEFAULT;

    dmGraphics::HTexture texture = dmGraphics::NewTexture(context, creation_params);
    dmGraphics::SetTexture(context, texture, params);
    return texture;
}

static inline int CountSamplersInTextureBindTable(dmRender::HRenderContext context, dmhash_t sampler_name)
{
    int c = 0;
    for (int i = 0; i < context->m_TextureBindTable.Size(); ++i)
    {
        if (context->m_TextureBindTable[i].m_Samplerhash == sampler_name)
        {
            c++;
        }
    }
    return c;
}

TEST_F(dmRenderTest, TestEnableTextureByHash)
{
    const char* shader_src = "uniform lowp sampler2D texture_sampler_1;\n"
                             "uniform lowp sampler2D texture_sampler_2;\n"
                             "uniform lowp sampler2DArray texture_sampler_3;\n"
                             "uniform lowp sampler2D texture_sampler_4;\n";

    dmGraphics::ShaderDescBuilder shader_desc_builder;
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, "foo", 3);
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, shader_src, strlen(shader_src));

    shader_desc_builder.AddTexture("texture_sampler_1", 0, dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D);
    shader_desc_builder.AddTexture("texture_sampler_2", 1, dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D);
    shader_desc_builder.AddTexture("texture_sampler_3", 2, dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D_ARRAY);
    shader_desc_builder.AddTexture("texture_sampler_4", 3, dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D);

    dmGraphics::HProgram program = dmGraphics::NewProgram(m_GraphicsContext, shader_desc_builder.Get(), 0, 0);

    dmRender::HMaterial material    = dmRender::NewMaterial(m_Context, program);
    dmhash_t texture_sampler_1_hash = dmHashString64("texture_sampler_1");
    dmhash_t texture_sampler_2_hash = dmHashString64("texture_sampler_2");
    dmhash_t texture_sampler_3_hash = dmHashString64("texture_sampler_3");
    dmhash_t texture_sampler_4_hash = dmHashString64("texture_sampler_4");

    SetMaterialSampler(material, texture_sampler_1_hash, 0, dmGraphics::TEXTURE_WRAP_REPEAT, dmGraphics::TEXTURE_WRAP_REPEAT, dmGraphics::TEXTURE_FILTER_LINEAR, dmGraphics::TEXTURE_FILTER_LINEAR, 1.0f);
    SetMaterialSampler(material, texture_sampler_2_hash, 1, dmGraphics::TEXTURE_WRAP_REPEAT, dmGraphics::TEXTURE_WRAP_REPEAT, dmGraphics::TEXTURE_FILTER_LINEAR, dmGraphics::TEXTURE_FILTER_LINEAR, 1.0f);
    SetMaterialSampler(material, texture_sampler_3_hash, 2, dmGraphics::TEXTURE_WRAP_REPEAT, dmGraphics::TEXTURE_WRAP_REPEAT, dmGraphics::TEXTURE_FILTER_LINEAR, dmGraphics::TEXTURE_FILTER_LINEAR, 1.0f);
    SetMaterialSampler(material, texture_sampler_4_hash, 3, dmGraphics::TEXTURE_WRAP_REPEAT, dmGraphics::TEXTURE_WRAP_REPEAT, dmGraphics::TEXTURE_FILTER_LINEAR, dmGraphics::TEXTURE_FILTER_LINEAR, 1.0f);

    dmhash_t tag = dmHashString64("tag");
    dmRender::SetMaterialTags(material, 1, &tag);

    dmGraphics::HVertexDeclaration vx_decl = dmGraphics::NewVertexDeclaration(m_GraphicsContext, 0, 0);
    dmGraphics::HVertexBuffer vx_buffer = dmGraphics::NewVertexBuffer(m_GraphicsContext, 0, 0, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

    dmGraphics::HTexture textures[dmRender::RenderObject::MAX_TEXTURE_COUNT] = {};

    for (int i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
    {
        textures[i] = MakeDummyTexture(m_GraphicsContext);
    }

    TestEnableTextureByHashDispatchCtx user_ctx;
    user_ctx.m_Context           = m_Context;
    user_ctx.m_Material          = material;
    user_ctx.m_VertexDeclaration = vx_decl;
    user_ctx.m_VertexBuffer      = vx_buffer;
    user_ctx.m_Textures          = textures;

    dmRender::RenderListBegin(m_Context);

    uint8_t dispatch = dmRender::RenderListMakeDispatch(m_Context, TestEnableTextureByHashDispatch, 0, &user_ctx);
    dmRender::RenderListEntry* out = dmRender::RenderListAlloc(m_Context, 1);

    dmRender::RenderListEntry & entry = out[0];
    entry.m_WorldPosition             = Point3(0,0,0);
    entry.m_MajorOrder                = 0;
    entry.m_MinorOrder                = 0;
    entry.m_TagListKey                = 0;
    entry.m_Order                     = 1;
    entry.m_BatchKey                  = 0;
    entry.m_Dispatch                  = dispatch;
    entry.m_UserData                  = 0;

    dmGraphics::NullContext* null_context = (dmGraphics::NullContext*) m_GraphicsContext;
    // Turn off all context features (mostly for testing array textutes here)
    null_context->m_ContextFeatures = 0;

    dmGraphics::HTexture test_texture_0     = MakeDummyTexture(m_GraphicsContext);
    dmGraphics::HTexture test_texture_1     = MakeDummyTexture(m_GraphicsContext);
    dmGraphics::HTexture test_texture_array = MakeDummyTexture(m_GraphicsContext, 4);

    SetTextureBindingByUnit(m_Context, 0, test_texture_0);
    SetTextureBindingByHash(m_Context, texture_sampler_1_hash, test_texture_1);

    dmRender::RenderListSubmit(m_Context, out, out + 1);
    dmRender::RenderListEnd(m_Context);

    dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);

    // we bind test_texture_0 to unit 0, but that binding will be overwritten by the name hash binding
    // since the "texture_sampler_1" sampler is bound to unit 0
    dmGraphics::NullTexture* tex0_ptr = dmGraphics::GetAssetFromContainer<dmGraphics::NullTexture>(null_context->m_BaseContext.m_AssetHandleContainer, test_texture_0);
    ASSERT_EQ(m_Context->m_TextureBindTable[0].m_Texture, test_texture_0);
    ASSERT_EQ(-1, tex0_ptr->m_LastBoundUnit[0]);

    // test_texture_1 is bound by hash to unit 0 ("texture_sampler_1")
    dmGraphics::NullTexture* tex1_ptr = dmGraphics::GetAssetFromContainer<dmGraphics::NullTexture>(null_context->m_BaseContext.m_AssetHandleContainer, test_texture_1);
    ASSERT_EQ(m_Context->m_TextureBindTable[1].m_Texture, test_texture_1);
    ASSERT_EQ(0, tex1_ptr->m_LastBoundUnit[0]);

    // we should allow binding the same texture to multiple logical units
    SetTextureBindingByHash(m_Context, texture_sampler_2_hash, test_texture_1);
    dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);

    ASSERT_EQ(m_Context->m_TextureBindTable[1].m_Texture, test_texture_1);
    ASSERT_EQ(m_Context->m_TextureBindTable[2].m_Texture, test_texture_1);

    // The last bound texture should be unit 1, since we bind it to the second sampler (and not the first)
    ASSERT_EQ(1, tex1_ptr->m_LastBoundUnit[0]);

    // Bind an array texture with 4 sub-ids
    SetTextureBindingByHash(m_Context, texture_sampler_3_hash, test_texture_array);

    // Bind another texture after, to make sure we can bind something after an array and all the unit offsets should be valid
    SetTextureBindingByHash(m_Context, texture_sampler_4_hash, test_texture_0);

    dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);

    dmGraphics::NullTexture* tex_array = dmGraphics::GetAssetFromContainer<dmGraphics::NullTexture>(null_context->m_BaseContext.m_AssetHandleContainer, test_texture_array);
    ASSERT_EQ(m_Context->m_TextureBindTable[3].m_Texture, test_texture_array);
    ASSERT_EQ(2, tex_array->m_LastBoundUnit[0]);
    ASSERT_EQ(3, tex_array->m_LastBoundUnit[1]);
    ASSERT_EQ(4, tex_array->m_LastBoundUnit[2]);
    ASSERT_EQ(5, tex_array->m_LastBoundUnit[3]);

    ASSERT_EQ(m_Context->m_TextureBindTable[0].m_Texture, test_texture_0);
    ASSERT_EQ(m_Context->m_TextureBindTable[4].m_Texture, test_texture_0);
    ASSERT_EQ(6, tex0_ptr->m_LastBoundUnit[0]);

    // Unbind everything
    for (int i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
    {
        SetTextureBindingByUnit(m_Context, i, 0);
    }

    // Drawing should trim the texture bind table based on where the last valid texture was found
    // which will set the table to zero in this case
    dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);
    ASSERT_EQ(0, m_Context->m_TextureBindTable.Size());

    // table is [t0, 0, 0, t0];
    SetTextureBindingByUnit(m_Context, 0, test_texture_0);
    SetTextureBindingByUnit(m_Context, 3, test_texture_0);
    ASSERT_EQ(4, m_Context->m_TextureBindTable.Size());
    ASSERT_EQ(test_texture_0, m_Context->m_TextureBindTable[0].m_Texture);
    ASSERT_EQ(test_texture_0, m_Context->m_TextureBindTable[3].m_Texture);
    // Unbind [t0, 0, 0, 0]
    SetTextureBindingByUnit(m_Context, 3, 0);

    // Draw should trim the array to [t0]
    dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);
    ASSERT_EQ(1, m_Context->m_TextureBindTable.Size());
    ASSERT_EQ(test_texture_0, m_Context->m_TextureBindTable[0].m_Texture);

    // Reset bind table
    m_Context->m_TextureBindTable.SetSize(0);

    // Test binding and unbinding in different order, each sampler can only have one entry in the list
    SetTextureBindingByHash(m_Context, texture_sampler_1_hash, textures[0]);
    SetTextureBindingByHash(m_Context, texture_sampler_2_hash, textures[0]);

    ASSERT_EQ(1, CountSamplersInTextureBindTable(m_Context, texture_sampler_1_hash));
    ASSERT_EQ(1, CountSamplersInTextureBindTable(m_Context, texture_sampler_2_hash));

    // Unbinding the first sampler will clear up one slot
    SetTextureBindingByHash(m_Context, texture_sampler_1_hash, 0);
    SetTextureBindingByHash(m_Context, texture_sampler_2_hash, textures[1]);

    // The second sampler should just be present once in the table
    ASSERT_EQ(0, CountSamplersInTextureBindTable(m_Context, texture_sampler_1_hash));
    ASSERT_EQ(1, CountSamplersInTextureBindTable(m_Context, texture_sampler_2_hash));

    dmGraphics::DeleteTexture(m_GraphicsContext, test_texture_0);
    dmGraphics::DeleteTexture(m_GraphicsContext, test_texture_1);
    dmGraphics::DeleteTexture(m_GraphicsContext, test_texture_array);

    for (int i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
    {
        dmGraphics::DeleteTexture(m_GraphicsContext, textures[i]);
    }

    dmGraphics::DeleteTexture(m_GraphicsContext, test_texture_0);

    dmGraphics::DeleteProgram(m_GraphicsContext, program);
    dmRender::DeleteMaterial(m_Context, material);

    dmGraphics::DeleteVertexBuffer(vx_buffer);
    dmGraphics::DeleteVertexDeclaration(vx_decl);
}

TEST_F(dmRenderTest, TestRenderObjectMaxTextureBindings)
{
    dmGraphics::ShaderDescBuilder shader_desc_builder;
    dmGraphics::HTexture textures[dmRender::RenderObject::MAX_TEXTURE_COUNT] = {};
    char tex_names[dmRender::RenderObject::MAX_TEXTURE_COUNT][16] = {}; // Ensure enough space for the longest texture name
    char fs_buf[2048];
    size_t off = 0;

    // Generate a fs-shader with MAX_TEXTURE_COUNT samplers
    for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
    {
        off += dmSnPrintf(fs_buf + off, sizeof(fs_buf) - off, "uniform lowp sampler2D tex%u;\n", i);
        dmSnPrintf(tex_names[i], sizeof(tex_names[i]), "tex%u", i);

        shader_desc_builder.AddTexture(tex_names[i], i, dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D);
        textures[i] = MakeDummyTexture(m_GraphicsContext);
    }

    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, "foo", 3);
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, fs_buf, (uint32_t) strlen(fs_buf));

    dmGraphics::HProgram program = dmGraphics::NewProgram(m_GraphicsContext, shader_desc_builder.Get(), 0, 0);
    dmRender::HMaterial material = dmRender::NewMaterial(m_Context, program);

    for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
    {
        ASSERT_TRUE(dmRender::SetMaterialSampler(material, dmHashString64(tex_names[i]), i, dmGraphics::TEXTURE_WRAP_REPEAT, dmGraphics::TEXTURE_WRAP_REPEAT, dmGraphics::TEXTURE_FILTER_LINEAR, dmGraphics::TEXTURE_FILTER_LINEAR, 1.0f));
    }

    dmhash_t tag = dmHashString64("tag");
    dmRender::SetMaterialTags(material, 1, &tag);

    dmGraphics::HVertexDeclaration vx_decl = dmGraphics::NewVertexDeclaration(m_GraphicsContext, 0, 0);
    dmGraphics::HVertexBuffer vx_buffer    = dmGraphics::NewVertexBuffer(m_GraphicsContext, 0, 0, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

    TestEnableTextureByHashDispatchCtx user_ctx;
    user_ctx.m_Context           = m_Context;
    user_ctx.m_Material          = material;
    user_ctx.m_VertexDeclaration = vx_decl;
    user_ctx.m_VertexBuffer      = vx_buffer;
    user_ctx.m_Textures          = textures;

    dmRender::RenderListBegin(m_Context);
    dmRender::RenderListEntry* out   = dmRender::RenderListAlloc(m_Context, 1);
    dmRender::RenderListEntry& entry = out[0];
    entry.m_WorldPosition            = Point3(0, 0, 0);
    entry.m_MajorOrder               = 0;
    entry.m_MinorOrder               = 0;
    entry.m_TagListKey               = 0;
    entry.m_Order                    = 1;
    entry.m_BatchKey                 = 0;
    entry.m_Dispatch                 = dmRender::RenderListMakeDispatch(m_Context, TestEnableTextureByHashDispatch, 0, &user_ctx);
    entry.m_UserData                 = 0;

    dmRender::RenderListSubmit(m_Context, out, out + 1);
    dmRender::RenderListEnd(m_Context);
    dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);

    // Note: After a render, the textures are unbound from the context.
    //       Hence we need to check the last bound unit for each texture.
    dmGraphics::NullContext* null_context = (dmGraphics::NullContext*) m_GraphicsContext;
    for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
    {
        dmGraphics::NullTexture* tex = dmGraphics::GetAssetFromContainer<dmGraphics::NullTexture>(null_context->m_BaseContext.m_AssetHandleContainer, textures[i]);
        ASSERT_EQ((int32_t) i, tex->m_LastBoundUnit[0]);
        dmGraphics::DeleteTexture(m_GraphicsContext, textures[i]);
    }

    dmGraphics::DeleteProgram(m_GraphicsContext, program);
    dmRender::DeleteMaterial(m_Context, material);
    dmGraphics::DeleteVertexBuffer(vx_buffer);
    dmGraphics::DeleteVertexDeclaration(vx_decl);
}

TEST_F(dmRenderTest, TestEnableDisableContextTextures)
{
    dmGraphics::ShaderDescBuilder shader_desc_builder;
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, "foo", 3);
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, "foo", 3);

    dmGraphics::HProgram program = dmGraphics::NewProgram(m_GraphicsContext, shader_desc_builder.Get(), 0, 0);
    dmRender::HMaterial material = dmRender::NewMaterial(m_Context, program);

    dmhash_t tag = dmHashString64("tag");
    dmRender::SetMaterialTags(material, 1, &tag);

    dmGraphics::HVertexDeclaration vx_decl = dmGraphics::NewVertexDeclaration(m_GraphicsContext, 0, 0);
    dmGraphics::HVertexBuffer vx_buffer = dmGraphics::NewVertexBuffer(m_GraphicsContext, 0, 0, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

    ASSERT_EQ(0, m_Context->m_TextureBindTable.Size());

    SetTextureBindingByUnit(m_Context, 0, 13);
    SetTextureBindingByUnit(m_Context, 7, 99);
    SetTextureBindingByUnit(m_Context, 31, 1337);

    ASSERT_EQ(32, m_Context->m_TextureBindTable.Size());
    ASSERT_EQ(13,   m_Context->m_TextureBindTable[0].m_Texture);
    ASSERT_EQ(99,   m_Context->m_TextureBindTable[7].m_Texture);
    ASSERT_EQ(1337, m_Context->m_TextureBindTable[31].m_Texture);

    // Unbind by unit
    SetTextureBindingByUnit(m_Context, 0, 0);
    SetTextureBindingByUnit(m_Context, 7, 0);
    SetTextureBindingByUnit(m_Context, 31, 0);

    ASSERT_EQ(0, m_Context->m_TextureBindTable[0].m_Texture);
    ASSERT_EQ(0, m_Context->m_TextureBindTable[7].m_Texture);
    ASSERT_EQ(0, m_Context->m_TextureBindTable[31].m_Texture);

    // Bind by unit and hashes
    dmhash_t sampler_0 = dmHashString64("sampler_0");
    dmhash_t sampler_1 = dmHashString64("sampler_1");

    SetTextureBindingByHash(m_Context, sampler_0, 13); // -> slot 0
    SetTextureBindingByUnit(m_Context, 0,         31); // -> overwrite slot 0
    ASSERT_EQ(31, m_Context->m_TextureBindTable[0].m_Texture);
    ASSERT_EQ(0,  m_Context->m_TextureBindTable[0].m_Samplerhash);

    SetTextureBindingByUnit(m_Context, 1,         64); // -> slot 1
    SetTextureBindingByHash(m_Context, sampler_0, 46); // -> slot 2 (not overwriting)

    ASSERT_EQ(64,        m_Context->m_TextureBindTable[1].m_Texture);
    ASSERT_EQ(0,         m_Context->m_TextureBindTable[1].m_Samplerhash);
    ASSERT_EQ(46,        m_Context->m_TextureBindTable[2].m_Texture);
    ASSERT_EQ(sampler_0, m_Context->m_TextureBindTable[2].m_Samplerhash);

    SetTextureBindingByUnit(m_Context, 1, 0); // reset 1
    ASSERT_EQ(0, m_Context->m_TextureBindTable[1].m_Texture);
    ASSERT_EQ(0, m_Context->m_TextureBindTable[1].m_Samplerhash);

    SetTextureBindingByHash(m_Context, sampler_1, 999); // -> reuse slot 1
    ASSERT_EQ(999,       m_Context->m_TextureBindTable[1].m_Texture);
    ASSERT_EQ(sampler_1, m_Context->m_TextureBindTable[1].m_Samplerhash);

    SetTextureBindingByHash(m_Context, sampler_1, 9000); // -> reuse hash sampler_1
    ASSERT_EQ(9000,      m_Context->m_TextureBindTable[1].m_Texture);
    ASSERT_EQ(sampler_1, m_Context->m_TextureBindTable[1].m_Samplerhash);

    SetTextureBindingByHash(m_Context, sampler_1, 0); // -> unbind sampler_1
    ASSERT_EQ(0, m_Context->m_TextureBindTable[1].m_Texture);
    ASSERT_EQ(0, m_Context->m_TextureBindTable[1].m_Samplerhash);

    dmRender::DeleteMaterial(m_Context, material);
    dmGraphics::DeleteProgram(m_GraphicsContext, program);

    dmGraphics::DeleteVertexBuffer(vx_buffer);
    dmGraphics::DeleteVertexDeclaration(vx_decl);
}

struct TestDefaultSamplerFiltersDispatchCtx
{
    dmRender::HRenderContext        m_Context;
    dmRender::RenderObject          m_RenderObjects[2];
    dmRender::HMaterial             m_Material[2];
    dmGraphics::HVertexDeclaration  m_VertexDeclaration;
    dmGraphics::HVertexBuffer       m_VertexBuffer;
    dmGraphics::HTexture            m_Texture;
    uint8_t                         m_Dispatch;
};

static void TestDefaultSamplerFiltersDispatch(dmRender::RenderListDispatchParams const & params)
{
    if (params.m_Operation == dmRender::RENDER_LIST_OPERATION_BATCH)
    {
        TestDefaultSamplerFiltersDispatchCtx* user_ctx = (TestDefaultSamplerFiltersDispatchCtx*) params.m_UserData;
        dmRender::RenderObject* ro = &user_ctx->m_RenderObjects[user_ctx->m_Dispatch];

        ro->Init();
        ro->m_Material          = user_ctx->m_Material[user_ctx->m_Dispatch];
        ro->m_VertexCount       = 1;
        ro->m_VertexDeclaration = user_ctx->m_VertexDeclaration;
        ro->m_VertexBuffer      = user_ctx->m_VertexBuffer;
        ro->m_Textures[0]       = user_ctx->m_Texture;
        ro->m_Textures[1]       = user_ctx->m_Texture;
        ro->m_Textures[2]       = user_ctx->m_Texture;

        AddToRender(user_ctx->m_Context, ro);
    }
}

TEST_F(dmRenderTest, TestDefaultSamplerFilters)
{
    dmGraphics::TextureFilter gfx_min_filter_default;
    dmGraphics::TextureFilter gfx_mag_filter_default;
    dmGraphics::GetDefaultTextureFilters(m_GraphicsContext, gfx_min_filter_default, gfx_mag_filter_default);
    ASSERT_EQ(dmGraphics::TEXTURE_FILTER_LINEAR, gfx_min_filter_default);
    ASSERT_EQ(dmGraphics::TEXTURE_FILTER_LINEAR, gfx_mag_filter_default);

    const char* shader_src = "uniform lowp sampler2D texture_sampler_1;\n"
                             "uniform lowp sampler2D texture_sampler_2;\n"
                             "uniform lowp sampler2D texture_sampler_3;\n";

    dmGraphics::ShaderDescBuilder shader_desc_builder;
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, shader_src, strlen(shader_src));
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, shader_src, strlen(shader_src));

    shader_desc_builder.AddTexture("texture_sampler_1", 0, dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D);
    shader_desc_builder.AddTexture("texture_sampler_2", 1, dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D);
    shader_desc_builder.AddTexture("texture_sampler_3", 2, dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D);

    dmGraphics::HProgram program             = dmGraphics::NewProgram(m_GraphicsContext, shader_desc_builder.Get(), 0, 0);
    dmRender::HMaterial material             = dmRender::NewMaterial(m_Context, program);
    dmRender::HMaterial material_no_samplers = dmRender::NewMaterial(m_Context, program);

    dmhash_t tag = dmHashString64("tag");
    dmRender::SetMaterialTags(material, 1, &tag);

    dmGraphics::TextureCreationParams creation_params;
    creation_params.m_Width          = 2;
    creation_params.m_Height         = 2;
    creation_params.m_OriginalWidth  = 2;
    creation_params.m_OriginalHeight = 2;
    dmGraphics::HTexture texture     = dmGraphics::NewTexture(m_GraphicsContext, creation_params);

    uint8_t tex_data[2 * 2];
    dmGraphics::TextureParams params;
    params.m_DataSize  = sizeof(tex_data);
    params.m_Data      = tex_data;
    params.m_Width     = creation_params.m_Width;
    params.m_Height    = creation_params.m_Height;
    params.m_Format    = dmGraphics::TEXTURE_FORMAT_LUMINANCE;
    params.m_MinFilter = dmGraphics::TEXTURE_FILTER_DEFAULT;
    params.m_MagFilter = dmGraphics::TEXTURE_FILTER_DEFAULT;
    dmGraphics::SetTexture(m_GraphicsContext, texture, params);

    ASSERT_TRUE(dmRender::SetMaterialSampler(material,
        dmHashString64("texture_sampler_1"), 0,
        dmGraphics::TEXTURE_WRAP_REPEAT,
        dmGraphics::TEXTURE_WRAP_REPEAT,
        dmGraphics::TEXTURE_FILTER_NEAREST,
        dmGraphics::TEXTURE_FILTER_NEAREST,
        1.0f));

    ASSERT_TRUE(dmRender::SetMaterialSampler(material,
        dmHashString64("texture_sampler_2"), 1,
        dmGraphics::TEXTURE_WRAP_REPEAT,
        dmGraphics::TEXTURE_WRAP_REPEAT,
        dmGraphics::TEXTURE_FILTER_DEFAULT,
        dmGraphics::TEXTURE_FILTER_NEAREST,
        1.0f));

    ASSERT_TRUE(dmRender::SetMaterialSampler(material,
        dmHashString64("texture_sampler_3"), 2,
        dmGraphics::TEXTURE_WRAP_REPEAT,
        dmGraphics::TEXTURE_WRAP_REPEAT,
        dmGraphics::TEXTURE_FILTER_DEFAULT,
        dmGraphics::TEXTURE_FILTER_DEFAULT,
        1.0f));

    dmGraphics::HVertexDeclaration vx_decl = dmGraphics::NewVertexDeclaration(m_GraphicsContext, 0, 0);
    dmGraphics::HVertexBuffer vx_buffer = dmGraphics::NewVertexBuffer(m_GraphicsContext, 0, 0, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

    TestDefaultSamplerFiltersDispatchCtx user_ctx;
    user_ctx.m_Context           = m_Context;
    user_ctx.m_Dispatch          = 0;
    user_ctx.m_Material[0]       = material;
    user_ctx.m_Material[1]       = material_no_samplers;
    user_ctx.m_Texture           = texture;
    user_ctx.m_VertexDeclaration = vx_decl;
    user_ctx.m_VertexBuffer      = vx_buffer;

    dmRender::RenderListBegin(m_Context);
    uint8_t dispatch = dmRender::RenderListMakeDispatch(m_Context, TestDefaultSamplerFiltersDispatch, 0, &user_ctx);

    dmRender::RenderListEntry* out = dmRender::RenderListAlloc(m_Context, 1);
    dmRender::RenderListEntry& entry = out[0];
    entry.m_WorldPosition             = Point3(0,0,0);
    entry.m_MajorOrder                = 0;
    entry.m_MinorOrder                = 0;
    entry.m_TagListKey                = 0;
    entry.m_Order                     = 1;
    entry.m_BatchKey                  = 0;
    entry.m_Dispatch                  = dispatch;
    entry.m_UserData                  = 0;

    dmGraphics::TextureFilter gfx_min_filter_active;
    dmGraphics::TextureFilter gfx_mag_filter_active;

    {
        dmRender::RenderListSubmit(m_Context, out, out + 1);
        dmRender::RenderListEnd(m_Context);
        dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);

        dmGraphics::GetTextureFilters(m_GraphicsContext, 0, gfx_min_filter_active, gfx_mag_filter_active);
        ASSERT_EQ(dmGraphics::TEXTURE_FILTER_NEAREST, gfx_min_filter_active);
        ASSERT_EQ(dmGraphics::TEXTURE_FILTER_NEAREST, gfx_mag_filter_active);

        dmGraphics::GetTextureFilters(m_GraphicsContext, 1, gfx_min_filter_active, gfx_mag_filter_active);
        ASSERT_EQ(gfx_min_filter_default, gfx_min_filter_active);
        ASSERT_EQ(dmGraphics::TEXTURE_FILTER_NEAREST, gfx_mag_filter_active);

        dmGraphics::GetTextureFilters(m_GraphicsContext, 2, gfx_min_filter_active, gfx_mag_filter_active);
        ASSERT_EQ(gfx_min_filter_default, gfx_min_filter_active);
        ASSERT_EQ(gfx_mag_filter_default, gfx_mag_filter_active);
    }

    user_ctx.m_Dispatch++;

    {
        dmRender::RenderListSubmit(m_Context, out, out + 1);
        dmRender::RenderListEnd(m_Context);
        dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);

        dmGraphics::GetTextureFilters(m_GraphicsContext, 0, gfx_min_filter_active, gfx_mag_filter_active);
        ASSERT_EQ(gfx_min_filter_default, gfx_min_filter_active);
        ASSERT_EQ(gfx_mag_filter_default, gfx_mag_filter_active);

        dmGraphics::GetTextureFilters(m_GraphicsContext, 1, gfx_min_filter_active, gfx_mag_filter_active);
        ASSERT_EQ(gfx_min_filter_default, gfx_min_filter_active);
        ASSERT_EQ(gfx_mag_filter_default, gfx_mag_filter_active);

        dmGraphics::GetTextureFilters(m_GraphicsContext, 2, gfx_min_filter_active, gfx_mag_filter_active);
        ASSERT_EQ(gfx_min_filter_default, gfx_min_filter_active);
        ASSERT_EQ(gfx_mag_filter_default, gfx_mag_filter_active);
    }

    dmRender::DeleteMaterial(m_Context, material);
    dmRender::DeleteMaterial(m_Context, material_no_samplers);
    dmGraphics::DeleteProgram(m_GraphicsContext, program);

    dmGraphics::DeleteTexture(m_GraphicsContext, texture);
    dmGraphics::DeleteVertexBuffer(vx_buffer);
    dmGraphics::DeleteVertexDeclaration(vx_decl);
}


static void NoopDrawDispatch(dmRender::RenderListDispatchParams const & params)
{
    (void) params;
}

static void TestDrawVisibilityDispatch(dmRender::RenderListDispatchParams const & params)
{
    TestDrawDispatchCtx *ctx = (TestDrawDispatchCtx*) params.m_UserData;
    uint32_t* i;

    switch (params.m_Operation)
    {
        case dmRender::RENDER_LIST_OPERATION_BEGIN:
            ctx->m_BeginCalls++;
            break;
        case dmRender::RENDER_LIST_OPERATION_BATCH:
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
            ctx->m_EndCalls++;
            break;
    }
}

static void TestDrawVisibility(dmRender::RenderListVisibilityParams const &params)
{
    // Special test to only enable every other render item
    for (uint32_t i = 0; i < params.m_NumEntries; ++i)
    {
        dmRender::RenderListEntry* entry = &params.m_Entries[i];
        bool intersect = dmIntersection::TestFrustumPoint(*params.m_Frustum, entry->m_WorldPosition);
        entry->m_Visibility = intersect ? dmRender::VISIBILITY_FULL : dmRender::VISIBILITY_NONE;
    }
}

TEST_F(dmRenderTest, TestRenderListCulling)
{
    dmVMath::Matrix4 view = dmVMath::Matrix4::identity();
    dmVMath::Matrix4 proj = dmVMath::Matrix4::orthographic(0.0f, WIDTH, 0.0f, HEIGHT, -1.0f, 1.0f);
    dmRender::SetViewMatrix(m_Context, view);
    dmRender::SetProjectionMatrix(m_Context, proj);

    const uint32_t num_x = 8;
    const uint32_t num_y = 8;
    const uint32_t n = num_y * num_y;

    const dmRender::RenderOrder majors[3] = {
        dmRender::RENDER_ORDER_BEFORE_WORLD,
        dmRender::RENDER_ORDER_WORLD,
        dmRender::RENDER_ORDER_AFTER_WORLD
    };

    uint32_t            num_rendered[2] = {};
    dmVMath::Matrix4*   frustum_matrices[2] = {};
    uint32_t num_cases = DM_ARRAY_SIZE(frustum_matrices);

    frustum_matrices[0]     = 0;            // testing no frustum culling
    num_rendered[0]         = n;            // expecting all items to be rendered

    dmVMath::Matrix4 view_proj = proj * view;
    frustum_matrices[1]     = &view_proj;
    num_rendered[1]         = 5*5;          // Testing the function by setting every other entry as visible

    float step_x = WIDTH / num_x * 2;
    float step_y = HEIGHT / num_y * 2;

    for (uint32_t c = 0; c < num_cases; ++c)
    {
        TestDrawDispatchCtx ctx;
        memset(&ctx, 0x00, sizeof(TestDrawDispatchCtx));

        dmRender::RenderListBegin(m_Context);

        uint8_t dispatch1 = dmRender::RenderListMakeDispatch(m_Context, TestDrawVisibilityDispatch, TestDrawVisibility, &ctx);
        uint8_t dispatch2 = dmRender::RenderListMakeDispatch(m_Context, TestDrawVisibilityDispatch, TestDrawVisibility, &ctx);

        dmRender::RenderListEntry* out = dmRender::RenderListAlloc(m_Context, n);

        for (uint32_t y = 0; y < num_y; ++y)
        {
            for (uint32_t x = 0; x < num_x; ++x)
            {
                uint32_t i = y * num_x + x;

                dmRender::RenderListEntry & entry = out[i];
                bool group1 = i < (n/2);
                entry.m_WorldPosition = Point3(x * step_x, y * step_y, group1 ? i : (i*100 + i));
                entry.m_MajorOrder = majors[i % 3];
                entry.m_MinorOrder = 0;
                entry.m_TagListKey = 0;
                entry.m_Order = i+1;
                entry.m_BatchKey = group1 ? 1 : 2;
                entry.m_Dispatch = group1 ? dispatch1 : dispatch2;
                entry.m_UserData = 0;
                entry.m_Visibility = dmRender::VISIBILITY_NONE;
            }
        }

        dmRender::RenderListSubmit(m_Context, out, out + n);
        dmRender::RenderListEnd(m_Context);

        if (frustum_matrices[c])
        {
            dmRender::FrustumOptions frustum_options;
            frustum_options.m_Matrix = *frustum_matrices[c];
            frustum_options.m_NumPlanes = dmRender::FRUSTUM_PLANES_SIDES;
            dmRender::DrawRenderList(m_Context, 0, 0, &frustum_options, dmRender::SORT_BACK_TO_FRONT);
        }
        else
        {
            dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);
        }

        ASSERT_EQ(ctx.m_BeginCalls, 2);
        ASSERT_GT(ctx.m_BatchCalls, 2);
        ASSERT_EQ(ctx.m_EntriesRendered, num_rendered[c]);
        ASSERT_EQ(ctx.m_EndCalls, 2);
    }
}

struct FrustumCullDedupTestCtx
{
    uint32_t m_VisibilityEntryProcessCount;
};

static void CountingTestDrawVisibility(dmRender::RenderListVisibilityParams const &params)
{
    FrustumCullDedupTestCtx* ctx = (FrustumCullDedupTestCtx*)params.m_UserData;
    ASSERT_NE(ctx, (FrustumCullDedupTestCtx*)0);
    ctx->m_VisibilityEntryProcessCount += params.m_NumEntries;
    TestDrawVisibility(params);
}

TEST_F(dmRenderTest, TestRenderListFrustumCullDeduplication)
{
    dmVMath::Matrix4 view = dmVMath::Matrix4::identity();
    dmVMath::Matrix4 proj = dmVMath::Matrix4::orthographic(0.0f, WIDTH, 0.0f, HEIGHT, -1.0f, 1.0f);
    dmRender::SetViewMatrix(m_Context, view);
    dmRender::SetProjectionMatrix(m_Context, proj);

    dmVMath::Matrix4 view_proj = proj * view;
    dmRender::FrustumOptions frustum_options;
    frustum_options.m_Matrix = view_proj;
    frustum_options.m_NumPlanes = dmRender::FRUSTUM_PLANES_SIDES;

    FrustumCullDedupTestCtx test_ctx;
    memset(&test_ctx, 0, sizeof(test_ctx));

    const uint32_t n = 16;
    dmRender::RenderListBegin(m_Context);
    uint8_t dispatch = dmRender::RenderListMakeDispatch(m_Context, NoopDrawDispatch, CountingTestDrawVisibility, &test_ctx);
    dmRender::RenderListEntry* out = dmRender::RenderListAlloc(m_Context, n);
    for (uint32_t i = 0; i < n; ++i)
    {
        dmRender::RenderListEntry& entry = out[i];
        entry.m_WorldPosition = Point3((float)i * 10.f, 0.f, (float)(i + 1));
        entry.m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
        entry.m_MinorOrder = 0;
        entry.m_TagListKey = 0;
        entry.m_Order = i + 1;
        entry.m_BatchKey = 1;
        entry.m_Dispatch = dispatch;
        entry.m_UserData = 0;
    }

    dmRender::RenderListSubmit(m_Context, out, out + n);
    dmRender::RenderListEnd(m_Context);

    // First frustum draw: every entry is uncached, visibility runs for all n entries.
    dmRender::DrawRenderList(m_Context, 0, 0, &frustum_options, dmRender::SORT_BACK_TO_FRONT);
    const uint32_t after_first = test_ctx.m_VisibilityEntryProcessCount;
    ASSERT_EQ(after_first, n);

    // Same frustum and list size: culling is skipped for existing entries (counter unchanged).
    dmRender::DrawRenderList(m_Context, 0, 0, &frustum_options, dmRender::SORT_BACK_TO_FRONT);
    ASSERT_EQ(test_ctx.m_VisibilityEntryProcessCount, after_first);

    const uint32_t k = 4;
    dmRender::RenderListEntry* extra = dmRender::RenderListAlloc(m_Context, k);
    for (uint32_t i = 0; i < k; ++i)
    {
        dmRender::RenderListEntry& entry = extra[i];
        entry.m_WorldPosition = Point3((float)(n + i) * 10.f, 100.f, (float)(n + i + 1));
        entry.m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
        entry.m_MinorOrder = 0;
        entry.m_TagListKey = 0;
        entry.m_Order = n + i + 1;
        entry.m_BatchKey = 1;
        entry.m_Dispatch = dispatch;
        entry.m_UserData = 0;
    }
    dmRender::RenderListSubmit(m_Context, extra, extra + k);

    // List grew: only the k new entries run visibility; original n stay skipped (counter += k).
    dmRender::DrawRenderList(m_Context, 0, 0, &frustum_options, dmRender::SORT_BACK_TO_FRONT);
    ASSERT_EQ(test_ctx.m_VisibilityEntryProcessCount, after_first + k);

    frustum_options.m_NumPlanes = dmRender::FRUSTUM_PLANES_ALL;
    // Different frustum key (plane count): all n+k entries must be culled again.
    dmRender::DrawRenderList(m_Context, 0, 0, &frustum_options, dmRender::SORT_BACK_TO_FRONT);
    const uint32_t total_after_plane_change = (n + k) * 2;
    ASSERT_EQ(test_ctx.m_VisibilityEntryProcessCount, total_after_plane_change);

    // No frustum: every render list entry is forced to VISIBILITY_FULL (asserted below).
    dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);

    dmRender::RenderContext* rc = (dmRender::RenderContext*)m_Context;
    const uint32_t total_entries = n + k;
    ASSERT_EQ(rc->m_RenderList.Size(), total_entries);
    for (uint32_t i = 0; i < total_entries; ++i)
    {
        ASSERT_EQ(rc->m_RenderList[i].m_Visibility, dmRender::VISIBILITY_FULL);
    }
}

struct TestRenderListOrderDispatchCtx
{
    int m_BeginCalls;
    int m_BatchCalls;
    int m_EndCalls;
    int m_EntriesRendered;
    int m_Order;
    int m_MajorOrder;
    float m_Z;
};

static void TestRenderListOrderDispatch(dmRender::RenderListDispatchParams const & params)
{
    TestRenderListOrderDispatchCtx *ctx = (TestRenderListOrderDispatchCtx*) params.m_UserData;
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

            for ( i=params.m_Begin; i != params.m_End; ++i, ++ctx->m_MajorOrder)
            {
                // major order, sequential (world before, world, after) indpenendent of minor order
                ASSERT_EQ(ctx->m_MajorOrder, params.m_Buf[*i].m_MajorOrder);
                 // verify strictly increasing z for world entries.
                 ASSERT_GT(params.m_Buf[*i].m_WorldPosition.getZ(), ctx->m_Z);
                 ctx->m_Z = params.m_Buf[*i].m_WorldPosition.getZ();
                 ctx->m_Order = params.m_Buf[*i].m_Order;
             }
             ctx->m_EntriesRendered++;
            break;
        default:
            ASSERT_EQ(params.m_Operation, dmRender::RENDER_LIST_OPERATION_END);
            ASSERT_EQ(ctx->m_BeginCalls, 1);
            ASSERT_EQ(ctx->m_EndCalls, 0);
            ctx->m_EndCalls++;
            break;
    }
}

TEST_F(dmRenderTest, TestRenderListOrder)
{
    // Test order (sort), major order and minor order integrity and minor order breaking batching
    TestRenderListOrderDispatchCtx ctx;
    memset(&ctx, 0x00, sizeof(TestRenderListOrderDispatchCtx));

    dmVMath::Matrix4 view = dmVMath::Matrix4::identity();
    dmVMath::Matrix4 proj = dmVMath::Matrix4::orthographic(0.0f, WIDTH, 0.0f, HEIGHT, 0.1f, 1.0f);
    dmRender::SetViewMatrix(m_Context, view);
    dmRender::SetProjectionMatrix(m_Context, proj);

    dmRender::RenderListBegin(m_Context);
    uint8_t dispatch = dmRender::RenderListMakeDispatch(m_Context, TestRenderListOrderDispatch, 0, &ctx);

    const uint32_t n = 3;
    const uint32_t orders[n] = {  99997,99998,99999 };
    const uint32_t major_orders[n] = {
        dmRender::RENDER_ORDER_BEFORE_WORLD,
        dmRender::RENDER_ORDER_WORLD,
        dmRender::RENDER_ORDER_AFTER_WORLD
    };
    const uint32_t minor_orders[n] = { 0,1,1 };

    dmRender::RenderListEntry* out = dmRender::RenderListAlloc(m_Context, n);
    for (uint32_t i=0;i!=n;i++)
    {
        dmRender::RenderListEntry & entry = out[i];
        entry.m_WorldPosition = Point3(0,0,orders[i]);
        entry.m_MajorOrder = major_orders[i];
        entry.m_MinorOrder = 0;
        entry.m_TagListKey = 0;
        entry.m_Order = orders[i];
        entry.m_BatchKey = 0;
        entry.m_Dispatch = dispatch;
        entry.m_UserData = 0;
    }
    dmRender::RenderListSubmit(m_Context, out, out + n);
    dmRender::RenderListEnd(m_Context);
    dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);
    ASSERT_EQ(ctx.m_BeginCalls, 1);
    ASSERT_EQ(ctx.m_BatchCalls, 1);
    ASSERT_EQ(ctx.m_EntriesRendered, 1);
    ASSERT_EQ(ctx.m_EndCalls, 1);
    ASSERT_EQ(ctx.m_Order, orders[2]); // highest after world entry
    ASSERT_EQ(ctx.m_Z, orders[2]);

    dmRender::RenderListBegin(m_Context);
    memset(&ctx, 0x00, sizeof(TestRenderListOrderDispatchCtx));
    dispatch = dmRender::RenderListMakeDispatch(m_Context, TestRenderListOrderDispatch, 0, &ctx);
    out = dmRender::RenderListAlloc(m_Context, n);
    for (uint32_t i=0;i!=n;i++)
    {
        dmRender::RenderListEntry & entry = out[i];
        entry.m_WorldPosition = Point3(0,0,orders[i]);
        entry.m_MajorOrder = major_orders[i];
        entry.m_MinorOrder = minor_orders[i];
        entry.m_TagListKey = 0;
        entry.m_Order = orders[i];
        entry.m_BatchKey = 0;
        entry.m_Dispatch = dispatch;
        entry.m_UserData = 0;
    }
    dmRender::RenderListSubmit(m_Context, out, out + n);
    dmRender::RenderListEnd(m_Context);
    dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);
    ASSERT_EQ(ctx.m_BeginCalls, 1);
    ASSERT_EQ(ctx.m_BatchCalls, 2);
    ASSERT_EQ(ctx.m_EntriesRendered, 2);
    ASSERT_EQ(ctx.m_EndCalls, 1);
    ASSERT_EQ(ctx.m_Order, orders[2]);
    ASSERT_EQ(ctx.m_Z, orders[2]);
}

TEST_F(dmRenderTest, TestRenderListDebug)
{
    // Test submitting debug drawing when there is no other drawing going on

    // See DEF-1475 Crash: Physics debug with one GO containing collision object crashes engine
    //
    //   Reason: Render system failed to set Capacity properly to account for debug
    //           render objects being flushed in during render.

    dmVMath::Matrix4 view = dmVMath::Matrix4::identity();
    dmVMath::Matrix4 proj = dmVMath::Matrix4::orthographic(0.0f, WIDTH, 0.0f, HEIGHT, 0.1f, 1.0f);
    dmRender::SetViewMatrix(m_Context, view);
    dmRender::SetProjectionMatrix(m_Context, proj);

    dmRender::RenderListBegin(m_Context);
    dmRender::Square2d(m_Context, 0, 0, 100, 100, Vector4(0,0,0,0));
    dmRender::RenderListEnd(m_Context);

    dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);
    dmRender::DrawDebug2d(m_Context);
    dmRender::DrawDebug3d(m_Context, 0);
}

struct TestSortFrontToBackCtx
{
    int   m_BeginCalls;
    int   m_BatchCalls;
    int   m_EndCalls;
    float m_LastZ;
    int   m_Count;
};

static void TestSortFrontToBackDispatch(dmRender::RenderListDispatchParams const & params)
{
    TestSortFrontToBackCtx* ctx = (TestSortFrontToBackCtx*) params.m_UserData;
    switch (params.m_Operation)
    {
        case dmRender::RENDER_LIST_OPERATION_BEGIN:
            ctx->m_BeginCalls++;
            break;
        case dmRender::RENDER_LIST_OPERATION_BATCH:
        {
            ctx->m_BatchCalls++;
            for (uint32_t* i = params.m_Begin; i != params.m_End; ++i)
            {
                const dmRender::RenderListEntry& e = params.m_Buf[*i];
                if (e.m_MajorOrder == dmRender::RENDER_ORDER_WORLD)
                {
                    if (ctx->m_Count > 0)
                    {
                        // For FRONT_TO_BACK we expect decreasing world z (relative to the default back-to-front tests)
                        ASSERT_LT(e.m_WorldPosition.getZ(), ctx->m_LastZ);
                    }
                    ctx->m_LastZ = e.m_WorldPosition.getZ();
                    ctx->m_Count++;
                }
            }
        } break;
        default:
            ctx->m_EndCalls++;
            break;
    }
}

TEST_F(dmRenderTest, TestRenderListSortFrontToBack)
{
    // Ensure FRONT_TO_BACK produces the inverse ordering compared to the existing back-to-front behavior
    TestSortFrontToBackCtx ctx = {};
    ctx.m_LastZ = 0.0f;

    dmVMath::Matrix4 view = dmVMath::Matrix4::identity();
    dmVMath::Matrix4 proj = dmVMath::Matrix4::orthographic(0.0f, WIDTH, 0.0f, HEIGHT, 0.1f, 1.0f);
    dmRender::SetViewMatrix(m_Context, view);
    dmRender::SetProjectionMatrix(m_Context, proj);

    dmRender::RenderListBegin(m_Context);
    uint8_t dispatch = dmRender::RenderListMakeDispatch(m_Context, TestSortFrontToBackDispatch, 0, &ctx);

    const uint32_t n = 5;
    const uint32_t orders[n] = { 2, 5, 1, 4, 3 }; // unsorted insertion order

    dmRender::RenderListEntry* out = dmRender::RenderListAlloc(m_Context, n);
    for (uint32_t i = 0; i < n; ++i)
    {
        dmRender::RenderListEntry& e = out[i];
        e.m_WorldPosition = Point3(0, 0, (float)orders[i]);
        e.m_MajorOrder    = dmRender::RENDER_ORDER_WORLD;
        e.m_MinorOrder    = 0;
        e.m_TagListKey    = 0;
        e.m_Order         = orders[i];
        e.m_BatchKey      = 0;
        e.m_Dispatch      = dispatch;
        e.m_UserData      = 0;
    }
    dmRender::RenderListSubmit(m_Context, out, out + n);
    dmRender::RenderListEnd(m_Context);

    dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_FRONT_TO_BACK);

    ASSERT_EQ(ctx.m_BeginCalls, 1);
    ASSERT_EQ(ctx.m_BatchCalls, 1);
    ASSERT_EQ(ctx.m_EndCalls, 1);
    ASSERT_EQ(ctx.m_Count, (int)n);
}

struct TestSortNoneCtx
{
    const uint32_t* m_Expected;
    uint32_t        m_Count;
    uint32_t        m_Index;
};

static void TestSortNoneDispatch(dmRender::RenderListDispatchParams const & params)
{
    TestSortNoneCtx* ctx = (TestSortNoneCtx*) params.m_UserData;
    if (params.m_Operation != dmRender::RENDER_LIST_OPERATION_BATCH)
        return;

    for (uint32_t* i = params.m_Begin; i != params.m_End; ++i)
    {
        const dmRender::RenderListEntry& e = params.m_Buf[*i];
        if (e.m_MajorOrder != dmRender::RENDER_ORDER_WORLD)
            continue;
        ASSERT_LT(ctx->m_Index, ctx->m_Count);
        ASSERT_EQ((uint32_t)e.m_WorldPosition.getZ(), ctx->m_Expected[ctx->m_Index]);
        ctx->m_Index++;
    }
}

TEST_F(dmRenderTest, TestRenderListSortNoneUsesInsertionOrder)
{
    // With SORT_NONE we should iterate entries in insertion order
    const uint32_t expected[] = { 7, 1, 5, 3, 9 };

    TestSortNoneCtx ctx = {};
    ctx.m_Expected = expected;
    ctx.m_Count    = DM_ARRAY_SIZE(expected);
    ctx.m_Index    = 0;

    dmVMath::Matrix4 view = dmVMath::Matrix4::identity();
    dmVMath::Matrix4 proj = dmVMath::Matrix4::orthographic(0.0f, WIDTH, 0.0f, HEIGHT, 0.1f, 1.0f);
    dmRender::SetViewMatrix(m_Context, view);
    dmRender::SetProjectionMatrix(m_Context, proj);

    dmRender::RenderListBegin(m_Context);
    uint8_t dispatch = dmRender::RenderListMakeDispatch(m_Context, TestSortNoneDispatch, 0, &ctx);

    dmRender::RenderListEntry* out = dmRender::RenderListAlloc(m_Context, ctx.m_Count);
    for (uint32_t i = 0; i < ctx.m_Count; ++i)
    {
        dmRender::RenderListEntry& e = out[i];
        e.m_WorldPosition = Point3(0, 0, (float)expected[i]);
        e.m_MajorOrder    = dmRender::RENDER_ORDER_WORLD;
        e.m_MinorOrder    = 0;
        e.m_TagListKey    = 0;
        e.m_Order         = expected[i];
        e.m_BatchKey      = 0;
        e.m_Dispatch      = dispatch;
        e.m_UserData      = 0;
    }

    dmRender::RenderListSubmit(m_Context, out, out + ctx.m_Count);
    dmRender::RenderListEnd(m_Context);

    dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_NONE);

    ASSERT_EQ(ctx.m_Index, ctx.m_Count);
}

#define ASSERT_LINE(index, count, lines, i)\
    ASSERT_EQ(char_width * count, lines[i].m_Width);\
    ASSERT_EQ(index, lines[i].m_Index);\
    ASSERT_EQ(count, lines[i].m_Count);

static inline float ExpectedHeight(float line_height, float num_lines, float leading)
{
    return num_lines * (line_height * fabsf(leading)) - line_height * (fabsf(leading) - 1.0f);
}

TEST_F(dmRenderTest, TextAlignment)
{
    dmRender::TextMetrics metrics = {0};

    const int charwidth     = 2;
    const int ascent        = 2;
    const int descent       = 1;
    const int lineheight    = ascent + descent;

    float tracking;
    int numlines;

    dmRender::HFontRenderBackend font_backend = dmRender::CreateFontRenderBackend();

    float leadings[] = { 1.0f, 2.0f, 0.5f };
    for( size_t i = 0; i < sizeof(leadings)/sizeof(leadings[0]); ++i )
    {
        float leading = leadings[i];
        tracking = 0.0f;
        numlines = 3;

        TextLayoutSettings settings = {0};
        settings.m_Width        = 8*charwidth;
        settings.m_Leading      = leading;
        settings.m_Tracking     = tracking;
        settings.m_LineBreak    = true;

        dmRender::GetTextMetrics(font_backend, m_SystemFontMap, "Hello World Bonanza", &settings, &metrics);
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

    dmRender::DestroyFontRenderBackend(font_backend);
}


TEST_F(dmRenderTest, GetTextMetrics)
{

    dmRender::HFontRenderBackend font_backend = dmRender::CreateFontRenderBackend();

    dmRender::TextMetrics metrics = {0};

    const int charwidth     = 2;
    const int ascent        = 2;
    const int descent       = 1;
    const int lineheight    = ascent + descent;


    TextLayoutSettings settings = {0};
    settings.m_Width = 0;
    settings.m_Leading = 1.0f;
    settings.m_Tracking = 0.0f;
    settings.m_LineBreak = false;

    GetTextMetrics(font_backend, m_SystemFontMap, "Hello World", &settings, &metrics);
    ASSERT_EQ(ascent, metrics.m_MaxAscent);
    ASSERT_EQ(descent, metrics.m_MaxDescent);
    ASSERT_EQ(charwidth*11, metrics.m_Width);
    ASSERT_EQ(lineheight*1, metrics.m_Height);

    // line break in the middle of the sentence
    int numlines = 2;

    settings.m_Width = 8*charwidth;
    settings.m_Leading = 1.0f;
    settings.m_Tracking = 0.0f;
    settings.m_LineBreak = true;

    GetTextMetrics(font_backend, m_SystemFontMap, "Hello World", &settings, &metrics);
    ASSERT_EQ(ascent, metrics.m_MaxAscent);
    ASSERT_EQ(descent, metrics.m_MaxDescent);
    ASSERT_EQ(charwidth*5, metrics.m_Width);
    ASSERT_EQ(lineheight*numlines, metrics.m_Height);


    settings.m_Width = 8*charwidth;
    settings.m_Leading = 2.0f;
    settings.m_Tracking = 0.0f;
    settings.m_LineBreak = true;

    GetTextMetrics(font_backend, m_SystemFontMap, "Hello World", &settings, &metrics);
    ASSERT_EQ(ascent, metrics.m_MaxAscent);
    ASSERT_EQ(descent, metrics.m_MaxDescent);
    ASSERT_EQ(charwidth*5, metrics.m_Width);
    ASSERT_EQ(ExpectedHeight(lineheight, numlines, settings.m_Leading), metrics.m_Height);

    settings.m_Width = 8*charwidth;
    settings.m_Leading = 0.0f;
    settings.m_Tracking = 0.0f;
    settings.m_LineBreak = true;

    GetTextMetrics(font_backend, m_SystemFontMap, "Hello World", &settings, &metrics);
    ASSERT_EQ(ascent, metrics.m_MaxAscent);
    ASSERT_EQ(descent, metrics.m_MaxDescent);
    ASSERT_EQ(charwidth*5, metrics.m_Width);
    ASSERT_EQ(ExpectedHeight(lineheight, numlines, settings.m_Leading), metrics.m_Height);

    settings.m_Width = 8*charwidth;
    settings.m_Leading = 1.0f;
    settings.m_Tracking = 0.0f;
    settings.m_LineBreak = true;

    numlines = 3;
    GetTextMetrics(font_backend, m_SystemFontMap, "Hello World Bonanza", &settings, &metrics);
    ASSERT_EQ(ascent, metrics.m_MaxAscent);
    ASSERT_EQ(descent, metrics.m_MaxDescent);
    ASSERT_EQ(charwidth*7, metrics.m_Width);
    ASSERT_EQ(ExpectedHeight(lineheight, numlines, settings.m_Leading), metrics.m_Height);
    ASSERT_EQ(numlines, metrics.m_LineCount);

    dmRender::DestroyFontRenderBackend(font_backend);
}

TEST_F(dmRenderTest, GetPreparedTextMetrics)
{
    dmRender::HFontRenderBackend font_backend = dmRender::CreateFontRenderBackend();

    const char* text = "Hello World Bonanza";
    TextLayoutSettings settings = {0};
    settings.m_Width = 16.0f;
    settings.m_Leading = 1.0f;
    settings.m_Tracking = 0.0f;
    settings.m_LineBreak = true;

    dmRender::TextMetrics raw_metrics = {0};
    dmRender::GetTextMetrics(font_backend, m_SystemFontMap, text, &settings, &raw_metrics);

    HTextLayout layout = CreateTextLayout(m_SystemFontMap, text, settings);
    ASSERT_NE((HTextLayout)0, layout);
    dmRender::TextMetrics prepared_metrics = {0};
    dmRender::GetTextMetrics(m_SystemFontMap, layout, &prepared_metrics);

    ASSERT_EQ(raw_metrics.m_Width, prepared_metrics.m_Width);
    ASSERT_EQ(raw_metrics.m_Height, prepared_metrics.m_Height);
    ASSERT_EQ(raw_metrics.m_MaxAscent, prepared_metrics.m_MaxAscent);
    ASSERT_EQ(raw_metrics.m_MaxDescent, prepared_metrics.m_MaxDescent);
    ASSERT_EQ(raw_metrics.m_LineCount, prepared_metrics.m_LineCount);

    TextLayoutRelease(layout);
    dmRender::DestroyFontRenderBackend(font_backend);
}

TEST_F(dmRenderTest, GetTextMetricsWithNullPreparedLayout)
{
    dmRender::TextMetrics metrics = {};
    dmRender::GetTextMetrics(m_SystemFontMap, (HTextLayout)0, &metrics);

    ASSERT_EQ(0.0f, metrics.m_Width);
    ASSERT_EQ(0.0f, metrics.m_Height);
    ASSERT_EQ(2.0f, metrics.m_MaxAscent);
    ASSERT_EQ(1.0f, metrics.m_MaxDescent);
    ASSERT_EQ(0u, metrics.m_LineCount);
}

TEST_F(dmRenderTest, CreateFontVertexDataWithPreparedTextLayoutMatchesRawTextLayout)
{
    dmRender::HFontRenderBackend font_backend = dmRender::CreateFontRenderBackend();

    const char* text = "Hello World Bonanza";
    const uint32_t max_vertices = 128;
    const uint32_t vertex_stride = dmRender::GetFontVertexSize(font_backend);

    TextLayoutSettings settings = {0};
    settings.m_Width = 16.0f;
    settings.m_Leading = 1.0f;
    settings.m_Tracking = 0.0f;
    settings.m_LineBreak = true;

    dmRender::TextEntry te = {};
    te.m_Transform = Matrix4::identity();
    te.m_FaceColor = COLOR_WHITE_RGBA;
    te.m_OutlineColor = COLOR_TRANSPARENT_RGBA;
    te.m_ShadowColor = COLOR_TRANSPARENT_RGBA;
    te.m_Width = settings.m_Width;
    te.m_Height = 0.0f;
    te.m_Leading = settings.m_Leading;
    te.m_Tracking = settings.m_Tracking;
    te.m_LineBreak = settings.m_LineBreak;
    te.m_Align = dmRender::TEXT_ALIGN_LEFT;
    te.m_VAlign = dmRender::TEXT_VALIGN_TOP;

    dmArray<uint8_t> raw_vertices;
    raw_vertices.SetCapacity(max_vertices * vertex_stride);
    raw_vertices.SetSize(max_vertices * vertex_stride);
    memset(raw_vertices.Begin(), 0, raw_vertices.Size());

    uint32_t raw_count = dmRender::CreateFontVertexData(font_backend, m_SystemFontMap, 0, text, te, 1.0f, 1.0f, 1.0f, raw_vertices.Begin(), max_vertices);

    HTextLayout layout = CreateTextLayout(m_SystemFontMap, text, settings);
    ASSERT_NE((HTextLayout)0, layout);
    dmRender::TextEntry prepared_te = te;
    prepared_te.m_TextLayout = layout;

    dmArray<uint8_t> prepared_vertices;
    prepared_vertices.SetCapacity(max_vertices * vertex_stride);
    prepared_vertices.SetSize(max_vertices * vertex_stride);
    memset(prepared_vertices.Begin(), 0, prepared_vertices.Size());

    uint32_t prepared_count = dmRender::CreateFontVertexData(font_backend, m_SystemFontMap, 0, text, prepared_te, 1.0f, 1.0f, 1.0f, prepared_vertices.Begin(), max_vertices);

    ASSERT_EQ(raw_count, prepared_count);
    ASSERT_EQ(0, memcmp(raw_vertices.Begin(), prepared_vertices.Begin(), raw_count * vertex_stride));

    TextLayoutRelease(layout);
    dmRender::DestroyFontRenderBackend(font_backend);
}

TEST_F(dmRenderTest, CreateFontVertexDataUsesPreparedTextLayout)
{
    dmRender::HFontRenderBackend font_backend = dmRender::CreateFontRenderBackend();

    const char* text = "Hello World";
    const uint32_t max_vertices = 128;
    const uint32_t vertex_stride = dmRender::GetFontVertexSize(font_backend);

    dmRender::TextEntry te = {};
    te.m_Transform = Matrix4::identity();
    te.m_FaceColor = COLOR_WHITE_RGBA;
    te.m_OutlineColor = COLOR_TRANSPARENT_RGBA;
    te.m_ShadowColor = COLOR_TRANSPARENT_RGBA;
    te.m_Width = 128.0f;
    te.m_Height = 0.0f;
    te.m_Leading = 1.0f;
    te.m_Tracking = 0.0f;
    te.m_LineBreak = false;
    te.m_Align = dmRender::TEXT_ALIGN_LEFT;
    te.m_VAlign = dmRender::TEXT_VALIGN_TOP;

    dmArray<uint8_t> raw_vertices;
    raw_vertices.SetCapacity(max_vertices * vertex_stride);
    raw_vertices.SetSize(max_vertices * vertex_stride);
    memset(raw_vertices.Begin(), 0, raw_vertices.Size());

    uint32_t raw_count = dmRender::CreateFontVertexData(font_backend, m_SystemFontMap, 0, text, te, 1.0f, 1.0f, 1.0f, raw_vertices.Begin(), max_vertices);

    TextLayoutSettings wrapped_settings = {0};
    wrapped_settings.m_Width = 16.0f;
    wrapped_settings.m_Leading = 1.0f;
    wrapped_settings.m_Tracking = 0.0f;
    wrapped_settings.m_LineBreak = true;

    HTextLayout wrapped_layout = CreateTextLayout(m_SystemFontMap, text, wrapped_settings);
    ASSERT_NE((HTextLayout)0, wrapped_layout);
    dmRender::TextEntry prepared_te = te;
    prepared_te.m_TextLayout = wrapped_layout;

    dmArray<uint8_t> prepared_vertices;
    prepared_vertices.SetCapacity(max_vertices * vertex_stride);
    prepared_vertices.SetSize(max_vertices * vertex_stride);
    memset(prepared_vertices.Begin(), 0, prepared_vertices.Size());

    uint32_t prepared_count = dmRender::CreateFontVertexData(font_backend, m_SystemFontMap, 0, text, prepared_te, 1.0f, 1.0f, 1.0f, prepared_vertices.Begin(), max_vertices);

    ASSERT_EQ(raw_count, prepared_count);
    ASSERT_NE(0, memcmp(raw_vertices.Begin(), prepared_vertices.Begin(), raw_count * vertex_stride));

    TextLayoutRelease(wrapped_layout);
    dmRender::DestroyFontRenderBackend(font_backend);
}

TEST_F(dmRenderTest, DrawTextUsesPreparedTextLayoutThroughRenderQueue)
{
    dmVMath::Matrix4 view = dmVMath::Matrix4::identity();
    dmVMath::Matrix4 proj = dmVMath::Matrix4::orthographic(0.0f, WIDTH, 0.0f, HEIGHT, 0.1f, 1.0f);
    dmRender::SetViewMatrix(m_Context, view);
    dmRender::SetProjectionMatrix(m_Context, proj);

    dmGraphics::ShaderDescBuilder shader_desc_builder;
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, "foo", 3);
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, "foo", 3);

    dmGraphics::HProgram program = dmGraphics::NewProgram(m_GraphicsContext, shader_desc_builder.Get(), 0, 0);
    dmRender::HMaterial old_material = dmRender::GetFontMapMaterial(m_SystemFontMap);
    dmRender::HMaterial material = dmRender::NewMaterial(m_Context, program);
    dmRender::SetFontMapMaterial(m_SystemFontMap, material);

    const char* text = "Hello World Bonanza";

    dmRender::DrawTextParams raw_params;
    raw_params.m_Text = text;
    raw_params.m_Width = 128.0f;
    raw_params.m_Height = 0.0f;
    raw_params.m_Leading = 1.0f;
    raw_params.m_Tracking = 0.0f;
    raw_params.m_LineBreak = false;
    raw_params.m_Align = dmRender::TEXT_ALIGN_LEFT;
    raw_params.m_VAlign = dmRender::TEXT_VALIGN_TOP;

    ASSERT_EQ(dmRender::RESULT_OK, dmRender::ClearRenderObjects(m_Context));

    dmArray<uint8_t> raw_vertices;
    HTextLayout queued_raw_layout = 0;
    float raw_radius_sq = 0.0f;
    uint32_t raw_count = QueueTextAndCopyVertices(m_Context, m_SystemFontMap, raw_params, raw_vertices, &queued_raw_layout, &raw_radius_sq);

    ASSERT_EQ((HTextLayout)0, queued_raw_layout);
    ASSERT_EQ(1u, m_Context->m_TextContext.m_TextEntries.Size());
    ASSERT_GT(raw_count, 0u);
    ASSERT_GT(raw_vertices.Size(), 0u);
    ASSERT_EQ((uint32_t)(strlen(text) + 1), m_Context->m_TextContext.m_TextBuffer.Size());

    TextLayoutSettings wrapped_settings = {0};
    wrapped_settings.m_Width = 16.0f;
    wrapped_settings.m_Leading = 1.0f;
    wrapped_settings.m_Tracking = 0.0f;
    wrapped_settings.m_LineBreak = true;

    HTextLayout wrapped_layout = CreateTextLayout(m_SystemFontMap, text, wrapped_settings);
    ASSERT_NE((HTextLayout)0, wrapped_layout);

    dmRender::DrawTextParams prepared_params = raw_params;
    prepared_params.m_Text = 0;
    prepared_params.m_TextLayout = wrapped_layout;

    ASSERT_EQ(dmRender::RESULT_OK, dmRender::ClearRenderObjects(m_Context));

    dmArray<uint8_t> prepared_vertices;
    HTextLayout queued_prepared_layout = 0;
    float prepared_radius_sq = 0.0f;
    uint32_t prepared_count = QueueTextAndCopyVertices(m_Context, m_SystemFontMap, prepared_params, prepared_vertices, &queued_prepared_layout, &prepared_radius_sq);

    ASSERT_EQ(wrapped_layout, queued_prepared_layout);
    ASSERT_EQ(1u, m_Context->m_TextContext.m_TextEntries.Size());
    ASSERT_EQ(raw_count, prepared_count);
    ASSERT_EQ(raw_vertices.Size(), prepared_vertices.Size());
    ASSERT_NE(0, memcmp(raw_vertices.Begin(), prepared_vertices.Begin(), raw_vertices.Size()));
    ASSERT_NE(raw_radius_sq, prepared_radius_sq);
    ASSERT_EQ(0u, m_Context->m_TextContext.m_TextBuffer.Size());

    TextLayoutRelease(wrapped_layout);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::ClearRenderObjects(m_Context));
    dmRender::SetFontMapMaterial(m_SystemFontMap, old_material);
    dmRender::DeleteMaterial(m_Context, material);
    dmGraphics::DeleteProgram(m_GraphicsContext, program);
}

TEST_F(dmRenderTest, DrawTextPreparedTextLayoutRetainedUntilClear)
{
    dmVMath::Matrix4 view = dmVMath::Matrix4::identity();
    dmVMath::Matrix4 proj = dmVMath::Matrix4::orthographic(0.0f, WIDTH, 0.0f, HEIGHT, 0.1f, 1.0f);
    dmRender::SetViewMatrix(m_Context, view);
    dmRender::SetProjectionMatrix(m_Context, proj);

    dmGraphics::ShaderDescBuilder shader_desc_builder;
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, "foo", 3);
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, "foo", 3);

    dmGraphics::HProgram program = dmGraphics::NewProgram(m_GraphicsContext, shader_desc_builder.Get(), 0, 0);
    dmRender::HMaterial old_material = dmRender::GetFontMapMaterial(m_SystemFontMap);
    dmRender::HMaterial material = dmRender::NewMaterial(m_Context, program);
    dmRender::SetFontMapMaterial(m_SystemFontMap, material);

    TextLayoutSettings settings = {};
    settings.m_Width = 16.0f;
    settings.m_Leading = 1.0f;
    settings.m_Tracking = 0.0f;
    settings.m_LineBreak = true;

    HTextLayout layout = CreateTextLayout(m_SystemFontMap, "Hello World Bonanza", settings);
    ASSERT_NE((HTextLayout)0, layout);

    TextLayoutAcquire(layout);
    ASSERT_EQ(2u, layout->m_RefCount);

    dmRender::DrawTextParams params;
    params.m_Text = 0;
    params.m_TextLayout = layout;
    params.m_Width = settings.m_Width;
    params.m_Height = 0.0f;
    params.m_Leading = settings.m_Leading;
    params.m_Tracking = settings.m_Tracking;
    params.m_LineBreak = settings.m_LineBreak;
    params.m_Align = dmRender::TEXT_ALIGN_LEFT;
    params.m_VAlign = dmRender::TEXT_VALIGN_TOP;

    ASSERT_EQ(dmRender::RESULT_OK, dmRender::ClearRenderObjects(m_Context));

    dmRender::RenderListBegin(m_Context);
    dmRender::DrawText(m_Context, m_SystemFontMap, 0, 0, params);
    ASSERT_EQ(1u, m_Context->m_TextContext.m_TextEntries.Size());
    ASSERT_EQ(layout, m_Context->m_TextContext.m_TextEntries[0].m_TextLayout);
    ASSERT_EQ(3u, layout->m_RefCount);

    TextLayoutRelease(layout);
    ASSERT_EQ(2u, layout->m_RefCount);

    dmRender::FlushTexts(m_Context, dmRender::RENDER_ORDER_AFTER_WORLD, true);
    dmRender::RenderListEnd(m_Context);
    dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);
    ASSERT_EQ(2u, layout->m_RefCount);

    ASSERT_EQ(dmRender::RESULT_OK, dmRender::ClearRenderObjects(m_Context));
    ASSERT_EQ(1u, layout->m_RefCount);

    TextLayoutRelease(layout);
    dmRender::SetFontMapMaterial(m_SystemFontMap, old_material);
    dmRender::DeleteMaterial(m_Context, material);
    dmGraphics::DeleteProgram(m_GraphicsContext, program);
}

// TEST_F(dmRenderTest, GetTextMetricsMeasureTrailingSpace)
// {
//     dmRender::TextMetrics metricsHello;
//     dmRender::TextMetrics metricsMultiLineHelloAndSpace;
//     dmRender::TextMetrics metricsSingleLineHelloAndSpace;
//     dmRender::TextMetrics metricsSingleLineSpace;

//     GetTextMetrics(m_SystemFontMap, "Hello", 0, true, 1.0f, 0.0f, &metricsHello);
//     GetTextMetrics(m_SystemFontMap, "Hello      ", 0, true, 1.0f, 0.0f, &metricsMultiLineHelloAndSpace);
//     ASSERT_EQ(metricsHello.m_Width, metricsMultiLineHelloAndSpace.m_Width);

//     GetTextMetrics(m_SystemFontMap, "Hello      ", 0, false, 1.0f, 0.0f, &metricsSingleLineHelloAndSpace);
//     ASSERT_LT(metricsHello.m_Width, metricsSingleLineHelloAndSpace.m_Width);

//     GetTextMetrics(m_SystemFontMap, " ", 0, false, 1.0f, 0.0f, &metricsSingleLineSpace);
//     ASSERT_GT(metricsSingleLineSpace.m_Width, 0);
// }


struct SRangeCtx
{
    uint32_t m_NumRanges;
    dmRender::RenderListRange m_Ranges[16];
};

static void CollectRenderEntryRange(void* _ctx, uint32_t tag_list_key, size_t start, size_t count)
{
    SRangeCtx* ctx = (SRangeCtx*)_ctx;
    if (ctx->m_NumRanges >= sizeof(ctx->m_Ranges)/sizeof(ctx->m_Ranges[0]))
    {
        return;
    }
    dmRender::RenderListRange& range = ctx->m_Ranges[ctx->m_NumRanges++];
    range.m_TagListKey = tag_list_key;
    range.m_Start = start;
    range.m_Count = count;
}

TEST_F(dmRenderTest, FindRanges)
{
    // Create unsorted list
    const uint32_t count = 32; // Large enough to avoid the initial intro sort
    dmRender::RenderListEntry entries[count];
    uint32_t indices[count];
    for( uint32_t i = 0; i < count; ++i) {
        indices[i] = i;
        entries[i].m_Order = i;
        entries[i].m_TagListKey = i % 5;
    }

    // Sort the entries
    dmRender::RenderListEntrySorter sort;
    sort.m_Base = entries;
    std::stable_sort(indices, indices + count, sort);

    // Make sure it's sorted
    bool sorted = true;
    uint32_t last_idx = 0;
    uint32_t tag_list_key = entries[0].m_TagListKey;
    for( uint32_t i = 0; i < count; ++i)
    {
        uint32_t idx = indices[i];

        if (entries[idx].m_TagListKey < tag_list_key)
        {
            sorted = false;
            break;
        }
        else if(entries[idx].m_TagListKey > tag_list_key)
        {
            last_idx = idx;
        }

        if (idx < last_idx)
        {
            sorted = false;
            break;
        }

        tag_list_key = entries[idx].m_TagListKey;
        last_idx = idx;
    }
    ASSERT_TRUE(sorted);

    SRangeCtx ctx;
    ctx.m_NumRanges = 0;

    dmRender::FindRangeComparator comp;
    comp.m_Entries = entries;
    dmRender::FindRenderListRanges(indices, 0, sizeof(indices)/sizeof(indices[0]), entries, comp, &ctx, CollectRenderEntryRange);

    ASSERT_EQ(5, ctx.m_NumRanges);

    dmRender::RenderListRange range;
    ASSERT_TRUE(dmRender::FindTagListRange(ctx.m_Ranges, ctx.m_NumRanges, 0, range));
    ASSERT_EQ(0, range.m_TagListKey);
    ASSERT_EQ(0, range.m_Start);
    ASSERT_EQ(7, range.m_Count);
    ASSERT_TRUE(dmRender::FindTagListRange(ctx.m_Ranges, ctx.m_NumRanges, 1, range));
    ASSERT_EQ(1, range.m_TagListKey);
    ASSERT_EQ(7, range.m_Start);
    ASSERT_EQ(7, range.m_Count);
    ASSERT_TRUE(dmRender::FindTagListRange(ctx.m_Ranges, ctx.m_NumRanges, 2, range));
    ASSERT_EQ(2, range.m_TagListKey);
    ASSERT_EQ(14, range.m_Start);
    ASSERT_EQ(6, range.m_Count);
    ASSERT_TRUE(dmRender::FindTagListRange(ctx.m_Ranges, ctx.m_NumRanges, 3, range));
    ASSERT_EQ(3, range.m_TagListKey);
    ASSERT_EQ(20, range.m_Start);
    ASSERT_EQ(6, range.m_Count);
    ASSERT_TRUE(dmRender::FindTagListRange(ctx.m_Ranges, ctx.m_NumRanges, 4, range));
    ASSERT_EQ(4, range.m_TagListKey);
    ASSERT_EQ(26, range.m_Start);
    ASSERT_EQ(6, range.m_Count);
}

TEST_F(dmRenderTest, FontMapSetup)
{
    HFontCollection font_collection = FontCollectionCreate();
    FontCollectionAddFont(font_collection, m_Font);

    dmRender::FontMapParams font_map_params;
    font_map_params.m_CacheWidth = 128;
    font_map_params.m_CacheHeight = 128;
    font_map_params.m_CacheCellWidth = 8;
    font_map_params.m_CacheCellHeight = 8;
    font_map_params.m_MaxAscent = 2;
    font_map_params.m_MaxDescent = 1;
    font_map_params.m_FontCollection = font_collection;

    font_map_params.m_GlyphChannels = 4; // Issue https://github.com/defold/defold/issues/11397

    dmRender::HFontMap font = dmRender::NewFontMap(m_Context, m_GraphicsContext, font_map_params);

    ASSERT_NE((dmRender::HFontMap)0, font);

    dmRender::DeleteFontMap(font);
}

TEST_F(dmRenderTest, LightBufferTestSimple)
{
    // Default params (point light, white, intensity 1, range 10, etc.)
    dmRender::LightPrototypeParams light_params;
    dmRender::HLightPrototype light_prototype = dmRender::NewLightPrototype(m_Context, light_params);
    ASSERT_NE((dmRender::HLightPrototype)0, light_prototype);

    {
        dmRender::HLightInstance light0 = dmRender::NewLightInstance(m_Context, light_prototype);
        ASSERT_NE((dmRender::HLightInstance)0, light0);

        dmVMath::Point3 p0;
        dmVMath::Quat r0;
        dmRender::SetLightInstance(m_Context, light0, p0, r0);
        dmRender::DeleteLightInstance(m_Context, light0);
    }

    dmRender::DeleteLightPrototype(m_Context, light_prototype);
}

TEST_F(dmRenderTest, LightBufferTestAllTypes)
{
    dmRender::LightPrototypeParams params;

    params.m_Type = dmRender::LIGHT_TYPE_DIRECTIONAL;
    params.m_Color = dmVMath::Vector4(1.0f, 0.0f, 0.0f, 1.0f);
    params.m_Direction = dmVMath::Vector3(0.0f, -1.0f, 0.0f);
    params.m_Intensity = 2.0f;
    dmRender::HLightPrototype proto_dir = dmRender::NewLightPrototype(m_Context, params);
    ASSERT_NE((dmRender::HLightPrototype)0, proto_dir);
    dmRender::HLightInstance inst_dir = dmRender::NewLightInstance(m_Context, proto_dir);
    ASSERT_NE((dmRender::HLightInstance)0, inst_dir);
    dmRender::SetLightInstance(m_Context, inst_dir, dmVMath::Point3(0, 10, 0), dmVMath::Quat::identity());
    dmRender::DeleteLightInstance(m_Context, inst_dir);
    dmRender::DeleteLightPrototype(m_Context, proto_dir);

    params.m_Type = dmRender::LIGHT_TYPE_POINT;
    params.m_Color = dmVMath::Vector4(0.0f, 1.0f, 0.0f, 1.0f);
    params.m_Range = 25.0f;
    params.m_Intensity = 0.5f;
    dmRender::HLightPrototype proto_point = dmRender::NewLightPrototype(m_Context, params);
    ASSERT_NE((dmRender::HLightPrototype)0, proto_point);
    dmRender::HLightInstance inst_point = dmRender::NewLightInstance(m_Context, proto_point);
    ASSERT_NE((dmRender::HLightInstance)0, inst_point);
    dmRender::SetLightInstance(m_Context, inst_point, dmVMath::Point3(5.0f, 0.0f, -3.0f), dmVMath::Quat::identity());
    dmRender::DeleteLightInstance(m_Context, inst_point);
    dmRender::DeleteLightPrototype(m_Context, proto_point);

    params.m_Type = dmRender::LIGHT_TYPE_SPOT;
    params.m_Color = dmVMath::Vector4(0.0f, 0.0f, 1.0f, 1.0f);
    params.m_Direction = dmVMath::Vector3(0.0f, 0.0f, -1.0f);
    params.m_InnerConeAngle = 0.2f;
    params.m_OuterConeAngle = 0.8f;
    params.m_Range = 15.0f;
    dmRender::HLightPrototype proto_spot = dmRender::NewLightPrototype(m_Context, params);
    ASSERT_NE((dmRender::HLightPrototype)0, proto_spot);
    dmRender::HLightInstance inst_spot = dmRender::NewLightInstance(m_Context, proto_spot);
    ASSERT_NE((dmRender::HLightInstance)0, inst_spot);
    dmRender::SetLightInstance(m_Context, inst_spot, dmVMath::Point3(0, 0, 10), dmVMath::Quat::identity());
    dmRender::DeleteLightInstance(m_Context, inst_spot);
    dmRender::DeleteLightPrototype(m_Context, proto_spot);
}

TEST_F(dmRenderTest, LightBufferTestMultipleInstances)
{
    dmRender::LightPrototypeParams params;
    params.m_Type = dmRender::LIGHT_TYPE_POINT;
    params.m_Intensity = 1.0f;
    params.m_Range = 10.0f;

    dmRender::HLightPrototype prototype = dmRender::NewLightPrototype(m_Context, params);
    ASSERT_NE((dmRender::HLightPrototype)0, prototype);

    dmRender::HLightInstance lights[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        lights[i] = dmRender::NewLightInstance(m_Context, prototype);
        ASSERT_NE((dmRender::HLightInstance)0, lights[i]);
        dmRender::SetLightInstance(m_Context, lights[i], dmVMath::Point3((float)i, (float)i * 2.0f, (float)i * -1.0f), dmVMath::Quat::identity());
    }

    for (uint32_t i = 0; i < 4; ++i)
    {
        dmRender::DeleteLightInstance(m_Context, lights[i]);
    }
    dmRender::DeleteLightPrototype(m_Context, prototype);
}

TEST_F(dmRenderTest, LightBufferTestIndexReuse)
{
    dmRender::LightPrototypeParams params;
    params.m_Type = dmRender::LIGHT_TYPE_POINT;
    dmRender::HLightPrototype prototype = dmRender::NewLightPrototype(m_Context, params);
    ASSERT_NE((dmRender::HLightPrototype)0, prototype);

    dmRender::HLightInstance a = dmRender::NewLightInstance(m_Context, prototype);
    dmRender::HLightInstance b = dmRender::NewLightInstance(m_Context, prototype);
    ASSERT_NE((dmRender::HLightInstance)0, a);
    ASSERT_NE((dmRender::HLightInstance)0, b);

    dmRender::DeleteLightInstance(m_Context, a);
    dmRender::HLightInstance c = dmRender::NewLightInstance(m_Context, prototype);
    ASSERT_NE((dmRender::HLightInstance)0, c);

    dmRender::SetLightInstance(m_Context, b, dmVMath::Point3(1, 0, 0), dmVMath::Quat::identity());
    dmRender::SetLightInstance(m_Context, c, dmVMath::Point3(2, 0, 0), dmVMath::Quat::identity());

    dmRender::DeleteLightInstance(m_Context, b);
    dmRender::DeleteLightInstance(m_Context, c);
    dmRender::DeleteLightPrototype(m_Context, prototype);
}

TEST_F(dmRenderTest, LightBufferTestSetLightInstanceUpdates)
{
    dmRender::LightPrototypeParams params;
    params.m_Type = dmRender::LIGHT_TYPE_SPOT;
    params.m_Direction = dmVMath::Vector3(0.0f, 0.0f, -1.0f);
    dmRender::HLightPrototype prototype = dmRender::NewLightPrototype(m_Context, params);
    ASSERT_NE((dmRender::HLightPrototype)0, prototype);

    dmRender::HLightInstance instance = dmRender::NewLightInstance(m_Context, prototype);
    ASSERT_NE((dmRender::HLightInstance)0, instance);

    dmRender::SetLightInstance(m_Context, instance, dmVMath::Point3(0, 0, 0), dmVMath::Quat::identity());
    dmRender::SetLightInstance(m_Context, instance, dmVMath::Point3(1, 2, 3), dmVMath::Quat::identity());
    // Z rotation by 45 degrees: quat (0, 0, sin(θ/2), cos(θ/2))
    const float half = 3.14159265f / 8.0f;
    dmVMath::Quat rot_z(0.0f, 0.0f, sinf(half), cosf(half));
    dmRender::SetLightInstance(m_Context, instance, dmVMath::Point3(1, 2, 3), rot_z);

    dmRender::DeleteLightInstance(m_Context, instance);
    dmRender::DeleteLightPrototype(m_Context, prototype);
}

TEST(Constants, Constant)
{
    dmhash_t original_name_hash = dmHashString64("test_constant");
    dmRender::HConstant constant = dmRender::NewConstant(original_name_hash);
    ASSERT_TRUE(constant != 0);
    ASSERT_EQ(original_name_hash, dmRender::GetConstantName(constant));

    dmhash_t original_name_hash_new = dmHashString64("test_constant_new");
    dmRender::SetConstantName(constant, original_name_hash_new);
    ASSERT_EQ(original_name_hash_new, dmRender::GetConstantName(constant));

    ////////////////////////////////////////////////////////////
    dmRender::SetConstantLocation(constant, 17);
    ASSERT_EQ(17, dmRender::GetConstantLocation(constant));

    ////////////////////////////////////////////////////////////
    dmVMath::Vector4 original_values[] = {dmVMath::Vector4(1,2,3,4), dmVMath::Vector4(5,6,7,8)};

    dmRender::SetConstantValues(constant, original_values, DM_ARRAY_SIZE(original_values));
    uint32_t num_values = 0;
    dmVMath::Vector4* values = dmRender::GetConstantValues(constant, &num_values);
    ASSERT_TRUE(values != 0);
    ASSERT_EQ(DM_ARRAY_SIZE(original_values), num_values);
    ASSERT_ARRAY_EQ_LEN(original_values, values, num_values);

    ////////////////////////////////////////////////////////////
    dmRender::SetConstantType(constant, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_NORMAL);
    ASSERT_EQ(dmRenderDDF::MaterialDesc::CONSTANT_TYPE_NORMAL, dmRender::GetConstantType(constant));

    ////////////////////////////////////////////////////////////
    dmRender::SetConstantType(constant, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_TIME);
    ASSERT_EQ(dmRenderDDF::MaterialDesc::CONSTANT_TYPE_TIME, dmRender::GetConstantType(constant));

    ////////////////////////////////////////////////////////////
    dmRender::SetConstantType(constant, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_TIME);
    ASSERT_EQ(dmRenderDDF::MaterialDesc::CONSTANT_TYPE_TIME, dmRender::GetConstantType(constant));

    ////////////////////////////////////////////////////////////
    dmRender::DeleteConstant(constant);
}

TEST_F(dmRenderTest, ConstantTypeTimeSetsTimeAndDt)
{
    // Build a simple shader with a single vec4 uniform "time" backed by a uniform buffer.
    dmGraphics::ShaderDescBuilder shader_desc_builder;
    shader_desc_builder.AddTypeMember("time", dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
    shader_desc_builder.AddUniformBuffer("time", 0, 0, dmGraphics::GetShaderTypeSize(dmGraphics::ShaderDesc::SHADER_TYPE_VEC4));

    const char* vertex_data   = "";
    const char* fragment_data = "";
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, vertex_data, (uint32_t) strlen(vertex_data));
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, fragment_data, (uint32_t) strlen(fragment_data));

    dmGraphics::ShaderDesc* shader = shader_desc_builder.Get();
    dmGraphics::HProgram program = dmGraphics::NewProgram(m_GraphicsContext, shader, 0, 0);

    const dmGraphics::Uniform* time_uniform = dmGraphics::GetUniform(program, dmHashString64("time"));
    ASSERT_NE(dmGraphics::INVALID_UNIFORM_LOCATION, time_uniform->m_Location);

    dmGraphics::NullProgram* null_program = (dmGraphics::NullProgram*) program;

    uint32_t set           = UNIFORM_LOCATION_GET_OP0(time_uniform->m_Location);
    uint32_t binding       = UNIFORM_LOCATION_GET_OP1(time_uniform->m_Location);
    uint32_t buffer_offset = UNIFORM_LOCATION_GET_OP2(time_uniform->m_Location);

    dmGraphics::ProgramResourceBinding& pgm_res = null_program->m_BaseProgram.m_ResourceBindings[set][binding];
    uint32_t uniform_offset = pgm_res.m_UniformBufferOffset + buffer_offset;

    // Set frame time values on the render context.
    float time = 123.0f;
    float dt   = 1.0f / 60.0f;
    dmRender::SetFrameTime(m_Context, time, dt);

    // Enable the program so constants can be written to its uniform buffer.
    dmGraphics::EnableProgram(m_GraphicsContext, program);

    dmVMath::Matrix4 identity = dmVMath::Matrix4::identity();
    dmRender::SetProgramConstant(m_Context,
                                 m_GraphicsContext,
                                 identity,
                                 identity,
                                 dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330,
                                 dmRenderDDF::MaterialDesc::CONSTANT_TYPE_TIME,
                                 program,
                                 time_uniform->m_Location,
                                 0);

    // Verify that the written uniform data matches (time, dt, 0, 0).
    float* written = (float*) (null_program->m_UniformData + uniform_offset);
    ASSERT_NEAR(time, written[0], EPSILON);
    ASSERT_NEAR(dt,   written[1], EPSILON);
    ASSERT_NEAR(0.0f, written[2], EPSILON);
    ASSERT_NEAR(0.0f, written[3], EPSILON);

    dmGraphics::DisableProgram(m_GraphicsContext);
    dmGraphics::DeleteProgram(m_GraphicsContext, program);
}

struct IterConstantContext
{
    int m_Count;
    dmArray<dmhash_t> m_Expected;
};

static void IterateNameConstantsCallback(dmhash_t name_hash, void* _ctx)
{
    IterConstantContext* ctx = (IterConstantContext*)_ctx;
    ctx->m_Count++;

    for (uint32_t i = 0; i < ctx->m_Expected.Size(); ++i)
    {
        if (ctx->m_Expected[i] == name_hash)
        {
            ctx->m_Expected.EraseSwap(i);
            break;
        }
    }
}

TEST(Constants, NamedConstantsArray)
{
    dmHashEnableReverseHash(true);
    dmRender::HNamedConstantBuffer buffer = dmRender::NewNamedConstantBuffer();
    ASSERT_TRUE(buffer != 0);

    dmVMath::Vector4* values = 0;
    uint32_t num_values;
    bool result;

    dmVMath::Vector4 original_values[] = {
        dmVMath::Vector4(2,4,8,10),
        dmVMath::Vector4(1,3,5,7),
        dmVMath::Vector4(11,12,13,14),
    };

    dmhash_t name_hash_array = dmHashString64("test_constant_array");

    ////////////////////////////////////////////////////////////
    // Test 1: set a single value
    ////////////////////////////////////////////////////////////
    dmRender::SetNamedConstantAtIndex(buffer, name_hash_array, &original_values[0], 1, 0, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER);
    result = dmRender::GetNamedConstant(buffer, name_hash_array, &values, &num_values);
    ASSERT_TRUE(result);
    ASSERT_EQ(num_values, 1);
    ASSERT_VEC4(original_values[0], values[0]);

    ////////////////////////////////////////////////////////////
    // Test 2: set two values
    ////////////////////////////////////////////////////////////
    dmRender::SetNamedConstantAtIndex(buffer, name_hash_array, &original_values[1], 1, 1, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER);
    result = dmRender::GetNamedConstant(buffer, name_hash_array, &values, &num_values);
    ASSERT_TRUE(result);
    ASSERT_EQ(num_values, 2);
    ASSERT_VEC4(original_values[0], values[0]);
    ASSERT_VEC4(original_values[1], values[1]);

    ////////////////////////////////////////////////////////////
    // Test 3: set a third value at an offset
    ////////////////////////////////////////////////////////////
    dmRender::SetNamedConstantAtIndex(buffer, name_hash_array, &original_values[2], 1, 6, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER);
    result = dmRender::GetNamedConstant(buffer, name_hash_array, &values, &num_values);
    ASSERT_TRUE(result);
    ASSERT_EQ(num_values, 7);
    ASSERT_VEC4(original_values[0], values[0]);
    ASSERT_VEC4(original_values[1], values[1]);
    ASSERT_VEC4(original_values[2], values[6]);

    ////////////////////////////////////////////////////////////
    // Test 4: add a second constant after array
    ////////////////////////////////////////////////////////////
    dmhash_t name_hash_single_value = dmHashString64("test_single_value");
    dmVMath::Vector4 test_single_value(1337,7331,3311,1133);

    dmRender::SetNamedConstant(buffer, name_hash_single_value, &test_single_value, 1);
    result = dmRender::GetNamedConstant(buffer, name_hash_single_value, &values, &num_values);
    ASSERT_TRUE(result);
    ASSERT_EQ(num_values, 1);
    ASSERT_VEC4(test_single_value, values[0]);

    ////////////////////////////////////////////////////////////
    // Test 4: resize test_constant_array to a single value, test_single_value should be the same
    ////////////////////////////////////////////////////////////
    dmRender::SetNamedConstant(buffer, name_hash_array, original_values, 1);
    result = dmRender::GetNamedConstant(buffer, name_hash_single_value, &values, &num_values);
    ASSERT_TRUE(result);
    ASSERT_EQ(num_values, 1);
    ASSERT_VEC4(test_single_value, values[0]);

    ////////////////////////////////////////////////////////////
    // Test 5: remove test_single_value constant
    ////////////////////////////////////////////////////////////
    dmRender::SetNamedConstantAtIndex(buffer, name_hash_array, &original_values[2], 1, 6, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER);
    ASSERT_EQ(GetNamedConstantCount(buffer), 2);
    dmRender::RemoveNamedConstant(buffer, name_hash_single_value);
    ASSERT_EQ(GetNamedConstantCount(buffer), 1);

    result = dmRender::GetNamedConstant(buffer, name_hash_array, &values, &num_values);
    ASSERT_TRUE(result);
    ASSERT_EQ(num_values, 7);

    // All intermediate vectors should be 0 i.e values of index [1...6]
    dmVMath::Vector4 test_zero_vec(0,0,0,0);
    for (int i = 1; i < num_values-1; ++i)
    {
        ASSERT_VEC4(test_zero_vec, values[i]);
    }
    ASSERT_VEC4(original_values[2], values[6]);

    ////////////////////////////////////////////////////////////
    // Test: Test iteration of the names constants
    ////////////////////////////////////////////////////////////

    // Readd it to have some more data
    dmRender::SetNamedConstant(buffer, dmHashString64("Value1"), &test_single_value, 1);
    dmRender::SetNamedConstant(buffer, dmHashString64("Value2"), &test_single_value, 1);

    IterConstantContext iter_ctx;
    iter_ctx.m_Count = 0;
    iter_ctx.m_Expected.SetCapacity(3);
    iter_ctx.m_Expected.Push(name_hash_array);
    iter_ctx.m_Expected.Push(dmHashString64("Value1"));
    iter_ctx.m_Expected.Push(dmHashString64("Value2"));
    dmRender::IterateNamedConstants(buffer, IterateNameConstantsCallback, &iter_ctx);

    ASSERT_EQ(0u, iter_ctx.m_Expected.Size());
    ASSERT_EQ(3, iter_ctx.m_Count);

    dmRender::DeleteNamedConstantBuffer(buffer);
}

TEST(Constants, NamedConstants)
{
    dmHashEnableReverseHash(true);

    dmRender::HNamedConstantBuffer buffer = dmRender::NewNamedConstantBuffer();
    ASSERT_TRUE(buffer != 0);

    dmVMath::Vector4* values = 0;
    uint32_t num_values;
    bool result;

    ////////////////////////////////////////////////////////////
    dmVMath::Vector4 original_values[] = {dmVMath::Vector4(2,4,8,10), dmVMath::Vector4(1,3,5,7)};
    dmhash_t name_hash = dmHashString64("test_constant");

    dmRender::HConstant constant = dmRender::NewConstant(name_hash);
    dmRender::SetConstantValues(constant, original_values, DM_ARRAY_SIZE(original_values));

    values = dmRender::GetConstantValues(constant, &num_values);
    ASSERT_TRUE(values != 0);
    ASSERT_EQ(DM_ARRAY_SIZE(original_values), num_values);
    ASSERT_ARRAY_EQ_LEN(original_values, values, num_values);

    dmRender::SetNamedConstants(buffer, &constant, 1);

    ////////////////////////////////////////////////////////////
    result = dmRender::GetNamedConstant(buffer, name_hash, &values, &num_values);
    ASSERT_TRUE(result);
    ASSERT_TRUE(values != 0);
    ASSERT_EQ(DM_ARRAY_SIZE(original_values), num_values);
    ASSERT_ARRAY_EQ_LEN(original_values, values, num_values);

    ////////////////////////////////////////////////////////////
    dmVMath::Vector4 original_values2[] = {dmVMath::Vector4(2,4,8,10), dmVMath::Vector4(1,3,5,7)};

    dmhash_t name_hash_2 = dmHashString64("test_constant_2");
    dmRender::SetNamedConstant(buffer, name_hash_2, original_values2, DM_ARRAY_SIZE(original_values2));

    result = dmRender::GetNamedConstant(buffer, name_hash_2, &values, &num_values);
    ASSERT_TRUE(result);
    ASSERT_TRUE(values != 0);
    ASSERT_EQ(DM_ARRAY_SIZE(original_values2), num_values);
    ASSERT_ARRAY_EQ_LEN(original_values2, values, num_values);

    ////////////////////////////////////////////////////////////
    dmRender::RemoveNamedConstant(buffer, name_hash_2);

    result = dmRender::GetNamedConstant(buffer, name_hash_2, &values, &num_values);
    ASSERT_FALSE(result);

    result = dmRender::GetNamedConstant(buffer, name_hash, &values, &num_values);
    ASSERT_TRUE(result);

    ////////////////////////////////////////////////////////////
    dmRender::ClearNamedConstantBuffer(buffer);

    result = dmRender::GetNamedConstant(buffer, name_hash, &values, &num_values);
    ASSERT_FALSE(result);

    ////////////////////////////////////////////////////////////
    // Test matrix constants
    ////////////////////////////////////////////////////////////
    dmVMath::Matrix4 original_matrix_value[] = {
        dmVMath::Matrix4::identity(),
        dmVMath::Matrix4::identity()};

    original_matrix_value[0].setElem(0,0,1.0f).setElem(1,1,2.0f).setElem(2,2,3.0f).setElem(3,3,4.0f);
    original_matrix_value[1].setElem(0,0,-1.0f).setElem(1,1,-2.0f).setElem(2,2,-3.0f).setElem(3,3,-4.0f);

    dmhash_t matrix_name_hash_1 = dmHashString64("test_matrix_constant_1");

    uint32_t num_values_matrix_test = sizeof(original_matrix_value[0]) / sizeof(dmVMath::Vector4);

    ////////////////////////////////////////////////////////////
    // Set a single matrix value
    dmRender::SetNamedConstant(buffer, matrix_name_hash_1, (dmVMath::Vector4*) &original_matrix_value[0],
        num_values_matrix_test, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4);

    dmRenderDDF::MaterialDesc::ConstantType constant_type;
    result = dmRender::GetNamedConstant(buffer, matrix_name_hash_1, &values, &num_values, &constant_type);
    ASSERT_TRUE(result);
    ASSERT_TRUE(constant_type == dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4);
    ASSERT_TRUE(num_values == num_values_matrix_test);
    ASSERT_ARRAY_EQ_LEN(values, (dmVMath::Vector4*) original_matrix_value, num_values);

    ////////////////////////////////////////////////////////////
    // Remove constant and add the entire array of matrices
    dmRender::RemoveNamedConstant(buffer, matrix_name_hash_1);
    dmRender::SetNamedConstant(buffer, name_hash_2, original_values2, DM_ARRAY_SIZE(original_values2));

    num_values_matrix_test *= DM_ARRAY_SIZE(original_matrix_value);
    dmRender::SetNamedConstant(buffer, matrix_name_hash_1, (dmVMath::Vector4*) original_matrix_value,
        num_values_matrix_test, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4);
    result = dmRender::GetNamedConstant(buffer, matrix_name_hash_1, &values, &num_values, &constant_type);
    ASSERT_TRUE(result);
    ASSERT_TRUE(constant_type == dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4);
    ASSERT_TRUE(num_values == num_values_matrix_test);
    ASSERT_ARRAY_EQ_LEN(values, (dmVMath::Vector4*) original_matrix_value, num_values);

    ////////////////////////////////////////////////////////////
    dmRender::DeleteConstant(constant);
    dmRender::DeleteNamedConstantBuffer(buffer);
}

static bool BatchEntryTestEq(int* a, int* b)
{
    return *a == *b;
}

TEST(Render, BatchIterator)
{
    int array1[] = {1, 1, 1, 2, 2, 5, 5, 5, 5};
    dmRender::BatchIterator<int*> iterator1(DM_ARRAY_SIZE(array1), array1, BatchEntryTestEq);

    ASSERT_TRUE(iterator1.Next());
    ASSERT_EQ(3U, iterator1.Length());
    ASSERT_EQ(array1+0, iterator1.Begin());
    ASSERT_EQ(1U, *iterator1.Begin());

    ASSERT_TRUE(iterator1.Next());
    ASSERT_EQ(2U, iterator1.Length());
    ASSERT_EQ(array1+3, iterator1.Begin());
    ASSERT_EQ(2U, *iterator1.Begin());

    ASSERT_TRUE(iterator1.Next());
    ASSERT_EQ(4U, iterator1.Length());
    ASSERT_EQ(array1+5, iterator1.Begin());
    ASSERT_EQ(5U, *iterator1.Begin());

    ASSERT_FALSE(iterator1.Next());
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
