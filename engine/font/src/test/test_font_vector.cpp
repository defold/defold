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
#include <float.h>
#include <dlib/memory.h>
#include <string.h>
#include <dmsdk/font/font.h>
#include <dmsdk/font/fontcollection.h>
#include "../font.h"
#include "../font_vector_slug.h"
#include "../font_vector_paint.hpp"
#include "../render/glyph_gen.h"

static uint32_t ReadFontU32(const uint8_t* p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static uint8_t* FindFontTable(uint8_t* data, const char* tag)
{
    uint32_t count = (uint32_t)data[4] << 8 | data[5];
    for (uint32_t i = 0; i < count; ++i)
    {
        uint8_t* record = data + 12 + i * 16;
        if (memcmp(record, tag, 4) == 0)
            return data + ReadFontU32(record + 8);
    }
    return 0;
}

// Both layout libraries must measure paint outlines when the base glyph is
// empty, for COLRv0 as well as COLRv1 without its optional ClipList.
TEST(FontVector, ColorBoundsWithoutBaseOutline)
{
    char path[512];
    dmTestUtil::MakeHostPath(path, sizeof(path), "src/test/data/color-test.ttf");
    for (uint32_t version = 0; version < 3; ++version)
    {
        uint32_t size;
        uint8_t* data = dmTestUtil::ReadFile(path, &size);
        ASSERT_NE((uint8_t*)0, data);
        uint8_t* loca = FindFontTable(data, "loca");
        uint8_t* colr = FindFontTable(data, "COLR");
        ASSERT_NE((uint8_t*)0, loca);
        ASSERT_NE((uint8_t*)0, colr);
        memcpy(loca + 4, loca + 8, 4); // Glyph A has no raw outline.
        if (version == 0)
        {
            const uint8_t v0[] = { 0, 0, 0, 1, 0, 0, 0, 14, 0, 0, 0, 20, 0, 1, 0, 1, 0, 0, 0, 1, 0, 3, 0, 1 };
            memcpy(colr, v0, sizeof(v0)); // A paints C's square outline.
        }
        else if (version == 1)
            memset(colr + 22, 0, 4); // Optional ClipList.
        else
        {
            uint8_t* base = colr + ReadFontU32(colr + 14);
            uint8_t* paint = base + ReadFontU32(base + 6);
            paint[0] = 11;
            paint[1] = 0;
            paint[2] = 2; // A paints B, whose clip halves its width.
        }
        HFont font = FontLoadFromMemory("clipless.ttf", data, size, false);
        ASSERT_NE((HFont)0, font);
        HFontCollection collection = FontCollectionCreate();
        ASSERT_EQ(FONT_RESULT_OK, FontCollectionAddFont(collection, font));
        TextLayoutSettings settings = {};
        settings.m_Size = 32;
        settings.m_Leading = 1;
        uint32_t    text = 'A';
        HTextLayout layout = 0;
        ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreate(collection, &text, 1, &settings, &layout));
        ASSERT_EQ(1u, TextLayoutGetGlyphCount(layout));
        const TextGlyph& glyph = TextLayoutGetGlyphs(layout)[0];
        // Legacy layout includes its default three font-unit padding per side.
        float width = version == 2 ? 500.0f : 1000.0f;
        float padding = FontGetLayoutType(font) == TEXT_LAYOUT_TYPE_LEGACY ? 6.0f : 0.0f;
        ASSERT_NEAR((width + padding) * .032f, glyph.m_Width, .0001f);
        ASSERT_NEAR((1000.0f + padding) * .032f, glyph.m_Height, .0001f);
        FontVectorSlugData   slug;
        FontVectorPaintCache cache;
        cache.m_Slug = &slug;
        uint32_t index;
        bool     color;
        ASSERT_TRUE(FontVectorPaintAddGlyph(&cache, font, glyph.m_GlyphIndex, &index, &color));
        ASSERT_TRUE(color);
        ASSERT_EQ(width, cache.m_Glyphs[index].m_Bounds.getZ());
        ASSERT_EQ(1000.0f, cache.m_Glyphs[index].m_Bounds.getW());
        TextLayoutRelease(layout);
        FontCollectionDestroy(collection);
        FontDestroy(font);
        dmMemory::AlignedFree(data);
    }
}

