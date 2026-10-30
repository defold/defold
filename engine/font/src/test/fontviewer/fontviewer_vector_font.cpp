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

#include "fontviewer_vector_font.h"
#include <float.h>
#include <math.h>
#include <string.h>
#include <dlib/array.h>
#include <dlib/log.h>
#include <graphics/graphics.h>
#include <graphics_private.h>
#include <test/test_graphics_util.h>
#include <font_ttf.h>
#include <font_vector_paint.hpp>
#include <font_vector_slug.h>
#include "fontviewer_vector_font.vp.embed.h"
#include "fontviewer_vector_font.fp.embed.h"
#include "font-vector-slug.glsl.embed.h"
#include "font-vector-paint.glsl.embed.h"

using dmVMath::Matrix4;
using dmVMath::Vector4;

struct FontViewerVectorVertex
{
    float                 m_Effect[4];
    FontVectorPaintVertex m_Glyph;
};

struct FontViewerVectorFont
{
    FontVectorSlugData             m_Slug;
    FontVectorPaintCache           m_Paint;
    dmArray<FontViewerVectorVertex> m_Vertices;
    dmGraphics::HContext           m_Context;
    dmGraphics::HProgram           m_Program;
    dmGraphics::HTexture           m_Textures[3];
    dmGraphics::HVertexBuffer      m_VertexBuffer;
    dmGraphics::HVertexDeclaration m_Declaration;
    bool                           m_Dirty;
};

FontViewerVectorFont* FontViewerVectorFontCreate()
{
    FontViewerVectorFont* cache = new FontViewerVectorFont;
    cache->m_Paint.m_Slug = &cache->m_Slug;
    cache->m_Context = 0;
    cache->m_Program = 0;
    cache->m_VertexBuffer = 0;
    cache->m_Declaration = 0;
    memset(cache->m_Textures, 0, sizeof(cache->m_Textures));
    cache->m_Dirty = true;
    return cache;
}

void FontViewerVectorFontClear(FontViewerVectorFont* cache)
{
    if (!cache)
        return;
    for (uint32_t i = 0; i < 3; ++i)
    {
        if (cache->m_Textures[i])
            dmGraphics::DeleteTexture(cache->m_Context, cache->m_Textures[i]);
        cache->m_Textures[i] = 0;
    }
    if (cache->m_Program)
        dmGraphics::DeleteProgram(cache->m_Context, cache->m_Program);
    if (cache->m_VertexBuffer)
        dmGraphics::DeleteVertexBuffer(cache->m_VertexBuffer);
    if (cache->m_Declaration)
        dmGraphics::DeleteVertexDeclaration(cache->m_Declaration);
    cache->m_Program = 0;
    cache->m_VertexBuffer = 0;
    cache->m_Declaration = 0;
    FontVectorPaintClear(&cache->m_Paint);
    cache->m_Vertices.SetSize(0);
    FontVectorSlugBegin(&cache->m_Slug);
    cache->m_Dirty = true;
}

void FontViewerVectorFontDestroy(FontViewerVectorFont* cache)
{
    if (!cache)
        return;
    FontViewerVectorFontClear(cache);
    delete cache;
}

bool FontViewerVectorFontAddGlyph(FontViewerVectorFont* cache, HFont source, uint32_t glyph_index, uint32_t* index, bool* has_color)
{
    uint32_t count = cache->m_Paint.m_Glyphs.Size();
    bool     result = FontVectorPaintAddGlyph(&cache->m_Paint, source, glyph_index, index, has_color);
    cache->m_Dirty |= count != cache->m_Paint.m_Glyphs.Size();
    return result;
}

void FontViewerVectorFontBegin(FontViewerVectorFont* cache)
{
    cache->m_Vertices.SetSize(0);
}

