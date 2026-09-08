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

#include <limits.h>
#include <string.h>

#include <jc_test/jc_test.h>

#include <dlib/hash.h>

#include <font/fontcollection.h>
#include <font/font_glyphbank.h>
#include <font/text_layout.h>

struct TestGlyphBankProvider
{
    FontGlyphBankProvider m_Provider;
    FontGlyphBankGlyph*   m_Glyphs;
    bool*                 m_Destroyed;
};

static uint32_t TestGlyphBankGetCodepoint(void* context, uint32_t glyph_index)
{
    return ((TestGlyphBankProvider*)context)->m_Glyphs[glyph_index].m_Codepoint;
}

static bool TestGlyphBankGetGlyph(void* context, uint32_t glyph_index, FontGlyphBankGlyph* output)
{
    *output = ((TestGlyphBankProvider*)context)->m_Glyphs[glyph_index];
    return true;
}

static void TestGlyphBankDestroy(void* context)
{
    TestGlyphBankProvider* glyph_bank = (TestGlyphBankProvider*)context;
    *glyph_bank->m_Destroyed = true;
    delete[] glyph_bank->m_Glyphs;
    delete glyph_bank;
}

static TestGlyphBankProvider* CreateTestGlyphBank(uint32_t glyph_count, bool* destroyed)
{
    TestGlyphBankProvider* glyph_bank = new TestGlyphBankProvider;
    memset(glyph_bank, 0, sizeof(*glyph_bank));
    glyph_bank->m_Glyphs = new FontGlyphBankGlyph[glyph_count];
    memset(glyph_bank->m_Glyphs, 0, sizeof(FontGlyphBankGlyph) * glyph_count);
    glyph_bank->m_Destroyed = destroyed;
    glyph_bank->m_Provider.m_Context = glyph_bank;
    glyph_bank->m_Provider.m_GetCodepoint = TestGlyphBankGetCodepoint;
    glyph_bank->m_Provider.m_GetGlyph = TestGlyphBankGetGlyph;
    glyph_bank->m_Provider.m_Destroy = TestGlyphBankDestroy;
    glyph_bank->m_Provider.m_ResourceSize = sizeof(*glyph_bank) + sizeof(FontGlyphBankGlyph) * glyph_count;
    glyph_bank->m_Provider.m_GlyphCount = glyph_count;
    return glyph_bank;
}

static void AssertGlyphBankLayout(HFontCollection     collection,
                                  const char*         text,
                                  TextLayoutSettings* settings,
                                  float               expected_width,
                                  float               expected_height,
                                  uint32_t            expected_line_count)
{
    dmArray<uint32_t> codepoints;
    TextToCodePoints(text, codepoints);

    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK,
              TextLayoutCreate(collection, codepoints.Begin(), codepoints.Size(), settings, &layout));
    ASSERT_NE((HTextLayout)0, layout);

    float width;
    float height;
    TextLayoutGetBounds(layout, &width, &height);
    ASSERT_EQ(expected_width, width);
    ASSERT_EQ(expected_height, height);
    ASSERT_EQ(expected_line_count, TextLayoutGetLineCount(layout));
    TextLayoutRelease(layout);
}

