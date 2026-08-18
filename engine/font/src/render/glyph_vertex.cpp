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

#include "glyph_vertex.h"

#include "../text_layout.h"

#include <assert.h>
#include <math.h>
#include <string.h>

using dmVMath::Vector4;

// A glyph has one six-vertex quad for each possible layer: shadow, outline, and face.
static const uint32_t FONT_GLYPH_VERTICES_PER_QUAD = 6;
static const uint32_t FONT_GLYPH_MAX_LAYER_COUNT = 3;

static void SetPosition(float* position, const Vector4& value)
{
    position[0] = value[0];
    position[1] = value[1];
    position[2] = value[2];
}

struct QuadPositions
{
    Vector4 m_BottomLeft;
    Vector4 m_BottomRight;
    Vector4 m_TopLeft;
    Vector4 m_TopRight;
};

// Transforms one quad origin and derives the other corners from its two edges.
static void CalculateQuadPositions(const dmVMath::Matrix4& transform,
                                   float                   origin_x,
                                   float                   origin_y,
                                   float                   edge_x_x,
                                   float                   edge_x_y,
                                   float                   edge_y_x,
                                   float                   edge_y_y,
                                   QuadPositions*          positions)
{
    const Vector4 transformed_edge_x = transform.getCol0() * edge_x_x + transform.getCol1() * edge_x_y;
    const Vector4 transformed_edge_y = transform.getCol0() * edge_y_x + transform.getCol1() * edge_y_y;
    positions->m_BottomLeft = transform * Vector4(origin_x, origin_y, 0.0f, 1.0f);
    positions->m_BottomRight = positions->m_BottomLeft + transformed_edge_x;
    positions->m_TopLeft = positions->m_BottomLeft + transformed_edge_y;
    positions->m_TopRight = positions->m_BottomRight + transformed_edge_y;
}

// Writes the four unique corners used by a six-vertex triangle-list quad.
static void SetQuadPositions(FontGlyphVertex* vertices, const QuadPositions& positions)
{
    SetPosition(vertices[0].m_Position, positions.m_BottomLeft);
    SetPosition(vertices[1].m_Position, positions.m_BottomRight);
    SetPosition(vertices[2].m_Position, positions.m_TopLeft);
    SetPosition(vertices[5].m_Position, positions.m_TopRight);
}

// Converts a unit RGBA color to four native-order bytes in a uint32_t.
static uint32_t PackColor(const float* color)
{
    const uint8_t r = (uint8_t)(color[0] * 255.0f);
    const uint8_t g = (uint8_t)(color[1] * 255.0f);
    const uint8_t b = (uint8_t)(color[2] * 255.0f);
    const uint8_t a = (uint8_t)(color[3] * 255.0f);

#if DM_ENDIAN == DM_ENDIAN_LITTLE
    return (uint32_t)a << 24 | (uint32_t)b << 16 | (uint32_t)g << 8 | r;
#else
    return (uint32_t)r << 24 | (uint32_t)g << 16 | (uint32_t)b << 8 | a;
#endif
}

static void SetColor(uint8_t* destination, uint32_t color)
{
    memcpy(destination, &color, sizeof(color));
}

void FontPackGlyphFaceColors(const TextGlyphFaceColors& face_colors, uint32_t packed_colors[4])
{
    packed_colors[0] = PackColor(face_colors.m_BottomLeft);
    packed_colors[1] = PackColor(face_colors.m_BottomRight);
    packed_colors[2] = PackColor(face_colors.m_TopLeft);
    packed_colors[3] = PackColor(face_colors.m_TopRight);
}

// Packs an atlas texel coordinate using a pre-scaled reciprocal dimension.
static uint16_t PackTexelCoordinate(float coordinate, float packed_reciprocal)
{
    return (uint16_t)(coordinate * packed_reciprocal + 0.5f);
}

