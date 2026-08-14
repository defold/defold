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
#include <math.h>
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
#include "font_outline.h"
#include "font_sdf.h"
#include "glyph_gen.h"
#include "glyph_vertex.h"
#include "text_layout.h"

//static const char* g_TextLorem = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Ut tempus quam in lacinia imperdiet. Vestibulum interdum erat quis purus lacinia, at ullamcorper arcu sagittis. Etiam molestie varius lacus, eget fringilla enim tempor quis. In at mollis dolor, et dictum sem. Mauris condimentum metus sed auctor tempus.";

#if defined(FONT_USE_SKRIBIDI)
#include <SheenBidi/SBScript.h>
static const char* g_TextArabic = "دينيس ريتشي فاش كان خدام ف مختبرات بيل، مابين 1972 و 1973";
#endif

class FontTest : public jc_test_base_class
{
protected:
    HFont           m_Font;
    HFontCollection m_FontCollection;

    virtual void SetUp() override
    {
        LoadFont("src/test/data/vera_mo_bd.ttf", &m_Font);

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
        ASSERT_STREQ(host_path, font_path);

        uint32_t path_hash = dmHashString32(host_path);
        ASSERT_EQ(path_hash, FontGetPathHash(font));

        *out = font;
    }
};

TEST_F(FontTest, LoadTTF)
{
    ASSERT_EQ(FONT_TYPE_TTF, FontGetType(m_Font));
}

TEST_F(FontTest, LoadOTFAndGenerateGlyph)
{
    HFont font;
    LoadFont("src/test/data/SourceCodePro-Regular.otf", &font);
    ASSERT_EQ(FONT_TYPE_OTF, FontGetType(font));

    FontGlyphGenParams params;
    params.m_Scale = FontGetScaleFromSize(font, 32.0f);
    params.m_SdfPadding = 6.0f;

    FontGlyph glyph;
    uint32_t glyph_index = FontGetGlyphIndex(font, 'A');
    ASSERT_NE(0u, glyph_index);
    ASSERT_EQ(FONT_RESULT_OK, FontGenerateGlyph(font, glyph_index, &params, &glyph));
    ASSERT_NE((uint8_t*)0, glyph.m_Bitmap.m_Data);
    ASSERT_GT(glyph.m_Bitmap.m_Width, 0u);
    ASSERT_GT(glyph.m_Bitmap.m_Height, 0u);
    ASSERT_GT(glyph.m_Advance, 0.0f);
    FontFreeGlyph(font, &glyph);
    FontDestroy(font);
}

TEST_F(FontTest, WorkSansOverlappingKGlyphHasNoBuriedEdges)
{
    // Work Sans keeps overlapping contours in K. Those overlaps are valid
    // TrueType outlines, but their buried edges must not contribute to the
    // distance field or they create dark seams through the filled glyph.
    HFont font;
    LoadFont("src/test/data/WorkSans.ttf", &font);

    FontGlyphGenParams params;
    params.m_Scale = FontGetScaleFromSize(font, 64.0f);
    params.m_SdfPadding = 8.0f;
    params.m_SdfEdgeValue = 190;

    FontGlyph glyph;
    uint32_t glyph_index = FontGetGlyphIndex(font, 'K');
    ASSERT_NE(0u, glyph_index);
    ASSERT_EQ(FONT_RESULT_OK, FontGenerateGlyph(font, glyph_index, &params, &glyph));
    ASSERT_EQ(51u, glyph.m_Bitmap.m_Width);
    ASSERT_EQ(59u, glyph.m_Bitmap.m_Height);

    // These pixels lie inside the two overlapping joins. Measuring distance
    // to a buried edge makes the first value too dark, while omitting the
    // exposed intersection point makes it saturate. Both are regressions.
    ASSERT_GE(glyph.m_Bitmap.m_Data[26 * glyph.m_Bitmap.m_Width + 24], 205u);
    ASSERT_LE(glyph.m_Bitmap.m_Data[26 * glyph.m_Bitmap.m_Width + 24], 215u);
    ASSERT_GE(glyph.m_Bitmap.m_Data[37 * glyph.m_Bitmap.m_Width + 13], 197u);
    ASSERT_LE(glyph.m_Bitmap.m_Data[37 * glyph.m_Bitmap.m_Width + 13], 207u);
    ASSERT_GE(glyph.m_Bitmap.m_Data[35 * glyph.m_Bitmap.m_Width + 11], 250u);

    FontFreeGlyph(font, &glyph);
    FontDestroy(font);
}

TEST(FontSDF, Rectangle)
{
    FontOutlineCommand commands[5] = {};
    commands[0].m_Type = FONT_OUTLINE_MOVE_TO;
    commands[0].m_Points[0] = { 0.0f, 0.0f };
    commands[1].m_Type = FONT_OUTLINE_LINE_TO;
    commands[1].m_Points[0] = { 8.0f, 0.0f };
    commands[2].m_Type = FONT_OUTLINE_LINE_TO;
    commands[2].m_Points[0] = { 8.0f, 8.0f };
    commands[3].m_Type = FONT_OUTLINE_LINE_TO;
    commands[3].m_Points[0] = { 0.0f, 8.0f };
    commands[4].m_Type = FONT_OUTLINE_CLOSE;
    FontOutline outline = { commands, 5 };

    FontSDFParams params = { 1.0f, 4, 128 };
    FontGlyphBitmap bitmap;
    int32_t offset_x;
    int32_t offset_y;
    ASSERT_EQ(FONT_RESULT_OK, FontSDFGenerate(&outline, &params, &bitmap, &offset_x, &offset_y));
    ASSERT_EQ(16u, bitmap.m_Width);
    ASSERT_EQ(16u, bitmap.m_Height);
    ASSERT_EQ(-4, offset_x);
    ASSERT_EQ(-12, offset_y);
    ASSERT_GT(bitmap.m_Data[8 * bitmap.m_Width + 8], 128u);
    ASSERT_LT(bitmap.m_Data[0], 128u);
    FontSDFFree(&bitmap);
}

TEST(FontSDF, PreservesSubpixelEdgeDistance)
{
    FontOutlineCommand commands[5] = {};
    commands[0].m_Type = FONT_OUTLINE_MOVE_TO;
    commands[0].m_Points[0] = { 0.25f, 0.0f };
    commands[1].m_Type = FONT_OUTLINE_LINE_TO;
    commands[1].m_Points[0] = { 4.25f, 0.0f };
    commands[2].m_Type = FONT_OUTLINE_LINE_TO;
    commands[2].m_Points[0] = { 4.25f, 4.0f };
    commands[3].m_Type = FONT_OUTLINE_LINE_TO;
    commands[3].m_Points[0] = { 0.25f, 4.0f };
    commands[4].m_Type = FONT_OUTLINE_CLOSE;
    FontOutline outline = { commands, 5 };

    FontSDFParams params = { 1.0f, 4, 128 };
    FontGlyphBitmap bitmap;
    int32_t offset_x;
    int32_t offset_y;
    ASSERT_EQ(FONT_RESULT_OK, FontSDFGenerate(&outline, &params, &bitmap, &offset_x, &offset_y));
    ASSERT_EQ(-4, offset_x);
    ASSERT_EQ(-8, offset_y);
    ASSERT_EQ(104u, bitmap.m_Data[6 * bitmap.m_Width + 3]);
    ASSERT_EQ(136u, bitmap.m_Data[6 * bitmap.m_Width + 4]);
    ASSERT_EQ(112u, bitmap.m_Data[8 * bitmap.m_Width + 6]);
    FontSDFFree(&bitmap);
}

TEST(FontSDF, OverlappingContoursMatchBooleanUnion)
{
    // These same-winding rectangles overlap from x=4 through x=8. The two
    // contours must produce the same field as their rectangular union; their
    // buried vertical edges are not boundaries of the non-zero fill.
    FontOutlineCommand overlapping_commands[10] = {};
    overlapping_commands[0].m_Type = FONT_OUTLINE_MOVE_TO;
    overlapping_commands[0].m_Points[0] = { 0.0f, 0.0f };
    overlapping_commands[1].m_Type = FONT_OUTLINE_LINE_TO;
    overlapping_commands[1].m_Points[0] = { 8.0f, 0.0f };
    overlapping_commands[2].m_Type = FONT_OUTLINE_LINE_TO;
    overlapping_commands[2].m_Points[0] = { 8.0f, 8.0f };
    overlapping_commands[3].m_Type = FONT_OUTLINE_LINE_TO;
    overlapping_commands[3].m_Points[0] = { 0.0f, 8.0f };
    overlapping_commands[4].m_Type = FONT_OUTLINE_CLOSE;
    overlapping_commands[5].m_Type = FONT_OUTLINE_MOVE_TO;
    overlapping_commands[5].m_Points[0] = { 4.0f, 0.0f };
    overlapping_commands[6].m_Type = FONT_OUTLINE_LINE_TO;
    overlapping_commands[6].m_Points[0] = { 12.0f, 0.0f };
    overlapping_commands[7].m_Type = FONT_OUTLINE_LINE_TO;
    overlapping_commands[7].m_Points[0] = { 12.0f, 8.0f };
    overlapping_commands[8].m_Type = FONT_OUTLINE_LINE_TO;
    overlapping_commands[8].m_Points[0] = { 4.0f, 8.0f };
    overlapping_commands[9].m_Type = FONT_OUTLINE_CLOSE;
    FontOutline overlapping = { overlapping_commands, 10 };

    FontOutlineCommand union_commands[5] = {};
    union_commands[0].m_Type = FONT_OUTLINE_MOVE_TO;
    union_commands[0].m_Points[0] = { 0.0f, 0.0f };
    union_commands[1].m_Type = FONT_OUTLINE_LINE_TO;
    union_commands[1].m_Points[0] = { 12.0f, 0.0f };
    union_commands[2].m_Type = FONT_OUTLINE_LINE_TO;
    union_commands[2].m_Points[0] = { 12.0f, 8.0f };
    union_commands[3].m_Type = FONT_OUTLINE_LINE_TO;
    union_commands[3].m_Points[0] = { 0.0f, 8.0f };
    union_commands[4].m_Type = FONT_OUTLINE_CLOSE;
    FontOutline expected_union = { union_commands, 5 };

    FontSDFParams params = { 1.0f, 4, 128 };
    FontGlyphBitmap overlapping_bitmap;
    FontGlyphBitmap union_bitmap;
    int32_t overlapping_x;
    int32_t overlapping_y;
    int32_t union_x;
    int32_t union_y;
    ASSERT_EQ(FONT_RESULT_OK, FontSDFGenerate(&overlapping, &params, &overlapping_bitmap, &overlapping_x, &overlapping_y));
    ASSERT_EQ(FONT_RESULT_OK, FontSDFGenerate(&expected_union, &params, &union_bitmap, &union_x, &union_y));
    ASSERT_EQ(union_x, overlapping_x);
    ASSERT_EQ(union_y, overlapping_y);
    ASSERT_EQ(union_bitmap.m_Width, overlapping_bitmap.m_Width);
    ASSERT_EQ(union_bitmap.m_Height, overlapping_bitmap.m_Height);
    ASSERT_EQ(union_bitmap.m_DataSize, overlapping_bitmap.m_DataSize);
    ASSERT_EQ(0, memcmp(union_bitmap.m_Data, overlapping_bitmap.m_Data, union_bitmap.m_DataSize));
    FontSDFFree(&union_bitmap);
    FontSDFFree(&overlapping_bitmap);
}

TEST(FontSDF, OppositeWindingContourRemainsHole)
{
    FontOutlineCommand commands[10] = {};
    commands[0].m_Type = FONT_OUTLINE_MOVE_TO;
    commands[0].m_Points[0] = { 0.0f, 0.0f };
    commands[1].m_Type = FONT_OUTLINE_LINE_TO;
    commands[1].m_Points[0] = { 12.0f, 0.0f };
    commands[2].m_Type = FONT_OUTLINE_LINE_TO;
    commands[2].m_Points[0] = { 12.0f, 12.0f };
    commands[3].m_Type = FONT_OUTLINE_LINE_TO;
    commands[3].m_Points[0] = { 0.0f, 12.0f };
    commands[4].m_Type = FONT_OUTLINE_CLOSE;
    commands[5].m_Type = FONT_OUTLINE_MOVE_TO;
    commands[5].m_Points[0] = { 4.0f, 4.0f };
    commands[6].m_Type = FONT_OUTLINE_LINE_TO;
    commands[6].m_Points[0] = { 4.0f, 8.0f };
    commands[7].m_Type = FONT_OUTLINE_LINE_TO;
    commands[7].m_Points[0] = { 8.0f, 8.0f };
    commands[8].m_Type = FONT_OUTLINE_LINE_TO;
    commands[8].m_Points[0] = { 8.0f, 4.0f };
    commands[9].m_Type = FONT_OUTLINE_CLOSE;
    FontOutline outline = { commands, 10 };

    FontSDFParams params = { 1.0f, 4, 128 };
    FontGlyphBitmap bitmap;
    int32_t offset_x;
    int32_t offset_y;
    ASSERT_EQ(FONT_RESULT_OK, FontSDFGenerate(&outline, &params, &bitmap, &offset_x, &offset_y));
    ASSERT_LT(bitmap.m_Data[9 * bitmap.m_Width + 9], 128u);
    ASSERT_GT(bitmap.m_Data[9 * bitmap.m_Width + 5], 128u);
    FontSDFFree(&bitmap);
}

TEST(FontOutline, BezierBounds)
{
    FontOutlineCommand commands[2] = {};
    commands[0].m_Type = FONT_OUTLINE_MOVE_TO;
    commands[1].m_Type = FONT_OUTLINE_CUBIC_TO;
    commands[1].m_Points[0] = { 0.0f, 100.0f };
    commands[1].m_Points[1] = { 100.0f, 100.0f };
    commands[1].m_Points[2] = { 100.0f, 0.0f };
    FontOutline outline = { commands, 2 };
    float x0, y0, x1, y1;
    ASSERT_TRUE(FontGetOutlineBounds(&outline, &x0, &y0, &x1, &y1));
    ASSERT_EQ(0.0f, x0);
    ASSERT_EQ(0.0f, y0);
    ASSERT_EQ(100.0f, x1);
    ASSERT_EQ(75.0f, y1);
}