TEST(FontGlyphBank, LookupMetricsBitmapAndOwnership)
{
    bool                   destroyed = false;
    TestGlyphBankProvider* glyph_bank = CreateTestGlyphBank(3, &destroyed);
    const uint8_t          compressed_bitmap[] = { 10, 20, 30, 40 };
    const uint8_t          uncompressed_bitmap[] = { 50, 60 };
    const uint32_t         codepoints[] = { 32, 65, 0x1f642 };
    for (uint32_t i = 0; i < 3; ++i)
    {
        glyph_bank->m_Glyphs[i].m_Codepoint = codepoints[i];
        glyph_bank->m_Glyphs[i].m_Width = 2.0f + i;
        glyph_bank->m_Glyphs[i].m_Advance = 3.0f + i;
        glyph_bank->m_Glyphs[i].m_LeftBearing = 1.0f;
        glyph_bank->m_Glyphs[i].m_Ascent = 4.0f;
        glyph_bank->m_Glyphs[i].m_Descent = 1.0f;
    }
    glyph_bank->m_Glyphs[0].m_Data = uncompressed_bitmap;
    glyph_bank->m_Glyphs[0].m_DataSize = sizeof(uncompressed_bitmap);
    glyph_bank->m_Glyphs[1].m_Data = compressed_bitmap;
    glyph_bank->m_Glyphs[1].m_DataSize = sizeof(compressed_bitmap);
    glyph_bank->m_Glyphs[1].m_BitmapFlags = FONT_GLYPH_BM_FLAG_COMPRESSION_DEFLATE;
    glyph_bank->m_Provider.m_GlyphPadding = 2;
    glyph_bank->m_Provider.m_GlyphChannels = 1;
    glyph_bank->m_Provider.m_MaxAscent = 7.0f;
    glyph_bank->m_Provider.m_MaxDescent = 2.0f;

    HFont font = FontCreateGlyphBank("test.glyph_bankc", &glyph_bank->m_Provider);
    ASSERT_NE((HFont)0, font);
    ASSERT_EQ(FONT_TYPE_GLYPH_BANK, FontGetType(font));
    ASSERT_STREQ("test.glyph_bankc", FontGetPath(font));
    ASSERT_EQ(dmHashString32("test.glyph_bankc"), FontGetPathHash(font));
    ASSERT_EQ(1U, FontGetGlyphIndex(font, 32));
    ASSERT_EQ(2U, FontGetGlyphIndex(font, 65));
    ASSERT_EQ(3U, FontGetGlyphIndex(font, 0x1f642));
    ASSERT_EQ(0U, FontGetGlyphIndex(font, 66));
    ASSERT_EQ(7.0f, FontGetAscent(font, 1.0f));
    ASSERT_EQ(2.0f, FontGetDescent(font, 1.0f));
    ASSERT_EQ(0.0f, FontGetLineGap(font, 1.0f));
    ASSERT_EQ(1.0f, FontGetScaleFromSize(font, 42.0f));

    const uint32_t provider_resource_size = (uint32_t)(sizeof(*glyph_bank) +
                                                        sizeof(FontGlyphBankGlyph) * 3);
    ASSERT_EQ(provider_resource_size, (uint32_t)glyph_bank->m_Provider.m_ResourceSize);
    const uint32_t resource_size = FontGetResourceSize(font);
    ASSERT_GT(resource_size, provider_resource_size);
    glyph_bank->m_Provider.m_ResourceSize += 17;
    ASSERT_EQ(resource_size + 17, FontGetResourceSize(font));
    glyph_bank->m_Provider.m_ResourceSize -= 17;

    FontGlyphOptions options = {};
    options.m_GenerateImage = true;
    FontGlyph glyph;
    ASSERT_EQ(FONT_RESULT_OK, FontGetGlyphByIndex(font, 1, &options, &glyph));
    ASSERT_EQ(uncompressed_bitmap, glyph.m_Bitmap.m_Data);
    ASSERT_EQ((uint32_t)sizeof(uncompressed_bitmap), glyph.m_Bitmap.m_DataSize);
    ASSERT_EQ(FONT_GLYPH_BM_FLAG_DATA_IS_BORROWED, glyph.m_Bitmap.m_Flags);

    ASSERT_EQ(FONT_RESULT_OK, FontGetGlyphByIndex(font, 2, &options, &glyph));
    ASSERT_EQ(65U, glyph.m_Codepoint);
    ASSERT_EQ(3.0f, glyph.m_Width);
    ASSERT_EQ(5.0f, glyph.m_Height);
    ASSERT_EQ(compressed_bitmap, glyph.m_Bitmap.m_Data);
    ASSERT_EQ((uint32_t)sizeof(compressed_bitmap), glyph.m_Bitmap.m_DataSize);
    ASSERT_EQ(7U, glyph.m_Bitmap.m_Width);
    ASSERT_EQ(9U, glyph.m_Bitmap.m_Height);
    ASSERT_EQ(1U, glyph.m_Bitmap.m_Channels);
    ASSERT_EQ(FONT_GLYPH_BM_FLAG_COMPRESSION_DEFLATE | FONT_GLYPH_BM_FLAG_DATA_IS_BORROWED, glyph.m_Bitmap.m_Flags);
    ASSERT_EQ(FONT_RESULT_ERROR, FontGetGlyphByIndex(font, 0, &options, &glyph));
    ASSERT_EQ(FONT_RESULT_ERROR, FontGetGlyphByIndex(font, 4, &options, &glyph));

    FontDestroy(font);
    ASSERT_TRUE(destroyed);
}

