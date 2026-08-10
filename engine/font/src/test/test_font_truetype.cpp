// Copyright 2020-2026 The Defold Foundation
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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <dlib/array.h>
#include <dlib/memory.h>
#include <dlib/testutil.h>

#include <harfbuzz/hb-ot.h>

#include "harfbuzz/font_harfbuzz.h"
#include "truetype/font_truetype.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

struct ReferenceFont
{
    uint8_t*       m_Data;
    uint32_t       m_DataSize;
    FontTrueType*  m_TrueType;
    FontHarfbuzz*  m_Harfbuzz;
    stbtt_fontinfo m_Stb;
    hb_blob_t*     m_HBBlob;
    hb_face_t*     m_HBFace;
    hb_font_t*     m_HBFont;
};

static bool LoadFont(const char* filename, ReferenceFont* font)
{
    memset(font, 0, sizeof(*font));
    char path[512];
    dmTestUtil::MakeHostPath(path, sizeof(path), filename);
    font->m_Data = dmTestUtil::ReadFile(path, &font->m_DataSize);
    if (!font->m_Data)
        return false;

    font->m_TrueType = FontTrueTypeCreate(font->m_Data, font->m_DataSize, 0);
    if (!font->m_TrueType)
        return false;

    font->m_Harfbuzz = FontHarfbuzzCreate(font->m_Data, font->m_DataSize, 0);
    if (!font->m_Harfbuzz)
        return false;

    font->m_HBBlob = hb_blob_create((const char*)font->m_Data, font->m_DataSize, HB_MEMORY_MODE_READONLY, 0, 0);
    font->m_HBFace = hb_face_create(font->m_HBBlob, 0);
    font->m_HBFont = hb_font_create(font->m_HBFace);
    hb_ot_font_set_funcs(font->m_HBFont);
    uint32_t units_per_em = hb_face_get_upem(font->m_HBFace);
    hb_font_set_scale(font->m_HBFont, units_per_em, units_per_em);
    return true;
}

static bool LoadReferenceFont(const char* filename, ReferenceFont* font)
{
    return LoadFont(filename, font) && stbtt_InitFont(&font->m_Stb, font->m_Data, 0);
}

static void FreeReferenceFont(ReferenceFont* font)
{
    hb_font_destroy(font->m_HBFont);
    hb_face_destroy(font->m_HBFace);
    hb_blob_destroy(font->m_HBBlob);
    FontHarfbuzzDestroy(font->m_Harfbuzz);
    FontTrueTypeDestroy(font->m_TrueType);
    dmMemory::AlignedFree(font->m_Data);
}

static void CompareOutlineType(ReferenceFont* font, FontOutlineType expected)
{
    ASSERT_EQ(expected, FontTrueTypeGetOutlineType(font->m_TrueType));

    ASSERT_EQ(expected, FontHarfbuzzGetOutlineType(font->m_Harfbuzz));
}

static void PushCommand(dmArray<FontOutlineCommand>& commands, FontOutlineCommandType type, float x0 = 0.0f, float y0 = 0.0f, float x1 = 0.0f, float y1 = 0.0f, float x2 = 0.0f, float y2 = 0.0f)
{
    if (commands.Full())
        commands.OffsetCapacity(32);
    FontOutlineCommand command = {};
    command.m_Type = type;
    command.m_Points[0] = { x0, y0 };
    command.m_Points[1] = { x1, y1 };
    command.m_Points[2] = { x2, y2 };
    commands.Push(command);
}

static void CloseStbContour(dmArray<FontOutlineCommand>& commands, float start_x, float start_y)
{
    FontOutlineCommand& last = commands.Back();
    if (last.m_Type == FONT_OUTLINE_LINE_TO && last.m_Points[0].m_X == start_x && last.m_Points[0].m_Y == start_y)
        commands.Pop();
    PushCommand(commands, FONT_OUTLINE_CLOSE);
}

