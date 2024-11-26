// Copyright 2020-2024 The Defold Foundation
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

#ifndef DM_FONT_RENDERER_PRIVATE
#define DM_FONT_RENDERER_PRIVATE

#include "font_renderer.h"
#include <dlib/utf8.h>
#include <dlib/math.h>

namespace dmRender
{
    const uint32_t ZERO_WIDTH_SPACE_UNICODE = 0x200B;

    enum RenderLayerMask
    {
        FACE    = 0x1,
        OUTLINE = 0x2,
        SHADOW  = 0x4
    };

    // We only store glyphs for our cache
    struct CacheGlyph
    {
        dmRender::FontGlyph* m_Glyph;
        uint32_t             m_Frame;
        int16_t              m_X;
        int16_t              m_Y;
    };

    struct FontMap
    {
        FontMap()
        : m_UserData(0)
        , m_Texture(0)
        , m_Material(0)
        , m_NameHash(0)
        , m_GetGlyph(0)
        , m_GetGlyphData(0)
        , m_ShadowX(0.0f)
        , m_ShadowY(0.0f)
        , m_MaxAscent(0.0f)
        , m_MaxDescent(0.0f)
        , m_CellTempData(0)
        , m_Cache(0)
        , m_CacheIndices(0)
        , m_CacheCursor(0)
        , m_CacheWidth(0)
        , m_CacheHeight(0)
        , m_CacheCellWidth(0)
        , m_CacheCellHeight(0)
        , m_CacheCellMaxAscent(0)
        , m_CacheColumns(0)
        , m_CacheRows(0)
        , m_CacheCellCount(0)
        , m_CacheCellPadding(0)
        , m_LayerMask(FACE)
        , m_IsMonospaced(false)
        , m_Padding(0)
        {
        }

        ~FontMap()
        {
            free(m_CacheIndices);
            m_CacheIndices = 0;

            free(m_Cache);
            m_Cache = 0;

            free(m_CellTempData);
            m_CellTempData = 0;

            dmGraphics::DeleteTexture(m_Texture);
        }

        void*                   m_UserData; // The font map resources (see res_font.cpp)
        dmGraphics::HTexture    m_Texture;
        HMaterial               m_Material;
        dmhash_t                m_NameHash;

        FGetGlyph               m_GetGlyph;
        FGetGlyphData           m_GetGlyphData;

        float                   m_ShadowX;
        float                   m_ShadowY;
        float                   m_MaxAscent;
        float                   m_MaxDescent;
        float                   m_SdfSpread;
        float                   m_SdfOffset;
        float                   m_SdfOutline;
        float                   m_SdfShadow;
        float                   m_Alpha;
        float                   m_OutlineAlpha;
        float                   m_ShadowAlpha;

        uint8_t*                m_CellTempData; // a temporary unpack buffer for the compressed glyphs

        dmHashTable32<CacheGlyph*>  m_GlyphCache;   // Quick check what glyphs are in the cache
        CacheGlyph*                 m_Cache;        // The data (i.e. the pool)
        uint16_t*                   m_CacheIndices; // Indices into the cache array
        uint32_t                    m_CacheCursor;

        dmGraphics::TextureFormat m_CacheFormat;
        dmGraphics::TextureFilter m_MinFilter;
        dmGraphics::TextureFilter m_MagFilter;

        uint32_t                m_CacheWidth;           // In texels
        uint32_t                m_CacheHeight;          // In texels
        uint32_t                m_CacheCellWidth;       // In texels
        uint32_t                m_CacheCellHeight;      // In texels
        uint32_t                m_CacheCellMaxAscent;   // In texels
        uint32_t                m_CacheColumns;         // Number of cells in horizontal direction
        uint32_t                m_CacheRows;            // Number of cells in horizontal direction
        uint32_t                m_CacheCellCount;       // Number of cells in total
        uint8_t                 m_CacheChannels;        // Number of channels
        uint8_t                 m_CacheCellPadding;
        uint8_t                 m_LayerMask;
        uint8_t                 m_IsMonospaced:1;
        uint8_t                 m_Padding:7;
    };

    ///////////////////////////////////////////////////////////////////////////////
    // Default implementation (wip, should go into the font_renderer_default.cpp)
    float GetLineTextMetrics(HFontMap font_map, float tracking, const char* text, int n, bool measure_trailing_space);

    struct LayoutMetrics
    {
        HFontMap m_FontMap;
        float m_Tracking;
        LayoutMetrics(HFontMap font_map, float tracking) : m_FontMap(font_map), m_Tracking(tracking) {}
        float operator()(const char* text, uint32_t n, bool measure_trailing_space)
        {
            return GetLineTextMetrics(m_FontMap, m_Tracking, text, n, measure_trailing_space);
        }
    };


