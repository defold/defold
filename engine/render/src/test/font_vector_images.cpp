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

#include <jc_test/jc_test.h>
#include <dlib/dstrings.h>
#include <dlib/sys.h>
#include <dlib/utf8.h>
#include <graphics/graphics_util.h>
#include <graphics_private.h>
#include <test/test_graphics_util.h>
#include <font/font.h>
#include <font_render.h>
#include <font/font_glyphbank.h>
#include <font/fontcollection.h>
#include <font/text_layout.h>
#include <script/script.h>
#include "render/font/fontmap_private.h"
#include "render/font/font_renderer_private.h"
#include "render/font/default/font_default_vertex.h"
#include "fontc.h"
#include "test_font_bitmap_gen.h"

static void LoadVectorImageFile(const char* path, dmArray<char>& data)
{
    uint32_t size = 0;
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::ResourceSize(path, &size));
    data.SetCapacity(size + 1);
    data.SetSize(size + 1);
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::LoadResource(path, data.Begin(), size, &size));
    data[size] = 0;
    const char  include[] = "#include \"/builtins/fonts/font-effect.glsl\"";
    const char* directive = strstr(data.Begin(), include);
    if (directive)
    {
        dmArray<char> effect;
        LoadVectorImageFile("content/builtins/fonts/font-effect.glsl", effect);
        const uint32_t prefix = directive - data.Begin();
        const uint32_t suffix = prefix + sizeof(include) - 1;
        dmArray<char>  expanded;
        expanded.SetCapacity(prefix + effect.Size() - 1 + data.Size() - suffix);
        expanded.SetSize(expanded.Capacity());
        memcpy(expanded.Begin(), data.Begin(), prefix);
        memcpy(expanded.Begin() + prefix, effect.Begin(), effect.Size() - 1);
        memcpy(expanded.Begin() + prefix + effect.Size() - 1, data.Begin() + suffix, data.Size() - suffix);
        data.Swap(expanded);
    }
}

struct VectorImageBank
{
    HFontRenderer               m_Compiler;
    FontGlyphBankProvider       m_Provider;
    dmArray<FontcGlyph>         m_Glyphs;
    dmArray<FontGlyphBankGlyph> m_Entries;
};

static uint32_t VectorImageCodepoint(void* context, uint32_t index)
{
    return ((VectorImageBank*)context)->m_Entries[index].m_Codepoint;
}

static bool VectorImageGlyph(void* context, uint32_t index, FontGlyphBankGlyph* glyph)
{
    *glyph = ((VectorImageBank*)context)->m_Entries[index];
    return true;
}

// Keep the compiler's exact curve/effect payload alive until the font map is
// destroyed. This tests serialized glyphs, not a second runtime curve path.
static void BakeVectorImageBank(const char* path, const FontImageCase& c, VectorImageBank& bank)
{
    dmArray<char> bytes;
    LoadVectorImageFile(path, bytes);
    FontcParams params = {};
    params.m_Size = 40;
    params.m_AtlasWidth = params.m_AtlasHeight = 512;
    params.m_SdfBasePadding = 3;
    params.m_SdfEdgeValue = 191;
    params.m_SdfSpread = 3 + c.m_Outline + ceilf(3 * c.m_ShadowBlur);
    params.m_LayerMask = 7;
    params.m_OutlineWidth = c.m_Outline;
    params.m_ShadowBlur = c.m_ShadowBlur;
    params.m_HasOutline = c.m_Outline > 0 && c.m_OutlineAlpha > 0;
    params.m_HasShadow = c.m_ShadowAlpha > 0;
    params.m_OutputBitmap = 2; // Fontc's Vector output contract.
    HFontRenderer& compiler = bank.m_Compiler;
    ASSERT_EQ(FONT_RENDERER_RESULT_OK, FontcCreate(path, (uint8_t*)bytes.Begin(), bytes.Size() - 1, &params, &compiler));
    bank.m_Glyphs.SetCapacity(95);
    bank.m_Entries.SetCapacity(95);
    for (uint32_t cp = 32; cp < 127; ++cp)
    {
        FontcGlyph glyph = {};
        ASSERT_EQ(FONT_RENDERER_RESULT_OK, FontcGenerateGlyph(compiler, cp, &glyph));
        bank.m_Glyphs.Push(glyph);
        ASSERT_GE(glyph.m_PixelCount, 20u);
        FontGlyphBankGlyph entry = {};
        entry.m_Codepoint = cp;
        entry.m_Width = glyph.m_Width;
        entry.m_Advance = glyph.m_Advance;
        entry.m_LeftBearing = glyph.m_LeftBearing;
        entry.m_Ascent = glyph.m_Ascent;
        entry.m_Descent = glyph.m_Descent;
        uint32_t vector_bytes;
        float    metrics[4];
        memcpy(&vector_bytes, glyph.m_Pixels, sizeof(vector_bytes));
        memcpy(metrics, glyph.m_Pixels + 4, sizeof(metrics));
        ASSERT_LE(20u + vector_bytes, glyph.m_PixelCount);
        entry.m_OutlineWidth = metrics[0];
        entry.m_OutlineLeftBearing = metrics[1];
        entry.m_OutlineAscent = metrics[2];
        entry.m_OutlineDescent = metrics[3];
        entry.m_VectorData = glyph.m_Pixels + 20;
        entry.m_VectorDataSize = vector_bytes;
        entry.m_Data = glyph.m_Pixels + 20 + vector_bytes;
        entry.m_DataSize = glyph.m_PixelCount - 20 - vector_bytes;
        bank.m_Entries.Push(entry);
    }
    FontcDestroy(compiler);
    compiler = 0;
}

// The outer entry point always releases these handles, including when a native
// assertion returns early, so one failed capture cannot stop the other cases.
struct VectorImageResources
{
    HFont                          m_Source;
    HFont                          m_BankFont;
    HFontCollection                m_Collection;
    HTextLayout                    m_Layout;
    dmGraphics::HProgram           m_Program;
    dmGraphics::HRenderTarget      m_Target;
    dmGraphics::HVertexBuffer      m_Buffer;
    dmGraphics::HVertexDeclaration m_Declaration;
    dmScript::HContext             m_Script;
    dmRender::HRenderContext       m_Render;
    dmRender::HMaterial            m_Material;
    dmRender::HFontMap             m_Map;
    VectorImageBank                m_Bank;
};

