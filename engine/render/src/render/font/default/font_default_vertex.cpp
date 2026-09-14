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

#include <assert.h>                        // for assert
#include <stdint.h>                         // for uint32_t
#include <string.h>                         // for memset
#include <dlib/log.h>                       // for dmLog*
#include <dlib/math.h>                      // for dmMath::Max
#include <dlib/profile.h>                   // for DM_PROFILE, DM_PROPERTY_*
#include <dlib/vmath.h>                     // for Vector4

#include <graphics/graphics.h>              // for AddVertexStream etc
#include <graphics/graphics_util.h>         // for UnpackRGBA

#include "render/render_private.h"          // for TextEntry
#include "render/font/fontmap.h"
#include "render/font/fontmap_private.h"
#include "render/font/font_renderer_private.h"
#include "render/font/default/font_default_vertex.h"

#include <dmsdk/font/text_layout.h>
#include <font/text_layout.h>

namespace dmRender
{

static const uint32_t FALLBACK_CODEPOINT = 126U; // '~'
typedef FontDefaultVertex GlyphVertex;

struct FontRenderBackend
{
    int dummy; // Making it non empty
};

HFontRenderBackend CreateFontRenderBackend()
{
    FontRenderBackend* ctx = new FontRenderBackend;
    memset(ctx, 0, sizeof(*ctx));
    return ctx;
}

void DestroyFontRenderBackend(HFontRenderBackend ctx)
{
    delete ctx;
}

uint32_t GetFontVertexSize(HFontRenderBackend backend)
{
    (void)backend;
    return sizeof(GlyphVertex);
}

dmGraphics::HVertexDeclaration CreateVertexDeclaration(HFontRenderBackend backend, dmGraphics::HContext context)
{
    (void)backend;

    dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(context);
    dmGraphics::AddVertexStream(stream_declaration, "position", 4, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(stream_declaration, "texcoord", 4, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(stream_declaration, "effect_params", 4, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(stream_declaration, "banding", 4, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(stream_declaration, "color", 4, dmGraphics::TYPE_UNSIGNED_BYTE, true);
    dmGraphics::AddVertexStream(stream_declaration, "texcoord0", 2, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(stream_declaration, "face_color", 4, dmGraphics::TYPE_FLOAT, true);
    dmGraphics::AddVertexStream(stream_declaration, "outline_color", 4, dmGraphics::TYPE_FLOAT, true);
    dmGraphics::AddVertexStream(stream_declaration, "shadow_color", 4, dmGraphics::TYPE_FLOAT, true);
    dmGraphics::AddVertexStream(stream_declaration, "sdf_params", 4, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(stream_declaration, "layer_mask", 3, dmGraphics::TYPE_FLOAT, false);

    dmGraphics::HVertexDeclaration decl = dmGraphics::NewVertexDeclaration(context, stream_declaration, GetFontVertexSize(backend));

    dmGraphics::DeleteVertexStreamDeclaration(stream_declaration);

    return decl;
}

#define HAS_LAYER(mask,layer) ((mask & layer) == layer)

static void ClearGlyphVertex(GlyphVertex& vertex)
{
    memset(&vertex, 0, sizeof(vertex));
}

static void ExpandFontGlyphVertices(FontGlyphVertex* compact_vertices, uint32_t vertex_count, GlyphVertex* vertices)
{
    // GlyphVertex is wider than FontGlyphVertex. Expanding from back to front
    // keeps each unread compact record intact while reusing the output buffer.
    for (uint32_t i = vertex_count; i > 0; --i)
    {
        const uint32_t vertex_index = i - 1;
        const FontGlyphVertex compact = compact_vertices[vertex_index];
        GlyphVertex& vertex = vertices[vertex_index];
        ClearGlyphVertex(vertex);

        memcpy(vertex.m_Position, compact.m_Position, sizeof(compact.m_Position));
        vertex.m_Position[3] = 1.0f;
        vertex.m_UV[0] = FontUnpackGlyphUV(compact.m_UV[0]);
        vertex.m_UV[1] = FontUnpackGlyphUV(compact.m_UV[1]);

        const float recip_255 = 1.0f / 255.0f;
        for (uint32_t component = 0; component < 4; ++component)
        {
            vertex.m_FaceColor[component] = compact.m_FaceColor[component] * recip_255;
            vertex.m_OutlineColor[component] = compact.m_OutlineColor[component] * recip_255;
            vertex.m_ShadowColor[component] = compact.m_ShadowColor[component] * recip_255;
        }

        memcpy(vertex.m_SdfParams, compact.m_SdfParams, sizeof(compact.m_SdfParams));
        memcpy(vertex.m_LayerMasks, compact.m_LayerMasks, sizeof(compact.m_LayerMasks));
    }
}

static uint8_t ToUNorm8(float v)
{
    v = dmMath::Max(0.0f, dmMath::Min(1.0f, v));
    return (uint8_t)(v * 255.0f + 0.5f);
}

static void SetVectorColor(GlyphVertex& vertex, const Vector4& color)
{
    vertex.m_VectorColor[0] = ToUNorm8(color.getX());
    vertex.m_VectorColor[1] = ToUNorm8(color.getY());
    vertex.m_VectorColor[2] = ToUNorm8(color.getZ());
    vertex.m_VectorColor[3] = ToUNorm8(color.getW());
}

static void SetSdfEffectProperties(GlyphVertex& vertex, float layer_mode, const Vector4& color)
{
    const bool is_outline = layer_mode == 1.0f;
    float* layer_color = is_outline ? vertex.m_OutlineColor : vertex.m_ShadowColor;
    layer_color[0] = color.getX();
    layer_color[1] = color.getY();
    layer_color[2] = color.getZ();
    layer_color[3] = color.getW();
    vertex.m_LayerMasks[is_outline ? 1 : 2] = 1.0f;
}

static void ClearGlyphQuad(uint32_t vertexindex, GlyphVertex* vertices)
{
    for (uint32_t i = 0; i < 6; ++i)
    {
        ClearGlyphVertex(vertices[vertexindex + i]);
    }
}

static void OutputGlyphVector(uint32_t vertexindex,
                              const dmVMath::Matrix4& transform,
                              float x,
                              float y,
                              float width,
                              float placement_width,
                              float placement_left_bearing,
                              float ascent,
                              float descent,
                              float curve_start,
                              float curve_count,
                              float curve_texel_stride,
                              float stripe_texel,
                              float stripe_count,
                              const float* banding,
                              float sdf_u0,
                              float sdf_v0,
                              float sdf_u1,
                              float sdf_v1,
                              bool use_sdf_shadow,
                              float sdf_outline,
                              float sdf_shadow,
                              float sdf_spread,
                              float sdf_smoothing,
                              float texcoord_min_x,
                              float texcoord_min_y,
                              float texcoord_max_x,
                              float texcoord_max_y,
                              float geometry_min_x,
                              float geometry_min_y,
                              float geometry_max_x,
                              float geometry_max_y,
                              float offset_x,
                              float offset_y,
                              float outline_width,
                              float shadow_blur,
                              float layer_mode,
                              const Vector4& color,
                              GlyphVertex* vertices)
{
    GlyphVertex& v1 = vertices[vertexindex];
    GlyphVertex& v2 = vertices[vertexindex + 1];
    GlyphVertex& v3 = vertices[vertexindex + 2];
    GlyphVertex& v4 = vertices[vertexindex + 3];
    GlyphVertex& v5 = vertices[vertexindex + 4];
    GlyphVertex& v6 = vertices[vertexindex + 5];

    ClearGlyphVertex(v1);
    ClearGlyphVertex(v2);
    ClearGlyphVertex(v3);
    ClearGlyphVertex(v6);

    float size_diff = width - placement_width;
    float quad_left = x - size_diff * 0.5f + placement_left_bearing + offset_x;
    float quad_bottom = y - descent + offset_y;
    float height = dmMath::Max(0.0001f, ascent + descent);

    (Vector4&)v1.m_Position = transform * Vector4(quad_left + geometry_min_x * width, quad_bottom + geometry_min_y * height, 0.0f, 1.0f);
    (Vector4&)v2.m_Position = transform * Vector4(quad_left + geometry_min_x * width, quad_bottom + geometry_max_y * height, 0.0f, 1.0f);
    (Vector4&)v3.m_Position = transform * Vector4(quad_left + geometry_max_x * width, quad_bottom + geometry_min_y * height, 0.0f, 1.0f);
    (Vector4&)v6.m_Position = transform * Vector4(quad_left + geometry_max_x * width, quad_bottom + geometry_max_y * height, 0.0f, 1.0f);

    #define SET_VECTOR_VERTEX(v, u, vv) \
        v.m_VectorTexcoord[0] = u; \
        v.m_VectorTexcoord[1] = vv; \
        v.m_VectorTexcoord[2] = curve_count; \
        v.m_VectorTexcoord[3] = use_sdf_shadow ? 1.0f : 0.0f; \
        v.m_VectorEffectParams[0] = use_sdf_shadow ? sdf_outline : stripe_texel; \
        v.m_VectorEffectParams[1] = use_sdf_shadow ? sdf_shadow : stripe_count; \
        v.m_VectorEffectParams[2] = outline_width; \
        v.m_VectorEffectParams[3] = shadow_blur; \
        if (banding && !use_sdf_shadow) memcpy(v.m_VectorEffectParams, banding, sizeof(v.m_VectorEffectParams)); \
        v.m_Position[2] = use_sdf_shadow ? v.m_Position[2] : curve_start; \
        v.m_Position[3] = use_sdf_shadow ? v.m_Position[3] : layer_mode; \
        v.m_SdfParams[0] = use_sdf_shadow ? 0.75f : width; \
        v.m_SdfParams[1] = use_sdf_shadow ? sdf_outline : height; \
        v.m_SdfParams[2] = use_sdf_shadow ? sdf_smoothing : curve_texel_stride; \
        v.m_SdfParams[3] = use_sdf_shadow ? sdf_shadow : sdf_spread; \
        v.m_LayerMasks[0] = use_sdf_shadow ? 1.0f : 0.0f; \
        SetVectorColor(v, color);

    SET_VECTOR_VERTEX(v1, texcoord_min_x, texcoord_min_y)
    SET_VECTOR_VERTEX(v2, texcoord_min_x, texcoord_max_y)
    SET_VECTOR_VERTEX(v3, texcoord_max_x, texcoord_min_y)
    SET_VECTOR_VERTEX(v6, texcoord_max_x, texcoord_max_y)

    if (use_sdf_shadow && banding)
    {
        GlyphVertex* quad[4] = { &v1, &v2, &v3, &v6 };
        const float u[4] = { sdf_u0, sdf_u0, sdf_u1, sdf_u1 };
        // Bitmap rows run downwards; vector quad coordinates run upwards.
        const float v[4] = { sdf_v1, sdf_v0, sdf_v1, sdf_v0 };
        for (uint32_t i = 0; i < 4; ++i)
        {
            quad[i]->m_VectorTexcoord[0] = u[i];
            quad[i]->m_VectorTexcoord[1] = v[i];
            quad[i]->m_VectorTexcoord[3] = layer_mode; // 1 = outline, 2 = shadow.
        }
    }

    if (use_sdf_shadow)
    {
        // The vector SDF fallback uses the established font-df vertex and
        // fragment contract. Its layer mask selects either outline or shadow.
        v1.m_LayerMasks[0] = 0.0f;
        v2.m_LayerMasks[0] = 0.0f;
        v3.m_LayerMasks[0] = 0.0f;
        v6.m_LayerMasks[0] = 0.0f;
        SetSdfEffectProperties(v1, layer_mode, color);
        SetSdfEffectProperties(v2, layer_mode, color);
        SetSdfEffectProperties(v3, layer_mode, color);
        SetSdfEffectProperties(v6, layer_mode, color);
    }

    #undef SET_VECTOR_VERTEX

    v1.m_UV[0] = sdf_u0;
    v1.m_UV[1] = sdf_v1;
    v2.m_UV[0] = sdf_u0;
    v2.m_UV[1] = sdf_v0;
    v3.m_UV[0] = sdf_u1;
    v3.m_UV[1] = sdf_v1;
    v6.m_UV[0] = sdf_u1;
    v6.m_UV[1] = sdf_v0;

    v4 = v3;
    v5 = v2;
}

void GetTextMetrics(HFontRenderBackend backend, HFontMap font_map, const char* text,
                    TextLayoutSettings* settings, TextMetrics* metrics)
{
    DM_PROFILE(__FUNCTION__);
    (void)backend;

    dmArray<uint32_t> codepoints;
    TextToCodePoints(text, codepoints);

    if (settings->m_Size <= 0.0f)
        settings->m_Size = GetFontMapSize(font_map);

    TextLayout* layout = 0;
    TextResult r = TextLayoutCreate(font_map->m_FontCollection, codepoints.Begin(), codepoints.Size(), settings, &layout);
    if (TEXT_RESULT_OK == r)
    {
        TextLayoutGetBounds(layout, &metrics->m_Width, &metrics->m_Height);
        metrics->m_LineCount   = TextLayoutGetLineCount(layout);
        metrics->m_MaxAscent   = font_map->m_MaxAscent;
        metrics->m_MaxDescent  = font_map->m_MaxDescent;
    }

    TextLayoutRelease(layout);
}

static uint32_t CreateFontVectorVertexData(HFontMap font_map,
                                           uint32_t frame,
                                           TextLayout* layout,
                                           const TextEntry& te,
                                           float sdf_scale,
                                           GlyphVertex* vertices,
                                           uint32_t num_vertices)
{
    const Vector4 face_color = dmGraphics::UnpackRGBA(te.m_FaceColor);
    const Vector4 outline_color = dmGraphics::UnpackRGBA(te.m_OutlineColor);
    const Vector4 shadow_color = dmGraphics::UnpackRGBA(te.m_ShadowColor);
    const float reference_size = GetFontMapSize(font_map);
    const float font_size = te.m_FontSize > 0.0f ? te.m_FontSize : reference_size;
    const float font_scale = reference_size > 0.0f ? font_size / reference_size : 1.0f;
    const float line_height = (font_map->m_MaxAscent + font_map->m_MaxDescent) * font_scale;
    const float leading = line_height * te.m_Leading;
    const float sdf_smoothing = 0.25f /
        (font_map->m_SdfSpread * dmMath::Max(sdf_scale * font_scale, 0.0001f));

    TextGlyph* glyphs = TextLayoutGetGlyphs(layout);
    uint32_t glyph_count = TextLayoutGetGlyphCount(layout);
    uint32_t line_count = TextLayoutGetLineCount(layout);
    TextLine* lines = TextLayoutGetLines(layout);

    const uint32_t vertices_per_quad = 6;
    const uint8_t requested_layer_mask = te.m_RenderLayerMask != 0
        ? te.m_RenderLayerMask
        : (FACE | OUTLINE | SHADOW);
    const uint8_t render_layer_mask = requested_layer_mask & font_map->m_LayerMask;
    const bool emit_face = HAS_LAYER(render_layer_mask, FACE);
    const bool emit_shadow = HAS_LAYER(render_layer_mask, SHADOW) &&
                             shadow_color.getW() > 0.0f;
    const bool emit_outline = HAS_LAYER(render_layer_mask, OUTLINE) &&
                              outline_color.getW() > 0.0f &&
                              font_map->m_OutlineWidth > 0.0f;
    const uint32_t layer_count = (emit_face ? 1 : 0) + (emit_outline ? 1 : 0) + (emit_shadow ? 1 : 0);
    if (layer_count == 0)
        return 0;

    uint32_t valid_glyph_count = glyph_count;
    for (uint32_t i = 0; i < glyph_count; ++i)
    {
        if (dmUtf8::IsWhiteSpace(glyphs[i].m_Codepoint))
        {
            valid_glyph_count--;
        }
    }

    uint32_t layer_stride = vertices_per_quad * valid_glyph_count;
    uint32_t glyph_slot = 0;

    uint32_t align = te.m_Align;
    float x_offset = OffsetX(align, te.m_Width);
    if (font_map->m_IsMonospaced)
    {
        x_offset -= font_map->m_Padding * 0.5f;
    }
    float y_offset = OffsetY(te.m_VAlign, te.m_Height, font_map->m_MaxAscent * font_scale, font_map->m_MaxDescent * font_scale, te.m_Leading, line_count);

    for (uint32_t i = 0; i < line_count; ++i)
    {
        TextLine& line = lines[i];
        int32_t first_x = glyphs[line.m_Index].m_X;
        int32_t first_y = glyphs[line.m_Index].m_Y;

        const float line_start_x = x_offset - OffsetX(align, line.m_Width);
        const float line_start_y = y_offset - i * leading;

        int gi_end = line.m_Index + line.m_Length;
        for (int gi = line.m_Index; gi < gi_end; ++gi)
        {
            TextGlyph* g = &glyphs[gi];
            if (dmUtf8::IsWhiteSpace(g->m_Codepoint))
                continue;

            if ((glyph_slot + 1) * vertices_per_quad * layer_count > num_vertices)
            {
                dmLogWarning("Character buffer exceeded (size: %d), increase the \"graphics.max_characters\" property in your game.project file.", num_vertices / 6);
                return glyph_slot * vertices_per_quad * layer_count;
            }

            uint32_t shadow_vertexindex = glyph_slot * vertices_per_quad;
            uint32_t outline_vertexindex = emit_shadow ? shadow_vertexindex + layer_stride : shadow_vertexindex;
            uint32_t face_vertexindex = outline_vertexindex + (emit_outline ? layer_stride : 0);

            float x = line_start_x + (g->m_X - first_x);
            float y = line_start_y + (g->m_Y - first_y);

            uint32_t glyph_index = g->m_GlyphIndex;
            HFont font = g->m_Font;
            FontGlyph* glyph = 0;
            FontResult r = dmRender::GetOrCreateGlyphByIndex(font_map, font, glyph_index, &glyph);
            if (FONT_RESULT_OK != r)
            {
                glyph_index = FontGetGlyphIndex(font, FALLBACK_CODEPOINT);
                r = dmRender::GetOrCreateGlyphByIndex(font_map, font, glyph_index, &glyph);
            }

            CacheGlyph* cache_glyph = 0;
            if (FONT_RESULT_OK == r && glyph &&
                (glyph->m_Outline.m_CommandCount > 0 || glyph->m_Vector.m_CurveCount > 0))
            {
                uint64_t glyph_key = dmRender::MakeGlyphIndexKey(font, glyph_index);
                if (!IsInCache(font_map, glyph_key))
                {
                    AddGlyphToCache(font_map, frame, glyph_key, glyph, 0);
                }
                cache_glyph = GetFromCache(font_map, glyph_key, frame);
            }

            if (!cache_glyph || !glyph)
            {
                if (emit_shadow)
                {
                    ClearGlyphQuad(shadow_vertexindex, vertices);
                }
                if (emit_outline)
                {
                    ClearGlyphQuad(outline_vertexindex, vertices);
                }
                if (emit_face)
                {
                    ClearGlyphQuad(face_vertexindex, vertices);
                }
                glyph_slot++;
                continue;
            }

            float glyph_width = dmMath::Max(0.0001f, glyph->m_Outline.m_Width * font_scale);
            float glyph_height = dmMath::Max(0.0001f, (glyph->m_Outline.m_Ascent + glyph->m_Outline.m_Descent) * font_scale);
            // Baked banks add an atlas border around the generated glyph.
            // It belongs to neither the glyph's geometry nor its sampled area.
            const float bitmap_border = FontGetType(font) == FONT_TYPE_GLYPH_BANK ? font_map->m_CacheCellPadding : 0.0f;
            float sdf_width = glyph->m_Bitmap.m_Width > 0 ? (glyph->m_Bitmap.m_Width - 2.0f * bitmap_border) * font_scale : glyph_width;
            float sdf_left_bearing = glyph->m_LeftBearing * font_scale;
            float sdf_ascent = glyph->m_Ascent * font_scale;
            float sdf_descent = glyph->m_Descent * font_scale;
            float face_texcoord_min_x = 0.0f;
            float face_texcoord_min_y = 0.0f;
            float face_texcoord_max_x = 1.0f;
            float face_texcoord_max_y = 1.0f;
            float outline_width = font_map->m_OutlineWidth * font_scale;
            float outline_width_u = outline_width / dmMath::Max(0.0001f, glyph_width);
            float outline_width_v = outline_width / glyph_height;
            float shadow_outline_width = emit_outline ? outline_width : 0.0f;
            // Keep the shadow quad large enough to cover the SDF-style shadow
            // ramp. The shadow spread is shadow_blur + sqrt(2), with one extra
            // pixel to avoid clipping from rasterization and interpolation.
            const float shadow_padding = (font_map->m_ShadowBlur + 2.4142f) * font_scale;
            float shadow_radius = shadow_outline_width + shadow_padding;
            float shadow_width_u = shadow_radius / dmMath::Max(0.0001f, glyph_width);
            float shadow_width_v = shadow_radius / glyph_height;
            float shadow_texcoord_min_x = -shadow_width_u;
            float shadow_texcoord_min_y = -shadow_width_v;
            float shadow_texcoord_max_x = 1.0f + shadow_width_u;
            float shadow_texcoord_max_y = 1.0f + shadow_width_v;
            // SDF outlines and bitmap shadows share the vector material. Legacy SDF effects
            // use a separate pass without the face.
            bool use_sdf_shadow = (!emit_face || font_map->m_VectorBitmapEffects) && emit_shadow &&
                                  cache_glyph->m_VectorSdfCached;
            bool use_sdf_outline = (!emit_face || font_map->m_VectorBitmapEffects) && emit_outline &&
                                   cache_glyph->m_VectorSdfCached;
            bool use_sdf_effect = use_sdf_shadow || use_sdf_outline;
            float sdf_u0 = 0.0f;
            float sdf_v0 = 0.0f;
            float sdf_u1 = 0.0f;
            float sdf_v1 = 0.0f;
            if (use_sdf_effect)
            {
                float atlas_width = (float)dmGraphics::GetTextureWidth(font_map->m_GraphicsContext,
                                                                       font_map->m_VectorSdfTexture);
                float atlas_height = (float)dmGraphics::GetTextureHeight(font_map->m_GraphicsContext,
                                                                         font_map->m_VectorSdfTexture);
                // Use texel edges: pixel centers then map to texel centers
                // without stretching the generated field by a texel.
                sdf_u0 = (cache_glyph->m_X + bitmap_border) / atlas_width;
                sdf_v0 = (cache_glyph->m_Y + bitmap_border) / atlas_height;
                sdf_u1 = (cache_glyph->m_X + glyph->m_Bitmap.m_Width - bitmap_border) / atlas_width;
                sdf_v1 = (cache_glyph->m_Y + glyph->m_Bitmap.m_Height - bitmap_border) / atlas_height;
                shadow_texcoord_min_x = 0.0f;
                shadow_texcoord_min_y = 0.0f;
                shadow_texcoord_max_x = 1.0f;
                shadow_texcoord_max_y = 1.0f;
            }
            if (emit_shadow)
            {
                float shadow_glyph_width = use_sdf_shadow ? sdf_width : glyph_width;
                float shadow_placement_width = use_sdf_shadow ? sdf_width : g->m_Width;
                float shadow_left_bearing = use_sdf_shadow ? sdf_left_bearing : g->m_LeftBearing;
                float shadow_ascent = use_sdf_shadow ? sdf_ascent : glyph->m_Ascent;
                float shadow_descent = use_sdf_shadow ? sdf_descent : glyph->m_Descent;
                OutputGlyphVector(shadow_vertexindex,
                                  te.m_Transform,
                                  x,
                                  y,
                                  shadow_glyph_width,
                                  shadow_placement_width,
                                  shadow_left_bearing,
                                  shadow_ascent,
                                  shadow_descent,
                                  cache_glyph->m_VectorCurveTexel,
                                  cache_glyph->m_VectorCurveCount,
                                  font_map->m_VectorCurveTexelsPerCurve,
                                  cache_glyph->m_VectorStripeTexel,
                                  cache_glyph->m_VectorStripeCount,
                                  font_map->m_VectorSlug ? cache_glyph->m_VectorBanding : 0,
                                  sdf_u0,
                                  sdf_v0,
                                  sdf_u1,
                                  sdf_v1,
                                  use_sdf_shadow,
                                  font_map->m_SdfOutline,
                                  font_map->m_SdfShadow,
                                  font_map->m_SdfSpread,
                                  sdf_smoothing,
                                  shadow_texcoord_min_x,
                                  shadow_texcoord_min_y,
                                  shadow_texcoord_max_x,
                                  shadow_texcoord_max_y,
                                  shadow_texcoord_min_x,
                                  shadow_texcoord_min_y,
                                  shadow_texcoord_max_x,
                                  shadow_texcoord_max_y,
                                  font_map->m_ShadowX * font_scale,
                                  font_map->m_ShadowY * font_scale,
                                  shadow_outline_width,
                                  font_map->m_ShadowBlur * font_scale,
                                  use_sdf_shadow ? 2.0f : 0.0f,
                                  shadow_color,
                                  vertices);
            }

            if (emit_outline)
            {
                float outline_min_x = face_texcoord_min_x - outline_width_u;
                float outline_min_y = face_texcoord_min_y - outline_width_v;
                float outline_max_x = face_texcoord_max_x + outline_width_u;
                float outline_max_y = face_texcoord_max_y + outline_width_v;
                float outline_sdf_u0 = 0.0f;
                float outline_sdf_v0 = 0.0f;
                float outline_sdf_u1 = 0.0f;
                float outline_sdf_v1 = 0.0f;
                if (use_sdf_outline)
                {
                    // The runtime bitmap metrics include SDF padding and do not
                    // share the analytical face's normalized bounds exactly.
                    // Use the complete atlas entry; it includes the padding
                    // required by the outline threshold.
                    outline_min_x = 0.0f;
                    outline_min_y = 0.0f;
                    outline_max_x = 1.0f;
                    outline_max_y = 1.0f;
                    outline_sdf_u0 = sdf_u0;
                    outline_sdf_v0 = sdf_v0;
                    outline_sdf_u1 = sdf_u1;
                    outline_sdf_v1 = sdf_v1;
                }
                float outline_glyph_width = use_sdf_outline ? sdf_width : glyph_width;
                float outline_placement_width = use_sdf_outline ? sdf_width : g->m_Width;
                float outline_left_bearing = use_sdf_outline ? sdf_left_bearing : g->m_LeftBearing;
                float outline_ascent = use_sdf_outline ? sdf_ascent : glyph->m_Ascent;
                float outline_descent = use_sdf_outline ? sdf_descent : glyph->m_Descent;
                OutputGlyphVector(outline_vertexindex,
                                  te.m_Transform,
                                  x,
                                  y,
                                  outline_glyph_width,
                                  outline_placement_width,
                                  outline_left_bearing,
                                  outline_ascent,
                                  outline_descent,
                                  cache_glyph->m_VectorCurveTexel,
                                  cache_glyph->m_VectorCurveCount,
                                  font_map->m_VectorCurveTexelsPerCurve,
                                  cache_glyph->m_VectorStripeTexel,
                                  cache_glyph->m_VectorStripeCount,
                                  font_map->m_VectorSlug ? cache_glyph->m_VectorBanding : 0,
                                  outline_sdf_u0,
                                  outline_sdf_v0,
                                  outline_sdf_u1,
                                  outline_sdf_v1,
                                  use_sdf_outline,
                                  font_map->m_SdfOutline,
                                  font_map->m_SdfShadow,
                                  font_map->m_SdfSpread,
                                  sdf_smoothing,
                                  outline_min_x,
                                  outline_min_y,
                                  outline_max_x,
                                  outline_max_y,
                                  outline_min_x,
                                  outline_min_y,
                                  outline_max_x,
                                  outline_max_y,
                                  0.0f,
                                  0.0f,
                                  outline_width,
                                  0.0f,
                                  1.0f,
                                  outline_color,
                                  vertices);
            }

            if (emit_face)
            {
                OutputGlyphVector(face_vertexindex,
                                  te.m_Transform,
                                  x,
                                  y,
                                  glyph_width,
                                  glyph->m_Outline.m_Width * font_scale,
                                  glyph->m_Outline.m_LeftBearing * font_scale,
                                  glyph->m_Outline.m_Ascent * font_scale,
                                  glyph->m_Outline.m_Descent * font_scale,
                                  cache_glyph->m_VectorCurveTexel,
                                  cache_glyph->m_VectorCurveCount,
                                  font_map->m_VectorCurveTexelsPerCurve,
                                  cache_glyph->m_VectorStripeTexel,
                                  cache_glyph->m_VectorStripeCount,
                                  font_map->m_VectorSlug ? cache_glyph->m_VectorBanding : 0,
                                  0.0f,
                                  0.0f,
                                  0.0f,
                                  0.0f,
                                  false,
                                  font_map->m_SdfOutline,
                                  font_map->m_SdfShadow,
                                  font_map->m_SdfSpread,
                                  sdf_smoothing,
                                  face_texcoord_min_x,
                                  face_texcoord_min_y,
                                  face_texcoord_max_x,
                                  face_texcoord_max_y,
                                  face_texcoord_min_x,
                                  face_texcoord_min_y,
                                  face_texcoord_max_x,
                                  face_texcoord_max_y,
                                  0.0f,
                                  0.0f,
                                  outline_width,
                                  0.0f,
                                  0.0f,
                                  face_color,
                                  vertices);
            }

            glyph_slot++;
        }
    }

    return glyph_slot * vertices_per_quad * layer_count;
}


static uint32_t CreateFontVertexDataFromTextLayout(HFontMap font_map, uint32_t frame, HTextLayout layout, const TextEntry& te, float sdf_scale, uint8_t* _vertices, uint32_t num_vertices)
{
    assert(layout->m_FontCollection == GetFontCollection(font_map));
    assert(font_map->m_IsVector);

    return CreateFontVectorVertexData(font_map, frame, layout, te, sdf_scale, (GlyphVertex*)_vertices, num_vertices);
}

#undef HAS_LAYER

uint32_t CreateFontVertexData(HFontRenderBackend backend, HFontMap font_map, uint32_t frame, const char* text, const TextEntry& te, float sdf_scale, float recip_w, float recip_h, uint8_t* _vertices, uint32_t num_vertices)
{
    DM_PROFILE(__FUNCTION__);
    (void)backend;

    if (!font_map->m_IsVector)
    {
        FontGlyphVertex* compact_vertices = (FontGlyphVertex*)_vertices;
        const uint32_t vertex_count = dmRender::CreateFontVertexData(font_map, frame, text, te, sdf_scale, recip_w, recip_h, compact_vertices, num_vertices);
        ExpandFontGlyphVertices(compact_vertices, vertex_count, (GlyphVertex*)_vertices);
        return vertex_count;
    }

    if (te.m_TextLayout)
    {
        return CreateFontVertexDataFromTextLayout(font_map, frame, te.m_TextLayout, te, sdf_scale, _vertices, num_vertices);
    }

    dmArray<uint32_t> codepoints;
    TextToCodePoints(text, codepoints);

    TextLayoutSettings layoutsettings = {0};
    layoutsettings.m_Size = te.m_FontSize > 0.0f ? te.m_FontSize : dmRender::GetFontMapSize(font_map);
    layoutsettings.m_LineBreak = te.m_LineBreak;
    layoutsettings.m_Width = te.m_Width;
    layoutsettings.m_Tracking = te.m_Tracking;
    layoutsettings.m_Leading = te.m_Leading;
    // legacy options for glyph bank fonts
    layoutsettings.m_Monospace = dmRender::GetFontMapMonospaced(font_map);
    layoutsettings.m_Padding = dmRender::GetFontMapPadding(font_map);

    HTextLayout layout = 0;
    TextResult r = TextLayoutCreate(font_map->m_FontCollection, codepoints.Begin(), codepoints.Size(), &layoutsettings, &layout);
    if (TEXT_RESULT_OK != r)
    {
        if (layout)
            TextLayoutRelease(layout);
        return 0;
    }

    uint32_t vertex_count = CreateFontVertexDataFromTextLayout(font_map, frame, layout, te, sdf_scale, _vertices, num_vertices);
    TextLayoutRelease(layout);
    return vertex_count;
}

} // namespace
