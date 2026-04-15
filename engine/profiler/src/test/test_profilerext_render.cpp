#include <stdint.h>
#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <testmain/testmain.h>

#include <dlib/hash.h>
#include <script/script.h>
#include "../../../font/src/text_layout.h"
#include <font/font.h>
#include <font/fontcollection.h>
#include <platform/window.hpp>

#include "../../../graphics/src/graphics_private.h"
#include "../../../graphics/src/test/test_graphics_util.h"
#include "../../../render/src/render/render.h"
#include "../../../render/src/render/render_private.h"
#include "../../../render/src/render/font/fontmap.h"
#include "../../../render/src/render/font/font_glyphbank.h"
#include <render/font_ddf.h>

#include "../profiler_render.h"

static const uint32_t WIDTH = 640;
static const uint32_t HEIGHT = 360;

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

static dmProfileRender::ProfilerFrame* CreateProfilerFrame()
{
    dmProfileRender::ProfilerFrame* frame = new dmProfileRender::ProfilerFrame;
    frame->m_Time = 1;

    dmProfileRender::ProfilerThread* thread = dmProfileRender::FindOrCreateProfilerThread(frame, dmHashString32("Main"));
    thread->m_Time = 1;
    thread->m_SamplesTotalTime = 1000;
    return frame;
}

class ProfilerRenderTest : public jc_test_base_class
{
protected:
    HWindow                     m_Window;
    dmRender::HRenderContext    m_Context;
    dmGraphics::HContext        m_GraphicsContext;
    dmScript::HContext          m_ScriptContext;
    dmRender::HFontMap          m_SystemFontMap;
    dmGraphics::HProgram        m_FontProgram;
    dmRender::HMaterial         m_FontMaterial;
    HFont                       m_Font;
    dmRenderDDF::GlyphBank*     m_GlyphBank;

    void SetUp() override
    {
        dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_NONE);

        WindowCreateParams win_params;
        WindowCreateParamsInitialize(&win_params);
        win_params.m_Width = WIDTH;
        win_params.m_Height = HEIGHT;
        win_params.m_ContextAlphabits = 8;

        m_Window = dmPlatform::NewWindow();
        dmPlatform::OpenWindow(m_Window, win_params);

        dmGraphics::ContextParams graphics_context_params = {};
        graphics_context_params.m_Window = m_Window;
        graphics_context_params.m_DefaultTextureMinFilter = dmGraphics::TEXTURE_FILTER_DEFAULT;
        graphics_context_params.m_DefaultTextureMagFilter = dmGraphics::TEXTURE_FILTER_DEFAULT;

        m_GraphicsContext = dmGraphics::NewContext(graphics_context_params);

        dmScript::ContextParams script_context_params = {};
        script_context_params.m_GraphicsContext = m_GraphicsContext;
        m_ScriptContext = dmScript::NewContext(script_context_params);

        dmRender::RenderContextParams render_context_params = {};
        render_context_params.m_MaxRenderTargets = 1;
        render_context_params.m_MaxInstances = 2;
        render_context_params.m_ScriptContext = m_ScriptContext;
        render_context_params.m_MaxDebugVertexCount = 256;
        render_context_params.m_MaxCharacters = 512;
        render_context_params.m_MaxBatches = 256;
        m_Context = dmRender::NewRenderContext(m_GraphicsContext, render_context_params);
        dmRender::SetLightBufferCount(m_Context, 32);

        m_GlyphBank = CreateGlyphBank(2, 1, 128);
        m_Font = CreateGlyphBankFont("test.glyph_bankc", m_GlyphBank);

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

        m_SystemFontMap = dmRender::NewFontMap(m_Context, m_GraphicsContext, font_map_params);

        dmGraphics::ShaderDescBuilder shader_desc_builder;
        shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, "foo", 3);
        shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, "foo", 3);
        m_FontProgram = dmGraphics::NewProgram(m_GraphicsContext, shader_desc_builder.Get(), 0, 0);
        m_FontMaterial = dmRender::NewMaterial(m_Context, m_FontProgram);
        dmRender::SetFontMapMaterial(m_SystemFontMap, m_FontMaterial);
    }

    void TearDown() override
    {
        dmRender::DeleteFontMap(m_SystemFontMap);
        dmRender::DeleteMaterial(m_Context, m_FontMaterial);
        dmGraphics::DeleteProgram(m_GraphicsContext, m_FontProgram);
        dmRender::DeleteRenderContext(m_Context, 0);

        DestroyGlyphBank(m_GlyphBank);
        FontDestroy(m_Font);

        dmGraphics::CloseWindow(m_GraphicsContext);
        dmGraphics::DeleteContext(m_GraphicsContext);
        dmScript::DeleteContext(m_ScriptContext);

        dmPlatform::CloseWindow(m_Window);
        dmPlatform::DeleteWindow(m_Window);
    }

    void QueueProfiler(dmProfileRender::HRenderProfile render_profile)
    {
        dmRender::RenderListBegin(m_Context);
        dmProfileRender::Draw(render_profile, m_Context, m_SystemFontMap);
        dmRender::RenderListEnd(m_Context);
        dmRender::SetViewMatrix(m_Context, Matrix4::identity());
        dmRender::SetProjectionMatrix(m_Context, Matrix4::orthographic(0.0f, WIDTH, 0.0f, HEIGHT, 1.0f, -1.0f));
    }
};

