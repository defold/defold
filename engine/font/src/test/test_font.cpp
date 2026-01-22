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

#include <stdio.h>
#include <stdint.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "../font_private.h"
#include "../util.h"

#include <dlib/log.h>
#include <dlib/testutil.h>
#include <dlib/time.h>
#include <dlib/utf8.h>

#include "font.h"
#include "fontcollection.h"
#include "text_layout.h"

//static const char* g_TextLorem = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Ut tempus quam in lacinia imperdiet. Vestibulum interdum erat quis purus lacinia, at ullamcorper arcu sagittis. Etiam molestie varius lacus, eget fringilla enim tempor quis. In at mollis dolor, et dictum sem. Mauris condimentum metus sed auctor tempus.";

static const char* g_TextArabic = "دينيس ريتشي فاش كان خدام ف مختبرات بيل، مابين 1972 و 1973";

class FontTest : public jc_test_base_class
{
protected:
    HFont           m_Font;
    HFontCollection m_FontCollection;

    virtual void SetUp() override
    {
        LoadFont("src/test/vera_mo_bd.ttf", &m_Font);

        m_FontCollection = FontCollectionCreate();
        FontResult r = FontCollectionAddFont(m_FontCollection, m_Font);
        ASSERT_EQ(FONT_RESULT_OK, r);
    }

    virtual void TearDown() override
    {
        FontCollectionDestroy(m_FontCollection);
        FontDestroy(m_Font);
    }

    void LoadFont(const char* path, HFont* out)
    {
        char buffer[512];
        const char* host_path = dmTestUtil::MakeHostPath(buffer, sizeof(buffer), path);
        
        HFont font = FontLoadFromPath(host_path);
        ASSERT_NE((HFont)0, font);

        const char* font_path = FontGetPath(font);
        ASSERT_STREQ(path, font_path);

        uint32_t path_hash = dmHashString32(path);
        ASSERT_EQ(path_hash, FontGetPathHash(font));

        *out = font;
    }
};

TEST_F(FontTest, LoadTTF)
{
    // Empty. Just loading/unloading a font
}


static uint32_t TextToCodePoints(const char* text, dmArray<uint32_t>& codepoints)
{
    uint32_t len = dmUtf8::StrLen(text);
    codepoints.SetCapacity(len);
    codepoints.SetSize(0);
    const char* cursor = text;
    while (uint32_t c = dmUtf8::NextChar(&cursor))
    {
        codepoints.Push(c);
    }
    return len;
}

static TextResult TestLayout(HFontCollection coll, dmArray<uint32_t>& codepoints,
                        TextLayoutSettings* settings,
                        HTextLayout* layout)
{
    uint64_t tstart = dmTime::GetMonotonicTime();

    uint32_t* pc = codepoints.Begin();
    uint32_t num_codepoints = codepoints.Size();
    TextResult r = TextLayoutCreate(coll, pc, num_codepoints, settings, layout);

    uint64_t tend = dmTime::GetMonotonicTime();
    if (*layout)
    {
        printf("Layout %u codepoints into %u glyphs took %.3f ms\n", codepoints.Size(), (*layout)->m_Glyphs.Size(), (tend-tstart)/1000.0f);
    }

    return r;
}

static void DebugPrintLayout(HTextLayout layout)
{
    printf("Layout:\n");
    printf("  %u lines, max width: %.3f\n", layout->m_Lines.Size(), layout->m_Width);

    uint32_t num_lines = layout->m_Lines.Size();
    for (uint32_t i = 0; i < num_lines; ++i)
    {
        TextLine& line = layout->m_Lines[i];
        printf("  %u: off: %3u  len: %3u  width: %.3f  |", i, line.m_Index, line.m_Length, line.m_Width);

        uint32_t end = line.m_Index + line.m_Length;
        for (uint32_t j = line.m_Index; j < end; ++j)
        {
            TextGlyph* glyph = &layout->m_Glyphs[j];
            uint32_t c = glyph->m_Codepoint;
            printf("%c", (char)c);
        }

        printf("|  idx: |");

        for (uint32_t j = line.m_Index; j < end; ++j)
        {
            TextGlyph* glyph = &layout->m_Glyphs[j];
            uint32_t gi = glyph->m_GlyphIndex;
            printf("%4u ", gi);
        }
        printf("|\n");
    }
}

