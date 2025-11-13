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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h> // INT_MAX

#include <dlib/log.h>
#include <dlib/profile.h>
#include <dlib/math.h>
#include <dlib/time.h>
#include <dlib/utf8.h>

#include <dmsdk/font/font.h>
#include <dmsdk/font/fontcollection.h>

#include "text_layout.h"

static inline uint32_t NextBreak(TextGlyph* glyphs, uint32_t num_glyphs, uint32_t* cursor, uint32_t* n)
{
    uint32_t c = 0;
    do
    {
        c = (*cursor) < num_glyphs ? glyphs[(*cursor)++].m_Codepoint : 0;
        if (c != 0)
            *n = *n + 1;
    } while (c != 0 && !dmUtf8::IsBreaking(c));
    return c;
}

static inline uint32_t SkipWS(TextGlyph* glyphs, uint32_t num_glyphs, uint32_t* cursor, uint32_t* n)
{
    uint32_t c = 0;
    do
    {
        c = (*cursor) < num_glyphs ? glyphs[(*cursor)++].m_Codepoint : 0;
        if (c != 0)
            *n = *n + 1;
    } while (c != 0 && (c == dmUtf8::UTF_WHITESPACE_SPACE || c == dmUtf8::UTF_WHITESPACE_ZERO_WIDTH_SPACE));

    return c;
}

/*
 * Simple text-layout.
 * Single trailing white-space is not accounted for when breaking but the count is included in the lines array
 * and should be skipped when rendering
 */
template <typename Metric>
void Layout(TextLayout*     layout,
                float       width,
                float*      text_width,
                Metric      metrics,
                bool        measure_trailing_space)
{
    TextGlyph*  glyphs = layout->m_Glyphs.Begin();
    uint32_t    num_glyphs = layout->m_Glyphs.Size();
    uint32_t    cursor = 0;
    float       max_width = 0;
    uint32_t    c;
    do
    {
        uint32_t n = 0, last_n = 0;
        uint32_t row_start = cursor;
        uint32_t last_cursor = cursor;
        float    w = 0, last_w = 0;
        do
        {
            c = NextBreak(glyphs, num_glyphs, &cursor, &n);
            if (n > 0)
            {
                int trim = 0;
                if (c != 0)
                    trim = 1;
                w = metrics(row_start, n - trim, measure_trailing_space);
                if (dmMath::Abs(w) <= width)
                {
                    last_n = n - trim;
                    last_w = w;
                    last_cursor = cursor;
                    if (c != '\n' && !measure_trailing_space)
                        c = SkipWS(glyphs, num_glyphs, &cursor, &n);
                }
                else if (last_n != 0)
                {
                    // rewind if we have more to scan
                    cursor = last_cursor;
                    c = glyphs[last_cursor++].m_Codepoint;
                }
            }
        } while (dmMath::Abs(w) <= width && c != 0 && c != '\n');
        if (dmMath::Abs(w) > width && last_n == 0)
        {
            int trim = 0;
            if (c != 0)
                trim = 1;
            last_n = n - trim;
            last_w = w;
        }

        if ((c != 0 || last_n > 0))
        {
            if (layout->m_Lines.Full())
                layout->m_Lines.OffsetCapacity(8);

            TextLine line = {0};
            line.m_Width = last_w;
            line.m_Index = row_start;
            line.m_Length = last_n;
            layout->m_Lines.Push(line);

            if (last_w < 0)
                max_width = dmMath::Min(max_width, last_w);
            else
                max_width = dmMath::Max(max_width, last_w);
        }
    } while (c);

    *text_width = max_width;
}

static float GetLineTextMetrics(TextGlyph* glyphs, uint32_t row_start, uint32_t n, bool monospace, float padding, float tracking, bool measure_trailing_space)
{
    if (n <= 0)
        return 0;

    glyphs += row_start;
    if (!measure_trailing_space)
    {
        if (glyphs[n-1].m_Codepoint == dmUtf8::UTF_WHITESPACE_SPACE)
        {
            --n;
            if (n <= 0)
                return 0;
        }
    }

    TextGlyph& last = glyphs[n-1];

    float row_start_x = glyphs[0].m_X;

    if (monospace)
    {
        float extent_last = last.m_Advance + padding;
        float width = last.m_X - row_start_x + (n-1) * tracking + extent_last;
        return width;
    }

    // the extent of the last character is left bearing + width
    float extent_last = last.m_LeftBearing + last.m_Width;
    float width = last.m_X - row_start_x + (n-1) * tracking + extent_last;

    return width;
}