static void RenderFontVectorImage(const FontImageCase& c, dmGraphics::HContext context, VectorImageResources& resources)
{
    const bool  baked = strstr(c.m_Source, "_bank") != 0;
    const bool  color = strstr(c.m_Name, "_color") != 0;
    const bool  depth_overlap = strstr(c.m_Name, "_depth_overlap") != 0;
    const bool  decorations = strstr(c.m_Name, "_decorations") != 0;
    const bool  atlas_growth = strstr(c.m_Name, "_atlas_growth") != 0;
    const bool  rtl_wrapped = strstr(c.m_Name, "_rtl_wrapped") != 0;
    const char* path = strncmp(c.m_Source, "otf", 3) == 0 ? "src/test/data/SourceCodePro-Regular.otf" : "src/test/data/WorkSans.ttf";
    if (atlas_growth)
        path = "src/test/data/vector_atlas_stress.ttf";
    else if (rtl_wrapped)
        path = "src/test/data/NotoSansArabic-Regular.ttf";
    if (color)
        path = "src/test/data/NotoColorEmoji-Regular.ttf";
    HFont       source = resources.m_Source = FontLoadFromPath(path);
    ASSERT_NE((HFont)0, source);
    VectorImageBank&       bank = resources.m_Bank;
    FontGlyphBankProvider& provider = bank.m_Provider;
    const float            scale = FontGetScaleFromSize(source, 40);
    provider.m_MaxAscent = FontGetAscent(source, scale);
    provider.m_MaxDescent = FontGetDescent(source, scale);
    provider.m_ReferenceSize = 40;
    HFont font = source;
    if (baked)
    {
        BakeVectorImageBank(path, c, bank);
        ASSERT_EQ(95u, bank.m_Entries.Size());
        provider.m_Context = &bank;
        provider.m_GlyphCount = bank.m_Entries.Size();
        provider.m_GetCodepoint = VectorImageCodepoint;
        provider.m_GetGlyph = VectorImageGlyph;
        provider.m_GlyphChannels = 3;
        font = resources.m_BankFont = FontCreateGlyphBank(c.m_Source, &provider);
        ASSERT_NE((HFont)0, font);
    }

    dmArray<char> vp, fp, slug, paint;
    LoadVectorImageFile("content/builtins/fonts/font-vector.vp", vp);
    LoadVectorImageFile("content/builtins/fonts/font-vector.fp", fp);
    LoadVectorImageFile("content/builtins/fonts/font-vector-slug.glsl", slug);
    LoadVectorImageFile("content/builtins/fonts/font-vector-paint.glsl", paint);
    // Standalone GL has no shader include loader. Inline the production shader
    // and lower its samplerless integer texture to an integer sampler,
    // as shaderc/SPIRV-Cross does for this backend. Coverage code stays unchanged.
    char* extension = strstr(fp.Begin(), "#extension");
    ASSERT_NE((char*)0, extension);
    memset(extension, ' ', strchr(extension, '\n') - extension);
    char* texture_type = strstr(slug.Begin(), "utexture2D");
    ASSERT_NE((char*)0, texture_type);
    memcpy(texture_type, "usampler2D", 10);
    char* include = strstr(fp.Begin(), "#include");
    ASSERT_NE((char*)0, include);
    char*         after_include = strchr(strstr(include + 1, "#include"), '\n');
    dmArray<char> fragment;
    fragment.SetCapacity(fp.Size() + slug.Size() + paint.Size());
    fragment.SetSize(fragment.Capacity());
    const uint32_t prefix = include - fp.Begin();
    memcpy(fragment.Begin(), fp.Begin(), prefix);
    dmSnPrintf(fragment.Begin() + prefix, fragment.Size() - prefix, "%s%s%s", slug.Begin(), paint.Begin(), after_include);
    dmGraphics::ShaderDescBuilder shaders;
    shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, vp.Begin(), vp.Size() - 1);
    shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, fragment.Begin(), strlen(fragment.Begin()));
    const char* inputs[] = { "position", "texcoord", "effect_params", "color", "texcoord0", "face_color", "outline_color", "effect" };
    for (uint32_t i = 0; i < 8; ++i)
        shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, inputs[i], i, i == 4 ? dmGraphics::ShaderDesc::SHADER_TYPE_VEC2 : dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
    shaders.AddTypeMember("view_proj", dmGraphics::ShaderDesc::SHADER_TYPE_MAT4);
    shaders.AddUniform("vs_uniforms", 0, 0);
    const char* samplers[] = { "curve_texture", "band_texture", "effect_bitmap", "paint_texture", "font_effects" };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(samplers); ++i)
        shaders.AddTexture(samplers[i], i, i == 1 ? dmGraphics::ShaderDesc::SHADER_TYPE_UTEXTURE2D : dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D);
    char                 error[1024] = {};
    dmGraphics::HProgram program = resources.m_Program = dmGraphics::NewProgram(context, shaders.Get(), error, sizeof(error));
    if (!program)
        printf("Vector shader: %s\n", error);
    ASSERT_NE((dmGraphics::HProgram)0, program);

    dmRender::HRenderContext render = resources.m_Render;
    dmRender::HMaterial      material = resources.m_Material = dmRender::NewMaterial(render, program);
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(samplers); ++i)
    {
        dmGraphics::TextureFilter filter = i == 2 ? dmGraphics::TEXTURE_FILTER_LINEAR : dmGraphics::TEXTURE_FILTER_NEAREST;
        ASSERT_TRUE(dmRender::SetMaterialSampler(material, dmHashString64(samplers[i]), i, dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE, dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE, filter, filter, 1));
    }
    HFontCollection collection = resources.m_Collection = FontCollectionCreate();
    ASSERT_EQ(FONT_RESULT_OK, FontCollectionAddFont(collection, font));
    TextRenderStyle style = {};
    style.m_Flags = TEXT_RENDER_STYLE_OUTLINE_WIDTH | TEXT_RENDER_STYLE_OUTLINE_ALPHA | TEXT_RENDER_STYLE_SHADOW_ALPHA;
    style.m_OutlineWidth = c.m_Outline;
    style.m_OutlineAlpha = c.m_OutlineAlpha;
    style.m_ShadowAlpha = c.m_ShadowAlpha;
    const dmhash_t default_style = dmHashString64("default");
    FontCollectionSetNamedStyle(collection, default_style, style, 0, 0);
    dmRender::FontMapParams params;
    params.m_FontCollection = collection; // Ownership transfers to the font map.
    params.m_Size = 40;
    params.m_MaxAscent = provider.m_MaxAscent;
    params.m_MaxDescent = provider.m_MaxDescent;
    params.m_CacheWidth = params.m_CacheHeight = 1024;
    params.m_CacheMaxWidth = params.m_CacheMaxHeight = 1024;
    params.m_CacheCellWidth = params.m_CacheCellHeight = 128;
    params.m_CacheCellMaxAscent = 96;
    params.m_GlyphChannels = 3;
    params.m_VectorBitmapEffects = true;
    params.m_ImageFormat = dmRenderDDF::TYPE_DISTANCE_FIELD;
    params.m_LayerMask = FONT_RENDERER_LAYER_FACE | (c.m_OutlineAlpha > 0 && c.m_Outline > 0 ? FONT_RENDERER_LAYER_OUTLINE : 0) | (c.m_ShadowAlpha > 0 ? FONT_RENDERER_LAYER_SHADOW : 0);
    params.m_OutlineWidth = c.m_Outline;
    params.m_OutlineAlpha = c.m_OutlineAlpha;
    params.m_ShadowAlpha = c.m_ShadowAlpha;
    params.m_ShadowBlur = c.m_ShadowBlur;
    params.m_ShadowX = c.m_ShadowX;
    params.m_ShadowY = c.m_ShadowY;
    params.m_SdfSpread = 3 + c.m_Outline + ceilf(3 * c.m_ShadowBlur);
    params.m_SdfOutline = .75f - FONT_SDF_DISTANCE_SCALE * c.m_Outline / params.m_SdfSpread;
    params.m_SdfShadow = .75f;
    params.m_IsDynamic = !baked;
    dmRender::HFontMap map = resources.m_Map = dmRender::NewFontMap(render, context, params);
    resources.m_Collection = 0; // NewFontMap takes ownership, including on failure.
    ASSERT_NE((dmRender::HFontMap)0, map);
    ASSERT_TRUE(dmRender::SetFontMapMaterial(map, material));
    ASSERT_TRUE(dmRender::GetFontMapIsVector(map));

    if (color)
    {
        // Raster and curve/band-only materials select a supplied monochrome
        // emoji fallback; only materials with paint_texture select COLR.
        HFont mono = FontLoadFromPath("src/test/data/NotoEmoji-Regular.ttf");
        ASSERT_NE((HFont)0, mono);
        ASSERT_EQ(FONT_RESULT_OK, FontCollectionAddFont(collection, mono));
        dmRender::HMaterial outline_material = dmRender::NewMaterial(render, program);
        for (uint32_t i = 0; i < 3; ++i)
            ASSERT_TRUE(dmRender::SetMaterialSampler(outline_material, dmHashString64(samplers[i]), i, dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE, dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE, dmGraphics::TEXTURE_FILTER_NEAREST, dmGraphics::TEXTURE_FILTER_NEAREST, 1));
        ASSERT_EQ(dmRender::INVALID_SAMPLER_UNIT, dmRender::GetMaterialSamplerUnit(outline_material, dmHashString64("paint_texture")));
        dmRender::HMaterial materials[] = { 0, material, outline_material, material };
        for (uint32_t pass = 0; pass < DM_ARRAY_SIZE(materials); ++pass)
        {
            ASSERT_TRUE(dmRender::SetFontMapMaterial(map, materials[pass]));
            TextLayoutSettings selection = {};
            selection.m_Size = 32;
            selection.m_Leading = 1;
            uint32_t    emoji = 0x1f600;
            HTextLayout selected = 0;
            ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreate(collection, &emoji, 1, &selection, &selected));
            ASSERT_EQ(1u, TextLayoutGetGlyphCount(selected));
            ASSERT_EQ(materials[pass] == material ? font : mono, TextLayoutGetGlyphs(selected)[0].m_Font);
            TextLayoutRelease(selected);
        }
        dmRender::DeleteMaterial(render, outline_material);
        FontCollectionRemoveFont(collection, mono);
        FontDestroy(mono);
    }

    TextLayoutSettings settings = {};
    settings.m_Size = c.m_Size;
    settings.m_Width = rtl_wrapped ? 100 : g_Capture_vector.m_LayoutWidth;
    settings.m_LineBreak = rtl_wrapped;
    settings.m_Leading = 1;
    settings.m_BaseStyle = default_style;
    settings.m_UseBaseStyle = true;
    HTextLayout& layout = resources.m_Layout;
    if (c.m_Markup)
    {
        HMarkup markup = 0;
        ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(c.m_Text, strlen(c.m_Text), &markup, 0));
        ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(collection, markup, &settings, &layout));
        MarkupDestroy(markup);
    }
    else
    {
        dmArray<uint32_t> codepoints;
        TextToCodePoints(c.m_Text, codepoints);
        ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreate(collection, codepoints.Begin(), codepoints.Size(), &settings, &layout));
    }
    // Keep bitmap cache cells large enough for every captured glyph.
    // Pure curves still need cache entries even though they have no bitmap.
    const TextGlyph* glyphs = TextLayoutGetGlyphs(layout);
    for (uint32_t i = 0; i < TextLayoutGetGlyphCount(layout); ++i)
    {
        FontGlyph* glyph = 0;
        ASSERT_EQ(FONT_RESULT_OK, dmRender::GetOrCreateGlyphByIndex(map, glyphs[i].m_Font, glyphs[i].m_GlyphIndex, &glyph));
        ASSERT_LE(glyph->m_Bitmap.m_Width, params.m_CacheCellWidth);
        ASSERT_LE(glyph->m_Bitmap.m_Height, params.m_CacheCellHeight);
    }
    dmRender::UpdateCacheTexture(map);
    const uint32_t      width = g_Capture_vector.m_Width;
    const uint32_t      height = g_Capture_vector.m_Height;
    dmRender::TextEntry entry = {};
    entry.m_Transform = dmVMath::Matrix4::translation(dmVMath::Vector3(g_Capture_vector.m_OriginX, height - g_Capture_vector.m_OriginTop, depth_overlap ? -0.5f : color ? 0.25f : 0.0f));
    entry.m_TextLayout = layout;
    entry.m_Material = material;
    entry.m_FontSize = c.m_Size;
    entry.m_Width = settings.m_Width;
    entry.m_Leading = 1;
    entry.m_VAlign = dmRender::TEXT_VALIGN_TOP;
    entry.m_FaceColor = dmGraphics::PackRGBA(dmVMath::Vector4(1, 1, 1, c.m_FaceAlpha));
    if (depth_overlap)
        entry.m_FaceColor = dmGraphics::PackRGBA(dmVMath::Vector4(0, 1, 0, 1));
    entry.m_OutlineColor = dmGraphics::PackRGBA(dmVMath::Vector4(0, 0, 1, 1));
    entry.m_ShadowColor = dmGraphics::PackRGBA(dmVMath::Vector4(0, 1, 0, 1));
    entry.m_OutlineAlpha = 1.0f;
    entry.m_ShadowAlpha = 1.0f;
    dmArray<dmRender::FontDefaultVertex> vertices;
    vertices.SetCapacity(TextLayoutGetGlyphCount(layout) * 18);
    vertices.SetSize(vertices.Capacity());
    if (color)
    {
        // A color glyph fits the same single-quad budget as ordinary text.
        // Start cold: truncated glyphs must not allocate curves or paints.
        ASSERT_EQ(0u, map->m_VectorPaint->m_Glyphs.Size());
        dmRender::FontDefaultVertex bounded[7];
        memset(bounded, 0xcd, sizeof(bounded));
        ASSERT_EQ(6u, dmRender::CreateFontVertexData(map, 1, "", entry, 1, 1, 1, (uint8_t*)bounded, 6));
        ASSERT_EQ(4.0f, bounded[0].m_VectorTexcoord[3]);
        ASSERT_EQ(1u, map->m_VectorPaint->m_Glyphs.Size());
        ASSERT_NE((const FontVectorPaintGlyph*)0, dmRender::GetVectorPaintGlyph(map, glyphs[0].m_Font, glyphs[0].m_GlyphIndex));
        // Guard the next slot against writes from any of its internal layers.
        const uint8_t* guard = (const uint8_t*)&bounded[6];
        for (uint32_t i = 0; i < sizeof(bounded[6]); ++i)
            ASSERT_EQ(0xcdu, guard[i]);
    }
    uint32_t count = dmRender::CreateFontVertexData(map, 1, "", entry, 1, 1, 1, (uint8_t*)vertices.Begin(), vertices.Size());
    if (atlas_growth)
    {
        // Explicit frame boundaries let the runtime grow its numeric atlas.
        // The same visible glyph set must eventually fit instead of starving.
        for (uint32_t frame = 2; frame <= 7 && map->m_SlugResetPending; ++frame)
        {
            dmRender::PrepareVectorCache(map, frame);
            count = dmRender::CreateFontVertexData(map, frame, "", entry, 1, 1, 1, (uint8_t*)vertices.Begin(), vertices.Size());
        }
    }
    ASSERT_GT(count, 0u);
    ASSERT_LE(count, vertices.Size());
    if (depth_overlap)
    {
        // Draw green text behind identical red text, in that order. With LESS
        // depth testing, losing world Z makes the second text disappear.
        const uint32_t back_count = count;
        vertices.SetCapacity(back_count * 2);
        vertices.SetSize(vertices.Capacity());
        entry.m_Transform.setCol3(dmVMath::Vector4(g_Capture_vector.m_OriginX, height - g_Capture_vector.m_OriginTop, 0.5f, 1));
        entry.m_FaceColor = dmGraphics::PackRGBA(dmVMath::Vector4(1, 0, 0, 1));
        count += dmRender::CreateFontVertexData(map, 1, "", entry, 1, 1, 1, (uint8_t*)(vertices.Begin() + back_count), back_count);
        ASSERT_EQ(back_count * 2, count);
    }
    bool face = false, outline = false, shadow = false, decoration = false;
    for (uint32_t i = 0; i < count; ++i)
    {
        face |= vertices[i].m_VectorTexcoord[2] > 0 && (vertices[i].m_VectorTexcoord[3] == 0 || vertices[i].m_VectorTexcoord[3] == 4);
        outline |= vertices[i].m_VectorTexcoord[3] == 1;
        shadow |= vertices[i].m_VectorTexcoord[3] == 2;
        decoration |= vertices[i].m_VectorTexcoord[3] == 3;
        // Color layer addresses must not overwrite the glyph's world depth.
        if (color && vertices[i].m_VectorTexcoord[3] == 4)
            ASSERT_EQ(0.25f, vertices[i].m_Position[3]);
    }
    // Underlines and color layers have separate shader modes in the same draw.
    if (color && c.m_Markup)
        ASSERT_TRUE(decoration);
    ASSERT_TRUE(face);
    ASSERT_EQ(c.m_OutlineAlpha > 0 && c.m_Outline > 0, outline);
    ASSERT_EQ(c.m_ShadowAlpha > 0, shadow);

    dmGraphics::RenderTargetCreationParams rt;
    rt.m_ColorBufferCreationParams[0].m_Width = rt.m_ColorBufferParams[0].m_Width = width;
    rt.m_ColorBufferCreationParams[0].m_Height = rt.m_ColorBufferParams[0].m_Height = height;
    rt.m_ColorBufferParams[0].m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
    rt.m_SampleCount = 1;
    uint32_t buffer_flags = dmGraphics::BUFFER_TYPE_COLOR0_BIT;
    if (depth_overlap)
    {
        rt.m_DepthBufferCreationParams.m_Width = rt.m_DepthBufferParams.m_Width = width;
        rt.m_DepthBufferCreationParams.m_Height = rt.m_DepthBufferParams.m_Height = height;
        rt.m_DepthBufferParams.m_Format = dmGraphics::TEXTURE_FORMAT_DEPTH;
        buffer_flags |= dmGraphics::BUFFER_TYPE_DEPTH_BIT;
    }
    dmGraphics::HRenderTarget      target = resources.m_Target = dmGraphics::NewRenderTarget(context, buffer_flags, rt);
    dmGraphics::HVertexBuffer      buffer = resources.m_Buffer = dmGraphics::NewVertexBuffer(context, count * sizeof(vertices[0]), vertices.Begin(), dmGraphics::BUFFER_USAGE_STATIC_DRAW);
    dmGraphics::HVertexDeclaration declaration = resources.m_Declaration = dmRender::CreateFontVertexDeclaration(context);
    FontUploadEffects(map->m_EffectCache);
    dmGraphics::HTexture textures[] = { map->m_Texture, map->m_VectorBandTexture, map->m_VectorSdfTexture ? map->m_VectorSdfTexture : map->m_Texture, map->m_VectorPaintTexture, FontGetEffectTexture(map->m_EffectCache) };
    dmGraphics::BeginFrame(context);
    dmGraphics::SetRenderTarget(context, target, dmGraphics::RenderTargetBindingParams());
    dmGraphics::SetViewport(context, 0, 0, width, height);
    dmGraphics::SetDepthMask(context, true);
    dmGraphics::Clear(context, buffer_flags, 0, 0, 0, 255, 1, 0);
    dmGraphics::EnableState(context, dmGraphics::STATE_BLEND);
    if (depth_overlap)
    {
        dmGraphics::DisableState(context, dmGraphics::STATE_BLEND);
        dmGraphics::EnableState(context, dmGraphics::STATE_DEPTH_TEST);
        dmGraphics::SetDepthFunc(context, dmGraphics::COMPARE_FUNC_LESS);
    }
    if (decorations || color)
    {
        // Ordinary glyphs, decorations and composed color glyphs must all
        // remain visible with the same clockwise winding.
        dmGraphics::EnableState(context, dmGraphics::STATE_CULL_FACE);
        dmGraphics::SetCullFace(context, dmGraphics::FACE_TYPE_FRONT);
    }
    dmGraphics::SetBlendFunc(context, dmGraphics::BLEND_FACTOR_ONE, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    dmGraphics::EnableProgram(context, program);
    dmVMath::Matrix4 projection = dmVMath::Matrix4::orthographic(0, width, 0, height, -1, 1);
    dmGraphics::SetConstantM4(context, (dmVMath::Vector4*)&projection, 1, dmGraphics::FindUniformLocation(program, "view_proj"));
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(samplers); ++i)
    {
        dmGraphics::SetSampler(context, dmGraphics::FindUniformLocation(program, samplers[i]), i);
        dmGraphics::EnableTexture(context, i, 0, textures[i]);
    }
    dmGraphics::EnableVertexBuffer(context, buffer, 0);
    dmGraphics::EnableVertexDeclaration(context, declaration, 0, 0, program);
    dmGraphics::Draw(context, dmGraphics::PRIMITIVE_TRIANGLES, 0, count, 1);
    dmArray<uint8_t> pixels;
    pixels.SetCapacity(width * height * 4);
    pixels.SetSize(pixels.Capacity());
    dmGraphics::ReadPixels(context, 0, 0, width, height, pixels.Begin(), pixels.Size());
    if (color && !decoration)
    {
        // Test isolated glyphs: decorations can overlap them as separate draws.
        // A half-opacity glyph must be its completed opaque image faded over
        // the background. Per-layer alpha would expose the paints underneath.
        entry.m_FaceColor = dmGraphics::PackRGBA(dmVMath::Vector4(1, 1, 1, 128.0f / 255.0f));
        uint32_t faded_count = dmRender::CreateFontVertexData(map, 1, "", entry, 1, 1, 1, (uint8_t*)vertices.Begin(), vertices.Size());
        ASSERT_EQ(count, faded_count);
        dmGraphics::SetVertexBufferData(buffer, count * sizeof(vertices[0]), vertices.Begin(), dmGraphics::BUFFER_USAGE_STATIC_DRAW);
        dmGraphics::Clear(context, buffer_flags, 0, 0, 0, 255, 1, 0);
        dmGraphics::Draw(context, dmGraphics::PRIMITIVE_TRIANGLES, 0, count, 1);
        dmArray<uint8_t> faded;
        faded.SetCapacity(pixels.Size());
        faded.SetSize(pixels.Size());
        dmGraphics::ReadPixels(context, 0, 0, width, height, faded.Begin(), faded.Size());
        uint32_t max_error = 0;
        for (uint32_t i = 0; i < pixels.Size(); ++i)
            if (i % 4 != 3)
                max_error = dmMath::Max(max_error, (uint32_t)abs((int)faded[i] - (int)(pixels[i] * 128 / 255)));
        ASSERT_LE(max_error, 2u);
    }
    if (color && c.m_Markup)
    {
        // Exercise the production shader: a spatial gradient must hide the composed emoji.
        const char* types[] = { "linear", "radial" };
        for (uint32_t type = 0; type < DM_ARRAY_SIZE(types); ++type)
        {
            char text[256];
            dmSnPrintf(text, sizeof(text), "<gradient type=%s color0=#ffffff00 color1=#ffffff00>😀</gradient>", types[type]);
            HMarkup     markup = 0;
            HTextLayout transparent = 0;
            ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(text, strlen(text), &markup, 0));
            ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(collection, markup, &settings, &transparent));
            MarkupDestroy(markup);
            entry.m_TextLayout = transparent;
            entry.m_FaceColor = dmGraphics::PackRGBA(dmVMath::Vector4(1, 1, 1, 1));
            uint32_t transparent_count = dmRender::CreateFontVertexData(map, 1, "", entry, 1, 1, 1, (uint8_t*)vertices.Begin(), vertices.Size());
            ASSERT_EQ(6u, transparent_count);
            ASSERT_EQ(4.0f, vertices[0].m_VectorTexcoord[3]);
            ASSERT_GT(vertices[0].m_Effect[2], 0.0f);
            ASSERT_LT(vertices[0].m_Effect[0], vertices[2].m_Effect[0]);
            ASSERT_LT(vertices[0].m_Effect[1], vertices[1].m_Effect[1]);
            FontUploadEffects(map->m_EffectCache);
            textures[4] = FontGetEffectTexture(map->m_EffectCache);
            dmGraphics::EnableTexture(context, 4, 0, textures[4]);
            dmGraphics::SetVertexBufferData(buffer, transparent_count * sizeof(vertices[0]), vertices.Begin(), dmGraphics::BUFFER_USAGE_STATIC_DRAW);
            dmGraphics::Clear(context, buffer_flags, 0, 0, 0, 255, 1, 0);
            dmGraphics::Draw(context, dmGraphics::PRIMITIVE_TRIANGLES, 0, transparent_count, 1);
            dmArray<uint8_t> hidden;
            hidden.SetCapacity(pixels.Size());
            hidden.SetSize(pixels.Size());
            dmGraphics::ReadPixels(context, 0, 0, width, height, hidden.Begin(), hidden.Size());
            uint32_t max_error = 0;
            for (uint32_t i = 0; i < hidden.Size(); ++i)
                max_error = dmMath::Max(max_error, (uint32_t)abs((int)hidden[i] - (i % 4 == 3 ? 255 : 0)));
            EXPECT_EQ(0u, max_error);
            TextLayoutRelease(transparent);
        }
        entry.m_TextLayout = layout;
    }
    dmGraphics::DisableState(context, dmGraphics::STATE_DEPTH_TEST);
    dmGraphics::DisableState(context, dmGraphics::STATE_CULL_FACE);
    dmGraphics::DisableVertexDeclaration(context, declaration);
    dmGraphics::DisableVertexBuffer(context, buffer);
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(samplers); ++i)
        dmGraphics::DisableTexture(context, i, textures[i]);
    dmGraphics::DisableProgram(context);
    dmGraphics::SetRenderTarget(context, 0, dmGraphics::RenderTargetBindingParams());
    dmGraphics::Flip(context);
    char metadata[512];
    dmSnPrintf(metadata, sizeof(metadata), "{\"backend\":\"opengl\",\"renderer\":\"vector-slug\",\"generation\":\"%s\",\"glyphs\":%u,\"vertices\":%u,\"curve_count\":%u,\"outline_data\":%s,\"shadow_data\":%s,\"baked_size\":40,\"font_size\":%.9g}", baked ? "fontc-glyph-bank" : "runtime", TextLayoutGetGlyphCount(layout), count, map->m_SlugData->m_CurveCount, outline ? "true" : "false", shadow ? "true" : "false", c.m_Size);
    // Inspect the actual BGRA readback, independently of optional baselines.
    uint32_t outline_pixels = 0;
    uint32_t shadow_pixels = 0;
    uint32_t front_pixels = 0;
    uint32_t yellow_pixels = 0, gray_pixels = 0;
    for (uint32_t i = 0; i < pixels.Size(); i += 4)
    {
        const uint8_t* pixel = &pixels[i];
        yellow_pixels += pixel[2] > 190 && pixel[1] > 140 && pixel[0] < 90;
        gray_pixels += pixel[2] > 200 && pixel[1] > 200 && pixel[0] > 200;
        outline_pixels += pixel[0] > pixel[1] + 16 && pixel[0] > pixel[2] + 16;
        shadow_pixels += pixel[1] > pixel[0] + 16 && pixel[1] > pixel[2] + 16;
        front_pixels += pixel[2] > pixel[0] + 16 && pixel[2] > pixel[1] + 16;
    }
    if (color)
    {
        ASSERT_GT(yellow_pixels, 100u);
        if (strstr(c.m_Name, "_fallback"))
            ASSERT_GT(gray_pixels, 10u);
    }
    if (strstr(c.m_Name, "_gradient_"))
    {
        ASSERT_GT(front_pixels, 0u);
        ASSERT_GT(shadow_pixels, 0u);
        ASSERT_GT(outline_pixels, 0u);
        uint32_t yellow_pixels = 0;
        for (uint32_t i = 0; i < pixels.Size(); i += 4)
        {
            const uint8_t* pixel = &pixels[i];
            yellow_pixels += pixel[1] > 100 && pixel[2] > 100 && pixel[0] < 40;
        }
        ASSERT_GT(yellow_pixels, 0u);
    }
    if (outline)
        ASSERT_GT(outline_pixels, 0u);
    if (shadow)
        ASSERT_GT(shadow_pixels, 0u);
    uint32_t visible_decorations = 0;
    if (decorations)
    {
        const TextDecoration* lines = TextLayoutGetDecorations(layout);
        const TextLine* text_lines = TextLayoutGetLines(layout);
        float layout_width, layout_height;
        TextLayoutGetBounds(layout, &layout_width, &layout_height);
        for (uint32_t i = 0; i < TextLayoutGetDecorationCount(layout); ++i)
        {
            const TextDecoration& line = lines[i];
            uint32_t space = line.m_GlyphStart;
            while (space < line.m_GlyphStart + line.m_GlyphCount && glyphs[space].m_Codepoint != ' ')
                ++space;
            ASSERT_LT(space + 1, line.m_GlyphStart + line.m_GlyphCount);
            const float first_x = glyphs[text_lines[line.m_LineIndex].m_Index].m_X;
            const int x0 = (int)ceilf(g_Capture_vector.m_OriginX + glyphs[space].m_X - first_x + 1);
            const int x1 = (int)floorf(g_Capture_vector.m_OriginX + glyphs[space + 1].m_X - first_x - 1);
            const float y = height - g_Capture_vector.m_OriginTop - layout_height + text_lines[line.m_LineIndex].m_Baseline + line.m_Y;
            bool visible = false;
            for (int py = (int)floorf(height - y - line.m_Thickness); py <= (int)ceilf(height - y + line.m_Thickness); ++py)
                for (int px = x0; px <= x1; ++px)
                    if (px >= 0 && px < (int)width && py >= 0 && py < (int)height)
                    {
                        const uint8_t* pixel = pixels.Begin() + (py * width + px) * 4;
                        visible |= pixel[0] > 127 && pixel[1] > 127 && pixel[2] > 127;
                    }
            visible_decorations += visible;
        }
    }
    WriteFontTestImage(c, width, height, pixels, metadata);
    if (atlas_growth)
    {
        ASSERT_FALSE(map->m_SlugResetPending);
        ASSERT_EQ(64u * 6, count);
        for (uint32_t i = 0; i < count; i += 6)
        {
            ASSERT_GT(vertices[i].m_VectorTexcoord[2], 0.0f);
            const int x0 = (int)ceilf(vertices[i].m_Position[0]);
            const int x1 = (int)floorf(vertices[i + 2].m_Position[0]);
            const int y0 = (int)ceilf(height - vertices[i + 1].m_Position[1]);
            const int y1 = (int)floorf(height - vertices[i].m_Position[1]);
            ASSERT_GE(x0, 0);
            ASSERT_LT(x1, (int)width);
            ASSERT_GE(y0, 0);
            ASSERT_LT(y1, (int)height);
            bool visible = false;
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x)
                    visible |= pixels[(y * width + x) * 4] > 127;
            ASSERT_TRUE(visible);
        }
    }
    if (rtl_wrapped)
    {
        // Wrapped RTL whitespace must not move visible glyphs away from the
        // shared content origin used by decorations and hit testing.
        ASSERT_GT(TextLayoutGetLineCount(layout), 1u);
        ASSERT_EQ(TEXT_DIRECTION_RTL, TextLayoutGetParagraphs(layout)[0].m_Direction);
        const TextLine* lines = TextLayoutGetLines(layout);
        uint32_t        vertex = 0;
        bool            has_trimmed_space = false;
        for (uint32_t i = 0; i < TextLayoutGetLineCount(layout); ++i)
        {
            const TextLine& line = lines[i];
            const float     origin = TextLayoutGetLineOriginX(layout, i);
            for (uint32_t j = line.m_Index; j < line.m_Index + line.m_Length; ++j)
            {
                if (dmUtf8::IsWhiteSpace(glyphs[j].m_Codepoint))
                {
                    has_trimmed_space |= glyphs[j].m_X < origin;
                    continue;
                }
                FontGlyph* glyph = 0;
                ASSERT_EQ(FONT_RESULT_OK, dmRender::GetOrCreateGlyphByIndex(map, glyphs[j].m_Font, glyphs[j].m_GlyphIndex, &glyph));
                const float x = g_Capture_vector.m_OriginX + settings.m_Width - line.m_Width + glyphs[j].m_X - origin + glyph->m_Outline.m_LeftBearing * c.m_Size / params.m_Size;
                ASSERT_LT(vertex, count);
                ASSERT_NEAR(x, vertices[vertex].m_Position[0], 0.001f);
                vertex += 6;
            }
        }
        ASSERT_TRUE(has_trimmed_space);
        ASSERT_EQ(count, vertex);
    }
    if (strstr(c.m_Name, "_decorations_outline_"))
    {
        // The requested 8px outline is capped by the font's 2px reference outline:
        // rendering a 40px font at 20px permits 1px, and at 80px permits 4px.
        const float expected_outline = c.m_Size == 20 ? 1.0f : 4.0f;
        const TextDecoration* lines = TextLayoutGetDecorations(layout);
        uint32_t outline_count = 0;
        for (uint32_t i = 0; i < count; i += 6)
        {
            const dmRender::FontDefaultVertex& vertex = vertices[i];
            if (vertex.m_VectorTexcoord[3] != 3 || vertex.m_VectorColor[0] != 0 || vertex.m_VectorColor[2] != 255)
                continue;
            float min_y = vertex.m_Position[1];
            float max_y = min_y;
            for (uint32_t j = 1; j < 6; ++j)
            {
                min_y = dmMath::Min(min_y, vertices[i + j].m_Position[1]);
                max_y = dmMath::Max(max_y, vertices[i + j].m_Position[1]);
            }
            ASSERT_LT(outline_count, TextLayoutGetDecorationCount(layout));
            ASSERT_NEAR(lines[outline_count].m_Thickness + 2 * expected_outline, max_y - min_y, 0.0001f);
            ++outline_count;
        }
        ASSERT_EQ(2u, outline_count);
    }
    if (depth_overlap)
    {
        ASSERT_GT(front_pixels, 0u);
        ASSERT_EQ(0u, shadow_pixels);
    }
    if (decorations)
    {
        ASSERT_EQ(2u, TextLayoutGetDecorationCount(layout));
        ASSERT_EQ(2u, visible_decorations);
    }
}

