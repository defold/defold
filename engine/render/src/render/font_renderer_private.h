#ifndef DM_FONT_RENDERER_PRIVATE
#define DM_FONT_RENDERER_PRIVATE

#include "font_renderer.h"
#include <dlib/utf8.h>
#include <dlib/math.h>

namespace dmRender
{
    static uint32_t NextBreak(const char** cursor, int* n) {
        uint32_t c = dmUtf8::NextChar(cursor);
        while (c != 0) {
            *n = *n + 1;
            if (c == ' ' || c == '\n')
                return c;
            c = dmUtf8::NextChar(cursor);
        }
        return c;
    }

    /*
     * Simple text-layout.
     * Single trailing white-space is not accounted for
     * when breaking but the count is included in the lines array
     * and should be skipped when rendering
     */
    template <typename Metric>
    int Layout(const char* str,
               float width,
               uint16_t* lines, uint16_t lines_count,
               float* text_width,
               Metric metrics)
    {
        const char* cursor = str;

        float max_width = 0;

        int l = 0;
        uint32_t c;
        do {
            int n = 0;
            const char* row_start = cursor;
            int row_count = 0;
            float w, trimmed_w;
            do {
                c = NextBreak(&cursor, &n);
                int trim = 0;
                // Skip single trailing white-space in actual width
                if (c == ' ' || c == '\n')
                    trim = 1;
                w = metrics(row_start, row_count + n);
                trimmed_w = metrics(row_start, row_count + n - trim);
            } while (w <= width && c != 0 && c != '\n');

            if (l < lines_count) {
                lines[l] = n;
                l++;
                max_width = dmMath::Max(max_width, trimmed_w);
            } else {
                // Out of lines
            }
        } while (c);

        *text_width = max_width;
        return l;
    }
}

#endif // #ifndef DM_FONT_RENDERER_PRIVATE
