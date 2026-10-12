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

#ifndef TEST_FONT_ALIGNMENT_H
#define TEST_FONT_ALIGNMENT_H

#include <ddf/ddf.h>
#include <dlib/math.h>
#include <dlib/testutil.h>
#include <font/glyphbank_ddf.h>
#include "font.h"
#include "font_glyphbank.h"
#include "fontcollection.h"
#include "glyph_gen.h"
#include "layout_vertex.h"
#include "text_layout.h"

// Same nine horizontal/vertical alignments as GUI pivots. Values are the
// font renderer's LEFT/CENTER/RIGHT and TOP/MIDDLE/BOTTOM enums.
struct FontAlignmentPivot
{
    const char* m_Name;
    uint32_t m_Align;
    uint32_t m_VAlign;
};

static const FontAlignmentPivot FONT_ALIGNMENT_PIVOTS[] = {
    { "nw", 0, 0 }, { "n", 1, 0 }, { "ne", 2, 0 },
    { "w", 0, 1 }, { "center", 1, 1 }, { "e", 2, 1 },
    { "sw", 0, 2 }, { "s", 1, 2 }, { "se", 2, 2 }
};

static uint32_t AlignmentCodepoint(void* context, uint32_t index)
{
    return ((dmFontDDF::GlyphBank*)context)->m_Glyphs.m_Data[index].m_Character;
}

static bool AlignmentGlyph(void* context, uint32_t index, FontGlyphBankGlyph* output)
{
    dmFontDDF::GlyphBank* bank = (dmFontDDF::GlyphBank*)context;
    const dmFontDDF::GlyphBank::Glyph& glyph = bank->m_Glyphs.m_Data[index];
    memset(output, 0, sizeof(*output));
    output->m_Codepoint = glyph.m_Character;
    output->m_Width = glyph.m_Width;
    output->m_Advance = glyph.m_Advance;
    output->m_LeftBearing = glyph.m_LeftBearing;
    output->m_Ascent = glyph.m_Ascent;
    output->m_Descent = glyph.m_Descent;
    output->m_LayoutWidth = glyph.m_LayoutWidth;
    output->m_LayoutLeftBearing = glyph.m_LayoutLeftBearing;
    if (glyph.m_GlyphDataSize)
    {
        const uint8_t* data = bank->m_GlyphData.m_Data + glyph.m_GlyphDataOffset;
        output->m_Data = data + 1;
        output->m_DataSize = glyph.m_GlyphDataSize - 1;
        output->m_BitmapFlags = data[0];
    }
    return true;
}

// Owns the compiled resource, font, layout and seven cached glyphs. Both the
// numeric test and the image fixture use the actual Bob-produced glyph bank.
struct FontAlignmentFixture
{
    dmFontDDF::GlyphBank* m_Bank;
    FontGlyphBankProvider m_Provider;
    HFont m_Font;
    HFontCollection m_Collection;
    HTextLayout m_Layout;
    FontGlyph m_Glyphs[7];
    uint32_t m_GlyphCount;
    FontLayoutVertexConfig m_Config;
    FontGlyphVertex m_Vertices[42];

    FontAlignmentFixture() : m_Bank(0), m_Font(0), m_Collection(0), m_Layout(0), m_GlyphCount(0) {}
    ~FontAlignmentFixture()
    {
        for (uint32_t i = 0; i < m_GlyphCount; ++i)
            FontFreeGlyph(m_Font, &m_Glyphs[i]);
        if (m_Layout)
            TextLayoutRelease(m_Layout);
        if (m_Collection)
            FontCollectionDestroy(m_Collection);
        if (m_Font)
            FontDestroy(m_Font);
        if (m_Bank)
            dmDDF::FreeMessage(m_Bank);
    }
};

static bool ResolveAlignmentGlyph(void* context, const TextGlyph& text, FontLayoutCachedGlyph* output)
{
    FontAlignmentFixture* fixture = (FontAlignmentFixture*)context;
    for (uint32_t i = 0; i < fixture->m_GlyphCount; ++i)
    {
        if (fixture->m_Glyphs[i].m_GlyphIndex != text.m_GlyphIndex)
            continue;
        output->m_Glyph = &fixture->m_Glyphs[i];
        output->m_CellX = i * 32;
        output->m_CellY = 0;
        return true;
    }
    return false;
}

