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


static void FontDestroyGB(HFont hfont)
{
    GlyphBankFont* font = (GlyphBankFont*)hfont;
    // The actual data comes from the resource system, so no need to free that
    free((void*)font);
}

static uint32_t GetResourceSizeGB(HFont hfont)
{
    dmRenderDDF::GlyphBank* bank = ToGlyphBank(hfont);
    uint32_t size = sizeof(dmRenderDDF::GlyphBank)
                    + sizeof(GlyphBankFont)
                    + sizeof(dmRenderDDF::GlyphBank::Glyph) + bank->m_Glyphs.m_Count
                    + bank->m_GlyphData.m_Count;
    return size;
}

static float GetScaleFromSizeGB(HFont hfont, uint32_t size)
{
    // These fonts are prebaked
    return 1.0f;
}

static float GetAscentGB(HFont hfont, float scale)
{
    (void)scale; // should be 1.0f
    dmRenderDDF::GlyphBank* bank = ToGlyphBank(hfont);
    return bank->m_MaxAscent;
}

static float GetDescentGB(HFont hfont, float scale)
{
    (void)scale; // should be 1.0f
    dmRenderDDF::GlyphBank* bank = ToGlyphBank(hfont);
    return bank->m_MaxDescent;
}

static float GetLineGapGB(HFont hfont, float scale)
{
    return 0.0f;
}

static FontResult FreeGlyphGB(HFont hfont, FontGlyph* glyph)
{
    (void)hfont;
    (void)glyph;
    return FONT_RESULT_OK;
}

static dmRenderDDF::GlyphBank::Glyph* FindByCodePoint(dmRenderDDF::GlyphBank* bank, uint32_t c)
{
//TODO: Make it a binary search!!!
    uint32_t n = bank->m_Glyphs.m_Count;
    for (uint32_t i = 0; i < n; ++i)
    {
        dmRenderDDF::GlyphBank::Glyph* g = &bank->m_Glyphs[i];
        if (g->m_Character == c)
        {
            return g;
        }
    }
    return 0;
}

static uint32_t GetGlyphIndexGB(HFont hfont, uint32_t codepoint)
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

static FontResult GetGlyphGB(HFont hfont, uint32_t glyph_index, const FontGlyphOptions* options, FontGlyph* out)
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

            out->m_Bitmap.m_Data = glyph_data + 1;
            out->m_Bitmap.m_Flags = glyph_data[0];
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
    font->m_Base.m_DestroyFont = FontDestroyGB;
    font->m_Base.m_GetResourceSize = GetResourceSizeGB;
    font->m_Base.m_GetScaleFromSize = GetScaleFromSizeGB;
    font->m_Base.m_GetAscent = GetAscentGB;
    font->m_Base.m_GetDescent = GetDescentGB;
    font->m_Base.m_GetLineGap = GetLineGapGB;
    font->m_Base.m_GetGlyphIndex = GetGlyphIndexGB;
    font->m_Base.m_GetGlyph = GetGlyphGB;
    font->m_Base.m_FreeGlyph = FreeGlyphGB;

#define FOURCC(a,b,c,d) ( (uint32_t) (((d)<<24) | ((c)<<16) | ((b)<<8) | (a)) )

    font->m_Base.m_Type = (FontType)FOURCC('g', 'l', 'y', 'p');
    font->m_Base.m_Path = strdup(path);

#undef FOURCC

    font->m_GlyphBank = glyph_bank;

    // Making sure that it's an increasing list of codepoints, as it's required
    // by the binary search algorithm
    int32_t c = -1;
    uint32_t n = glyph_bank->m_Glyphs.m_Count;
    for (uint32_t i = 0; i < n; ++i)
    {
        dmRenderDDF::GlyphBank::Glyph& glyph = glyph_bank->m_Glyphs[i];
        assert(c < (int)glyph.m_Character);
        c = glyph.m_Character;
    }

    return (Font*)font;
}
