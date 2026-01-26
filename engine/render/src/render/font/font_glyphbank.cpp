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

#include <stdlib.h>
#include <font/font.h>


#include <render/font_ddf.h>

#include "font_glyphbank.h"

struct GlyphBankFont
{
    Font m_Base;
    dmRenderDDF::GlyphBank* m_GlyphBank;
};

static dmRenderDDF::GlyphBank* ToGlyphBank(HFont hfont)
{
    return ((GlyphBankFont*)hfont)->m_GlyphBank;
}


static void GBFontDestroy(HFont hfont)
{
    GlyphBankFont* font = (GlyphBankFont*)hfont;
    // The actual data comes from the resource system, so no need to free that
    delete font;
}

static uint32_t GBGetResourceSize(HFont hfont)
{
    dmRenderDDF::GlyphBank* bank = ToGlyphBank(hfont);
    uint32_t size = sizeof(dmRenderDDF::GlyphBank)
                    + sizeof(GlyphBankFont)
                    + sizeof(dmRenderDDF::GlyphBank::Glyph) + bank->m_Glyphs.m_Count
                    + bank->m_GlyphData.m_Count;
    return size;
}

static float GBGetScaleFromSize(HFont hfont, uint32_t size)
{
    // These fonts are prebaked
    return 1.0f;
}

static float GBGetAscent(HFont hfont, float scale)
{
    (void)scale; // should be 1.0f
    dmRenderDDF::GlyphBank* bank = ToGlyphBank(hfont);
    return bank->m_MaxAscent;
}

static float GBGetDescent(HFont hfont, float scale)
{
    (void)scale; // should be 1.0f
    dmRenderDDF::GlyphBank* bank = ToGlyphBank(hfont);
    return bank->m_MaxDescent;
}

static float GBGetLineGap(HFont hfont, float scale)
{
    return 0.0f;
}

static FontResult GBFreeGlyph(HFont hfont, FontGlyph* glyph)
{
    (void)hfont;
    (void)glyph;
    return FONT_RESULT_OK;
}

static dmRenderDDF::GlyphBank::Glyph* FindByCodePoint(dmRenderDDF::GlyphBank* bank, uint32_t c)
{
    // do a binary search of the glyphs
    // requires the glyphs to be sorted
    // a binary search repeatedly compares the target value to the middle
    // element of a sorted array and halves the search range to the left or
    // right until the value is found or the range is empty
    uint32_t n = bank->m_Glyphs.m_Count;
    int left = 0;
    int right = n - 1;
    while (left <= right)
    {
        int mid = left + (right - left) / 2;

        dmRenderDDF::GlyphBank::Glyph* g = &bank->m_Glyphs[mid];
        if (g->m_Character == c)
        {
            return g;
        }
        
        if (g->m_Character < c)
        {
            // the glyph is in the upper/right half of the range
            left = mid + 1;
        }
        else
        {
            // the glyph is in the lower/left half of the range
            right = mid - 1;
        }
    }

    return 0;
}

static uint32_t GBGetGlyphIndex(HFont hfont, uint32_t codepoint)
{
    dmRenderDDF::GlyphBank* bank = ToGlyphBank(hfont);
    dmRenderDDF::GlyphBank::Glyph* g = FindByCodePoint(bank, codepoint);
    if (!g)
        return 0;
    dmRenderDDF::GlyphBank::Glyph* first = &bank->m_Glyphs[0];
    return 1 + (uint32_t)(uintptr_t)(g - first);
}

static inline uint8_t* GetPointer(void* data, uint32_t offset)
{
    return ((uint8_t*)data) + offset;
}

static FontResult GBGetGlyph(HFont hfont, uint32_t glyph_index, const FontGlyphOptions* options, FontGlyph* out)
{
    if (glyph_index == 0)
        return FONT_RESULT_ERROR;

    dmRenderDDF::GlyphBank* bank = ToGlyphBank(hfont);
    dmRenderDDF::GlyphBank::Glyph* g = &bank->m_Glyphs[glyph_index-1];

    memset(out, 0, sizeof(*out));
    out->m_GlyphIndex   = glyph_index;
    out->m_Codepoint    = g->m_Character;
    out->m_Advance      = g->m_Advance;
    out->m_LeftBearing  = g->m_LeftBearing;
    out->m_Ascent       = g->m_Ascent;
    out->m_Descent      = g->m_Descent;

    uint32_t padding2 = bank->m_GlyphPadding*2;
    out->m_Width        = g->m_Width + padding2;
    out->m_Height       = g->m_Ascent + g->m_Descent + padding2;

    if (options->m_GenerateImage)
    {
        if (g->m_GlyphDataSize != 0)
        {
            uint8_t* data = (uint8_t*)bank->m_GlyphData.m_Data;
            uint8_t* glyph_data = GetPointer(data, g->m_GlyphDataOffset);

            out->m_Bitmap.m_DataSize = g->m_GlyphDataSize;
            out->m_Bitmap.m_Data = glyph_data + 1;
            out->m_Bitmap.m_Flags = glyph_data[0] | FONT_GLYPH_BM_FLAG_DATA_IS_BORROWED;
            out->m_Bitmap.m_Width = out->m_Width;
            out->m_Bitmap.m_Height = out->m_Height;
            out->m_Bitmap.m_Channels = bank->m_GlyphChannels;
        }
    }
    return FONT_RESULT_OK;
}

HFont CreateGlyphBankFont(const char* path, dmRenderDDF::GlyphBank* glyph_bank)
{
    GlyphBankFont* font = new GlyphBankFont;
    memset(font, 0, sizeof(*font));

    font->m_Base.m_LoadFontFromMemory = 0; // it's this current function!
    font->m_Base.m_DestroyFont = GBFontDestroy;
    font->m_Base.m_GetResourceSize = GBGetResourceSize;
    font->m_Base.m_GetScaleFromSize = GBGetScaleFromSize;
    font->m_Base.m_GetAscent = GBGetAscent;
    font->m_Base.m_GetDescent = GBGetDescent;
    font->m_Base.m_GetLineGap = GBGetLineGap;
    font->m_Base.m_GetGlyphIndex = GBGetGlyphIndex;
    font->m_Base.m_GetGlyph = GBGetGlyph;
    font->m_Base.m_FreeGlyph = GBFreeGlyph;

#define FOURCC(a,b,c,d) ( (uint32_t) (((d)<<24) | ((c)<<16) | ((b)<<8) | (a)) )

    font->m_Base.m_Type = (FontType)FOURCC('g', 'l', 'y', 'p');
    font->m_Base.m_Path = strdup(path);

#undef FOURCC

    font->m_GlyphBank = glyph_bank;

    return (Font*)font;
}