void FontViewerVectorFontPack(FontViewerVectorFont* cache, uint32_t index, float scale, float pixel_scale, float x, float y, const Matrix4& transform, const Vector4& clip, const FontLayoutVertexConfig& config, const TextLine& line, const TextGlyph& text_glyph, const TextGlyphRenderData& render_data, FontSpatialGradientCache* spatial_cache)
{
    const FontVectorPaintGlyph& glyph = cache->m_Paint.m_Glyphs[index];
    uint32_t                    first = cache->m_Vertices.Size();
    uint32_t                    end = first + 6;
    if (cache->m_Vertices.Capacity() < end)
        cache->m_Vertices.SetCapacity(end + 192);
    cache->m_Vertices.SetSize(end);
    FontVectorPaintVertex packed[6];
    FontVectorPaintPackGlyph(glyph, scale, pixel_scale, x + render_data.m_OffsetX, y + render_data.m_OffsetY, transform, clip, render_data.m_FaceColors, packed);
    FontGlyphVertex gradient_vertices[6] = {};
    if (!config.m_Layout->m_Effects.Empty())
    {
        // Slug's clockwise triangles use the opposite winding to the shared packer.
        static const uint32_t shared[] = { 0, 2, 1, 1, 2, 5 };
        for (uint32_t i = 0; i < 6; ++i)
        {
            const uint32_t color = FontPackColor(packed[shared[i]].m_Color);
            memcpy(gradient_vertices[i].m_FaceColor, &color, sizeof(color));
        }
        const TextGradientBounds bounds = { x + glyph.m_Bounds.getX() * scale, y + glyph.m_Bounds.getY() * scale, x + glyph.m_Bounds.getZ() * scale, y + glyph.m_Bounds.getW() * scale };
        const TextGradientBounds quad = { x + packed[0].m_FontPosition[0] * scale, y + packed[0].m_FontPosition[1] * scale, x + packed[5].m_FontPosition[0] * scale, y + packed[5].m_FontPosition[1] * scale };
        FontVertexLayerParams    layers = {};
        layers.m_FaceVertices = gradient_vertices;
        FontApplySpatialGradient(config, line, text_glyph, bounds, quad, spatial_cache, layers);
        for (uint32_t i = 0; i < 6; ++i)
        {
            FontViewerVectorVertex& vertex = cache->m_Vertices[first + i];
            memcpy(vertex.m_Effect, gradient_vertices[shared[i]].m_Effect, sizeof(vertex.m_Effect));
            if (!vertex.m_Effect[2])
            {
                const uint8_t* color = gradient_vertices[shared[i]].m_FaceColor;
                packed[i].m_Color = Vector4(color[0], color[1], color[2], color[3]) * (1.0f / 255.0f);
            }
        }
    }
    else
    {
        for (uint32_t i = 0; i < 6; ++i)
            memset(cache->m_Vertices[first + i].m_Effect, 0, sizeof(cache->m_Vertices[first + i].m_Effect));
    }
    for (uint32_t i = 0; i < 6; ++i)
        cache->m_Vertices[first + i].m_Glyph = packed[i];
}