static void GetStbOutline(stbtt_fontinfo* font, uint32_t glyph_index, dmArray<FontOutlineCommand>& commands)
{
    stbtt_vertex* vertices = 0;
    int           vertex_count = stbtt_GetGlyphShape(font, glyph_index, &vertices);
    bool          contour_open = false;
    float         start_x = 0.0f;
    float         start_y = 0.0f;
    for (int i = 0; i < vertex_count; ++i)
    {
        const stbtt_vertex& vertex = vertices[i];
        switch (vertex.type)
        {
            case STBTT_vmove:
                if (contour_open)
                    CloseStbContour(commands, start_x, start_y);
                PushCommand(commands, FONT_OUTLINE_MOVE_TO, vertex.x, vertex.y);
                start_x = vertex.x;
                start_y = vertex.y;
                contour_open = true;
                break;
            case STBTT_vline:
                PushCommand(commands, FONT_OUTLINE_LINE_TO, vertex.x, vertex.y);
                break;
            case STBTT_vcurve:
                PushCommand(commands, FONT_OUTLINE_QUADRATIC_TO, vertex.cx, vertex.cy, vertex.x, vertex.y);
                break;
            case STBTT_vcubic:
                PushCommand(commands, FONT_OUTLINE_CUBIC_TO, vertex.cx, vertex.cy, vertex.cx1, vertex.cy1, vertex.x, vertex.y);
                break;
        }
    }
    if (contour_open)
        CloseStbContour(commands, start_x, start_y);
    stbtt_FreeShape(font, vertices);
}

static uint32_t GetCommandPointCount(FontOutlineCommandType type)
{
    switch (type)
    {
        case FONT_OUTLINE_MOVE_TO:      return 1;
        case FONT_OUTLINE_LINE_TO:      return 1;
        case FONT_OUTLINE_QUADRATIC_TO: return 2;
        case FONT_OUTLINE_CUBIC_TO:     return 3;
        case FONT_OUTLINE_CLOSE:        return 0;
    }
    return 0;
}

static void NormalizeOutline(const FontOutline* outline, bool round_glyf_midpoints, dmArray<FontOutlineCommand>& commands)
{
    float start_x = 0.0f;
    float start_y = 0.0f;
    for (uint32_t i = 0; i < outline->m_CommandCount; ++i)
    {
        FontOutlineCommand command = outline->m_Commands[i];
        if (command.m_Type == FONT_OUTLINE_MOVE_TO)
        {
            start_x = command.m_Points[0].m_X;
            start_y = command.m_Points[0].m_Y;
        }
        else if (command.m_Type == FONT_OUTLINE_CLOSE && !commands.Empty())
        {
            FontOutlineCommand& last = commands.Back();
            if (last.m_Type == FONT_OUTLINE_LINE_TO && last.m_Points[0].m_X == start_x && last.m_Points[0].m_Y == start_y)
                commands.Pop();
        }
        if (round_glyf_midpoints)
        {
            // stb stores glyf vertices as integers and calculates implied
            // on-curve points with (a + b) >> 1. FontTrueType preserves .5.
            uint32_t point_count = GetCommandPointCount(command.m_Type);
            for (uint32_t point = 0; point < point_count; ++point)
            {
                command.m_Points[point].m_X = floorf(command.m_Points[point].m_X);
                command.m_Points[point].m_Y = floorf(command.m_Points[point].m_Y);
            }
        }
        if (commands.Full())
            commands.OffsetCapacity(32);
        commands.Push(command);
    }
}