// Color paint caches reuse complete graphs and roll back unsupported or overflowing glyphs.
TEST(FontVector, ColorPaintCacheFallbackAndReuse)
{
    char  path[512];
    HFont font = FontLoadFromPath(dmTestUtil::MakeHostPath(path, sizeof(path), "src/test/data/NotoColorEmoji-Regular.ttf"));
    ASSERT_NE((HFont)0, font);
    FontVectorSlugData   data;
    FontVectorPaintCache cache;
    cache.m_Slug = &data;
    const uint32_t glyphs[] = { FontGetGlyphIndex(font, 0x1f600), 2017, 2688 };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(glyphs); ++i)
    {
        uint32_t index;
        bool     color;
        ASSERT_TRUE(FontVectorPaintAddGlyph(&cache, font, glyphs[i], &index, &color));
        ASSERT_TRUE(color);
        const FontVectorPaintGlyph& glyph = cache.m_Glyphs[index];
        if (i == 0)
            ASSERT_GT(glyph.m_LayerCount, 1u);
        else
        {
            ASSERT_EQ(1u, glyph.m_LayerCount);
            bool notdef = false;
            for (uint32_t p = 0; p < cache.m_Paths.Size(); ++p)
                if (cache.m_Paths[p].m_Glyph == 0)
                    notdef = cache.m_Paths[p].m_Slug.m_BandTexel == (uint32_t)cache.m_Paints[glyph.m_Paint + 1].getW();
            ASSERT_TRUE(notdef);
            uint32_t paint = (uint32_t)cache.m_Paints[glyph.m_Paint + 2].getW();
            ASSERT_EQ(0.0f, cache.m_Paints[paint].getX()); // Solid fallback, no partial graph.
        }
        ASSERT_EQ(0u, cache.m_LayerScratch.Size());
        uint32_t curves = data.m_Curves.Size(), paints = cache.m_Paints.Size();
        uint32_t again;
        ASSERT_TRUE(FontVectorPaintAddGlyph(&cache, font, glyphs[i], &again, &color));
        ASSERT_EQ(index, again);
        ASSERT_EQ(curves, data.m_Curves.Size());
        ASSERT_EQ(0u, cache.m_LayerScratch.Size());
        ASSERT_EQ(paints, cache.m_Paints.Size());
    }
    // Atlas exhaustion must preserve previously submitted glyphs and metadata.
    uint32_t curves = data.m_Curves.Size(), bands = data.m_Bands.Size();
    uint32_t paths = cache.m_Paths.Size();
    uint32_t count = data.m_CurveCount, index;
    bool     color;
    data.m_MaxTexels = bands;
    ASSERT_FALSE(FontVectorPaintAddGlyph(&cache, font, FontGetGlyphIndex(font, 0x1f680), &index, &color));
    ASSERT_TRUE(data.m_Overflow);
    ASSERT_EQ(curves, data.m_Curves.Size());
    ASSERT_EQ(bands, data.m_Bands.Size());
    ASSERT_EQ(0u, cache.m_LayerScratch.Size());
    ASSERT_EQ(paths, cache.m_Paths.Size());
    ASSERT_EQ(count, data.m_CurveCount);
    FontVectorPaintClear(&cache);
    ASSERT_EQ(0u, cache.m_GlyphLookup.Size());
    ASSERT_EQ(0u, cache.m_PathLookup.Size());
    FontDestroy(font);
}

// Encoding many distinct glyphs must reuse one layer buffer without growing it
// with the total layer count or changing previously encoded paint records.
TEST(FontVector, ColorPaintScratchReuse)
{
    char  path[512];
    HFont font = FontLoadFromPath(dmTestUtil::MakeHostPath(path, sizeof(path), "src/test/data/NotoColorEmoji-Regular.ttf"));
    ASSERT_NE((HFont)0, font);
    FontVectorSlugData   data;
    FontVectorPaintCache cache;
    cache.m_Slug = &data;
    cache.m_LayerScratch.SetCapacity(64);
    dmArray<dmVMath::Vector4> original;
    uint32_t                  layers = 0;
    for (uint32_t codepoint = 0x1f600; codepoint < 0x1f640; ++codepoint)
    {
        uint32_t index;
        bool     color;
        ASSERT_TRUE(FontVectorPaintAddGlyph(&cache, font, FontGetGlyphIndex(font, codepoint), &index, &color));
        ASSERT_TRUE(color);
        ASSERT_LE(cache.m_Glyphs[index].m_LayerCount, 64u);
        layers += cache.m_Glyphs[index].m_LayerCount;
        ASSERT_EQ(0u, cache.m_LayerScratch.Size());
        ASSERT_EQ(64u, cache.m_LayerScratch.Capacity());
        ASSERT_GE(cache.m_Paints.Size(), original.Size());
        if (!original.Empty())
            ASSERT_EQ(0, memcmp(original.Begin(), cache.m_Paints.Begin(), original.Size() * sizeof(dmVMath::Vector4)));
        original.SetCapacity(cache.m_Paints.Size());
        original.SetSize(cache.m_Paints.Size());
        memcpy(original.Begin(), cache.m_Paints.Begin(), original.Size() * sizeof(dmVMath::Vector4));
    }
    ASSERT_EQ(64u, cache.m_Glyphs.Size());
    ASSERT_GT(layers, cache.m_LayerScratch.Capacity());
    FontDestroy(font);
}

