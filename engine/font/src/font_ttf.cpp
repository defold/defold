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

#include <math.h>
#include <stdlib.h> // free

#include "font_private.h"
#include "font_outline.h"
#include "font_sdf.h"

#if defined(FONT_USE_HARFBUZZ)
    #include "harfbuzz/font_harfbuzz.h"

    typedef FontHarfbuzz FontImpl;
    #define FontImplCreate               FontHarfbuzzCreate
    #define FontImplDestroy              FontHarfbuzzDestroy
    #define FontImplGetScaleFromSize     FontHarfbuzzGetScaleFromSize
    #define FontImplGetGlyphIndex        FontHarfbuzzGetGlyphIndex
    #define FontImplGetGlyphOutline      FontHarfbuzzGetGlyphOutline
    #define FontImplGetGlyphHMetrics     FontHarfbuzzGetGlyphHMetrics
    #define FontImplGetGlyphBox          FontHarfbuzzGetGlyphBox
    #define FontImplGetOutlineType       FontHarfbuzzGetOutlineType
    #define FontImplGetVerticalMetrics   FontHarfbuzzGetVerticalMetrics

#else
    #include "truetype/font_truetype.h"

    typedef FontTrueType FontImpl;
    #define FontImplCreate               FontTrueTypeCreate
    #define FontImplDestroy              FontTrueTypeDestroy
    #define FontImplGetScaleFromSize     FontTrueTypeGetScaleFromSize
    #define FontImplGetGlyphIndex        FontTrueTypeGetGlyphIndex
    #define FontImplGetGlyphOutline      FontTrueTypeGetGlyphOutline
    #define FontImplGetGlyphHMetrics     FontTrueTypeGetGlyphHMetrics
    #define FontImplGetGlyphBox          FontTrueTypeGetGlyphBox
    #define FontImplGetOutlineType       FontTrueTypeGetOutlineType
    #define FontImplGetVerticalMetrics   FontTrueTypeGetVerticalMetrics
#endif

struct TTFFont
{
    Font            m_Base;

    FontImpl*       m_Font;

    const char*     m_Path;
    const void*     m_Data;
    uint32_t        m_DataSize;

    int             m_Ascent;
    int             m_Descent;
    int             m_LineGap;
    uint32_t        m_Allocated:1;
    uint32_t        m_HasGlyfOutlines:1;
};

static inline TTFFont* ToFont(HFont hfont)
{
    return (TTFFont*)hfont;
}

static void FontDestroyTTF(HFont hfont)
{
    TTFFont* font = ToFont(hfont);

    FontImplDestroy(font->m_Font);

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
    return FontImplGetScaleFromSize(font->m_Font, size);
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
    FontSDFFree(&glyph->m_Bitmap);
    return FONT_RESULT_OK;
}

static uint32_t GetGlyphIndexTTF(HFont hfont, uint32_t codepoint)
{
    TTFFont* font = ToFont(hfont);
    return FontImplGetGlyphIndex(font->m_Font, codepoint);
}

static FontResult GetGlyphOutlineTTF(HFont hfont, uint32_t glyph_index, FontOutline* outline)
{
    FontResult result = FontImplGetGlyphOutline(ToFont(hfont)->m_Font, glyph_index, outline);
    if (result != FONT_RESULT_OK)
        return result;

    result = FontOutlineMakeYMonotonic(outline);
    if (result != FONT_RESULT_OK)
        FontFreeGlyphOutline(outline);

    return result;
}