TEST_F(FontTest, LayoutSingleLine)
{
    dmArray<uint32_t> codepoints;

    // Note: Simulate an input field, where adding extra spaces would move the visible cursor
    const char* original_text = "Hello World!  ";
    TextToCodePoints(original_text, codepoints);

    TextLayoutSettings settings = {0};
    settings.m_LineBreak = false;
    settings.m_Width = 0;
    settings.m_Size = 28.0f;

    HTextLayout layout = 0;

    TextResult r = TestLayout(m_FontCollection, codepoints, &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, r);
    ASSERT_NE((TextLayout*)0, layout);
    DebugPrintLayout(layout);
    ASSERT_EQ(1u, layout->m_Lines.Size());
    ASSERT_LT(0.0f, layout->m_Width);
    ASSERT_GE(300.0f, layout->m_Width);

    TextLine& line = layout->m_Lines[0];
    ASSERT_EQ(0u, line.m_Index);
    ASSERT_EQ(14u, line.m_Length);

    dmArray<char> outtext;
    outtext.SetCapacity(line.m_Length);
    for (uint32_t i = 0; i < line.m_Length; ++i)
    {
        TextGlyph& g = layout->m_Glyphs[i];
        outtext.Push((char)g.m_Codepoint);
    }
    ASSERT_ARRAY_EQ_LEN(original_text, outtext.Begin(), line.m_Length);

    TextLayoutFree(layout);

    // Test the same without any lines
    r = TestLayout(m_FontCollection, codepoints, &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, r);
    ASSERT_NE((TextLayout*)0, layout);
    DebugPrintLayout(layout);
    ASSERT_EQ(1u, layout->m_Lines.Size());

    TextLayoutFree(layout);
}

// See https://github.com/defold/defold/issues/11766
TEST_F(FontTest, LayoutSingleLineWithUnknownCharacterLast)
{
    HFont font;
    LoadFont("src/test/vera_mo_bd_atoz.ttf", &font);
    
    HFontCollection fontCollection = FontCollectionCreate();
    FontResult fr = FontCollectionAddFont(fontCollection, font);
    ASSERT_EQ(FONT_RESULT_OK, fr);

    dmArray<uint32_t> codepoints;

    const char* original_text = "HELLO WORLD!";
    TextToCodePoints(original_text, codepoints);

    TextLayoutSettings settings = {0};
    settings.m_LineBreak = false;
    settings.m_Width = 0;
    settings.m_Size = 28.0f;

    HTextLayout layout = 0;
    TextResult tr = TestLayout(fontCollection, codepoints, &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, tr);
    TextLine& line = layout->m_Lines[0];
    ASSERT_NE(0.0, line.m_Width);

    FontCollectionDestroy(fontCollection);
    TextLayoutFree(layout);
    FontDestroy(font);
}

TEST_F(FontTest, LayoutEmptyString)
{
    TextLayoutSettings settings = {0};
    settings.m_LineBreak = false;
    settings.m_Width = 0.0f;
    settings.m_Size = 16.0f;

    HTextLayout layout = 0;
    TextResult r = TextLayoutCreate(m_FontCollection, 0, 0, &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, r);
    ASSERT_NE((HTextLayout)0, layout);
    ASSERT_EQ(0u, layout->m_Lines.Size());
    TextLayoutFree(layout);
}

TEST_F(FontTest, LayoutMultiLine)
{
    dmArray<uint32_t> codepoints;

    // NOTE: For multiline text, we strip the whitespaces off of each line
    const char* original_text = "Hello World!   How are you?  ";

    // NOTE: Our rules for breaking is a bit weird,
    // but changing them is for another time
#if defined(FONT_USE_SKRIBIDI)
    const char* expected_text_1 = "Hello World!   ";
    const char* expected_text_2 = "How are you?  ";
    uint32_t line2_start = (uint32_t)strlen(expected_text_1);
#else
    const char* expected_text_1 = "Hello World!";
    const char* expected_text_2 = "  How are you?";
    uint32_t line2_start = 13u;
#endif

    TextToCodePoints(original_text, codepoints);

    TextLayoutSettings settings = {0};
    settings.m_LineBreak = true;
    settings.m_Width = 260.0f;
    settings.m_Size = 28.0f;

    HTextLayout layout = 0;

    TextResult r = TestLayout(m_FontCollection, codepoints, &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, r);
    ASSERT_NE((TextLayout*)0, layout);
    DebugPrintLayout(layout);
    ASSERT_EQ(2u, layout->m_Lines.Size());

    TextLine& line1 = layout->m_Lines[0];
    TextLine& line2 = layout->m_Lines[1];

    ASSERT_EQ(0u, line1.m_Index);
    ASSERT_EQ((uint32_t)strlen(expected_text_1), line1.m_Length);
    ASSERT_EQ(line2_start, line2.m_Index);
    ASSERT_EQ((uint32_t)strlen(expected_text_2), line2.m_Length);

    dmArray<char> outtext;
    outtext.SetCapacity(layout->m_Glyphs.Size()+1);
    for (uint32_t i = 0; i < layout->m_Glyphs.Size(); ++i)
    {
        TextGlyph& g = layout->m_Glyphs[i];
        outtext.Push((char)g.m_Codepoint);
    }
    outtext.Push(0);
    ASSERT_ARRAY_EQ_LEN(expected_text_1, outtext.Begin() + line1.m_Index, line1.m_Length);
    ASSERT_ARRAY_EQ_LEN(expected_text_2, outtext.Begin() + line2.m_Index, line2.m_Length);

    TextLayoutFree(layout);
}