TEST_F(ProfilerRenderTest, TransientLayoutsSurviveProfilerClearBeforeDraw)
{
    dmProfileRender::HRenderProfile render_profile = dmProfileRender::NewRenderProfile(60.0f);
    dmProfileRender::ProfilerFrame* frame = CreateProfilerFrame();
    dmProfileRender::UpdateRenderProfile(render_profile, frame);
    dmProfileRender::DeleteProfilerFrame(frame);

    ASSERT_EQ(dmRender::RESULT_OK, dmRender::ClearRenderObjects(m_Context));
    QueueProfiler(render_profile);

    // The profiler owns these layouts. Clearing them before DrawRenderList used
    // to leave the render queue with dangling prepared-layout pointers.
    ASSERT_GT(m_Context->m_TextContext.m_TextEntries.Size(), 1u);
    HTextLayout cached_layout = m_Context->m_TextContext.m_TextEntries[0].m_TextLayout;
    HTextLayout transient_layout = m_Context->m_TextContext.m_TextEntries[1].m_TextLayout;
    ASSERT_NE((HTextLayout)0, cached_layout);
    ASSERT_NE((HTextLayout)0, transient_layout);
    ASSERT_NE(cached_layout, transient_layout);

    TextLayoutAcquire(cached_layout);
    TextLayoutAcquire(transient_layout);
    ASSERT_EQ(3u, cached_layout->m_RefCount);
    ASSERT_EQ(3u, transient_layout->m_RefCount);

    dmProfileRender::ClearTransientTextLayouts(render_profile);
    ASSERT_EQ(3u, cached_layout->m_RefCount);
    ASSERT_EQ(2u, transient_layout->m_RefCount);

    dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::ClearRenderObjects(m_Context));
    ASSERT_EQ(2u, cached_layout->m_RefCount);
    ASSERT_EQ(1u, transient_layout->m_RefCount);

    dmProfileRender::DeleteRenderProfile(render_profile);
    ASSERT_EQ(1u, cached_layout->m_RefCount);

    TextLayoutRelease(cached_layout);
    TextLayoutRelease(transient_layout);
}

TEST_F(ProfilerRenderTest, CachedLayoutsSurviveProfileDeletionBeforeDraw)
{
    dmProfileRender::HRenderProfile render_profile = dmProfileRender::NewRenderProfile(60.0f);
    dmProfileRender::ProfilerFrame* frame = CreateProfilerFrame();
    dmProfileRender::UpdateRenderProfile(render_profile, frame);
    dmProfileRender::DeleteProfilerFrame(frame);

    ASSERT_EQ(dmRender::RESULT_OK, dmRender::ClearRenderObjects(m_Context));
    QueueProfiler(render_profile);

    // Deleting the profile before DrawRenderList used to free the prepared
    // layouts out from under the queued text entries.
    ASSERT_GT(m_Context->m_TextContext.m_TextEntries.Size(), 1u);
    HTextLayout cached_layout = m_Context->m_TextContext.m_TextEntries[0].m_TextLayout;
    HTextLayout transient_layout = m_Context->m_TextContext.m_TextEntries[1].m_TextLayout;
    ASSERT_NE((HTextLayout)0, cached_layout);
    ASSERT_NE((HTextLayout)0, transient_layout);
    ASSERT_NE(cached_layout, transient_layout);

    TextLayoutAcquire(cached_layout);
    TextLayoutAcquire(transient_layout);
    ASSERT_EQ(3u, cached_layout->m_RefCount);
    ASSERT_EQ(3u, transient_layout->m_RefCount);

    dmProfileRender::DeleteRenderProfile(render_profile);
    ASSERT_EQ(2u, cached_layout->m_RefCount);
    ASSERT_EQ(2u, transient_layout->m_RefCount);

    dmRender::DrawRenderList(m_Context, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::ClearRenderObjects(m_Context));
    ASSERT_EQ(1u, cached_layout->m_RefCount);
    ASSERT_EQ(1u, transient_layout->m_RefCount);

    TextLayoutRelease(cached_layout);
    TextLayoutRelease(transient_layout);
}

extern "C" void dmExportedSymbols();

int main(int argc, char** argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