TEST(FontOutline, MakeYMonotonic)
{
    FontOutline outline = {};
    outline.m_CommandCount = 3;
    outline.m_Commands = (FontOutlineCommand*)calloc(outline.m_CommandCount, sizeof(FontOutlineCommand));
    outline.m_Commands[0].m_Type = FONT_OUTLINE_MOVE_TO;
    outline.m_Commands[0].m_Points[0] = { 0.0f, 0.0f };
    outline.m_Commands[1].m_Type = FONT_OUTLINE_QUADRATIC_TO;
    outline.m_Commands[1].m_Points[0] = { 5.0f, 10.0f };
    outline.m_Commands[1].m_Points[1] = { 10.0f, 0.0f };
    outline.m_Commands[2].m_Type = FONT_OUTLINE_CUBIC_TO;
    outline.m_Commands[2].m_Points[0] = { 12.0f, 10.0f };
    outline.m_Commands[2].m_Points[1] = { 18.0f, -10.0f };
    outline.m_Commands[2].m_Points[2] = { 20.0f, 0.0f };

    ASSERT_EQ(FONT_RESULT_OK, FontOutlineMakeYMonotonic(&outline));
    ASSERT_EQ(6u, outline.m_CommandCount);
    ASSERT_EQ(FONT_OUTLINE_MOVE_TO, outline.m_Commands[0].m_Type);
    ASSERT_EQ(FONT_OUTLINE_QUADRATIC_TO, outline.m_Commands[1].m_Type);
    ASSERT_EQ(FONT_OUTLINE_QUADRATIC_TO, outline.m_Commands[2].m_Type);
    ASSERT_EQ(5.0f, outline.m_Commands[1].m_Points[1].m_X);
    ASSERT_EQ(5.0f, outline.m_Commands[1].m_Points[1].m_Y);
    ASSERT_EQ(FONT_OUTLINE_CUBIC_TO, outline.m_Commands[3].m_Type);
    ASSERT_EQ(FONT_OUTLINE_CUBIC_TO, outline.m_Commands[4].m_Type);
    ASSERT_EQ(FONT_OUTLINE_CUBIC_TO, outline.m_Commands[5].m_Type);

    ASSERT_EQ(FONT_RESULT_OK, FontOutlineMakeYMonotonic(&outline));
    ASSERT_EQ(6u, outline.m_CommandCount);

    FontFreeGlyphOutline(&outline);
}

TEST_F(FontTest, GenerateSdfGlyphWithShadowChannels)
{
    FontGlyphGenParams params;
    params.m_Scale = FontGetScaleFromSize(m_Font, 32.0f);
    params.m_SdfPadding = 6.0f;
    params.m_OutlineWidth = 1.0f;
    params.m_ShadowBlur = 2.0f;

    FontGlyph glyph;
    uint32_t glyph_index = FontGetGlyphIndex(m_Font, 'A');
    ASSERT_EQ(FONT_RESULT_OK, FontGenerateGlyph(m_Font, glyph_index, &params, &glyph));
    ASSERT_EQ(3u, glyph.m_Bitmap.m_Channels);
    ASSERT_EQ((uint32_t)(glyph.m_Bitmap.m_Width * glyph.m_Bitmap.m_Height * 3), glyph.m_Bitmap.m_DataSize);
    ASSERT_NE((uint8_t*)0, glyph.m_Bitmap.m_Data);
    FontFreeGlyph(m_Font, &glyph);
}

TEST_F(FontTest, GenerateBitmapGlyphWithBlurredOutlineShadow)
{
    FontGlyphGenParams params;
    params.m_Scale = FontGetScaleFromSize(m_Font, 32.0f);
    params.m_SdfPadding = 8.0f;
    params.m_OutlineWidth = 2.0f;
    params.m_OutputBitmap = true;
    params.m_HasOutline = true;
    params.m_HasShadow = true;

    const uint32_t glyph_index = FontGetGlyphIndex(m_Font, 'A');
    FontGlyph source_glyph;
    ASSERT_EQ(FONT_RESULT_OK, FontGenerateGlyph(m_Font, glyph_index, &params, &source_glyph));

    params.m_ShadowBlur = 2.0f;
    FontGlyph blurred_glyph;
    ASSERT_EQ(FONT_RESULT_OK, FontGenerateGlyph(m_Font, glyph_index, &params, &blurred_glyph));
    ASSERT_EQ(source_glyph.m_Bitmap.m_Width, blurred_glyph.m_Bitmap.m_Width);
    ASSERT_EQ(source_glyph.m_Bitmap.m_Height, blurred_glyph.m_Bitmap.m_Height);
    ASSERT_EQ(3u, source_glyph.m_Bitmap.m_Channels);
    ASSERT_EQ(3u, blurred_glyph.m_Bitmap.m_Channels);

    const uint32_t width = source_glyph.m_Bitmap.m_Width;
    const uint32_t height = source_glyph.m_Bitmap.m_Height;
    const uint32_t pixel_count = width * height;
    dmArray<uint8_t> expected;
    dmArray<uint8_t> target;
    expected.SetCapacity(pixel_count);
    expected.SetSize(pixel_count);
    target.SetCapacity(pixel_count);
    target.SetSize(pixel_count);

    for (uint32_t i = 0; i < pixel_count; ++i)
    {
        const uint32_t offset = i * 3;
        // Before blur, the shadow source is the complete face-plus-outline silhouette.
        ASSERT_EQ(source_glyph.m_Bitmap.m_Data[offset + 1], source_glyph.m_Bitmap.m_Data[offset + 2]);
        expected[i] = source_glyph.m_Bitmap.m_Data[offset + 2];
    }

    for (uint32_t pass = 0; pass < 2; ++pass)
    {
        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                const uint32_t offset = y * width + x;
                if (x == 0 || y == 0 || x + 1 == width || y + 1 == height)
                {
                    target[offset] = expected[offset];
                    continue;
                }
                const uint32_t sum =
                    expected[offset - width - 1] + 2 * expected[offset - width] + expected[offset - width + 1] +
                    2 * expected[offset - 1] + 4 * expected[offset] + 2 * expected[offset + 1] +
                    expected[offset + width - 1] + 2 * expected[offset + width] + expected[offset + width + 1];
                target[offset] = (uint8_t)(sum / 16);
            }
        }
        memcpy(expected.Begin(), target.Begin(), pixel_count);
    }

    uint32_t graduated_shadow_pixels = 0;
    for (uint32_t i = 0; i < pixel_count; ++i)
    {
        const uint32_t offset = i * 3;
        ASSERT_EQ(source_glyph.m_Bitmap.m_Data[offset + 0], blurred_glyph.m_Bitmap.m_Data[offset + 0]);
        ASSERT_EQ(source_glyph.m_Bitmap.m_Data[offset + 1], blurred_glyph.m_Bitmap.m_Data[offset + 1]);
        ASSERT_EQ(expected[i], blurred_glyph.m_Bitmap.m_Data[offset + 2]);
        if (expected[i] > 0 && expected[i] < 255)
            ++graduated_shadow_pixels;
    }
    ASSERT_GT(graduated_shadow_pixels, 0u);

    FontFreeGlyph(m_Font, &source_glyph);
    FontFreeGlyph(m_Font, &blurred_glyph);
}

TEST_F(FontTest, GlyphChannelCountMatchesOutputMode)
{
    ASSERT_EQ(1u, FontGetGlyphChannelCount(false, false, false, 0.0f));
    ASSERT_EQ(1u, FontGetGlyphChannelCount(false, true, true, 0.0f));
    ASSERT_EQ(3u, FontGetGlyphChannelCount(false, false, false, 1.0f));
    ASSERT_EQ(1u, FontGetGlyphChannelCount(true, false, false, 1.0f));
    ASSERT_EQ(3u, FontGetGlyphChannelCount(true, true, false, 0.0f));
    ASSERT_EQ(3u, FontGetGlyphChannelCount(true, false, true, 0.0f));
}

TEST_F(FontTest, PackLayeredGlyphVertices)
{
    ASSERT_EQ(96u, sizeof(FontGlyphVertex));

    FontGlyphGenParams params;
    params.m_Scale = FontGetScaleFromSize(m_Font, 32.0f);
    FontGlyph glyph;
    uint32_t glyph_index = FontGetGlyphIndex(m_Font, 'A');
    ASSERT_EQ(FONT_RESULT_OK, FontGenerateGlyph(m_Font, glyph_index, &params, &glyph));

    FontGlyphVertex vertices[18];
    memset(vertices, 0, sizeof(vertices));
    dmVMath::Matrix4 transform = dmVMath::Matrix4::identity();
    dmVMath::Vector4 white(1.0f, 1.0f, 1.0f, 1.0f);
    dmVMath::Vector4 black(0.0f, 0.0f, 0.0f, 1.0f);
    FontPackGlyphVertices(&glyph, 1.0f / 256.0f, 1.0f / 256.0f,
                          0, 0, (uint32_t)glyph.m_Ascent, 1,
                          3, FONT_RENDER_LAYER_FACE | FONT_RENDER_LAYER_OUTLINE | FONT_RENDER_LAYER_SHADOW,
                          0, 6, transform, 0.0f, 0.0f,
                          white, black, black, 0.75f, 0.5f, 0.1f, 0.25f,
                          2.0f, -2.0f, true, vertices);

    ASSERT_EQ(1.0f, vertices[12].m_LayerMasks[0]);
    ASSERT_EQ(1.0f, vertices[6].m_LayerMasks[1]);
    ASSERT_EQ(1.0f, vertices[0].m_LayerMasks[2]);
    ASSERT_NE(vertices[12].m_Position[0], vertices[0].m_Position[0]);
    FontFreeGlyph(m_Font, &glyph);
}