// A decoration can use one quad when its color is constant or varies only
// across the complete span. Split it at glyph boundaries when glyphs refer to
// different styles/spans, or when a glyph-fitted gradient must be preserved.
bool FontDecorationRequiresGlyphSegments(HTextLayout layout, const TextDecoration& decoration)
{
    TextLayout* internal = (TextLayout*)layout;

    if (decoration.m_GlyphCount <= 1)
    {
        return false;
    }

    const TextGlyph* glyphs = TextLayoutGetGlyphs(layout);
    const TextGlyph& first = glyphs[decoration.m_GlyphStart];

    for (uint32_t i = 1; i < decoration.m_GlyphCount; ++i)
    {
        const TextGlyph& glyph = glyphs[decoration.m_GlyphStart + i];

        if (glyph.m_StyleIndex != first.m_StyleIndex || glyph.m_MarkupSpanIndex != first.m_MarkupSpanIndex)
        {
            return true;
        }
    }

    if (first.m_MarkupSpanIndex == UINT16_MAX || first.m_MarkupSpanIndex >= internal->m_ResolvedSpans.Size())
    {
        return false;
    }

    const TextResolvedSpan& span = internal->m_ResolvedSpans[first.m_MarkupSpanIndex];

    for (uint32_t i = 0; i < span.m_EffectCount; ++i)
    {
        const TextEffect& effect = internal->m_Effects[internal->m_SpanEffects[span.m_EffectIndex + i]];

        if (effect.m_Type == TEXT_EFFECT_GRADIENT && effect.m_Gradient.m_Fit == TEXT_EFFECT_FIT_GLYPH)
        {
            return true;
        }
    }

    return false;
}

// Returns the number of six-vertex quads needed for the decoration. Most
// decorations use one quad for the complete span.
uint32_t FontGetDecorationQuadCount(HTextLayout layout, const TextDecoration& decoration)
{
    return FontDecorationRequiresGlyphSegments(layout, decoration) ? decoration.m_GlyphCount : 1;
}

// Converts the dashed pattern from layout units to repeating shader
// coordinates. Start and end are measured in cycles and duty is the visible
// fraction of each cycle. A zero duty identifies a solid decoration.
void FontGetDecorationPattern(const TextDecoration& decoration, uint32_t segment_index, uint32_t segment_count, FontDecorationPattern* pattern)
{
    pattern->m_Start = 0.0f;
    pattern->m_End = 0.0f;
    pattern->m_Duty = 0.0f;

    if (decoration.m_Pattern != TEXT_DECORATION_PATTERN_DASHED)
    {
        return;
    }

    const float segment_length = decoration.m_Length / segment_count;
    const float dash = fmaxf(1.0f, decoration.m_Thickness * 3.0f);
    const float gap = fmaxf(1.0f, decoration.m_Thickness * 2.0f);
    const float cycle = dash + gap;
    const float pattern_position = decoration.m_PatternOffset + segment_length * segment_index;
    pattern->m_Start = pattern_position / cycle;
    pattern->m_End = (pattern_position + segment_length) / cycle;
    pattern->m_Duty = dash / cycle;
}

// Resolves the colors at the geometrical ends of a decoration. Looking up the
// leftmost and rightmost glyph, rather than the first and last logical glyph,
// also produces the correct gradient direction for right-to-left text.
void FontGetDecorationFaceColors(HTextLayout layout, const TextDecoration& decoration, const float base_color[4], TextGlyphFaceColors* face_colors)
{
    const TextGlyph* glyphs = TextLayoutGetGlyphs(layout);
    uint32_t left_index = decoration.m_GlyphStart;
    uint32_t right_index = left_index;

    for (uint32_t i = 1; i < decoration.m_GlyphCount; ++i)
    {
        const uint32_t glyph_index = decoration.m_GlyphStart + i;

        if (glyphs[glyph_index].m_X < glyphs[left_index].m_X)
        {
            left_index = glyph_index;
        }

        if (glyphs[glyph_index].m_X + glyphs[glyph_index].m_Width > glyphs[right_index].m_X + glyphs[right_index].m_Width)
        {
            right_index = glyph_index;
        }
    }

    TextGlyphRenderData left;
    TextGlyphRenderData right;
    TextLayoutGetGlyphRenderData(layout, glyphs[left_index], base_color, &left);
    TextLayoutGetGlyphRenderData(layout, glyphs[right_index], base_color, &right);
    memcpy(face_colors->m_BottomLeft, left.m_FaceColors.m_BottomLeft, sizeof(face_colors->m_BottomLeft));
    memcpy(face_colors->m_TopLeft, left.m_FaceColors.m_TopLeft, sizeof(face_colors->m_TopLeft));
    memcpy(face_colors->m_BottomRight, right.m_FaceColors.m_BottomRight, sizeof(face_colors->m_BottomRight));
    memcpy(face_colors->m_TopRight, right.m_FaceColors.m_TopRight, sizeof(face_colors->m_TopRight));
}