// Exercise the production root selector on the GPU, including both signed zeros.
// Each pixel covers one of the 64 triples drawn from {-1, -0, +0, +1}.
static void TestVectorRootEligibility(dmGraphics::HContext context)
{
    dmArray<char> slug;
    LoadVectorImageFile("content/builtins/fonts/font-vector-slug.glsl", slug);
    char* texture_type = strstr(slug.Begin(), "utexture2D");
    ASSERT_NE((char*)0, texture_type);
    memcpy(texture_type, "usampler2D", 10);
    const char* vertex =
    "#version 140\n"
    "in vec4 position;\n"
    "void main() { gl_Position = position; }\n";
    const char* fragment_main =
    "uniform test_uniforms { vec4 samples; };\n"
    "out vec4 color;\n"
    "void main() {\n"
    "    int i = int(gl_FragCoord.x);\n"
    "    int code = CalcRootCode(samples[i % 4], samples[(i / 4) % 4], samples[i / 16]);\n"
    "    color = vec4(float(code & 1), float((code >> 8) & 1), 0.0, 1.0);\n"
    "}\n";
    dmArray<char> fragment;
    fragment.SetCapacity(slug.Size() + strlen(fragment_main) + 32);
    fragment.SetSize(fragment.Capacity());
    dmSnPrintf(fragment.Begin(), fragment.Size(), "#version 140\n%s\n%s", slug.Begin(), fragment_main);
    dmGraphics::ShaderDescBuilder shaders;
    shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, vertex, strlen(vertex));
    shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, fragment.Begin(), strlen(fragment.Begin()));
    shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "position", 0, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
    shaders.AddTypeMember("samples", dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
    shaders.AddUniform("test_uniforms", 0, 0);
    char                 error[1024] = {};
    dmGraphics::HProgram program = dmGraphics::NewProgram(context, shaders.Get(), error, sizeof(error));
    if (!program)
        printf("Vector root shader: %s\n", error);
    ASSERT_NE((dmGraphics::HProgram)0, program);

    const float                          vertices[] = { -1, -1, 0, 1, 3, -1, 0, 1, -1, 3, 0, 1 };
    dmGraphics::HVertexBuffer            buffer = dmGraphics::NewVertexBuffer(context, sizeof(vertices), vertices, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
    dmGraphics::HVertexStreamDeclaration streams = dmGraphics::NewVertexStreamDeclaration(context);
    dmGraphics::AddVertexStream(streams, "position", 4, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::HVertexDeclaration declaration = dmGraphics::NewVertexDeclaration(context, streams, 4 * sizeof(float));
    dmGraphics::DeleteVertexStreamDeclaration(streams);
    dmGraphics::RenderTargetCreationParams rt;
    rt.m_ColorBufferCreationParams[0].m_Width = rt.m_ColorBufferParams[0].m_Width = 64;
    rt.m_ColorBufferCreationParams[0].m_Height = rt.m_ColorBufferParams[0].m_Height = 1;
    rt.m_ColorBufferParams[0].m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
    rt.m_SampleCount = 1;
    dmGraphics::HRenderTarget target = dmGraphics::NewRenderTarget(context, dmGraphics::BUFFER_TYPE_COLOR0_BIT, rt);
    dmGraphics::BeginFrame(context);
    dmGraphics::SetRenderTarget(context, target, dmGraphics::RenderTargetBindingParams());
    dmGraphics::SetViewport(context, 0, 0, 64, 1);
    dmGraphics::DisableState(context, dmGraphics::STATE_BLEND);
    dmGraphics::DisableState(context, dmGraphics::STATE_DEPTH_TEST);
    dmGraphics::DisableState(context, dmGraphics::STATE_CULL_FACE);
    dmGraphics::Clear(context, dmGraphics::BUFFER_TYPE_COLOR0_BIT, 0, 0, 0, 0, 1, 0);
    dmGraphics::EnableProgram(context, program);
    const dmVMath::Vector4 samples(-1.0f, -0.0f, 0.0f, 1.0f);
    dmGraphics::SetConstantV4(context, &samples, 1, dmGraphics::FindUniformLocation(program, "samples"));
    dmGraphics::EnableVertexBuffer(context, buffer, 0);
    dmGraphics::EnableVertexDeclaration(context, declaration, 0, 0, program);
    dmGraphics::Draw(context, dmGraphics::PRIMITIVE_TRIANGLES, 0, 3, 1);
    uint8_t pixels[64 * 4];
    dmGraphics::ReadPixels(context, 0, 0, 64, 1, pixels, sizeof(pixels));
    dmGraphics::DisableVertexDeclaration(context, declaration);
    dmGraphics::DisableVertexBuffer(context, buffer);
    dmGraphics::DisableProgram(context);
    dmGraphics::SetRenderTarget(context, 0, dmGraphics::RenderTargetBindingParams());
    dmGraphics::Flip(context);
    dmGraphics::DeleteRenderTarget(context, target);
    dmGraphics::DeleteVertexDeclaration(declaration);
    dmGraphics::DeleteVertexBuffer(buffer);
    dmGraphics::DeleteProgram(context, program);

    // Positive coordinates and either zero use the same half-open boundary.
    // These are the eligible roots for the eight combinations of negative signs.
    const uint16_t expected_codes[] = { 0, 256, 257, 256, 1, 257, 1, 0 };
    for (uint32_t i = 0; i < 64; ++i)
    {
        uint32_t signs = (i % 4 == 0 ? 1 : 0) | ((i / 4) % 4 == 0 ? 2 : 0) | (i / 16 == 0 ? 4 : 0);
        uint16_t expected = expected_codes[signs];
        // OpenGL readback is BGRA, as in the font likeness captures.
        ASSERT_EQ((expected & 1) ? 255 : 0, pixels[i * 4 + 2]);
        ASSERT_EQ((expected & 256) ? 255 : 0, pixels[i * 4 + 1]);
        ASSERT_EQ(255, pixels[i * 4 + 3]);
    }
}

void TestFontVectorImage(const FontImageCase& c, dmGraphics::HContext context)
{
    if (strcmp(c.m_Name, "ttf_vector_multi_face_only") == 0)
        TestVectorRootEligibility(context);
    VectorImageResources    resources = {};
    dmScript::ContextParams script_params = {};
    script_params.m_GraphicsContext = context;
    resources.m_Script = dmScript::NewContext(script_params);
    dmRender::RenderContextParams render_params;
    render_params.m_ScriptContext = resources.m_Script;
    render_params.m_MaxCharacters = 512;
    render_params.m_MaxBatches = 8;
    resources.m_Render = dmRender::NewRenderContext(context, render_params);
    RenderFontVectorImage(c, context, resources);
    if (resources.m_Declaration)
        dmGraphics::DeleteVertexDeclaration(resources.m_Declaration);
    if (resources.m_Buffer)
        dmGraphics::DeleteVertexBuffer(resources.m_Buffer);
    if (resources.m_Target)
        dmGraphics::DeleteRenderTarget(context, resources.m_Target);
    if (resources.m_Layout)
        TextLayoutRelease(resources.m_Layout);
    if (resources.m_Map)
        dmRender::DeleteFontMap(resources.m_Map);
    if (resources.m_Collection)
        FontCollectionDestroy(resources.m_Collection);
    if (resources.m_Material)
        dmRender::DeleteMaterial(resources.m_Render, resources.m_Material);
    if (resources.m_Program)
        dmGraphics::DeleteProgram(context, resources.m_Program);
    dmRender::DeleteRenderContext(resources.m_Render, 0);
    dmScript::DeleteContext(resources.m_Script);
    if (resources.m_BankFont)
        FontDestroy(resources.m_BankFont);
    for (uint32_t i = 0; i < resources.m_Bank.m_Glyphs.Size(); ++i)
        FontcFreeGlyph(&resources.m_Bank.m_Glyphs[i]);
    if (resources.m_Bank.m_Compiler)
        FontcDestroy(resources.m_Bank.m_Compiler);
    if (resources.m_Source)
        FontDestroy(resources.m_Source);
}
