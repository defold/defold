// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0 (https://www.defold.com/license).
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dlib/array.h>
#include <dlib/sys.h>
#include <math.h>
#include <dmsdk/font/font.h>
#include "../font.h"
#include "../font_vector_slug.h"
#include "../render/glyph_gen.h"

TEST(FontVector, CpuEffectsPreserveFaceCurvesAndOutlineDistanceField)
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
        FontGlyphOptions options;
        options.m_Scale = params.m_Scale;
        options.m_GenerateImage = true;
        options.m_GenerateOutline = true;
        options.m_StbttSDFPadding = params.m_OutlineWidth + ceilf(3.0f * params.m_ShadowBlur) + 3.0f;
        options.m_StbttSDFOnEdgeValue = params.m_SdfEdgeValue;
        FontGlyph sdf;
        ASSERT_EQ(FONT_RESULT_OK, FontGetGlyphByIndex(font, glyph_index, &options, &sdf));
        ASSERT_EQ(glyph.m_Bitmap.m_Width, sdf.m_Bitmap.m_Width);
        ASSERT_EQ(glyph.m_Bitmap.m_Height, sdf.m_Bitmap.m_Height);
        const float outline_edge = params.m_SdfEdgeValue * (1.0f - params.m_OutlineWidth / options.m_StbttSDFPadding);
        uint32_t outline_only = 0, shadow_only = 0;
        for (uint32_t p = 0; p < glyph.m_Bitmap.m_DataSize; p += 3)
        {
            const uint8_t* pixel = glyph.m_Bitmap.m_Data + p;
            ASSERT_EQ(sdf.m_Bitmap.m_Data[p / 3], pixel[1]);
            outline_only += pixel[1] > outline_edge && pixel[0] == 0;
            shadow_only += pixel[1] < outline_edge && pixel[2] > 0;
            if (i == 0) ASSERT_GE(pixel[2], pixel[0]);
        }
        ASSERT_GT(outline_only, 0u);
        if (i > 0) ASSERT_GT(shadow_only, 0u);
        FontFreeGlyph(font, &sdf);
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

TEST(FontVector, EffectsPreserveEncodedFaceCurves)
{
    uint32_t size = 0;
    const char* path = "src/test/data/NotoSans-Regular.ttf";
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::ResourceSize(path, &size));
    dmArray<uint8_t> bytes;
    bytes.SetCapacity(size);
    bytes.SetSize(size);
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::LoadResource(path, bytes.Begin(), size, &size));
    HFont font = FontLoadFromMemory(path, bytes.Begin(), size, false);
    ASSERT_NE((HFont)0, font);
    FontVectorSlugData face;
    FontVectorSlugData effects;
    for (const char* c = "Example"; *c; ++c)
    {
        FontGlyphGenParams params;
        params.m_Scale = FontGetScaleFromSize(font, 36);
        for (uint32_t pass = 0; pass < 2; ++pass)
        {
            params.m_HasOutline = params.m_HasShadow = pass != 0;
            params.m_OutlineWidth = 2;
            params.m_ShadowBlur = 2;
            FontGlyph glyph;
            ASSERT_EQ(FONT_RESULT_OK, FontGenerateVectorGlyph(font, FontGetGlyphIndex(font, *c), &params, &glyph));
            FontVectorSlugGlyph encoded;
            ASSERT_TRUE(FontVectorSlugAddFontGlyph(pass == 0 ? &face : &effects, glyph, 8, &encoded));
            FontFreeGlyph(font, &glyph);
        }
        ASSERT_EQ(face.m_Curves.Size(), effects.m_Curves.Size());
        ASSERT_EQ(0, memcmp(face.m_Curves.Begin(), effects.m_Curves.Begin(), face.m_Curves.Size() * sizeof(uint16_t)));
        ASSERT_EQ(face.m_Bands.Size(), effects.m_Bands.Size());
        ASSERT_EQ(0, memcmp(face.m_Bands.Begin(), effects.m_Bands.Begin(), face.m_Bands.Size() * sizeof(uint32_t)));
    }
    FontDestroy(font);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
