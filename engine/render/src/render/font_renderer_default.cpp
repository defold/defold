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

#include <math.h>                           // for sqrtf
#include <stdint.h>                         // for uint32_t, int16_t
#include <dlib/align.h>                     // for DM_ALIGNED
#include <dlib/log.h>                       // for dmLog*
#include <dlib/profile.h>                   // for DM_PROFILE, DM_PROPERTY_*
#include <dlib/vmath.h>                     // for Vector4
#include <dlib/time.h>                      // for dmTime::GetMonotonicTime()

#include <graphics/graphics.h>              // for AddVertexStream etc
#include <graphics/graphics_util.h>         // for UnpackRGBA

#include "font.h"                           // for GetGlyph, TextMetri...
#include "font_renderer_private.h"          // for RenderLayerMask
#include "font_renderer_default.h"          // for Layout
#include "render/render_private.h"          // for TextEntry

#include "kb_text_shape.h"

    // kbts_font Font;
    //    size_t ScratchSize = kbts_ReadFontHeader(&Font, Data, Size);

static void* ReadFile(const char* path, uint32_t* file_size)
{
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        printf("Failed to load %s\n", path);
        return 0;
    }

    fseek(file, 0, SEEK_END);
    size_t size = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    void* mem = malloc(size);

    size_t nread = fread(mem, 1, size, file);
    fclose(file);

    if (nread != size)
    {
        printf("Failed to read %u bytes from %s\n", (uint32_t)size, path);
        free(mem);
        return 0;
    }

    if (file_size)
        *file_size = size;

    return mem;
}


namespace dmRender
{
    struct DM_ALIGNED(16) GlyphVertex
    {
        // NOTE: The struct *must* be 16-bytes aligned due to SIMD operations.
        float m_Position[4];
        float m_UV[2];
        float m_FaceColor[4];
        float m_OutlineColor[4];
        float m_ShadowColor[4];
        float m_SdfParams[4];
        float m_LayerMasks[3];
    };

    struct FontRenderBackend
    {
        // intentionally empty, as we currently don't need a context.
        uint32_t _unused;

        void*       m_FontData;
        uint32_t    m_FontSize;
        kbts_font   m_Font;

        void*       m_FontArabicData;
        uint32_t    m_FontArabicSize;
        kbts_font   m_FontArabic;

    };

    static void LoadFont(const char* path, void** data, uint32_t* datasize, kbts_font* font)
    {
        *data = ReadFile(path, datasize);
        assert(*data);

        printf("LOAD FONT: %s\n", path);

        size_t scratch_size = kbts_ReadFontHeader(font, *data, *datasize);

        void* mem = malloc(scratch_size);
        size_t full_size = kbts_ReadFontData(font, mem, scratch_size);

        if(full_size > scratch_size)
        {
            mem = realloc(mem, full_size);
        }
        printf("  size: %u", (uint32_t)scratch_size);

        kbts_PostReadFontInitialize(font, mem, full_size);

        dmLogWarning("  kbts_FontIsValid: %d", kbts_FontIsValid(font));
    }

    HFontRenderBackend CreateFontRenderBackend()
    {
        FontRenderBackend* ctx = new FontRenderBackend;
        memset(ctx, 0, sizeof(*ctx));

        LoadFont("./assets/fonts/noto/Noto_Sans/static/NotoSans-Regular.ttf",
                &ctx->m_FontData, &ctx->m_FontSize, &ctx->m_Font);

        LoadFont("./assets/fonts/noto/Noto_Sans_Arabic/static/NotoSansArabic-Regular.ttf",
                &ctx->m_FontArabicData, &ctx->m_FontArabicSize, &ctx->m_FontArabic);

        return ctx;
    }



    void DestroyFontRenderBackend(HFontRenderBackend ctx)
    {
        // TODO: Memory leak
        // if (kbts_FontIsValid(&ctx->m_Font))
        // {
        //     kbts_FreeFont(&ctx->m_Font);
        // }
        free(ctx->m_FontData);
        free(ctx->m_FontArabicData);
        delete ctx;
    }

    uint32_t GetFontVertexSize(HFontRenderBackend backend)
    {
        (void)backend;
        return sizeof(GlyphVertex);
    }

    dmGraphics::HVertexDeclaration CreateVertexDeclaration(HFontRenderBackend backend, dmGraphics::HContext context)
    {
        (void)backend;

        dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(context);
        dmGraphics::AddVertexStream(stream_declaration, "position", 4, dmGraphics::TYPE_FLOAT, false);
        dmGraphics::AddVertexStream(stream_declaration, "texcoord0", 2, dmGraphics::TYPE_FLOAT, false);
        dmGraphics::AddVertexStream(stream_declaration, "face_color", 4, dmGraphics::TYPE_FLOAT, true);
        dmGraphics::AddVertexStream(stream_declaration, "outline_color", 4, dmGraphics::TYPE_FLOAT, true);
        dmGraphics::AddVertexStream(stream_declaration, "shadow_color", 4, dmGraphics::TYPE_FLOAT, true);
        dmGraphics::AddVertexStream(stream_declaration, "sdf_params", 4, dmGraphics::TYPE_FLOAT, false);
        dmGraphics::AddVertexStream(stream_declaration, "layer_mask", 3, dmGraphics::TYPE_FLOAT, false);

        dmGraphics::HVertexDeclaration decl = dmGraphics::NewVertexDeclaration(context, stream_declaration, GetFontVertexSize(backend));

        dmGraphics::DeleteVertexStreamDeclaration(stream_declaration);

        return decl;
    }

