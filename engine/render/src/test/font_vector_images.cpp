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
#include <graphics/graphics_util.h>
#include <graphics_private.h>
#include <test/test_graphics_util.h>
#include <font/font.h>
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
    const char* path = strncmp(c.m_Source, "otf", 3) == 0 ? "src/test/data/SourceCodePro-Regular.otf" : "src/test/data/WorkSans.ttf";
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

    dmArray<char> vp, fp, slug;
    LoadVectorImageFile("content/builtins/fonts/font-vector.vp", vp);
    LoadVectorImageFile("content/builtins/fonts/font-vector.fp", fp);
    LoadVectorImageFile("content/builtins/fonts/font-vector-slug.glsl", slug);
    // Standalone GL has no shader include loader. Inline the production shader
    // and lower its samplerless integer texture to GLSL 330's integer sampler,
    // as shaderc/SPIRV-Cross does for this backend. Coverage code stays unchanged.
    char* extension = strstr(fp.Begin(), "#extension");
    ASSERT_NE((char*)0, extension);
    memset(extension, ' ', strchr(extension, '\n') - extension);
    char* texture_type = strstr(slug.Begin(), "utexture2D");
    ASSERT_NE((char*)0, texture_type);
    memcpy(texture_type, "usampler2D", 10);
    char* include = strstr(fp.Begin(), "#include");
    ASSERT_NE((char*)0, include);
    char*         after_include = strchr(include, '\n');
    dmArray<char> fragment;
    fragment.SetCapacity(fp.Size() + slug.Size());
    fragment.SetSize(fragment.Capacity());
    const uint32_t prefix = include - fp.Begin();
    memcpy(fragment.Begin(), fp.Begin(), prefix);
    dmSnPrintf(fragment.Begin() + prefix, fragment.Size() - prefix, "%s%s", slug.Begin(), after_include);
    dmGraphics::ShaderDescBuilder shaders;
    shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, vp.Begin(), vp.Size() - 1);
    shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, fragment.Begin(), strlen(fragment.Begin()));
    const char* inputs[] = { "position", "texcoord", "effect_params", "color" };
    for (uint32_t i = 0; i < 4; ++i)
        shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, inputs[i], i, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
    shaders.AddTypeMember("view_proj", dmGraphics::ShaderDesc::SHADER_TYPE_MAT4);
    shaders.AddUniform("vs_uniforms", 0, 0);
    const char* samplers[] = { "curve_texture", "band_texture", "effect_bitmap" };
    for (uint32_t i = 0; i < 3; ++i)
        shaders.AddTexture(samplers[i], i, i == 1 ? dmGraphics::ShaderDesc::SHADER_TYPE_UTEXTURE2D : dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D);
    char                 error[1024] = {};
    dmGraphics::HProgram program = resources.m_Program = dmGraphics::NewProgram(context, shaders.Get(), error, sizeof(error));
    if (!program)
        printf("Vector shader: %s\n", error);
    ASSERT_NE((dmGraphics::HProgram)0, program);

    dmRender::HRenderContext render = resources.m_Render;
    dmRender::HMaterial      material = resources.m_Material = dmRender::NewMaterial(render, program);
    for (uint32_t i = 0; i < 3; ++i)
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

    TextLayoutSettings settings = {};
    settings.m_Size = c.m_Size;
    settings.m_Width = g_Capture_vector.m_LayoutWidth;
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
    // Keep the capture cache fixed so every glyph fits in the first frame.
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
    entry.m_Transform = dmVMath::Matrix4::translation(dmVMath::Vector3(g_Capture_vector.m_OriginX, height - g_Capture_vector.m_OriginTop, 0));
    entry.m_TextLayout = layout;
    entry.m_FontSize = c.m_Size;
    entry.m_Width = settings.m_Width;
    entry.m_Leading = 1;
    entry.m_VAlign = dmRender::TEXT_VALIGN_TOP;
    entry.m_FaceColor = dmGraphics::PackRGBA(dmVMath::Vector4(1, 1, 1, c.m_FaceAlpha));
    entry.m_OutlineColor = dmGraphics::PackRGBA(dmVMath::Vector4(0, 0, 1, 1));
    entry.m_ShadowColor = dmGraphics::PackRGBA(dmVMath::Vector4(0, 1, 0, 1));
    entry.m_OutlineAlpha = 1.0f;
    entry.m_ShadowAlpha = 1.0f;
    dmArray<dmRender::FontDefaultVertex> vertices;
    vertices.SetCapacity(TextLayoutGetGlyphCount(layout) * 18);
    vertices.SetSize(vertices.Capacity());
    const uint32_t count = dmRender::CreateFontVertexData(map, 1, "", entry, 1, 1, 1, (uint8_t*)vertices.Begin(), vertices.Size());
    ASSERT_GT(count, 0u);
    ASSERT_LE(count, vertices.Size());
    bool face = false, outline = false, shadow = false;
    for (uint32_t i = 0; i < count; ++i)
    {
        face |= vertices[i].m_VectorTexcoord[2] > 0 && vertices[i].m_VectorTexcoord[3] == 0;
        outline |= vertices[i].m_VectorTexcoord[3] == 1;
        shadow |= vertices[i].m_VectorTexcoord[3] == 2;
    }
    ASSERT_TRUE(face);
    ASSERT_EQ(c.m_OutlineAlpha > 0 && c.m_Outline > 0, outline);
    ASSERT_EQ(c.m_ShadowAlpha > 0, shadow);

    dmGraphics::RenderTargetCreationParams rt;
    rt.m_ColorBufferCreationParams[0].m_Width = rt.m_ColorBufferParams[0].m_Width = width;
    rt.m_ColorBufferCreationParams[0].m_Height = rt.m_ColorBufferParams[0].m_Height = height;
    rt.m_ColorBufferParams[0].m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
    rt.m_SampleCount = 1;
    dmGraphics::HRenderTarget      target = resources.m_Target = dmGraphics::NewRenderTarget(context, dmGraphics::BUFFER_TYPE_COLOR0_BIT, rt);
    dmGraphics::HVertexBuffer      buffer = resources.m_Buffer = dmGraphics::NewVertexBuffer(context, count * sizeof(vertices[0]), vertices.Begin(), dmGraphics::BUFFER_USAGE_STATIC_DRAW);
    dmGraphics::HVertexDeclaration declaration = resources.m_Declaration = dmRender::CreateFontVertexDeclaration(context);
    dmGraphics::HTexture           textures[] = { map->m_Texture, map->m_VectorBandTexture, map->m_VectorSdfTexture ? map->m_VectorSdfTexture : map->m_Texture };
    dmGraphics::BeginFrame(context);
    dmGraphics::SetRenderTarget(context, target, dmGraphics::RenderTargetBindingParams());
    dmGraphics::SetViewport(context, 0, 0, width, height);
    dmGraphics::Clear(context, dmGraphics::BUFFER_TYPE_COLOR0_BIT, 0, 0, 0, 255, 1, 0);
    dmGraphics::EnableState(context, dmGraphics::STATE_BLEND);
    dmGraphics::SetBlendFunc(context, dmGraphics::BLEND_FACTOR_ONE, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    dmGraphics::EnableProgram(context, program);
    dmVMath::Matrix4 projection = dmVMath::Matrix4::orthographic(0, width, 0, height, -1, 1);
    dmGraphics::SetConstantM4(context, (dmVMath::Vector4*)&projection, 1, dmGraphics::FindUniformLocation(program, "view_proj"));
    for (uint32_t i = 0; i < 3; ++i)
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
    dmGraphics::DisableVertexDeclaration(context, declaration);
    dmGraphics::DisableVertexBuffer(context, buffer);
    for (uint32_t i = 0; i < 3; ++i)
        dmGraphics::DisableTexture(context, i, textures[i]);
    dmGraphics::DisableProgram(context);
    dmGraphics::SetRenderTarget(context, 0, dmGraphics::RenderTargetBindingParams());
    dmGraphics::Flip(context);
    char metadata[512];
    dmSnPrintf(metadata, sizeof(metadata), "{\"backend\":\"opengl\",\"renderer\":\"vector-slug\",\"generation\":\"%s\",\"glyphs\":%u,\"vertices\":%u,\"curve_count\":%u,\"outline_data\":%s,\"shadow_data\":%s,\"baked_size\":40,\"font_size\":%.9g}", baked ? "fontc-glyph-bank" : "runtime", TextLayoutGetGlyphCount(layout), count, map->m_SlugData->m_CurveCount, outline ? "true" : "false", shadow ? "true" : "false", c.m_Size);
    // Inspect the actual BGRA readback, independently of optional baselines.
    uint32_t outline_pixels = 0;
    uint32_t shadow_pixels = 0;
    for (uint32_t i = 0; i < pixels.Size(); i += 4)
    {
        const uint8_t* pixel = &pixels[i];
        outline_pixels += pixel[0] > pixel[1] + 16 && pixel[0] > pixel[2] + 16;
        shadow_pixels += pixel[1] > pixel[0] + 16 && pixel[1] > pixel[2] + 16;
    }
    if (outline)
        ASSERT_GT(outline_pixels, 0u);
    if (shadow)
        ASSERT_GT(shadow_pixels, 0u);
    WriteFontTestImage(c, width, height, pixels, metadata);
}

void TestFontVectorImage(const FontImageCase& c, dmGraphics::HContext context)
{
    VectorImageResources    resources = {};
    dmScript::ContextParams script_params = {};
    script_params.m_GraphicsContext = context;
    resources.m_Script = dmScript::NewContext(script_params);
    dmRender::RenderContextParams render_params;
    render_params.m_ScriptContext = resources.m_Script;
    render_params.m_MaxCharacters = 512;
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
