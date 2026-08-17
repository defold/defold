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

#ifndef DM_FONT_GLYPH_VERTEX_H
#define DM_FONT_GLYPH_VERTEX_H

#include <stdint.h>

#include <dlib/vmath.h>
#include <dmsdk/font/font.h>
#include <dmsdk/font/text_layout.h>

enum FontRenderLayerMask
{
    FONT_RENDER_LAYER_FACE = 0x1,
    FONT_RENDER_LAYER_OUTLINE = 0x2,
    FONT_RENDER_LAYER_SHADOW = 0x4,
};

// Packs a normalized texture coordinate into the font vertex UV format.
static inline uint16_t FontPackGlyphUV(float uv)
{
    return (uint16_t)(uv * 65535.0f + 0.5f);
}

// Expands a packed font vertex UV into a normalized texture coordinate.
static inline float FontUnpackGlyphUV(uint16_t uv)
{
    return uv * (1.0f / 65535.0f);
}

struct FontGlyphVertex
{
    float    m_Position[3];
    // The vertex declaration expands these normalized shorts to a vec2 input.
    uint16_t m_UV[2];
    // The vertex declaration expands these normalized bytes to vec4 inputs.
    uint8_t  m_FaceColor[4];
    uint8_t  m_OutlineColor[4];
    uint8_t  m_ShadowColor[4];
    float    m_SdfParams[4];
    float    m_LayerMasks[3];
};

struct TextGlyphFaceColors;

struct FontDecorationPattern
{
    // Pattern positions are measured in cycles. A zero duty marks a solid quad.
    float m_Start;
    float m_End;
    float m_Duty;
};

// Packs a glyph with a single face color into a layer-major vertex buffer.
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
                           const dmVMath::Vector4& face_color,
                           const dmVMath::Vector4& outline_color,
                           const dmVMath::Vector4& shadow_color,
                           float                   sdf_edge_value,
                           float                   sdf_outline,
                           float                   sdf_smoothing,
                           float                   sdf_shadow,
                           float                   shadow_x,
                           float                   shadow_y,
                           bool                    metrics_from_ttf,
                           FontGlyphVertex*        vertices);

// Packs a glyph with independent corner colors into a layer-major vertex buffer.
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
                                  const dmVMath::Vector4&    outline_color,
                                  const dmVMath::Vector4&    shadow_color,
                                  float                      sdf_edge_value,
                                  float                      sdf_outline,
                                  float                      sdf_smoothing,
                                  float                      sdf_shadow,
                                  float                      shadow_x,
                                  float                      shadow_y,
                                  bool                       metrics_from_ttf,
                                  FontGlyphVertex*           vertices);

// Packs one six-vertex glyph quad into each requested output layer. The face
// output is required; outline and shadow outputs may be null.
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
                                          const dmVMath::Vector4&    outline_color,
                                          const dmVMath::Vector4&    shadow_color,
                                          float                      sdf_edge_value,
                                          float                      sdf_outline,
                                          float                      sdf_smoothing,
                                          float                      sdf_shadow,
                                          float                      shadow_x,
                                          float                      shadow_y,
                                          bool                       metrics_from_ttf,
                                          FontGlyphVertex*           face_vertices,
                                          FontGlyphVertex*           outline_vertices,
                                          FontGlyphVertex*           shadow_vertices);

// Packs a face-only line-decoration quad around the supplied center line.
// Pattern positions are in cycles; a zero duty produces a solid line.
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
                                FontGlyphVertex*           vertices);

// Returns whether the decoration must be split to preserve per-glyph styling.
bool FontDecorationRequiresGlyphSegments(HTextLayout layout, const TextDecoration& decoration);

// Returns the number of quads required to preserve the decoration's styling.
uint32_t FontGetDecorationQuadCount(HTextLayout layout, const TextDecoration& decoration);

// Resolves shader pattern coordinates for one of the decoration's segments.
void FontGetDecorationPattern(const TextDecoration& decoration, uint32_t segment_index, uint32_t segment_count, FontDecorationPattern* pattern);

// Resolves decoration corner colors from its geometrically outermost glyphs.
void FontGetDecorationFaceColors(HTextLayout layout, const TextDecoration& decoration, const float base_color[4], TextGlyphFaceColors* face_colors);

#endif // DM_FONT_GLYPH_VERTEX_H
