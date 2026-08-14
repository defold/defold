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

bool FontDecorationRequiresGlyphSegments(HTextLayout layout, const TextDecoration& decoration)
{
    TextLayout* internal = (TextLayout*)layout;
    if (decoration.m_GlyphCount <= 1)
        return false;
    const TextGlyph* glyphs = TextLayoutGetGlyphs(layout);
    const TextGlyph& first = glyphs[decoration.m_GlyphStart];
    for (uint32_t i = 1; i < decoration.m_GlyphCount; ++i)
    {
        const TextGlyph& glyph = glyphs[decoration.m_GlyphStart + i];
        if (glyph.m_StyleIndex != first.m_StyleIndex || glyph.m_MarkupSpanIndex != first.m_MarkupSpanIndex)
            return true;
    }
    if (first.m_MarkupSpanIndex == UINT16_MAX || first.m_MarkupSpanIndex >= internal->m_ResolvedSpans.Size())
        return false;
    const TextResolvedSpan& span = internal->m_ResolvedSpans[first.m_MarkupSpanIndex];
    for (uint32_t i = 0; i < span.m_EffectCount; ++i)
    {
        const TextEffect& effect = internal->m_Effects[internal->m_SpanEffects[span.m_EffectIndex + i]];
        if (effect.m_Type == TEXT_EFFECT_GRADIENT && effect.m_Gradient.m_Fit == TEXT_EFFECT_FIT_GLYPH)
            return true;
    }
    return false;
}

uint32_t FontGetDecorationQuadCount(HTextLayout layout, const TextDecoration& decoration)
{
    return FontDecorationRequiresGlyphSegments(layout, decoration) ? decoration.m_GlyphCount : 1;
}

void FontGetDecorationPattern(const TextDecoration& decoration, uint32_t segment_index, uint32_t segment_count, FontDecorationPattern* pattern)
{
    pattern->m_Start = 0.0f;
    pattern->m_End = 0.0f;
    pattern->m_Duty = 0.0f;
    if (decoration.m_Pattern != TEXT_DECORATION_PATTERN_DASHED)
        return;
    const float segment_length = decoration.m_Length / segment_count;
    const float dash = fmaxf(1.0f, decoration.m_Thickness * 3.0f);
    const float gap = fmaxf(1.0f, decoration.m_Thickness * 2.0f);
    const float cycle = dash + gap;
    const float pattern_position = decoration.m_PatternOffset + segment_length * segment_index;
    pattern->m_Start = pattern_position / cycle;
    pattern->m_End = (pattern_position + segment_length) / cycle;
    pattern->m_Duty = dash / cycle;
}