static TextResult TestLayout(HFontCollection coll, dmArray<uint32_t>& codepoints, TextLayoutSettings* settings, HTextLayout* layout)
{
    uint64_t   tstart = dmTime::GetMonotonicTime();

    uint32_t*  pc = codepoints.Begin();
    uint32_t   num_codepoints = codepoints.Size();
    TextResult r = TextLayoutCreate(coll, pc, num_codepoints, settings, layout);

    uint64_t   tend = dmTime::GetMonotonicTime();
    if (*layout)
    {
        printf("Layout %u codepoints into %u glyphs took %.3f ms\n", codepoints.Size(), (*layout)->m_Glyphs.Size(), (tend - tstart) / 1000.0f);
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

    TextLayoutRelease(layout);

    // Test the same without any lines
    r = TestLayout(m_FontCollection, codepoints, &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, r);
    ASSERT_NE((TextLayout*)0, layout);
    DebugPrintLayout(layout);
    ASSERT_EQ(1u, layout->m_Lines.Size());

    TextLayoutRelease(layout);
}

// See https://github.com/defold/defold/issues/11766
TEST_F(FontTest, LayoutSingleLineWithUnknownCharacterLast)
{
    HFont font;
    LoadFont("src/test/data/vera_mo_bd_atoz.ttf", &font);

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
    TextLayoutRelease(layout);
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
    TextLayoutRelease(layout);
}

TEST_F(FontTest, LayoutAcquireRelease)
{
    // Prepared text layouts are now shared across font/render call sites, so
    // the basic retain/release contract needs explicit coverage here.
    TextLayoutSettings settings = {0};
    settings.m_LineBreak = false;
    settings.m_Width = 0.0f;
    settings.m_Size = 16.0f;

    HTextLayout layout = 0;
    TextResult r = TextLayoutCreate(m_FontCollection, 0, 0, &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, r);
    ASSERT_NE((HTextLayout)0, layout);
    ASSERT_EQ(1u, layout->m_RefCount);

    TextLayoutAcquire(layout);
    ASSERT_EQ(2u, layout->m_RefCount);

    TextLayoutRelease(layout);
    ASSERT_EQ(1u, layout->m_RefCount);

    TextLayoutRelease(layout);
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

    TextLayoutRelease(layout);
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

    TextLayoutRelease(layout);
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

    ASSERT_EQ(3u, TextLayoutGetParagraphCount(layout));
    TextParagraph* paragraphs = TextLayoutGetParagraphs(layout);
    ASSERT_EQ(0u, paragraphs[0].m_TextIndex);
    ASSERT_EQ(3u, paragraphs[0].m_TextLength);
    ASSERT_EQ(0u, paragraphs[0].m_LineIndex);
    ASSERT_EQ(1u, paragraphs[0].m_LineCount);
    ASSERT_EQ(4u, paragraphs[1].m_TextIndex);
    ASSERT_EQ(0u, paragraphs[1].m_TextLength);
    ASSERT_EQ(1u, paragraphs[1].m_LineIndex);
    ASSERT_EQ(1u, paragraphs[1].m_LineCount);
    ASSERT_EQ(5u, paragraphs[2].m_TextIndex);
    ASSERT_EQ(3u, paragraphs[2].m_TextLength);
    ASSERT_EQ(2u, paragraphs[2].m_LineIndex);
    ASSERT_EQ(1u, paragraphs[2].m_LineCount);
    ASSERT_EQ(0u, line1.m_ParagraphIndex);
    ASSERT_EQ(1u, line2.m_ParagraphIndex);
    ASSERT_EQ(2u, line3.m_ParagraphIndex);

    TextLayoutRelease(layout);
}

TEST_F(FontTest, LegacyLayoutMultilineHeightUsesFontDescent)
{
    dmArray<uint32_t> codepoints;
    TextToCodePoints("A\nA", codepoints);

    TextLayoutSettings settings = {0};
    settings.m_LineBreak = false;
    settings.m_Width = 0.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    settings.m_Tracking = 0.0f;

    HTextLayout layout = 0;
    TextResult r = TextLayoutLegacyCreate(m_FontCollection, codepoints.Begin(), codepoints.Size(), &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, r);
    ASSERT_NE((HTextLayout)0, layout);
    ASSERT_EQ(2u, layout->m_Lines.Size());

    float scale = FontGetScaleFromSize(m_Font, settings.m_Size);
    uint32_t ascent = (uint32_t)FontGetAscent(m_Font, 1.0f);
    uint32_t descent = (uint32_t)fabsf(FontGetDescent(m_Font, 1.0f));
    ASSERT_GT(descent, 0u);

    float expected_line_height = (ascent + descent) * scale;
    float expected_height = expected_line_height * layout->m_Lines.Size();
    ASSERT_NEAR(expected_height, layout->m_Height, 0.01f);

    TextLayoutRelease(layout);
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
    float layout_line_height = layout->m_Height;
    TextLayoutRelease(layout);

    // Measure tracking impact as a width delta between two adjacent glyphs.
    const char* tracking_text = "AA";
    TextToCodePoints(tracking_text, codepoints);
    r = TestLayout(m_FontCollection, codepoints, &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, r);
    ASSERT_NE((HTextLayout)0, layout);
    float width_no_tracking = layout->m_Width;
    TextLayoutRelease(layout);

    float tracking_value = 0.25f;
    settings.m_Tracking = tracking_value;
    r = TestLayout(m_FontCollection, codepoints, &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, r);
    ASSERT_NE((HTextLayout)0, layout);
    float width_tracking = layout->m_Width;
    TextLayoutRelease(layout);

    // Legacy tracking scales by line height. Skribidi scales by font size in pixels.
    float expected_tracking = 0.0f;
#if defined(FONT_USE_SKRIBIDI)
    // Skribidi interprets tracking in pixels based on font size.
    expected_tracking = tracking_value * settings.m_Size;
#else
    float scale = FontGetScaleFromSize(m_Font, settings.m_Size);
    uint32_t ascent = (uint32_t)FontGetAscent(m_Font, 1.0f);
    uint32_t descent = (uint32_t)fabsf(FontGetDescent(m_Font, 1.0f));
    float tracking_line_height = (ascent + descent) * scale;
    expected_tracking = tracking_value * tracking_line_height;
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
    TextLayoutRelease(layout);

    settings.m_Leading = 2.0f;
    r = TestLayout(m_FontCollection, codepoints, &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, r);
    ASSERT_NE((HTextLayout)0, layout);
    ASSERT_EQ(2u, layout->m_Lines.Size());
    float height_leading_2 = layout->m_Height;
    TextLayoutRelease(layout);

    // Leading should add one extra line height for the entire layout.
    float expected_leading_delta = layout_line_height;
    float height_leading_delta = height_leading_2 - height_leading_1;
    ASSERT_NEAR(expected_leading_delta, height_leading_delta, epsilon);
}

TEST_F(FontTest, FontTracking)
{
    const char text[] = "test with many characters";
    const float spacing_epsilon = 0.01f;
    const float width_epsilon = 0.02f;

    dmArray<uint32_t> codepoints;
    TextToCodePoints(text, codepoints);
    ASSERT_GT(codepoints.Size(), 1u);

    TextLayoutSettings settings = {0};
    settings.m_LineBreak = false;
    settings.m_Width = 0.0f;
    settings.m_Size = 28.0f;
    settings.m_Leading = 1.0f;
    settings.m_Tracking = 0.0f;

    HTextLayout layout = 0;

    // Match the legacy tracking unit:
    // tracking_px = tracking * ((uint32_t)ascent + (uint32_t)abs(descent)) * scale
    float scale = FontGetScaleFromSize(m_Font, settings.m_Size);
    uint32_t ascent = (uint32_t)FontGetAscent(m_Font, 1.0f);
    uint32_t descent = (uint32_t)fabsf(FontGetDescent(m_Font, 1.0f));
    const float line_height = (ascent + descent) * scale;
    ASSERT_GT(line_height, 0.0f);

    const uint32_t max_glyphs = 256;
    ASSERT_LE(codepoints.Size(), max_glyphs);
    const uint32_t tracking_steps = 11; // -0.05 .. 0.05 in 0.01 increments

    float baseline_spacing[max_glyphs];
    float baseline_prev_advance[max_glyphs];
#if !defined(FONT_USE_SKRIBIDI)
    uint32_t baseline_tracking_pair_count = 0;
#endif
    for (uint32_t i = 0; i < max_glyphs; ++i)
    {
        baseline_spacing[i] = 0.0f;
        baseline_prev_advance[i] = 0.0f;
    }

    float full_width_at_zero = 0.0f;
    uint32_t baseline_glyph_count = 0;

    // Baseline pass: capture spacing between consecutive glyph pen positions at tracking=0.
    {
        TextResult r = TestLayout(m_FontCollection, codepoints, &settings, &layout);
        ASSERT_EQ(TEXT_RESULT_OK, r);
        ASSERT_NE((HTextLayout)0, layout);
        ASSERT_GT(layout->m_Glyphs.Size(), 1u);

        baseline_glyph_count = layout->m_Glyphs.Size();
        full_width_at_zero = layout->m_Width;

        for (uint32_t i = 0; i < baseline_glyph_count; ++i)
        {
            TextGlyph& glyph = layout->m_Glyphs[i];

            if (i > 0)
            {
                TextGlyph& prev_glyph = layout->m_Glyphs[i - 1];
                baseline_spacing[i] = glyph.m_X - prev_glyph.m_X;
                baseline_prev_advance[i] = prev_glyph.m_Advance;
#if !defined(FONT_USE_SKRIBIDI)
                if (baseline_prev_advance[i] > 0.0f)
                    ++baseline_tracking_pair_count;
#endif
            }
        }

        TextLayoutRelease(layout);
        layout = 0;
    }

    // Sweep tracking values and verify adjacent glyph spacing and total width progression.
    for (uint32_t step = 0; step < tracking_steps; ++step)
    {
        if (step == 5) // tracking == 0.0f
            continue;

        const float tracking = -0.05f + step * 0.01f;
        float tracking_pixels = 0.0f;
#if defined(FONT_USE_SKRIBIDI)
        tracking_pixels = tracking * settings.m_Size;
#else
        tracking_pixels = tracking * line_height;
#endif
        settings.m_Tracking = tracking;

        TextResult r = TestLayout(m_FontCollection, codepoints, &settings, &layout);
        ASSERT_EQ(TEXT_RESULT_OK, r);
        ASSERT_NE((HTextLayout)0, layout);
        ASSERT_EQ(baseline_glyph_count, layout->m_Glyphs.Size());

        for (uint32_t i = 1; i < layout->m_Glyphs.Size(); ++i)
        {
            TextGlyph& prev_glyph = layout->m_Glyphs[i - 1];
            TextGlyph& glyph = layout->m_Glyphs[i];

            float actual_spacing = glyph.m_X - prev_glyph.m_X;
            bool has_prev_advance = true;
#if !defined(FONT_USE_SKRIBIDI)
            has_prev_advance = baseline_prev_advance[i] > 0.0f;
#endif
            float expected_spacing = has_prev_advance ? (baseline_spacing[i] + tracking_pixels) : baseline_spacing[i];
            ASSERT_NEAR(expected_spacing, actual_spacing, spacing_epsilon);
        }

        float expected_width = 0.0f;
#if defined(FONT_USE_SKRIBIDI)
        // Skribidi spacing applies to all glyphs in the run; for positive tracking
        // the layout code compensates one slot in the final width.
        expected_width = full_width_at_zero + baseline_glyph_count * tracking_pixels;
        if (tracking > 0.0f)
            expected_width -= tracking_pixels;
#else
        expected_width = full_width_at_zero + baseline_tracking_pair_count * tracking_pixels;
#endif
        ASSERT_NEAR(expected_width, layout->m_Width, width_epsilon);

        TextLayoutRelease(layout);
        layout = 0;
    }
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

TEST_F(FontTest, LegacyLayoutResolvesRenderOnlyMarkup)
{
    const char source[] =
    "A<color=#00FF00>B</color>"
    "<gradient fit=glyph left=#FF0000 right=#0000FF>CD</gradient>"
    "<outline size=2 color=#FFFF00>E</outline>"
    "<shake hz=12 amplitude=0.8>F</shake>"
    "<shadow x=3 y=-2 blur=2 color=#204080A0>G</shadow>H";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));

    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutLegacyCreateMarkup(m_FontCollection, markup, &settings, &layout));

    TextLayout* internal = (TextLayout*)layout;
    TextGlyph*  glyphs = TextLayoutGetGlyphs(layout);
    ASSERT_EQ(8u, TextLayoutGetGlyphCount(layout));
    for (uint32_t i = 0; i < TextLayoutGetGlyphCount(layout); ++i)
    {
        ASSERT_EQ(i, glyphs[i].m_Cluster);
        ASSERT_NE(MARKUP_INVALID_INDEX, glyphs[i].m_MarkupSpanIndex);
    }

    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyphRenderData render_data;
    TextLayoutGetGlyphRenderData(layout, glyphs[1], white, &render_data);
    ASSERT_EQ(0.0f, render_data.m_FaceColors.m_BottomLeft[0]);
    ASSERT_EQ(1.0f, render_data.m_FaceColors.m_BottomLeft[1]);
    ASSERT_EQ(0.0f, render_data.m_FaceColors.m_BottomLeft[2]);

    TextLayoutGetGlyphRenderData(layout, glyphs[2], white, &render_data);
    ASSERT_EQ(0.75f, render_data.m_FaceColors.m_BottomLeft[0]);
    ASSERT_EQ(0.25f, render_data.m_FaceColors.m_BottomLeft[2]);
    ASSERT_EQ(render_data.m_FaceColors.m_BottomLeft[0], render_data.m_FaceColors.m_BottomRight[0]);
    ASSERT_EQ(render_data.m_FaceColors.m_BottomLeft[2], render_data.m_FaceColors.m_BottomRight[2]);
    TextLayoutGetGlyphRenderData(layout, glyphs[3], white, &render_data);
    ASSERT_EQ(0.25f, render_data.m_FaceColors.m_BottomLeft[0]);
    ASSERT_EQ(0.75f, render_data.m_FaceColors.m_BottomLeft[2]);
    ASSERT_EQ(render_data.m_FaceColors.m_BottomLeft[0], render_data.m_FaceColors.m_BottomRight[0]);
    ASSERT_EQ(render_data.m_FaceColors.m_BottomLeft[2], render_data.m_FaceColors.m_BottomRight[2]);

    ASSERT_TRUE(TextLayoutHasMarkupOutline(layout));
    ASSERT_EQ(2.0f, TextLayoutGetMaxMarkupOutlineWidth(layout));
    TextLayoutGetGlyphRenderData(layout, glyphs[4], white, &render_data);
    ASSERT_EQ(2.0f, render_data.m_OutlineWidth);
    ASSERT_EQ(1.0f, render_data.m_OutlineColor[0]);
    ASSERT_EQ(1.0f, render_data.m_OutlineColor[1]);
    ASSERT_EQ(0.0f, render_data.m_OutlineColor[2]);

    TextGlyphRenderData shake_before;
    TextLayoutGetGlyphRenderData(layout, glyphs[5], white, &shake_before);
    TextLayoutUpdate(layout, 0.03f);
    TextGlyphRenderData shake_after;
    TextLayoutGetGlyphRenderData(layout, glyphs[5], white, &shake_after);
    ASSERT_TRUE(fabsf(shake_before.m_OffsetX - shake_after.m_OffsetX) > 0.0001f ||
                fabsf(shake_before.m_OffsetY - shake_after.m_OffsetY) > 0.0001f);
    TextLayoutGetGlyphRenderData(layout, glyphs[6], white, &render_data);
    ASSERT_TRUE(TextLayoutHasMarkupShadow(layout));
    ASSERT_EQ(3.0f, render_data.m_ShadowX);
    ASSERT_EQ(-2.0f, render_data.m_ShadowY);
    ASSERT_EQ(2.0f, render_data.m_ShadowBlur);
    ASSERT_NEAR(32.0f / 255.0f, render_data.m_ShadowColor[0], 0.0001f);
    ASSERT_NEAR(160.0f / 255.0f, render_data.m_ShadowColor[3], 0.0001f);
    TextLayoutGetGlyphRenderData(layout, glyphs[7], white, &render_data);
    ASSERT_EQ(0.0f, render_data.m_OffsetX);
    ASSERT_EQ(0.0f, render_data.m_OffsetY);
    ASSERT_EQ(0u, render_data.m_StyleFlags & (TEXT_RENDER_STYLE_SHADOW_COLOR | TEXT_RENDER_STYLE_SHADOW_X | TEXT_RENDER_STYLE_SHADOW_Y | TEXT_RENDER_STYLE_SHADOW_BLUR));

    ASSERT_GT(internal->m_Styles.Size(), 0u);
    ASSERT_GT(internal->m_Effects.Size(), 0u);
    TextLayoutRelease(layout);

#if !defined(FONT_USE_SKRIBIDI)
    HTextLayout selected_layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &selected_layout));
    ASSERT_GT(((TextLayout*)selected_layout)->m_Effects.Size(), 0u);
    TextLayoutRelease(selected_layout);
#endif

    MarkupDestroy(markup);
}

TEST_F(FontTest, LegacyLayoutUsesMarkupFontSize)
{
    const char source[] = "<size=64>A</size>A";
    HMarkup    markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));

    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutLegacyCreateMarkup(m_FontCollection, markup, &settings, &layout));

    ASSERT_EQ(2u, TextLayoutGetGlyphCount(layout));
    TextLayout* internal = (TextLayout*)layout;
    TextGlyph*  glyphs = TextLayoutGetGlyphs(layout);
    ASSERT_NEAR(2.0f, glyphs[0].m_RenderScale, 0.0001f);
    ASSERT_EQ(1.0f, glyphs[1].m_RenderScale);
    ASSERT_NEAR(glyphs[1].m_Width * 2.0f, glyphs[0].m_Width, 0.0001f);
    ASSERT_NEAR(glyphs[1].m_Height * 2.0f, glyphs[0].m_Height, 0.0001f);
    ASSERT_TRUE((internal->m_Styles[glyphs[0].m_StyleIndex].m_Flags & TEXT_RENDER_STYLE_FONT_SIZE) != 0);

    dmArray<uint32_t> plain_codepoints;
    TextToCodePoints("AA", plain_codepoints);
    HTextLayout plain_layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutLegacyCreate(m_FontCollection, plain_codepoints.Begin(), plain_codepoints.Size(), &settings, &plain_layout));
    float markup_width;
    float markup_height;
    float plain_width;
    float plain_height;
    TextLayoutGetBounds(layout, &markup_width, &markup_height);
    TextLayoutGetBounds(plain_layout, &plain_width, &plain_height);
    ASSERT_GT(markup_width, plain_width);
    ASSERT_GT(markup_height, plain_height);

    TextLayoutRelease(plain_layout);
    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

