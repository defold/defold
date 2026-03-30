// Copyright 2021 The Defold Foundation
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

#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/time.h>

#include <stdlib.h> // free

// Making sure we can guarantuee the functions
#define STBTT_malloc(x,u)  ((void)(u),malloc(x))
#define STBTT_free(x,u)    ((void)(u),free(x))

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "external/stb_truetype.h"

#include "font_private.h"

#if defined(FONT_USE_HARFBUZZ)
    #include <harfbuzz/hb.h>
#endif

//#define FONT_TTF_DEBUG_TIMING

#if defined(FONT_TTF_DEBUG_TIMING)
struct FontTimingScope
{
    const char* m_Label;
    const char* m_FontPath;
    uint32_t    m_GlyphIndex;
    uint64_t    m_Start;

    FontTimingScope(const char* label, const char* font_path, uint32_t glyph_index)
    : m_Label(label)
    , m_FontPath(font_path)
    , m_GlyphIndex(glyph_index)
    , m_Start(dmTime::GetMonotonicTime())
    {
    }

    ~FontTimingScope()
    {
        uint64_t tend = dmTime::GetMonotonicTime();
        float elapsed_ms = (tend - m_Start) / 1000.0f;
        dmLogWarning("FONT_TTF_DEBUG_TIMING: %s font='%s' glyph=%u took %.3f ms", m_Label, m_FontPath ? m_FontPath : "<null>", m_GlyphIndex, elapsed_ms);
    }
};

#define FONT_TTF_TIMING_SCOPE(label, font_path, glyph_index) FontTimingScope font_timing_scope(label, font_path, glyph_index)
#else
#define FONT_TTF_TIMING_SCOPE(label, font_path, glyph_index)
#endif

struct TTFFont
{
    Font            m_Base;

    stbtt_fontinfo  m_Font;

#if defined(FONT_USE_HARFBUZZ)
    hb_font_t*      m_HBFont;
#endif

    const char*     m_Path;
    const void*     m_Data;
    uint32_t        m_DataSize;

    int             m_Ascent;
    int             m_Descent;
    int             m_LineGap;
    uint32_t        m_Allocated:1;
};

static inline TTFFont* ToFont(HFont hfont)
{
    return (TTFFont*)hfont;
}

static void FontDestroyTTF(HFont hfont)
{
    TTFFont* font = ToFont(hfont);

#if defined(FONT_USE_HARFBUZZ)
    hb_font_destroy(font->m_HBFont);
#endif

    if (font->m_Allocated)
    {
        free((void*)font->m_Data);
    }
    memset(font, 0, sizeof(*font));
    delete font;
}

uint32_t GetResourceSizeTTF(HFont hfont)
{
    TTFFont* font = ToFont(hfont);
    return font->m_DataSize;
}

static float GetScaleFromSizeTTF(HFont hfont, uint32_t size)
{
    TTFFont* font = ToFont(hfont);
    return stbtt_ScaleForMappingEmToPixels(&font->m_Font, (int)size);
}

static float GetAscentTTF(HFont hfont, float scale)
{
    TTFFont* font = ToFont(hfont);
    return font->m_Ascent * scale;
}

static float GetDescentTTF(HFont hfont, float scale)
{
    TTFFont* font = ToFont(hfont);
    return font->m_Descent * scale;
}

static float GetLineGapTTF(HFont hfont, float scale)
{
    TTFFont* font = ToFont(hfont);
    return font->m_LineGap * scale;
}

static FontResult FreeGlyphTTF(HFont hfont, FontGlyph* glyph)
{
    (void)hfont;

    if (glyph->m_Bitmap.m_Data)
    {
        stbtt_FreeSDF(glyph->m_Bitmap.m_Data, 0);
    }

    free(glyph->m_Outline.m_Commands);
    glyph->m_Outline.m_Commands = 0;
    glyph->m_Outline.m_CommandCount = 0;
    glyph->m_Outline.m_Flags = 0;

    return FONT_RESULT_OK;
}

