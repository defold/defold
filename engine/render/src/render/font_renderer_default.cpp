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

#include "font_renderer.h"
#include "font_renderer_private.h"

namespace dmRender
{
    float GetLineTextMetrics(HFontMap font_map, float tracking, const char* text, int n, bool measure_trailing_space)
    {
        float width = 0;
        const char* cursor = text;
        const FontGlyph* last = 0;
        for (int i = 0; i < n; ++i)
        {
            uint32_t c = dmUtf8::NextChar(&cursor);
            const FontGlyph* g = GetGlyph(font_map, c);
            if (!g) {
                continue;
            }

            last = g;
            width += g->m_Advance + tracking;
        }
        if (n > 0 && 0 != last)
        {
            if (font_map->m_IsMonospaced) {
                width += font_map->m_Padding;
            }
            else {
                uint32_t last_width = (measure_trailing_space && last->m_Character == ' ') ? last->m_Advance : last->m_Width;
                float last_end_point = last->m_LeftBearing + last_width;
                float last_right_bearing = last->m_Advance - last_end_point;
                width = width - last_right_bearing;
            }
            width -= tracking;
        }

        return width;
    }

    void GetTextMetrics(HFontMap font_map, const char* text, TextMetricsSettings* settings, TextMetrics* metrics)
    //void GetTextMetrics(HFontMap font_map, const char* text, float width, bool line_break, float leading, float tracking, TextMetrics* metrics)
    {
        metrics->m_MaxAscent = font_map->m_MaxAscent;
        metrics->m_MaxDescent = font_map->m_MaxDescent;

        float width = settings->m_Width;
        if (!settings->m_LineBreak) {
            width = 1000000.0f;
        }

        const uint32_t max_lines = 128;
        dmRender::TextLine lines[max_lines];

        float line_height = font_map->m_MaxAscent + font_map->m_MaxDescent;

        // Trailing space characters should be ignored when measuring and
        // rendering multiline text.
        // For single line text we still want to include spaces when the text
        // layout is calculated (https://github.com/defold/defold/issues/5911)
        bool measure_trailing_space = !settings->m_LineBreak;

        LayoutMetrics lm(font_map, settings->m_Tracking * line_height);
        float layout_width;
        uint32_t num_lines = Layout(text, width, lines, max_lines, &layout_width, lm, measure_trailing_space);
        metrics->m_Width = layout_width;
        metrics->m_Height = num_lines * (line_height * settings->m_Leading) - line_height * (settings->m_Leading - 1.0f);
        metrics->m_LineCount = num_lines;
    }
} // namespace