static void CompareOutline(ReferenceFont* font, uint32_t codepoint, bool round_glyf_midpoints)
{
    uint32_t glyph_index = FontTrueTypeGetGlyphIndex(font->m_TrueType, codepoint);
    ASSERT_NE(0u, glyph_index);
    ASSERT_EQ((int)glyph_index, stbtt_FindGlyphIndex(&font->m_Stb, codepoint));

    FontOutline outline = {};
    ASSERT_EQ(FONT_RESULT_OK, FontTrueTypeGetGlyphOutline(font->m_TrueType, glyph_index, &outline));
    dmArray<FontOutlineCommand> stb_commands;
    dmArray<FontOutlineCommand> true_type_commands;
    GetStbOutline(&font->m_Stb, glyph_index, stb_commands);
    NormalizeOutline(&outline, round_glyf_midpoints, true_type_commands);
    ASSERT_EQ(stb_commands.Size(), true_type_commands.Size());
    for (uint32_t i = 0; i < true_type_commands.Size(); ++i)
    {
        ASSERT_EQ(stb_commands[i].m_Type, true_type_commands[i].m_Type);
        uint32_t point_count = GetCommandPointCount(true_type_commands[i].m_Type);
        for (uint32_t point = 0; point < point_count; ++point)
        {
            ASSERT_EQ(stb_commands[i].m_Points[point].m_X, true_type_commands[i].m_Points[point].m_X);
            ASSERT_EQ(stb_commands[i].m_Points[point].m_Y, true_type_commands[i].m_Points[point].m_Y);
        }
    }
    FontFreeGlyphOutline(&outline);
}

static void CompareMetrics(ReferenceFont* font, uint32_t codepoint, bool compare_harfbuzz_extents = true)
{
    uint32_t glyph_index = FontTrueTypeGetGlyphIndex(font->m_TrueType, codepoint);
    ASSERT_NE(0u, glyph_index);
    ASSERT_EQ((int)glyph_index, stbtt_FindGlyphIndex(&font->m_Stb, codepoint));

    hb_codepoint_t hb_glyph_index = 0;
    ASSERT_TRUE(hb_font_get_nominal_glyph(font->m_HBFont, codepoint, &hb_glyph_index));
    ASSERT_EQ(glyph_index, hb_glyph_index);

    int32_t advance, left_bearing;
    int     stb_advance, stb_left_bearing;
    FontTrueTypeGetGlyphHMetrics(font->m_TrueType, glyph_index, &advance, &left_bearing);
    stbtt_GetGlyphHMetrics(&font->m_Stb, glyph_index, &stb_advance, &stb_left_bearing);
    ASSERT_EQ(stb_advance, advance);
    ASSERT_EQ(stb_left_bearing, left_bearing);
    ASSERT_EQ(advance, hb_font_get_glyph_h_advance(font->m_HBFont, glyph_index));

    int32_t harfbuzz_advance, harfbuzz_left_bearing;
    FontHarfbuzzGetGlyphHMetrics(font->m_Harfbuzz, glyph_index, &harfbuzz_advance, &harfbuzz_left_bearing);
    ASSERT_EQ(advance, harfbuzz_advance);
    ASSERT_EQ(left_bearing, harfbuzz_left_bearing);

    int32_t x0, y0, x1, y1;
    int     stb_x0, stb_y0, stb_x1, stb_y1;
    ASSERT_TRUE(FontTrueTypeGetGlyphBox(font->m_TrueType, glyph_index, &x0, &y0, &x1, &y1));
    ASSERT_TRUE(stbtt_GetGlyphBox(&font->m_Stb, glyph_index, &stb_x0, &stb_y0, &stb_x1, &stb_y1));
    ASSERT_EQ(stb_x0, x0);
    ASSERT_EQ(stb_y0, y0);
    ASSERT_EQ(stb_x1, x1);
    ASSERT_EQ(stb_y1, y1);

    if (compare_harfbuzz_extents)
    {
        hb_glyph_extents_t extents;
        ASSERT_TRUE(hb_font_get_glyph_extents(font->m_HBFont, glyph_index, &extents));
        ASSERT_EQ(x0, extents.x_bearing);
        ASSERT_EQ(y1, extents.y_bearing);
        ASSERT_EQ(x1, extents.x_bearing + extents.width);
        ASSERT_EQ(y0, extents.y_bearing + extents.height);
    }
}

