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
#define STBTT_STATIC
#include "external/stb_truetype.h"

#include "font_private.h"

#if defined(FONT_USE_HARFBUZZ)
    #include <harfbuzz/hb.h>
#endif

struct TTFFont
{
    Font            m_Base;

    stbtt_fontinfo  m_Font;

#if defined(FONT_USE_HARFBUZZ)
    hb_font_t*      m_HBFont;
#endif

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

static void FontDestroyTTF(HFont hfont)
{
    TTFFont* font = ToFont(hfont);

#if defined(FONT_USE_HARFBUZZ)
    hb_font_destroy(font->m_HBFont);
#endif

    if (font->m_Allocated)
    {
        free((void*)font->m_Data);
    }
    memset(font, 0, sizeof(*font));
    delete font;
}

uint32_t GetResourceSizeTTF(HFont hfont)
{
    TTFFont* font = ToFont(hfont);
    return font->m_DataSize;
}

static float GetScaleFromSizeTTF(HFont hfont, uint32_t size)
{
    TTFFont* font = ToFont(hfont);
    return stbtt_ScaleForMappingEmToPixels(&font->m_Font, (int)size);
}

static float GetAscentTTF(HFont hfont, float scale)
{
    TTFFont* font = ToFont(hfont);
    return font->m_Ascent * scale;
}

static float GetDescentTTF(HFont hfont, float scale)
{
    TTFFont* font = ToFont(hfont);
    return font->m_Descent * scale;
}

static float GetLineGapTTF(HFont hfont, float scale)
{
    TTFFont* font = ToFont(hfont);
    return font->m_LineGap * scale;
}

static FontResult FreeGlyphTTF(HFont hfont, FontGlyph* glyph)
{
    (void)hfont;
    stbtt_FreeSDF(glyph->m_Bitmap.m_Data, 0);
    return FONT_RESULT_OK;
}

static uint32_t GetGlyphIndexTTF(HFont hfont, uint32_t codepoint)
{
    TTFFont* font = ToFont(hfont);
    stbtt_fontinfo* info = &font->m_Font;
    return (uint32_t)stbtt_FindGlyphIndex(info, (int)codepoint);
}

static FontResult GetGlyphTTF(HFont hfont, uint32_t glyph_index, const FontGlyphOptions* options, FontGlyph* glyph)
{
    TTFFont* font = ToFont(hfont);

    memset(glyph, 0, sizeof(*glyph));
    glyph->m_GlyphIndex = glyph_index;

    stbtt_fontinfo* info = &font->m_Font;

    int advx = 0, lsb = 0;
    stbtt_GetGlyphHMetrics(info, glyph_index, &advx, &lsb);

    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
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
            glyph->m_Bitmap.m_Flags = FONT_GLYPH_BM_FLAG_COMPRESSION_NONE;
            glyph->m_Bitmap.m_Width = srcw;
            glyph->m_Bitmap.m_Height = srch;
            glyph->m_Bitmap.m_Channels = 1;
            glyph->m_Bitmap.m_DataSize = srcw * srch * 1;

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
    glyph->m_Advance = advx*scale;
    glyph->m_LeftBearing = lsb*scale;
    glyph->m_Ascent = ascent;
    glyph->m_Descent = descent;

    return FONT_RESULT_OK;
}

static HFont LoadTTFInternal(const char* path, const void* buffer, uint32_t buffer_size, bool allocate);

HFont FontLoadFromMemoryTTF(const char* path, const void* buffer, uint32_t buffer_size, bool allocate)
{
    return LoadTTFInternal(path, buffer, buffer_size, allocate);
}

#if defined(FONT_USE_HARFBUZZ)
static int LoadHBFont(TTFFont* font, void* buffer, uint32_t buffer_size)
{
    hb_blob_t* blob = 0;
    hb_face_t* face = 0;
    int result = 0;

    blob = hb_blob_create((const char*)buffer, buffer_size, HB_MEMORY_MODE_READONLY, 0, 0);
    if (!blob) goto cleanup;

    face = hb_face_create(blob, 0);
    if (!face) goto cleanup;

    font->m_HBFont = hb_font_create(face);
    result = font->m_HBFont != 0;

cleanup:
    hb_face_destroy(face);
    hb_blob_destroy(blob);
    return result;
}
#endif

static HFont LoadTTFInternal(const char* path, const void* buffer, uint32_t buffer_size, bool allocate)
{
    TTFFont* font = new TTFFont;
    memset(font, 0, sizeof(*font));

    font->m_Base.m_LoadFontFromMemory = FontLoadFromMemoryTTF;
    font->m_Base.m_DestroyFont = FontDestroyTTF;
    font->m_Base.m_GetResourceSize = GetResourceSizeTTF;
    font->m_Base.m_GetScaleFromSize = GetScaleFromSizeTTF;
    font->m_Base.m_GetAscent = GetAscentTTF;
    font->m_Base.m_GetDescent = GetDescentTTF;
    font->m_Base.m_GetLineGap = GetLineGapTTF;
    font->m_Base.m_GetGlyphIndex = GetGlyphIndexTTF;
    font->m_Base.m_GetGlyph = GetGlyphTTF;
    font->m_Base.m_FreeGlyph = FreeGlyphTTF;

    if (allocate)
    {
        font->m_DataSize = buffer_size;
        font->m_Data     = (const void*)malloc(buffer_size);
        memcpy((void*)font->m_Data, buffer, buffer_size);
        font->m_Allocated = 1;
    }
    else
    {
        font->m_Data    = buffer;
        font->m_DataSize= buffer_size;
    }

#if defined(FONT_USE_HARFBUZZ)
    if (!LoadHBFont(font, (void*)font->m_Data, font->m_DataSize))
    {
        dmLogError("Failed to create Harfbuzz font from '%s'", path);
        FontDestroyTTF((HFont)font);
        delete font;
        return 0;
    }
#endif

    int index = stbtt_GetFontOffsetForIndex((const unsigned char*)font->m_Data,0);
    int result = stbtt_InitFont(&font->m_Font, (const unsigned char*)font->m_Data, index);
    if (!result)
    {
        dmLogError("Failed to load font from '%s'", path);
        FontDestroyTTF((HFont)font);
        delete font;
        return 0;
    }

    stbtt_GetFontVMetrics(&font->m_Font, &font->m_Ascent, &font->m_Descent, &font->m_LineGap);
    return (HFont)font;
}

#if defined(FONT_USE_HARFBUZZ)
hb_font_t* FontGetHarfbuzzFontFromTTF(HFont hfont)
{
    TTFFont* font = ToFont(hfont);
    return font->m_HBFont;
}
#endif

bool FontGetGlyphBoxTTF(HFont hfont, uint32_t glyph_index, int32_t* x0, int32_t* y0, int32_t* x1, int32_t* y1)
{
    TTFFont* font = ToFont(hfont);
    x0 = y0 = x1 = y1 = 0;
    return stbtt_GetGlyphBox(&font->m_Font, glyph_index, x0, y0, x1, y1);
}