struct LayoutObjectTestContext
{
    float    m_ProposedWidth[8];
    float    m_ProposedHeight[8];
    uint32_t m_ResolveCount;
    uint32_t m_ReleaseCount;
};

static uint8_t ResolveTestLayoutObject(void* context, const char*, const TextLayoutObjectAttribute*, float proposed_width, float proposed_height, TextLayoutObject* object)
{
    LayoutObjectTestContext* test_context = (LayoutObjectTestContext*)context;
    const uint32_t           index = test_context->m_ResolveCount++;
    test_context->m_ProposedWidth[index] = proposed_width;
    test_context->m_ProposedHeight[index] = proposed_height;
    object->m_Width = proposed_width;
    object->m_Height = proposed_height;
    object->m_Resource = index + 1;
    return 1;
}

static void ReleaseTestLayoutObject(void* context, const TextLayoutObject*)
{
    ++((LayoutObjectTestContext*)context)->m_ReleaseCount;
}

static void AssertLayoutObjectAttribute(HTextLayout layout, const TextLayoutObject& object, uint32_t index, const char* expected_name, const char* expected_value)
{
    const TextLayoutObjectAttribute& attribute = TextLayoutGetObjectAttributes(layout)[object.m_AttributeIndex + index];
    const char*                      source = TextLayoutGetObjectSource(layout);
    ASSERT_EQ(strlen(expected_name), attribute.m_NameLength);
    ASSERT_EQ(0, memcmp(expected_name, source + attribute.m_NameOffset, attribute.m_NameLength));
    ASSERT_EQ(strlen(expected_value), attribute.m_ValueLength);
    ASSERT_EQ(0, memcmp(expected_value, source + attribute.m_ValueOffset, attribute.m_ValueLength));
}

TEST_F(FontTest, LayoutResolvesAndOwnsMarkupObjects)
{
    const char source[] =
    "A<sprite src=/icon.png/>B<a href=https://defold.com>CD</a>"
    "<sprite atlas=/icons.atlas width=2em height=50%/>"
    "<sprite src=/badge.png width=48px height=12/>"
    "<image src=/not-an-object.png/>"
    "<link href=/old-tag>Not an object</link>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));

    LayoutObjectTestContext context = {};
    TextLayoutSettings      settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    settings.m_ResolveObject = ResolveTestLayoutObject;
    settings.m_ReleaseObject = ReleaseTestLayoutObject;
    settings.m_ObjectContext = &context;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
    MarkupDestroy(markup);

    ASSERT_EQ(3u, context.m_ResolveCount);
    ASSERT_EQ(32.0f, context.m_ProposedWidth[0]);
    ASSERT_EQ(32.0f, context.m_ProposedHeight[0]);
    ASSERT_EQ(64.0f, context.m_ProposedWidth[1]);
    ASSERT_EQ(16.0f, context.m_ProposedHeight[1]);
    ASSERT_EQ(48.0f, context.m_ProposedWidth[2]);
    ASSERT_EQ(12.0f, context.m_ProposedHeight[2]);
    ASSERT_EQ(4u, TextLayoutGetObjectCount(layout));

    const TextLayoutObject* objects = TextLayoutGetObjects(layout);
    ASSERT_EQ(TEXT_LAYOUT_OBJECT_SPRITE, objects[0].m_Type);
    ASSERT_EQ(1u, objects[0].m_TextOffset);
    ASSERT_EQ(1u, objects[0].m_TextLength);
    ASSERT_EQ((uintptr_t)1, objects[0].m_Resource);
    AssertLayoutObjectAttribute(layout, objects[0], 0, "src", "/icon.png");

    ASSERT_EQ(TEXT_LAYOUT_OBJECT_LINK, objects[1].m_Type);
    ASSERT_NE(0u, objects[1].m_Id);
    ASSERT_EQ(0u, objects[1].m_State);
    ASSERT_EQ(3u, objects[1].m_TextOffset);
    ASSERT_EQ(2u, objects[1].m_TextLength);
    AssertLayoutObjectAttribute(layout, objects[1], 0, "href", "https://defold.com");

    ASSERT_EQ(TEXT_LAYOUT_OBJECT_SPRITE, objects[2].m_Type);
    ASSERT_EQ(5u, objects[2].m_TextOffset);
    ASSERT_EQ(1u, objects[2].m_TextLength);
    ASSERT_EQ(64.0f, objects[2].m_Width);
    ASSERT_EQ(16.0f, objects[2].m_Height);
    ASSERT_EQ(TEXT_LAYOUT_OBJECT_SPRITE, objects[3].m_Type);
    ASSERT_EQ(48.0f, objects[3].m_Width);
    ASSERT_EQ(12.0f, objects[3].m_Height);
    ASSERT_STREQ(source, TextLayoutGetObjectSource(layout));

    TextGlyph* glyphs = TextLayoutGetGlyphs(layout);
    ASSERT_EQ(0xfffcu, glyphs[1].m_Codepoint);
    ASSERT_EQ((uint16_t)TEXT_GLYPH_FLAG_OBJECT, glyphs[1].m_Flags);
    ASSERT_EQ(32.0f, glyphs[1].m_Width);
    ASSERT_EQ(32.0f, glyphs[1].m_Height);
    ASSERT_TRUE(glyphs[2].m_X >= glyphs[1].m_X + objects[0].m_Width);

    TextLayoutRelease(layout);
    ASSERT_EQ(3u, context.m_ReleaseCount);
}

static TextRenderStyle MakeTestColorStyle(float red, float green, float blue)
{
    TextRenderStyle style = {};
    style.m_FaceColor[0] = red;
    style.m_FaceColor[1] = green;
    style.m_FaceColor[2] = blue;
    style.m_FaceColor[3] = 1.0f;
    style.m_Flags = TEXT_RENDER_STYLE_FACE_COLOR;
    return style;
}

static void AssertGlyphColor(HTextLayout layout, uint32_t glyph_index, float red, float green, float blue)
{
    const float         white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyphRenderData data;
    TextLayoutGetGlyphRenderData(layout, TextLayoutGetGlyphs(layout)[glyph_index], white, &data);
    ASSERT_NEAR(red, data.m_FaceColors.m_BottomLeft[0], 0.0001f);
    ASSERT_NEAR(green, data.m_FaceColors.m_BottomLeft[1], 0.0001f);
    ASSERT_NEAR(blue, data.m_FaceColors.m_BottomLeft[2], 0.0001f);
}

TEST_F(FontTest, LayoutRecoversInvalidEffectAndPreservesSurroundingStyles)
{
    const char source[] =
    "<color=#00FF00>A"
    "<gradient hz=0.25 fit=chars left=#FF5555 right=#5555FF>B<size=64>C</size></gradient>"
    "<color=#FF0000>D</color>E</color>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));

    ASSERT_EQ(5u, TextLayoutGetGlyphCount(layout));
    ASSERT_EQ(0u, ((TextLayout*)layout)->m_Effects.Size());
    AssertGlyphColor(layout, 0, 0.0f, 1.0f, 0.0f);
    AssertGlyphColor(layout, 1, 0.0f, 1.0f, 0.0f);
    AssertGlyphColor(layout, 2, 0.0f, 1.0f, 0.0f);
    AssertGlyphColor(layout, 3, 1.0f, 0.0f, 0.0f);
    AssertGlyphColor(layout, 4, 0.0f, 1.0f, 0.0f);
    ASSERT_EQ(1.0f, TextLayoutGetGlyphs(layout)[2].m_RenderScale);

    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, LayoutUsesHorizontalGradientAcrossTextByDefault)
{
    const char source[] = "<gradient left=#FF00FF right=#FFFFFF>Horizontal Gradient</gradient>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));

    TextLayout* internal = (TextLayout*)layout;
    ASSERT_EQ(1u, internal->m_Effects.Size());
    ASSERT_EQ((uint8_t)TEXT_EFFECT_FIT_TEXT, internal->m_Effects[0].m_Gradient.m_Fit);
    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyphFaceColors first;
    TextGlyphFaceColors last;
    TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[0], white, &first);
    TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[TextLayoutGetGlyphCount(layout) - 1], white, &last);
    ASSERT_EQ(0.0f, first.m_BottomLeft[1]);
    ASSERT_GT(first.m_BottomRight[1], first.m_BottomLeft[1]);
    ASSERT_LT(first.m_BottomRight[1], 1.0f);
    ASSERT_EQ(1.0f, last.m_BottomRight[1]);

    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, LayoutAnimatesHorizontalGradientPerGlyph)
{
    const char source[] = "<gradient hz=0.25 left=#FF5555 right=#5555FF fit=glyph>Flowing Glyph Colors</gradient>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));

    TextLayout* internal = (TextLayout*)layout;
    ASSERT_EQ(1u, internal->m_Effects.Size());
    ASSERT_EQ((uint8_t)TEXT_EFFECT_FIT_GLYPH, internal->m_Effects[0].m_Gradient.m_Fit);
    ASSERT_EQ((uint8_t)TEXT_GRADIENT_MODE_HORIZONTAL, internal->m_Effects[0].m_Gradient.m_Mode);
    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyphFaceColors first;
    TextGlyphFaceColors second;
    TextGlyphFaceColors animated;
    TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[0], white, &first);
    TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[1], white, &second);
    ASSERT_EQ(first.m_BottomLeft[0], first.m_BottomRight[0]);
    ASSERT_EQ(first.m_BottomLeft[0], first.m_TopLeft[0]);
    ASSERT_EQ(first.m_BottomLeft[0], first.m_TopRight[0]);
    ASSERT_EQ(first.m_BottomLeft[2], first.m_BottomRight[2]);
    ASSERT_NE(first.m_BottomLeft[0], second.m_BottomLeft[0]);
    ASSERT_NE(first.m_BottomLeft[2], second.m_BottomLeft[2]);

    TextLayoutUpdate(layout, 1.0f);
    TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[0], white, &animated);
    ASSERT_NE(first.m_BottomLeft[0], animated.m_BottomLeft[0]);
    ASSERT_NE(first.m_BottomLeft[2], animated.m_BottomLeft[2]);
    ASSERT_EQ(animated.m_BottomLeft[0], animated.m_BottomRight[0]);
    ASSERT_EQ(animated.m_BottomLeft[2], animated.m_TopRight[2]);

    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, LayoutAnimatesOneColorForWholeGradientSpan)
{
    const char source[] = "<gradient hz=0.25 left=#FF00FF right=#FFFFFF fit=span>Flowing Span Color</gradient>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));

    TextLayout* internal = (TextLayout*)layout;
    ASSERT_EQ(1u, internal->m_Effects.Size());
    ASSERT_EQ((uint8_t)TEXT_EFFECT_FIT_SPAN, internal->m_Effects[0].m_Gradient.m_Fit);
    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyphFaceColors first;
    TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[0], white, &first);
    ASSERT_EQ(1.0f, first.m_BottomLeft[0]);
    ASSERT_EQ(0.0f, first.m_BottomLeft[1]);
    ASSERT_EQ(1.0f, first.m_BottomLeft[2]);
    for (uint32_t glyph_index = 0; glyph_index < TextLayoutGetGlyphCount(layout); ++glyph_index)
    {
        TextGlyphFaceColors colors;
        TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[glyph_index], white, &colors);
        for (uint32_t channel = 0; channel < 4; ++channel)
        {
            ASSERT_EQ(first.m_BottomLeft[channel], colors.m_BottomLeft[channel]);
            ASSERT_EQ(first.m_BottomLeft[channel], colors.m_BottomRight[channel]);
            ASSERT_EQ(first.m_BottomLeft[channel], colors.m_TopLeft[channel]);
            ASSERT_EQ(first.m_BottomLeft[channel], colors.m_TopRight[channel]);
        }
    }

    TextLayoutUpdate(layout, 1.0f);
    TextGlyphFaceColors animated_first;
    TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[0], white, &animated_first);
    ASSERT_EQ(1.0f, animated_first.m_BottomLeft[0]);
    ASSERT_EQ(0.5f, animated_first.m_BottomLeft[1]);
    ASSERT_EQ(1.0f, animated_first.m_BottomLeft[2]);
    for (uint32_t glyph_index = 0; glyph_index < TextLayoutGetGlyphCount(layout); ++glyph_index)
    {
        TextGlyphFaceColors colors;
        TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[glyph_index], white, &colors);
        for (uint32_t channel = 0; channel < 4; ++channel)
        {
            ASSERT_EQ(animated_first.m_BottomLeft[channel], colors.m_BottomLeft[channel]);
            ASSERT_EQ(animated_first.m_BottomLeft[channel], colors.m_BottomRight[channel]);
            ASSERT_EQ(animated_first.m_BottomLeft[channel], colors.m_TopLeft[channel]);
            ASSERT_EQ(animated_first.m_BottomLeft[channel], colors.m_TopRight[channel]);
        }
    }

    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, LayoutObjectStateResolvesNamedStylesWithoutRelayout)
{
    FontCollectionSetNamedStyle(m_FontCollection, dmHashString64("action"), MakeTestColorStyle(1.0f, 0.0f, 0.0f));
    FontCollectionSetNamedStyle(m_FontCollection, dmHashString64("action:hover"), MakeTestColorStyle(0.0f, 1.0f, 0.0f));
    FontCollectionSetNamedStyle(m_FontCollection, dmHashString64("action:active"), MakeTestColorStyle(0.0f, 0.0f, 1.0f));

    const char source[] = "<a id=manual style=action>AB</a>C";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
    MarkupDestroy(markup);

    ASSERT_EQ(1u, TextLayoutGetObjectCount(layout));
    const uint64_t object_id = TextLayoutGetObjects(layout)[0].m_Id;
    ASSERT_EQ(dmHashString64("manual"), object_id);
    AssertGlyphColor(layout, 0, 1.0f, 0.0f, 0.0f);
    AssertGlyphColor(layout, 1, 1.0f, 0.0f, 0.0f);
    AssertGlyphColor(layout, 2, 1.0f, 1.0f, 1.0f);

    ASSERT_EQ(1u, TextLayoutSetObjectState(layout, object_id, TEXT_LAYOUT_OBJECT_STATE_HOVERED, 1));
    ASSERT_EQ(TEXT_LAYOUT_OBJECT_STATE_HOVERED, TextLayoutGetObjects(layout)[0].m_State);
    AssertGlyphColor(layout, 0, 0.0f, 1.0f, 0.0f);
    ASSERT_EQ(0u, TextLayoutSetObjectState(layout, object_id, TEXT_LAYOUT_OBJECT_STATE_HOVERED, 1));

    ASSERT_EQ(1u, TextLayoutSetObjectState(layout, object_id, TEXT_LAYOUT_OBJECT_STATE_ACTIVE, 1));
    AssertGlyphColor(layout, 0, 0.0f, 0.0f, 1.0f);
    ASSERT_EQ(1u, TextLayoutSetObjectState(layout, object_id, TEXT_LAYOUT_OBJECT_STATE_ACTIVE, 0));
    AssertGlyphColor(layout, 0, 0.0f, 1.0f, 0.0f);
    ASSERT_EQ(1u, TextLayoutSetObjectState(layout, object_id, TEXT_LAYOUT_OBJECT_STATE_HOVERED, 0));
    AssertGlyphColor(layout, 0, 1.0f, 0.0f, 0.0f);

    FontCollectionSetNamedStyle(m_FontCollection, dmHashString64("action"), MakeTestColorStyle(1.0f, 1.0f, 0.0f));
    ASSERT_TRUE(TextLayoutRefreshObjectStyles(layout));
    AssertGlyphColor(layout, 0, 1.0f, 1.0f, 0.0f);

    TextLayoutRelease(layout);
}