static uint32_t GetGlyphIndexTTF(HFont hfont, uint32_t codepoint)
{
    TTFFont* font = ToFont(hfont);
    stbtt_fontinfo* info = &font->m_Font;
    return (uint32_t)stbtt_FindGlyphIndex(info, (int)codepoint);
}

struct OutlineBuildContext
{
    dmArray<FontCurveCommand> m_Commands;
    bool                      m_Unsupported; // E.g. vcubic

    OutlineBuildContext()
    : m_Unsupported(false)
    {
    }
};

static void EnsureOutlineCapacity(OutlineBuildContext* ctx)
{
    if (ctx->m_Commands.Full())
    {
        ctx->m_Commands.OffsetCapacity(16);
    }
}

static void PushOutlineCommand(OutlineBuildContext* ctx, const FontCurveCommand& command)
{
    EnsureOutlineCapacity(ctx);
    ctx->m_Commands.Push(command);
}

static void OutlineMoveTo(OutlineBuildContext* ctx, float scale, float to_x, float to_y)
{
    FontCurveCommand command;
    memset(&command, 0, sizeof(command));
    command.m_Type = FONT_CURVE_MOVE_TO;
    command.m_Points[0].m_X = to_x * scale;
    command.m_Points[0].m_Y = to_y * scale;
    PushOutlineCommand(ctx, command);
}

static void OutlineLineTo(OutlineBuildContext* ctx, float scale, float to_x, float to_y)
{
    FontCurveCommand command;
    memset(&command, 0, sizeof(command));
    command.m_Type = FONT_CURVE_LINE_TO;
    command.m_Points[0].m_X = to_x * scale;
    command.m_Points[0].m_Y = to_y * scale;
    PushOutlineCommand(ctx, command);
}

static void OutlineQuadraticTo(OutlineBuildContext* ctx, float scale,
                               float control_x, float control_y,
                               float to_x, float to_y)
{
    FontCurveCommand command;
    memset(&command, 0, sizeof(command));
    command.m_Type = FONT_CURVE_QUADRATIC_TO;
    command.m_Points[0].m_X = control_x * scale;
    command.m_Points[0].m_Y = control_y * scale;
    command.m_Points[1].m_X = to_x * scale;
    command.m_Points[1].m_Y = to_y * scale;
    PushOutlineCommand(ctx, command);
}

static void OutlineClosePath(OutlineBuildContext* ctx)
{
    FontCurveCommand command;
    memset(&command, 0, sizeof(command));
    command.m_Type = FONT_CURVE_CLOSE;
    PushOutlineCommand(ctx, command);
}

static FontResult GenerateGlyphOutlineTTF(TTFFont* font, uint32_t glyph_index, float scale, FontGlyph* glyph)
{
    FONT_TTF_TIMING_SCOPE(__FUNCTION__, font->m_Path, glyph_index);

    stbtt_vertex* vertices = 0;
    int vertex_count = stbtt_GetGlyphShape(&font->m_Font, glyph_index, &vertices);
    if (vertex_count < 0)
    {
        return FONT_RESULT_ERROR;
    }

    OutlineBuildContext ctx;
    ctx.m_Commands.SetCapacity(32);
    ctx.m_Commands.SetSize(0);

    bool path_open = false;
    for (int i = 0; i < vertex_count; ++i)
    {
        stbtt_vertex& vertex = vertices[i];
        switch (vertex.type)
        {
            case STBTT_vmove:
            {
                if (path_open)
                {
                    OutlineClosePath(&ctx);
                }
                OutlineMoveTo(&ctx, scale, vertex.x, vertex.y);
                path_open = true;
                break;
            }
            case STBTT_vline:
            {
                OutlineLineTo(&ctx, scale, vertex.x, vertex.y);
                break;
            }
            case STBTT_vcurve:
            {
                OutlineQuadraticTo(&ctx, scale, vertex.cx, vertex.cy, vertex.x, vertex.y);
                break;
            }
            case STBTT_vcubic:
            {
                ctx.m_Unsupported = true;
                break;
            }
            default:
            {
                break;
            }
        }
    }

    if (path_open)
    {
        OutlineClosePath(&ctx);
    }

    stbtt_FreeShape(&font->m_Font, vertices);

    if (ctx.m_Unsupported)
    {
        return FONT_RESULT_NOT_SUPPORTED;
    }

    if (ctx.m_Commands.Size() == 0)
    {
        return FONT_RESULT_OK;
    }

    uint32_t data_size = ctx.m_Commands.Size() * sizeof(FontCurveCommand);
    FontCurveCommand* commands = (FontCurveCommand*)malloc(data_size);
    if (!commands)
    {
        return FONT_RESULT_ERROR;
    }

    memcpy(commands, ctx.m_Commands.Begin(), data_size);
    glyph->m_Outline.m_Commands = commands;
    glyph->m_Outline.m_CommandCount = ctx.m_Commands.Size();
    glyph->m_Outline.m_Flags = 0;
    return FONT_RESULT_OK;
}