    static bool IsBreaking(uint32_t c)
    {
        return c == ' ' || c == '\n' || c == ZERO_WIDTH_SPACE_UNICODE;
    }

    static inline uint32_t NextBreak(const char** cursor, int* n) {
        uint32_t c = 0;
        do {
            c = dmUtf8::NextChar(cursor);
            if (c != 0)
                *n = *n + 1;
        } while (c != 0 && !IsBreaking(c));
        return c;
    }

    static inline uint32_t SkipWS(const char** cursor, int* n) {
        uint32_t c = 0;
        do {
            c = dmUtf8::NextChar(cursor);
            if (c != 0)
                *n = *n + 1;
        } while (c != 0 && (c == ' ' || c == ZERO_WIDTH_SPACE_UNICODE));

        return c;
    }

    struct TextLine {
        float m_Width;
        uint16_t m_Index;
        uint16_t m_Count;
    };

    /*
     * Simple text-layout.
     * Single trailing white-space is not accounted for
     * when breaking but the count is included in the lines array
     * and should be skipped when rendering
     */
    template <typename Metric>
    uint32_t Layout(const char* str,
               float width,
               TextLine* lines, uint16_t lines_count,
               float* text_width,
               Metric metrics,
               bool measure_trailing_space)
    {
        const char* cursor = str;

        float max_width = 0;

        uint32_t l = 0;
        uint32_t c;
        do {
            int n = 0, last_n = 0;
            const char* row_start = cursor;
            const char* last_cursor = cursor;
            float w = 0.0f, last_w = 0.0f;
            do {
                c = NextBreak(&cursor, &n);
                if (n > 0)
                {
                    int trim = 0;
                    if (c != 0)
                        trim = 1;
                    w = metrics(row_start, n-trim, measure_trailing_space);
                    if (w <= width)
                    {
                        last_n = n-trim;
                        last_w = w;
                        last_cursor = cursor;
                        if (c != '\n' && !measure_trailing_space)
                            c = SkipWS(&cursor, &n);
                    }
                    else if (last_n != 0)
                    {
                        // rewind if we have more to scan
                        cursor = last_cursor;
                        c = dmUtf8::NextChar(&last_cursor);
                    }
                }
            } while (w <= width && c != 0 && c != '\n');
            if (w > width && last_n == 0)
            {
                int trim = 0;
                if (c != 0)
                    trim = 1;
                last_n = n-trim;
                last_w = w;
            }

            if (l < lines_count && (c != 0 || last_n > 0)) {
                TextLine& tl = lines[l];
                tl.m_Width = last_w;
                tl.m_Index = (row_start - str);
                tl.m_Count = last_n;
                l++;
                max_width = dmMath::Max(max_width, last_w);
            } else {
                // Out of lines
            }
        } while (c);

        *text_width = max_width;
        return l;
    }

    // Helper to calculate horizontal pivot point
    static inline float OffsetX(uint32_t align, float width)
    {
        switch (align)
        {
            case TEXT_ALIGN_LEFT:
                return 0.0f;
            case TEXT_ALIGN_CENTER:
                return width * 0.5f;
            case TEXT_ALIGN_RIGHT:
                return width;
            default:
                return 0.0f;
        }
    }

    // Helper to calculate vertical pivot point
    static inline float OffsetY(uint32_t valign, float height, float ascent, float descent, float leading, uint32_t line_count)
    {
        float line_height = ascent + descent;
        switch (valign)
        {
            case TEXT_VALIGN_TOP:
                return height - ascent;
            case TEXT_VALIGN_MIDDLE:
                return height * 0.5f + (line_count * (line_height * leading) - line_height * (leading - 1.0f)) * 0.5f - ascent;
            case TEXT_VALIGN_BOTTOM:
                return (line_height * leading * (line_count - 1)) + descent;
            default:
                return height - ascent;
        }
    }

    // Used in unit tests
    bool VerifyFontMapMinFilter(dmRender::HFontMap font_map, dmGraphics::TextureFilter filter);
    bool VerifyFontMapMagFilter(dmRender::HFontMap font_map, dmGraphics::TextureFilter filter);

    dmRender::FontGlyph* GetGlyph(dmRender::HFontMap font_map, uint32_t codepoint);
    const uint8_t*       GetGlyphData(dmRender::HFontMap font_map, uint32_t codepoint, uint32_t* out_size, uint32_t* out_compression, uint32_t* out_width, uint32_t* out_height);
}

#endif // #ifndef DM_FONT_RENDERER_PRIVATE
