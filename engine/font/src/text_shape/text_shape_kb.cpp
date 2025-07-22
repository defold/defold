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

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "external/kb_text_shape.h"


static uint32_t ShapeText(kbts_font* font, TextShapeInfo* info,
                            kbts_cursor* cursor, kbts_direction main_direction,
                            kbts_direction direction, kbts_script script,
                            uint32_t* codepoints, uint32_t num_codepoints,
                            TextRun* run)
{
    // Shapes a single run

    // TODO: Look into using some scratch buffer for this
    kbts_glyph* glyphs = (kbts_glyph*)malloc(sizeof(kbts_glyph) * num_codepoints);
    for (size_t i = 0; i < num_codepoints; ++i)
    {
        glyphs[i] = kbts_CodepointToGlyph(font, codepoints[i]);
    }

    kbts_shape_state* state = kbts_CreateShapeState(font);
    kbts_shape_config config = kbts_ShapeConfig(font, script, KBTS_LANGUAGE_DONT_KNOW);

    uint32_t glyph_count = num_codepoints;
    uint32_t glyph_capacity = glyph_count;
    while (kbts_Shape(state, &config, main_direction, direction, glyphs, &glyph_count, glyph_capacity))
    {
        // We need to loop as it may substitue one codepoint with many
        glyph_capacity = state->RequiredGlyphCapacity;
        glyphs = (kbts_glyph*)realloc(glyphs, sizeof(kbts_glyph) * glyph_capacity);
    }

    uint32_t remaining = info->m_Glyphs.Remaining();
    if (remaining < glyph_count)
    {
        info->m_Glyphs.OffsetCapacity(glyph_count - remaining);
    }

    TextShapeGlyph* out = info->m_Glyphs.End();
    info->m_Glyphs.SetSize(info->m_Glyphs.Size() + glyph_count);

    HFont hfont = info->m_Font;
    uint32_t num_whitespace = 0;
    for (size_t i = 0; i < glyph_count; ++i)
    {
        kbts_glyph* glyph = &glyphs[i];

        TextShapeGlyph& g = out[i];

        int x, y;
        kbts_PositionGlyph(cursor, glyph, &x, &y);

        int32_t width = 0;
        int32_t height = 0;
        int32_t x0, y0, x1, y1;
        if (FontGetGlyphBoxTTF(hfont, glyph->Id, &x0, &y0, &x1, &y1))
        {
            width = (x1 - x0);
            height = (y1 - y0);
        }

        g.m_Codepoint   = glyph->Codepoint;
        g.m_GlyphIndex  = glyph->Id;
        g.m_X           = x;
        g.m_Y           = y;
        g.m_Width       = (int16_t)width;
        g.m_Height      = (int16_t)height;

        num_whitespace += dmUtf8::IsWhiteSpace(glyph->Codepoint);
    }

    free((void*)glyphs);

    return num_whitespace;
}

// https://www.newroadoldway.com/text1.html
static void SegmentText(HFont hfont, uint32_t* codepoints, uint32_t num_codepoints,
                        TextShapeInfo* info)
{
    // Breaks up text into multiple "runs":
    //   "Runs are sequences of characters that share a common direction (left-to-right or right-to-left) as well as a common script"

    FontType font_type = FontGetType(hfont);
    assert(font_type == FONT_TYPE_STBTTF || font_type == FONT_TYPE_STBOTF);

    kbts_font* font = (kbts_font*)FontGetKbTextShapeFontFromTTF(hfont);
    assert(font != 0);

    kbts_cursor      Cursor = { 0 };
    kbts_direction   Direction = KBTS_DIRECTION_NONE;
    kbts_script      Script = KBTS_SCRIPT_DONT_KNOW;
    size_t           RunStart = 0;
    kbts_break_state BreakState;
    kbts_BeginBreak(&BreakState, KBTS_DIRECTION_NONE, KBTS_JAPANESE_LINE_BREAK_STYLE_NORMAL);

    uint32_t num_whitespaces = 0;
    for (size_t i = 0; i < num_codepoints; ++i)
    {
        kbts_BreakAddCodepoint(&BreakState, codepoints[i], 1, (i + 1) == num_codepoints);
        kbts_break Break;
        while (kbts_Break(&BreakState, &Break))
        {
            if ((Break.Position > RunStart) && (Break.Flags & (KBTS_BREAK_FLAG_DIRECTION | KBTS_BREAK_FLAG_SCRIPT | KBTS_BREAK_FLAG_LINE_HARD)))
            {
                size_t RunLength = Break.Position - RunStart;

                TextRun run = {0};
                // run.m_Script       = Script;
                // run.m_MainDirection= BreakState.MainDirection;
                // run.m_Direction    = Direction;
                run.m_Index     = info->m_Glyphs.Size();
                run.m_Length    = RunLength;

                uint32_t run_num_whitespaces = ShapeText(font, info, &Cursor, BreakState.MainDirection, Direction, Script,
                                                    codepoints + RunStart, RunLength, &run);

                if (info->m_Runs.Full())
                {
                    info->m_Runs.OffsetCapacity(16);
                }
                info->m_Runs.Push(run);
                num_whitespaces += run_num_whitespaces;

                RunStart = Break.Position;
            }

            if (Break.Flags & KBTS_BREAK_FLAG_DIRECTION)
            {
                Direction = Break.Direction;
                if (!Cursor.Direction)
                    Cursor = kbts_Cursor(BreakState.MainDirection);
            }
            if (Break.Flags & KBTS_BREAK_FLAG_SCRIPT)
            {
                Script = Break.Script;
            }
        }
    }

    info->m_NumValidGlyphs = info->m_Glyphs.Size() - num_whitespaces;
}

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

    SegmentText(font, codepoints, num_codepoints, info);
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