TEST_F(FontTest, LayoutObjectStyleMarkupComposesEffects)
{
    MarkupError error = {};
    const char base_definition[] = "<color=#ff0000><shake amplitude=2 hz=10>";
    ASSERT_TRUE(FontCollectionSetNamedStyleMarkup(m_FontCollection, dmHashString64("alert"), base_definition, sizeof(base_definition) - 1, &error));
    const char hover_definition[] = "<color=#00ff00><wave amplitude=1 hz=2>";
    ASSERT_TRUE(FontCollectionSetNamedStyleMarkup(m_FontCollection, dmHashString64("alert:hover"), hover_definition, sizeof(hover_definition) - 1, &error));

    const char source[] = "<a id=alert style=alert>AB</a>C";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
    MarkupDestroy(markup);

    TextLayout* internal = (TextLayout*)layout;
    TextGlyph*  glyphs = TextLayoutGetGlyphs(layout);
    AssertGlyphColor(layout, 0, 1.0f, 0.0f, 0.0f);
    ASSERT_EQ(1u, internal->m_Effects.Size());
    ASSERT_EQ(1u, internal->m_ResolvedSpans[glyphs[0].m_MarkupSpanIndex].m_EffectCount);

    const uint64_t object_id = TextLayoutGetObjects(layout)[0].m_Id;
    ASSERT_EQ(1u, TextLayoutSetObjectState(layout, object_id, TEXT_LAYOUT_OBJECT_STATE_HOVERED, 1));
    AssertGlyphColor(layout, 0, 0.0f, 1.0f, 0.0f);
    ASSERT_EQ(2u, internal->m_Effects.Size());
    ASSERT_EQ(2u, internal->m_ResolvedSpans[glyphs[0].m_MarkupSpanIndex].m_EffectCount);

    ASSERT_EQ(1u, TextLayoutSetObjectState(layout, object_id, TEXT_LAYOUT_OBJECT_STATE_HOVERED, 0));
    ASSERT_EQ(1u, internal->m_Effects.Size());
    ASSERT_EQ(1u, internal->m_ResolvedSpans[glyphs[0].m_MarkupSpanIndex].m_EffectCount);

    TextLayoutRelease(layout);
}

TEST_F(FontTest, LayoutResolvesUnderlineAndStrikeDecorations)
{
    const char source[] = "<ul>A</ul><strike pattern=dashed>B</strike><ul><strike>C</strike></ul>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
    MarkupDestroy(markup);

    ASSERT_EQ(4u, TextLayoutGetDecorationCount(layout));
    const TextDecoration* decorations = TextLayoutGetDecorations(layout);
    ASSERT_EQ((uint8_t)TEXT_DECORATION_PATTERN_SOLID, decorations[0].m_Pattern);
    ASSERT_TRUE(decorations[0].m_Y < 0.0f);
    ASSERT_EQ((uint8_t)TEXT_DECORATION_PATTERN_DASHED, decorations[1].m_Pattern);
    ASSERT_TRUE(decorations[1].m_Y > 0.0f);
    ASSERT_TRUE(decorations[2].m_Y < 0.0f);
    ASSERT_TRUE(decorations[3].m_Y > 0.0f);
    for (uint32_t i = 0; i < TextLayoutGetDecorationCount(layout); ++i)
    {
        ASSERT_TRUE(decorations[i].m_Length > 0.0f);
        ASSERT_TRUE(decorations[i].m_Thickness > 0.0f);
        ASSERT_EQ(0u, decorations[i].m_LineIndex);
        ASSERT_EQ(1u, decorations[i].m_GlyphCount);
    }
    TextLayoutRelease(layout);
}

TEST_F(FontTest, DecorationGeometryCoalescesCompatibleGlyphs)
{
    const char* source[] = {
        "<ul>ABCDE</ul>",
        "<ul><gradient left=#FF0000 right=#0000FF fit=span>ABCDE</gradient></ul>",
        "<ul pattern=dashed>ABCDE</ul>",
    };
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(source); ++i)
    {
        HMarkup markup = 0;
        ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source[i], (uint32_t)strlen(source[i]), &markup, 0));
        HTextLayout layout = 0;
        ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
        MarkupDestroy(markup);

        ASSERT_EQ(1u, TextLayoutGetDecorationCount(layout));
        const TextDecoration& decoration = TextLayoutGetDecorations(layout)[0];
        ASSERT_EQ(5u, decoration.m_GlyphCount);
        ASSERT_FALSE(FontDecorationRequiresGlyphSegments(layout, decoration));
        if (decoration.m_Pattern == TEXT_DECORATION_PATTERN_SOLID)
            ASSERT_EQ(1u, FontGetDecorationQuadCount(layout, decoration));
        else
            ASSERT_EQ(1u, FontGetDecorationQuadCount(layout, decoration));
        TextLayoutRelease(layout);
    }
}

TEST_F(FontTest, DashedDecorationUsesOneQuadWithStablePatternCoordinates)
{
    TextDecoration decoration = {};
    decoration.m_Length = 20.0f;
    decoration.m_Thickness = 2.0f;
    decoration.m_PatternOffset = 3.0f;
    decoration.m_Pattern = TEXT_DECORATION_PATTERN_DASHED;

    FontDecorationPattern first;
    FontDecorationPattern second;
    FontGetDecorationPattern(decoration, 0, 2, &first);
    FontGetDecorationPattern(decoration, 1, 2, &second);
    ASSERT_NEAR(0.3f, first.m_Start, 0.0001f);
    ASSERT_NEAR(1.3f, first.m_End, 0.0001f);
    ASSERT_NEAR(0.6f, first.m_Duty, 0.0001f);
    ASSERT_NEAR(first.m_End, second.m_Start, 0.0001f);
    ASSERT_NEAR(2.3f, second.m_End, 0.0001f);

    TextGlyphFaceColors colors = {};
    FontGlyphVertex     vertices[6] = {};
    FontPackDecorationVertices(0.0f, 0.0f, 1, 0, 6, dmVMath::Matrix4::identity(),
                               0.0f, 0.0f, 10.0f, 0.0f, decoration.m_Thickness,
                               first.m_Start, first.m_End, first.m_Duty, colors, vertices);
    ASSERT_NEAR(1.0f, vertices[0].m_LayerMasks[0], 0.0001f);
    ASSERT_NEAR(first.m_Start, vertices[0].m_LayerMasks[1], 0.0001f);
    ASSERT_NEAR(-first.m_Duty, vertices[0].m_LayerMasks[2], 0.0001f);
    ASSERT_NEAR(first.m_End, vertices[1].m_LayerMasks[1], 0.0001f);
    ASSERT_NEAR(first.m_Start, vertices[2].m_LayerMasks[1], 0.0001f);
    ASSERT_NEAR(first.m_End, vertices[5].m_LayerMasks[1], 0.0001f);
}

TEST_F(FontTest, DecorationGeometryPreservesPerGlyphGradient)
{
    const char source[] = "<ul><gradient left=#FF0000 right=#0000FF fit=glyph>ABCDE</gradient></ul>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
    MarkupDestroy(markup);

    ASSERT_EQ(1u, TextLayoutGetDecorationCount(layout));
    const TextDecoration& decoration = TextLayoutGetDecorations(layout)[0];
    ASSERT_TRUE(FontDecorationRequiresGlyphSegments(layout, decoration));
    ASSERT_EQ(decoration.m_GlyphCount, FontGetDecorationQuadCount(layout, decoration));
    TextLayoutRelease(layout);
}

TEST_F(FontTest, LayoutRecoversInvalidDecorations)
{
    const char* invalid[] = {
        "<ul color=#ff0000>A</ul>",
        "<ul pattern=dotted>A</ul>",
        "<strike pattern=wave>A</strike>",
        "<strike value=solid>A</strike>",
    };
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(invalid); ++i)
    {
        HMarkup markup = 0;
        ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(invalid[i], (uint32_t)strlen(invalid[i]), &markup, 0));
        HTextLayout layout = 0;
        ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
        ASSERT_EQ(0u, TextLayoutGetDecorationCount(layout));
        TextLayoutRelease(layout);
        MarkupDestroy(markup);
    }
}

TEST_F(FontTest, LayoutDecorationPositionTracksSpanFontSize)
{
    const char source[] = "<strike>A<size=200%>B</size></strike>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
    MarkupDestroy(markup);

    ASSERT_EQ(2u, TextLayoutGetDecorationCount(layout));
    const TextDecoration* decorations = TextLayoutGetDecorations(layout);
    ASSERT_TRUE(decorations[0].m_Y > 0.0f);
    ASSERT_NEAR(decorations[0].m_Y * 2.0f, decorations[1].m_Y, 0.0001f);
    ASSERT_NEAR(decorations[0].m_Thickness * 2.0f, decorations[1].m_Thickness, 0.0001f);
    TextLayoutRelease(layout);
}

TEST_F(FontTest, LayoutEvaluatesWaveEffect)
{
    const char source[] = "<wave amplitude=4 hz=0.5 wavelength=4>AB</wave>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
    MarkupDestroy(markup);

    const float base_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyph* glyphs = TextLayoutGetGlyphs(layout);
    TextGlyphRenderData first;
    TextGlyphRenderData second;
    TextLayoutGetGlyphRenderData(layout, glyphs[0], base_color, &first);
    TextLayoutGetGlyphRenderData(layout, glyphs[1], base_color, &second);
    ASSERT_NEAR(0.0f, first.m_OffsetY, 0.0001f);
    ASSERT_NEAR(4.0f, second.m_OffsetY, 0.0001f);

    TextLayoutUpdate(layout, 0.5f);
    TextLayoutGetGlyphRenderData(layout, glyphs[0], base_color, &first);
    ASSERT_NEAR(4.0f, first.m_OffsetY, 0.0001f);
    TextLayoutRelease(layout);
}

TEST_F(FontTest, LayoutWaveUsesDefaultAnimationSettings)
{
    const char source[] = "<wave>AB</wave>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
    MarkupDestroy(markup);

    const float base_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyph* glyphs = TextLayoutGetGlyphs(layout);
    TextGlyphRenderData first;
    TextGlyphRenderData second;
    TextLayoutGetGlyphRenderData(layout, glyphs[0], base_color, &first);
    TextLayoutGetGlyphRenderData(layout, glyphs[1], base_color, &second);
    ASSERT_NEAR(0.0f, first.m_OffsetY, 0.0001f);
    ASSERT_NEAR(sinf(6.28318530717958647692f / 6.0f), second.m_OffsetY, 0.0001f);

    TextLayoutUpdate(layout, 0.5f);
    TextLayoutGetGlyphRenderData(layout, glyphs[0], base_color, &first);
    TextLayoutGetGlyphRenderData(layout, glyphs[1], base_color, &second);
    ASSERT_NEAR(0.0f, first.m_OffsetY, 0.0001f);
    ASSERT_NEAR(-sinf(6.28318530717958647692f / 3.0f), second.m_OffsetY, 0.0001f);
    TextLayoutRelease(layout);
}

