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
#include <font/render/layout_vertex.h>

namespace dmRender
{

static const uint32_t FALLBACK_CODEPOINT = 126U; // '~'
typedef FontDefaultVertex GlyphVertex;

dmGraphics::HVertexDeclaration CreateFontVertexDeclaration(dmGraphics::HContext context)
{
    dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(context);
    dmGraphics::AddVertexStream(stream_declaration, "position", 4, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(stream_declaration, "texcoord", 4, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(stream_declaration, "effect_params", 4, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(stream_declaration, "color", 4, dmGraphics::TYPE_UNSIGNED_BYTE, true);
    dmGraphics::AddVertexStream(stream_declaration, "texcoord0", 2, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(stream_declaration, "face_color", 4, dmGraphics::TYPE_FLOAT, true);
    dmGraphics::AddVertexStream(stream_declaration, "outline_color", 4, dmGraphics::TYPE_FLOAT, true);
    dmGraphics::AddVertexStream(stream_declaration, "shadow_color", 4, dmGraphics::TYPE_FLOAT, true);
    dmGraphics::AddVertexStream(stream_declaration, "sdf_params", 4, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(stream_declaration, "layer_mask", 3, dmGraphics::TYPE_FLOAT, false);

    dmGraphics::HVertexDeclaration decl = dmGraphics::NewVertexDeclaration(context, stream_declaration, sizeof(GlyphVertex));

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

// The caller clears the planned vertex range, including missing glyphs.
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
                              const float* banding,
                              float sdf_u0,
                              float sdf_v0,
                              float sdf_u1,
                              float sdf_v1,
                              bool bitmap_effect,
                              float sdf_outline,
                              float offset_x,
                              float offset_y,
                              float layer_mode,
                              const Vector4& color,
                              const uint32_t* face_colors,
                              GlyphVertex* vertices)
{
    GlyphVertex& v1 = vertices[vertexindex];
    GlyphVertex& v2 = vertices[vertexindex + 1];
    GlyphVertex& v3 = vertices[vertexindex + 2];
    GlyphVertex& v4 = vertices[vertexindex + 3];
    GlyphVertex& v5 = vertices[vertexindex + 4];
    GlyphVertex& v6 = vertices[vertexindex + 5];

    float size_diff = width - placement_width;
    float quad_left = x - size_diff * 0.5f + placement_left_bearing + offset_x;
    float quad_bottom = y - descent + offset_y;
    float height = dmMath::Max(0.0001f, ascent + descent);

    (Vector4&)v1.m_Position = transform * Vector4(quad_left, quad_bottom, 0.0f, 1.0f);
    (Vector4&)v2.m_Position = transform * Vector4(quad_left, quad_bottom + height, 0.0f, 1.0f);
    (Vector4&)v3.m_Position = transform * Vector4(quad_left + width, quad_bottom, 0.0f, 1.0f);
    (Vector4&)v6.m_Position = transform * Vector4(quad_left + width, quad_bottom + height, 0.0f, 1.0f);

    #define SET_VECTOR_VERTEX(v, u, vv) \
        v.m_Position[3] = v.m_Position[2]; \
        v.m_VectorTexcoord[0] = u; \
        v.m_VectorTexcoord[1] = vv; \
        v.m_VectorTexcoord[2] = curve_count; \
        v.m_VectorTexcoord[3] = 0.0f; \
        if (bitmap_effect) v.m_VectorEffectParams[0] = sdf_outline; \
        else memcpy(v.m_VectorEffectParams, banding, sizeof(v.m_VectorEffectParams)); \
        if (!bitmap_effect) v.m_Position[2] = curve_start;

    SET_VECTOR_VERTEX(v1, 0.0f, 0.0f)
    SET_VECTOR_VERTEX(v2, 0.0f, 1.0f)
    SET_VECTOR_VERTEX(v3, 1.0f, 0.0f)
    SET_VECTOR_VERTEX(v6, 1.0f, 1.0f)

    if (face_colors)
    {
        memcpy(v1.m_VectorColor, &face_colors[0], sizeof(uint32_t));
        memcpy(v2.m_VectorColor, &face_colors[2], sizeof(uint32_t));
        memcpy(v3.m_VectorColor, &face_colors[1], sizeof(uint32_t));
        memcpy(v6.m_VectorColor, &face_colors[3], sizeof(uint32_t));
    }
    else
    {
        SetVectorColor(v1, color);
        memcpy(v2.m_VectorColor, v1.m_VectorColor, sizeof(v1.m_VectorColor));
        memcpy(v3.m_VectorColor, v1.m_VectorColor, sizeof(v1.m_VectorColor));
        memcpy(v6.m_VectorColor, v1.m_VectorColor, sizeof(v1.m_VectorColor));
    }

    if (bitmap_effect)
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

    #undef SET_VECTOR_VERTEX

    v4 = v3;
    v5 = v2;
}

static uint32_t CreateFontVectorVertexData(HFontMap font_map,
                                           uint32_t frame,
                                           TextLayout* layout,
                                           const TextEntry& te,
                                           GlyphVertex* vertices,
                                           uint32_t num_vertices)
{
    if (num_vertices < 6)
        return 0;

    TextLayoutRefreshObjectStyles(layout);
    const Vector4 face_color = dmGraphics::UnpackRGBA(te.m_FaceColor);
    const float reference_size = GetFontMapSize(font_map);
    const float font_size = te.m_FontSize > 0.0f ? te.m_FontSize : reference_size;
    const float base_scale = reference_size > 0.0f ? font_size / reference_size : 1.0f;
    const uint8_t render_layer_mask = font_map->m_LayerMask;

    FontLayoutVertexConfig config = {};
    config.m_Layout = layout;
    config.m_OutlineColor = dmGraphics::UnpackRGBA(te.m_OutlineColor);
    config.m_ShadowColor = dmGraphics::UnpackRGBA(te.m_ShadowColor);
    config.m_OutlineColor.setW(te.m_OutlineAlpha);
    config.m_ShadowColor.setW(te.m_ShadowAlpha);
    for (uint32_t i = 0; i < 4; ++i)
        config.m_FaceColor[i] = face_color[i];
    config.m_SdfEdge = 0.75f;
    config.m_SdfOutline = font_map->m_SdfOutline;
    config.m_SdfShadow = font_map->m_SdfShadow;
    config.m_SdfSpread = font_map->m_SdfSpread;
    config.m_OutlineWidth = font_map->m_OutlineWidth;
    config.m_ShadowX = font_map->m_ShadowX;
    config.m_ShadowY = font_map->m_ShadowY;
    config.m_ShadowBlur = font_map->m_ShadowBlur;
    config.m_BaseShadowAlpha = font_map->m_ShadowAlpha;
    config.m_BaseLayerMask = font_map->m_LayerMask;
    config.m_IsSdf = true;
    config.m_MaxVertexCount = num_vertices;
    config.m_FaceOnly = render_layer_mask == FACE;
    config.m_RenderDecorations = true;
    config.m_Transform = te.m_Transform;
    config.m_Width = te.m_Width;
    config.m_Height = te.m_Height;
    config.m_Align = te.m_Align;
    config.m_VerticalAlign = te.m_VAlign;
    config.m_MonospacePadding = font_map->m_IsMonospaced ? font_map->m_Padding : 0.0f;

    FontLayoutVertexMetrics metrics;
    if (!FontGetLayoutVertexMetrics(config, &metrics))
        return 0;
    if (metrics.m_Truncated)
        dmLogWarning("Character buffer exceeded (size: %u), increase the \"graphics.max_characters\" property in your game.project file.", num_vertices / 6);

    // Layer offsets come from the bounded shared plan, including span effects.
    // Missing glyphs leave transparent quads.
    memset(vertices, 0, metrics.m_VertexCount * sizeof(*vertices));
    uint32_t shadow_vertexindex = 0;
    uint32_t outline_vertexindex = metrics.m_ShadowQuadCount * 6;
    uint32_t face_vertexindex = (metrics.m_ShadowQuadCount + metrics.m_OutlineQuadCount) * 6;
    uint32_t glyph_slot = 0;
    TextGlyph* glyphs = TextLayoutGetGlyphs(layout);
    TextLine* lines = TextLayoutGetLines(layout);
    TextParagraph* paragraphs = TextLayoutGetParagraphs(layout);
    float layout_width, layout_height;
    TextLayoutGetBounds(layout, &layout_width, &layout_height);
    const float layout_y = OffsetLayoutY(te.m_VAlign, te.m_Height, layout_height);
    const uint32_t line_count = TextLayoutGetLineCount(layout);
    float sdf_atlas_width = 0.0f;
    float sdf_atlas_height = 0.0f;

    for (uint32_t i = 0; i < line_count; ++i)
    {
        const TextLine& line = lines[i];
        if (line.m_Length == 0)
            continue;
        float first_x = glyphs[line.m_Index].m_X;
        const float first_y = glyphs[line.m_Index].m_Y;
        for (uint32_t j = line.m_Index + 1; j < line.m_Index + line.m_Length; ++j)
            first_x = dmMath::Min(first_x, glyphs[j].m_X);
        uint32_t align = te.m_Align;
        if (paragraphs[line.m_ParagraphIndex].m_Direction == TEXT_DIRECTION_RTL && align != TEXT_ALIGN_CENTER)
            align = align == TEXT_ALIGN_LEFT ? TEXT_ALIGN_RIGHT : TEXT_ALIGN_LEFT;
        const float line_start_x = OffsetX(align, te.m_Width) - OffsetX(align, line.m_Width) - (font_map->m_IsMonospaced ? font_map->m_Padding * 0.5f : 0.0f);
        const float line_start_y = layout_y + line.m_Baseline;

        for (uint32_t gi = line.m_Index; gi < line.m_Index + line.m_Length; ++gi)
        {
            TextGlyph* g = &glyphs[gi];
            if ((g->m_Flags & TEXT_GLYPH_FLAG_OBJECT) || dmUtf8::IsWhiteSpace(g->m_Codepoint))
                continue;
            if (glyph_slot == metrics.m_GlyphQuadCount)
                continue;

            TextGlyphRenderData render_data;
            TextLayoutGetGlyphRenderData(layout, *g, config.m_FaceColor, &render_data);
            // The cached curves and effects use the font's reference size.
            const float font_scale = base_scale * g->m_RenderScale;
            TextGlyph scaled_glyph = *g;
            scaled_glyph.m_RenderScale = font_scale;
            FontGlyphLayerRenderData layer_data;
            FontResolveGlyphLayerRenderData(config, scaled_glyph, render_data, &layer_data);
            const bool emit_face = (render_layer_mask & FACE) != 0;
            const bool emit_outline = (render_layer_mask & layer_data.m_LayerMask & OUTLINE) != 0;
            const bool emit_shadow = (render_layer_mask & layer_data.m_LayerMask & SHADOW) != 0;
            const Vector4 outline_color = dmGraphics::UnpackRGBA(layer_data.m_OutlineColor);
            const Vector4 shadow_color = dmGraphics::UnpackRGBA(layer_data.m_ShadowColor);
            const float x = line_start_x + g->m_X - first_x + render_data.m_OffsetX;
            const float y = line_start_y + g->m_Y - first_y + render_data.m_OffsetY;
            const uint32_t glyph_shadow_vertexindex = shadow_vertexindex;
            const uint32_t glyph_outline_vertexindex = outline_vertexindex;
            const uint32_t glyph_face_vertexindex = face_vertexindex;
            shadow_vertexindex += (layer_data.m_LayerMask & SHADOW) != 0 ? 6 : 0;
            outline_vertexindex += (layer_data.m_LayerMask & OUTLINE) != 0 ? 6 : 0;
            face_vertexindex += 6;
            ++glyph_slot;

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
                continue;

            float glyph_width = dmMath::Max(0.0001f, glyph->m_Outline.m_Width * font_scale);
            // Baked banks add an atlas border around the generated glyph.
            // It belongs to neither the glyph's geometry nor its sampled area.
            const float bitmap_border = FontGetType(font) == FONT_TYPE_GLYPH_BANK ? font_map->m_CacheCellPadding : 0.0f;
            float sdf_width = glyph->m_Bitmap.m_Width > 0 ? (glyph->m_Bitmap.m_Width - 2.0f * bitmap_border) * font_scale : glyph_width;
            float sdf_left_bearing = glyph->m_LeftBearing * font_scale;
            float sdf_ascent = glyph->m_Ascent * font_scale;
            float sdf_descent = glyph->m_Descent * font_scale;
            // SDF outlines and bitmap shadows share the vector material.
            const bool use_sdf_effect = cache_glyph->m_VectorSdfCached && (emit_shadow || emit_outline);
            float sdf_u0 = 0.0f;
            float sdf_v0 = 0.0f;
            float sdf_u1 = 0.0f;
            float sdf_v1 = 0.0f;
            if (use_sdf_effect)
            {
                if (sdf_atlas_width == 0.0f)
                {
                    // Cache growth is applied before vertex generation, so these dimensions
                    // stay fixed for this text. Face-only text never needs the effect atlas.
                    sdf_atlas_width = (float)dmGraphics::GetTextureWidth(font_map->m_GraphicsContext, font_map->m_VectorSdfTexture);
                    sdf_atlas_height = (float)dmGraphics::GetTextureHeight(font_map->m_GraphicsContext, font_map->m_VectorSdfTexture);
                }
                // Use texel edges: pixel centers then map to texel centers
                // without stretching the generated field by a texel.
                sdf_u0 = (cache_glyph->m_X + bitmap_border) / sdf_atlas_width;
                sdf_v0 = (cache_glyph->m_Y + bitmap_border) / sdf_atlas_height;
                sdf_u1 = (cache_glyph->m_X + glyph->m_Bitmap.m_Width - bitmap_border) / sdf_atlas_width;
                sdf_v1 = (cache_glyph->m_Y + glyph->m_Bitmap.m_Height - bitmap_border) / sdf_atlas_height;
            }
            if (emit_shadow && cache_glyph->m_VectorSdfCached)
            {
                OutputGlyphVector(glyph_shadow_vertexindex,
                                  te.m_Transform,
                                  x,
                                  y,
                                  sdf_width,
                                  sdf_width,
                                  sdf_left_bearing,
                                  sdf_ascent,
                                  sdf_descent,
                                  cache_glyph->m_VectorCurveTexel,
                                  cache_glyph->m_VectorCurveCount,
                                  cache_glyph->m_VectorBanding,
                                  sdf_u0,
                                  sdf_v0,
                                  sdf_u1,
                                  sdf_v1,
                                  true,
                                  layer_data.m_SdfOutline,
                                  layer_data.m_ShadowX * ((render_data.m_StyleFlags & TEXT_RENDER_STYLE_SHADOW_X) ? 1.0f : font_scale),
                                  layer_data.m_ShadowY * ((render_data.m_StyleFlags & TEXT_RENDER_STYLE_SHADOW_Y) ? 1.0f : font_scale),
                                  2.0f,
                                  shadow_color,
                                  0,
                                  vertices);
            }

            if (emit_outline && cache_glyph->m_VectorSdfCached)
            {
                // Use the complete atlas entry, including padding for the outline threshold.
                OutputGlyphVector(glyph_outline_vertexindex,
                                  te.m_Transform,
                                  x,
                                  y,
                                  sdf_width,
                                  sdf_width,
                                  sdf_left_bearing,
                                  sdf_ascent,
                                  sdf_descent,
                                  cache_glyph->m_VectorCurveTexel,
                                  cache_glyph->m_VectorCurveCount,
                                  cache_glyph->m_VectorBanding,
                                  sdf_u0,
                                  sdf_v0,
                                  sdf_u1,
                                  sdf_v1,
                                  true,
                                  layer_data.m_SdfOutline,
                                  0.0f,
                                  0.0f,
                                  1.0f,
                                  outline_color,
                                  0,
                                  vertices);
            }

            if (emit_face)
            {
                uint32_t colors[4];
                FontPackGlyphFaceColors(render_data.m_FaceColors, colors);
                OutputGlyphVector(glyph_face_vertexindex,
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
                                  cache_glyph->m_VectorBanding,
                                  0.0f,
                                  0.0f,
                                  0.0f,
                                  0.0f,
                                  false,
                                  layer_data.m_SdfOutline,
                                  0.0f,
                                  0.0f,
                                  0.0f,
                                  face_color,
                                  colors,
                                  vertices);
            }
        }
    }

    if (metrics.m_DecorationQuadCount)
    {
        // Reuse the shared layout geometry for decorations. The glyph loop has
        // consumed its portion of each layer; only the decoration slots remain.
        FontLayoutVertexMetrics decoration_metrics = {};
        decoration_metrics.m_DecorationQuadCount = metrics.m_DecorationQuadCount;
        decoration_metrics.m_FaceQuadCount = metrics.m_DecorationQuadCount;
        decoration_metrics.m_ShadowQuadCount = metrics.m_ShadowQuadCount - shadow_vertexindex / 6;
        decoration_metrics.m_OutlineQuadCount = metrics.m_OutlineQuadCount - (outline_vertexindex / 6 - metrics.m_ShadowQuadCount);
        decoration_metrics.m_VertexCount = (decoration_metrics.m_FaceQuadCount + decoration_metrics.m_OutlineQuadCount + decoration_metrics.m_ShadowQuadCount) * 6;
        decoration_metrics.m_LayerCount = metrics.m_LayerCount;
        dmArray<FontGlyphVertex>& scratch = font_map->m_DecorationVertices;
        if (scratch.Capacity() < decoration_metrics.m_VertexCount)
            scratch.SetCapacity(decoration_metrics.m_VertexCount);
        scratch.SetSize(decoration_metrics.m_VertexCount);
        memset(scratch.Begin(), 0, scratch.Size() * sizeof(FontGlyphVertex));
        // Express the reference-size outline limit in layout units, matching
        // the scaled glyph used to resolve the vector glyph's effects above.
        FontLayoutVertexConfig decoration_config = config;
        decoration_config.m_OutlineWidth *= base_scale;
        decoration_config.m_SdfSpread *= base_scale;
        FontCreateLayoutVertices(decoration_config, decoration_metrics, scratch.Begin(), scratch.Size());
        const uint32_t counts[3] = { decoration_metrics.m_ShadowQuadCount * 6, decoration_metrics.m_OutlineQuadCount * 6, decoration_metrics.m_FaceQuadCount * 6 };
        const uint32_t offsets[3] = { shadow_vertexindex, outline_vertexindex, face_vertexindex };
        // Shared quads wind counterclockwise; vector glyphs wind clockwise.
        const uint32_t source_order[6] = { 0, 2, 1, 1, 2, 5 };
        uint32_t source_index = 0;
        for (uint32_t layer = 0; layer < 3; ++layer)
        {
            for (uint32_t i = 0; i < counts[layer]; ++i)
            {
                const FontGlyphVertex& source = scratch[source_index + i / 6 * 6 + source_order[i % 6]];
                GlyphVertex& destination = vertices[offsets[layer] + i];
                memcpy(destination.m_Position, source.m_Position, sizeof(source.m_Position));
                destination.m_Position[3] = source.m_Position[2];
                destination.m_VectorTexcoord[0] = source.m_LayerMasks[1];
                destination.m_VectorTexcoord[1] = dmMath::Max(0.0f, -source.m_LayerMasks[2]);
                destination.m_VectorTexcoord[3] = 3.0f; // Solid or dashed decoration.
                memcpy(destination.m_VectorColor, layer == 0 ? source.m_ShadowColor : layer == 1 ? source.m_OutlineColor : source.m_FaceColor, 4);
            }
            source_index += counts[layer];
        }
    }
    return metrics.m_VertexCount;
}

static uint32_t CreateFontVertexDataFromTextLayout(HFontMap font_map, uint32_t frame, HTextLayout layout, const TextEntry& te, uint8_t* _vertices, uint32_t num_vertices)
{
    assert(layout->m_FontCollection == GetFontCollection(font_map));
    assert(font_map->m_IsVector);

    return CreateFontVectorVertexData(font_map, frame, layout, te, (GlyphVertex*)_vertices, num_vertices);
}

#undef HAS_LAYER

uint32_t CreateFontVertexData(HFontMap font_map, uint32_t frame, const char* text, const TextEntry& te, float sdf_scale, float recip_w, float recip_h, uint8_t* _vertices, uint32_t num_vertices)
{
    DM_PROFILE(__FUNCTION__);

    if (!font_map->m_IsVector)
    {
        FontGlyphVertex* compact_vertices = (FontGlyphVertex*)_vertices;
        const uint32_t vertex_count = dmRender::CreateFontVertexData(font_map, frame, text, te, sdf_scale, recip_w, recip_h, compact_vertices, num_vertices);
        ExpandFontGlyphVertices(compact_vertices, vertex_count, (GlyphVertex*)_vertices);
        return vertex_count;
    }

    if (te.m_TextLayout)
    {
        return CreateFontVertexDataFromTextLayout(font_map, frame, te.m_TextLayout, te, _vertices, num_vertices);
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

    uint32_t vertex_count = CreateFontVertexDataFromTextLayout(font_map, frame, layout, te, _vertices, num_vertices);
    TextLayoutRelease(layout);
    return vertex_count;
}

} // namespace