static FontResult GetGlyphTTF(HFont hfont, uint32_t glyph_index, const FontGlyphOptions* options, FontGlyph* glyph)
{
    TTFFont* font = ToFont(hfont);
    FONT_TTF_TIMING_SCOPE(__FUNCTION__, font->m_Path, glyph_index);

    memset(glyph, 0, sizeof(*glyph));
    glyph->m_GlyphIndex = glyph_index;

    stbtt_fontinfo* info = &font->m_Font;

    int advx = 0, lsb = 0;
    stbtt_GetGlyphHMetrics(info, glyph_index, &advx, &lsb);

    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetGlyphBox(info, glyph_index, &x0, &y0, &x1, &y1);

    float scale = options->m_Scale;
    float padding = options->m_StbttSDFPadding;
    int on_edge_value = options->m_StbttSDFOnEdgeValue;

    float ascent = y1 * scale;
    float descent = -y0 * scale;
    int srcw = 0;
    int srch = 0;
    int offsetx = 0;
    int offsety = 0;

    if (options->m_GenerateImage)
    {
        float pixel_dist_scale = (float)on_edge_value/padding;

        glyph->m_Bitmap.m_Data = stbtt_GetGlyphSDF(info, scale, glyph_index, (int)padding, on_edge_value, pixel_dist_scale,
                                                   &srcw, &srch, &offsetx, &offsety);

        if (glyph->m_Bitmap.m_Data)
        {
            glyph->m_Bitmap.m_Flags = FONT_GLYPH_BM_FLAG_COMPRESSION_NONE;
            glyph->m_Bitmap.m_Width = srcw;
            glyph->m_Bitmap.m_Height = srch;
            glyph->m_Bitmap.m_Channels = 1;
            glyph->m_Bitmap.m_DataSize = srcw * srch * 1;

            // We don't call stbtt_FreeSDF(src, 0);
            // But instead let the user call FreeGlyphTTF()

            ascent = (float)-offsety;
            descent = (float)srch - ascent;
        }
    }

    // The dimensions of the visible area
    if (options->m_GenerateImage && x0 != x1 && y0 != y1)
    {
        // Only modify non empty glyphs (from stbtt_GetGlyphSDF())
        x0 -= padding;
        y0 -= padding;
        x1 += padding;
        y1 += padding;
    }

    glyph->m_Width = (x1 - x0) * scale;
    glyph->m_Height = (y1 - y0) * scale;
    glyph->m_Advance = advx*scale;
    glyph->m_LeftBearing = (options->m_GenerateOutline && !options->m_GenerateImage) ? x0 * scale : lsb * scale;
    glyph->m_Ascent = ascent;
    glyph->m_Descent = descent;

    if (options->m_GenerateOutline)
    {
        FontResult outline_result = GenerateGlyphOutlineTTF(font, glyph_index, scale, glyph);
        if (outline_result != FONT_RESULT_OK)
        {
            if (glyph->m_Bitmap.m_Data)
            {
                stbtt_FreeSDF(glyph->m_Bitmap.m_Data, 0);
                glyph->m_Bitmap.m_Data = 0;
            }
            return outline_result;
        }
    }

    return FONT_RESULT_OK;
}