TEST_F(FontTest, LayoutExplicitLineBreaks)
{
    dmArray<uint32_t> codepoints;

    // Explicit line break should always split into multiple lines,
    // even when automatic line breaking is disabled.
    const char* original_text = "Hello World!\nHow are you?  ";
    const char* expected_text_1 = "Hello World!";
    const char* expected_text_2 = "How are you?  ";

    TextToCodePoints(original_text, codepoints);

    TextLayoutSettings settings = {0};
    settings.m_LineBreak = false; // do not auto-wrap; rely on explicit '\n'
    settings.m_Width = 0.0f;
    settings.m_Size = 28.0f;

    HTextLayout layout = 0;
    TextResult r = TestLayout(m_FontCollection, codepoints, &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, r);
    ASSERT_NE((TextLayout*)0, layout);

    DebugPrintLayout(layout);
    ASSERT_EQ(2u, layout->m_Lines.Size());

    TextLine& line1 = layout->m_Lines[0];
    TextLine& line2 = layout->m_Lines[1];

    // Collect laid out codepoints back into a contiguous buffer for comparison
    dmArray<char> outtext;
    outtext.SetCapacity(layout->m_Glyphs.Size()+1);
    for (uint32_t i = 0; i < layout->m_Glyphs.Size(); ++i)
    {
        TextGlyph& g = layout->m_Glyphs[i];
        outtext.Push((char)g.m_Codepoint);
    }
    outtext.Push(0);

    ASSERT_EQ((uint32_t)strlen(expected_text_1), line1.m_Length);
    ASSERT_ARRAY_EQ_LEN(expected_text_1, outtext.Begin() + line1.m_Index, line1.m_Length);

    ASSERT_EQ((uint32_t)strlen(expected_text_2), line2.m_Length);
    ASSERT_ARRAY_EQ_LEN(expected_text_2, outtext.Begin() + line2.m_Index, line2.m_Length);

    TextLayoutFree(layout);
}

TEST_F(FontTest, LayoutExplicitDoubleLineBreaks)
{
    dmArray<uint32_t> codepoints;

    // Two consecutive newlines create an empty middle line
    const char* original_text = "abc\n\nbar";
    const char* expected_text_1 = "abc";
    const char* expected_text_2 = "";   // empty line
    const char* expected_text_3 = "bar";

    TextToCodePoints(original_text, codepoints);

    TextLayoutSettings settings = {0};
    settings.m_LineBreak = false; // do not auto-wrap; rely on explicit '\n'
    settings.m_Width = 0.0f;
    settings.m_Size = 28.0f;

    HTextLayout layout = 0;
    TextResult r = TestLayout(m_FontCollection, codepoints, &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, r);
    ASSERT_NE((TextLayout*)0, layout);

    DebugPrintLayout(layout);
    ASSERT_EQ(3u, layout->m_Lines.Size());

    TextLine& line1 = layout->m_Lines[0];
    TextLine& line2 = layout->m_Lines[1];
    TextLine& line3 = layout->m_Lines[2];

    dmArray<char> outtext;
    outtext.SetCapacity(layout->m_Glyphs.Size()+1);
    for (uint32_t i = 0; i < layout->m_Glyphs.Size(); ++i)
    {
        TextGlyph& g = layout->m_Glyphs[i];
        outtext.Push((char)g.m_Codepoint);
    }
    outtext.Push(0);

    ASSERT_EQ((uint32_t)strlen(expected_text_1), line1.m_Length);
    ASSERT_ARRAY_EQ_LEN(expected_text_1, outtext.Begin() + line1.m_Index, line1.m_Length);

    ASSERT_EQ((uint32_t)strlen(expected_text_2), line2.m_Length);
    // Only compare when there is content
    if (line2.m_Length > 0)
        ASSERT_ARRAY_EQ_LEN(expected_text_2, outtext.Begin() + line2.m_Index, line2.m_Length);

    ASSERT_EQ((uint32_t)strlen(expected_text_3), line3.m_Length);
    ASSERT_ARRAY_EQ_LEN(expected_text_3, outtext.Begin() + line3.m_Index, line3.m_Length);

    TextLayoutFree(layout);
}