static bool CreateProgram(FontViewerVectorFont* cache)
{
    dmArray<char> slug;
    slug.SetCapacity(FONTVIEWER_SLUG_SIZE + 1);
    slug.SetSize(slug.Capacity());
    memcpy(slug.Begin(), FONTVIEWER_SLUG, FONTVIEWER_SLUG_SIZE);
    slug[FONTVIEWER_SLUG_SIZE] = 0;
    // Standalone OpenGL requires a combined integer sampler.
    char* texture_type = strstr(slug.Begin(), "utexture2D");
    memcpy(texture_type, "usampler2D", 10);
    const char*   source = (const char*)FONTVIEWER_VECTOR_FP;
    const char*   include = strstr(source, "#include");
    const char*   paint_include = strstr(include + 1, "#include");
    const char*   suffix = strchr(paint_include, '\n');
    uint32_t      prefix_size = include - source;
    uint32_t      suffix_size = FONTVIEWER_VECTOR_FP_SIZE - (suffix - source);
    dmArray<char> fragment;
    fragment.SetCapacity(prefix_size + FONTVIEWER_SLUG_SIZE + FONTVIEWER_PAINT_SIZE + suffix_size + 1);
    fragment.SetSize(fragment.Capacity());
    memcpy(fragment.Begin(), source, prefix_size);
    memcpy(fragment.Begin() + prefix_size, slug.Begin(), FONTVIEWER_SLUG_SIZE);
    memcpy(fragment.Begin() + prefix_size + FONTVIEWER_SLUG_SIZE, FONTVIEWER_PAINT, FONTVIEWER_PAINT_SIZE);
    memcpy(fragment.Begin() + prefix_size + FONTVIEWER_SLUG_SIZE + FONTVIEWER_PAINT_SIZE, suffix, suffix_size);
    fragment[fragment.Size() - 1] = 0;
    dmGraphics::ShaderDescBuilder shaders;
    shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, (const char*)FONTVIEWER_VECTOR_VP, FONTVIEWER_VECTOR_VP_SIZE);
    shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, fragment.Begin(), fragment.Size() - 1);
    const char*                                attributes[] = { "effect", "position", "color", "clip", "font_position", "paint" };
    const uint32_t                             components[] = { 4, 4, 4, 4, 2, 1 };
    const dmGraphics::ShaderDesc::ShaderDataType types[] = {
        dmGraphics::ShaderDesc::SHADER_TYPE_VEC4,
        dmGraphics::ShaderDesc::SHADER_TYPE_VEC4,
        dmGraphics::ShaderDesc::SHADER_TYPE_VEC4,
        dmGraphics::ShaderDesc::SHADER_TYPE_VEC4,
        dmGraphics::ShaderDesc::SHADER_TYPE_VEC2,
        dmGraphics::ShaderDesc::SHADER_TYPE_FLOAT
    };
    dmGraphics::HVertexStreamDeclaration streams = dmGraphics::NewVertexStreamDeclaration(cache->m_Context);
    for (uint32_t i = 0; i < 6; ++i)
    {
        shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, attributes[i], i, types[i]);
        dmGraphics::AddVertexStream(streams, attributes[i], components[i], dmGraphics::TYPE_FLOAT, false);
    }
    cache->m_Declaration = dmGraphics::NewVertexDeclaration(cache->m_Context, streams, sizeof(FontViewerVectorVertex));
    dmGraphics::DeleteVertexStreamDeclaration(streams);
    shaders.AddTexture("curve_texture", 0, dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D);
    shaders.AddTexture("band_texture", 1, dmGraphics::ShaderDesc::SHADER_TYPE_UTEXTURE2D);
    shaders.AddTexture("paint_texture", 2, dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D);
    shaders.AddTexture("font_effects", 3, dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D);
    char error[1024] = {};
    cache->m_Program = dmGraphics::NewProgram(cache->m_Context, shaders.Get(), error, sizeof(error));
    if (!cache->m_Program)
        dmLogError("Unable to create color Slug shader: %s", error);
    return cache->m_Program != 0;
}

static bool UploadTexture(FontViewerVectorFont* cache, uint32_t index, const void* bytes, uint32_t size, uint32_t bytes_per_texel, dmGraphics::TextureFormat format)
{
    const uint32_t width = FONT_VECTOR_SLUG_WIDTH;
    const uint32_t height = (size + width * bytes_per_texel - 1) / (width * bytes_per_texel);
    if (!height || height > FONT_VECTOR_SLUG_MAX_HEIGHT)
        return false;
    dmArray<uint8_t> padded;
    padded.SetCapacity(width * height * bytes_per_texel);
    padded.SetSize(padded.Capacity());
    memcpy(padded.Begin(), bytes, size);
    memset(padded.Begin() + size, 0, padded.Size() - size);
    if (!cache->m_Textures[index])
    {
        dmGraphics::TextureCreationParams creation;
        creation.m_Width = creation.m_OriginalWidth = width;
        creation.m_Height = creation.m_OriginalHeight = height;
        cache->m_Textures[index] = dmGraphics::NewTexture(cache->m_Context, creation);
    }
    dmGraphics::TextureParams params;
    params.m_Width = width;
    params.m_Height = height;
    params.m_Depth = params.m_LayerCount = 1;
    params.m_Format = format;
    params.m_MinFilter = params.m_MagFilter = dmGraphics::TEXTURE_FILTER_NEAREST;
    params.m_Data = padded.Begin();
    params.m_DataSize = padded.Size();
    dmGraphics::SetTexture(cache->m_Context, cache->m_Textures[index], params);
    return cache->m_Textures[index] != 0;
}

