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
    bool    m_OutputBitmap;
    bool    m_Antialias;
    bool    m_HasOutline;
    bool    m_HasShadow;
};

FontResult FontGenerateGlyph(HFont font, uint32_t glyph_index, const FontGlyphGenParams* params, FontGlyph* glyph);
uint32_t FontGetGlyphChannelCount(bool output_bitmap, bool has_outline, bool has_shadow, float shadow_blur);

#endif // DM_FONT_GLYPH_GEN_H