static void CompareAllGlyphBoundsWithOutlines(ReferenceFont* font)
{
    uint32_t glyph_count = hb_face_get_glyph_count(font->m_HBFace);
    for (uint32_t glyph_index = 0; glyph_index < glyph_count; ++glyph_index)
    {
        FontOutline outline = {};
        ASSERT_EQ(FONT_RESULT_OK, FontTrueTypeGetGlyphOutline(font->m_TrueType, glyph_index, &outline));

        float fx0, fy0, fx1, fy1;
        bool outline_has_bounds = FontGetOutlineBounds(&outline, &fx0, &fy0, &fx1, &fy1);
        int32_t x0, y0, x1, y1;
        bool box_has_bounds = FontTrueTypeGetGlyphBox(font->m_TrueType, glyph_index, &x0, &y0, &x1, &y1);
        ASSERT_EQ(outline_has_bounds, box_has_bounds);
        if (outline_has_bounds)
        {
            ASSERT_EQ((int32_t)floorf(fx0), x0);
            ASSERT_EQ((int32_t)floorf(fy0), y0);
            ASSERT_EQ((int32_t)ceilf(fx1), x1);
            ASSERT_EQ((int32_t)ceilf(fy1), y1);
        }

        FontFreeGlyphOutline(&outline);
    }
}

static void CompareVerticalMetrics(ReferenceFont* font)
{
    uint32_t units_per_em = hb_face_get_upem(font->m_HBFace);
    float    scale = FontTrueTypeGetScaleFromSize(font->m_TrueType, 1);
    ASSERT_NEAR(1.0f / units_per_em, scale, 0.000001f);
    ASSERT_NEAR(stbtt_ScaleForMappingEmToPixels(&font->m_Stb, 1.0f), scale, 0.000001f);

    int32_t ascent, descent, line_gap;
    int     stb_ascent, stb_descent, stb_line_gap;
    ASSERT_TRUE(FontTrueTypeGetVerticalMetrics(font->m_TrueType, &ascent, &descent, &line_gap));
    stbtt_GetFontVMetrics(&font->m_Stb, &stb_ascent, &stb_descent, &stb_line_gap);
    ASSERT_EQ(stb_ascent, ascent);
    ASSERT_EQ(stb_descent, descent);
    ASSERT_EQ(stb_line_gap, line_gap);

    hb_font_extents_t extents;
    ASSERT_TRUE(hb_font_get_h_extents(font->m_HBFont, &extents));
    ASSERT_EQ(ascent, extents.ascender);
    ASSERT_EQ(descent, extents.descender);
    ASSERT_EQ(line_gap, extents.line_gap);
}

static void CompareEmptyGlyph(ReferenceFont* font, uint32_t codepoint)
{
    uint32_t glyph_index = FontTrueTypeGetGlyphIndex(font->m_TrueType, codepoint);
    ASSERT_NE(0u, glyph_index);
    ASSERT_EQ((int)glyph_index, stbtt_FindGlyphIndex(&font->m_Stb, codepoint));

    int32_t advance, left_bearing;
    int     stb_advance, stb_left_bearing;
    FontTrueTypeGetGlyphHMetrics(font->m_TrueType, glyph_index, &advance, &left_bearing);
    stbtt_GetGlyphHMetrics(&font->m_Stb, glyph_index, &stb_advance, &stb_left_bearing);
    ASSERT_EQ(stb_advance, advance);
    ASSERT_EQ(stb_left_bearing, left_bearing);
    ASSERT_EQ(advance, hb_font_get_glyph_h_advance(font->m_HBFont, glyph_index));

    FontOutline outline = {};
    ASSERT_EQ(FONT_RESULT_OK, FontTrueTypeGetGlyphOutline(font->m_TrueType, glyph_index, &outline));
    ASSERT_EQ(0u, outline.m_CommandCount);
    stbtt_vertex* vertices = 0;
    ASSERT_EQ(0, stbtt_GetGlyphShape(&font->m_Stb, glyph_index, &vertices));
    stbtt_FreeShape(&font->m_Stb, vertices);
}

TEST(FontTrueTypeReference, TrueTypeGlyf)
{
    ReferenceFont font;
    ASSERT_TRUE(LoadReferenceFont("src/test/data/NotoSans-Regular.ttf", &font));
    CompareOutlineType(&font, FONT_OUTLINE_TYPE_GLYF);
    CompareVerticalMetrics(&font);
    CompareMetrics(&font, 'A');
    CompareMetrics(&font, 0x00e9);
    CompareEmptyGlyph(&font, ' ');
    CompareOutline(&font, 'A', true);
    CompareOutline(&font, 0x00e9, true);
    FreeReferenceFont(&font);
}

