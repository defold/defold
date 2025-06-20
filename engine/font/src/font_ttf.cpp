// Copyright 2021 The Defold Foundation
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

#include <dmsdk/dlib/log.h>

#include <stdlib.h> // free

// Making sure we can guarantuee the functions
#define STBTT_malloc(x,u)  ((void)(u),malloc(x))
#define STBTT_free(x,u)    ((void)(u),free(x))

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "font_private.h"

namespace dmFont
{

struct TTFFont
{
    dmFont::Font    m_Base;

    stbtt_fontinfo  m_Font;
    const char*     m_Path;
    const void*     m_Data;
    uint32_t        m_DataSize;

    int             m_Ascent;
    int             m_Descent;
    int             m_LineGap;
    uint32_t        m_Allocated:1;
};

static inline TTFFont* ToFont(HFont hfont)
{
    return (TTFFont*)hfont;
}

void DestroyFontTTF(HFont hfont)
{
    TTFFont* font = ToFont(hfont);
    if (font->m_Allocated)
    {
        free((void*)font->m_Data);
    }
    free((void*)font);
}

static HFont LoadTTFInternal(const char* path, const void* buffer, uint32_t buffer_size, bool allocate)
{
    TTFFont* font = new TTFFont;
    memset(font, 0, sizeof(*font));

    if (allocate)
    {
        font->m_Data = (const void*)malloc(buffer_size);
        memcpy((void*)font->m_Data, buffer, buffer_size);
        font->m_Allocated = 1;
    }
    else
    {
        font->m_Data    = buffer;
        font->m_DataSize= buffer_size;
    }

    int index = stbtt_GetFontOffsetForIndex((const unsigned char*)font->m_Data,0);
    int result = stbtt_InitFont(&font->m_Font, (const unsigned char*)font->m_Data, index);
    if (!result)
    {
        dmLogError("Failed to load font from '%s'", path);
        DestroyFontTTF((HFont)font);
        delete font;
        return 0;
    }

    stbtt_GetFontVMetrics(&font->m_Font, &font->m_Ascent, &font->m_Descent, &font->m_LineGap);
    font->m_Path = strdup(path);
    return (HFont)font;
}

HFont LoadFontFromMemoryTTF(const char* path, const void* buffer, uint32_t buffer_size, bool allocate)
{
    return LoadTTFInternal(path, buffer, buffer_size, allocate);
}


uint32_t GetResourceSizeTTF(HFont hfont)
{
    TTFFont* font = ToFont(hfont);
    return font->m_DataSize;
}

float GetPixelScaleFromSizeTTF(HFont hfont, uint32_t size)
{
    TTFFont* font = ToFont(hfont);
    return stbtt_ScaleForMappingEmToPixels(&font->m_Font, (int)size);
}

float GetAscentTTF(HFont hfont, float scale)
{
    TTFFont* font = ToFont(hfont);
    return font->m_Ascent * scale;
}

float GetDescentTTF(HFont hfont, float scale)
{
    TTFFont* font = ToFont(hfont);
    return font->m_Descent * scale;
}

float GetLineGapTTF(HFont hfont, float scale)
{
    TTFFont* font = ToFont(hfont);
    return font->m_LineGap * scale;
}

FontResult FreeGlyphTTF(HFont hfont, Glyph* glyph)
{
    (void)hfont;
    stbtt_FreeSDF(glyph->m_Bitmap.m_Data, 0);
    return RESULT_OK;
}

FontResult GetGlyphTTF(HFont hfont, uint32_t codepoint, const GlyphOptions* options, Glyph* glyph)
{
    TTFFont* font = ToFont(hfont);

    memset(glyph, 0, sizeof(*glyph));
    glyph->m_Codepoint = codepoint;

    stbtt_fontinfo* info = &font->m_Font;
    int glyph_index = stbtt_FindGlyphIndex(info, (int)codepoint);
    if (!glyph_index)
    {
        return RESULT_NOT_SUPPORTED;
    }

    int advx, lsb;
    stbtt_GetGlyphHMetrics(info, glyph_index, &advx, &lsb);

    int x0, y0, x1, y1;
    stbtt_GetGlyphBox(info, glyph_index, &x0, &y0, &x1, &y1);

    float scale = options->m_Scale;
    float padding = options->m_StbttSDFPadding;
    int on_edge_value = options->m_StbttSDFOnEdgeValue;

    int ascent = 0;
    int descent = 0;
    int srcw = 0;
    int srch = 0;
    int offsetx = 0;
    int offsety = 0;

    if (options->m_GenerateImage)
    {
        float pixel_dist_scale = (float)on_edge_value/padding;

        glyph->m_Bitmap.m_Data = stbtt_GetGlyphSDF(info, scale, glyph_index, (int)padding, on_edge_value, pixel_dist_scale,
                                                   &srcw, &srch, &offsetx, &offsety);

        if (glyph->m_Bitmap.m_Data)
        {
            glyph->m_Bitmap.m_Flags = dmFont::GLYPH_BM_FLAG_COMPRESSION_NONE;
            glyph->m_Bitmap.m_Width = srcw;
            glyph->m_Bitmap.m_Height = srch;
            glyph->m_Bitmap.m_Channels = 1;

            // We don't call stbtt_FreeSDF(src, 0);
            // But instead let the user call FreeGlyphTTF()

            ascent = -offsety;
            descent = srch - ascent;
        }
    }

    // The dimensions of the visible area
    if (x0 != x1 && y0 != y1)
    {
        // Only modify non empty glyphs (from stbtt_GetGlyphSDF())
        x0 -= padding;
        y0 -= padding;
        x1 += padding;
        y1 += padding;
    }


    glyph->m_Width = (x1 - x0) * scale;
    glyph->m_Height = (y1 - y0) * scale;
    glyph->m_Advance = advx*scale;;
    glyph->m_LeftBearing = lsb*scale;
    glyph->m_Ascent = ascent;
    glyph->m_Descent = descent;

    return RESULT_OK;
}

} // namespace