static void CreateAlignmentFixture(FontAlignmentFixture& fixture, bool compiled)
{
    char path[512];
    TextLayoutSettings settings = {};
    settings.m_Size = 15;
    settings.m_Width = 65;
    settings.m_Leading = 1;
    if (compiled)
    {
        const char* host_path = dmTestUtil::MakeHostPath(path, sizeof(path), "build/src/test/data/font_render/alignment.glyph_bankc");
        ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::LoadMessageFromFile(host_path,
            dmFontDDF::GlyphBank::m_DDFDescriptor, (void**)&fixture.m_Bank));
        dmFontDDF::GlyphBank* bank = fixture.m_Bank;
        memset(&fixture.m_Provider, 0, sizeof(fixture.m_Provider));
        fixture.m_Provider.m_Context = bank;
        fixture.m_Provider.m_GetCodepoint = AlignmentCodepoint;
        fixture.m_Provider.m_GetGlyph = AlignmentGlyph;
        fixture.m_Provider.m_GlyphCount = bank->m_Glyphs.m_Count;
        fixture.m_Provider.m_GlyphPadding = bank->m_GlyphPadding;
        fixture.m_Provider.m_GlyphChannels = bank->m_GlyphChannels;
        fixture.m_Provider.m_MaxAscent = bank->m_MaxAscent;
        fixture.m_Provider.m_MaxDescent = bank->m_MaxDescent;
        fixture.m_Provider.m_HasLayoutMetrics = bank->m_HasLayoutMetrics;
        fixture.m_Font = FontCreateGlyphBank("alignment.glyph_bankc", &fixture.m_Provider);
        settings.m_Monospace = bank->m_IsMonospaced && !bank->m_HasLayoutMetrics;
        settings.m_Padding = bank->m_HasLayoutMetrics ? 0 : bank->m_Padding;
    }
    else
    {
        fixture.m_Font = FontLoadFromPath(dmTestUtil::MakeHostPath(path, sizeof(path), "src/test/data/vera_mo_bd.ttf"));
    }
    ASSERT_NE((HFont)0, fixture.m_Font);
    fixture.m_Collection = FontCollectionCreate();
    ASSERT_EQ(FONT_RESULT_OK, FontCollectionAddFont(fixture.m_Collection, fixture.m_Font));
    dmArray<uint32_t> codepoints;
    TextToCodePoints("Example", codepoints);
    // Editor preview disables shaping for an offline font, including in the
    // full-layout build. Compile-time layout selection must not change this test.
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutLegacyCreate(fixture.m_Collection, codepoints.Begin(), codepoints.Size(), &settings, &fixture.m_Layout));
    FontGlyphGenParams params;
    params.m_Scale = FontGetScaleFromSize(fixture.m_Font, 15);
    params.m_SdfPadding = 3;
    for (uint32_t i = 0; i < codepoints.Size(); ++i)
    {
        uint32_t index = FontGetGlyphIndex(fixture.m_Font, codepoints[i]);
        FontGlyph* glyph = &fixture.m_Glyphs[fixture.m_GlyphCount];
        if (compiled)
        {
            FontGlyphOptions options;
            options.m_GenerateImage = true;
            ASSERT_EQ(FONT_RESULT_OK, FontGetGlyphByIndex(fixture.m_Font, index, &options, glyph));
        }
        else
        {
            ASSERT_EQ(FONT_RESULT_OK, FontGenerateGlyph(fixture.m_Font, index, &params, glyph));
        }
        ++fixture.m_GlyphCount;
    }
    FontLayoutVertexConfig& config = fixture.m_Config;
    memset(&config, 0, sizeof(config));
    config.m_Layout = fixture.m_Layout;
    config.m_ResolveGlyph = ResolveAlignmentGlyph;
    config.m_ResolveGlyphContext = &fixture;
    config.m_Width = 65;
    config.m_Height = 30;
    config.m_RecipAtlasWidth = 1.0f / 256;
    config.m_RecipAtlasHeight = 1.0f / 32;
    config.m_CacheCellMaxAscent = 16;
    config.m_CacheCellPadding = 1;
    config.m_MonospacePadding = settings.m_Monospace ? settings.m_Padding : 0;
    config.m_MetricsFromTtf = !compiled;
    config.m_IsSdf = true;
    config.m_BaseLayerMask = FONT_RENDER_LAYER_FACE;
    config.m_FaceColor[3] = 1;
    config.m_SdfSpread = 3;
    config.m_SdfEdge = 0.75f;
    config.m_SdfOutline = 0.75f;
    config.m_SdfSmoothing = FONT_SDF_DISTANCE_SCALE / (3 * 3);
}

static void CreateAlignmentVertices(FontAlignmentFixture& fixture, const FontAlignmentPivot& pivot)
{
    FontLayoutVertexConfig& config = fixture.m_Config;
    config.m_Align = pivot.m_Align;
    config.m_VerticalAlign = pivot.m_VAlign;
    // A 200x100 white box at (20,20), with the text anchored to the matching
    // pivot. Apply the GUI text-node pivot translation before its 3x scale.
    const float x = pivot.m_Align * 0.5f;
    const float y = 1 - pivot.m_VAlign * 0.5f;
    config.m_Transform = dmVMath::Matrix4::translation(dmVMath::Vector3(20 + x * (200 - 65 * 3), 20 + y * (100 - 30 * 3), 0)) *
                          dmVMath::Matrix4::scale(dmVMath::Vector3(3));
    FontLayoutVertexMetrics metrics;
    ASSERT_TRUE(FontGetLayoutVertexMetrics(config, &metrics));
    ASSERT_EQ(42u, FontCreateLayoutVertices(config, metrics, fixture.m_Vertices, 42));
}

#endif