    static float GetLineTextMetrics(HFontMap font_map, float tracking, const char* text, int n, bool measure_trailing_space)
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
                float last_width = (measure_trailing_space && last->m_Character == ' ') ? last->m_Advance : last->m_Width;
                float last_end_point = last->m_LeftBearing + last_width;
                float last_right_bearing = last->m_Advance - last_end_point;
                width = width - last_right_bearing;
            }
            width -= tracking;
        }

        return width;
    }

    struct Pos2i
    {
        int x, y;
    };



    struct TextRun
    {
        TextRun*            m_Next;
        dmArray<kbts_glyph> m_Glyphs;
        dmArray<Pos2i>      m_Positions;

        kbts_script         m_Script;
        kbts_direction      m_MainDirection;
        kbts_direction      m_Direction;

        uint16_t            m_CodepointStart; // index into list of codepoints
        uint16_t            m_CodepointLength; // index into list of codepoints
        uint16_t            m_NumWhiteSpaces;
    };

    static void ShapeText(HFontRenderBackend backend, kbts_cursor* cursor,
                            kbts_direction main_direction, kbts_direction direction, kbts_script script,
                            uint32_t* codepoints, uint32_t num_codepoints,
                            TextRun* run)
    {

        kbts_font* font = &backend->m_Font;

        if (direction == KBTS_DIRECTION_RTL)
        {
            font = &backend->m_FontArabic;
        }

        dmArray<kbts_glyph>& glyphs = run->m_Glyphs;
        dmArray<Pos2i>&      positions = run->m_Positions;

        glyphs.SetCapacity(num_codepoints);
        glyphs.SetSize(num_codepoints);

        for (size_t i = 0; i < num_codepoints; ++i)
        {
            glyphs[i] = kbts_CodepointToGlyph(font, codepoints[i]);
        }

        kbts_shape_state* state = kbts_CreateShapeState(font);
        kbts_shape_config config = kbts_ShapeConfig(font, script, KBTS_LANGUAGE_DONT_KNOW);

        uint32_t glyph_count = num_codepoints;
        uint32_t glyph_capacity = glyph_count;
        while (kbts_Shape(state, &config, main_direction, direction, glyphs.Begin(), &glyph_count, glyph_capacity))
        {
            // We need to loop as it may substitue one codepoint with many
            glyph_capacity = state->RequiredGlyphCapacity;

            if (glyphs.Capacity() < glyph_capacity)
            {
                glyphs.SetCapacity(glyph_capacity);
                glyphs.SetSize(glyph_capacity);
            }
        }

        if (positions.Capacity() < glyphs.Capacity())
        {
            positions.SetCapacity(glyphs.Capacity());
            positions.SetSize(positions.Capacity());
        }

        uint32_t num_whitespace = 0;
        for (size_t i = 0; i < glyph_count; ++i)
        {
            kbts_glyph* glyph = &glyphs[i];

            int x, y;
            kbts_PositionGlyph(cursor, glyph, &x, &y);

            positions[i].x = x;
            positions[i].y = y;

            num_whitespace += dmUtf8::IsWhiteSpace(glyph->Codepoint);
        }

        run->m_NumWhiteSpaces = num_whitespace;
    }

    static void FreeTextRuns(TextRun* run)
    {
        while (run)
        {
            TextRun* next = run->m_Next;
            delete run;
            run = next;
        }
    }

    // https://www.newroadoldway.com/text1.html
    static TextRun* SegmentText(HFontRenderBackend backend, uint32_t* codepoints, uint32_t codepoint_len)
    {
        // Breaks up text into multiple "runs":
        //   "Runs are sequences of characters that share a common direction (left-to-right or right-to-left) as well as a common script"

        //printf("SegmentText:\n");
        TextRun* runs = 0;

        kbts_cursor      Cursor = { 0 };
        kbts_direction   Direction = KBTS_DIRECTION_NONE;
        kbts_script      Script = KBTS_SCRIPT_DONT_KNOW;
        size_t           RunStart = 0;
        kbts_break_state BreakState;
        kbts_BeginBreak(&BreakState, KBTS_DIRECTION_NONE, KBTS_JAPANESE_LINE_BREAK_STYLE_NORMAL);
        for (size_t CodepointIndex = 0; CodepointIndex < codepoint_len; ++CodepointIndex)
        {
            kbts_BreakAddCodepoint(&BreakState, codepoints[CodepointIndex], 1, (CodepointIndex + 1) == codepoint_len);
            kbts_break Break;
            while (kbts_Break(&BreakState, &Break))
            {
                if ((Break.Position > RunStart) && (Break.Flags & (KBTS_BREAK_FLAG_DIRECTION | KBTS_BREAK_FLAG_SCRIPT | KBTS_BREAK_FLAG_LINE_HARD)))
                {
                    size_t RunLength = Break.Position - RunStart;

                    TextRun* run = new TextRun;

                    if (runs == 0)
                        runs = run;
                    else
                    {
                        // Add the run last, for easier debugging
                        TextRun* p = runs;
                        while (p->m_Next)
                        {
                            p = p->m_Next;
                        }
                        p->m_Next = run;
                    }

                    run->m_Next = 0;

                    run->m_Script       = Script;
                    run->m_MainDirection= BreakState.MainDirection;
                    run->m_Direction    = Direction;

                    run->m_CodepointStart   = RunStart;
                    run->m_CodepointLength  = RunLength; // index into list of codepoints

                    ShapeText(backend, &Cursor, BreakState.MainDirection, Direction, Script,
                                codepoints + RunStart, RunLength, run);

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
        return runs;
    }

    // static inline uint32_t NextBreak(uint32_t* pcursor, int* n, kbts_glyph* glyphs, uint32_t num_glyphs)
    // {
    //     //printf("KB NextBreak: '%c' n: %d", *pcursor < num_glyphs ? glyphs[*pcursor].Codepoint : '~', *n);
    //     uint32_t c = 0;
    //     uint32_t i = *pcursor;
    //     for (; i < num_glyphs; ++i)
    //     {
    //         c = glyphs[i].Codepoint;
    //         if (c != 0)
    //             *n = *n + 1;
    //         else
    //             break; // c == 0

    //         if (IsBreaking(c))
    //             break;
    //     }
    //     *pcursor = i+1;
    //     //printf(" -> '%c' n: %d\n", c, *n);

    //     return i < num_glyphs ? c : 0;
    // }

    // static inline uint32_t SkipWS(uint32_t* pcursor, int* n, kbts_glyph* glyphs, uint32_t num_glyphs)
    // {
    //     //printf("KB SkipWS: '%c' n: %d", *pcursor < num_glyphs ? glyphs[*pcursor].Codepoint : '~', *n);
    //     uint32_t c = 0;
    //     uint32_t i = *pcursor;
    //     for (; i < num_glyphs; ++i)
    //     {
    //         c = glyphs[i].Codepoint;
    //         if (c != 0)
    //             *n = *n + 1;

    //         int is_whitespace = c != 0 && (c == ' ' || c == ZERO_WIDTH_SPACE_UNICODE);
    //         if (!is_whitespace)
    //             break;
    //     }
    //     *pcursor = i+1;
    //     //printf(" -> '%c' n: %d\n", c, *n);

    //     return i < num_glyphs ? c : 0;
    // }

    // static uint32_t LayoutGlyphs(HFontMap font_map,
    //                             kbts_glyph* glyphs, uint32_t num_glyphs,
    //                            TextLine* lines, uint32_t max_lines_count,
    //                            float scale,
    //                            float width,
    //                            float tracking,
    //                            bool measure_trailing_space,
    //                            float* out_width)
    // {
    //     float max_width = 0;

    //     uint32_t cursor = 0;

    //     uint32_t l = 0;
    //     uint32_t c;
    //     do {
    //         int n = 0, last_n = 0;
    //         uint32_t row_start = cursor;
    //         uint32_t last_cursor = cursor;
    //         float w = 0.0f, last_w = 0.0f;
    //         do {
    //             c = NextBreak(&cursor, &n, glyphs, num_glyphs);
    //             if (n > 0)
    //             {
    //                 int trim = 0;
    //                 if (c != 0)
    //                     trim = 1;
    //                 w = GetLineTextMetricsGlyphs(font_map, glyphs + row_start, n - trim, scale, tracking, measure_trailing_space);
    //                 if (w <= width)
    //                 {
    //                     last_n = n-trim;
    //                     last_w = w;
    //                     last_cursor = cursor;
    //                     if (c != '\n' && !measure_trailing_space)
    //                         c = SkipWS(&cursor, &n, glyphs, num_glyphs);

    //                 }
    //                 else if (last_n != 0)
    //                 {
    //                     // rewind if we have more to scan
    //                     cursor = last_cursor;
    //                     c = glyphs[last_cursor].Codepoint;
    //                 }
    //             }
    //         } while (w <= width && c != 0 && c != '\n');

    //         if (w > width && last_n == 0)
    //         {
    //             int trim = 0;
    //             if (c != 0)
    //                 trim = 1;
    //             last_n = n-trim;
    //             last_w = w;
    //         }

    //         if (l < max_lines_count && (c != 0 || last_n > 0)) {
    //             TextLine& tl = lines[l];
    //             tl.m_Width = last_w;
    //             tl.m_Index = row_start;
    //             tl.m_Count = last_n;
    //             l++;
    //             max_width = dmMath::Max(max_width, last_w);
    //         } else {
    //             // Out of lines
    //         }
    //     } while (c);

    //     *out_width = max_width;
    //     return l;
    // }

    // // Legacy
    // struct LayoutMetrics
    // {
    //     HFontMap m_FontMap;
    //     float m_Tracking;
    //     LayoutMetrics(HFontMap font_map, float tracking) : m_FontMap(font_map), m_Tracking(tracking) {}
    //     float operator()(const char* text, uint32_t n, bool measure_trailing_space)
    //     {
    //         return GetLineTextMetrics(m_FontMap, m_Tracking, text, n, measure_trailing_space);
    //     }
    // };

    // void GetTextMetrics(HFontRenderBackend backend, HFontMap font_map, const char* text, TextMetricsSettings* settings, TextMetrics* metrics)
    // {
    //     DM_PROFILE(__FUNCTION__);
    //     (void)backend;

    //     metrics->m_MaxAscent = font_map->m_MaxAscent;
    //     metrics->m_MaxDescent = font_map->m_MaxDescent;

    //     float width = settings->m_Width;
    //     if (!settings->m_LineBreak) {
    //         width = 1000000.0f;
    //     }

    //     const uint32_t max_lines = 128;
    //     dmRender::TextLine lines[max_lines];

    //     float line_height = font_map->m_MaxAscent + font_map->m_MaxDescent;

    //     // Trailing space characters should be ignored when measuring and
    //     // rendering multiline text.
    //     // For single line text we still want to include spaces when the text
    //     // layout is calculated (https://github.com/defold/defold/issues/5911)
    //     bool measure_trailing_space = !settings->m_LineBreak;

    //     LayoutMetrics lm(font_map, settings->m_Tracking * line_height);
    //     float layout_width;
    //     uint32_t num_lines = Layout(text, width, lines, max_lines, &layout_width, lm, measure_trailing_space);
    //     metrics->m_Width = layout_width;
    //     metrics->m_Height = num_lines * (line_height * settings->m_Leading) - line_height * (settings->m_Leading - 1.0f);
    //     metrics->m_LineCount = num_lines;
    // }

    // static uint32_t CountValidAndCachedGlyphsText(HFontMap font_map, uint32_t frame, uint32_t max_num_vertices, uint32_t layer_count, const char* text, TextLine* lines, uint32_t line_count)
    // {
    //     const uint32_t vertices_per_quad  = 6;
    //     uint32_t vertexindex = 0;
    //     uint32_t valid_glyph_count = 0;
    //     for (int line = 0; line < line_count; ++line)
    //     {
    //         TextLine& l = lines[line];
    //         const char* cursor = &text[l.m_Index];
    //         bool inner_break = false;

    //         //printf("    valid_glyph_count: %u\n", valid_glyph_count);
    //         //printf("    l: %u: count: %u\n", line, l.m_Count);

    //         for (int j = 0; j < l.m_Count; ++j)
    //         {
    //             uint32_t c = dmUtf8::NextChar(&cursor);
    //             // if (line == 5)
    //             //     printf("'%c' ", c);
    //             dmRender::FontGlyph* g = GetGlyph(font_map, c);
    //             if (!g)
    //             {
    //                 continue;
    //             }

    //             // If we get here, then it may be that c != glyph->m_Character
    //             c = g->m_Character;

    //             if ((vertexindex + vertices_per_quad) * layer_count > max_num_vertices)
    //             {
    //                 inner_break = true;
    //                 break;
    //             }

    //             if (g->m_Width > 0)
    //             {
    //                 int16_t px_cell_offset_y = font_map->m_CacheCellMaxAscent - (int16_t)g->m_Ascent;

    //                 // Prepare the cache here aswell since we only count glyphs we definitely will render.
    //                 if (!IsInCache(font_map, c))
    //                 {
    //                     AddGlyphToCache(font_map, frame, g, px_cell_offset_y);
    //                 }

    //                 CacheGlyph* cache_glyph = GetFromCache(font_map, c);
    //                 if (cache_glyph)
    //                 {
    //                     valid_glyph_count++;

    //                     vertexindex += vertices_per_quad;
    //                 }
    //             }
    //         }
    //         // if (line == 5)
    //         //     printf("\n");

    //         if (inner_break)
    //         {
    //             break;
    //         }
    //     }
    //     return valid_glyph_count;
    // }

    // static uint32_t CountValidAndCachedGlyphsGlyphs(HFontMap font_map, uint32_t frame, uint32_t max_num_vertices, uint32_t layer_count,
    //                                                     kbts_glyph* glyphs, uint32_t num_glyphs, TextLine* lines, uint32_t line_count)
    // {
    //     // TODO: Should be enough to return number of glyphs??
    //     // if (true)
    //     //     return num_glyphs;

    //     const uint32_t vertices_per_quad  = 6;
    //     uint32_t vertexindex = 0;
    //     uint32_t valid_glyph_count = 0;
    //     for (int line = 0; line < line_count; ++line)
    //     {
    //         TextLine& l = lines[line];
    //         bool inner_break = false;

    //         for (int j = 0; j < l.m_Count; ++j)
    //         {
    //             uint32_t c = glyphs[l.m_Index + j].Codepoint;
    //             //uint32_t c = codepoints[l.m_Index + j];

    //             dmRender::FontGlyph* g = GetGlyph(font_map, c);
    //             if (!g)
    //             {
    //                 continue;
    //             }

    //             // If we get here, then it may be that c != glyph->m_Character
    //             c = g->m_Character;

    //             if ((vertexindex + vertices_per_quad) * layer_count > max_num_vertices)
    //             {
    //                 inner_break = true;
    //                 break;
    //             }

    //             if (g->m_Width > 0)
    //             {
    //                 int16_t px_cell_offset_y = font_map->m_CacheCellMaxAscent - (int16_t)g->m_Ascent;

    //                 // Prepare the cache here aswell since we only count glyphs we definitely will render.
    //                 if (!IsInCache(font_map, c))
    //                 {
    //                     AddGlyphToCache(font_map, frame, g, px_cell_offset_y);
    //                 }

    //                 CacheGlyph* cache_glyph = GetFromCache(font_map, c);
    //                 if (cache_glyph)
    //                 {
    //                     valid_glyph_count++;

    //                     vertexindex += vertices_per_quad;
    //                 }
    //             }
    //         }

    //         if (inner_break)
    //         {
    //             break;
    //         }
    //     }
    //     return valid_glyph_count;
    // }


    // static uint32_t CountValidAndCachedGlyphsGlyphs(HFontMap font_map, uint32_t frame, uint32_t max_num_vertices, uint32_t layer_count,
    //                                                     kbts_glyph* glyphs, uint32_t num_glyphs)
    // {
    //     // TODO: Should be enough to return number of glyphs??
    //     // if (true)
    //     //     return num_glyphs;

    //     const uint32_t vertices_per_quad  = 6;
    //     uint32_t vertexindex = 0;
    //     uint32_t valid_glyph_count = 0;

    //     for (int i = 0; i < num_glyphs; ++i)
    //     {
    //         uint32_t c = glyphs[i].Codepoint;

    //         bool inner_break = false;

    //         dmRender::FontGlyph* g = GetGlyph(font_map, c);
    //         if (!g)
    //         {
    //             continue;
    //         }

    //         // If we get here, then it may be that c != glyph->m_Character
    //         c = g->m_Character;

    //         if ((vertexindex + vertices_per_quad) * layer_count > max_num_vertices)
    //         {
    //             break;
    //         }

    //         if (g->m_Width > 0)
    //         {
    //             int16_t px_cell_offset_y = font_map->m_CacheCellMaxAscent - (int16_t)g->m_Ascent;

    //             // Prepare the cache here aswell since we only count glyphs we definitely will render.
    //             if (!IsInCache(font_map, c))
    //             {
    //                 AddGlyphToCache(font_map, frame, g, px_cell_offset_y);
    //             }

    //             CacheGlyph* cache_glyph = GetFromCache(font_map, c);
    //             if (cache_glyph)
    //             {
    //                 valid_glyph_count++;

    //                 vertexindex += vertices_per_quad;
    //             }
    //         }
    //     }
    //     return valid_glyph_count;
    // }

    uint32_t CreateFontVertexData(HFontRenderBackend backend, HFontMap font_map, uint32_t frame, const char* text, const TextEntry& te, float recip_w, float recip_h, uint8_t* _vertices, uint32_t num_vertices)
    {
        DM_PROFILE(__FUNCTION__);
        (void)backend;

        GlyphVertex* vertices = (GlyphVertex*)_vertices;

        float width = te.m_Width;
        if (!te.m_LineBreak) {
            width = 1000000.0f;
        }
        float line_height = font_map->m_MaxAscent + font_map->m_MaxDescent;
        float leading = line_height * te.m_Leading;
        float tracking = line_height * te.m_Tracking;

        const uint32_t max_lines = 128;
        TextLine lines[max_lines];

        // Trailing space characters should be ignored when measuring and
        // rendering multiline text.
        // For single line text we still want to include spaces when the text
        // layout is calculated (https://github.com/defold/defold/issues/5911)
        bool measure_trailing_space = !te.m_LineBreak;

        uint64_t tstart = dmTime::GetMonotonicTime();

        LayoutMetrics lm(font_map, tracking);
        float layout_width;
        int line_count = Layout(text, width, lines, max_lines, &layout_width, lm, measure_trailing_space);

        uint64_t tend = dmTime::GetMonotonicTime();
        printf("Generated %d lines in %.3f ms\n", line_count, (tend-tstart)/1000.0f);


        float x_offset = OffsetX(te.m_Align, te.m_Width);
        if (font_map->m_IsMonospaced)
        {
            x_offset -= font_map->m_Padding * 0.5f;
        }
        float y_offset = OffsetY(te.m_VAlign, te.m_Height, font_map->m_MaxAscent, font_map->m_MaxDescent, te.m_Leading, line_count);

        const Vector4 face_color    = dmGraphics::UnpackRGBA(te.m_FaceColor);
        const Vector4 outline_color = dmGraphics::UnpackRGBA(te.m_OutlineColor);
        const Vector4 shadow_color  = dmGraphics::UnpackRGBA(te.m_ShadowColor);

        // No support for non-uniform scale with SDF so just peek at the first
        // row to extract scale factor. The purpose of this scaling is to have
        // world space distances in the computation, for good 'anti aliasing' no matter
        // what scale is being rendered in.
        const Vector4 r0 = te.m_Transform.getRow(0);
        const float sdf_edge_value = 0.75f;
        float sdf_world_scale = sqrtf(r0.getX() * r0.getX() + r0.getY() * r0.getY());
        float sdf_outline = font_map->m_SdfOutline;
        float sdf_shadow  = font_map->m_SdfShadow;
        // For anti-aliasing, 0.25 represents the single-axis radius of half a pixel.
        float sdf_smoothing = 0.25f / (font_map->m_SdfSpread * sdf_world_scale);

        // TODO: Create a backend scratch buffer
        dmArray<uint32_t> codepoints;

        uint64_t tstart_seg = dmTime::GetMonotonicTime();
        {
            const char* cursor = text;
            uint32_t num_codepoints = dmUtf8::StrLen(text);
            codepoints.SetCapacity(num_codepoints);
            codepoints.SetSize(num_codepoints);
            for (uint32_t i = 0; i < num_codepoints; ++i)
            {
                codepoints[i] = dmUtf8::NextChar(&cursor);
            }

// disable arabic for now
if (codepoints[0] > 300)
{
    return 0;
}
        }
        uint64_t tend_seg_alloc = dmTime::GetMonotonicTime();

        TextRun* runs = SegmentText(backend, codepoints.Begin(), codepoints.Size());

        uint64_t tend_seg = dmTime::GetMonotonicTime();

        printf("KB Segmented text in %.3f ms (alloc: %.3f ms)\n", (tend_seg - tstart_seg)/1000.0f, (tend_seg_alloc-tstart_seg)/1000.0f);


        float pixel_scale = font_map->m_PixelScale;

        uint32_t vertexindex        = 0;
        uint32_t valid_glyph_count  = 0;
        uint8_t  vertices_per_quad  = 6;
        uint8_t  layer_count        = 1;
        uint8_t  layer_mask         = font_map->m_LayerMask;
        float shadow_x              = font_map->m_ShadowX;
        float shadow_y              = font_map->m_ShadowY;

        #define HAS_LAYER(mask,layer) ((mask & layer) == layer)

        if (!HAS_LAYER(layer_mask, FACE))
        {
            dmLogError("Encountered invalid layer mask when rendering font!");
            return 0;
        }

        TextRun* run = runs;

         // Vertex buffer consume strategy:
        // * For single-layered approach, we do as per usual and consume vertices based on offset 0.
        // * For the layered approach, we need to place vertices in sorted order from
        //     back to front layer in the order of shadow -> outline -> face, where the offset of each
        //     layer depends on how many glyphs we actually can place in the buffer. To get a valid count, we
        //     do a dry run first over the input string and place glyphs in the cache if they are renderable.
        layer_count += HAS_LAYER(layer_mask,OUTLINE) + HAS_LAYER(layer_mask,SHADOW);
        if (layer_count > 1)
        {
            // Calculate number of renderable glyphs.
            // Wee need this as we're uploading constants to the GPU, with indices referring to faces
            while (run)
            {
                valid_glyph_count += run->m_Glyphs.Size() - run->m_NumWhiteSpaces; // They should all be valid, shouldn't they?
                run = run->m_Next;
            }
        }

        run = runs;
        while (run)
        {
            uint64_t tstart_kb = dmTime::GetMonotonicTime();
            float out_width = 0;
            int line_count_cp = LayoutGlyphs(font_map,
                                            run->m_Glyphs.Begin(), run->m_Glyphs.Size(),
                                            lines2, max_lines,
                                            pixel_scale,
                                            width,
                                            tracking,
                                            measure_trailing_space,
                                            &out_width);

            uint64_t tend_kb = dmTime::GetMonotonicTime();

            printf("KB Generated %d lines, with max width %.3f in %.3f ms\n", line_count_cp, out_width, (tend_kb-tstart_kb)/1000.0f);

            // for (uint32_t i = 0; i < line_count; ++i)
            // {
            //     printf("LINE: %u index: %u  count: %u  width: %.3f\n", i, lines[i].m_Index, lines[i].m_Count, lines[i].m_Width);
            // }

            // for (uint32_t i = 0; i < line_count_cp; ++i)
            // {
            //     printf("KB LINE: %u index: %u  count: %u  width: %.3f\n", i, lines2[i].m_Index, lines2[i].m_Count, lines2[i].m_Width);
            // }


            static bool use_kb = true;

            static dmhash_t arabic = dmHashString64("/assets/fonts/noto/noto_outline_arabic.fontc");
            static dmhash_t latin = dmHashString64("/assets/fonts/noto/noto_outline.fontc");
            bool is_arabic = font_map->m_NameHash == arabic;

            for (int line = 0; line < line_count; ++line) {
                TextLine& l = lines2[line];

                uint32_t align = te.m_Align;
                if (use_kb)
                {

                    // KBTS_DIRECTION_RTL
                    // if (is_arabic)
                    // {
                    //     // Hack: negate the current setting
                    //     if (align == dmRender::TEXT_ALIGN_LEFT)
                    //         align = dmRender::TEXT_ALIGN_RIGHT;
                    //     else if (align == dmRender::TEXT_ALIGN_RIGHT)
                    //         align = dmRender::TEXT_ALIGN_LEFT;
                    // }
                }

                if (is_arabic)
                {
                    x_offset += 200;
                }

                const float line_start_x = x_offset - OffsetX(align, l.m_Width);
                const float line_start_y = y_offset - line * leading;
                float x = line_start_x;
                float y = line_start_y;

                const char* cursor = &text[l.m_Index];

                // all glyphs are positions on an infinite line, so we want the position of the first glyph on the line
                Pos2i first_pos;
                if (use_kb)
                {
                    first_pos = run->m_Positions[l.m_Index];
                }

                int n = l.m_Count;
                for (int j = 0; j < n; ++j)
                {
                    kbts_glyph* g = 0;
                    Pos2i pos;
                    uint32_t c = 0;
                    if (!use_kb)
                    {
                        c = dmUtf8::NextChar(&cursor);
                    }
                    else
                    {
                        pos = run->m_Positions[l.m_Index + j];
                        g = &run->m_Glyphs[l.m_Index + j];
                        c = g->Codepoint;

                        if (is_arabic)
                            printf("glyph: %X  x: %d\n", g->Codepoint, pos.x);

                        // Since we're dealing with absolute offsets, we should place
                        float offx = (pos.x - first_pos.x) * pixel_scale;
                        float offy = (pos.y - first_pos.y) * pixel_scale;
                        x = line_start_x + offx;
                        y = line_start_y + offy;
                    }

                    FontGlyph* glyph = GetGlyph(font_map, c);
                    if (!glyph) {
                        continue;
                    }

                    // If we get here, then it may be that c != glyph->m_Character
                    if (!use_kb)
                    {
                        c = glyph->m_Character;
                    }

                    // Look ahead and see if we can produce vertices for the next glyph or not
                    if ((vertexindex + vertices_per_quad) * layer_count > num_vertices)
                    {
                        dmLogWarning("Character buffer exceeded (size: %d), increase the \"graphics.max_characters\" property in your game.project file.", num_vertices / 6);
                        return vertexindex * layer_count;
                    }

                    if (c == 0x0644)
                    {
                        printf("DEBUG: %X  width: %f\n", c, glyph->m_Width);
                    }
                    if (glyph->m_Width > 0)
                    {
                        float f_width        = glyph->m_ImageWidth;
                        float f_descent      = glyph->m_Descent;
                        float f_ascent       = glyph->m_Ascent;
                        float f_left_bearing = glyph->m_LeftBearing;
                        // This is the difference in size of the glyph image and glyph size
                        float f_size_diff    = glyph->m_ImageWidth - glyph->m_Width;

                        int16_t width        = (int16_t) f_width;
                        int16_t descent      = (int16_t) f_descent;
                        int16_t ascent       = (int16_t) f_ascent;

                        // Calculate y-offset in cache-cell space by moving glyphs down to baseline
                        int16_t px_cell_offset_y = font_map->m_CacheCellMaxAscent - ascent;

                        if (!IsInCache(font_map, c))
                        {
                            AddGlyphToCache(font_map, frame, glyph, px_cell_offset_y);
                        }

                        CacheGlyph* cache_glyph = GetFromCache(font_map, c);
                        if (cache_glyph)
                        {
                            uint32_t face_index = vertexindex + vertices_per_quad * valid_glyph_count * (layer_count-1);
                            // TODO: Add api to get the uv coords for the glyph
                            uint32_t tx = cache_glyph->m_X;
                            uint32_t ty = cache_glyph->m_Y;

                            // Set face vertices first, this will always hold since we can't have less than 1 layer
                            GlyphVertex& v1_layer_face = vertices[face_index];
                            GlyphVertex& v2_layer_face = vertices[face_index + 1];
                            GlyphVertex& v3_layer_face = vertices[face_index + 2];
                            GlyphVertex& v4_layer_face = vertices[face_index + 3];
                            GlyphVertex& v5_layer_face = vertices[face_index + 4];
                            GlyphVertex& v6_layer_face = vertices[face_index + 5];

                            float xx = x - f_size_diff * 0.5f;

                            (Vector4&) v1_layer_face.m_Position = te.m_Transform * Vector4(xx + f_left_bearing, y - f_descent, 0, 1);
                            (Vector4&) v2_layer_face.m_Position = te.m_Transform * Vector4(xx + f_left_bearing, y + f_ascent, 0, 1);
                            (Vector4&) v3_layer_face.m_Position = te.m_Transform * Vector4(xx + f_left_bearing + f_width, y - f_descent, 0, 1);
                            (Vector4&) v6_layer_face.m_Position = te.m_Transform * Vector4(xx + f_left_bearing + f_width, y + f_ascent, 0, 1);

                            v1_layer_face.m_UV[0] = (tx + font_map->m_CacheCellPadding) * recip_w;
                            v1_layer_face.m_UV[1] = (ty + font_map->m_CacheCellPadding + ascent + descent + px_cell_offset_y) * recip_h;

                            v2_layer_face.m_UV[0] = (tx + font_map->m_CacheCellPadding) * recip_w;
                            v2_layer_face.m_UV[1] = (ty + font_map->m_CacheCellPadding + px_cell_offset_y) * recip_h;

                            v3_layer_face.m_UV[0] = (tx + font_map->m_CacheCellPadding + width) * recip_w;
                            v3_layer_face.m_UV[1] = (ty + font_map->m_CacheCellPadding + ascent + descent + px_cell_offset_y) * recip_h;

                            v6_layer_face.m_UV[0] = (tx + font_map->m_CacheCellPadding + width) * recip_w;
                            v6_layer_face.m_UV[1] = (ty + font_map->m_CacheCellPadding + px_cell_offset_y) * recip_h;

                            #define SET_VERTEX_FONT_PROPERTIES(v) \
                                v.m_FaceColor[0]    = face_color[0]; \
                                v.m_FaceColor[1]    = face_color[1]; \
                                v.m_FaceColor[2]    = face_color[2]; \
                                v.m_FaceColor[3]    = face_color[3]; \
                                v.m_OutlineColor[0] = outline_color[0]; \
                                v.m_OutlineColor[1] = outline_color[1]; \
                                v.m_OutlineColor[2] = outline_color[2]; \
                                v.m_OutlineColor[3] = outline_color[3]; \
                                v.m_ShadowColor[0]  = shadow_color[0]; \
                                v.m_ShadowColor[1]  = shadow_color[1]; \
                                v.m_ShadowColor[2]  = shadow_color[2]; \
                                v.m_ShadowColor[3]  = shadow_color[3]; \
                                v.m_FaceColor[0]    = face_color[0]; \
                                v.m_FaceColor[1]    = face_color[1]; \
                                v.m_FaceColor[2]    = face_color[2]; \
                                v.m_FaceColor[3]    = face_color[3]; \
                                v.m_SdfParams[0]    = sdf_edge_value; \
                                v.m_SdfParams[1]    = sdf_outline; \
                                v.m_SdfParams[2]    = sdf_smoothing; \
                                v.m_SdfParams[3]    = sdf_shadow;

                            SET_VERTEX_FONT_PROPERTIES(v1_layer_face)
                            SET_VERTEX_FONT_PROPERTIES(v2_layer_face)
                            SET_VERTEX_FONT_PROPERTIES(v3_layer_face)
                            SET_VERTEX_FONT_PROPERTIES(v6_layer_face)

                            #undef SET_VERTEX_FONT_PROPERTIES

                            v4_layer_face = v3_layer_face;
                            v5_layer_face = v2_layer_face;

                            #define SET_VERTEX_LAYER_MASK(v,f,o,s) \
                                v.m_LayerMasks[0] = f; \
                                v.m_LayerMasks[1] = o; \
                                v.m_LayerMasks[2] = s;

                            // Set outline vertices
                            if (HAS_LAYER(layer_mask,OUTLINE))
                            {
                                uint32_t outline_index = vertexindex + vertices_per_quad * valid_glyph_count * (layer_count-2);

                                GlyphVertex& v1_layer_outline = vertices[outline_index];
                                GlyphVertex& v2_layer_outline = vertices[outline_index + 1];
                                GlyphVertex& v3_layer_outline = vertices[outline_index + 2];
                                GlyphVertex& v4_layer_outline = vertices[outline_index + 3];
                                GlyphVertex& v5_layer_outline = vertices[outline_index + 4];
                                GlyphVertex& v6_layer_outline = vertices[outline_index + 5];

                                v1_layer_outline = v1_layer_face;
                                v2_layer_outline = v2_layer_face;
                                v3_layer_outline = v3_layer_face;
                                v4_layer_outline = v4_layer_face;
                                v5_layer_outline = v5_layer_face;
                                v6_layer_outline = v6_layer_face;

                                SET_VERTEX_LAYER_MASK(v1_layer_outline,0,1,0)
                                SET_VERTEX_LAYER_MASK(v2_layer_outline,0,1,0)
                                SET_VERTEX_LAYER_MASK(v3_layer_outline,0,1,0)
                                SET_VERTEX_LAYER_MASK(v4_layer_outline,0,1,0)
                                SET_VERTEX_LAYER_MASK(v5_layer_outline,0,1,0)
                                SET_VERTEX_LAYER_MASK(v6_layer_outline,0,1,0)
                            }

                            // Set shadow vertices
                            if (HAS_LAYER(layer_mask,SHADOW))
                            {
                                uint32_t shadow_index = vertexindex;

                                GlyphVertex& v1_layer_shadow = vertices[shadow_index];
                                GlyphVertex& v2_layer_shadow = vertices[shadow_index + 1];
                                GlyphVertex& v3_layer_shadow = vertices[shadow_index + 2];
                                GlyphVertex& v4_layer_shadow = vertices[shadow_index + 3];
                                GlyphVertex& v5_layer_shadow = vertices[shadow_index + 4];
                                GlyphVertex& v6_layer_shadow = vertices[shadow_index + 5];

                                v1_layer_shadow = v1_layer_face;
                                v2_layer_shadow = v2_layer_face;
                                v3_layer_shadow = v3_layer_face;
                                v6_layer_shadow = v6_layer_face;

                                // Shadow offsets must be calculated since we need to offset in local space (before vertex transformation)
                                (Vector4&) v1_layer_shadow.m_Position = te.m_Transform * Vector4(xx + f_left_bearing + shadow_x, y - f_descent + shadow_y, 0, 1);
                                (Vector4&) v2_layer_shadow.m_Position = te.m_Transform * Vector4(xx + f_left_bearing + shadow_x, y + f_ascent + shadow_y, 0, 1);
                                (Vector4&) v3_layer_shadow.m_Position = te.m_Transform * Vector4(xx + f_left_bearing + shadow_x + f_width, y - f_descent + shadow_y, 0, 1);
                                (Vector4&) v6_layer_shadow.m_Position = te.m_Transform * Vector4(xx + f_left_bearing + shadow_x + f_width, y + f_ascent + shadow_y, 0, 1);

                                v4_layer_shadow = v3_layer_shadow;
                                v5_layer_shadow = v2_layer_shadow;

                                SET_VERTEX_LAYER_MASK(v1_layer_shadow,0,0,1)
                                SET_VERTEX_LAYER_MASK(v2_layer_shadow,0,0,1)
                                SET_VERTEX_LAYER_MASK(v3_layer_shadow,0,0,1)
                                SET_VERTEX_LAYER_MASK(v4_layer_shadow,0,0,1)
                                SET_VERTEX_LAYER_MASK(v5_layer_shadow,0,0,1)
                                SET_VERTEX_LAYER_MASK(v6_layer_shadow,0,0,1)
                            }

                            // If we only have one layer, we need to set the mask to (1,1,1)
                            // so that we can use the same calculations for both single and multi.
                            // The mask is set last for layer 1 since we copy the vertices to
                            // all other layers to avoid re-calculating their data.
                            uint8_t is_one_layer = layer_count > 1 ? 0 : 1;
                            SET_VERTEX_LAYER_MASK(v1_layer_face,1,is_one_layer,is_one_layer)
                            SET_VERTEX_LAYER_MASK(v2_layer_face,1,is_one_layer,is_one_layer)
                            SET_VERTEX_LAYER_MASK(v3_layer_face,1,is_one_layer,is_one_layer)
                            SET_VERTEX_LAYER_MASK(v4_layer_face,1,is_one_layer,is_one_layer)
                            SET_VERTEX_LAYER_MASK(v5_layer_face,1,is_one_layer,is_one_layer)
                            SET_VERTEX_LAYER_MASK(v6_layer_face,1,is_one_layer,is_one_layer)

                            #undef SET_VERTEX_LAYER_MASK

                            vertexindex += vertices_per_quad;
                        }
                    }

                    if (!use_kb)
                    {
                        x += glyph->m_Advance + tracking;
                    }
                }
            }

            run = run->m_Next;
            break; // TODO: Do layout with respect to different runs!

        } // while(runs)

        #undef HAS_LAYER

        return vertexindex * layer_count;
    }

} // namespace
