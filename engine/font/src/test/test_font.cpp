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

#if defined(FONT_USE_SKRIBIDI)
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
