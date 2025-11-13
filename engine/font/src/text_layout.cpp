// Copyright 2020-2025 The Defold Foundation
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

#include "text_layout.h"
#include "fontcollection.h"

uint32_t TextLayoutGetGlyphCount(HTextLayout layout)
{
    return layout->m_Glyphs.Size();
}

TextGlyph* TextLayoutGetGlyphs(HTextLayout layout)
{
    return layout->m_Glyphs.Begin();
}

uint32_t TextLayoutGetLineCount(HTextLayout layout)
{
    return layout->m_Lines.Size();
}

TextLine* TextLayoutGetLines(HTextLayout layout)
{
    return layout->m_Lines.Begin();
}

void TextLayoutGetBounds(HTextLayout layout, float* width, float* height)
{
    *width = layout->m_Width;
    *height = layout->m_Height;
}

void TextLayoutFree(HTextLayout layout)
{
    layout->m_Free(layout);
}

TextResult TextLayoutCreate(HFontCollection collection,
                            uint32_t* codepoints, uint32_t num_codepoints,
                            TextLayoutSettings* settings, HTextLayout* outlayout)
{
#if defined(FONT_USE_SKRIBIDI)
    TextLayoutType layout_type = FontCollectionGetLayoutType(collection);
    if (layout_type == TEXT_LAYOUT_TYPE_FULL)
        return TextLayoutSkribidiCreate(collection, codepoints, num_codepoints, settings, outlayout);
#endif
    return TextLayoutLegacyCreate(collection, codepoints, num_codepoints, settings, outlayout);
}

