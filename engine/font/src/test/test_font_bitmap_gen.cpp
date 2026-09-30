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

#include "test_font_bitmap_gen.h"
#include <stdio.h>
#include <stdint.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../font_private.h"
#include "../util.h"
#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/sys.h>
#include <dlib/dstrings.h>
#include <dlib/utf8.h>
#include "font.h"
#include "fontcollection.h"
#include "glyph_gen.h"
#include "glyph_vertex.h"
#include "layout_vertex.h"
#include "text_layout.h"

// PNG support is private to the bitmap generator, never libfont.
#define STB_IMAGE_STATIC
#define STBIDEF static inline
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_STATIC
#define STBIWDEF static inline
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

static const char* g_TestImageDirectory = 0;
static uint32_t    g_TestImagesWritten = 0;

// Test-only rendering follows fontviewer: generated atlas, font-library packed
// vertices and the actual font shaders. One hidden context serves the entire
// process; pixels come from an off-screen target, never the window framebuffer.
#include <testmain/testmain.h>
#include <graphics/graphics.h>
#include <graphics_private.h>
#include <test/test_graphics_util.h>
#include <platform/window.hpp>
#include <font_render.h>
#include <font_glyphbank.h>

extern "C" void                       dmExportedSymbols();

static HWindow                        g_ImageWindow = 0;
static dmGraphics::HContext           g_ImageContext = 0;
static dmGraphics::HProgram           g_ImagePrograms[3] = {};
static dmGraphics::HVertexDeclaration g_ImageDeclaration = 0;

static bool                           MakeImageDirectory(const char* directory)
{
    char path[1024];
    if (!directory[0] || dmStrlCpy(path, directory, sizeof(path)) >= sizeof(path))
        return false;
    for (char* cursor = path + 1; *cursor; ++cursor)
    {
        if (*cursor != '/' && *cursor != '\\')
            continue;
        char separator = *cursor;
        *cursor = 0;
        dmSys::Result result = dmSys::Mkdir(path, 0755);
        *cursor = separator;
        if (result != dmSys::RESULT_OK && result != dmSys::RESULT_EXIST)
            return false;
    }
    dmSys::Result result = dmSys::Mkdir(path, 0755);
    return result == dmSys::RESULT_OK || result == dmSys::RESULT_EXIST;
}

static bool LoadImageShader(const char* path, dmArray<char>& data)
{
    FILE* file = fopen(path, "rb");
    if (!file)
        return false;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    data.SetCapacity(size + 1);
    data.SetSize(size + 1);
    bool ok = fread(data.Begin(), 1, size, file) == (size_t)size;
    data[size] = 0;
    fclose(file);
    return ok;
}

static void InitializeFontImages()
{
    if (g_ImageContext)
        return;
#if defined(__linux__)
    // Mesa's optimized 8-bit AoS filtering rounds differently across CPU
    // architectures, which SDF smoothstep amplifies. Use float sampling for
    // reproducible image tests, including manual --case runs.
    ASSERT_EQ(0, setenv("GALLIVM_PERF", "no_aos_sampling", 1));
#endif
    TestMainPlatformInit();
    dmExportedSymbols();
    ASSERT_TRUE(dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_OPENGL));
    g_ImageWindow = dmPlatform::NewWindow();
    WindowCreateParams window;
    WindowCreateParamsInitialize(&window);
    window.m_Width = window.m_Height = 16;
    window.m_Title = "Font unit tests";
    window.m_Hidden = 1;
    window.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGL;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(g_ImageWindow, window));
    dmGraphics::ContextParams params;
    params.m_Window = g_ImageWindow;
    params.m_Width = params.m_Height = 16;
    g_ImageContext = dmGraphics::NewContext(params);
    ASSERT_NE((dmGraphics::HContext)0, g_ImageContext);
    dmGraphics::SetSwapInterval(g_ImageContext, 0);
    g_ImageDeclaration = FontCreateGlyphVertexDeclaration(g_ImageContext);
    const char* vertex_paths[] = { "content/builtins/fonts/font-df.vp", "src/test/data/font_render/font.vp", "src/test/data/font_render/font.vp" };
    const char* fragment_paths[] = { "content/builtins/fonts/font-df.fp", "src/test/data/font_render/font.fp", "src/test/data/font_render/font-fnt.fp" };
    for (uint32_t i = 0; i < 3; ++i)
    {
        dmArray<char> vp, fp;
        ASSERT_TRUE(LoadImageShader(vertex_paths[i], vp));
        ASSERT_TRUE(LoadImageShader(fragment_paths[i], fp));
        dmGraphics::ShaderDescBuilder shaders;
        shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, vp.Begin(), vp.Size() - 1);
        shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, fp.Begin(), fp.Size() - 1);
        const char*                            inputs[] = { "position", "texcoord0", "face_color", "outline_color", "shadow_color", "sdf_params", "layer_mask" };
        dmGraphics::ShaderDesc::ShaderDataType types[] = { dmGraphics::ShaderDesc::SHADER_TYPE_VEC4, dmGraphics::ShaderDesc::SHADER_TYPE_VEC2, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4, dmGraphics::ShaderDesc::SHADER_TYPE_VEC3 };
        for (uint32_t j = 0; j < 7; ++j)
            shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, inputs[j], j, types[j]);
        shaders.AddTypeMember("view_proj", dmGraphics::ShaderDesc::SHADER_TYPE_MAT4);
        shaders.AddUniform("vs_uniforms", 0, 0);
        shaders.AddTexture("texture_sampler", 0, dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D);
        char error[1024] = {};
        g_ImagePrograms[i] = dmGraphics::NewProgram(g_ImageContext, shaders.Get(), error, sizeof(error));
        if (!g_ImagePrograms[i])
            printf("Font shader: %s\n", error);
        ASSERT_NE((dmGraphics::HProgram)0, g_ImagePrograms[i]);
    }
}

static void FinalizeFontImages()
{
    if (!g_ImageContext)
        return;
    for (uint32_t i = 0; i < 3; ++i)
        if (g_ImagePrograms[i])
            dmGraphics::DeleteProgram(g_ImageContext, g_ImagePrograms[i]);
    dmGraphics::DeleteVertexDeclaration(g_ImageDeclaration);
    dmGraphics::DeleteContext(g_ImageContext);
    dmPlatform::CloseWindow(g_ImageWindow);
    dmPlatform::DeleteWindow(g_ImageWindow);
    g_ImageContext = 0;
}

