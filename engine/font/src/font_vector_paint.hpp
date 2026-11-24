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

#ifndef DM_FONT_VECTOR_PAINT_H
#define DM_FONT_VECTOR_PAINT_H

#include <dlib/array.h>
#include <dlib/hashtable.h>
#include <dlib/vmath.h>
#include "font_vector_slug.h"
#include "text_layout.h"

// Glyphs use font units. Fonts and the borrowed Slug data must outlive this cache.
struct FontVectorPaintPath
{
    uint32_t            m_Glyph;
    FontVectorSlugGlyph m_Slug;
    dmVMath::Vector4    m_Bounds;
};

struct FontVectorPaintLayer
{
    uint32_t         m_Path;
    uint32_t         m_Paint;
    uint32_t         m_CompositeCount; // On the last layer: paints sharing one outline coverage.
    dmVMath::Matrix4 m_Transform;
    dmVMath::Matrix4 m_PaintTransform;
    dmVMath::Vector4 m_Clip;
};

struct FontVectorPaintGlyph
{
    uint32_t         m_Glyph;
    uint32_t         m_LayerCount;
    uint32_t         m_Paint; // Header followed by six texels per layer in m_Paints.
    dmVMath::Vector4 m_Bounds;
};

struct FontVectorPaintVertex
{
    dmVMath::Vector4 m_Position;
    dmVMath::Vector4 m_Color;
    dmVMath::Vector4 m_Clip;
    float            m_FontPosition[2];
    float            m_Paint;
};

struct FontVectorPaintCache
{
    FontVectorSlugData*           m_Slug;
    dmArray<FontVectorPaintPath>  m_Paths;
    dmArray<FontVectorPaintLayer> m_LayerScratch; // Reused for one glyph; empty between calls.
    dmArray<FontVectorPaintGlyph> m_Glyphs;
    dmArray<dmVMath::Vector4>     m_Paints;
    dmHashTable64<uint32_t>       m_PathLookup;
    dmHashTable64<uint32_t>       m_GlyphLookup;
};

// Discards CPU paint/path metadata. The owner resets the borrowed Slug data.
void FontVectorPaintClear(FontVectorPaintCache* cache);
// Whether this source has a COLR graph (including unsupported graphs).
bool FontVectorPaintHasGlyph(HFont font, uint32_t glyph);
// Returns ~0u for bitmap sources. Unsupported color graphs use the glyph's
// outline, or .notdef when it is empty. Output is cached, including fallback.
// False means the bounded cache is full or the outline cannot be decoded.
bool FontVectorPaintAddGlyph(FontVectorPaintCache* cache, HFont font, uint32_t glyph, uint32_t* index, bool* has_color);
// Packs one glyph into six clockwise vertices. Scale maps font units to pixels;
// pixel_scale includes view zoom for antialiasing. Transform maps to the
// caller's coordinate space; clip is in that same space.
void FontVectorPaintPackGlyph(const FontVectorPaintGlyph& glyph, float scale, float pixel_scale, float x, float y, const dmVMath::Matrix4& transform, const dmVMath::Vector4& clip, const TextGlyphFaceColors& colors, FontVectorPaintVertex vertices[6]);

#endif
