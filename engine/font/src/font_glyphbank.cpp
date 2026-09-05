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

#include "font_glyphbank.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <dlib/hash.h>

#include "font.h"

struct GlyphBankFont
{
    Font                   m_Base;
    FontGlyphBankProvider* m_Provider;
};

static FontGlyphBankProvider* GetProvider(HFont hfont)
{
    return ((GlyphBankFont*)hfont)->m_Provider;
}

static void GlyphBankDestroy(HFont hfont)
{
    GlyphBankFont* font = (GlyphBankFont*)hfont;
    if (font->m_Provider->m_Destroy)
    {
        font->m_Provider->m_Destroy(font->m_Provider->m_Context);
    }
    delete font;
}

static uint32_t GlyphBankGetResourceSize(HFont hfont)
{
    const uint64_t provider_size = GetProvider(hfont)->m_ResourceSize;
    if (provider_size > UINT32_MAX - sizeof(GlyphBankFont))
        return UINT32_MAX;
    return (uint32_t)(sizeof(GlyphBankFont) + provider_size);
}

static float GlyphBankGetScaleFromSize(HFont hfont, uint32_t size)
{
    (void)hfont;
    (void)size;
    return 1.0f;
}

static float GlyphBankGetAscent(HFont hfont, float scale)
{
    (void)scale;
    return GetProvider(hfont)->m_MaxAscent;
}

static float GlyphBankGetDescent(HFont hfont, float scale)
{
    (void)scale;
    return GetProvider(hfont)->m_MaxDescent;
}

static float GlyphBankGetLineGap(HFont hfont, float scale)
{
    (void)hfont;
    (void)scale;
    return 0.0f;
}

static uint32_t GlyphBankGetGlyphIndex(HFont hfont, uint32_t codepoint)
{
    FontGlyphBankProvider* provider = GetProvider(hfont);
    int32_t                left = 0;
    int32_t                right = (int32_t)provider->m_GlyphCount - 1;
    while (left <= right)
    {
        const int32_t  mid = left + (right - left) / 2;
        const uint32_t current = provider->m_GetCodepoint(provider->m_Context, (uint32_t)mid);
        if (current == codepoint)
            return (uint32_t)mid + 1;
        if (current < codepoint)
            left = mid + 1;
        else
            right = mid - 1;
    }
    return 0;
}

static FontResult GlyphBankGetGlyph(HFont hfont, uint32_t glyph_index, const FontGlyphOptions* options, FontGlyph* output)
{
    FontGlyphBankProvider* provider = GetProvider(hfont);
    if (glyph_index == 0 || glyph_index > provider->m_GlyphCount || !options || !output)
        return FONT_RESULT_ERROR;

    FontGlyphBankGlyph glyph;
    if (!provider->m_GetGlyph(provider->m_Context, glyph_index - 1, &glyph))
        return FONT_RESULT_ERROR;

    memset(output, 0, sizeof(*output));
    output->m_GlyphIndex = glyph_index;
    output->m_Codepoint = glyph.m_Codepoint;
    output->m_Width = glyph.m_Width;
    output->m_Height = glyph.m_Ascent + glyph.m_Descent;
    output->m_Advance = glyph.m_Advance;
    output->m_LeftBearing = glyph.m_LeftBearing;
    output->m_Ascent = glyph.m_Ascent;
    output->m_Descent = glyph.m_Descent;

    if (options->m_GenerateImage && glyph.m_DataSize != 0)
    {
        const uint32_t padding = provider->m_GlyphPadding * 2;
        output->m_Bitmap.m_Data = (uint8_t*)glyph.m_Data;
        output->m_Bitmap.m_DataSize = glyph.m_DataSize;
        output->m_Bitmap.m_Width = (uint16_t)((uint32_t)glyph.m_Width + padding);
        output->m_Bitmap.m_Height = (uint16_t)((uint32_t)(glyph.m_Ascent + glyph.m_Descent) + padding);
        output->m_Bitmap.m_Channels = (uint8_t)provider->m_GlyphChannels;
        output->m_Bitmap.m_Flags = glyph.m_BitmapFlags | FONT_GLYPH_BM_FLAG_DATA_IS_BORROWED;
    }
    return FONT_RESULT_OK;
}

static FontResult GlyphBankFreeGlyph(HFont hfont, FontGlyph* glyph)
{
    (void)hfont;
    (void)glyph;
    return FONT_RESULT_OK;
}

HFont FontCreateGlyphBank(const char* path, FontGlyphBankProvider* provider)
{
    if (!path || !provider || !provider->m_GetCodepoint || !provider->m_GetGlyph || provider->m_GlyphCount > INT32_MAX)
        return 0;

    GlyphBankFont* font = new GlyphBankFont;
    memset(font, 0, sizeof(*font));
    font->m_Base.m_Path = strdup(path);
    if (!font->m_Base.m_Path)
    {
        delete font;
        return 0;
    }

    font->m_Base.m_Type = FONT_TYPE_GLYPH_BANK;
    font->m_Base.m_PathHash = dmHashString32(path);
    font->m_Base.m_DestroyFont = GlyphBankDestroy;
    font->m_Base.m_GetResourceSize = GlyphBankGetResourceSize;
    font->m_Base.m_GetScaleFromSize = GlyphBankGetScaleFromSize;
    font->m_Base.m_GetAscent = GlyphBankGetAscent;
    font->m_Base.m_GetDescent = GlyphBankGetDescent;
    font->m_Base.m_GetLineGap = GlyphBankGetLineGap;
    font->m_Base.m_GetGlyphIndex = GlyphBankGetGlyphIndex;
    font->m_Base.m_GetGlyph = GlyphBankGetGlyph;
    font->m_Base.m_FreeGlyph = GlyphBankFreeGlyph;
    font->m_Provider = provider;
    return &font->m_Base;
}