// Internal paint transforms must not change the single glyph quad's winding
// or its text opacity, and cached composition records must stay within budget.
TEST(FontVector, ColorGlyphQuadAndCompositionRecords)
{
    char  path[512];
    HFont font = FontLoadFromPath(dmTestUtil::MakeHostPath(path, sizeof(path), "src/test/data/NotoColorEmoji-Regular.ttf"));
    ASSERT_NE((HFont)0, font);
    FontVectorSlugData   data;
    FontVectorPaintCache cache;
    cache.m_Slug = &data;
    uint32_t index;
    bool     color;
    ASSERT_TRUE(FontVectorPaintAddGlyph(&cache, font, FontGetGlyphIndex(font, 0x1f600), &index, &color));
    const FontVectorPaintGlyph& glyph = cache.m_Glyphs[index];
    ASSERT_GT(glyph.m_LayerCount, 1u);
    ASSERT_EQ((float)glyph.m_LayerCount, cache.m_Paints[glyph.m_Paint].getX());
    ASSERT_EQ(glyph.m_Paint + 1 + glyph.m_LayerCount * 6, cache.m_Paints.Size());
    ASSERT_LE(cache.m_Paints.Size(), data.m_MaxTexels);
    TextGlyphFaceColors colors;
    float*              corners[] = { colors.m_BottomLeft, colors.m_BottomRight, colors.m_TopLeft, colors.m_TopRight };
    for (uint32_t i = 0; i < 4; ++i)
    {
        corners[i][0] = corners[i][1] = corners[i][2] = 1;
        corners[i][3] = .5f;
    }
    FontVectorPaintVertex vertices[6];
    FontVectorPaintPackGlyph(glyph, 1, 1, 0, 0, dmVMath::Matrix4::identity(), dmVMath::Vector4(-FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX), colors, vertices);
    for (uint32_t i = 0; i < 6; ++i)
    {
        ASSERT_EQ(.5f, vertices[i].m_Color.getW());
        ASSERT_EQ((float)glyph.m_Paint, vertices[i].m_Paint);
    }
    for (uint32_t i = 0; i < 6; i += 3)
    {
        dmVMath::Vector4 a = vertices[i + 1].m_Position - vertices[i].m_Position;
        dmVMath::Vector4 b = vertices[i + 2].m_Position - vertices[i].m_Position;
        ASSERT_LT(a.getX() * b.getY() - a.getY() * b.getX(), 0.0f);
    }
    // Reuse the paths but leave room only for paint nodes, without another
    // composition header/layer table. The original glyph must remain intact.
    uint32_t                  paints = cache.m_Paints.Size();
    dmArray<dmVMath::Vector4> original;
    original.SetCapacity(paints);
    original.SetSize(paints);
    memcpy(original.Begin(), cache.m_Paints.Begin(), paints * sizeof(dmVMath::Vector4));
    cache.m_GlyphLookup.Clear();
    data.m_MaxTexels = paints + glyph.m_Paint;
    ASSERT_FALSE(FontVectorPaintAddGlyph(&cache, font, glyph.m_Glyph, &index, &color));
    ASSERT_TRUE(data.m_Overflow);
    ASSERT_EQ(paints, cache.m_Paints.Size());
    ASSERT_EQ(0u, cache.m_LayerScratch.Size());
    ASSERT_EQ(0, memcmp(original.Begin(), cache.m_Paints.Begin(), paints * sizeof(dmVMath::Vector4)));
    ASSERT_EQ(1u, cache.m_Glyphs.Size());
    FontDestroy(font);
}