TEST_F(FontTest, LayoutTrackingAndLeading)
{
    dmArray<uint32_t> codepoints;

    HTextLayout layout = 0;

    TextLayoutSettings settings = {0};
    settings.m_LineBreak = false;
    settings.m_Width = 0.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    settings.m_Tracking = 0.0f;

    // Capture the layout line height used by the engine for comparison.
    const char* line_height_text = "A";
    TextToCodePoints(line_height_text, codepoints);

    TextResult r = TestLayout(m_FontCollection, codepoints, &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, r);
    ASSERT_NE((HTextLayout)0, layout);
    float line_height = layout->m_Height;
    TextLayoutFree(layout);

    // Measure tracking impact as a width delta between two adjacent glyphs.
    const char* tracking_text = "AA";
    TextToCodePoints(tracking_text, codepoints);
    r = TestLayout(m_FontCollection, codepoints, &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, r);
    ASSERT_NE((HTextLayout)0, layout);
    float width_no_tracking = layout->m_Width;
#if !defined(FONT_USE_SKRIBIDI)
    float first_advance = layout->m_Glyphs[0].m_Advance;
#endif
    TextLayoutFree(layout);

    float tracking_value = 0.25f;
    settings.m_Tracking = tracking_value;
    r = TestLayout(m_FontCollection, codepoints, &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, r);
    ASSERT_NE((HTextLayout)0, layout);
    float width_tracking = layout->m_Width;
    TextLayoutFree(layout);

    // Legacy tracking scales by line height and is quantized to integer pixel steps.
    // Skribidi scales by font size in pixels.
    float expected_tracking = 0.0f;
#if defined(FONT_USE_SKRIBIDI)
    // Skribidi interprets tracking in pixels based on font size.
    expected_tracking = tracking_value * settings.m_Size;
#else
    // Legacy uses line height (font metrics scaled by size) for tracking.
    float scale = FontGetScaleFromSize(m_Font, settings.m_Size);
    float raw_tracking = tracking_value * line_height * scale;
    uint32_t advance_px = (uint32_t)first_advance;
    uint32_t advance_plus_tracking_px = (uint32_t)(first_advance + raw_tracking);
    expected_tracking = (float)(advance_plus_tracking_px - advance_px);
#endif

    const float epsilon = 0.05f;
    ASSERT_NEAR(expected_tracking, width_tracking - width_no_tracking, epsilon);

    // Enable explicit line breaks and compare leading deltas across two lines.
    // Skribidi layout height is normalized in text_layout_skribidi.cpp to match legacy.
    const char* leading_text = "A\nA";
    TextToCodePoints(leading_text, codepoints);

    settings.m_LineBreak = true;
    settings.m_Width = 1000.0f;
    settings.m_Tracking = 0.0f;
    settings.m_Leading = 1.0f;

    r = TestLayout(m_FontCollection, codepoints, &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, r);
    ASSERT_NE((HTextLayout)0, layout);
    uint32_t line_count = layout->m_Lines.Size();
    ASSERT_EQ(2u, line_count);
    float height_leading_1 = layout->m_Height;
    TextLayoutFree(layout);

    settings.m_Leading = 2.0f;
    r = TestLayout(m_FontCollection, codepoints, &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, r);
    ASSERT_NE((HTextLayout)0, layout);
    ASSERT_EQ(2u, layout->m_Lines.Size());
    float height_leading_2 = layout->m_Height;
    TextLayoutFree(layout);

    // Leading should add one extra line height for the entire layout.
    float expected_leading_delta = line_height;
    float height_leading_delta = height_leading_2 - height_leading_1;
    ASSERT_NEAR(expected_leading_delta, height_leading_delta, epsilon);
}


