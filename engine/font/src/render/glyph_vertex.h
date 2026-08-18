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

#include <dlib/endian.h>
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

// Converts a final unit RGBA color to the normalized byte format used by font
// vertices. Call this after applying all styles and animated effects.
static inline uint32_t FontPackColor(const dmVMath::Vector4& color)
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

// Converts final glyph-corner colors to normalized bytes in bottom-left,
// bottom-right, top-left, top-right order.
void FontPackGlyphFaceColors(const TextGlyphFaceColors& face_colors, uint32_t packed_colors[4]);

// Final render values and output ranges shared by glyph and decoration quads.
// The transform, face colors, and face output are required. Outline and shadow
// outputs may be null.
struct FontVertexLayerParams
{
    const dmVMath::Matrix4* m_Transform;
    const uint32_t*         m_FaceColors;
    FontGlyphVertex*        m_FaceVertices;
    FontGlyphVertex*        m_OutlineVertices;
    FontGlyphVertex*        m_ShadowVertices;
    uint32_t                m_OutlineColor;
    uint32_t                m_ShadowColor;
    float                   m_SdfEdge;
    float                   m_SdfOutline;
    float                   m_SdfSmoothing;
    float                   m_SdfShadow;
    float                   m_ShadowX;
    float                   m_ShadowY;
    uint32_t                m_LayerCount;
};

// Atlas placement and local-space geometry for one glyph quad.
struct FontGlyphVertexParams
{
    FontGlyph* m_Glyph;
    float      m_RecipAtlasWidth;
    float      m_RecipAtlasHeight;
    float      m_X;
    float      m_Y;
    float      m_RenderScale;
    uint32_t   m_CellX;
    uint32_t   m_CellY;
    uint32_t   m_CacheCellMaxAscent;
    uint32_t   m_CacheCellPadding;
    bool       m_MetricsFromTtf;
};

// Atlas placement, local-space geometry, and pattern for one decoration quad.
struct FontDecorationVertexParams
{
    float m_TextureU;
    float m_TextureV;
    float m_X0;
    float m_Y0;
    float m_X1;
    float m_Y1;
    float m_Thickness;
    float m_PatternStart;
    float m_PatternEnd;
    float m_PatternDuty;
    float m_OutlineWidth;
};

// Packs one glyph quad into the requested output layers.
void FontPackGlyphVertices(const FontGlyphVertexParams& glyph, const FontVertexLayerParams& layers);

// Packs one line-decoration quad into the requested output layers.
void FontPackDecorationVertices(const FontDecorationVertexParams& decoration, const FontVertexLayerParams& layers);

// Returns whether the decoration must be split to preserve per-glyph styling.
bool FontDecorationRequiresGlyphSegments(HTextLayout layout, const TextDecoration& decoration);

// Returns the number of quads required to preserve the decoration's styling.
uint32_t FontGetDecorationQuadCount(HTextLayout layout, const TextDecoration& decoration);

// Resolves shader pattern coordinates for one of the decoration's segments.
void FontGetDecorationPattern(const TextDecoration& decoration, uint32_t segment_index, uint32_t segment_count, FontDecorationPattern* pattern);

// Resolves decoration corner colors from its geometrically outermost glyphs.
void FontGetDecorationFaceColors(HTextLayout layout, const TextDecoration& decoration, const float base_color[4], TextGlyphFaceColors* face_colors);

#endif // DM_FONT_GLYPH_VERTEX_H
