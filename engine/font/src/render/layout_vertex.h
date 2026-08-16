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

#ifndef DM_FONT_LAYOUT_VERTEX_H
#define DM_FONT_LAYOUT_VERTEX_H

#include "glyph_vertex.h"

struct FontLayoutCachedGlyph
{
    FontGlyph* m_Glyph;
    uint32_t   m_CellX;
    uint32_t   m_CellY;
};

typedef bool (*FontLayoutResolveGlyph)(void* context, const TextGlyph& text_glyph, FontLayoutCachedGlyph* cached_glyph);

struct FontLayoutVertexConfig
{
    HTextLayout            m_Layout;
    FontLayoutResolveGlyph m_ResolveGlyph;
    void*                  m_ResolveGlyphContext;
    dmVMath::Matrix4       m_Transform;
    dmVMath::Vector4       m_OutlineColor;
    dmVMath::Vector4       m_ShadowColor;
    float                  m_FaceColor[4];
    float                  m_Width;
    float                  m_Height;
    float                  m_RecipAtlasWidth;
    float                  m_RecipAtlasHeight;
    float                  m_DecorationU;
    float                  m_DecorationV;
    float                  m_SdfEdge;
    float                  m_SdfOutline;
    float                  m_SdfSmoothing;
    float                  m_SdfShadow;
    float                  m_SdfSpread;
    float                  m_ShadowX;
    float                  m_ShadowY;
    float                  m_ShadowBlur;
    float                  m_MonospacePadding;
    uint32_t               m_CacheCellMaxAscent;
    uint32_t               m_CacheCellPadding;
    uint32_t               m_MaxVertexCount;
    uint32_t               m_Align;
    uint32_t               m_VerticalAlign;
    uint8_t                m_BaseLayerMask;
    bool                   m_MetricsFromTtf;
    bool                   m_RenderDecorations;
    bool                   m_RenderObjectOutlines;
    bool                   m_ResolveGlyphsForMetrics;
};

struct FontLayoutVertexMetrics
{
    uint32_t m_GlyphQuadCount;
    uint32_t m_ObjectQuadCount;
    uint32_t m_DecorationQuadCount;
    uint32_t m_FaceQuadCount;
    uint32_t m_OutlineQuadCount;
    uint32_t m_ShadowQuadCount;
    uint32_t m_QuadCount;
    uint32_t m_VertexCount;
    uint32_t m_VertexBufferSize;
    uint32_t m_LayerCount;
    uint8_t  m_LayerMask;
    bool     m_Truncated;
};

bool     FontGetLayoutVertexMetrics(const FontLayoutVertexConfig& config, FontLayoutVertexMetrics* metrics);

uint32_t FontCreateLayoutVertices(const FontLayoutVertexConfig&  config,
                                  const FontLayoutVertexMetrics& metrics,
                                  FontGlyphVertex*               vertices,
                                  uint32_t                       max_vertices);

#endif // DM_FONT_LAYOUT_VERTEX_H