TEST_F(FontTest, LayoutWaveSupportsGlyphAndSpanFit)
{
    const char source[] = "<wave amplitude=4 hz=1 fit=span>AB</wave><wave amplitude=4 hz=1 fit=glyph>CD</wave>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));

    const float base_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyph* glyphs = TextLayoutGetGlyphs(layout);
    TextGlyphRenderData first;
    TextGlyphRenderData second;
    TextLayoutUpdate(layout, 0.125f);
    TextLayoutGetGlyphRenderData(layout, glyphs[0], base_color, &first);
    TextLayoutGetGlyphRenderData(layout, glyphs[1], base_color, &second);
    ASSERT_EQ(first.m_OffsetY, second.m_OffsetY);
    TextLayoutGetGlyphRenderData(layout, glyphs[2], base_color, &first);
    TextLayoutGetGlyphRenderData(layout, glyphs[3], base_color, &second);
    ASSERT_NE(first.m_OffsetY, second.m_OffsetY);

    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, LayoutShakeSupportsGlyphAndSpanFit)
{
    const char source[] = "<shake hz=12 amplitude=0.8 fit=span>AB</shake><shake hz=12 amplitude=0.8 fit=glyph>CD</shake>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));

    const float base_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyph* glyphs = TextLayoutGetGlyphs(layout);
    TextGlyphRenderData first;
    TextGlyphRenderData second;
    TextLayoutUpdate(layout, 0.03f);
    TextLayoutGetGlyphRenderData(layout, glyphs[0], base_color, &first);
    TextLayoutGetGlyphRenderData(layout, glyphs[1], base_color, &second);
    ASSERT_EQ(first.m_OffsetX, second.m_OffsetX);
    ASSERT_EQ(first.m_OffsetY, second.m_OffsetY);
    TextLayoutGetGlyphRenderData(layout, glyphs[2], base_color, &first);
    TextLayoutGetGlyphRenderData(layout, glyphs[3], base_color, &second);
    ASSERT_TRUE(first.m_OffsetX != second.m_OffsetX || first.m_OffsetY != second.m_OffsetY);

    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, LayoutWaveSupportsReverseDirection)
{
    const char source[] = "<wave amplitude=4 hz=1 fit=span direction=forward>A</wave><wave amplitude=4 hz=1 fit=span direction=reverse>B</wave>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));

    const float base_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyphRenderData forward;
    TextGlyphRenderData reverse;
    TextLayoutUpdate(layout, 0.125f);
    TextLayoutGetGlyphRenderData(layout, TextLayoutGetGlyphs(layout)[0], base_color, &forward);
    TextLayoutGetGlyphRenderData(layout, TextLayoutGetGlyphs(layout)[1], base_color, &reverse);
    ASSERT_NEAR(forward.m_OffsetY, -reverse.m_OffsetY, 0.0001f);

    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, LayoutRecoversInvalidWave)
{
    const char* invalid_waves[] = {
        "<wave hz=-1>AB</wave>",
        "<wave amplitude=-1>AB</wave>",
        "<wave wavelength=0>AB</wave>",
        "<wave speed=1>AB</wave>",
        "<wave span=char>AB</wave>",
        "<wave fit=word>AB</wave>",
        "<wave direction=backward>AB</wave>",
    };
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(invalid_waves); ++i)
    {
        HMarkup markup = 0;
        ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(invalid_waves[i], (uint32_t)strlen(invalid_waves[i]), &markup, 0));
        HTextLayout layout = 0;
        ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
        ASSERT_EQ(0u, ((TextLayout*)layout)->m_Effects.Size());
        TextLayoutRelease(layout);
        MarkupDestroy(markup);
    }
}

TEST_F(FontTest, LayoutRejectsInvalidObjectDimensions)
{
    const char* invalid_objects[] = {
        "<sprite width=0/>",
        "<sprite height=-1/>",
        "<sprite width=bad/>",
        "<sprite height=0em/>",
    };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(invalid_objects); ++i)
    {
        HMarkup markup = 0;
        ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(invalid_objects[i], strlen(invalid_objects[i]), &markup, 0));
        LayoutObjectTestContext context = {};
        TextLayoutSettings      settings = {};
        settings.m_Size = 32.0f;
        settings.m_ResolveObject = ResolveTestLayoutObject;
        settings.m_ObjectContext = &context;
        HTextLayout layout = (HTextLayout)(uintptr_t)1;
        ASSERT_EQ(TEXT_RESULT_ERROR, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
        ASSERT_EQ((HTextLayout)0, layout);
        ASSERT_EQ(0u, context.m_ResolveCount);
        MarkupDestroy(markup);
    }
}

TEST_F(FontTest, LayoutRequiresResolverForResourceObjects)
{
    const char source[] = "<sprite src=/icon.png/>";
    HMarkup    markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));
    TextLayoutSettings settings = {};
    settings.m_Size = 32.0f;
    HTextLayout layout = (HTextLayout)(uintptr_t)1;
    ASSERT_EQ(TEXT_RESULT_ERROR, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
    ASSERT_EQ((HTextLayout)0, layout);
    MarkupDestroy(markup);
}

#if defined(FONT_USE_SKRIBIDI)
TEST_F(FontTest, SkribidiLayoutResolvesNestedMarkup)
{
    const char source[] =
    "<wave amplitude=4 hz=2 wavelength=3>This <gradient fit=glyph left=#FF00FF right=#FFFFFF>Whole</gradient> Text</wave>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));

    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));

    ASSERT_NE((HTextLayout)0, layout);

    TextLayout* internal = (TextLayout*)layout;
    ASSERT_EQ(1u, internal->m_Styles.Size());
    ASSERT_EQ(2u, internal->m_Effects.Size());
    ASSERT_EQ(3u, internal->m_ResolvedSpans.Size());
    ASSERT_EQ(4u, internal->m_SpanEffects.Size());

    const TextEffect& wave = internal->m_Effects[0];
    ASSERT_EQ((uint16_t)TEXT_EFFECT_WAVE, wave.m_Type);
    ASSERT_EQ((uint16_t)TEXT_EFFECT_AFFECTS_POSITION, wave.m_Flags);
    ASSERT_EQ(0u, wave.m_TextOffset);
    ASSERT_EQ(15u, wave.m_TextLength);
    ASSERT_EQ(4.0f, wave.m_Wave.m_Amplitude);
    ASSERT_EQ(2.0f, wave.m_Wave.m_Hz);
    ASSERT_EQ(3.0f, wave.m_Wave.m_Wavelength);

    const TextEffect& gradient = internal->m_Effects[1];
    ASSERT_EQ((uint16_t)TEXT_EFFECT_GRADIENT, gradient.m_Type);
    ASSERT_EQ((uint16_t)TEXT_EFFECT_AFFECTS_COLOR, gradient.m_Flags);
    ASSERT_EQ(5u, gradient.m_TextOffset);
    ASSERT_EQ(5u, gradient.m_TextLength);

    const TextResolvedSpan& gradient_span = internal->m_ResolvedSpans[1];
    ASSERT_EQ(5u, gradient_span.m_TextOffset);
    ASSERT_EQ(5u, gradient_span.m_TextLength);
    ASSERT_EQ(2u, gradient_span.m_EffectCount);
    ASSERT_EQ(0u, internal->m_SpanEffects[gradient_span.m_EffectIndex]);
    ASSERT_EQ(1u, internal->m_SpanEffects[gradient_span.m_EffectIndex + 1]);

    TextGlyph*  glyphs = TextLayoutGetGlyphs(layout);
    uint32_t    glyph_count = TextLayoutGetGlyphCount(layout);
    bool        found_gradient_glyph = false;
    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    for (uint32_t i = 0; i < glyph_count; ++i)
    {
        if (glyphs[i].m_Cluster >= 5 && glyphs[i].m_Cluster < 10)
        {
            ASSERT_EQ(1u, glyphs[i].m_MarkupSpanIndex);
            TextGlyphFaceColors colors;
            TextLayoutGetGlyphFaceColors(layout, glyphs[i], white, &colors);
            const float expected_green = ((float)glyphs[i].m_Cluster - 5.0f + 0.5f) / 5.0f;
            ASSERT_NEAR(expected_green, colors.m_BottomLeft[1], 0.0001f);
            ASSERT_EQ(colors.m_BottomLeft[1], colors.m_TopLeft[1]);
            ASSERT_EQ(colors.m_BottomLeft[1], colors.m_BottomRight[1]);
            ASSERT_EQ(colors.m_BottomLeft[1], colors.m_TopRight[1]);
            found_gradient_glyph = true;
        }
    }
    ASSERT_TRUE(found_gradient_glyph);

    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, SkribidiLayoutResolvesVerticalGradient)
{
    const char source[] = "<gradient fit=glyph bottom=#FF0000 top=#0000FF>Up</gradient>";
    HMarkup    markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));

    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));

    TextLayout* internal = (TextLayout*)layout;
    ASSERT_EQ(1u, internal->m_Effects.Size());
    ASSERT_EQ((uint8_t)TEXT_EFFECT_FIT_GLYPH, internal->m_Effects[0].m_Gradient.m_Fit);
    const float         white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyphFaceColors colors;
    TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[0], white, &colors);
    ASSERT_EQ(1.0f, colors.m_BottomLeft[0]);
    ASSERT_EQ(1.0f, colors.m_BottomRight[0]);
    ASSERT_EQ(0.0f, colors.m_BottomLeft[2]);
    ASSERT_EQ(0.0f, colors.m_TopLeft[0]);
    ASSERT_EQ(1.0f, colors.m_TopLeft[2]);
    ASSERT_EQ(1.0f, colors.m_TopRight[2]);

    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, SkribidiLayoutAnimatesGradientContinuously)
{
    const char source[] = "<gradient hz=0.25 left=#FF5555 right=#5555FF>Flowing colors</gradient>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
    ASSERT_EQ((uint8_t)TEXT_EFFECT_FIT_TEXT, ((TextLayout*)layout)->m_Effects[0].m_Gradient.m_Fit);

    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyphFaceColors start;
    TextGlyphFaceColors quarter;
    TextGlyphFaceColors quarter_last;
    TextGlyphFaceColors three_quarters;
    TextGlyphFaceColors wrapped;
    TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[0], white, &start);
    TextLayoutUpdate(layout, 1.0f);
    TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[0], white, &quarter);
    TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[TextLayoutGetGlyphCount(layout) - 1], white, &quarter_last);
    TextLayoutUpdate(layout, 2.0f);
    TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[0], white, &three_quarters);
    TextLayoutUpdate(layout, 1.0f);
    TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[0], white, &wrapped);
    ASSERT_NE(start.m_BottomRight[0], quarter.m_BottomRight[0]);
    ASSERT_NE(start.m_BottomRight[2], quarter.m_BottomRight[2]);
    ASSERT_NE(quarter.m_BottomLeft[0], quarter.m_BottomRight[0]);
    ASSERT_EQ(quarter.m_BottomLeft[0], quarter.m_TopLeft[0]);
    ASSERT_EQ(quarter.m_BottomRight[0], quarter.m_TopRight[0]);
    ASSERT_NE(quarter.m_BottomLeft[0], quarter_last.m_BottomLeft[0]);
    ASSERT_NE(quarter.m_BottomLeft[2], quarter.m_BottomRight[2]);
    ASSERT_EQ(quarter.m_BottomLeft[2], quarter.m_TopLeft[2]);
    ASSERT_EQ(quarter.m_BottomRight[2], quarter.m_TopRight[2]);
    ASSERT_NE(quarter.m_BottomLeft[2], quarter_last.m_BottomLeft[2]);
    ASSERT_LT(quarter.m_BottomRight[0], three_quarters.m_BottomRight[0]);
    ASSERT_GT(quarter.m_BottomRight[2], three_quarters.m_BottomRight[2]);
    ASSERT_NEAR(start.m_BottomLeft[0], wrapped.m_BottomLeft[0], 0.0001f);
    ASSERT_NEAR(start.m_BottomLeft[2], wrapped.m_BottomLeft[2], 0.0001f);

    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, SkribidiLayoutReversesGradientDirection)
{
    const char source[] =
    "<gradient hz=1 direction=forward fit=span left=#FF0000 right=#0000FF>AB</gradient>"
    "<gradient hz=1 direction=reverse fit=span left=#FF0000 right=#0000FF>CD</gradient>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));

    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyphFaceColors forward;
    TextGlyphFaceColors reverse;
    TextLayoutUpdate(layout, 0.125f);
    TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[0], white, &forward);
    TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[2], white, &reverse);
    ASSERT_NEAR(0.75f, forward.m_BottomRight[0], 0.0001f);
    ASSERT_NEAR(0.25f, reverse.m_BottomRight[0], 0.0001f);

    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, SkribidiLayoutAnimatesGlyphGradient)
{
    const char source[] = "<gradient hz=0.25 left=#FF5555 right=#5555FF fit=glyph>Flowing Glyph Colors</gradient>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));

    TextLayout* internal = (TextLayout*)layout;
    ASSERT_EQ(1u, internal->m_Effects.Size());
    ASSERT_EQ((uint8_t)TEXT_EFFECT_FIT_GLYPH, internal->m_Effects[0].m_Gradient.m_Fit);
    ASSERT_EQ(0.25f, internal->m_Effects[0].m_Gradient.m_Hz);
    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyphFaceColors start;
    TextGlyphFaceColors animated;
    TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[0], white, &start);
    TextLayoutUpdate(layout, 1.0f);
    TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[0], white, &animated);
    ASSERT_NE(start.m_BottomLeft[0], animated.m_BottomLeft[0]);
    ASSERT_NE(start.m_BottomLeft[2], animated.m_BottomLeft[2]);
    ASSERT_EQ(start.m_BottomLeft[0], start.m_BottomRight[0]);
    ASSERT_EQ(start.m_BottomLeft[2], start.m_BottomRight[2]);
    ASSERT_EQ(animated.m_BottomLeft[0], animated.m_BottomRight[0]);
    ASSERT_EQ(animated.m_BottomLeft[2], animated.m_BottomRight[2]);

    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, SkribidiLayoutResolvesFourCornerGradient)
{
    const char source[] =
    "<gradient fit=glyph tl=#0000FF tr=#FFFFFF bl=#FF0000 br=#00FF00>Quad</gradient>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));

    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));

    const float         white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyphFaceColors colors;
    TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[0], white, &colors);
    ASSERT_EQ(1.0f, colors.m_BottomLeft[0]);
    ASSERT_EQ(1.0f, colors.m_BottomRight[1]);
    ASSERT_EQ(1.0f, colors.m_TopLeft[2]);
    ASSERT_EQ(1.0f, colors.m_TopRight[0]);
    ASSERT_EQ(1.0f, colors.m_TopRight[1]);
    ASSERT_EQ(1.0f, colors.m_TopRight[2]);

    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, SkribidiLayoutRecoversInvalidGradient)
{
    const char* invalid_gradients[] = {
        "<gradient left=#FF0000>Text</gradient>",
        "<gradient top=#FF0000>Text</gradient>",
        "<gradient left=#FF0000 right=#00FF00 top=#0000FF bottom=#FFFFFF>Text</gradient>",
        "<gradient tl=#0000FF tr=#FFFFFF bl=#FF0000>Text</gradient>",
        "<gradient left=invalid right=#00FF00>Text</gradient>",
        "<gradient left=0xFF0000 right=#00FF00>Text</gradient>",
        "<gradient left=FF0000 right=#00FF00>Text</gradient>",
        "<gradient fit=word left=#FF0000 right=#00FF00>Text</gradient>",
        "<gradient speed=1 left=#FF0000 right=#00FF00>Text</gradient>",
        "<gradient hz=-1 left=#FF0000 right=#00FF00>Text</gradient>",
        "<gradient direction=backward left=#FF0000 right=#00FF00>Text</gradient>",
    };
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(invalid_gradients); ++i)
    {
        HMarkup markup = 0;
        ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(invalid_gradients[i], (uint32_t)strlen(invalid_gradients[i]), &markup, 0));
        HTextLayout layout = 0;
        ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
        ASSERT_EQ(0u, ((TextLayout*)layout)->m_Effects.Size());
        TextLayoutRelease(layout);
        MarkupDestroy(markup);
    }
}

