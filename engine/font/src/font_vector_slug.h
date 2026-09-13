// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0 (https://www.defold.com/license).
#ifndef DM_FONT_VECTOR_SLUG_H
#define DM_FONT_VECTOR_SLUG_H
#include <dlib/array.h>
#include <dmsdk/font/font.h>
static const uint32_t FONT_VECTOR_SLUG_WIDTH = 4096;
static const uint32_t FONT_VECTOR_SLUG_MAX_HEIGHT = 256;
struct FontVectorSlugGlyph
{
    uint32_t m_BandTexel;
    uint32_t m_CurveCount;
    float m_BandTransform[4];
};

// Owns CPU texture data for the active glyph set. Curves are RGBA16F; each
// band texel packs two unsigned 16-bit fields in R32UI. No raster coverage.
struct FontVectorSlugData
{
    FontVectorSlugData();
    dmArray<uint16_t> m_Curves;
    dmArray<uint32_t> m_Bands;
    bool m_Overflow; // Last append failed because the numeric atlas could not fit.
    uint32_t m_MaxTexels; // Set before appending; bounds both numeric textures.
    uint32_t m_CurveTexels;
    uint32_t m_BandTexels;
    uint32_t m_CurveCount;
    uint32_t m_BandReferences;
    uint32_t m_MaxBandCurves;
};

void FontVectorSlugBegin(FontVectorSlugData* data);
// Appends one glyph using unhinted, unsplit quadratic outlines in em-space.
// Each band header/list and each curve's two texels stay within a texture row.
// On failure, existing records and counters are preserved.
bool FontVectorSlugAddGlyph(FontVectorSlugData* data, const FontGlyphOutline& outline,
                           float font_size, uint32_t bands, FontVectorSlugGlyph* glyph);

// Normalizes runtime or prebaked glyph geometry to its original glyph bounds.
// Inputs are borrowed. On failure, existing records and counters are preserved.
bool FontVectorSlugAddFontGlyph(FontVectorSlugData* data, const FontGlyph& source,
                                uint32_t bands, FontVectorSlugGlyph* glyph);

#endif