// Packs a six-vertex glyph quad into each requested output layer. Corner colors
// allow horizontal, vertical, and four-corner gradients without subdivision.
void FontPackGlyphVertices(const FontGlyphVertexParams& params, const FontVertexLayerParams& layers)
{
    assert(layers.m_Transform);

    FontGlyph*              glyph = params.m_Glyph;
    const float             recip_w = params.m_RecipAtlasWidth;
    const float             recip_h = params.m_RecipAtlasHeight;
    const uint32_t          cell_x = params.m_CellX;
    const uint32_t          cell_y = params.m_CellY;
    const uint32_t          cache_cell_max_ascent = params.m_CacheCellMaxAscent;
    const uint32_t          cache_cell_padding = params.m_CacheCellPadding;
    const uint32_t          layer_count = layers.m_LayerCount;
    const dmVMath::Matrix4& transform = *layers.m_Transform;
    const float             x = params.m_X;
    const float             y = params.m_Y;
    const float             render_scale = params.m_RenderScale;
    const uint32_t*         face_colors = layers.m_FaceColors;
    const uint32_t          outline_color = layers.m_OutlineColor;
    const uint32_t          shadow_color = layers.m_ShadowColor;
    const float             sdf_edge_value = layers.m_SdfEdge;
    const float             sdf_outline = layers.m_SdfOutline;
    const float             sdf_smoothing = layers.m_SdfSmoothing;
    const float             sdf_shadow = layers.m_SdfShadow;
    const float             shadow_x = layers.m_ShadowX;
    const float             shadow_y = layers.m_ShadowY;
    const bool              metrics_from_ttf = params.m_MetricsFromTtf;
    FontGlyphVertex*        face_vertices = layers.m_FaceVertices;
    FontGlyphVertex*        outline_vertices = layers.m_OutlineVertices;
    FontGlyphVertex*        shadow_vertices = layers.m_ShadowVertices;

    assert(layer_count > 0 && layer_count <= FONT_GLYPH_MAX_LAYER_COUNT);
    assert(face_colors);
    assert(face_vertices);

    float source_width;
    float source_descent;
    float source_ascent;
    float source_left_bearing;
    float source_size_difference;
    if (glyph)
    {
        assert(glyph->m_Bitmap.m_Width != 0);
        source_width = metrics_from_ttf ? glyph->m_Bitmap.m_Width : glyph->m_Width;
        source_descent = glyph->m_Descent;
        source_ascent = glyph->m_Ascent;
        source_left_bearing = glyph->m_LeftBearing;
        source_size_difference = source_width - glyph->m_Width;
    }
    else
    {
        source_width = 0.0f;
        source_descent = 0.0f;
        source_ascent = 0.0f;
        source_left_bearing = 0.0f;
        source_size_difference = 0.0f;
    }

    const int16_t    width = (int16_t)source_width;
    const int16_t    descent = (int16_t)source_descent;
    const int16_t    ascent = (int16_t)source_ascent;
    const int16_t    cell_offset_y = cache_cell_max_ascent - ascent;
    const float      glyph_width = source_width * render_scale;
    const float      glyph_descent = source_descent * render_scale;
    const float      glyph_ascent = source_ascent * render_scale;
    const float      glyph_left_bearing = source_left_bearing * render_scale;
    const float      local_x = x - source_size_difference * render_scale * 0.5f;

    FontGlyphVertex& face_1 = face_vertices[0];
    FontGlyphVertex& face_2 = face_vertices[1];
    FontGlyphVertex& face_3 = face_vertices[2];
    FontGlyphVertex& face_4 = face_vertices[3];
    FontGlyphVertex& face_5 = face_vertices[4];
    FontGlyphVertex& face_6 = face_vertices[5];

    QuadPositions face_positions;
    CalculateQuadPositions(transform, local_x + glyph_left_bearing, y - glyph_descent, glyph_width, 0.0f, 0.0f, glyph_ascent + glyph_descent, &face_positions);
    SetQuadPositions(face_vertices, face_positions);

    const float packed_recip_w = recip_w * 65535.0f;
    const float packed_recip_h = recip_h * 65535.0f;
    face_1.m_UV[0] = PackTexelCoordinate(cell_x + cache_cell_padding, packed_recip_w);
    face_1.m_UV[1] = PackTexelCoordinate(cell_y + cache_cell_padding + ascent + descent + cell_offset_y, packed_recip_h);
    face_2.m_UV[0] = PackTexelCoordinate(cell_x + cache_cell_padding + width, packed_recip_w);
    face_2.m_UV[1] = face_1.m_UV[1];
    face_3.m_UV[0] = face_1.m_UV[0];
    face_3.m_UV[1] = PackTexelCoordinate(cell_y + cache_cell_padding + cell_offset_y, packed_recip_h);
    face_6.m_UV[0] = face_2.m_UV[0];
    face_6.m_UV[1] = face_3.m_UV[1];

#define SET_PROPERTIES(vertex, face_color) \
    SetColor(vertex.m_FaceColor, face_color); \
    SetColor(vertex.m_OutlineColor, outline_color); \
    SetColor(vertex.m_ShadowColor, shadow_color); \
    vertex.m_SdfParams[0] = sdf_edge_value; \
    vertex.m_SdfParams[1] = sdf_outline; \
    vertex.m_SdfParams[2] = sdf_smoothing; \
    vertex.m_SdfParams[3] = sdf_shadow;

    SET_PROPERTIES(face_1, face_colors[0])
    SET_PROPERTIES(face_2, face_colors[1])
    SET_PROPERTIES(face_3, face_colors[2])
    SET_PROPERTIES(face_6, face_colors[3])
#undef SET_PROPERTIES

    face_4 = face_3;
    face_5 = face_2;

#define SET_MASK(vertex, face, outline, shadow) \
    vertex.m_LayerMasks[0] = face; \
    vertex.m_LayerMasks[1] = outline; \
    vertex.m_LayerMasks[2] = shadow;

    if (outline_vertices)
    {
        for (uint32_t i = 0; i < FONT_GLYPH_VERTICES_PER_QUAD; ++i)
        {
            outline_vertices[i] = face_vertices[i];
            SET_MASK(outline_vertices[i], 0, 1, 0)
        }
    }

    if (shadow_vertices)
    {
        for (uint32_t i = 0; i < FONT_GLYPH_VERTICES_PER_QUAD; ++i)
            shadow_vertices[i] = face_vertices[i];

        FontGlyphVertex& shadow_4 = shadow_vertices[3];
        FontGlyphVertex& shadow_5 = shadow_vertices[4];
        const Vector4    shadow_offset = transform.getCol0() * shadow_x + transform.getCol1() * shadow_y;
        QuadPositions    shadow_positions;
        shadow_positions.m_BottomLeft = face_positions.m_BottomLeft + shadow_offset;
        shadow_positions.m_BottomRight = face_positions.m_BottomRight + shadow_offset;
        shadow_positions.m_TopLeft = face_positions.m_TopLeft + shadow_offset;
        shadow_positions.m_TopRight = face_positions.m_TopRight + shadow_offset;
        SetQuadPositions(shadow_vertices, shadow_positions);
        shadow_4 = shadow_vertices[2];
        shadow_5 = shadow_vertices[1];
        for (uint32_t i = 0; i < FONT_GLYPH_VERTICES_PER_QUAD; ++i)
        {
            SET_MASK(shadow_vertices[i], 0, 0, 1)
        }
    }

    const uint8_t one_layer = layer_count == 1 ? 1 : 0;
    for (uint32_t i = 0; i < FONT_GLYPH_VERTICES_PER_QUAD; ++i)
    {
        SET_MASK(face_vertices[i], 1, one_layer, one_layer)
    }

#undef SET_MASK
}