#if !defined(FONT_USE_SKRIBIDI) && defined(FOO)
static void CreateTestGlyphs(TextShapeInfo* info, const char* text, int32_t x_step, dmArray<uint32_t>& codepoints)
{
    uint32_t len = TextToCodePoints(text, codepoints);

    info->m_Glyphs.SetCapacity(len);
    info->m_Glyphs.SetSize(0);

    uint32_t num_valid_glyphs = 0;
    for (uint32_t i = 0; i < len; ++i)
    {
        uint32_t c = codepoints[i];

        TextGlyph g = {0};
        g.m_X = i * x_step;
        g.m_Y = 0;
        if (c == dmUtf8::UTF_WHITESPACE_NEW_LINE)
        {
            g.m_Width = 0;
            g.m_Height = 0;
        }
        else
        {
            g.m_Width = x_step;
            g.m_Height = x_step;
        }
        g.m_Codepoint = c;
        info->m_Glyphs.Push(g);

        if (!dmUtf8::IsWhiteSpace(c))
            ++num_valid_glyphs;
    }

    TextRun run;
    run.m_Index = 0;
    run.m_Length = len;

    info->m_Runs.SetCapacity(1);
    info->m_Runs.SetSize(0);
    info->m_Runs.Push(run);

    info->m_NumValidGlyphs = num_valid_glyphs;

    printf("**********************************************************\n");
    {

        TextGlyph* glyphs = info->m_Glyphs.Begin();
        uint32_t        num_glyphs = info->m_Glyphs.Size();
        printf("Layout %u: |", num_glyphs);
        for (int f = 0; f < num_glyphs; ++f)
        {
            printf("%c", glyphs[f].m_Codepoint);
        }
        printf("|\n");
    }
    printf("**********************************************************\n");

}

#define ASSERT_LINE(index, count, lines, i)\
    ASSERT_EQ(char_width * count, lines[i].m_Width);\
    ASSERT_EQ(index, lines[i].m_Index);\
    ASSERT_EQ(count, lines[i].m_Length);

