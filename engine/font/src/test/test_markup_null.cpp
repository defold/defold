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

#include "../fontcollection.h"
#include "../markup.h"
#include "../text_layout.h"

TEST(MarkupNull, ReportsUnsupported)
{
    HMarkup     markup = (HMarkup)(uintptr_t)1;
    MarkupError error = { 12, MARKUP_ERROR_NONE };
    ASSERT_EQ(MARKUP_RESULT_UNSUPPORTED, MarkupCreate("text", 4, &markup, &error));
    ASSERT_EQ((HMarkup)0, markup);
    ASSERT_EQ(MARKUP_ERROR_UNSUPPORTED, error.m_Type);
    ASSERT_EQ(0u, error.m_ByteOffset);

    markup = (HMarkup)(uintptr_t)1;
    error.m_ByteOffset = 12;
    error.m_Type = MARKUP_ERROR_NONE;
    ASSERT_EQ(MARKUP_RESULT_UNSUPPORTED, MarkupCreateStyleFragment("<color=#fff>", 12, &markup, &error));
    ASSERT_EQ((HMarkup)0, markup);
    ASSERT_EQ(MARKUP_ERROR_UNSUPPORTED, error.m_Type);
    ASSERT_EQ(0u, error.m_ByteOffset);
}

TEST(MarkupNull, ResolvesPlainRenderData)
{
    const float base_color[4] = { 0.25f, 0.5f, 0.75f, 0.125f };
    TextGlyph   glyph = {};
    TextGlyphRenderData data;
    TextLayoutGetGlyphRenderData(0, glyph, base_color, &data);

    ASSERT_EQ(0u, data.m_StyleFlags);
    ASSERT_EQ(0.0f, data.m_OutlineWidth);
    ASSERT_EQ(0.0f, data.m_ShadowX);
    ASSERT_EQ(0.0f, data.m_ShadowY);
    ASSERT_EQ(0.0f, data.m_ShadowBlur);
    ASSERT_EQ(0.0f, data.m_OffsetX);
    ASSERT_EQ(0.0f, data.m_OffsetY);

    for (uint32_t channel = 0; channel < DM_ARRAY_SIZE(base_color); ++channel)
    {
        ASSERT_EQ(base_color[channel], data.m_FaceColors.m_BottomLeft[channel]);
        ASSERT_EQ(base_color[channel], data.m_FaceColors.m_BottomRight[channel]);
        ASSERT_EQ(base_color[channel], data.m_FaceColors.m_TopLeft[channel]);
        ASSERT_EQ(base_color[channel], data.m_FaceColors.m_TopRight[channel]);
        ASSERT_EQ(1.0f, data.m_OutlineColor[channel]);
        ASSERT_EQ(1.0f, data.m_ShadowColor[channel]);
    }

    ASSERT_FALSE(TextLayoutHasMarkupOutline(0));
    ASSERT_EQ(0.0f, TextLayoutGetMaxMarkupOutlineWidth(0));
    ASSERT_FALSE(TextLayoutHasMarkupShadow(0));
}

TEST(MarkupNull, LinksPlainTextLayout)
{
    dmArray<uint32_t> codepoints;
    ASSERT_EQ(4u, TextToCodePoints("text", codepoints));
    ASSERT_EQ(4u, codepoints.Size());
}

TEST(MarkupNull, PrecompiledBaseStyleRendersWithoutParser)
{
    HFont font = FontLoadFromPath("src/test/data/vera_mo_bd.ttf");
    ASSERT_NE((HFont)0, font);
    HFontCollection collection = FontCollectionCreate();
    ASSERT_EQ(FONT_RESULT_OK, FontCollectionAddFont(collection, font));
    TextRenderStyle style = {};
    style.m_Flags = TEXT_RENDER_STYLE_OUTLINE_WIDTH | TEXT_RENDER_STYLE_OUTLINE_ALPHA;
    style.m_OutlineWidth = 1.25f;
    style.m_OutlineAlpha = 0.375f;
    TextNamedStyleDecoration decoration = {};
    decoration.m_Flags = TEXT_RESOLVED_DECORATION_UNDERLINE;
    TextEffect effect = {};
    effect.m_Type = TEXT_EFFECT_WAVE;
    effect.m_Flags = TEXT_EFFECT_AFFECTS_POSITION;
    effect.m_Wave.m_Amplitude = 2.0f;
    effect.m_Wave.m_Hz = 1.0f;
    effect.m_Wave.m_Wavelength = 4.0f;
    dmhash_t name = dmHashString64("compiled");
    FontCollectionSetNamedStyle(collection, name, style, &effect, 1);
    FontCollectionSetNamedStyleDecoration(collection, name, decoration);
    TextLayoutSettings settings = {};
    settings.m_Size = 16.0f;
    settings.m_Leading = 1.0f;
    settings.m_UseBaseStyle = 1;
    settings.m_BaseStyle = name;
    uint32_t    text[] = { 'A', 'B' };
    HTextLayout layout = 0;
    ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreate(collection, text, 2, &settings, &layout));
    ASSERT_EQ(1u, TextLayoutGetDecorationCount(layout));
    ASSERT_EQ(1u, layout->m_Effects.Size());
    TextLayoutUpdate(layout, 0.25f);
    const float         color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyphRenderData data;
    TextLayoutGetGlyphRenderData(layout, TextLayoutGetGlyphs(layout)[0], color, &data);
    ASSERT_EQ(1.25f, data.m_OutlineWidth);
    ASSERT_EQ(0.375f, data.m_OutlineColor[3]);
    ASSERT_NE(0.0f, data.m_OffsetY);
    TextLayoutRelease(layout);
    FontCollectionDestroy(collection);
    FontDestroy(font);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);

    return jc_test_run_all();
}