struct LayoutMetrics
{
    TextGlyph*  m_Glyphs;
    bool        m_Monospace;
    float       m_Padding;
    float       m_Tracking;
    LayoutMetrics(TextGlyph* glyphs, bool monospace, float padding, float tracking)
    : m_Glyphs(glyphs)
    , m_Monospace(monospace)
    , m_Padding(padding)
    , m_Tracking(tracking)
    {}
    float operator()(uint32_t row_start, uint32_t n, bool measure_trailing_space)
    {
        return GetLineTextMetrics(m_Glyphs, row_start, n, m_Monospace, m_Padding, m_Tracking, measure_trailing_space);
    }
};

static void TextLayoutLegacyFree(TextLayout* layout)
{
    delete layout;
}

TextResult TextLayoutLegacyCreate(HFontCollection collection,
                            uint32_t* codepoints, uint32_t num_codepoints,
                            TextLayoutSettings* settings, HTextLayout* outlayout)
{
    TextLayout* layout = new TextLayout;
    layout->m_Free = TextLayoutLegacyFree;

    layout->m_Glyphs.SetCapacity(num_codepoints);
    layout->m_Glyphs.SetSize(num_codepoints);
    layout->m_Lines.SetSize(0);
    layout->m_FontCollection = collection;
    layout->m_Direction = TEXT_DIRECTION_LTR;
    layout->m_NumValidGlyphs = 0;

    HFont font = FontCollectionGetFont(collection, 0);
    float scale = FontGetScaleFromSize(font, settings->m_Size);

    FontGlyphOptions options;
    options.m_Scale = 1.0f; // Return in points
    options.m_GenerateImage = false;

    uint32_t num_whitespaces = 0;
    // Lay them all out in a single line, using points
    uint32_t x = 0;
    uint32_t y = 0; // the legacy "shaping" doesn't support Y offsets
    uint32_t fallback_codepoint = 126; // '~'   ;
    FontGlyph font_glyph;
    for (uint32_t i = 0; i < num_codepoints; ++i)
    {
        uint32_t c = codepoints[i];
        FontResult r = FontGetGlyph(font, c, &options, &font_glyph);

        if (FONT_RESULT_OK != r && fallback_codepoint)
        {
            r = FontGetGlyph(font, fallback_codepoint, &options, &font_glyph);
        }

        TextGlyph g = {0};
        g.m_Font = font;
        if (FONT_RESULT_OK == r)
        {
            g.m_Cluster = i;
            g.m_Codepoint = font_glyph.m_Codepoint;
            g.m_GlyphIndex = font_glyph.m_GlyphIndex;
            g.m_X = x;
            g.m_Y = y;
            g.m_Width = font_glyph.m_Width * scale;
            g.m_Height = font_glyph.m_Height * scale;
            g.m_Advance = font_glyph.m_Advance * scale;
            g.m_LeftBearing = font_glyph.m_LeftBearing * scale;

            x += g.m_Advance;
        }

        num_whitespaces += dmUtf8::IsWhiteSpace(c);

        layout->m_Glyphs[i] = g;
    }

    layout->m_NumValidGlyphs = layout->m_Glyphs.Size() - num_whitespaces;

    LayoutMetrics lm(layout->m_Glyphs.Begin(), settings->m_Monospace, settings->m_Padding, settings->m_Tracking);
    float max_line_width;

    float width = settings->m_Width;
    if (!settings->m_LineBreak)
        width = 1000000.0f;
    Layout(layout, width, &max_line_width, lm, !settings->m_LineBreak);

    uint32_t ascent = (uint32_t)FontGetAscent(font, 1.0f);
    uint32_t descent = (uint32_t)FontGetDescent(font, 1.0f); // positive value
    uint32_t line_height = ascent + descent;

    // metrics->m_MaxAscent = ascent;
    // metrics->m_MaxDescent = descent;
    uint32_t num_lines = layout->m_Lines.Size();
    layout->m_Width = max_line_width;
    layout->m_Height = num_lines * (line_height * settings->m_Leading) - line_height * (settings->m_Leading - 1.0f);

    *outlayout = layout;
    return TEXT_RESULT_OK;
}