TEST_F(FontTest, Layout)
{
    const float char_width = 4;

    const uint32_t  lines_count = 8;
    TextLine        lines[lines_count];

    TextShapeResult r;

    TextMetrics metrics;
    TextShapeInfo info;
    info.m_Font = m_Font;

    TextLayoutSettings settings = {0};
    settings.m_LineBreak = false;

    dmArray<uint32_t> codepoints;

    memset(&metrics, 0, sizeof(metrics));
    settings.m_Width = 100;
    CreateTestGlyphs(&info, "", char_width, codepoints);
    r = TextLayout(&settings, &info, lines, lines_count, &metrics);
    DebugPrintLayout(&info, &metrics, lines, 1.0f);
    ASSERT_EQ(TEXT_SHAPE_RESULT_OK, r);
    ASSERT_EQ(0, metrics.m_LineCount);
    ASSERT_EQ(0, metrics.m_Width);

    memset(&metrics, 0, sizeof(metrics));
    settings.m_Width = 100;
    CreateTestGlyphs(&info, "x", char_width, codepoints);
    r = TextLayout(&settings, &info, lines, lines_count, &metrics);
    DebugPrintLayout(&info, &metrics, lines, 1.0f);
    ASSERT_EQ(TEXT_SHAPE_RESULT_OK, r);
    ASSERT_EQ(1, metrics.m_LineCount);
    ASSERT_LINE(0, 1, lines, 0);
    ASSERT_EQ(char_width * 1, metrics.m_Width);

    memset(&metrics, 0, sizeof(metrics));
    settings.m_Width = 100;
    CreateTestGlyphs(&info, "x\x00 123", char_width, codepoints);
    r = TextLayout(&settings, &info, lines, lines_count, &metrics);
    DebugPrintLayout(&info, &metrics, lines, 1.0f);
    ASSERT_EQ(TEXT_SHAPE_RESULT_OK, r);
    ASSERT_EQ(1, metrics.m_LineCount);
    ASSERT_LINE(0, 1, lines, 0);
    ASSERT_EQ(char_width * 1, metrics.m_Width);

    memset(&metrics, 0, sizeof(metrics));
    settings.m_Width = 0;
    CreateTestGlyphs(&info, "x", char_width, codepoints);
    r = TextLayout(&settings, &info, lines, lines_count, &metrics);
    DebugPrintLayout(&info, &metrics, lines, 1.0f);
    ASSERT_EQ(TEXT_SHAPE_RESULT_OK, r);
    ASSERT_EQ(1, metrics.m_LineCount);
    ASSERT_LINE(0, 1, lines, 0);
    ASSERT_EQ(char_width * 1, metrics.m_Width);

    memset(&metrics, 0, sizeof(metrics));
    settings.m_Width = 3 * char_width;
    CreateTestGlyphs(&info, "abc", char_width, codepoints);
    r = TextLayout(&settings, &info, lines, lines_count, &metrics);
    DebugPrintLayout(&info, &metrics, lines, 1.0f);
    ASSERT_EQ(TEXT_SHAPE_RESULT_OK, r);
    ASSERT_EQ(1, metrics.m_LineCount);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_EQ(char_width * 3, metrics.m_Width);

    memset(&metrics, 0, sizeof(metrics));
    settings.m_Width = 3 * char_width - 1;
    CreateTestGlyphs(&info, "abc", char_width, codepoints);
    r = TextLayout(&settings, &info, lines, lines_count, &metrics);
    DebugPrintLayout(&info, &metrics, lines, 1.0f);
    ASSERT_EQ(TEXT_SHAPE_RESULT_OK, r);
    ASSERT_EQ(1, metrics.m_LineCount);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_EQ(char_width * 3, metrics.m_Width);

    memset(&metrics, 0, sizeof(metrics));
    settings.m_LineBreak = true;
    settings.m_Width = 3 * char_width;
    CreateTestGlyphs(&info, "abc bar", char_width, codepoints);
    r = TextLayout(&settings, &info, lines, lines_count, &metrics);
    DebugPrintLayout(&info, &metrics, lines, 1.0f);
    ASSERT_EQ(TEXT_SHAPE_RESULT_OK, r);
    ASSERT_EQ(2, metrics.m_LineCount);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_LINE(4, 3, lines, 1);
    ASSERT_EQ(char_width * 3, metrics.m_Width);

    // Don't split a word in two, even if the width is shorter than the line width
    memset(&metrics, 0, sizeof(metrics));
    settings.m_LineBreak = true;
    settings.m_Width = 3 * char_width;
    CreateTestGlyphs(&info, "abc defg", char_width, codepoints);
    r = TextLayout(&settings, &info, lines, lines_count, &metrics);
    DebugPrintLayout(&info, &metrics, lines, 1.0f);
    ASSERT_EQ(TEXT_SHAPE_RESULT_OK, r);
    ASSERT_EQ(2, metrics.m_LineCount);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_LINE(4, 4, lines, 1);
    ASSERT_EQ(char_width * 4, metrics.m_Width);

    memset(&metrics, 0, sizeof(metrics));
    settings.m_LineBreak = true;
    settings.m_Width = 3 * char_width;
    CreateTestGlyphs(&info, "abcd efg", char_width, codepoints);
    r = TextLayout(&settings, &info, lines, lines_count, &metrics);
    DebugPrintLayout(&info, &metrics, lines, 1.0f);
    ASSERT_EQ(TEXT_SHAPE_RESULT_OK, r);
    ASSERT_EQ(2, metrics.m_LineCount);
    ASSERT_LINE(0, 4, lines, 0);
    ASSERT_LINE(5, 3, lines, 1);
    ASSERT_EQ(char_width * 4, metrics.m_Width);

    memset(&metrics, 0, sizeof(metrics));
    settings.m_LineBreak = true;
    settings.m_Width = 1000;
    CreateTestGlyphs(&info, "abc bar", char_width, codepoints);
    r = TextLayout(&settings, &info, lines, lines_count, &metrics);
    DebugPrintLayout(&info, &metrics, lines, 1.0f);
    ASSERT_EQ(TEXT_SHAPE_RESULT_OK, r);
    ASSERT_EQ(1u, metrics.m_LineCount);
    ASSERT_LINE(0, 7, lines, 0);
    ASSERT_EQ(char_width * 7, metrics.m_Width);

    memset(&metrics, 0, sizeof(metrics));
    settings.m_LineBreak = true;
    settings.m_Width = 1000;
    CreateTestGlyphs(&info, "abc  bar", char_width, codepoints);
    r = TextLayout(&settings, &info, lines, lines_count, &metrics);
    DebugPrintLayout(&info, &metrics, lines, 1.0f);
    ASSERT_EQ(TEXT_SHAPE_RESULT_OK, r);
    ASSERT_EQ(1u, metrics.m_LineCount);
    ASSERT_LINE(0, 8, lines, 0);
    ASSERT_EQ(char_width * 8, metrics.m_Width);

    memset(&metrics, 0, sizeof(metrics));
    settings.m_LineBreak = true;
    settings.m_Width = 3 * char_width;
    CreateTestGlyphs(&info, "abc\n\nbar", char_width, codepoints);
    r = TextLayout(&settings, &info, lines, lines_count, &metrics);
    DebugPrintLayout(&info, &metrics, lines, 1.0f);
    ASSERT_EQ(TEXT_SHAPE_RESULT_OK, r);
    ASSERT_EQ(3u, metrics.m_LineCount);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_LINE(4, 0, lines, 1);
    ASSERT_LINE(5, 3, lines, 2);
    ASSERT_EQ(char_width * 3, metrics.m_Width);

    // // 0x200B = Unicode "zero width space", UTF8 representation: E2 80 8B
    memset(&metrics, 0, sizeof(metrics));
    settings.m_LineBreak = true;
    settings.m_Width = 3 * char_width;
    CreateTestGlyphs(&info, "abc" "\xe2\x80\x8b" "bar", char_width, codepoints);
    r = TextLayout(&settings, &info, lines, lines_count, &metrics);
    DebugPrintLayout(&info, &metrics, lines, 1.0f);
    ASSERT_EQ(TEXT_SHAPE_RESULT_OK, r);
    ASSERT_EQ(2, metrics.m_LineCount);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_LINE(4, 3, lines, 1);
    ASSERT_EQ(char_width * 3, metrics.m_Width);

    // // Note that second line would include a "zero width space" as first
    // // character since we don't trim whitespace currently.
    memset(&metrics, 0, sizeof(metrics));
    settings.m_LineBreak = true;
    settings.m_Width = 3 * char_width;
    CreateTestGlyphs(&info, "abc" "\xe2\x80\x8b\xe2\x80\x8b" "bar", char_width, codepoints);
    r = TextLayout(&settings, &info, lines, lines_count, &metrics);
    DebugPrintLayout(&info, &metrics, lines, 1.0f);
    ASSERT_EQ(TEXT_SHAPE_RESULT_OK, r);
    ASSERT_EQ(2, metrics.m_LineCount);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_LINE(4, 4, lines, 1);
    ASSERT_EQ(char_width * 4, metrics.m_Width);

    // åäö
    memset(&metrics, 0, sizeof(metrics));
    settings.m_LineBreak = true;
    settings.m_Width = 3 * char_width;
    CreateTestGlyphs(&info, "\xc3\xa5\xc3\xa4\xc3\xb6", char_width, codepoints);
    r = TextLayout(&settings, &info, lines, lines_count, &metrics);
    DebugPrintLayout(&info, &metrics, lines, 1.0f);
    ASSERT_EQ(TEXT_SHAPE_RESULT_OK, r);
    ASSERT_EQ(1, metrics.m_LineCount);
    ASSERT_EQ(char_width * 3, lines[0].m_Width);
    ASSERT_LINE(0, 3, lines, 0);
    ASSERT_EQ(char_width * 3, metrics.m_Width);

    memset(&metrics, 0, sizeof(metrics));
    settings.m_LineBreak = true;
    settings.m_Width = 3 * char_width;
    CreateTestGlyphs(&info, "Welcome to the Kingdom of Games...", char_width, codepoints);
    r = TextLayout(&settings, &info, lines, lines_count, &metrics);
    DebugPrintLayout(&info, &metrics, lines, 1.0f);
    ASSERT_EQ(TEXT_SHAPE_RESULT_OK, r);
    ASSERT_EQ(6, metrics.m_LineCount);
    ASSERT_LINE(0, 7, lines, 0);
    ASSERT_LINE(8, 2, lines, 1);
    ASSERT_LINE(11, 3, lines, 2);
    ASSERT_LINE(15, 7, lines, 3);
    ASSERT_LINE(23, 2, lines, 4);
    ASSERT_LINE(26, 8, lines, 5);
    ASSERT_EQ(char_width * 8, metrics.m_Width);

    memset(&metrics, 0, sizeof(metrics));
    settings.m_LineBreak = false;
    settings.m_Width = 1000000.0f;
    CreateTestGlyphs(&info, "Hello World!\nHow are you?  ", char_width, codepoints);
    r = TextLayout(&settings, &info, lines, lines_count, &metrics);
    DebugPrintLayout(&info, &metrics, lines, 1.0f);
    ASSERT_EQ(TEXT_SHAPE_RESULT_OK, r);
    ASSERT_EQ(2, metrics.m_LineCount);
    ASSERT_LINE(0, 12, lines, 0);
    ASSERT_LINE(13, 14, lines, 1);
    ASSERT_EQ(char_width * 14, metrics.m_Width);

    memset(&metrics, 0, sizeof(metrics));
    settings.m_LineBreak = true;
    settings.m_Width = 17 * char_width;
    CreateTestGlyphs(&info, "Hello World!   How are you?  ", char_width, codepoints);
    r = TextLayout(&settings, &info, lines, lines_count, &metrics);
    DebugPrintLayout(&info, &metrics, lines, 1.0f);
    ASSERT_EQ(TEXT_SHAPE_RESULT_OK, r);
    ASSERT_EQ(2, metrics.m_LineCount);
    ASSERT_LINE(0, 12, lines, 0);
    ASSERT_LINE(13, 14, lines, 1);
    ASSERT_EQ(char_width * 14, metrics.m_Width);
}

