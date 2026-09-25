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
#include <dlib/testutil.h>
#include <math.h>
#include <dmsdk/font/font.h>
#include "../font.h"
#include "../font_vector_slug.h"
#include "../font_vector_paint.hpp"
#include "../render/glyph_gen.h"

TEST(FontVector, ColorPaintCacheFallbackAndReuse)
{
    char path[512];
    HFont font = FontLoadFromPath(dmTestUtil::MakeHostPath(path, sizeof(path), "src/test/data/NotoColorEmoji-Regular.ttf"));
    ASSERT_NE((HFont)0, font);
    FontVectorSlugData data;
    FontVectorPaintCache cache;
    cache.m_Slug = &data;
    const uint32_t glyphs[] = { FontGetGlyphIndex(font, 0x1f600), 2017, 2688 };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(glyphs); ++i)
    {
        uint32_t index;
        bool color;
        ASSERT_TRUE(FontVectorPaintAddGlyph(&cache, font, glyphs[i], &index, &color));
        ASSERT_TRUE(color);
        const FontVectorPaintGlyph& glyph = cache.m_Glyphs[index];
        if (i == 0)
            ASSERT_GT(glyph.m_LayerCount, 1u);
        else
        {
            ASSERT_EQ(1u, glyph.m_LayerCount);
            const FontVectorPaintLayer& layer = cache.m_Layers[glyph.m_FirstLayer];
            ASSERT_EQ(0u, cache.m_Paths[layer.m_Path].m_Glyph);
            ASSERT_EQ(0.0f, cache.m_Paints[layer.m_Paint].getX()); // Solid fallback, no partial graph.
        }
        uint32_t curves = data.m_Curves.Size(), layers = cache.m_Layers.Size(), paints = cache.m_Paints.Size();
        uint32_t again;
        ASSERT_TRUE(FontVectorPaintAddGlyph(&cache, font, glyphs[i], &again, &color));
        ASSERT_EQ(index, again);
        ASSERT_EQ(curves, data.m_Curves.Size());
        ASSERT_EQ(layers, cache.m_Layers.Size());
        ASSERT_EQ(paints, cache.m_Paints.Size());
    }
    // Atlas exhaustion must preserve previously submitted glyphs and metadata.
    uint32_t curves = data.m_Curves.Size(), bands = data.m_Bands.Size();
    uint32_t layers = cache.m_Layers.Size(), paths = cache.m_Paths.Size();
    uint32_t count = data.m_CurveCount, index;
    bool color;
    data.m_MaxTexels = bands;
    ASSERT_FALSE(FontVectorPaintAddGlyph(&cache, font, FontGetGlyphIndex(font, 0x1f680), &index, &color));
    ASSERT_TRUE(data.m_Overflow);
    ASSERT_EQ(curves, data.m_Curves.Size());
    ASSERT_EQ(bands, data.m_Bands.Size());
    ASSERT_EQ(layers, cache.m_Layers.Size());
    ASSERT_EQ(paths, cache.m_Paths.Size());
    ASSERT_EQ(count, data.m_CurveCount);
    FontVectorPaintClear(&cache);
    ASSERT_EQ(0u, cache.m_GlyphLookup.Size());
    ASSERT_EQ(0u, cache.m_PathLookup.Size());
    FontDestroy(font);
}

TEST(FontVector, CpuEffectsPreserveFaceCurvesAndOutlineDistanceField)
{
    char path[512];
    HFont font = FontLoadFromPath(dmTestUtil::MakeHostPath(path, sizeof(path), "src/test/data/NotoSans-Regular.ttf"));
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
        char path[512];
        HFont font = FontLoadFromPath(dmTestUtil::MakeHostPath(path, sizeof(path), paths[f]));
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
    char path[512];
    HFont font = FontLoadFromPath(dmTestUtil::MakeHostPath(path, sizeof(path), "src/test/data/NotoSans-Regular.ttf"));
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