bool FontViewerVectorFontUpload(FontViewerVectorFont* cache, dmGraphics::HContext context)
{
    if (cache->m_Vertices.Empty())
        return true;
    cache->m_Context = context;
    if (!cache->m_Program && !CreateProgram(cache))
        return false;
    if (cache->m_Dirty)
    {
        if (!UploadTexture(cache, 0, cache->m_Slug.m_Curves.Begin(), cache->m_Slug.m_Curves.Size() * sizeof(uint16_t), 8, dmGraphics::TEXTURE_FORMAT_RGBA16F) ||
            !UploadTexture(cache, 1, cache->m_Slug.m_Bands.Begin(), cache->m_Slug.m_Bands.Size() * sizeof(uint32_t), 4, dmGraphics::TEXTURE_FORMAT_R32UI) ||
            !UploadTexture(cache, 2, cache->m_Paint.m_Paints.Begin(), cache->m_Paint.m_Paints.Size() * sizeof(Vector4), 16, dmGraphics::TEXTURE_FORMAT_RGBA32F))
            return false;
        cache->m_Dirty = false;
    }
    if (!cache->m_VertexBuffer)
        cache->m_VertexBuffer = dmGraphics::NewVertexBuffer(context, 0, 0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
    dmGraphics::SetVertexBufferData(cache->m_VertexBuffer, cache->m_Vertices.Size() * sizeof(FontViewerVectorVertex), cache->m_Vertices.Begin(), dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
    return cache->m_VertexBuffer != 0;
}

void FontViewerVectorFontDraw(FontViewerVectorFont* cache, dmGraphics::HTexture effects)
{
    if (cache->m_Vertices.Empty())
        return;
    dmGraphics::HContext context = cache->m_Context;
    dmGraphics::SetBlendFunc(context, dmGraphics::BLEND_FACTOR_ONE, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    dmGraphics::EnableProgram(context, cache->m_Program);
    const char* samplers[] = { "curve_texture", "band_texture", "paint_texture" };
    for (uint32_t i = 0; i < 3; ++i)
    {
        dmGraphics::SetSampler(context, dmGraphics::FindUniformLocation(cache->m_Program, samplers[i]), i);
        dmGraphics::EnableTexture(context, i, 0, cache->m_Textures[i]);
    }
    dmGraphics::SetSampler(context, dmGraphics::FindUniformLocation(cache->m_Program, "font_effects"), 3);
    dmGraphics::EnableTexture(context, 3, 0, effects);
    dmGraphics::EnableVertexBuffer(context, cache->m_VertexBuffer, 0);
    dmGraphics::EnableVertexDeclaration(context, cache->m_Declaration, 0, 0, cache->m_Program);
    dmGraphics::Draw(context, dmGraphics::PRIMITIVE_TRIANGLES, 0, cache->m_Vertices.Size(), 1);
    dmGraphics::DisableVertexDeclaration(context, cache->m_Declaration);
    dmGraphics::DisableVertexBuffer(context, cache->m_VertexBuffer);
    dmGraphics::DisableTexture(context, 3, effects);
    for (uint32_t i = 0; i < 3; ++i)
        dmGraphics::DisableTexture(context, i, cache->m_Textures[i]);
    dmGraphics::SetBlendFunc(context, dmGraphics::BLEND_FACTOR_SRC_ALPHA, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
}
