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

#ifndef DM_TEXT_LAYOUT_H
#define DM_TEXT_LAYOUT_H

#include <dlib/utf8.h>
#include "text_shape.h"

static inline uint32_t NextBreak(TextShapeGlyph* glyphs, uint32_t num_glyphs, uint32_t* cursor, uint32_t* n)
{
    uint32_t c = 0;
    do {
        c = (*cursor) < num_glyphs ? glyphs[(*cursor)++].m_Codepoint : 0;
        if (c != 0)
            *n = *n + 1;
    } while (c != 0 && !dmUtf8::IsBreaking(c));
    return c;
}

static inline uint32_t SkipWS(TextShapeGlyph* glyphs, uint32_t num_glyphs, uint32_t* cursor, uint32_t* n)
{
    uint32_t c = 0;
    do {
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
uint32_t Layout(TextShapeInfo* info,
                uint32_t width,
                TextLine* lines, uint32_t lines_count,
                uint32_t* text_width,
                Metric metrics,
                bool measure_trailing_space)
{
    TextShapeGlyph* glyphs = info->m_Glyphs.Begin();
    uint32_t        num_glyphs = info->m_Glyphs.Size();

    uint32_t cursor = 0;

    uint32_t max_width = 0;

    uint32_t l = 0;
    uint32_t c;
    do {
        uint32_t n = 0, last_n = 0;
        uint32_t row_start = cursor;
        uint32_t last_cursor = cursor;
        uint32_t w = 0, last_w = 0;
        do {
            c = NextBreak(glyphs, num_glyphs, &cursor, &n);
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
                        c = SkipWS(glyphs, num_glyphs, &cursor, &n);
                }
                else if (last_n != 0)
                {
                    // rewind if we have more to scan
                    cursor = last_cursor;
                    c = glyphs[last_cursor++].m_Codepoint;
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
            tl.m_Index = row_start;
            tl.m_Length = last_n;
            l++;
            max_width = dmMath::Max(max_width, last_w);
        } else {
            // Out of lines
        }
    } while (c);

    *text_width = max_width;
    return l;
}

#endif // DM_TEXT_LAYOUT_H
