#ifndef DM_FONT_RENDERER_PRIVATE
#define DM_FONT_RENDERER_PRIVATE

#include "font_renderer.h"
#include <dlib/utf8.h>
#include <dlib/math.h>

namespace dmRender
{
    static bool IsBreaking(uint32_t c)
    {
        return c == ' ' || c == '\n';
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
        } while (c != 0 && c == ' ');

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
                    if (fabsf(w) <= width)
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
            } while (fabsf(w) <= width && c != 0 && c != '\n');
            if (fabsf(w) > width && last_n == 0)
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
                max_width = dmMath::Max(max_width, fabsf(last_w));
            } else {
                // Out of lines
            }
        } while (c);

        *text_width = max_width;
        return l;
    }
}

#endif // #ifndef DM_FONT_RENDERER_PRIVATE
