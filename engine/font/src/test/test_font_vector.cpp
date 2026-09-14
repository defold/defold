// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0 (https://www.defold.com/license).
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dlib/array.h>
#include <dlib/sys.h>
#include <dmsdk/font/font.h>
#include "../font.h"
#include "../font_vector_slug.h"
#include "../render/glyph_gen.h"

TEST(FontVector, CpuBitmapsPreserveFaceCurvesAndSeparateEffectChannels)
{
    uint32_t size = 0;
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::ResourceSize("src/test/data/NotoSans-Regular.ttf", &size));
    dmArray<uint8_t> bytes;
    bytes.SetCapacity(size); bytes.SetSize(size);
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::LoadResource("src/test/data/NotoSans-Regular.ttf", bytes.Begin(), size, &size));
    HFont font = FontLoadFromMemory("NotoSans-Regular.ttf", bytes.Begin(), size, false);
    ASSERT_NE((HFont)0, font);
    const uint32_t glyph_index = FontGetGlyphIndex(font, 'A');
    uint32_t curve_count = 0;
    for (uint32_t i = 0; i < 3; ++i)
    {
        FontGlyphGenParams params;
        params.m_Scale = FontGetScaleFromSize(font, 48);
        params.m_OutlineWidth = 4.0f;
        params.m_ShadowBlur = i == 0 ? 0.0f : i == 1 ? 4.0f : 16.0f;
        params.m_HasOutline = true;
        params.m_HasShadow = true;
        FontGlyph glyph;
        ASSERT_EQ(FONT_RESULT_OK, FontGenerateVectorGlyph(font, glyph_index, &params, &glyph));
        ASSERT_EQ(3u, glyph.m_Bitmap.m_Channels);
        ASSERT_EQ((uint32_t)glyph.m_Bitmap.m_Width * glyph.m_Bitmap.m_Height * 3, glyph.m_Bitmap.m_DataSize);
        FontVectorSlugData data;
        FontVectorSlugGlyph encoded;
        ASSERT_TRUE(FontVectorSlugAddFontGlyph(&data, glyph, 8, &encoded));
        ASSERT_GT(encoded.m_CurveCount, 0u);
        if (i == 0) curve_count = encoded.m_CurveCount;
        ASSERT_EQ(curve_count, encoded.m_CurveCount);
        uint32_t outline_only = 0, shadow_only = 0;
        for (uint32_t p = 0; p < glyph.m_Bitmap.m_DataSize; p += 3)
        {
            const uint8_t* pixel = glyph.m_Bitmap.m_Data + p;
            ASSERT_GE(pixel[1], pixel[0]);
            outline_only += pixel[1] > pixel[0];
            shadow_only += pixel[1] == 0 && pixel[2] > 0;
            if (i == 0) ASSERT_EQ(pixel[1], pixel[2]);
        }
        ASSERT_GT(outline_only, 0u);
        if (i > 0) ASSERT_GT(shadow_only, 0u);
        FontFreeGlyph(font, &glyph);
    }
    FontDestroy(font);
}

TEST(FontVector, CubicOpenTypeOutlinesGenerateSizeIndependentVectorCurves)
{
    const char* paths[] = { "src/test/data/SourceCodePro-Regular.otf", "src/test/data/SourceSerif4Variable-Roman_cff2.otf" };
    for (uint32_t f = 0; f < DM_ARRAY_SIZE(paths); ++f)
    {
        uint32_t size = 0;
        ASSERT_EQ(dmSys::RESULT_OK, dmSys::ResourceSize(paths[f], &size));
        dmArray<uint8_t> bytes;
        bytes.SetCapacity(size);
        bytes.SetSize(size);
        ASSERT_EQ(dmSys::RESULT_OK, dmSys::LoadResource(paths[f], bytes.Begin(), size, &size));
        HFont font = FontLoadFromMemory(paths[f], bytes.Begin(), size, false);
        ASSERT_NE((HFont)0, font);
        const char* text = "Example";
        for (const char* c = text; *c; ++c)
        {
            uint32_t curve_count = 0;
            for (uint32_t pass = 0; pass < 2; ++pass)
            {
                FontGlyphGenParams params;
                params.m_Scale = FontGetScaleFromSize(font, pass == 0 ? 16 : 36);
                params.m_HasOutline = pass != 0;
                params.m_HasShadow = pass != 0;
                params.m_OutlineWidth = 2;
                params.m_ShadowBlur = 2;
                FontGlyph glyph;
                ASSERT_EQ(FONT_RESULT_OK, FontGenerateVectorGlyph(font, FontGetGlyphIndex(font, *c), &params, &glyph));
                FontVectorSlugData data;
                FontVectorSlugGlyph encoded;
                ASSERT_TRUE(FontVectorSlugAddFontGlyph(&data, glyph, 8, &encoded));
                ASSERT_GT(encoded.m_CurveCount, 0u);
                if (pass == 0)
                    curve_count = encoded.m_CurveCount;
                else
                {
                    ASSERT_EQ(curve_count, encoded.m_CurveCount);
                    ASSERT_EQ(3u, glyph.m_Bitmap.m_Channels);
                    ASSERT_GT(glyph.m_Bitmap.m_DataSize, 0u);
                }
                FontFreeGlyph(font, &glyph);
            }
        }
        FontDestroy(font);
    }
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
