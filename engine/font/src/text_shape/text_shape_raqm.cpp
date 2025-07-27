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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
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

#include "external/raqm.h"

static bool SegmentText(HFont hfont, raqm_t* rq, uint32_t* codepoints, uint32_t num_codepoints,
                        TextShapeInfo* info)
{
    bool result = raqm_set_text(rq, codepoints, num_codepoints);
    if (!result)
        return result;

    hb_font_t* hbfont = (hb_font_t*)FontGetHarfbuzzFontFromTTF(hfont);
    result = raqm_set_hb_font(rq, hbfont);
    if (!result)
        return result;

    result = raqm_layout(rq);
    if (!result)
        return result;

    uint32_t num_whitespaces = 0;

    info->m_Direction = RAQM_DIRECTION_RTL == raqm_get_par_resolved_direction(rq) ?
                            TEXT_DIRECTION_RTL : TEXT_DIRECTION_LTR;

    int dir = TEXT_DIRECTION_RTL == info->m_Direction ? -1 : 1;

    size_t count;
    raqm_glyph_t *glyphs = raqm_get_glyphs(rq, &count);

    if (count == 0)
    {
        return true;
    }

    uint32_t remaining = info->m_Glyphs.Remaining();
    if (remaining < count)
    {
        info->m_Glyphs.OffsetCapacity(count - remaining);
    }

    int32_t x = 0;
    int32_t y = 0;
    for (size_t c = 0, i = dir < 0 ? count-1 : 0; c < count; ++c, i+=dir)
    {
        int xx;
        int yy;
        if (dir < 0)
        {
            xx = x + glyphs[i].x_advance - glyphs[i].x_offset;
            yy = y + glyphs[i].y_advance + glyphs[i].y_offset;
            x += glyphs[i].x_advance;
            y += glyphs[i].y_advance;

            xx *= dir;
        }
        else
        {
            xx = x + glyphs[i].x_offset;
            yy = y + glyphs[i].y_offset;
            x += glyphs[i].x_advance;
            y += glyphs[i].y_advance;
        }

        uint32_t glyph_index = glyphs[i].index;

        int32_t width = 0;
        int32_t height = 0;
        int32_t x0, y0, x1, y1;
        if (FontGetGlyphBoxTTF(hfont, glyph_index, &x0, &y0, &x1, &y1))
        {
            width = (x1 - x0);
            height = (y1 - y0);
        }

        TextShapeGlyph glyph = {0};
        glyph.m_X           = xx;
        glyph.m_Y           = yy;
        glyph.m_GlyphIndex  = glyph_index;
        glyph.m_Cluster     = glyphs[i].cluster;
        glyph.m_Codepoint   = codepoints[glyph.m_Cluster];
        glyph.m_Width       = width;
        glyph.m_Height      = height;

        info->m_Glyphs.Push(glyph);

        num_whitespaces += dmUtf8::IsWhiteSpace(glyph.m_Codepoint);
    }

    info->m_NumValidGlyphs = info->m_Glyphs.Size() - num_whitespaces;

    return true;
}


static int32_t GetLineTextMetrics(TextShapeGlyph* glyphs, int32_t row_start, uint32_t n, TextDirection dir, bool measure_trailing_space)
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

    int32_t glyph_width;
    if (TEXT_DIRECTION_RTL == dir)
    {
        glyph_width = -glyphs[0].m_Width;
    }
    else
    {
        glyph_width = glyphs[n-1].m_Width;
    }

    int32_t row_start_x = glyphs[0].m_X;
    int32_t row_end_x   = glyphs[n-1].m_X + glyph_width;
    int32_t width = row_end_x - row_start_x;

    return width;
}

struct LayoutMetrics
{
    TextShapeGlyph* m_Glyphs;
    TextDirection   m_Direction;
    LayoutMetrics(TextShapeGlyph* glyphs, TextDirection dir)
        : m_Glyphs(glyphs)
        , m_Direction(dir)
        {}
    float operator()(int32_t row_start, uint32_t n, bool measure_trailing_space)
    {
        return GetLineTextMetrics(m_Glyphs, row_start, n, m_Direction, measure_trailing_space);
    }
};

static raqm_t* CreateRaqm(HFont font)
{
    raqm_t* rq = raqm_create();

    // TODO: get these from a settings struct?
    // raqm_set_par_direction(rq, RAQM_DIRECTION_LTR);
    // raqm_set_language()
    // raqm_add_font_feature()

    return rq;

raqm_init_fail:
    raqm_destroy(rq);
    return 0;
}

TextShapeResult TextShapeText(HFont font, uint32_t* codepoints, uint32_t num_codepoints, TextShapeInfo* info)
{
    info->m_Glyphs.SetSize(0);
    info->m_Runs.SetSize(0);
    info->m_NumValidGlyphs = 0;
    info->m_Font = font;

    raqm_t* rq = CreateRaqm(font);
    if (!rq)
        return TEXT_SHAPE_RESULT_ERROR;

    bool result = SegmentText(font, rq, codepoints, num_codepoints, info);
    return result ? TEXT_SHAPE_RESULT_OK : TEXT_SHAPE_RESULT_ERROR;
}

TextShapeResult TextLayout(TextMetricsSettings* settings, TextShapeInfo* info,
                            TextLine* lines, uint32_t max_num_lines,
                            TextMetrics* metrics)
{
    uint32_t width = settings->m_Width;
    if (!settings->m_LineBreak)
        width = INT_MAX;

    LayoutMetrics lm(info->m_Glyphs.Begin(), info->m_Direction);
    int32_t max_line_width;
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