// Packs one line-decoration quad around the local-space center line from
// (x0, y0) to (x1, y1). The quad samples a single opaque atlas texel in every
// requested layer. Pattern start/end are repeating shader coordinates;
// pattern_duty is the visible fraction of a cycle, or zero for a solid line.
void FontPackDecorationVertices(const FontDecorationVertexParams& params, const FontVertexLayerParams& layers)
{
    assert(layers.m_Transform);

    const float             texture_u = params.m_TextureU;
    const float             texture_v = params.m_TextureV;
    const uint32_t          layer_count = layers.m_LayerCount;
    const dmVMath::Matrix4& transform = *layers.m_Transform;
    const float             x0 = params.m_X0;
    const float             y0 = params.m_Y0;
    const float             x1 = params.m_X1;
    const float             y1 = params.m_Y1;
    const float             thickness = params.m_Thickness;
    const float             pattern_start = params.m_PatternStart;
    const float             pattern_end = params.m_PatternEnd;
    const float             pattern_duty = params.m_PatternDuty;
    const uint32_t*         face_colors = layers.m_FaceColors;
    const uint32_t          outline_color = layers.m_OutlineColor;
    const uint32_t          shadow_color = layers.m_ShadowColor;
    const float             sdf_edge_value = layers.m_SdfEdge;
    const float             sdf_outline = layers.m_SdfOutline;
    const float             sdf_smoothing = layers.m_SdfSmoothing;
    const float             sdf_shadow = layers.m_SdfShadow;
    const float             outline_width = params.m_OutlineWidth;
    const float             shadow_x = layers.m_ShadowX;
    const float             shadow_y = layers.m_ShadowY;
    FontGlyphVertex*        face_vertices = layers.m_FaceVertices;
    FontGlyphVertex*        outline_vertices = layers.m_OutlineVertices;
    FontGlyphVertex*        shadow_vertices = layers.m_ShadowVertices;

    assert(layer_count > 0 && layer_count <= FONT_GLYPH_MAX_LAYER_COUNT);
    assert(face_colors);
    assert(face_vertices);

    const float   delta_x = x1 - x0;
    const float   delta_y = y1 - y0;
    const float   length = sqrtf(delta_x * delta_x + delta_y * delta_y);
    const float   normal_scale = length > 0.0f ? thickness * 0.5f / length : 0.0f;
    const float   normal_x = -delta_y * normal_scale;
    const float   normal_y = delta_x * normal_scale;
    QuadPositions positions;
    CalculateQuadPositions(transform, x0 - normal_x, y0 - normal_y, delta_x, delta_y, normal_x * 2.0f, normal_y * 2.0f, &positions);
    SetQuadPositions(face_vertices, positions);
    const uint32_t corners[4] = { 0, 1, 2, 5 };
    const uint16_t packed_texture_u = FontPackGlyphUV(texture_u);
    const uint16_t packed_texture_v = FontPackGlyphUV(texture_v);

    for (uint32_t i = 0; i < 4; ++i)
    {
        FontGlyphVertex& vertex = face_vertices[corners[i]];
        vertex.m_UV[0] = packed_texture_u;
        vertex.m_UV[1] = packed_texture_v;
        SetColor(vertex.m_FaceColor, face_colors[i]);
        SetColor(vertex.m_OutlineColor, outline_color);
        SetColor(vertex.m_ShadowColor, shadow_color);
        vertex.m_SdfParams[0] = sdf_edge_value;
        vertex.m_SdfParams[1] = sdf_outline;
        vertex.m_SdfParams[2] = sdf_smoothing;
        vertex.m_SdfParams[3] = sdf_shadow;
    }

    face_vertices[3] = face_vertices[2];
    face_vertices[4] = face_vertices[1];

    if (outline_vertices)
    {
        memcpy(outline_vertices, face_vertices, sizeof(FontGlyphVertex) * FONT_GLYPH_VERTICES_PER_QUAD);

        const float tangent_x = length > 0.0f ? delta_x / length : 0.0f;
        const float tangent_y = length > 0.0f ? delta_y / length : 0.0f;
        const float outline_normal_scale = length > 0.0f ? (thickness * 0.5f + outline_width) / length : 0.0f;
        const float outline_normal_x = -delta_y * outline_normal_scale;
        const float outline_normal_y = delta_x * outline_normal_scale;
        QuadPositions outline_positions;
        CalculateQuadPositions(transform,
                               x0 - tangent_x * outline_width - outline_normal_x,
                               y0 - tangent_y * outline_width - outline_normal_y,
                               delta_x + tangent_x * outline_width * 2.0f,
                               delta_y + tangent_y * outline_width * 2.0f,
                               outline_normal_x * 2.0f,
                               outline_normal_y * 2.0f,
                               &outline_positions);
        SetQuadPositions(outline_vertices, outline_positions);
        outline_vertices[3] = outline_vertices[2];
        outline_vertices[4] = outline_vertices[1];
    }

    if (shadow_vertices)
    {
        memcpy(shadow_vertices, face_vertices, sizeof(FontGlyphVertex) * FONT_GLYPH_VERTICES_PER_QUAD);
        const Vector4 shadow_offset = transform.getCol0() * shadow_x + transform.getCol1() * shadow_y;
        QuadPositions shadow_positions;
        shadow_positions.m_BottomLeft = positions.m_BottomLeft + shadow_offset;
        shadow_positions.m_BottomRight = positions.m_BottomRight + shadow_offset;
        shadow_positions.m_TopLeft = positions.m_TopLeft + shadow_offset;
        shadow_positions.m_TopRight = positions.m_TopRight + shadow_offset;
        SetQuadPositions(shadow_vertices, shadow_positions);
        shadow_vertices[3] = shadow_vertices[2];
        shadow_vertices[4] = shadow_vertices[1];
    }

    const float cycles_per_unit = length > 0.0f ? (pattern_end - pattern_start) / length : 0.0f;
    const float layer_pattern_start[3] = { pattern_start, pattern_start - outline_width * cycles_per_unit, pattern_start };
    const float layer_pattern_end[3] = { pattern_end, pattern_end + outline_width * cycles_per_unit, pattern_end };
    FontGlyphVertex* layer_vertices[3] = { face_vertices, outline_vertices, shadow_vertices };
    const float      solid_masks[3][3] = {
        { 1.0f, layer_count == 1 ? 1.0f : 0.0f, layer_count == 1 ? 1.0f : 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f },
    };

    for (uint32_t layer = 0; layer < 3; ++layer)
    {
        FontGlyphVertex* output = layer_vertices[layer];

        if (!output)
        {
            continue;
        }

        for (uint32_t i = 0; i < FONT_GLYPH_VERTICES_PER_QUAD; ++i)
        {
            if (pattern_duty > 0.0f)
            {
                // Dashed decorations use x for the target layer, y for the
                // interpolated position in cycles, and negative z for duty.
                output[i].m_LayerMasks[0] = (float)(layer + 1);
                output[i].m_LayerMasks[1] = i == 0 || i == 2 || i == 3 ? layer_pattern_start[layer] : layer_pattern_end[layer];
                output[i].m_LayerMasks[2] = -pattern_duty;
            }
            else
            {
                memcpy(output[i].m_LayerMasks, solid_masks[layer], sizeof(output[i].m_LayerMasks));
            }
        }
    }
}
