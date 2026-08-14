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
    ASSERT_EQ(MARKUP_RESULT_UNSUPPORTED, MarkupCreateRecovering("text", 4, &markup, &error));
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

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