#endif // !defined(FONT_USE_SKRIBIDI)

#if defined(FONT_USE_SKRIBIDI)
TEST_F(FontTest, TextArabic)
{
    HFont font;
    LoadFont("src/test/NotoSansArabic-Regular.ttf", &font);

    HFontCollection fontCollection = FontCollectionCreate();
    FontResult fr = FontCollectionAddFont(fontCollection, font);
    ASSERT_EQ(FONT_RESULT_OK, fr);

    dmArray<uint32_t> codepoints;
    TextToCodePoints(g_TextArabic, codepoints);

    TextLayoutSettings settings = {0};
    settings.m_LineBreak = false;
    settings.m_Width = 260.0f;
    settings.m_Size = 28.0f;

    HTextLayout layout = 0;

    TextResult tr = TestLayout(fontCollection, codepoints, &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, tr);
    ASSERT_NE((TextLayout*)0, layout);
    DebugPrintLayout(layout);
    ASSERT_EQ(1u, layout->m_Lines.Size());

    printf("Codepoints: %u\n    ", codepoints.Size());
    for (uint32_t i = 0; i < codepoints.Size(); ++i)
    {
        printf("0x%X ", codepoints[i]);
    }
    printf("\n");
    for (uint32_t i = 0; i < codepoints.Size(); ++i)
    {
        printf("'%c' ", codepoints[i]);
    }
    printf("\n");

    DebugPrintLayout(layout);

    FontCollectionDestroy(fontCollection);
    TextLayoutFree(layout);
    FontDestroy(font);
}
#endif

