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

#include <assert.h>

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
    assert(layer_count > 0 && layer_count <= FONT_GLYPH_MAX_LAYER_COUNT);

    float glyph_width;
    float glyph_descent;
    float glyph_ascent;
    float glyph_left_bearing;
    float glyph_size_difference;
    if (glyph)
    {
        assert(glyph->m_Bitmap.m_Width != 0);
        glyph_width = metrics_from_ttf ? glyph->m_Bitmap.m_Width : glyph->m_Width;
        glyph_descent = glyph->m_Descent;
        glyph_ascent = glyph->m_Ascent;
        glyph_left_bearing = glyph->m_LeftBearing;
        glyph_size_difference = glyph_width - glyph->m_Width;
    }
    else
    {
        glyph_width = 0.0f;
        glyph_descent = 0.0f;
        glyph_ascent = 0.0f;
        glyph_left_bearing = 0.0f;
        glyph_size_difference = 0.0f;
    }

    const int16_t    width = (int16_t)glyph_width;
    const int16_t    descent = (int16_t)glyph_descent;
    const int16_t    ascent = (int16_t)glyph_ascent;
    const int16_t    cell_offset_y = cache_cell_max_ascent - ascent;
    const uint32_t   face_index = vertex_index + vertex_layer_stride * (layer_count - 1);
    const float      local_x = x - glyph_size_difference * 0.5f;

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

#define SET_PROPERTIES(vertex) \
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

    SET_PROPERTIES(face_1)
    SET_PROPERTIES(face_2)
    SET_PROPERTIES(face_3)
    SET_PROPERTIES(face_6)
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

#undef HAS_LAYER
