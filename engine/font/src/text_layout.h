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

#ifndef DM_TEXT_LAYOUT_H
#define DM_TEXT_LAYOUT_H

#include <dmsdk/dlib/array.h>
#include <dmsdk/font/text_layout.h>

typedef void (*FTextLayoutFree)(HTextLayout layout);

struct TextLayout
{
    FTextLayoutFree    m_Free;

    // TODO: Make these C arrays?
    dmArray<TextGlyph> m_Glyphs;
    dmArray<TextLine>  m_Lines;

    HFontCollection    m_FontCollection;

    uint16_t           m_NumValidGlyphs;   // TODO: Remove non renderable glyphs
    TextDirection      m_Direction;

    // Used for creating a cell in the glyph image cache texture
    float              m_MaxGlyphWidth;
    float              m_MaxGlyphHeight;

    // Bounds of the entire layout
    float              m_Width;
    float              m_Height;
};

TextResult TextLayoutLegacyCreate(HFontCollection collection,
                                uint32_t* codepoints, uint32_t num_codepoints,
                                TextLayoutSettings* settings, HTextLayout* outlayout);

TextResult TextLayoutSkribidiCreate(HFontCollection collection,
                                uint32_t* codepoints, uint32_t num_codepoints,
                                TextLayoutSettings* settings, HTextLayout* outlayout);

#endif // DM_TEXT_LAYOUT_H