TEST(FontTrueTypeReference, OpenTypeCFF1)
{
    ReferenceFont font;
    ASSERT_TRUE(LoadReferenceFont("src/test/data/SourceCodePro-Regular.otf", &font));
    CompareOutlineType(&font, FONT_OUTLINE_TYPE_CFF1);
    CompareVerticalMetrics(&font);
    // Defold's size-optimized HarfBuzz build excludes CFF outline access, so
    // stb remains the independent bounds and outline reference for this file.
    CompareMetrics(&font, 'A', false);
    CompareMetrics(&font, 'B', false);
    CompareOutline(&font, 'A', false);
    CompareOutline(&font, 'B', false);
    CompareAllGlyphBoundsWithOutlines(&font);
    FreeReferenceFont(&font);
}

TEST(FontTrueTypeReference, OpenTypeCFF2)
{
    ReferenceFont font;
    ASSERT_TRUE(LoadFont("src/test/data/SourceSerif4Variable-Roman_cff2.otf", &font));
    CompareOutlineType(&font, FONT_OUTLINE_TYPE_CFF2);

    uint32_t glyph_index = FontTrueTypeGetGlyphIndex(font.m_TrueType, 'S');
    ASSERT_NE(0u, glyph_index);
    hb_codepoint_t hb_glyph_index = 0;
    ASSERT_TRUE(hb_font_get_nominal_glyph(font.m_HBFont, 'S', &hb_glyph_index));
    ASSERT_EQ(glyph_index, hb_glyph_index);

    int32_t advance;
    int32_t left_bearing;
    FontTrueTypeGetGlyphHMetrics(font.m_TrueType, glyph_index, &advance, &left_bearing);
    ASSERT_EQ(advance, hb_font_get_glyph_h_advance(font.m_HBFont, glyph_index));
    int32_t harfbuzz_advance;
    int32_t harfbuzz_left_bearing;
    FontHarfbuzzGetGlyphHMetrics(font.m_Harfbuzz, glyph_index, &harfbuzz_advance, &harfbuzz_left_bearing);
    ASSERT_EQ(advance, harfbuzz_advance);
    ASSERT_EQ(left_bearing, harfbuzz_left_bearing);

    FontOutline outline = {};
    ASSERT_EQ(FONT_RESULT_OK, FontTrueTypeGetGlyphOutline(font.m_TrueType, glyph_index, &outline));
    ASSERT_EQ(26u, outline.m_CommandCount);
    ASSERT_EQ(FONT_OUTLINE_MOVE_TO, outline.m_Commands[0].m_Type);
    ASSERT_EQ(235.0f, outline.m_Commands[0].m_Points[0].m_X);
    ASSERT_EQ(-15.0f, outline.m_Commands[0].m_Points[0].m_Y);
    ASSERT_EQ(FONT_OUTLINE_CLOSE, outline.m_Commands[outline.m_CommandCount - 1].m_Type);

    float x0;
    float y0;
    float x1;
    float y1;
    ASSERT_TRUE(FontGetOutlineBounds(&outline, &x0, &y0, &x1, &y1));
    ASSERT_EQ(40.0f, x0);
    ASSERT_EQ(-15.0f, y0);
    ASSERT_EQ(472.0f, x1);
    ASSERT_EQ(685.0f, y1);

    FontFreeGlyphOutline(&outline);
    CompareAllGlyphBoundsWithOutlines(&font);
    FreeReferenceFont(&font);
}

TEST(FontTrueTypeReference, RejectsInvalidData)
{
    const uint8_t invalid[] = { 0, 1, 0, 0 };
    ASSERT_EQ(0u, FontTrueTypeGetFaceCount(invalid, sizeof(invalid)));
    ASSERT_EQ((FontTrueType*)0, FontTrueTypeCreate(invalid, sizeof(invalid), 0));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