static FontResult GetGlyphTTF(HFont hfont, uint32_t glyph_index, const FontGlyphOptions* options, FontGlyph* glyph)
{
    TTFFont* font = ToFont(hfont);

    memset(glyph, 0, sizeof(*glyph));
    glyph->m_GlyphIndex = glyph_index;

    int advx = 0;
    int lsb = 0;
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    // A glyf header provides bounds without decoding or traversing an outline.
    // CFF has no stored per-glyph box, so reuse the outline that image
    // generation already needs instead of interpreting its CharString twice.
    bool bounds_from_outline = options->m_GenerateImage && !font->m_HasGlyfOutlines;
    FontImplGetGlyphHMetrics(font->m_Font, glyph_index, &advx, &lsb);
    if (!bounds_from_outline)
        FontImplGetGlyphBox(font->m_Font, glyph_index, &x0, &y0, &x1, &y1);

    float scale = options->m_Scale;
    float padding = options->m_StbttSDFPadding;
    int on_edge_value = options->m_StbttSDFOnEdgeValue;

    int ascent = 0;
    int descent = 0;
    int srch = 0;
    int offsetx = 0;
    int offsety = 0;

    if (options->m_GenerateImage)
    {
        FontOutline outline = {};
        FontResult outline_result = GetGlyphOutlineTTF(hfont, glyph_index, &outline);
        if (outline_result != FONT_RESULT_OK)
            return outline_result;

        if (bounds_from_outline)
        {
            float fx0, fy0, fx1, fy1;
            if (FontGetOutlineBounds(&outline, &fx0, &fy0, &fx1, &fy1))
            {
                x0 = (int32_t)floorf(fx0);
                y0 = (int32_t)floorf(fy0);
                x1 = (int32_t)ceilf(fx1);
                y1 = (int32_t)ceilf(fy1);
            }
        }

        FontSDFParams sdf_params;
        sdf_params.m_Scale = scale;
        sdf_params.m_Spread = (uint32_t)padding;
        sdf_params.m_OnEdgeValue = on_edge_value;
        FontResult result = FontSDFGenerate(&outline, &sdf_params, &glyph->m_Bitmap, &offsetx, &offsety);
        if (result != FONT_RESULT_OK)
        {
            FontFreeGlyphOutline(&outline);
            return result;
        }

        if (glyph->m_Bitmap.m_Data)
        {
            srch = glyph->m_Bitmap.m_Height;
            ascent = -offsety;
            descent = srch - ascent;
        }

        FontFreeGlyphOutline(&outline);
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

    font->m_Font = FontImplCreate(font->m_Data, font->m_DataSize, 0);
    if (!font->m_Font)
    {
        dmLogError("Failed to load font from '%s'", path);
        FontDestroyTTF((HFont)font);
        return 0;
    }

    font->m_HasGlyfOutlines = FontImplGetOutlineType(font->m_Font) == FONT_OUTLINE_TYPE_GLYF;
    bool has_vertical_metrics = FontImplGetVerticalMetrics(font->m_Font, &font->m_Ascent, &font->m_Descent, &font->m_LineGap);
    if (!has_vertical_metrics)
    {
        dmLogError("Failed to load font metrics from '%s'", path);
        FontDestroyTTF((HFont)font);
        return 0;
    }

    return (HFont)font;
}

bool FontGetGlyphBoxTTF(HFont hfont, uint32_t glyph_index, int32_t* x0, int32_t* y0, int32_t* x1, int32_t* y1)
{
    return FontImplGetGlyphBox(ToFont(hfont)->m_Font, glyph_index, x0, y0, x1, y1);
}

#if defined(FONT_USE_HARFBUZZ)
hb_font_t* FontGetHarfbuzzFontFromTTF(HFont hfont)
{
    return FontHarfbuzzGetFont(ToFont(hfont)->m_Font);
}
#endif

FontResult FontGetGlyphSDFMetricsTTF(HFont hfont, uint32_t glyph_index, float scale, float padding, FontGlyph* glyph)
{
    if (!hfont || !glyph || glyph_index == 0 || scale <= 0.0f || padding <= 0.0f)
        return FONT_RESULT_ERROR;

    TTFFont* font = ToFont(hfont);
    memset(glyph, 0, sizeof(*glyph));

    int advance = 0;
    int left_bearing = 0;
    FontImplGetGlyphHMetrics(font->m_Font, glyph_index, &advance, &left_bearing);

    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    int font_x0 = 0;
    int font_y0 = 0;
    int font_x1 = 0;
    int font_y1 = 0;
    if (FontImplGetGlyphBox(font->m_Font, glyph_index, &font_x0, &font_y0, &font_x1, &font_y1))
    {
        // Match stbtt_GetGlyphBitmapBoxSubpixel(): convert the font's
        // y-up coordinates to the bitmap's y-down pixel coordinates.
        x0 = (int)floorf(font_x0 * scale);
        y0 = (int)floorf(-font_y1 * scale);
        x1 = (int)ceilf(font_x1 * scale);
        y1 = (int)ceilf(-font_y0 * scale);
    }

    if (x0 != x1 && y0 != y1)
    {
        const int sdf_padding = (int)padding;
        x0 -= sdf_padding;
        y0 -= sdf_padding;
        x1 += sdf_padding;
        y1 += sdf_padding;
    }

    glyph->m_GlyphIndex = glyph_index;
    glyph->m_Width = (float)(x1 - x0);
    glyph->m_Height = (float)(y1 - y0);
    glyph->m_Advance = advance * scale;
    glyph->m_LeftBearing = left_bearing * scale;
    glyph->m_Ascent = -y0;
    glyph->m_Descent = y1;
    return FONT_RESULT_OK;
}