TEST_F(FontTest, SkribidiLayoutResolvesShakeAndAccumulatesTime)
{
    const char source[] =
    "<shake>Default</shake> <shake hz=12 amplitude=0.8 fit=span>Custom</shake>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));

    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));

    TextLayout* internal = (TextLayout*)layout;
    ASSERT_EQ(2u, internal->m_Effects.Size());
    const TextEffect& default_shake = internal->m_Effects[0];
    ASSERT_EQ((uint16_t)TEXT_EFFECT_SHAKE, default_shake.m_Type);
    ASSERT_EQ((uint16_t)TEXT_EFFECT_AFFECTS_POSITION, default_shake.m_Flags);
    ASSERT_EQ(20.0f, default_shake.m_Shake.m_Hz);
    ASSERT_EQ(0.5f, default_shake.m_Shake.m_Amplitude);
    ASSERT_EQ((uint8_t)TEXT_EFFECT_FIT_GLYPH, default_shake.m_Shake.m_Fit);
    const TextEffect& custom_shake = internal->m_Effects[1];
    ASSERT_EQ(12.0f, custom_shake.m_Shake.m_Hz);
    ASSERT_EQ(0.8f, custom_shake.m_Shake.m_Amplitude);
    ASSERT_EQ((uint8_t)TEXT_EFFECT_FIT_SPAN, custom_shake.m_Shake.m_Fit);

    TextGlyph* glyphs = TextLayoutGetGlyphs(layout);
    ASSERT_GT(TextLayoutGetGlyphCount(layout), 0u);
    const float glyph_x = glyphs[0].m_X;
    const float glyph_y = glyphs[0].m_Y;
    float width_before;
    float height_before;
    TextLayoutGetBounds(layout, &width_before, &height_before);
    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyphRenderData before;
    TextLayoutGetGlyphRenderData(layout, glyphs[0], white, &before);

    TextLayoutUpdate(layout, 0.03f);
    TextLayoutUpdate(layout, -1.0f);
    TextLayoutUpdate(layout, INFINITY);
    ASSERT_NEAR(0.03, internal->m_ElapsedTime, 0.000001);
    TextGlyphRenderData after;
    TextLayoutGetGlyphRenderData(layout, glyphs[0], white, &after);
    ASSERT_TRUE(fabsf(before.m_OffsetX - after.m_OffsetX) > 0.0001f ||
                fabsf(before.m_OffsetY - after.m_OffsetY) > 0.0001f);
    TextGlyphRenderData span_first;
    TextGlyphRenderData span_second;
    TextLayoutGetGlyphRenderData(layout, glyphs[8], white, &span_first);
    TextLayoutGetGlyphRenderData(layout, glyphs[9], white, &span_second);
    ASSERT_EQ(span_first.m_OffsetX, span_second.m_OffsetX);
    ASSERT_EQ(span_first.m_OffsetY, span_second.m_OffsetY);
    ASSERT_EQ(glyph_x, glyphs[0].m_X);
    ASSERT_EQ(glyph_y, glyphs[0].m_Y);
    float width_after;
    float height_after;
    TextLayoutGetBounds(layout, &width_after, &height_after);
    ASSERT_EQ(width_before, width_after);
    ASSERT_EQ(height_before, height_after);

    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, SkribidiLayoutRecoversInvalidShake)
{
    const char* invalid_shakes[] = {
        "<shake hz=-1>Text</shake>",
        "<shake hz=nan>Text</shake>",
        "<shake amplitude=-1>Text</shake>",
        "<shake fit=word>Text</shake>",
        "<shake rate=20>Text</shake>",
        "<shake connected=1>Text</shake>",
    };
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(invalid_shakes); ++i)
    {
        HMarkup markup = 0;
        ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(invalid_shakes[i], (uint32_t)strlen(invalid_shakes[i]), &markup, 0));
        HTextLayout layout = 0;
        ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
        ASSERT_EQ(0u, ((TextLayout*)layout)->m_Effects.Size());
        TextLayoutRelease(layout);
        MarkupDestroy(markup);
    }
}

TEST_F(FontTest, SkribidiLayoutResolvesMarkupOutline)
{
    const char source[] =
    "<outline size=3 color=#80FF00>AB<outline size=0>C</outline></outline>D";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));

    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
    ASSERT_TRUE(TextLayoutHasMarkupOutline(layout));
    ASSERT_EQ(3.0f, TextLayoutGetMaxMarkupOutlineWidth(layout));

    TextLayout* internal = (TextLayout*)layout;
    TextGlyph*  glyphs = TextLayoutGetGlyphs(layout);
    ASSERT_EQ(4u, TextLayoutGetGlyphCount(layout));
    const TextRenderStyle& outlined = internal->m_Styles[glyphs[0].m_StyleIndex];
    ASSERT_EQ((uint32_t)(TEXT_RENDER_STYLE_OUTLINE_COLOR | TEXT_RENDER_STYLE_OUTLINE_WIDTH),
              outlined.m_Flags & (TEXT_RENDER_STYLE_OUTLINE_COLOR | TEXT_RENDER_STYLE_OUTLINE_WIDTH));
    ASSERT_EQ(3.0f, outlined.m_OutlineWidth);
    ASSERT_NEAR(128.0f / 255.0f, outlined.m_OutlineColor[0], 0.0001f);
    ASSERT_EQ(1.0f, outlined.m_OutlineColor[1]);
    ASSERT_EQ(0.0f, outlined.m_OutlineColor[2]);

    const TextRenderStyle& disabled = internal->m_Styles[glyphs[2].m_StyleIndex];
    ASSERT_EQ(0.0f, disabled.m_OutlineWidth);
    ASSERT_TRUE((disabled.m_Flags & TEXT_RENDER_STYLE_OUTLINE_WIDTH) != 0);
    ASSERT_TRUE((disabled.m_Flags & TEXT_RENDER_STYLE_OUTLINE_COLOR) != 0);
    const TextRenderStyle& plain = internal->m_Styles[glyphs[3].m_StyleIndex];
    ASSERT_EQ(0u, plain.m_Flags & (TEXT_RENDER_STYLE_OUTLINE_COLOR | TEXT_RENDER_STYLE_OUTLINE_WIDTH));

    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyphRenderData render_data;
    TextLayoutGetGlyphRenderData(layout, glyphs[0], white, &render_data);
    ASSERT_EQ(3.0f, render_data.m_OutlineWidth);
    ASSERT_NEAR(128.0f / 255.0f, render_data.m_OutlineColor[0], 0.0001f);
    ASSERT_EQ(1.0f, render_data.m_OutlineColor[1]);
    ASSERT_EQ(0.0f, render_data.m_OutlineColor[2]);

    TextLayoutGetGlyphRenderData(layout, glyphs[2], white, &render_data);
    ASSERT_EQ(0.0f, render_data.m_OutlineWidth);
    ASSERT_EQ(0.0f, render_data.m_OutlineColor[3]);

    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, LayoutResolvesMarkupShadow)
{
    const char source[] =
    "<shadow x=3 y=-2 blur=4 color=#204080A0>A<shadow x=-1 blur=2>B</shadow></shadow>C";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));

    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
    ASSERT_TRUE(TextLayoutHasMarkupShadow(layout));
    ASSERT_EQ(3u, TextLayoutGetGlyphCount(layout));

    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyph* glyphs = TextLayoutGetGlyphs(layout);
    TextGlyphRenderData render_data;
    TextLayoutGetGlyphRenderData(layout, glyphs[0], white, &render_data);
    ASSERT_EQ(3.0f, render_data.m_ShadowX);
    ASSERT_EQ(-2.0f, render_data.m_ShadowY);
    ASSERT_EQ(4.0f, render_data.m_ShadowBlur);
    ASSERT_NEAR(32.0f / 255.0f, render_data.m_ShadowColor[0], 0.0001f);
    ASSERT_NEAR(64.0f / 255.0f, render_data.m_ShadowColor[1], 0.0001f);
    ASSERT_NEAR(128.0f / 255.0f, render_data.m_ShadowColor[2], 0.0001f);
    ASSERT_NEAR(160.0f / 255.0f, render_data.m_ShadowColor[3], 0.0001f);

    TextLayoutGetGlyphRenderData(layout, glyphs[1], white, &render_data);
    ASSERT_EQ(-1.0f, render_data.m_ShadowX);
    ASSERT_EQ(-2.0f, render_data.m_ShadowY);
    ASSERT_EQ(2.0f, render_data.m_ShadowBlur);
    ASSERT_TRUE((render_data.m_StyleFlags & TEXT_RENDER_STYLE_SHADOW_X) != 0);
    ASSERT_TRUE((render_data.m_StyleFlags & TEXT_RENDER_STYLE_SHADOW_Y) != 0);
    ASSERT_TRUE((render_data.m_StyleFlags & TEXT_RENDER_STYLE_SHADOW_BLUR) != 0);

    TextLayoutGetGlyphRenderData(layout, glyphs[2], white, &render_data);
    ASSERT_EQ(0u, render_data.m_StyleFlags & (TEXT_RENDER_STYLE_SHADOW_COLOR | TEXT_RENDER_STYLE_SHADOW_X | TEXT_RENDER_STYLE_SHADOW_Y | TEXT_RENDER_STYLE_SHADOW_BLUR));
    ASSERT_EQ(1.0f, render_data.m_ShadowColor[3]);

    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, LayoutRecoversInvalidMarkupShadow)
{
    const char* invalid_shadows[] = {
        "<shadow>Text</shadow>",
        "<shadow x=nan>Text</shadow>",
        "<shadow y=inf>Text</shadow>",
        "<shadow blur=-1>Text</shadow>",
        "<shadow blur=nan>Text</shadow>",
        "<shadow color=000000>Text</shadow>",
        "<shadow color=#12345>Text</shadow>",
    };
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(invalid_shadows); ++i)
    {
        HMarkup markup = 0;
        ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(invalid_shadows[i], (uint32_t)strlen(invalid_shadows[i]), &markup, 0));
        HTextLayout layout = 0;
        ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
        ASSERT_FALSE(TextLayoutHasMarkupShadow(layout));
        TextLayoutRelease(layout);
        MarkupDestroy(markup);
    }
}

TEST_F(FontTest, SkribidiLayoutResolvesMarkupOutlineColorOnly)
{
    const char source[] = "<outline color=#FF00FFFF>Color only</outline>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));

    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
    ASSERT_FALSE(TextLayoutHasMarkupOutline(layout));

    TextLayout* internal = (TextLayout*)layout;
    const TextRenderStyle& style = internal->m_Styles[TextLayoutGetGlyphs(layout)[0].m_StyleIndex];
    ASSERT_TRUE((style.m_Flags & TEXT_RENDER_STYLE_OUTLINE_COLOR) != 0);
    ASSERT_EQ(0u, style.m_Flags & TEXT_RENDER_STYLE_OUTLINE_WIDTH);

    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, SkribidiLayoutResolvesMarkupOutlineWidthOnly)
{
    const char source[] = "<outline size=1.5>Width only</outline>";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));

    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
    ASSERT_TRUE(TextLayoutHasMarkupOutline(layout));
    ASSERT_EQ(1.5f, TextLayoutGetMaxMarkupOutlineWidth(layout));

    TextLayout* internal = (TextLayout*)layout;
    const TextRenderStyle& style = internal->m_Styles[TextLayoutGetGlyphs(layout)[0].m_StyleIndex];
    ASSERT_TRUE((style.m_Flags & TEXT_RENDER_STYLE_OUTLINE_WIDTH) != 0);
    ASSERT_EQ(0u, style.m_Flags & TEXT_RENDER_STYLE_OUTLINE_COLOR);
    ASSERT_EQ(1.5f, style.m_OutlineWidth);

    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyphRenderData render_data;
    TextLayoutGetGlyphRenderData(layout, TextLayoutGetGlyphs(layout)[0], white, &render_data);
    ASSERT_EQ(1.0f, render_data.m_OutlineColor[0]);
    ASSERT_EQ(1.0f, render_data.m_OutlineColor[1]);
    ASSERT_EQ(1.0f, render_data.m_OutlineColor[2]);
    ASSERT_EQ(1.0f, render_data.m_OutlineColor[3]);

    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, SkribidiLayoutRecoversInvalidMarkupOutline)
{
    const char* invalid_outlines[] = {
        "<outline>Text</outline>",
        "<outline size=-1>Text</outline>",
        "<outline size=nan>Text</outline>",
        "<outline size=2px>Text</outline>",
        "<outline color=FF0000>Text</outline>",
        "<outline color=#GG0000>Text</outline>",
    };
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(invalid_outlines); ++i)
    {
        HMarkup markup = 0;
        ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(invalid_outlines[i], (uint32_t)strlen(invalid_outlines[i]), &markup, 0));
        HTextLayout layout = 0;
        ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
        ASSERT_FALSE(TextLayoutHasMarkupOutline(layout));
        TextLayoutRelease(layout);
        MarkupDestroy(markup);
    }
}

