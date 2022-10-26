// Copyright 2020-2022 The Defold Foundation
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
#include <dmsdk/vectormath/cpp/vectormath_aos.h>
#include <dmsdk/dlib/intersection.h>

#include <dlib/hash.h>
#include <dlib/math.h>

#include <script/script.h>
#include <algorithm> // std::stable_sort

#include "render/render.h"
#include "render/render_private.h"
#include "render/font_renderer_private.h"

const static uint32_t WIDTH = 600;
const static uint32_t HEIGHT = 400;

using namespace dmVMath;

class dmRenderTest : public jc_test_base_class
{
protected:
    dmRender::HRenderContext m_Context;
    dmGraphics::HContext m_GraphicsContext;
    dmScript::HContext m_ScriptContext;
    dmRender::HFontMap m_SystemFontMap;

    virtual void SetUp()
    {
        dmGraphics::Initialize();
        m_GraphicsContext = dmGraphics::NewContext(dmGraphics::ContextParams());
        dmRender::RenderContextParams params;
        m_ScriptContext = dmScript::NewContext(0, 0, true);
        params.m_MaxRenderTargets = 1;
        params.m_MaxInstances = 2;
        params.m_ScriptContext = m_ScriptContext;
        params.m_MaxDebugVertexCount = 256;
        params.m_MaxCharacters = 256;
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

TEST_F(dmRenderTest, TestFontMapTextureFiltering)
{
    dmRender::HFontMap bitmap_font_map;
    dmRender::FontMapParams bitmap_font_map_params;
    bitmap_font_map_params.m_CacheWidth = 1;
    bitmap_font_map_params.m_CacheHeight = 1;
    bitmap_font_map_params.m_CacheCellWidth = 8;
    bitmap_font_map_params.m_CacheCellHeight = 8;
    bitmap_font_map_params.m_MaxAscent = 2;
    bitmap_font_map_params.m_MaxDescent = 1;
    bitmap_font_map_params.m_Glyphs.SetCapacity(1);
    bitmap_font_map_params.m_Glyphs.SetSize(1);
    memset((void*)&bitmap_font_map_params.m_Glyphs[0], 0, sizeof(dmRender::Glyph)*1);
    for (uint32_t i = 0; i < 1; ++i)
    {
        bitmap_font_map_params.m_Glyphs[i].m_Character = i;
        bitmap_font_map_params.m_Glyphs[i].m_Width = 1;
        bitmap_font_map_params.m_Glyphs[i].m_LeftBearing = 1;
        bitmap_font_map_params.m_Glyphs[i].m_Advance = 2;
        bitmap_font_map_params.m_Glyphs[i].m_Ascent = 2;
        bitmap_font_map_params.m_Glyphs[i].m_Descent = 1;
    }
    bitmap_font_map_params.m_ImageFormat = dmRenderDDF::TYPE_BITMAP;

    bitmap_font_map = dmRender::NewFontMap(m_GraphicsContext, bitmap_font_map_params);
    ASSERT_TRUE(VerifyFontMapMinFilter(bitmap_font_map, dmGraphics::TEXTURE_FILTER_LINEAR));
    ASSERT_TRUE(VerifyFontMapMagFilter(bitmap_font_map, dmGraphics::TEXTURE_FILTER_LINEAR));
    dmRender::DeleteFontMap(bitmap_font_map);
}

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
    uint32_t flags = dmGraphics::BUFFER_TYPE_COLOR0_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT;
    dmGraphics::HRenderTarget target = dmGraphics::NewRenderTarget(m_GraphicsContext, flags, creation_params, params);
    dmGraphics::DeleteRenderTarget(target);
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

    dmRender::DrawRenderList(m_Context, 0, 0, 0);

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

static inline dmGraphics::ShaderDesc::Shader MakeDDFShader(const char* data, uint32_t count)
{
    dmGraphics::ShaderDesc::Shader ddf;
    memset(&ddf,0,sizeof(ddf));
    ddf.m_Source.m_Data  = (uint8_t*)data;
    ddf.m_Source.m_Count = count;
    return ddf;
}

TEST_F(dmRenderTest, TestRenderListDrawState)
{
    dmRender::RenderListBegin(m_Context);

    dmGraphics::ShaderDesc::Shader shader = MakeDDFShader("foo", 3);
    dmGraphics::HVertexProgram vp = dmGraphics::NewVertexProgram(m_GraphicsContext, &shader);
    dmGraphics::HFragmentProgram fp = dmGraphics::NewFragmentProgram(m_GraphicsContext, &shader);

    dmRender::HMaterial material = dmRender::NewMaterial(m_Context, vp, fp);
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
    dmRender::DrawRenderList(m_Context, 0, 0, 0);

    dmGraphics::PipelineState ps_after = dmGraphics::GetPipelineState(m_GraphicsContext);

    ASSERT_EQ(0, memcmp(&ps_before, &ps_after, sizeof(dmGraphics::PipelineState)));

    dmGraphics::DeleteVertexProgram(vp);
    dmGraphics::DeleteFragmentProgram(fp);
    dmRender::DeleteMaterial(m_Context, material);

    dmGraphics::DeleteVertexBuffer(vx_buffer);
    dmGraphics::DeleteVertexDeclaration(vx_decl);
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
        bool intersect = dmIntersection::TestFrustumPoint(*params.m_Frustum, entry->m_WorldPosition, true);
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

        dmRender::DrawRenderList(m_Context, 0, 0, frustum_matrices[c]);

        ASSERT_EQ(ctx.m_BeginCalls, 2);
        ASSERT_GT(ctx.m_BatchCalls, 2);
        ASSERT_EQ(ctx.m_EntriesRendered, num_rendered[c]);
        ASSERT_EQ(ctx.m_EndCalls, 2);
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
    dmRender::DrawRenderList(m_Context, 0, 0, 0);
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
    dmRender::DrawRenderList(m_Context, 0, 0, 0);
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

    dmRender::DrawRenderList(m_Context, 0, 0, 0);
    dmRender::DrawDebug2d(m_Context);
    dmRender::DrawDebug3d(m_Context, 0);
}

static float Metric(const char* text, int n, bool measure_trailing_space)
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
    total_lines = dmRender::Layout("", 100, lines, lines_count, &w, Metric, false);
    ASSERT_EQ(0, total_lines);
    ASSERT_EQ(0, w);

    total_lines = dmRender::Layout("x", 100, lines, lines_count, &w, Metric, false);
    ASSERT_EQ(1, total_lines);
    ASSERT_LINE(0, 1, lines, 0);
    ASSERT_EQ(char_width * 1, w);

    total_lines = dmRender::Layout("x\x00 123", 100, lines, lines_count, &w, Metric, false);
    ASSERT_EQ(1, total_lines);
    ASSERT_LINE(0, 1, lines, 0);
    ASSERT_EQ(char_width * 1, w);

    total_lines = dmRender::Layout("x", 0, lines, lines_count, &w, Metric, false);
    ASSERT_EQ(1, total_lines);
    ASSERT_LINE(0, 1, lines, 0);
    ASSERT_EQ(char_width * 1, w);

    total_lines = dmRender::Layout("foo", 3 * char_width, lines, lines_count, &w, Metric, false);
    ASSERT_EQ(1, total_lines);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_EQ(char_width * 3, w);

    total_lines = dmRender::Layout("foo", 3 * char_width - 1, lines, lines_count, &w, Metric, false);
    ASSERT_EQ(1, total_lines);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_EQ(char_width * 3, w);

    total_lines = dmRender::Layout("foo bar", 3 * char_width, lines, lines_count, &w, Metric, false);
    ASSERT_EQ(2, total_lines);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_LINE(4, 3, lines, 1);
    ASSERT_EQ(char_width * 3, w);

    total_lines = dmRender::Layout("foo bar", 1000, lines, lines_count, &w, Metric, false);
    ASSERT_EQ(1, total_lines);
    ASSERT_LINE(0, 7, lines, 0);
    ASSERT_EQ(char_width * 7, w);

    total_lines = dmRender::Layout("foo  bar", 1000, lines, lines_count, &w, Metric, false);
    ASSERT_EQ(1, total_lines);
    ASSERT_LINE(0, 8, lines, 0);
    ASSERT_EQ(char_width * 8, w);

    total_lines = dmRender::Layout("foo\n\nbar", 3 * char_width, lines, lines_count, &w, Metric, false);
    ASSERT_EQ(3, total_lines);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_LINE(4, 0, lines, 1);
    ASSERT_LINE(5, 3, lines, 2);
    ASSERT_EQ(char_width * 3, w);

    // 0x200B = Unicode "zero width space", UTF8 representation: E2 80 8B
    total_lines = dmRender::Layout("foo" "\xe2\x80\x8b" "bar", 3 * char_width, lines, lines_count, &w, Metric, false);
    ASSERT_EQ(2, total_lines);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_LINE(6, 3, lines, 1);
    ASSERT_EQ(char_width * 3, w);

    // Note that second line would include a "zero width space" as first
    // character since we don't trim whitespace currently.
    total_lines = dmRender::Layout("foo" "\xe2\x80\x8b\xe2\x80\x8b" "bar", 3 * char_width, lines, lines_count, &w, Metric, false);
    ASSERT_EQ(2, total_lines);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_LINE(6, 4, lines, 1);
    ASSERT_EQ(char_width * 4, w);

    // åäö
    total_lines = dmRender::Layout("\xc3\xa5\xc3\xa4\xc3\xb6", 3 * char_width, lines, lines_count, &w, Metric, false);
    ASSERT_EQ(1, total_lines);
    ASSERT_EQ(char_width * 3, lines[0].m_Width);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_EQ(char_width * 3, w);

    total_lines = dmRender::Layout("Welcome to the Kingdom of Games...", 0, lines, lines_count, &w, Metric, false);
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
    ASSERT_EQ(numlines, metrics.m_LineCount);
}

TEST_F(dmRenderTest, GetTextMetricsMeasureTrailingSpace)
{
    dmRender::TextMetrics metricsHello;
    dmRender::TextMetrics metricsMultiLineHelloAndSpace;
    dmRender::TextMetrics metricsSingleLineHelloAndSpace;
    dmRender::TextMetrics metricsSingleLineSpace;

    dmRender::GetTextMetrics(m_SystemFontMap, "Hello", 0, true, 1.0f, 0.0f, &metricsHello);
    dmRender::GetTextMetrics(m_SystemFontMap, "Hello      ", 0, true, 1.0f, 0.0f, &metricsMultiLineHelloAndSpace);
    ASSERT_EQ(metricsHello.m_Width, metricsMultiLineHelloAndSpace.m_Width);

    dmRender::GetTextMetrics(m_SystemFontMap, "Hello      ", 0, false, 1.0f, 0.0f, &metricsSingleLineHelloAndSpace);
    ASSERT_LT(metricsHello.m_Width, metricsSingleLineHelloAndSpace.m_Width);

    dmRender::GetTextMetrics(m_SystemFontMap, " ", 0, false, 1.0f, 0.0f, &metricsSingleLineSpace);
    ASSERT_GT(metricsSingleLineSpace.m_Width, 0);
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
    for( size_t i = 0; i < sizeof(leadings)/sizeof(leadings[0]); ++i )
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
    dmRender::DeleteConstant(constant);
}

TEST(Constants, NamedConstantsArray)
{
    #define ASSERT_VEC4_EPS 0.0001f
    #define ASSERT_VEC4(exp, act)\
        ASSERT_NEAR(exp.getX(), act.getX(), ASSERT_VEC4_EPS);\
        ASSERT_NEAR(exp.getY(), act.getY(), ASSERT_VEC4_EPS);\
        ASSERT_NEAR(exp.getZ(), act.getZ(), ASSERT_VEC4_EPS);\
        ASSERT_NEAR(exp.getW(), act.getW(), ASSERT_VEC4_EPS);

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

    dmRender::DeleteNamedConstantBuffer(buffer);

    #undef ASSERT_VEC4_EPS
    #undef ASSERT_VEC4
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

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