// static int TestStandalone(const char* path, float size, float padding, const char* text)
// {
//     HFont font = FontLoadFromPath(path);
//     if (!font)
//     {
//         dmLogError("Failed to load font '%s'", path);
//         return 1;
//     }

//     float scale = FontGetScaleFromSize(font, size);
//     FontDebug(font, scale, padding, text);

//     TextLayoutSettings settings = {0};
//     settings.m_LineBreak = true;
//     settings.m_Width = 600 / scale;
//     settings.m_Leading = 0;
//     settings.m_Tracking = 0;

//     TextMetrics metrics = {0};
//     TextShapeInfo info;

//     const uint32_t  max_num_lines = 16;
//     TextLine        lines[max_num_lines];

//     dmArray<uint32_t> codepoints;
//     TextToCodePoints(g_TextLorem, codepoints);

//     TestLayout(font, codepoints, &settings, &info, &metrics, lines, max_num_lines);

//     DebugPrintLayout(&info, &metrics, lines, scale);

//     FontDestroy(font);
//     return 0;
// }

int main(int argc, char **argv)
{
    dmLog::LogParams params;
    dmLog::LogInitialize(&params);

    // if (argc > 1 && (strstr(argv[1], ".ttf") != 0 ||
    //                  strstr(argv[1], ".otf") != 0))
    // {
    //     const char* path = argv[1];
    //     const char* text = "abcABC123åäö!\"";
    //     float size = 1.0f;
    //     float padding = 3.0f;

    //     if (argc > 2)
    //     {
    //         text = argv[2];
    //     }

    //     if (argc > 3)
    //     {
    //         int nresult = sscanf(argv[3], "%f", &size);
    //         if (nresult != 1)
    //         {
    //             dmLogError("Failed to parse size: '%s'", argv[3]);
    //             return 1;
    //         }
    //     }

    //     if (argc > 4)
    //     {
    //         int nresult = sscanf(argv[4], "%f", &padding);
    //         if (nresult != 1)
    //         {
    //             dmLogError("Failed to parse padding: '%s'", argv[4]);
    //             return 1;
    //         }
    //     }
    //     int ret = TestStandalone(path, size, padding, text);
    //     dmLog::LogFinalize();
    //     return ret;
    // }

    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();

    dmLog::LogFinalize();
    return ret;
}
