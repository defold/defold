#ifndef DM_FONT_RENDERER_PRIVATE
#define DM_FONT_RENDERER_PRIVATE

#include "font_renderer.h"
#include <dlib/utf8.h>
#include <dlib/math.h>

namespace dmRender
{
    const uint32_t ZERO_WIDTH_SPACE_UNICODE = 0x200B;

    static bool IsBreaking(uint32_t c)
    {
        return c == ' ' || c == '\n' || c == ZERO_WIDTH_SPACE_UNICODE;
    }

    static uint32_t NextBreak(const char** cursor, int* n) {
        uint32_t c = 0;
        do {
            c = dmUtf8::NextChar(cursor);
            if (c != 0)
                *n = *n + 1;
        } while (c != 0 && !IsBreaking(c));
        return c;
    }

    static uint32_t SkipWS(const char** cursor, int* n) {
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
               Metric metrics)
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
                    w = metrics(row_start, n-trim);
                    if (w <= width)
                    {
                        last_n = n-trim;
                        last_w = w;
                        last_cursor = cursor;
                        if (c != '\n')
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
    static float OffsetX(uint32_t align, float width)
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
    static float OffsetY(uint32_t valign, float height, float ascent, float descent, float leading, uint32_t line_count)
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
}

#endif // #ifndef DM_FONT_RENDERER_PRIVATE