struct ImageGlyph
{
    FontGlyph m_Glyph;
    HFont     m_Font;
    uint32_t  m_Index;
    uint32_t  m_X;
};

struct ImageBank
{
    FontGlyphBankProvider       m_Provider;
    dmArray<FontGlyphBankGlyph> m_Entries;
    dmArray<FontGlyph>          m_Glyphs;
    HFont                       m_Source;
    uint8_t*                    m_Atlas;
};

static uint32_t ImageBankCodepoint(void* context, uint32_t index)
{
    return ((ImageBank*)context)->m_Entries[index].m_Codepoint;
}
static bool ImageBankGlyph(void* context, uint32_t index, FontGlyphBankGlyph* glyph)
{
    *glyph = ((ImageBank*)context)->m_Entries[index];
    return true;
}

static void CreateImageBank(ImageBank& bank, HFont source, const FontGlyphGenParams& params, bool fnt)
{
    memset(&bank.m_Provider, 0, sizeof(bank.m_Provider));
    bank.m_Source = source;
    bank.m_Atlas = 0;
    bank.m_Entries.SetCapacity(128);
    bank.m_Glyphs.SetCapacity(128);
    if (fnt)
    {
        int width, height, channels;
        bank.m_Atlas = stbi_load("src/test/data/font_render/bmfont_example.png", &width, &height, &channels, 4);
        ASSERT_NE((uint8_t*)0, bank.m_Atlas);
        // Bob premultiplies BMFont page pixels before storing the glyph bank.
        // The font shader and ONE/ONE_MINUS_SRC_ALPHA blending expect this.
        for (int i = 0; i < width * height; ++i)
        {
            for (int channel = 0; channel < 3; ++channel)
                bank.m_Atlas[i * 4 + channel] = bank.m_Atlas[i * 4 + channel] * bank.m_Atlas[i * 4 + 3] / 255;
        }
        FILE* file = fopen("src/test/data/font_render/bmfont_example.fnt", "rb");
        ASSERT_NE((FILE*)0, file);
        char line[1024];
        while (fgets(line, sizeof(line), file))
        {
            int id, x, y, w, h, ox, oy, advance;
            if (sscanf(line, "char id=%d x=%d y=%d width=%d height=%d xoffset=%d yoffset=%d xadvance=%d", &id, &x, &y, &w, &h, &ox, &oy, &advance) != 8)
                continue;
            // Validate the atlas rectangle before converting dimensions to unsigned byte sizes.
            ASSERT_GE(x, 0);
            ASSERT_GE(y, 0);
            ASSERT_GE(w, 0);
            ASSERT_GE(h, 0);
            ASSERT_LE(x, width);
            ASSERT_LE(y, height);
            ASSERT_LE(w, width - x);
            ASSERT_LE(h, height - y);
            const size_t row_bytes = (size_t)w * 4;
            const size_t glyph_bytes = row_bytes * (size_t)h;
            ASSERT_LE(glyph_bytes, (size_t)UINT32_MAX);
            FontGlyph glyph = {};
            if (glyph_bytes > 0)
            {
                glyph.m_Bitmap.m_Data = (uint8_t*)malloc(glyph_bytes);
                ASSERT_NE((uint8_t*)0, glyph.m_Bitmap.m_Data);
                for (int row = 0; row < h; ++row)
                    memcpy(glyph.m_Bitmap.m_Data + (size_t)row * row_bytes, bank.m_Atlas + ((size_t)(y + row) * width + x) * 4, row_bytes);
            }
            FontGlyphBankGlyph entry = {};
            entry.m_Codepoint = id;
            entry.m_Width = w;
            entry.m_Advance = advance;
            entry.m_LeftBearing = ox;
            entry.m_Ascent = 40 - oy;
            entry.m_Descent = h - entry.m_Ascent;
            entry.m_Data = glyph.m_Bitmap.m_Data;
            entry.m_DataSize = (uint32_t)glyph_bytes;
            bank.m_Glyphs.Push(glyph);
            bank.m_Entries.Push(entry);
        }
        fclose(file);
        bank.m_Provider.m_GlyphChannels = 4;
    }
    else
    {
        // Snapshot the generated glyphs into the public glyph-bank provider.
        // This exercises prebaked-font layout/fallback, not the Bob compiler.
        for (uint32_t cp = 32; cp < 127; ++cp)
        {
            FontGlyph glyph;
            ASSERT_EQ(FONT_RESULT_OK, FontGenerateGlyph(source, FontGetGlyphIndex(source, cp), &params, &glyph));
            FontGlyphBankGlyph entry = {};
            entry.m_Codepoint = cp;
            entry.m_Width = glyph.m_Bitmap.m_Width;
            entry.m_Advance = glyph.m_Advance;
            entry.m_LeftBearing = glyph.m_LeftBearing - (glyph.m_Bitmap.m_Width - glyph.m_Width) * 0.5f;
            entry.m_Ascent = glyph.m_Ascent;
            entry.m_Descent = glyph.m_Bitmap.m_Height - glyph.m_Ascent;
            entry.m_Data = glyph.m_Bitmap.m_Data;
            entry.m_DataSize = glyph.m_Bitmap.m_DataSize;
            bank.m_Glyphs.Push(glyph);
            bank.m_Entries.Push(entry);
        }
        bank.m_Provider.m_GlyphChannels = FontGetGlyphChannelCount(params.m_OutputBitmap, true, false, 0);
    }
    // BMFont files need not be sorted, while the provider uses binary search.
    for (uint32_t i = 1; i < bank.m_Entries.Size(); ++i)
        for (uint32_t j = i; j > 0 && bank.m_Entries[j].m_Codepoint < bank.m_Entries[j - 1].m_Codepoint; --j)
        {
            FontGlyphBankGlyph tmp = bank.m_Entries[j];
            bank.m_Entries[j] = bank.m_Entries[j - 1];
            bank.m_Entries[j - 1] = tmp;
        }
    for (uint32_t i = 0; i < bank.m_Entries.Size(); ++i)
    {
        bank.m_Provider.m_MaxAscent = dmMath::Max(bank.m_Provider.m_MaxAscent, bank.m_Entries[i].m_Ascent);
        bank.m_Provider.m_MaxDescent = dmMath::Max(bank.m_Provider.m_MaxDescent, bank.m_Entries[i].m_Descent);
    }
    bank.m_Provider.m_Context = &bank;
    bank.m_Provider.m_GetCodepoint = ImageBankCodepoint;
    bank.m_Provider.m_GetGlyph = ImageBankGlyph;
    bank.m_Provider.m_GlyphCount = bank.m_Entries.Size();
}