void FontGetDecorationFaceColors(HTextLayout layout, const TextDecoration& decoration, const float base_color[4], TextGlyphFaceColors* face_colors)
{
    const TextGlyph* glyphs = TextLayoutGetGlyphs(layout);
    uint32_t left_index = decoration.m_GlyphStart;
    uint32_t right_index = left_index;
    for (uint32_t i = 1; i < decoration.m_GlyphCount; ++i)
    {
        const uint32_t glyph_index = decoration.m_GlyphStart + i;
        if (glyphs[glyph_index].m_X < glyphs[left_index].m_X)
            left_index = glyph_index;
        if (glyphs[glyph_index].m_X + glyphs[glyph_index].m_Width > glyphs[right_index].m_X + glyphs[right_index].m_Width)
            right_index = glyph_index;
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
    assert(layer_count > 0 && layer_count <= FONT_GLYPH_MAX_LAYER_COUNT);

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
    const uint32_t   face_index = vertex_index + vertex_layer_stride * (layer_count - 1);
    const float      glyph_width = source_width * render_scale;
    const float      glyph_descent = source_descent * render_scale;
    const float      glyph_ascent = source_ascent * render_scale;
    const float      glyph_left_bearing = source_left_bearing * render_scale;
    const float      local_x = x - source_size_difference * render_scale * 0.5f;

    FontGlyphVertex& face_1 = vertices[face_index + 0];
    FontGlyphVertex& face_2 = vertices[face_index + 1];
    FontGlyphVertex& face_3 = vertices[face_index + 2];
    FontGlyphVertex& face_4 = vertices[face_index + 3];
    FontGlyphVertex& face_5 = vertices[face_index + 4];
    FontGlyphVertex& face_6 = vertices[face_index + 5];

    SetPosition(face_1.m_Position, transform * Vector4(local_x + glyph_left_bearing, y - glyph_descent, 0, 1));
    SetPosition(face_2.m_Position, transform * Vector4(local_x + glyph_left_bearing + glyph_width, y - glyph_descent, 0, 1));
    SetPosition(face_3.m_Position, transform * Vector4(local_x + glyph_left_bearing, y + glyph_ascent, 0, 1));
    SetPosition(face_6.m_Position, transform * Vector4(local_x + glyph_left_bearing + glyph_width, y + glyph_ascent, 0, 1));

    face_1.m_UV[0] = (cell_x + cache_cell_padding) * recip_w;
    face_1.m_UV[1] = (cell_y + cache_cell_padding + ascent + descent + cell_offset_y) * recip_h;
    face_2.m_UV[0] = (cell_x + cache_cell_padding + width) * recip_w;
    face_2.m_UV[1] = face_1.m_UV[1];
    face_3.m_UV[0] = face_1.m_UV[0];
    face_3.m_UV[1] = (cell_y + cache_cell_padding + cell_offset_y) * recip_h;
    face_6.m_UV[0] = face_2.m_UV[0];
    face_6.m_UV[1] = face_3.m_UV[1];

#define SET_PROPERTIES(vertex, face_color) \
    vertex.m_FaceColor[0] = face_color[0]; \
    vertex.m_FaceColor[1] = face_color[1]; \
    vertex.m_FaceColor[2] = face_color[2]; \
    vertex.m_FaceColor[3] = face_color[3]; \
    vertex.m_OutlineColor[0] = outline_color[0]; \
    vertex.m_OutlineColor[1] = outline_color[1]; \
    vertex.m_OutlineColor[2] = outline_color[2]; \
    vertex.m_OutlineColor[3] = outline_color[3]; \
    vertex.m_ShadowColor[0] = shadow_color[0]; \
    vertex.m_ShadowColor[1] = shadow_color[1]; \
    vertex.m_ShadowColor[2] = shadow_color[2]; \
    vertex.m_ShadowColor[3] = shadow_color[3]; \
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

    if (HAS_LAYER(layer_mask, FONT_RENDER_LAYER_OUTLINE))
    {
        const uint32_t outline_index = vertex_index + vertex_layer_stride * (layer_count - 2);
        for (uint32_t i = 0; i < FONT_GLYPH_VERTICES_PER_QUAD; ++i)
        {
            vertices[outline_index + i] = vertices[face_index + i];
            SET_MASK(vertices[outline_index + i], 0, 1, 0)
        }
    }

    if (HAS_LAYER(layer_mask, FONT_RENDER_LAYER_SHADOW))
    {
        const uint32_t shadow_index = vertex_index;
        for (uint32_t i = 0; i < FONT_GLYPH_VERTICES_PER_QUAD; ++i)
            vertices[shadow_index + i] = vertices[face_index + i];

        FontGlyphVertex& shadow_1 = vertices[shadow_index + 0];
        FontGlyphVertex& shadow_2 = vertices[shadow_index + 1];
        FontGlyphVertex& shadow_3 = vertices[shadow_index + 2];
        FontGlyphVertex& shadow_4 = vertices[shadow_index + 3];
        FontGlyphVertex& shadow_5 = vertices[shadow_index + 4];
        FontGlyphVertex& shadow_6 = vertices[shadow_index + 5];
        SetPosition(shadow_1.m_Position, transform * Vector4(local_x + glyph_left_bearing + shadow_x, y - glyph_descent + shadow_y, 0, 1));
        SetPosition(shadow_2.m_Position, transform * Vector4(local_x + glyph_left_bearing + shadow_x + glyph_width, y - glyph_descent + shadow_y, 0, 1));
        SetPosition(shadow_3.m_Position, transform * Vector4(local_x + glyph_left_bearing + shadow_x, y + glyph_ascent + shadow_y, 0, 1));
        SetPosition(shadow_6.m_Position, transform * Vector4(local_x + glyph_left_bearing + shadow_x + glyph_width, y + glyph_ascent + shadow_y, 0, 1));
        shadow_4 = shadow_3;
        shadow_5 = shadow_2;
        for (uint32_t i = 0; i < FONT_GLYPH_VERTICES_PER_QUAD; ++i)
        {
            SET_MASK(vertices[shadow_index + i], 0, 0, 1)
        }
    }

    const uint8_t one_layer = layer_count == 1 ? 1 : 0;
    for (uint32_t i = 0; i < FONT_GLYPH_VERTICES_PER_QUAD; ++i)
    {
        SET_MASK(vertices[face_index + i], 1, one_layer, one_layer)
    }

#undef SET_MASK
}

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
    const float half_thickness = thickness * 0.5f;
    SetPosition(face[0].m_Position, transform * Vector4(x0, y0 - half_thickness, 0, 1));
    SetPosition(face[1].m_Position, transform * Vector4(x1, y1 - half_thickness, 0, 1));
    SetPosition(face[2].m_Position, transform * Vector4(x0, y0 + half_thickness, 0, 1));
    SetPosition(face[5].m_Position, transform * Vector4(x1, y1 + half_thickness, 0, 1));
    const float* colors[4] = { face_colors.m_BottomLeft, face_colors.m_BottomRight, face_colors.m_TopLeft, face_colors.m_TopRight };
    const uint32_t corners[4] = { 0, 1, 2, 5 };
    for (uint32_t i = 0; i < 4; ++i)
    {
        FontGlyphVertex& vertex = face[corners[i]];
        vertex.m_UV[0] = texture_u;
        vertex.m_UV[1] = texture_v;
        memcpy(vertex.m_FaceColor, colors[i], sizeof(vertex.m_FaceColor));
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
            memset(hidden[i].m_LayerMasks, 0, sizeof(hidden[i].m_LayerMasks));
    }
}

#undef HAS_LAYER
