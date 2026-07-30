// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0.

#ifndef DM_FONT_GLYPH_GEN_H
#define DM_FONT_GLYPH_GEN_H

#include <dmsdk/font/font.h>

struct FontGlyphGenParams
{
    FontGlyphGenParams();

    float   m_Scale;
    float   m_SdfPadding;
    float   m_OutlineWidth;
    float   m_ShadowBlur;
    uint8_t m_SdfEdgeValue;
};

FontResult FontGenerateGlyph(HFont font, uint32_t glyph_index, const FontGlyphGenParams* params, FontGlyph* glyph);

#endif // DM_FONT_GLYPH_GEN_H
