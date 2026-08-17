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

#define HAS_LAYER(mask, layer) (((mask) & (layer)) == (layer))

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

// Converts a unit RGBA color to the normalized byte format used by the GPU.
static void PackColor(uint8_t* packed, const float* color)
{
    packed[0] = (uint8_t)(color[0] * 255.0f);
    packed[1] = (uint8_t)(color[1] * 255.0f);
    packed[2] = (uint8_t)(color[2] * 255.0f);
    packed[3] = (uint8_t)(color[3] * 255.0f);
}

static void PackColor(uint8_t* packed, const Vector4& color)
{
    packed[0] = (uint8_t)(color[0] * 255.0f);
    packed[1] = (uint8_t)(color[1] * 255.0f);
    packed[2] = (uint8_t)(color[2] * 255.0f);
    packed[3] = (uint8_t)(color[3] * 255.0f);
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

// Packs a six-vertex glyph quad into each requested output layer. Face vertices
// are required; outline and shadow vertices are optional. Corner colors allow
// horizontal, vertical, and four-corner gradients without subdividing a glyph.
void FontPackGlyphVertices4ColorsToLayers(FontGlyph*                 glyph,
                                          float                      recip_w,
                                          float                      recip_h,
                                          uint32_t                   cell_x,
                                          uint32_t                   cell_y,
                                          uint32_t                   cache_cell_max_ascent,
                                          uint32_t                   cache_cell_padding,
                                          uint32_t                   layer_count,
                                          const dmVMath::Matrix4&    transform,
                                          float                      x,
                                          float                      y,
                                          float                      render_scale,
                                          const TextGlyphFaceColors& face_colors,
                                          const Vector4&             outline_color,
                                          const Vector4&             shadow_color,
                                          float                      sdf_edge_value,
                                          float                      sdf_outline,
                                          float                      sdf_smoothing,
                                          float                      sdf_shadow,
                                          float                      shadow_x,
                                          float                      shadow_y,
                                          bool                       metrics_from_ttf,
                                          FontGlyphVertex*           face_vertices,
                                          FontGlyphVertex*           outline_vertices,
                                          FontGlyphVertex*           shadow_vertices)
{
    assert(layer_count > 0 && layer_count <= FONT_GLYPH_MAX_LAYER_COUNT);
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

    uint8_t packed_outline_color[4];
    uint8_t packed_shadow_color[4];
    PackColor(packed_outline_color, outline_color);
    PackColor(packed_shadow_color, shadow_color);

#define SET_PROPERTIES(vertex, face_color) \
    PackColor(vertex.m_FaceColor, face_color); \
    memcpy(vertex.m_OutlineColor, packed_outline_color, sizeof(vertex.m_OutlineColor)); \
    memcpy(vertex.m_ShadowColor, packed_shadow_color, sizeof(vertex.m_ShadowColor)); \
    vertex.m_SdfParams[0] = sdf_edge_value; \
    vertex.m_SdfParams[1] = sdf_outline; \
    vertex.m_SdfParams[2] = sdf_smoothing; \
    vertex.m_SdfParams[3] = sdf_shadow;

    SET_PROPERTIES(face_1, face_colors.m_BottomLeft)
    SET_PROPERTIES(face_2, face_colors.m_BottomRight)
    SET_PROPERTIES(face_3, face_colors.m_TopLeft)
    SET_PROPERTIES(face_6, face_colors.m_TopRight)
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

// Packs a gradient-colored glyph into the renderer's layer-major vertex buffer.
// vertex_layer_stride is the distance between corresponding shadow, outline,
// and face ranges; vertex_index selects the glyph within each range.
void FontPackGlyphVertices4Colors(FontGlyph*                 glyph,
                                  float                      recip_w,
                                  float                      recip_h,
                                  uint32_t                   cell_x,
                                  uint32_t                   cell_y,
                                  uint32_t                   cache_cell_max_ascent,
                                  uint32_t                   cache_cell_padding,
                                  uint32_t                   layer_count,
                                  uint32_t                   layer_mask,
                                  uint32_t                   vertex_index,
                                  uint32_t                   vertex_layer_stride,
                                  const dmVMath::Matrix4&    transform,
                                  float                      x,
                                  float                      y,
                                  float                      render_scale,
                                  const TextGlyphFaceColors& face_colors,
                                  const Vector4&             outline_color,
                                  const Vector4&             shadow_color,
                                  float                      sdf_edge_value,
                                  float                      sdf_outline,
                                  float                      sdf_smoothing,
                                  float                      sdf_shadow,
                                  float                      shadow_x,
                                  float                      shadow_y,
                                  bool                       metrics_from_ttf,
                                  FontGlyphVertex*           vertices)
{
    FontGlyphVertex* face_vertices = vertices + vertex_index + vertex_layer_stride * (layer_count - 1);
    FontGlyphVertex* outline_vertices = HAS_LAYER(layer_mask, FONT_RENDER_LAYER_OUTLINE) ? vertices + vertex_index + vertex_layer_stride * (layer_count - 2) : 0;
    FontGlyphVertex* shadow_vertices = HAS_LAYER(layer_mask, FONT_RENDER_LAYER_SHADOW) ? vertices + vertex_index : 0;
    FontPackGlyphVertices4ColorsToLayers(glyph, recip_w, recip_h, cell_x, cell_y, cache_cell_max_ascent, cache_cell_padding, layer_count, transform, x, y, render_scale, face_colors, outline_color, shadow_color, sdf_edge_value, sdf_outline, sdf_smoothing, sdf_shadow, shadow_x, shadow_y, metrics_from_ttf, face_vertices, outline_vertices, shadow_vertices);
}

// Packs a glyph with one face color by forwarding that color to all four
// corners of FontPackGlyphVertices4Colors.
void FontPackGlyphVertices(FontGlyph*              glyph,
                           float                   recip_w,
                           float                   recip_h,
                           uint32_t                cell_x,
                           uint32_t                cell_y,
                           uint32_t                cache_cell_max_ascent,
                           uint32_t                cache_cell_padding,
                           uint32_t                layer_count,
                           uint32_t                layer_mask,
                           uint32_t                vertex_index,
                           uint32_t                vertex_layer_stride,
                           const dmVMath::Matrix4& transform,
                           float                   x,
                           float                   y,
                           float                   render_scale,
                           const Vector4&          face_color,
                           const Vector4&          outline_color,
                           const Vector4&          shadow_color,
                           float                   sdf_edge_value,
                           float                   sdf_outline,
                           float                   sdf_smoothing,
                           float                   sdf_shadow,
                           float                   shadow_x,
                           float                   shadow_y,
                           bool                    metrics_from_ttf,
                           FontGlyphVertex*        vertices)
{
    TextGlyphFaceColors face_colors;

    for (uint32_t i = 0; i < 4; ++i)
    {
        face_colors.m_BottomLeft[i] = face_color[i];
        face_colors.m_BottomRight[i] = face_color[i];
        face_colors.m_TopLeft[i] = face_color[i];
        face_colors.m_TopRight[i] = face_color[i];
    }

    FontPackGlyphVertices4Colors(glyph, recip_w, recip_h, cell_x, cell_y, cache_cell_max_ascent, cache_cell_padding, layer_count, layer_mask, vertex_index, vertex_layer_stride, transform, x, y, render_scale, face_colors, outline_color, shadow_color, sdf_edge_value, sdf_outline, sdf_smoothing, sdf_shadow, shadow_x, shadow_y, metrics_from_ttf, vertices);
}

// Packs one line-decoration quad around the local-space center line from
// (x0, y0) to (x1, y1). The quad samples a single opaque atlas texel and renders
// only in the face layer. Pattern start/end are repeating shader coordinates;
// pattern_duty is the visible fraction of a cycle, or zero for a solid line.
// Hidden copies keep the layer-major vertex ranges aligned with glyph quads.
void FontPackDecorationVertices(float                      texture_u,
                                float                      texture_v,
                                uint32_t                   layer_count,
                                uint32_t                   vertex_index,
                                uint32_t                   vertex_layer_stride,
                                const dmVMath::Matrix4&    transform,
                                float                      x0,
                                float                      y0,
                                float                      x1,
                                float                      y1,
                                float                      thickness,
                                float                      pattern_start,
                                float                      pattern_end,
                                float                      pattern_duty,
                                const TextGlyphFaceColors& face_colors,
                                FontGlyphVertex*           vertices)
{
    const uint32_t face_index = vertex_index + vertex_layer_stride * (layer_count - 1);
    FontGlyphVertex* face = vertices + face_index;
    const float delta_x = x1 - x0;
    const float delta_y = y1 - y0;
    const float length = sqrtf(delta_x * delta_x + delta_y * delta_y);
    const float normal_scale = length > 0.0f ? thickness * 0.5f / length : 0.0f;
    const float normal_x = -delta_y * normal_scale;
    const float normal_y = delta_x * normal_scale;
    QuadPositions positions;
    CalculateQuadPositions(transform, x0 - normal_x, y0 - normal_y, delta_x, delta_y, normal_x * 2.0f, normal_y * 2.0f, &positions);
    SetQuadPositions(face, positions);
    const float* colors[4] = { face_colors.m_BottomLeft, face_colors.m_BottomRight, face_colors.m_TopLeft, face_colors.m_TopRight };
    const uint32_t corners[4] = { 0, 1, 2, 5 };
    const uint16_t packed_texture_u = FontPackGlyphUV(texture_u);
    const uint16_t packed_texture_v = FontPackGlyphUV(texture_v);

    for (uint32_t i = 0; i < 4; ++i)
    {
        FontGlyphVertex& vertex = face[corners[i]];
        vertex.m_UV[0] = packed_texture_u;
        vertex.m_UV[1] = packed_texture_v;
        PackColor(vertex.m_FaceColor, colors[i]);
        memset(vertex.m_OutlineColor, 0, sizeof(vertex.m_OutlineColor));
        memset(vertex.m_ShadowColor, 0, sizeof(vertex.m_ShadowColor));
        vertex.m_SdfParams[0] = 0.75f;
        vertex.m_SdfParams[1] = 0.75f;
        vertex.m_SdfParams[2] = 0.01f;
        vertex.m_SdfParams[3] = 0.75f;
        // Decorations only render in the face layer. A negative third component
        // marks a procedural dash; the shaders decode the second component as
        // the interpolated position in cycles and negate the third into duty.
        vertex.m_LayerMasks[0] = 1.0f;
        vertex.m_LayerMasks[1] = i == 0 || i == 2 ? pattern_start : pattern_end;
        vertex.m_LayerMasks[2] = -pattern_duty;
    }

    face[3] = face[2];
    face[4] = face[1];

    for (uint32_t layer = 0; layer + 1 < layer_count; ++layer)
    {
        FontGlyphVertex* hidden = vertices + vertex_index + vertex_layer_stride * layer;
        memcpy(hidden, face, sizeof(FontGlyphVertex) * FONT_GLYPH_VERTICES_PER_QUAD);

        for (uint32_t i = 0; i < FONT_GLYPH_VERTICES_PER_QUAD; ++i)
        {
            memset(hidden[i].m_LayerMasks, 0, sizeof(hidden[i].m_LayerMasks));
        }
    }
}

#undef HAS_LAYER