static HFont LoadTTFInternal(const char* path, const void* buffer, uint32_t buffer_size, bool allocate);

HFont FontLoadFromMemoryTTF(const char* path, const void* buffer, uint32_t buffer_size, bool allocate)
{
    return LoadTTFInternal(path, buffer, buffer_size, allocate);
}

#if defined(FONT_USE_HARFBUZZ)
static int LoadHBFont(TTFFont* font, void* buffer, uint32_t buffer_size)
{
    hb_blob_t* blob = 0;
    hb_face_t* face = 0;
    int result = 0;

    blob = hb_blob_create((const char*)buffer, buffer_size, HB_MEMORY_MODE_READONLY, 0, 0);
    if (!blob) goto cleanup;

    face = hb_face_create(blob, 0);
    if (!face) goto cleanup;

    font->m_HBFont = hb_font_create(face);
    result = font->m_HBFont != 0;

cleanup:
    hb_face_destroy(face);
    hb_blob_destroy(blob);
    return result;
}
#endif

static HFont LoadTTFInternal(const char* path, const void* buffer, uint32_t buffer_size, bool allocate)
{
    TTFFont* font = new TTFFont;
    memset(font, 0, sizeof(*font));

    font->m_Base.m_LoadFontFromMemory = FontLoadFromMemoryTTF;
    font->m_Base.m_DestroyFont = FontDestroyTTF;
    font->m_Base.m_GetResourceSize = GetResourceSizeTTF;
    font->m_Base.m_GetScaleFromSize = GetScaleFromSizeTTF;
    font->m_Base.m_GetAscent = GetAscentTTF;
    font->m_Base.m_GetDescent = GetDescentTTF;
    font->m_Base.m_GetLineGap = GetLineGapTTF;
    font->m_Base.m_GetGlyphIndex = GetGlyphIndexTTF;
    font->m_Base.m_GetGlyph = GetGlyphTTF;
    font->m_Base.m_FreeGlyph = FreeGlyphTTF;

    if (allocate)
    {
        font->m_DataSize = buffer_size;
        font->m_Data     = (const void*)malloc(buffer_size);
        memcpy((void*)font->m_Data, buffer, buffer_size);
        font->m_Allocated = 1;
    }
    else
    {
        font->m_Data    = buffer;
        font->m_DataSize= buffer_size;
    }

#if defined(FONT_USE_HARFBUZZ)
    if (!LoadHBFont(font, (void*)font->m_Data, font->m_DataSize))
    {
        dmLogError("Failed to create Harfbuzz font from '%s'", path);
        FontDestroyTTF((HFont)font);
        delete font;
        return 0;
    }
#endif

    int index = stbtt_GetFontOffsetForIndex((const unsigned char*)font->m_Data,0);
    int result = stbtt_InitFont(&font->m_Font, (const unsigned char*)font->m_Data, index);
    if (!result)
    {
        dmLogError("Failed to load font from '%s'", path);
        FontDestroyTTF((HFont)font);
        delete font;
        return 0;
    }

    stbtt_GetFontVMetrics(&font->m_Font, &font->m_Ascent, &font->m_Descent, &font->m_LineGap);
    return (HFont)font;
}

#if defined(FONT_USE_HARFBUZZ)
hb_font_t* FontGetHarfbuzzFontFromTTF(HFont hfont)
{
    TTFFont* font = ToFont(hfont);
    return font->m_HBFont;
}
#endif

bool FontGetGlyphBoxTTF(HFont hfont, uint32_t glyph_index, int32_t* x0, int32_t* y0, int32_t* x1, int32_t* y1)
{
    TTFFont* font = ToFont(hfont);
    x0 = y0 = x1 = y1 = 0;
    return stbtt_GetGlyphBox(&font->m_Font, glyph_index, x0, y0, x1, y1);
}