TEST_F(FontTest, SkribidiLayoutUsesMarkupFontSize)
{
    const char source[] =
    "A<size=24px>B</size><size=24>C</size><size=2em>D</size><size=120%>E</size>"
    "<size=+4>F</size><size=-4>G</size><size=2em><size value=\"120%\">H</size></size>"
    "<size value=20px>I</size>J";
    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));

    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));

    dmArray<uint32_t> plain_codepoints;
    TextToCodePoints("ABCDEFGHIJ", plain_codepoints);
    HTextLayout plain_layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreate(m_FontCollection, plain_codepoints.Begin(), plain_codepoints.Size(), &settings, &plain_layout));
    float markup_width;
    float markup_height;
    float plain_width;
    float plain_height;
    TextLayoutGetBounds(layout, &markup_width, &markup_height);
    TextLayoutGetBounds(plain_layout, &plain_width, &plain_height);
    (void)markup_width;
    (void)plain_width;
    ASSERT_GT(markup_height, plain_height);

    TextLayout* internal = (TextLayout*)layout;
    const float expected_sizes[] = { 32.0f, 24.0f, 24.0f, 64.0f, 38.4f, 36.0f, 28.0f, 38.4f, 20.0f, 32.0f };
    TextGlyph* glyphs = TextLayoutGetGlyphs(layout);
    for (uint32_t i = 0; i < TextLayoutGetGlyphCount(layout); ++i)
    {
        const TextRenderStyle& style = internal->m_Styles[glyphs[i].m_StyleIndex];
        ASSERT_LT(glyphs[i].m_Cluster, DM_ARRAY_SIZE(expected_sizes));
        ASSERT_NEAR(expected_sizes[glyphs[i].m_Cluster], style.m_FontSize, 0.0001f);
        ASSERT_NEAR(expected_sizes[glyphs[i].m_Cluster] / settings.m_Size, glyphs[i].m_RenderScale, 0.0001f);
        if (glyphs[i].m_Cluster > 0 && glyphs[i].m_Cluster < 9)
            ASSERT_TRUE((style.m_Flags & TEXT_RENDER_STYLE_FONT_SIZE) != 0);
        else
            ASSERT_EQ(0u, style.m_Flags & TEXT_RENDER_STYLE_FONT_SIZE);
    }

    TextLayoutRelease(plain_layout);
    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, SkribidiLayoutRecoversInvalidMarkupFontSize)
{
    const char* invalid_sizes[] = {
        "0", "0px", "0%", "0em", "-32", "-1%", "-1em", "24pt",
        "+", "-", "%", "px", "em", "nan", "inf", "1e39"
    };
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(invalid_sizes); ++i)
    {
        char source[64];
        snprintf(source, sizeof(source), "<size=%s>Text</size>", invalid_sizes[i]);
        HMarkup markup = 0;
        ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, (uint32_t)strlen(source), &markup, 0));
        HTextLayout layout = 0;
        ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
        ASSERT_EQ(1.0f, TextLayoutGetGlyphs(layout)[0].m_RenderScale);
        TextLayoutRelease(layout);
        MarkupDestroy(markup);
    }
}

TEST_F(FontTest, SkribidiLayoutUsesMarkupColor)
{
    const char source[] = "A<color=#00FF0080>B</color><color value=#FF0000>C</color><color value=\"#0000FF\">D</color>E";
    HMarkup    markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, sizeof(source) - 1, &markup, 0));

    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));

    TextLayout* internal = (TextLayout*)layout;
    ASSERT_EQ(4u, internal->m_Styles.Size());
    TextGlyph* glyphs = TextLayoutGetGlyphs(layout);
    for (uint32_t i = 0; i < TextLayoutGetGlyphCount(layout); ++i)
    {
        const TextRenderStyle& style = internal->m_Styles[glyphs[i].m_StyleIndex];
        if (glyphs[i].m_Cluster == 1)
        {
            ASSERT_TRUE((style.m_Flags & TEXT_RENDER_STYLE_FACE_COLOR) != 0);
            ASSERT_EQ(0.0f, style.m_FaceColor[0]);
            ASSERT_EQ(1.0f, style.m_FaceColor[1]);
            ASSERT_EQ(0.0f, style.m_FaceColor[2]);
            ASSERT_NEAR(128.0f / 255.0f, style.m_FaceColor[3], 0.0001f);
        }
        else if (glyphs[i].m_Cluster == 2)
        {
            ASSERT_TRUE((style.m_Flags & TEXT_RENDER_STYLE_FACE_COLOR) != 0);
            ASSERT_EQ(1.0f, style.m_FaceColor[0]);
            ASSERT_EQ(0.0f, style.m_FaceColor[1]);
            ASSERT_EQ(0.0f, style.m_FaceColor[2]);
            ASSERT_EQ(1.0f, style.m_FaceColor[3]);
        }
        else if (glyphs[i].m_Cluster == 3)
        {
            ASSERT_TRUE((style.m_Flags & TEXT_RENDER_STYLE_FACE_COLOR) != 0);
            ASSERT_EQ(0.0f, style.m_FaceColor[0]);
            ASSERT_EQ(0.0f, style.m_FaceColor[1]);
            ASSERT_EQ(1.0f, style.m_FaceColor[2]);
            ASSERT_EQ(1.0f, style.m_FaceColor[3]);
        }
        else
        {
            ASSERT_EQ(0u, style.m_Flags & TEXT_RENDER_STYLE_FACE_COLOR);
        }
    }

    TextLayoutRelease(layout);
    MarkupDestroy(markup);
}

TEST_F(FontTest, SkribidiLayoutRecoversColorWithoutHashPrefix)
{
    const char* invalid_colors[] = { "0x00FF00", "00FF00" };
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 32.0f;
    settings.m_Leading = 1.0f;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(invalid_colors); ++i)
    {
        char source[64];
        snprintf(source, sizeof(source), "<color=%s>Text</color>", invalid_colors[i]);
        HMarkup markup = 0;
        ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(source, (uint32_t)strlen(source), &markup, 0));
        HTextLayout layout = 0;
        ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(m_FontCollection, markup, &settings, &layout));
        AssertGlyphColor(layout, 0, 1.0f, 1.0f, 1.0f);
        TextLayoutRelease(layout);
        MarkupDestroy(markup);
    }
}

struct FontFallbackContext
{
    HFont       m_Font;
    const char* m_Language;
    uint32_t    m_Script;
    uint32_t    m_CallCount;
};

static bool AddFontFallback(HFontCollection collection, const char* language, uint32_t script, uint8_t font_family, void* context)
{
    (void)font_family;
    FontFallbackContext* fallback = (FontFallbackContext*)context;
    fallback->m_Language = language;
    fallback->m_Script = script;
    ++fallback->m_CallCount;
    return FontCollectionAddFont(collection, fallback->m_Font) == FONT_RESULT_OK;
}

struct MixedFontFallbackContext
{
    const char* m_ArabicLanguage;
    const char* m_JapaneseLanguage;
};

static bool CaptureMixedFontFallback(HFontCollection collection, const char* language, uint32_t script, uint8_t font_family, void* context)
{
    (void)collection;
    (void)font_family;
    MixedFontFallbackContext* fallback = (MixedFontFallbackContext*)context;
    if (script == ((uint32_t)'A' << 24 | (uint32_t)'r' << 16 | (uint32_t)'a' << 8 | (uint32_t)'b'))
        fallback->m_ArabicLanguage = language;
    else if (script == ((uint32_t)'H' << 24 | (uint32_t)'a' << 16 | (uint32_t)'n' << 8 | (uint32_t)'i'))
        fallback->m_JapaneseLanguage = language;
    return false;
}

TEST_F(FontTest, SkribidiSegmentsParagraphsBeforeLayout)
{
    dmArray<uint32_t> codepoints;
    TextToCodePoints("Hello\r\nالعربية\n日本語です", codepoints);

    dmArray<TextLayoutRun> runs;
    dmArray<TextParagraph> paragraphs;
    TextLayoutSegmentRuns(codepoints.Begin(), codepoints.Size(), "en-SE", runs, paragraphs);

    uint32_t offset = 0;
    bool     found_arabic = false;
    bool     found_japanese_han = false;
    bool     found_japanese_hiragana = false;
    for (uint32_t i = 0; i < runs.Size(); ++i)
    {
        ASSERT_EQ(offset, runs[i].m_Offset);
        offset += runs[i].m_Length;
        if (runs[i].m_Script == SBScriptARAB)
        {
            ASSERT_STREQ("ar", runs[i].m_Language);
            found_arabic = true;
        }
        else if (runs[i].m_Script == SBScriptHANI)
        {
            ASSERT_STREQ("ja", runs[i].m_Language);
            found_japanese_han = true;
        }
        else if (runs[i].m_Script == SBScriptHIRA)
        {
            ASSERT_STREQ("ja", runs[i].m_Language);
            found_japanese_hiragana = true;
        }
    }

    ASSERT_EQ(codepoints.Size(), offset);
    ASSERT_EQ(2u, runs[1].m_Length);
    ASSERT_TRUE(found_arabic);
    ASSERT_TRUE(found_japanese_han);
    ASSERT_TRUE(found_japanese_hiragana);
}

TEST_F(FontTest, SkribidiUsesLocaleForAmbiguousHanParagraph)
{
    dmArray<uint32_t> codepoints;
    TextToCodePoints("日本語", codepoints);

    dmArray<TextLayoutRun> runs;
    dmArray<TextParagraph> paragraphs;
    TextLayoutSegmentRuns(codepoints.Begin(), codepoints.Size(), "en-SE", runs, paragraphs);
    ASSERT_EQ(1u, runs.Size());
    ASSERT_STREQ("zh", runs[0].m_Language);

    TextLayoutSegmentRuns(codepoints.Begin(), codepoints.Size(), "ja-JP", runs, paragraphs);
    ASSERT_EQ(1u, runs.Size());
    ASSERT_STREQ("ja-JP", runs[0].m_Language);
}

TEST_F(FontTest, SkribidiDetectsMixedParagraphDirections)
{
    dmArray<uint32_t> codepoints;
    TextToCodePoints("English\n\nالعربية\n\n日本語です", codepoints);

    TextLayoutSettings settings = {0};
    settings.m_LineBreak = false;
    settings.m_Size = 28.0f;

    HTextLayout layout = 0;
    TextResult result = TestLayout(m_FontCollection, codepoints, &settings, &layout);
    ASSERT_EQ(TEXT_RESULT_OK, result);
    ASSERT_EQ(5u, TextLayoutGetParagraphCount(layout));

    TextParagraph* paragraphs = TextLayoutGetParagraphs(layout);
    ASSERT_EQ(TEXT_DIRECTION_LTR, paragraphs[0].m_Direction);
    ASSERT_EQ(TEXT_DIRECTION_LTR, paragraphs[1].m_Direction);
    ASSERT_EQ(TEXT_DIRECTION_RTL, paragraphs[2].m_Direction);
    ASSERT_EQ(TEXT_DIRECTION_RTL, paragraphs[3].m_Direction);
    ASSERT_EQ(TEXT_DIRECTION_LTR, paragraphs[4].m_Direction);

    for (uint32_t i = 0; i < layout->m_Lines.Size(); ++i)
        ASSERT_EQ(i, layout->m_Lines[i].m_ParagraphIndex);

    TextLayoutRelease(layout);
}

TEST_F(FontTest, SkribidiPassesLanguagesFromMixedParagraphsToFallback)
{
    MixedFontFallbackContext fallback = {};
    FontCollectionSetFallbackCallback(m_FontCollection, CaptureMixedFontFallback, &fallback);

    dmArray<uint32_t> codepoints;
    TextToCodePoints("English\n\nالعربية\n\n日本語です", codepoints);
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 24.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TestLayout(m_FontCollection, codepoints, &settings, &layout));
    ASSERT_NE((HTextLayout)0, layout);
    ASSERT_STREQ("ar", fallback.m_ArabicLanguage);
    ASSERT_STREQ("ja", fallback.m_JapaneseLanguage);

    TextLayoutRelease(layout);
    FontCollectionSetFallbackCallback(m_FontCollection, 0, 0);
}

TEST_F(FontTest, SkribidiLayoutHeightMatchesLegacyLineHeight)
{
    dmArray<uint32_t> codepoints;
    TextToCodePoints("A\nA\nA\nA\nA\nA\nA\nA\nA\nA", codepoints);

    TextLayoutSettings settings = {0};
    settings.m_LineBreak = false;
    settings.m_Width = 0.0f;
    settings.m_Size = 40.0f;
    settings.m_Leading = 1.0f;

    HTextLayout legacy_layout = 0;
    TextResult r = TextLayoutLegacyCreate(m_FontCollection, codepoints.Begin(), codepoints.Size(), &settings, &legacy_layout);
    ASSERT_EQ(TEXT_RESULT_OK, r);
    ASSERT_NE((HTextLayout)0, legacy_layout);

    HTextLayout skribidi_layout = 0;
    r = TestLayout(m_FontCollection, codepoints, &settings, &skribidi_layout);
    ASSERT_EQ(TEXT_RESULT_OK, r);
    ASSERT_NE((HTextLayout)0, skribidi_layout);

    ASSERT_EQ(10u, skribidi_layout->m_Lines.Size());
    ASSERT_EQ(legacy_layout->m_Lines.Size(), skribidi_layout->m_Lines.Size());
    ASSERT_NEAR(legacy_layout->m_Height, skribidi_layout->m_Height, 0.01f);

    TextLayoutRelease(skribidi_layout);
    TextLayoutRelease(legacy_layout);
}

TEST_F(FontTest, SkribidiLoadsFontFallback)
{
    HFont arabic_font;
    LoadFont("src/test/data/NotoSansArabic-Regular.ttf", &arabic_font);
    FontFallbackContext fallback = { arabic_font, 0, 0, 0 };
    FontCollectionSetFallbackCallback(m_FontCollection, AddFontFallback, &fallback);

    dmArray<uint32_t> codepoints;
    TextToCodePoints(g_TextArabic, codepoints);
    TextLayoutSettings settings = {};
    settings.m_Width = 1000.0f;
    settings.m_Size = 24.0f;
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TestLayout(m_FontCollection, codepoints, &settings, &layout));
    ASSERT_NE((HTextLayout)0, layout);
    ASSERT_GT(fallback.m_CallCount, 0u);
    ASSERT_STREQ("ar", fallback.m_Language);
    ASSERT_EQ((uint32_t)'A' << 24 | (uint32_t)'r' << 16 | (uint32_t)'a' << 8 | (uint32_t)'b', fallback.m_Script);
    ASSERT_EQ(2u, FontCollectionGetFontCount(m_FontCollection));
    ASSERT_EQ(arabic_font, TextLayoutGetGlyphs(layout)[0].m_Font);

    TextLayoutRelease(layout);
    FontCollectionSetFallbackCallback(m_FontCollection, 0, 0);
    FontCollectionRemoveFont(m_FontCollection, arabic_font);
    FontDestroy(arabic_font);
}

TEST_F(FontTest, TextArabic)
{
    HFont font;
    LoadFont("src/test/data/NotoSansArabic-Regular.ttf", &font);

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
    TextLayoutRelease(layout);
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
