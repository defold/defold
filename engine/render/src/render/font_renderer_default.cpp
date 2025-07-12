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


    struct Pos2i
    {
        int x, y;
    };

    struct TextRun
    {
        uint16_t            m_GlyphStart; // index into list of glyphs
        uint16_t            m_GlyphLength; // index into list of glyphs

        kbts_script         m_Script;
        kbts_direction      m_MainDirection;
        kbts_direction      m_Direction;

        uint16_t            m_NumWhiteSpaces;
    };

    struct FontRenderBackend
    {
        // TEMP DATA
        void*       m_FontData;
        uint32_t    m_FontSize;
        kbts_font   m_Font;

        void*       m_FontArabicData;
        uint32_t    m_FontArabicSize;
        kbts_font   m_FontArabic;
        // END TEMP DATA


        dmArray<kbts_glyph> m_Glyphs;
        dmArray<Pos2i>      m_Positions;
        dmArray<TextRun>    m_Runs;
        uint32_t            m_NumValidGlyphs;
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

    static void ShapeText(HFontRenderBackend backend, kbts_cursor* cursor,
                            kbts_direction main_direction, kbts_direction direction, kbts_script script,
                            uint32_t* codepoints, uint32_t num_codepoints,
                            TextRun* run)
    {
        // SHapes a single run

        // printf("  SHAPETEXT: \"");
        // for (uint32_t i = 0; i < num_codepoints; ++i)
        // {
        //     printf("%c", (char)codepoints[i]);
        // }
        // printf("\" len: %u \n", num_codepoints);

        kbts_font* font = &backend->m_Font;

        if (direction == KBTS_DIRECTION_RTL)
        {
            font = &backend->m_FontArabic;
        }

        dmArray<kbts_glyph>& glyphs = backend->m_Glyphs;
        dmArray<Pos2i>&      positions = backend->m_Positions;

        // We don't want to overwrite the previous runs
        uint32_t remaining = glyphs.Remaining();
        if (remaining < num_codepoints)
        {
            glyphs.OffsetCapacity(num_codepoints - remaining);
        }

        uint32_t prev_glyph_index = glyphs.Size();
        for (size_t i = 0; i < num_codepoints; ++i)
        {
            glyphs.Push(kbts_CodepointToGlyph(font, codepoints[i]));
        }

        kbts_shape_state* state = kbts_CreateShapeState(font);
        kbts_shape_config config = kbts_ShapeConfig(font, script, KBTS_LANGUAGE_DONT_KNOW);

        uint32_t glyph_count = num_codepoints;
        uint32_t glyph_capacity = glyph_count;
        while (kbts_Shape(state, &config, main_direction, direction, glyphs.Begin(), &glyph_count, glyph_capacity))
        {
            // We need to loop as it may substitue one codepoint with many
            glyph_capacity = state->RequiredGlyphCapacity;

            remaining = glyphs.Remaining();
            if (remaining < glyph_capacity)
            {
                glyphs.OffsetCapacity(glyph_capacity - remaining);
                assert(glyphs.Capacity() >= (glyph_capacity + prev_glyph_index));
            }
        }
        glyphs.SetSize(prev_glyph_index + glyph_capacity);

        if (positions.Capacity() < glyphs.Capacity())
        {
            positions.SetCapacity(glyphs.Capacity());
        }
        positions.SetSize(glyphs.Size());


        uint32_t num_whitespace = 0;
        for (size_t i = 0; i < glyph_count; ++i)
        {
            uint32_t ii = prev_glyph_index + i;
            kbts_glyph* glyph = &glyphs[ii];

            int x, y;
            kbts_PositionGlyph(cursor, glyph, &x, &y);

            positions[ii].x = x;
            positions[ii].y = y;

            num_whitespace += dmUtf8::IsWhiteSpace(glyph->Codepoint);
        }

        run->m_NumWhiteSpaces = num_whitespace;
    }

    // https://www.newroadoldway.com/text1.html
    static void SegmentText(HFontRenderBackend backend, uint32_t* codepoints, uint32_t codepoint_len)
    {
        // Breaks up text into multiple "runs":
        //   "Runs are sequences of characters that share a common direction (left-to-right or right-to-left) as well as a common script"

        backend->m_Glyphs.SetSize(0);
        backend->m_Positions.SetSize(0);
        backend->m_Runs.SetSize(0);

        kbts_cursor      Cursor = { 0 };
        kbts_direction   Direction = KBTS_DIRECTION_NONE;
        kbts_script      Script = KBTS_SCRIPT_DONT_KNOW;
        size_t           RunStart = 0;
        kbts_break_state BreakState;
        kbts_BeginBreak(&BreakState, KBTS_DIRECTION_NONE, KBTS_JAPANESE_LINE_BREAK_STYLE_NORMAL);

        uint32_t num_whitespaces = 0;
        for (size_t CodepointIndex = 0; CodepointIndex < codepoint_len; ++CodepointIndex)
        {
            kbts_BreakAddCodepoint(&BreakState, codepoints[CodepointIndex], 1, (CodepointIndex + 1) == codepoint_len);
            kbts_break Break;
            while (kbts_Break(&BreakState, &Break))
            {
                if ((Break.Position > RunStart) && (Break.Flags & (KBTS_BREAK_FLAG_DIRECTION | KBTS_BREAK_FLAG_SCRIPT | KBTS_BREAK_FLAG_LINE_HARD)))
                {
                    size_t RunLength = Break.Position - RunStart;

                    TextRun run = {0};
                    run.m_Script       = Script;
                    run.m_MainDirection= BreakState.MainDirection;
                    run.m_Direction    = Direction;
                    run.m_GlyphStart   = backend->m_Glyphs.Size();
                    run.m_GlyphLength  = RunLength;

                    ShapeText(backend, &Cursor, BreakState.MainDirection, Direction, Script,
                                codepoints + RunStart, RunLength, &run);

                    if (backend->m_Runs.Full())
                    {
                        backend->m_Runs.OffsetCapacity(16);
                    }
                    backend->m_Runs.Push(run);
                    num_whitespaces += run.m_NumWhiteSpaces;

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

        backend->m_NumValidGlyphs = backend->m_Glyphs.Size() - num_whitespaces;
    }

    static uint32_t LayoutLines(HFontRenderBackend backend,
                                    int32_t line_width_points, int32_t tracking, int32_t fake_glyph_width,
                                    int32_t* max_line_width,
                                    TextLine* lines, uint32_t max_lines_count)
    {
        kbts_glyph* glyphs = backend->m_Glyphs.Begin();
        uint32_t num_glyphs = backend->m_Glyphs.Size();

        Pos2i* positions = backend->m_Positions.Begin();

        *max_line_width = 0;
        uint32_t line_count = 0;

        uint32_t line_start_index = 0; // The index of the previous breaking whitespace
        uint32_t last_ws_index = 0;    // The index of the last breaking whitespace
        int32_t line_start_position_x = 0; // the absolute (positive) position.x of the glyph on the infinite line

        for (uint32_t i = 0; i < num_glyphs; ++i)
        {
            kbts_glyph* glyph = &glyphs[i];
            Pos2i       pos = positions[i];

            int32_t pos_x = dmMath::Abs(pos.x);
            int32_t line_width = pos_x - line_start_position_x;

            bool line_break = false;
            if (!IsBreaking(glyph->Codepoint))
            {
                line_break = (line_width + fake_glyph_width) > line_width_points;
            }
            else
            {
                last_ws_index = i;
            }

            // we make sure that we've at least progressed one word before doing a line break
            if (line_break && (last_ws_index > line_start_index))
            {
                int32_t length = last_ws_index - line_start_index;

                // The X coordinate where we found the breaking white space
                int32_t break_position_x = dmMath::Abs(positions[last_ws_index].x);

                line_width = break_position_x - line_start_position_x + tracking * (length-1);
                *max_line_width = dmMath::Max(*max_line_width, line_width);

                if (lines)
                {
                    lines[line_count].m_Width = line_width;
                    lines[line_count].m_Index = line_start_index;
                    lines[line_count].m_Count = length;
                }
                ++line_count;

                // forward to the next non-breaking character
                while ((++last_ws_index) < num_glyphs)
                {
                    if (!IsBreaking(glyphs[last_ws_index].Codepoint))
                    {
                        // calculate where to continue the for-loop
                        i = last_ws_index - 1;
                        break;
                    }
                }

                line_start_index = last_ws_index;
                line_start_position_x = positions[last_ws_index].x;
            }
        }

        // Final line (e.g. when no line break)
        if (line_start_index < num_glyphs)
        {
            // TODO: account for last trailing space
            //kbts_glyph* glyph = &glyphs[i];

            int32_t length = num_glyphs - line_start_index;

            int32_t break_position_x = dmMath::Abs(positions[num_glyphs-1].x);
            int32_t line_width = break_position_x - line_start_position_x + fake_glyph_width + tracking * (length-1);
            *max_line_width = dmMath::Max(*max_line_width, line_width);

            if (lines)
            {
                lines[line_count].m_Width = line_width;
                lines[line_count].m_Index = line_start_index;
                lines[line_count].m_Count = length;
            }
            ++line_count;
        }

        return line_count;
    }

    void GetTextMetrics(HFontRenderBackend backend, HFontMap font_map, const char* text, TextMetricsSettings* settings, TextMetrics* metrics)
    {
        DM_PROFILE(__FUNCTION__);

        float pixel_scale = font_map->m_PixelScale;
        int32_t width_points = INT_MAX;
        if (settings->m_LineBreak) {
            width_points = settings->m_Width / pixel_scale;
        }

        dmArray<uint32_t> codepoints;
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
    memset(metrics, 0, sizeof(*metrics));
    return;
}
        }

        SegmentText(backend, codepoints.Begin(), codepoints.Size());

        int32_t fake_glyph_width = 28 / pixel_scale;
        int32_t max_line_width;
        uint32_t num_lines = LayoutLines(backend, width_points, settings->m_Tracking, fake_glyph_width, &max_line_width, 0, 0);

        float line_height = font_map->m_MaxAscent + font_map->m_MaxDescent;

        metrics->m_MaxAscent = font_map->m_MaxAscent;
        metrics->m_MaxDescent = font_map->m_MaxDescent;
        metrics->m_Width = max_line_width * pixel_scale;
        metrics->m_Height = num_lines * (line_height * settings->m_Leading) - line_height * (settings->m_Leading - 1.0f);
        metrics->m_LineCount = num_lines;

    }

    #define HAS_LAYER(mask,layer) ((mask & layer) == layer)

    static void OutputGlyph(FontGlyph* glyph,
                            float recip_w, float recip_h,
                            uint32_t cell_x, uint32_t cell_y, uint32_t cache_cell_max_ascent, uint32_t cache_cell_padding,
                            uint32_t layer_count, uint32_t layer_mask,
                            uint32_t vertexindex, uint32_t vertex_layer_stride,
                            const dmVMath::Matrix4& transform,
                            float x, float y,
                            const Vector4& face_color,
                            const Vector4& outline_color,
                            const Vector4& shadow_color,
                            float sdf_edge_value,
                            float sdf_outline,
                            float sdf_smoothing,
                            float sdf_shadow,
                            float shadow_x,
                            float shadow_y,
                            GlyphVertex* vertices)
    {
        float f_width;
        float f_descent;
        float f_ascent;
        float f_left_bearing;
        float f_size_diff;
        if (glyph)
        {
            assert(glyph->m_ImageWidth != 0);
            f_width        = glyph->m_ImageWidth;
            f_descent      = glyph->m_Descent;
            f_ascent       = glyph->m_Ascent;
            f_left_bearing = glyph->m_LeftBearing;
            // This is the difference in size of the glyph image and glyph size
            f_size_diff    = glyph->m_ImageWidth - glyph->m_Width;
        }
        else
        {
            // we're outputting a missing glyph
            f_width        = 0;
            f_descent      = 0;
            f_ascent       = 0;
            f_left_bearing = 0;
            f_size_diff    = 0;
        }
        int16_t width   = (int16_t) f_width;
        int16_t descent = (int16_t) f_descent;
        int16_t ascent  = (int16_t) f_ascent;

        // Calculate y-offset in cache-cell space by moving glyphs down to baseline
        int16_t px_cell_offset_y = cache_cell_max_ascent - ascent;

        uint32_t face_index = vertexindex + vertex_layer_stride * (layer_count-1);
        uint32_t tx = cell_x;
        uint32_t ty = cell_y;

        // Set face vertices first, this will always hold since we can't have less than 1 layer
        GlyphVertex& v1_layer_face = vertices[face_index];
        GlyphVertex& v2_layer_face = vertices[face_index + 1];
        GlyphVertex& v3_layer_face = vertices[face_index + 2];
        GlyphVertex& v4_layer_face = vertices[face_index + 3];
        GlyphVertex& v5_layer_face = vertices[face_index + 4];
        GlyphVertex& v6_layer_face = vertices[face_index + 5];

        float xx = x - f_size_diff * 0.5f;

        (Vector4&) v1_layer_face.m_Position = transform * Vector4(xx + f_left_bearing, y - f_descent, 0, 1);
        (Vector4&) v2_layer_face.m_Position = transform * Vector4(xx + f_left_bearing, y + f_ascent, 0, 1);
        (Vector4&) v3_layer_face.m_Position = transform * Vector4(xx + f_left_bearing + f_width, y - f_descent, 0, 1);
        (Vector4&) v6_layer_face.m_Position = transform * Vector4(xx + f_left_bearing + f_width, y + f_ascent, 0, 1);

        v1_layer_face.m_UV[0] = (tx + cache_cell_padding) * recip_w;
        v1_layer_face.m_UV[1] = (ty + cache_cell_padding + ascent + descent + px_cell_offset_y) * recip_h;

        v2_layer_face.m_UV[0] = (tx + cache_cell_padding) * recip_w;
        v2_layer_face.m_UV[1] = (ty + cache_cell_padding + px_cell_offset_y) * recip_h;

        v3_layer_face.m_UV[0] = (tx + cache_cell_padding + width) * recip_w;
        v3_layer_face.m_UV[1] = (ty + cache_cell_padding + ascent + descent + px_cell_offset_y) * recip_h;

        v6_layer_face.m_UV[0] = (tx + cache_cell_padding + width) * recip_w;
        v6_layer_face.m_UV[1] = (ty + cache_cell_padding + px_cell_offset_y) * recip_h;

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
            uint32_t outline_index = vertexindex + vertex_layer_stride * (layer_count-2);

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
            (Vector4&) v1_layer_shadow.m_Position = transform * Vector4(xx + f_left_bearing + shadow_x, y - f_descent + shadow_y, 0, 1);
            (Vector4&) v2_layer_shadow.m_Position = transform * Vector4(xx + f_left_bearing + shadow_x, y + f_ascent + shadow_y, 0, 1);
            (Vector4&) v3_layer_shadow.m_Position = transform * Vector4(xx + f_left_bearing + shadow_x + f_width, y - f_descent + shadow_y, 0, 1);
            (Vector4&) v6_layer_shadow.m_Position = transform * Vector4(xx + f_left_bearing + shadow_x + f_width, y + f_ascent + shadow_y, 0, 1);

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
    }

    uint32_t CreateFontVertexData(HFontRenderBackend backend, HFontMap font_map, uint32_t frame, const char* text, const TextEntry& te, float recip_w, float recip_h, uint8_t* _vertices, uint32_t num_vertices)
    {
        DM_PROFILE(__FUNCTION__);
        (void)backend;

        GlyphVertex* vertices = (GlyphVertex*)_vertices;

        float line_height = font_map->m_MaxAscent + font_map->m_MaxDescent;
        float leading = line_height * te.m_Leading;
        float tracking = te.m_Tracking;

        const uint32_t max_lines = 128;
        TextLine lines[max_lines];

        // Trailing space characters should be ignored when measuring and
        // rendering multiline text.
        // For single line text we still want to include spaces when the text
        // layout is calculated (https://github.com/defold/defold/issues/5911)
        // Basically it makes typing space into an input field a lot better looking.
        bool measure_trailing_space = !te.m_LineBreak;

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

        SegmentText(backend, codepoints.Begin(), codepoints.Size());
        if (backend->m_Runs.Size() == 0)
        {
            return 0; // no added vertices
        }

        float pixel_scale = font_map->m_PixelScale;

        // int dir = runs->m_MainDirection == KBTS_DIRECTION_RTL ? -1 : 1;
        // int width_points = dir > 0 ? INT_MAX : INT_MIN;
        int width_points = INT_MAX;

        //float width = te.m_Width;
        if (te.m_LineBreak) {
            //width = 1000000.0f;
            //width_points = (dir*te.m_Width) / pixel_scale;
            width_points = te.m_Width / pixel_scale;
        }

        uint64_t tend_seg = dmTime::GetMonotonicTime();

        printf("KB Segmented text in %.3f ms (alloc: %.3f ms)\n", (tend_seg - tstart_seg)/1000.0f, (tend_seg_alloc-tstart_seg)/1000.0f);
        printf("  #glyphs: %u  #runs: %u\n", backend->m_Glyphs.Size(), backend->m_Runs.Size());

        uint32_t vertexindex        = 0;
        uint32_t valid_glyph_count  = 0;
        uint8_t  vertices_per_quad  = 6;
        uint8_t  layer_count        = 1;
        uint8_t  layer_mask         = font_map->m_LayerMask;
        float shadow_x              = font_map->m_ShadowX;
        float shadow_y              = font_map->m_ShadowY;

        if (!HAS_LAYER(layer_mask, FACE))
        {
            dmLogError("Encountered invalid layer mask when rendering font!");
            return 0;
        }

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
            valid_glyph_count = backend->m_NumValidGlyphs;

            // // uint32_t test_count = CountValidAndCachedGlyphsGlyphs(font_map, frame, num_vertices, layer_count,
            // //                                                     backend->m_Glyphs.Begin(), backend->m_Glyphs.Size());

            // valid_glyph_count += backend->m_Glyphs.Size();
            // for (uint32_t i = 0; i < backend->m_Runs.Size(); ++i)
            // {
            //     TextRun* run = &backend->m_Runs[i];
            //     valid_glyph_count -= run->m_NumWhiteSpaces; // They should all be valid, shouldn't they?
            // }
        }

        uint64_t tstart_layout = dmTime::GetMonotonicTime();

        int32_t fake_glyph_width = 28 / pixel_scale;
        int32_t max_line_width;
        uint32_t line_count = LayoutLines(backend, width_points,
                                            tracking / pixel_scale,
                                            fake_glyph_width,
                                            &max_line_width, lines, max_lines);

        uint64_t tend_layout = dmTime::GetMonotonicTime();
        printf("LineCount: %u  line_width: %d glyph_width: %d  %.3f ms\n", line_count, width_points, fake_glyph_width, (tend_layout-tstart_layout)/1000.0f);

        float x_offset = OffsetX(te.m_Align, te.m_Width);
        if (font_map->m_IsMonospaced)
        {
            x_offset -= font_map->m_Padding * 0.5f;
        }
        float y_offset = OffsetY(te.m_VAlign, te.m_Height, font_map->m_MaxAscent, font_map->m_MaxDescent, te.m_Leading, line_count);

        // uint64_t tstart_kb = dmTime::GetMonotonicTime();
        // float out_width = 0;
        // int line_count_cp = LayoutGlyphs(font_map,
        //                                 run->m_Glyphs.Begin(), run->m_Glyphs.Size(),
        //                                 lines2, max_lines,
        //                                 pixel_scale,
        //                                 width,
        //                                 tracking,
        //                                 measure_trailing_space,
        //                                 &out_width);

        // uint64_t tend_kb = dmTime::GetMonotonicTime();

        // printf("KB Generated %d lines, with max width %.3f in %.3f ms\n", line_count_cp, out_width, (tend_kb-tstart_kb)/1000.0f);

        // for (uint32_t i = 0; i < line_count; ++i)
        // {
        //     printf("LINE: %u index: %u  count: %u  width: %.3f\n", i, lines[i].m_Index, lines[i].m_Count, lines[i].m_Width);
        // }

        // for (uint32_t i = 0; i < line_count_cp; ++i)
        // {
        //     printf("KB LINE: %u index: %u  count: %u  width: %.3f\n", i, lines2[i].m_Index, lines2[i].m_Count, lines2[i].m_Width);
        // }

        static dmhash_t arabic = dmHashString64("/assets/fonts/noto/noto_outline_arabic.fontc");
        static dmhash_t latin = dmHashString64("/assets/fonts/noto/noto_outline.fontc");
        bool is_arabic = font_map->m_NameHash == arabic;

        kbts_glyph* glyphs = backend->m_Glyphs.Begin();
        Pos2i* positions = backend->m_Positions.Begin();

        uint32_t count = 0;
        for (int line = 0; line < line_count; ++line) {
            TextLine& l = lines[line];

            uint32_t align = te.m_Align;

            // KBTS_DIRECTION_RTL
            // if (is_arabic)
            // {
            //     // Hack: negate the current setting
            //     if (align == dmRender::TEXT_ALIGN_LEFT)
            //         align = dmRender::TEXT_ALIGN_RIGHT;
            //     else if (align == dmRender::TEXT_ALIGN_RIGHT)
            //         align = dmRender::TEXT_ALIGN_LEFT;
            // }

            if (is_arabic)
            {
                x_offset += 200;
            }

            const float line_start_x = x_offset - OffsetX(align, l.m_Width);
            const float line_start_y = y_offset - line * leading;

            // all glyphs are positions on an infinite line, so we want the position of the first glyph on the line
            Pos2i first_pos = positions[l.m_Index];

            int n = l.m_Count;
            for (int j = 0; j < n; ++j)
            {
                // Look ahead and see if we can produce vertices for the next glyph or not
                if ((vertexindex + vertices_per_quad) * layer_count > num_vertices)
                {
                    dmLogWarning("Character buffer exceeded (size: %d), increase the \"graphics.max_characters\" property in your game.project file.", num_vertices / 6);
                    return vertexindex * layer_count;
                }

                kbts_glyph* g = &glyphs[l.m_Index + j];
                uint32_t c = g->Codepoint;
                if (dmUtf8::IsWhiteSpace(c))
                    continue;

                Pos2i pos = positions[l.m_Index + j];

                if (is_arabic)
                    printf("glyph: %X  x: %d\n", g->Codepoint, pos.x);

                // We're dealing with absolute coordinates on an infinite line
                float offx = (pos.x - first_pos.x) * pixel_scale;
                float offy = (pos.y - first_pos.y) * pixel_scale;
                float x = line_start_x + offx;
                float y = line_start_y + offy;

                uint32_t cell_x = 0;
                uint32_t cell_y = 0;

                FontGlyph* glyph = GetGlyph(font_map, c);
                if (glyph && glyph->m_Width > 0) // only add glyphs with a size (image) to the glyph cache
                {
                    if (!IsInCache(font_map, c))
                    {
                        // Calculate y-offset in cache-cell space by moving glyphs down to baseline
                        int16_t px_cell_offset_y = font_map->m_CacheCellMaxAscent - (int16_t)glyph->m_Ascent;
                        AddGlyphToCache(font_map, frame, glyph, px_cell_offset_y);
                    }

                    CacheGlyph* cache_glyph = GetFromCache(font_map, c);
                    if (cache_glyph)
                    {
                        cell_x = cache_glyph->m_X;
                        cell_y = cache_glyph->m_Y;
                    }
                }
                else
                {
                    glyph = 0;
                }

                // We've already discarded whitespaces, but the glyph may not yet be cached.
                // TO minimize overall edge case complexity, we output a zero size quad.
                OutputGlyph(glyph,
                            recip_w, recip_h,
                            cell_x, cell_y, font_map->m_CacheCellMaxAscent, font_map->m_CacheCellPadding,
                            layer_count, layer_mask,
                            vertexindex, vertices_per_quad * valid_glyph_count,
                            te.m_Transform,
                            x, y,
                            face_color,
                            outline_color,
                            shadow_color,
                            sdf_edge_value,
                            sdf_outline,
                            sdf_smoothing,
                            sdf_shadow,
                            shadow_x,
                            shadow_y,
                            vertices);

                vertexindex += vertices_per_quad;
            }
        }

        #undef HAS_LAYER

        return vertexindex * layer_count;
    }

} // namespace
