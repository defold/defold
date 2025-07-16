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
#include "../font_ttf.h"

#include "text_shape.h"
#include "text_layout.h"

static uint32_t GetLineTextMetrics(TextShapeGlyph* glyphs, uint32_t row_start, uint32_t n, bool measure_trailing_space)
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

    uint32_t row_start_x = glyphs[0].m_X;
    uint32_t width = glyphs[n-1].m_X + glyphs[n-1].m_Width - row_start_x;

    // if (n > 0 && 0 != last)
    // {
    //
    //     if (font_map->m_IsMonospaced) {
    //         width += font_map->m_Padding;  TODO: Move this to the m_Width of the actual font
    //     }
    //     else {
    //         float last_width = (measure_trailing_space && last->m_Character == ' ') ? last->m_Advance : last->m_Width;
    //         float last_end_point = last->m_LeftBearing + last_width;
    //         float last_right_bearing = last->m_Advance - last_end_point;
    //         width = width - last_right_bearing;
    //     }
    //     width -= tracking;
    // }

    return width;
}

struct LayoutMetrics
{
    TextShapeGlyph* m_Glyphs;
    LayoutMetrics(TextShapeGlyph* glyphs) : m_Glyphs(glyphs) {}
    float operator()(uint32_t row_start, uint32_t n, bool measure_trailing_space)
    {
        return GetLineTextMetrics(m_Glyphs, row_start, n, measure_trailing_space);
    }
};


TextShapeResult TextShapeText(HFont font, uint32_t* codepoints, uint32_t num_codepoints, TextShapeInfo* info)
{
    info->m_Glyphs.SetSize(0);
    info->m_Runs.SetSize(0);
    info->m_NumValidGlyphs = 0;
    info->m_Font = font;

    info->m_Glyphs.OffsetCapacity(info->m_Glyphs.Capacity() - num_codepoints);
    info->m_Glyphs.SetSize(num_codepoints);

    FontGlyphOptions options;
    options.m_Scale = 1.0f; // Return in points
    options.m_GenerateImage = false;

    uint32_t num_whitespaces = 0;
    // Lay them all out in a single line, using points
    uint32_t x = 0;
    uint32_t y = 0; // the legacy "shaping" doesn't support Y offsets
    for (uint32_t i = 0; i < num_codepoints; ++i)
    {
        uint32_t c = codepoints[i];
        FontGlyph font_glyph;
        FontResult r = FontGetGlyph(font, c, &options, &font_glyph);

        TextShapeGlyph g = {0};
        if (FONT_RESULT_OK == r)
        {
            g.m_X = x;
            g.m_Y = y;
            g.m_Width = font_glyph.m_Width;
            g.m_Height = font_glyph.m_Height;
            g.m_Codepoint = font_glyph.m_Codepoint;
            g.m_GlyphIndex = font_glyph.m_GlyphIndex;

            x += font_glyph.m_Advance;
        }

        num_whitespaces += dmUtf8::IsWhiteSpace(c);

        info->m_Glyphs[i] = g;
    }

    info->m_NumValidGlyphs = info->m_Glyphs.Size() - num_whitespaces;

    if (info->m_Runs.Full())
        info->m_Runs.SetCapacity(1);

    TextRun run;
    run.m_Index = 0;
    run.m_Length = num_codepoints;
    info->m_Runs.Push(run);

    return TEXT_SHAPE_RESULT_OK;
}

TextShapeResult TextLayout(TextMetricsSettings* settings, TextShapeInfo* info,
                            TextLine* lines, uint32_t max_num_lines,
                            TextMetrics* metrics)
{
    uint32_t width = settings->m_Width;
    if (!settings->m_LineBreak)
        width = INT_MAX;

    LayoutMetrics lm(info->m_Glyphs.Begin());
    uint32_t max_line_width;
    uint32_t num_lines = Layout(info, width, lines, max_num_lines,
                                &max_line_width, lm, !settings->m_LineBreak);
    if (metrics)
    {
        uint32_t ascent = (uint32_t)FontGetAscent(info->m_Font, 1.0f);
        uint32_t descent = (uint32_t)FontGetDescent(info->m_Font, 1.0f); // positive value
        uint32_t line_height = ascent + descent;

        metrics->m_MaxAscent = ascent;
        metrics->m_MaxDescent = descent;
        metrics->m_Width = max_line_width;
        metrics->m_Height = num_lines * (line_height * settings->m_Leading) - line_height * (settings->m_Leading - 1.0f);
        metrics->m_LineCount = num_lines;
    }
    return TEXT_SHAPE_RESULT_OK;
}

