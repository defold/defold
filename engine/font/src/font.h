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

#ifndef DM_FONT_H
#define DM_FONT_H

#include <dmsdk/font/font.h>
#include "text_layout.h"

typedef HFont       (*FontLoadFromMemoryFn)(const char* name, const void* data, uint32_t data_size, bool allocate);
typedef void        (*FontDestroyFn)(HFont font);
typedef uint32_t    (*FontGetResourceSizeFn)(HFont font);
typedef float       (*FontGetScaleFromSizeFn)(HFont hfont, uint32_t size);
typedef float       (*FontGetAscentFn)(HFont hfont, float scale);
typedef float       (*FontGetDescentFn)(HFont hfont, float scale);
typedef float       (*FontGetLineGapFn)(HFont hfont, float scale);
typedef uint32_t    (*FontGetGlyphIndexFn)(HFont font, uint32_t codepoint);
typedef FontResult  (*FontGetGlyphFn)(HFont hfont, uint32_t glyph_index, const FontGlyphOptions* options, FontGlyph* glyph);
typedef FontResult  (*FontFreeGlyphFn)(HFont hfont, FontGlyph* glyph);

struct Font
{
    FontType        m_Type;
    const char*     m_Path;
    uint32_t        m_PathHash;

    FontLoadFromMemoryFn        m_LoadFontFromMemory;
    FontDestroyFn               m_DestroyFont;
    FontGetResourceSizeFn       m_GetResourceSize;
    FontGetScaleFromSizeFn      m_GetScaleFromSize;
    FontGetAscentFn             m_GetAscent;
    FontGetDescentFn            m_GetDescent;
    FontGetLineGapFn            m_GetLineGap;
    FontGetGlyphIndexFn         m_GetGlyphIndex;
    FontGetGlyphFn              m_GetGlyph;
    FontFreeGlyphFn             m_FreeGlyph;
};

HFont FontCreate(Font* font);

TextLayoutType FontGetLayoutType(HFont hfont);

#if defined(FONT_USE_HARFBUZZ)

typedef struct hb_font_t hb_font_t;

hb_font_t* FontGetHarfbuzzFontFromTTF(HFont hfont);

#endif


#endif // DM_FONT_H