TEST(FontGlyphBank, EmptyRefreshAndResourceSizeSaturation)
{
    FontGlyphBankGlyph    glyphs[1] = {};
    TestGlyphBankProvider glyph_bank = {};
    glyph_bank.m_Glyphs = glyphs;
    glyph_bank.m_Provider.m_Context = &glyph_bank;
    glyph_bank.m_Provider.m_GetCodepoint = TestGlyphBankGetCodepoint;
    glyph_bank.m_Provider.m_GetGlyph = TestGlyphBankGetGlyph;
    glyph_bank.m_Provider.m_ResourceSize = UINT64_MAX;

    HFont font = FontCreateGlyphBank("refresh.glyph_bankc", &glyph_bank.m_Provider);
    ASSERT_NE((HFont)0, font);
    ASSERT_EQ(0U, FontGetGlyphIndex(font, 65));
    ASSERT_EQ(UINT32_MAX, FontGetResourceSize(font));

    glyphs[0].m_Codepoint = 65;
    glyphs[0].m_Ascent = 3.0f;
    glyphs[0].m_Descent = 1.0f;
    glyph_bank.m_Provider.m_GlyphCount = 1;
    glyph_bank.m_Provider.m_MaxAscent = 3.0f;
    glyph_bank.m_Provider.m_MaxDescent = 1.0f;
    ASSERT_EQ(1U, FontGetGlyphIndex(font, 65));
    ASSERT_EQ(3.0f, FontGetAscent(font, 1.0f));
    ASSERT_EQ(1.0f, FontGetDescent(font, 1.0f));

    FontDestroy(font);
}

TEST(FontGlyphBank, LayoutMetrics)
{
    bool                   destroyed = false;
    TestGlyphBankProvider* glyph_bank = CreateTestGlyphBank(128, &destroyed);
    for (uint32_t i = 0; i < glyph_bank->m_Provider.m_GlyphCount; ++i)
    {
        glyph_bank->m_Glyphs[i].m_Codepoint = i;
        glyph_bank->m_Glyphs[i].m_Width = 1.0f;
        glyph_bank->m_Glyphs[i].m_Advance = 2.0f;
        glyph_bank->m_Glyphs[i].m_LeftBearing = 1.0f;
        glyph_bank->m_Glyphs[i].m_Ascent = 2.0f;
        glyph_bank->m_Glyphs[i].m_Descent = 1.0f;
    }
    glyph_bank->m_Provider.m_MaxAscent = 2.0f;
    glyph_bank->m_Provider.m_MaxDescent = 1.0f;

    HFont font = FontCreateGlyphBank("layout.glyph_bankc", &glyph_bank->m_Provider);
    ASSERT_NE((HFont)0, font);
    HFontCollection collection = FontCollectionCreate();
    ASSERT_EQ(FONT_RESULT_OK, FontCollectionAddFont(collection, font));

    TextLayoutSettings settings = {};
    settings.m_Leading = 1.0f;
    AssertGlyphBankLayout(collection, "Hello World", &settings, 22.0f, 3.0f, 1);

    settings.m_Width = 16.0f;
    settings.m_LineBreak = true;
    AssertGlyphBankLayout(collection, "Hello World", &settings, 10.0f, 6.0f, 2);

    const float leadings[] = { 1.0f, 2.0f, 0.5f };
    const float expected_heights[] = { 9.0f, 15.0f, 6.0f };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(leadings); ++i)
    {
        settings.m_Leading = leadings[i];
        AssertGlyphBankLayout(collection, "Hello World Bonanza", &settings,
                              14.0f, expected_heights[i], 3);
    }

    settings.m_Leading = 0.0f;
    AssertGlyphBankLayout(collection, "Hello World", &settings, 10.0f, 3.0f, 2);

    FontCollectionDestroy(collection);
    FontDestroy(font);
    ASSERT_TRUE(destroyed);
}

TEST(FontGlyphBank, RejectsInvalidProvider)
{
    FontGlyphBankProvider provider = {};
    ASSERT_EQ((HFont)0, FontCreateGlyphBank("test.glyph_bankc", &provider));
    ASSERT_EQ((HFont)0, FontCreateGlyphBank(0, &provider));
    ASSERT_EQ((HFont)0, FontCreateGlyphBank("test.glyph_bankc", 0));
}