// PaintGlyph must use raw outline bounds even when that glyph has its own COLR clip.
TEST(FontVector, ColorClipDoesNotCropReusedOutline)
{
    char  path[512];
    HFont font = FontLoadFromPath(dmTestUtil::MakeHostPath(path, sizeof(path), "src/test/data/color-test.ttf"));
    ASSERT_NE((HFont)0, font);
    uint32_t         clipped = FontGetGlyphIndex(font, 'B');
    FontGlyphOptions options;
    options.m_StbttSDFPadding = 0;
    FontGlyph outline;
    ASSERT_EQ(FONT_RESULT_OK, FontGetGlyphByIndex(font, clipped, &options, &outline));
    ASSERT_EQ(500.0f, outline.m_Width); // Layout sees the color glyph's clipped bounds.
    FontFreeGlyph(font, &outline);
    options.m_GenerateOutline = true;
    ASSERT_EQ(FONT_RESULT_OK, FontGetGlyphByIndex(font, clipped, &options, &outline));
    ASSERT_EQ(1000.0f, outline.m_Outline.m_Width);
    ASSERT_EQ(1000.0f, outline.m_Outline.m_Height);
    FontFreeGlyph(font, &outline);

    FontVectorSlugData   data;
    FontVectorPaintCache cache;
    cache.m_Slug = &data;
    uint32_t index;
    bool     color;
    ASSERT_TRUE(FontVectorPaintAddGlyph(&cache, font, clipped, &index, &color));
    ASSERT_TRUE(color);
    ASSERT_EQ(500.0f, cache.m_Glyphs[index].m_Bounds.getZ());
    ASSERT_TRUE(FontVectorPaintAddGlyph(&cache, font, FontGetGlyphIndex(font, 'C'), &index, &color));
    ASSERT_TRUE(color);
    ASSERT_EQ(1000.0f, cache.m_Glyphs[index].m_Bounds.getZ());
    FontDestroy(font);
}

// Only explicit composites inside one outline share coverage. Nested composites
// flatten into that group, while sibling clips and layer lists remain separate.
TEST(FontVector, SharedClipCompositeRecords)
{
    char  path[512];
    HFont font = FontLoadFromPath(dmTestUtil::MakeHostPath(path, sizeof(path), "src/test/data/color-test.ttf"));
    ASSERT_NE((HFont)0, font);
    FontVectorSlugData   data;
    FontVectorPaintCache cache;
    cache.m_Slug = &data;
    const uint32_t counts[] = { 1, 2, 1, 1, 3, 2, 1 }; // J-P.
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(counts); ++i)
    {
        uint32_t index;
        bool     color;
        ASSERT_TRUE(FontVectorPaintAddGlyph(&cache, font, FontGetGlyphIndex(font, 'J' + i), &index, &color));
        ASSERT_TRUE(color);
        const FontVectorPaintGlyph& glyph = cache.m_Glyphs[index];
        ASSERT_EQ((float)counts[i], cache.m_Paints[glyph.m_Paint + 1 + (glyph.m_LayerCount - 1) * 6 + 3].getW());
        ASSERT_EQ(counts[i] > 1, cache.m_Paints[glyph.m_Paint + 1 + (glyph.m_LayerCount - 1) * 6 + 1].getW() < 0);
        ASSERT_EQ(glyph.m_Paint + 1 + glyph.m_LayerCount * 6, cache.m_Paints.Size());
        if (i == 2 || i == 3)
        {
            ASSERT_EQ(2u, glyph.m_LayerCount);
            ASSERT_EQ(1.0f, cache.m_Paints[glyph.m_Paint + 4].getW());
        }
    }
    FontDestroy(font);
}

// CPU effect generation preserves analytical faces and the outline distance field.
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
        if (i == 0)
            curve_count = encoded.m_CurveCount;
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
            if (i == 0)
                ASSERT_GE(pixel[2], pixel[0]);
        }
        ASSERT_GT(outline_only, 0u);
        if (i > 0)
            ASSERT_GT(shadow_only, 0u);
        FontFreeGlyph(font, &sdf);
        FontFreeGlyph(font, &glyph);
    }
    FontDestroy(font);
}

// Cubic OpenType outlines produce normalized vector curves independently of bitmap size.
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

// Enabling bitmap effects leaves encoded vector face curves unchanged.
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