static bool ResolveImageGlyph(void* context, const TextGlyph& glyph, FontLayoutCachedGlyph* output)
{
    dmArray<ImageGlyph>* cache = (dmArray<ImageGlyph>*)context;
    for (uint32_t i = 0; i < cache->Size(); ++i)
        if ((*cache)[i].m_Font == glyph.m_Font && (*cache)[i].m_Index == glyph.m_GlyphIndex)
        {
            output->m_Glyph = &(*cache)[i].m_Glyph;
            output->m_CellX = (*cache)[i].m_X;
            output->m_CellY = 0;
            return true;
        }
    return false;
}

// Render and finish readback without destroying resources, so transition cases
// can draw both states through the same atlas, buffer and render target.
static void CaptureFontImage(dmGraphics::HRenderTarget target, dmGraphics::HTexture texture,
                             dmGraphics::HVertexBuffer buffer, dmGraphics::HProgram program,
                             uint32_t vertex_count, uint32_t width, uint32_t height, dmArray<uint8_t>& pixels)
{
    dmGraphics::BeginFrame(g_ImageContext);
    dmGraphics::SetRenderTarget(g_ImageContext, target, dmGraphics::RenderTargetBindingParams());
    dmGraphics::SetViewport(g_ImageContext, 0, 0, width, height);
    dmGraphics::Clear(g_ImageContext, dmGraphics::BUFFER_TYPE_COLOR0_BIT, 0, 0, 0, 255, 1, 0);
    dmGraphics::EnableState(g_ImageContext, dmGraphics::STATE_BLEND);
    dmGraphics::SetBlendFunc(g_ImageContext, dmGraphics::BLEND_FACTOR_ONE, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    dmGraphics::EnableProgram(g_ImageContext, program);
    dmVMath::Matrix4 projection = dmVMath::Matrix4::orthographic(0, width, 0, height, -1, 1);
    dmGraphics::SetConstantM4(g_ImageContext, (dmVMath::Vector4*)&projection, 1, dmGraphics::FindUniformLocation(program, "view_proj"));
    dmGraphics::EnableTexture(g_ImageContext, 0, 0, texture);
    dmGraphics::EnableVertexBuffer(g_ImageContext, buffer, 0);
    dmGraphics::EnableVertexDeclaration(g_ImageContext, g_ImageDeclaration, 0, 0, program);
    dmGraphics::Draw(g_ImageContext, dmGraphics::PRIMITIVE_TRIANGLES, 0, vertex_count, 1);
    pixels.SetCapacity(width * height * 4);
    pixels.SetSize(pixels.Capacity());
    dmGraphics::ReadPixels(g_ImageContext, 0, 0, width, height, pixels.Begin(), pixels.Size());
    dmGraphics::DisableVertexDeclaration(g_ImageContext, g_ImageDeclaration);
    dmGraphics::DisableVertexBuffer(g_ImageContext, buffer);
    dmGraphics::DisableTexture(g_ImageContext, 0, texture);
    dmGraphics::SetRenderTarget(g_ImageContext, 0, dmGraphics::RenderTargetBindingParams());
    dmGraphics::Flip(g_ImageContext);
}

static void TestFontImage(const FontImageCase& c)
{
    InitializeFontImages();
    ASSERT_NE((dmGraphics::HContext)0, g_ImageContext);
    const bool  fnt = strcmp(c.m_Source, "fnt") == 0;
    const bool  bank_source = fnt || strstr(c.m_Source, "_bank");
    const bool  bitmap = fnt || strstr(c.m_Source, "bitmap");
    const bool  arabic = strcmp(c.m_Source, "arabic") == 0;
    const bool  paragraph = arabic || strcmp(c.m_Source, "latin") == 0;
    const char* path = arabic ? "src/test/data/NotoSansArabic-Regular.ttf" : paragraph ? "src/test/data/NotoSans-Regular.ttf" :
    strncmp(c.m_Source, "otf", 3) == 0                                                 ? "src/test/data/SourceCodePro-Regular.otf" :
                                                                                         "src/test/data/WorkSans.ttf";
    HFont       source = FontLoadFromPath(path);
    ASSERT_NE((HFont)0, source);
    FontGlyphGenParams params;
    params.m_Scale = FontGetScaleFromSize(source, c.m_Size);
    const float sdf_spread = 3 + c.m_Outline + c.m_ShadowBlur;
    params.m_SdfPadding = sdf_spread;
    params.m_OutlineWidth = c.m_Outline;
    params.m_OutputBitmap = bitmap;
    params.m_HasOutline = c.m_Outline > 0;
    params.m_ShadowBlur = c.m_ShadowBlur;
    params.m_HasShadow = c.m_ShadowAlpha > 0;
    ImageBank bank;
    HFont     font = source;
    if (bank_source)
    {
        CreateImageBank(bank, source, params, fnt);
        font = FontCreateGlyphBank(c.m_Source, &bank.m_Provider);
        ASSERT_NE((HFont)0, font);
        // Prebaked fonts must use legacy layout even in a full-layout binary.
        ASSERT_EQ(TEXT_LAYOUT_TYPE_LEGACY, FontGetLayoutType(font));
    }
    HFontCollection collection = FontCollectionCreate();
    ASSERT_EQ(FONT_RESULT_OK, FontCollectionAddFont(collection, font));
    HFont fallback = 0;
    if (arabic)
    {
        fallback = FontLoadFromPath("src/test/data/NotoSans-Regular.ttf");
        ASSERT_NE((HFont)0, fallback);
        ASSERT_EQ(FONT_RESULT_OK, FontCollectionAddFont(collection, fallback));
    }
    TextRenderStyle base = {};
    base.m_Flags = TEXT_RENDER_STYLE_OUTLINE_WIDTH | TEXT_RENDER_STYLE_OUTLINE_ALPHA | TEXT_RENDER_STYLE_SHADOW_ALPHA;
    base.m_OutlineWidth = c.m_Outline;
    base.m_OutlineAlpha = c.m_OutlineAlpha;
    base.m_ShadowAlpha = c.m_ShadowAlpha;
    dmhash_t default_style = dmHashString64("default");
    FontCollectionSetNamedStyle(collection, default_style, base, 0, 0);
    TextRenderStyle named = base;
    named.m_OutlineWidth = 2;
    FontCollectionSetNamedStyle(collection, dmHashString64("notice"), named, 0, 0);
    TextLayoutSettings settings = {};
    settings.m_Size = c.m_Size;
    FontImageCaptureGeometry geometry = arabic ? g_Capture_arabic : paragraph ? g_Capture_english : g_Capture_single_line;
    if (c.m_EdgeScale > 0.0f)
    {
        const FontImageCaptureGeometry ttf[] = { g_Capture_ttf_edge_half, g_Capture_ttf_edge_one, g_Capture_ttf_edge_two };
        const FontImageCaptureGeometry otf[] = { g_Capture_otf_edge_half, g_Capture_otf_edge_one, g_Capture_otf_edge_two };
        const float capture_scale = c.m_EdgeScale * c.m_Size / 32.0f;
        uint32_t index = capture_scale < 1.0f ? 0 : (capture_scale == 1.0f ? 1 : 2);
        geometry = strncmp(c.m_Source, "otf", 3) == 0 ? otf[index] : ttf[index];
    }
    settings.m_Width = geometry.m_LayoutWidth;
    settings.m_Leading = 1;
    settings.m_LineBreak = paragraph;
    settings.m_UseBaseStyle = 1;
    settings.m_BaseStyle = strstr(c.m_Name, "named_style") ? dmHashString64("notice") : default_style;
    dmArray<uint32_t> codepoints;
    HTextLayout       layout = 0;
    if (c.m_Markup)
    {
        HMarkup markup = 0;
        ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(c.m_Text, strlen(c.m_Text), &markup, 0));
        ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(collection, markup, &settings, &layout));
        MarkupDestroy(markup);
    }
    else
    {
        TextToCodePoints(c.m_Text, codepoints);
        ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreate(collection, codepoints.Begin(), codepoints.Size(), &settings, &layout));
    }
    ASSERT_GT(TextLayoutGetGlyphCount(layout), 0u);
    if (paragraph)
        ASSERT_GT(TextLayoutGetLineCount(layout), 1u);
    if (arabic)
    {
        bool rtl = false;
        bool shaped = false;
        for (uint32_t i = 0; i < TextLayoutGetParagraphCount(layout); ++i)
            rtl |= TextLayoutGetParagraphs(layout)[i].m_Direction == TEXT_DIRECTION_RTL;
        for (uint32_t i = 0; i < TextLayoutGetGlyphCount(layout); ++i)
        {
            const TextGlyph& glyph = TextLayoutGetGlyphs(layout)[i];
            if (glyph.m_Codepoint >= 0x600 && glyph.m_Codepoint <= 0x6ff)
            {
                ASSERT_EQ(source, glyph.m_Font);
                shaped |= glyph.m_GlyphIndex != FontGetGlyphIndex(source, glyph.m_Codepoint);
            }
        }
        ASSERT_TRUE(rtl);
        ASSERT_TRUE(shaped);
    }
    TextGlyph*          text_glyphs = TextLayoutGetGlyphs(layout);
    dmArray<ImageGlyph> glyphs;
    glyphs.SetCapacity(TextLayoutGetGlyphCount(layout));
    uint32_t atlas_width = 0, atlas_height = 0;
    int32_t  max_ascent = 0;
    for (uint32_t i = 0; i < TextLayoutGetGlyphCount(layout); ++i)
    {
        TextGlyph&            text = text_glyphs[i];
        FontLayoutCachedGlyph cached;
        if (ResolveImageGlyph(&glyphs, text, &cached) || dmUtf8::IsWhiteSpace(text.m_Codepoint))
            continue;
        ImageGlyph image = {};
        image.m_Font = text.m_Font;
        image.m_Index = text.m_GlyphIndex;
        image.m_X = atlas_width;
        if (bank_source)
        {
            FontGlyphOptions options;
            options.m_GenerateImage = true;
            ASSERT_EQ(FONT_RESULT_OK, FontGetGlyphByIndex(font, text.m_GlyphIndex, &options, &image.m_Glyph));
        }
        else
        {
            params.m_Scale = FontGetScaleFromSize(text.m_Font, c.m_Size);
            ASSERT_EQ(FONT_RESULT_OK, FontGenerateGlyph(text.m_Font, text.m_GlyphIndex, &params, &image.m_Glyph));
        }
        ASSERT_GT(image.m_Glyph.m_Bitmap.m_Width, 0);
        atlas_width += image.m_Glyph.m_Bitmap.m_Width + 2;
        max_ascent = dmMath::Max(max_ascent, (int32_t)image.m_Glyph.m_Ascent);
        glyphs.Push(image);
    }
    for (uint32_t i = 0; i < glyphs.Size(); ++i)
        atlas_height = dmMath::Max(atlas_height, (uint32_t)(max_ascent - glyphs[i].m_Glyph.m_Ascent + glyphs[i].m_Glyph.m_Bitmap.m_Height + 2));
    dmArray<uint8_t> atlas;
    atlas.SetCapacity(atlas_width * atlas_height * 4);
    atlas.SetSize(atlas.Capacity());
    memset(atlas.Begin(), 0, atlas.Size());
    for (uint32_t i = 0; i < glyphs.Size(); ++i)
    {
        ImageGlyph& image = glyphs[i];
        FontGlyph&  glyph = image.m_Glyph;
        for (uint32_t y = 0; y < glyph.m_Bitmap.m_Height; ++y)
            for (uint32_t x = 0; x < glyph.m_Bitmap.m_Width; ++x)
            {
                uint32_t dst = (((uint32_t)(1 + max_ascent - glyph.m_Ascent) + y) * atlas_width + image.m_X + 1 + x) * 4;
                uint32_t src = (y * glyph.m_Bitmap.m_Width + x) * glyph.m_Bitmap.m_Channels;
                for (uint32_t channel = 0; channel < 4; ++channel)
                    atlas[dst + channel] = channel == 3 && !fnt ? 255 : glyph.m_Bitmap.m_Data[src + (glyph.m_Bitmap.m_Channels == 1 ? 0 : channel)];
            }
    }
    FontLayoutVertexConfig config = {};
    config.m_Layout = layout;
    config.m_ResolveGlyph = ResolveImageGlyph;
    config.m_ResolveGlyphContext = &glyphs;
    config.m_Transform = dmVMath::Matrix4::identity();
    config.m_RecipAtlasWidth = 1.0f / atlas_width;
    config.m_RecipAtlasHeight = 1.0f / atlas_height;
    config.m_CacheCellMaxAscent = max_ascent;
    config.m_CacheCellPadding = 1;
    config.m_MetricsFromTtf = !bank_source;
    config.m_IsSdf = !bitmap;
    config.m_IsBMFont = fnt;
    config.m_ResolveGlyphsForMetrics = true;
    config.m_BaseLayerMask = c.m_Multi && !fnt ? 7 : 1;
    config.m_FaceColor[0] = config.m_FaceColor[1] = config.m_FaceColor[2] = 1;
    config.m_FaceColor[3] = c.m_FaceAlpha;
    // Effect alpha comes from the font style above. Component tint stays opaque
    // so partial font alpha is applied once, as in the stable label fixture.
    config.m_OutlineColor = dmVMath::Vector4(0, 0, 1, fnt ? 0 : 1);
    config.m_ShadowColor = dmVMath::Vector4(0, 1, 0, fnt ? 0 : 1);
    config.m_OutlineWidth = c.m_Outline;
    config.m_SdfSpread = sdf_spread;
    config.m_SdfEdge = .75f;
    config.m_SdfOutline = .75f - FONT_SDF_DISTANCE_SCALE * c.m_Outline / sdf_spread;
    config.m_SdfShadow = c.m_ShadowBlur > 0 ? .75f - FONT_SDF_DISTANCE_SCALE * c.m_ShadowBlur / sdf_spread : 1;
    config.m_SdfSmoothing = FONT_SDF_DISTANCE_SCALE / sdf_spread;
    if (c.m_EdgeScale > 0.0f)
    {
        // Sample the screen-space edge at 8x resolution. Only the geometry is
        // magnified; the smoothing still belongs to the requested screen scale.
        // This makes a subpixel transition measurable without resizing a PNG.
        config.m_Transform = dmVMath::Matrix4::scale(dmVMath::Vector3(8.0f * c.m_EdgeScale));
        config.m_SdfSmoothing /= c.m_EdgeScale;
    }
    config.m_ShadowX = c.m_ShadowX;
    config.m_ShadowY = c.m_ShadowY;
    config.m_ShadowBlur = c.m_ShadowBlur;
    config.m_BaseShadowAlpha = c.m_ShadowAlpha;
    config.m_Width = settings.m_Width;
    FontLayoutVertexMetrics metrics;
    ASSERT_TRUE(FontGetLayoutVertexMetrics(config, &metrics));
    ASSERT_GT(metrics.m_VertexCount, 0u);
    dmArray<FontGlyphVertex> vertices;
    vertices.SetCapacity(metrics.m_VertexCount);
    vertices.SetSize(metrics.m_VertexCount);
    ASSERT_EQ(metrics.m_VertexCount, FontCreateLayoutVertices(config, metrics, vertices.Begin(), vertices.Size()));
    bool outline_data = false;
    for (uint32_t i = 0; i < vertices.Size(); ++i)
        outline_data |= vertices[i].m_OutlineColor[3] != 0 && vertices[i].m_LayerMasks[1] != 0;
    if (!fnt && !c.m_Markup && c.m_OutlineAlpha > 0 && c.m_Outline > 0)
        ASSERT_TRUE(outline_data);
    // Inspect output alpha as well as screenshots: a duplicated 0.5 multiplier
    // produces 0.25 and must fail independently of reference image availability.
    if (!fnt && !c.m_Markup)
    {
        uint8_t outline_alpha = 0;
        uint8_t shadow_alpha = 0;
        for (uint32_t i = 0; i < vertices.Size(); ++i)
        {
            if (vertices[i].m_LayerMasks[1] != 0)
                outline_alpha = dmMath::Max(outline_alpha, vertices[i].m_OutlineColor[3]);
            if (vertices[i].m_LayerMasks[2] != 0)
                shadow_alpha = dmMath::Max(shadow_alpha, vertices[i].m_ShadowColor[3]);
        }
        if (c.m_Outline > 0)
            ASSERT_NEAR(c.m_OutlineAlpha * 255, outline_alpha, 1);
        ASSERT_NEAR(c.m_ShadowAlpha * 255, shadow_alpha, 1);
    }
    // Fixed origin and dimensions preserve the same sampling phase as the
    // stable reference fixture. Never derive the crop from rendered pixels.
    uint32_t width = geometry.m_Width;
    uint32_t height = geometry.m_Height;
    if (strcmp(c.m_Name, "manual") == 0)
    {
        // Manual inputs can exceed the frozen matrix's effect margins.
        geometry.m_OriginX = dmMath::Max(geometry.m_OriginX, (int32_t)ceilf(c.m_Outline + c.m_ShadowBlur + fabsf(c.m_ShadowX) + 4));
        geometry.m_OriginTop = dmMath::Max(geometry.m_OriginTop, (int32_t)ceilf(c.m_Outline + c.m_ShadowBlur + fabsf(c.m_ShadowY) + 4));
        float layout_width, layout_height;
        TextLayoutGetBounds(layout, &layout_width, &layout_height);
        width = dmMath::Max(width, (uint32_t)ceilf(layout_width + 2 * geometry.m_OriginX + c.m_Outline + fabsf(c.m_ShadowX)));
        height = dmMath::Max(height, (uint32_t)ceilf(layout_height + 2 * geometry.m_OriginTop + c.m_Outline + fabsf(c.m_ShadowY)));
    }
    ASSERT_LE(width, 4096u);
    ASSERT_LE(height, 4096u);
    for (uint32_t i = 0; i < vertices.Size(); ++i)
    {
        vertices[i].m_Position[0] += geometry.m_OriginX;
        vertices[i].m_Position[1] += (float)height - geometry.m_OriginTop;
    }
    dmGraphics::TextureCreationParams creation;
    creation.m_Width = atlas_width;
    creation.m_Height = atlas_height;
    dmGraphics::HTexture      texture = dmGraphics::NewTexture(g_ImageContext, creation);
    dmGraphics::TextureParams tex;
    tex.m_Width = atlas_width;
    tex.m_Height = atlas_height;
    tex.m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
    tex.m_Data = atlas.Begin();
    tex.m_DataSize = atlas.Size();
    tex.m_MinFilter = tex.m_MagFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
    dmGraphics::SetTexture(g_ImageContext, texture, tex);
    dmGraphics::RenderTargetCreationParams rt;
    rt.m_ColorBufferCreationParams[0].m_Width = width;
    rt.m_ColorBufferCreationParams[0].m_Height = height;
    rt.m_ColorBufferParams[0].m_Width = width;
    rt.m_ColorBufferParams[0].m_Height = height;
    rt.m_ColorBufferParams[0].m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
    rt.m_SampleCount = 1;
    dmGraphics::HRenderTarget target = dmGraphics::NewRenderTarget(g_ImageContext, dmGraphics::BUFFER_TYPE_COLOR0_BIT, rt);
    dmGraphics::HVertexBuffer buffer = dmGraphics::NewVertexBuffer(g_ImageContext, vertices.Size() * sizeof(FontGlyphVertex), vertices.Begin(), dmGraphics::BUFFER_USAGE_STATIC_DRAW);
    dmGraphics::HProgram      program = g_ImagePrograms[fnt ? 2 : bitmap ? 1 :
                                                                           0];
    dmArray<uint8_t> previous_pixels;
    if (c.m_Change)
    {
        const uint64_t expected_vertices = dmHashBuffer64(vertices.Begin(), vertices.Size() * sizeof(FontGlyphVertex));
        TextRenderStyle previous_style = base;
        previous_style.m_OutlineAlpha = 0.5f;
        FontCollectionSetNamedStyle(collection, default_style, previous_style, 0, 0);
        TextLayoutUpdate(layout, 0.0f);

        // Text is immutable in the font library: replace the layout while retaining
        // cached glyphs and GPU resources. These letters are already in the atlas.
        TextToCodePoints("ABCDEFG", codepoints);
        HTextLayout previous_layout = 0;
        ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreate(collection, codepoints.Begin(), codepoints.Size(), &settings, &previous_layout));
        FontLayoutVertexConfig previous_config = config;
        previous_config.m_Layout = previous_layout;
        FontLayoutVertexMetrics previous_metrics;
        ASSERT_TRUE(FontGetLayoutVertexMetrics(previous_config, &previous_metrics));
        ASSERT_LT(previous_metrics.m_VertexCount, metrics.m_VertexCount);
        dmArray<FontGlyphVertex> previous_vertices;
        previous_vertices.SetCapacity(previous_metrics.m_VertexCount);
        previous_vertices.SetSize(previous_metrics.m_VertexCount);
        ASSERT_EQ(previous_metrics.m_VertexCount, FontCreateLayoutVertices(previous_config, previous_metrics, previous_vertices.Begin(), previous_vertices.Size()));
        bool previous_outline = false;
        for (uint32_t i = 0; i < previous_vertices.Size(); ++i)
        {
            previous_vertices[i].m_Position[0] += geometry.m_OriginX;
            previous_vertices[i].m_Position[1] += (float)height - geometry.m_OriginTop;
            previous_outline |= previous_vertices[i].m_OutlineColor[3] == 127;
        }
        ASSERT_TRUE(previous_outline);
        dmGraphics::SetVertexBufferData(buffer, previous_vertices.Size() * sizeof(FontGlyphVertex), previous_vertices.Begin(), dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        CaptureFontImage(target, texture, buffer, program, previous_vertices.Size(), width, height, previous_pixels);
        bool previous_visible = false;
        for (uint32_t i = 0; i < previous_pixels.Size(); i += 4)
            previous_visible |= previous_pixels[i] || previous_pixels[i + 1] || previous_pixels[i + 2];
        ASSERT_TRUE(previous_visible);
        TextLayoutRelease(previous_layout);

        // Refresh the retained final layout after changing its named style back.
        // Its regenerated vertices must match a fresh layout, not retain half alpha.
        FontCollectionSetNamedStyle(collection, default_style, base, 0, 0);
        TextLayoutUpdate(layout, 0.0f);
        ASSERT_TRUE(FontGetLayoutVertexMetrics(config, &metrics));
        ASSERT_EQ(vertices.Size(), metrics.m_VertexCount);
        ASSERT_EQ(metrics.m_VertexCount, FontCreateLayoutVertices(config, metrics, vertices.Begin(), vertices.Size()));
        for (uint32_t i = 0; i < vertices.Size(); ++i)
        {
            vertices[i].m_Position[0] += geometry.m_OriginX;
            vertices[i].m_Position[1] += (float)height - geometry.m_OriginTop;
        }
        ASSERT_EQ(expected_vertices, dmHashBuffer64(vertices.Begin(), vertices.Size() * sizeof(FontGlyphVertex)));
        dmGraphics::SetVertexBufferData(buffer, vertices.Size() * sizeof(FontGlyphVertex), vertices.Begin(), dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
    }
    dmArray<uint8_t> pixels;
    CaptureFontImage(target, texture, buffer, program, vertices.Size(), width, height, pixels);
    if (c.m_Change)
    {
        ASSERT_EQ(previous_pixels.Size(), pixels.Size());
        ASSERT_NE(0, memcmp(previous_pixels.Begin(), pixels.Begin(), pixels.Size()));
    }
    const char* root = g_TestImageDirectory ? g_TestImageDirectory : "build/font-test-images";
#if defined(FONT_USE_SKRIBIDI)
    const char* layout_name = "full";
#else
    const char* layout_name = "legacy";
#endif
#if defined(FONT_IMAGE_RICH_NULL)
    const char* rich_name = "plain";
#else
    const char* rich_name = "rich";
#endif
    char directory[1024];
    dmSnPrintf(directory, sizeof(directory), "%s/%s-%s", root, layout_name, rich_name);
    ASSERT_TRUE(MakeImageDirectory(directory));
    // dmGraphics OpenGL readback already flips rows and returns BGRA, as in
    // fontviewer's ConvertBgraToRgbaAndUpdateBounds. Do not flip it twice.
    uint32_t left = width, top = height, right = 0, bottom = 0;
    for (uint32_t y = 0; y < height; ++y)
        for (uint32_t x = 0; x < width; ++x)
        {
            uint8_t* pixel = pixels.Begin() + (y * width + x) * 4;
            uint8_t  blue = pixel[0];
            pixel[0] = pixel[2];
            pixel[2] = blue;
            if (pixel[0] || pixel[1] || pixel[2])
            {
                left = dmMath::Min(left, x);
                top = dmMath::Min(top, y);
                right = dmMath::Max(right, x);
                bottom = dmMath::Max(bottom, y);
            }
        }
    ASSERT_LE(left, right);
    ASSERT_LE(top, bottom);
    // Detect capture clipping independently of the likeness comparison.
    ASSERT_GT(left, 0u);
    ASSERT_GT(top, 0u);
    ASSERT_LT(right, width - 1);
    ASSERT_LT(bottom, height - 1);
    char filename[1200];
    dmSnPrintf(filename, sizeof(filename), "%s/%s.png", directory, c.m_Name);
    ASSERT_NE(0, stbi_write_png(filename, width, height, 4, pixels.Begin(), width * 4));
    dmSnPrintf(filename, sizeof(filename), "%s/%s.json", directory, c.m_Name);
    FILE* data = fopen(filename, "wb");
    ASSERT_NE((FILE*)0, data);
    fprintf(data, "{\"backend\":\"opengl\",\"source\":\"render-target\",\"glyphs\":%u,\"lines\":%u,\"vertices\":%u,\"layers\":%u,\"width\":%u,\"height\":%u,\"origin_x\":%d,\"origin_top\":%d,\"layout_width\":%u,\"font_ascent\":%.9g,\"font_descent\":%.9g,\"outline_data\":%s,", TextLayoutGetGlyphCount(layout), TextLayoutGetLineCount(layout), metrics.m_VertexCount, metrics.m_LayerCount, width, height, geometry.m_OriginX, geometry.m_OriginTop, geometry.m_LayoutWidth, FontGetAscent(font, FontGetScaleFromSize(font, c.m_Size)), FontGetDescent(font, FontGetScaleFromSize(font, c.m_Size)), outline_data ? "true" : "false");
    // Fingerprint the exact CPU data sent to GL to distinguish glyph-generation
    // differences from backend sampling differences across CI hosts.
    fprintf(data, "\"atlas_width\":%u,\"atlas_height\":%u,\"atlas_hash\":\"%016llx\",\"vertex_hash\":\"%016llx\"}\n",
            atlas_width, atlas_height,
            (unsigned long long)dmHashBuffer64(atlas.Begin(), atlas.Size()),
            (unsigned long long)dmHashBuffer64(vertices.Begin(), vertices.Size() * sizeof(FontGlyphVertex)));
    ASSERT_EQ(0, fclose(data));
    ++g_TestImagesWritten;

    // Reuse the half-alpha fixtures for component alpha above one. Runtime
    // queuing is covered in test_render.cpp; these comparisons check the final
    // style multiplication and rendering with both bitmap and SDF shaders.
    if (strstr(c.m_Name, "outline_half") || strstr(c.m_Name, "shadow_half"))
    {
        const bool shadow = strstr(c.m_Name, "shadow_half") != 0;
        // With font alpha 0.5, all of these component alphas must be opaque.
        for (uint32_t node_alpha = 2; node_alpha <= 4; ++node_alpha)
        {
            FontLayoutVertexConfig alpha_config = config;
            (shadow ? alpha_config.m_ShadowColor : alpha_config.m_OutlineColor).setW((float)node_alpha);
            dmArray<FontGlyphVertex> actual_vertices, expected_vertices;
            actual_vertices.SetCapacity(vertices.Size());
            actual_vertices.SetSize(vertices.Size());
            expected_vertices.SetCapacity(vertices.Size());
            expected_vertices.PushArray(vertices.Begin(), vertices.Size());
            ASSERT_EQ(vertices.Size(), FontCreateLayoutVertices(alpha_config, metrics, actual_vertices.Begin(), actual_vertices.Size()));
            for (uint32_t i = 0; i < vertices.Size(); ++i)
            {
                actual_vertices[i].m_Position[0] += geometry.m_OriginX;
                actual_vertices[i].m_Position[1] += (float)height - geometry.m_OriginTop;
                (shadow ? expected_vertices[i].m_ShadowColor : expected_vertices[i].m_OutlineColor)[3] = 255;
            }
            dmArray<uint8_t> captures[2];
            dmGraphics::SetVertexBufferData(buffer, expected_vertices.Size() * sizeof(FontGlyphVertex), expected_vertices.Begin(), dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
            CaptureFontImage(target, texture, buffer, program, expected_vertices.Size(), width, height, captures[0]);
            dmGraphics::SetVertexBufferData(buffer, actual_vertices.Size() * sizeof(FontGlyphVertex), actual_vertices.Begin(), dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
            CaptureFontImage(target, texture, buffer, program, actual_vertices.Size(), width, height, captures[1]);
            const int difference = memcmp(captures[0].Begin(), captures[1].Begin(), captures[0].Size());
            if (difference)
                printf("%s: node alpha %u, font alpha 0.5, expected byte 255\n", c.m_Name, node_alpha);
            EXPECT_EQ(0, difference);
            for (uint32_t i = 0; i < DM_ARRAY_SIZE(captures); ++i)
            {
                for (uint32_t p = 0; p < captures[i].Size(); p += 4)
                {
                    uint8_t blue = captures[i][p];
                    captures[i][p] = captures[i][p + 2];
                    captures[i][p + 2] = blue;
                }
                dmSnPrintf(filename, sizeof(filename), "%s/%s_node_alpha_%u_%s.png", directory, c.m_Name, node_alpha, i == 0 ? "expected" : "actual");
                ASSERT_NE(0, stbi_write_png(filename, width, height, 4, captures[i].Begin(), width * 4));
            }
        }
    }
    dmGraphics::DeleteVertexBuffer(buffer);
    dmGraphics::DeleteTexture(g_ImageContext, texture);
    dmGraphics::DeleteRenderTarget(g_ImageContext, target);
    for (uint32_t i = 0; i < glyphs.Size(); ++i)
        FontFreeGlyph(glyphs[i].m_Font, &glyphs[i].m_Glyph);
    TextLayoutRelease(layout);
    FontCollectionDestroy(collection);
    if (bank_source)
    {
        FontDestroy(font);
        for (uint32_t i = 0; i < bank.m_Glyphs.Size(); ++i)
            if (fnt)
                free(bank.m_Glyphs[i].m_Bitmap.m_Data);
            else
                FontFreeGlyph(source, &bank.m_Glyphs[i]);
        if (bank.m_Atlas)
            stbi_image_free(bank.m_Atlas);
    }
    if (fallback)
        FontDestroy(fallback);
    FontDestroy(source);
}

#include "font_image_cases.inc"

static FontImageCase g_ManualCase = { "manual", "ttf_sdf", "ABCDEFGabcdefg 0123456789", 40, 4, 1, 1, 0, 0, 0, 0, false, false, false, 0 };

TEST(FontBitmapManual, Render)
{
    TestFontImage(g_ManualCase);
}

static void PrintUsage(const char* executable)
{
    printf(
    "Usage: %s [--output folder] [--case matrix-case | manual options]\n"
    "No case/options: generate the complete supported matrix.\n"
    "Manual defaults: ttf_sdf, size 40, single layer, outline 4, opaque face/outline, no shadow.\n"
    "  --source ttf_sdf|otf_sdf|ttf_bitmap|otf_bitmap|ttf_sdf_bank|otf_sdf_bank|ttf_bitmap_bank|otf_bitmap_bank|fnt\n"
    "  --layers single|multi  --text text  --markup\n"
    "  --size pixels  --outline pixels  --outline-alpha 0..1  --face-alpha 0..1\n"
    "  --shadow-alpha 0..1  --shadow-blur pixels  --shadow-x pixels  --shadow-y pixels\n"
    "Manual output: <folder>/<layout>-<rich|plain>/manual.png\n",
    executable);
}

int main(int argc, char** argv)
{
    bool        manual = false;
    const char* selected = 0;
    for (int i = 1; i < argc; ++i)
    {
        const char* option = argv[i];
        if (strcmp(option, "--help") == 0)
        {
            PrintUsage(argv[0]);
            return 0;
        }
        if (strcmp(option, "--markup") == 0)
        {
#if defined(FONT_IMAGE_RICH_NULL)
            fprintf(stderr, "This executable has no rich-text parser.\n");
            return 2;
#endif
            g_ManualCase.m_Markup = manual = true;
            continue;
        }
        if (++i == argc)
        {
            fprintf(stderr, "Missing value for %s\n", option);
            return 2;
        }
        const char* value = argv[i];
        if (strcmp(option, "--output") == 0 || strcmp(option, "--write-test-images") == 0)
        {
            if (!value[0])
                return 2;
            g_TestImageDirectory = value;
            continue;
        }
        if (strcmp(option, "--case") == 0)
        {
            selected = value;
            continue;
        }
        manual = true;
        if (strcmp(option, "--text") == 0)
        {
            g_ManualCase.m_Text = value;
            continue;
        }
        if (strcmp(option, "--source") == 0)
        {
            const char* sources[] = { "ttf_sdf", "otf_sdf", "ttf_bitmap", "otf_bitmap", "ttf_sdf_bank", "otf_sdf_bank", "ttf_bitmap_bank", "otf_bitmap_bank", "fnt" };
            bool        supported = false;
            for (uint32_t j = 0; j < sizeof(sources) / sizeof(sources[0]); ++j)
                supported |= strcmp(value, sources[j]) == 0;
            if (!supported)
            {
                fprintf(stderr, "Unsupported source: %s\n", value);
                return 2;
            }
            g_ManualCase.m_Source = value;
            continue;
        }
        if (strcmp(option, "--layers") == 0)
        {
            if (strcmp(value, "single") != 0 && strcmp(value, "multi") != 0)
                return 2;
            g_ManualCase.m_Multi = strcmp(value, "multi") == 0;
            continue;
        }
        struct NumericOption
        {
            const char* m_Name;
            float*      m_Value;
            float       m_Min;
            float       m_Max;
        };
        NumericOption options[] = {
            { "--size", &g_ManualCase.m_Size, 1, 256 },
            { "--outline", &g_ManualCase.m_Outline, 0, 64 },
            { "--outline-alpha", &g_ManualCase.m_OutlineAlpha, 0, 1 },
            { "--face-alpha", &g_ManualCase.m_FaceAlpha, 0, 1 },
            { "--shadow-alpha", &g_ManualCase.m_ShadowAlpha, 0, 1 },
            { "--shadow-blur", &g_ManualCase.m_ShadowBlur, 0, 64 },
            { "--shadow-x", &g_ManualCase.m_ShadowX, -256, 256 },
            { "--shadow-y", &g_ManualCase.m_ShadowY, -256, 256 }
        };
        bool found = false;
        for (uint32_t j = 0; j < sizeof(options) / sizeof(options[0]); ++j)
        {
            if (strcmp(option, options[j].m_Name) != 0)
                continue;
            char* end = 0;
            float number = strtof(value, &end);
            if (!value[0] || *end || !isfinite(number) || number < options[j].m_Min || number > options[j].m_Max)
            {
                fprintf(stderr, "Invalid value for %s (expected %g..%g): %s\n", option, options[j].m_Min, options[j].m_Max, value);
                return 2;
            }
            *options[j].m_Value = number;
            found = true;
            break;
        }
        if (!found)
        {
            fprintf(stderr, "Unknown option: %s\n", option);
            return 2;
        }
    }
    if (manual && selected)
    {
        fprintf(stderr, "Use either --case or manual options.\n");
        return 2;
    }
    if (selected)
    {
        bool found = false;
        for (uint32_t i = 0; i < sizeof(g_ImageCaseNames) / sizeof(g_ImageCaseNames[0]); ++i)
            found |= strcmp(selected, g_ImageCaseNames[i]) == 0;
        if (!found)
        {
            fprintf(stderr, "Unknown or unsupported case: %s\n", selected);
            return 2;
        }
    }
    char filter[512];
    dmSnPrintf(filter, sizeof(filter), "%s%s", manual ? "FontBitmapManual" : "FontImages_", selected ? selected : "");
    char*            test_argv[] = { argv[0], (char*)"--test-filter", filter, 0 };
    int              test_argc = 3;
    dmLog::LogParams params;
    dmLog::LogInitialize(&params);
    jc_test_init(&test_argc, test_argv);
    // All cases share the lazily initialized context and shader programs.
    // Finalize once, including when an individual case assertion failed.
    int result = jc_test_run_all();
    FinalizeFontImages();
    dmLog::LogFinalize();
    if (!g_TestImagesWritten)
    {
        fprintf(stderr, "No images generated; check the case name and output directory.\n");
        return 1;
    }
    return result;
}
