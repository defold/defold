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

#include <stdint.h>
#include <string.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "fontc.h"

static void AssertSourceString(const FontcMarkupData& data, FontcMarkupString string, const char* expected)
{
    ASSERT_EQ(strlen(expected), string.m_Length);
    ASSERT_EQ(0, memcmp(data.m_Source + string.m_Offset, expected, string.m_Length));
}

TEST(FontcMarkupApi, ParseAndGetData)
{
    const char expected_source[] = "<link id=\"<shadow blur=9>\"><outline size=1><shadow blur=2>A&amp;</shadow></outline></link>";
    char       source[sizeof(expected_source)];
    memcpy(source, expected_source, sizeof(source));

    HFontcMarkup     markup = 0;
    FontcMarkupError error = {};
    ASSERT_EQ(FONT_RENDERER_RESULT_OK, FontcParseMarkup(source, sizeof(source) - 1, &markup, &error));
    ASSERT_NE((HFontcMarkup)0, markup);
    ASSERT_EQ(FONTC_MARKUP_ERROR_NONE, error.m_Type);

    source[0] = '?';
    FontcMarkupData data = {};
    ASSERT_EQ(FONT_RENDERER_RESULT_OK, FontcGetMarkupData(markup, &data));
    ASSERT_EQ(sizeof(expected_source) - 1, data.m_SourceLength);
    ASSERT_EQ(0, memcmp(expected_source, data.m_Source, data.m_SourceLength));
    ASSERT_EQ('\0', data.m_Source[data.m_SourceLength]);

    ASSERT_EQ(2u, data.m_TextLength);
    ASSERT_EQ((uint32_t)'A', data.m_Text[0]);
    ASSERT_EQ((uint32_t)'&', data.m_Text[1]);

    ASSERT_EQ(4u, data.m_NodeCount);
    ASSERT_EQ(0xffffu, data.m_Nodes[0].m_Parent);
    ASSERT_EQ(0u, data.m_Nodes[0].m_Tag.m_Length);
    AssertSourceString(data, data.m_Nodes[1].m_Tag, "link");
    ASSERT_EQ(0u, data.m_Nodes[1].m_Parent);
    ASSERT_EQ(0u, data.m_Nodes[1].m_AttributeIndex);
    ASSERT_EQ(1u, data.m_Nodes[1].m_AttributeCount);
    AssertSourceString(data, data.m_Nodes[2].m_Tag, "outline");
    ASSERT_EQ(1u, data.m_Nodes[2].m_Parent);
    ASSERT_EQ(1u, data.m_Nodes[2].m_AttributeIndex);
    ASSERT_EQ(1u, data.m_Nodes[2].m_AttributeCount);
    AssertSourceString(data, data.m_Nodes[3].m_Tag, "shadow");
    ASSERT_EQ(2u, data.m_Nodes[3].m_Parent);
    ASSERT_EQ(2u, data.m_Nodes[3].m_AttributeIndex);
    ASSERT_EQ(1u, data.m_Nodes[3].m_AttributeCount);

    ASSERT_EQ(3u, data.m_AttributeCount);
    AssertSourceString(data, data.m_Attributes[0].m_Name, "id");
    AssertSourceString(data, data.m_Attributes[0].m_Value, "<shadow blur=9>");
    AssertSourceString(data, data.m_Attributes[1].m_Name, "size");
    AssertSourceString(data, data.m_Attributes[1].m_Value, "1");
    AssertSourceString(data, data.m_Attributes[2].m_Name, "blur");
    AssertSourceString(data, data.m_Attributes[2].m_Value, "2");

    ASSERT_EQ(1u, data.m_SpanCount);
    ASSERT_EQ(0u, data.m_Spans[0].m_TextOffset);
    ASSERT_EQ(2u, data.m_Spans[0].m_TextLength);
    ASSERT_EQ(3u, data.m_Spans[0].m_NodeIndex);

    FontcMarkupData second_view = {};
    ASSERT_EQ(FONT_RENDERER_RESULT_OK, FontcGetMarkupData(markup, &second_view));
    ASSERT_EQ(data.m_Source, second_view.m_Source);
    ASSERT_EQ(data.m_Text, second_view.m_Text);
    ASSERT_EQ(data.m_Nodes, second_view.m_Nodes);
    ASSERT_EQ(data.m_Attributes, second_view.m_Attributes);
    ASSERT_EQ(data.m_Spans, second_view.m_Spans);

    FontcDestroyMarkup(markup);
}

TEST(FontcMarkupApi, ReportsStructuredParseError)
{
    const char       source[] = "<outline>Text</shadow>";
    HFontcMarkup     markup = (HFontcMarkup)(uintptr_t)1;
    FontcMarkupError error = {};

    ASSERT_EQ(FONT_RENDERER_RESULT_TEXT_ERROR, FontcParseMarkup(source, sizeof(source) - 1, &markup, &error));
    ASSERT_EQ((HFontcMarkup)0, markup);
    ASSERT_EQ(FONTC_MARKUP_ERROR_MISMATCHED_CLOSING_TAG, error.m_Type);
    ASSERT_EQ(13u, error.m_ByteOffset);
}

TEST(FontcMarkupApi, ValidatesArgumentsAndAcceptsEmptyInput)
{
    HFontcMarkup     markup = 0;
    FontcMarkupError error = {};
    FontcMarkupData  data = {};

    ASSERT_EQ(FONT_RENDERER_RESULT_INVALID_ARGUMENT, FontcParseMarkup(0, 1, &markup, &error));
    ASSERT_EQ(FONT_RENDERER_RESULT_INVALID_ARGUMENT, FontcParseMarkup("", 0, 0, &error));
    ASSERT_EQ(FONT_RENDERER_RESULT_INVALID_ARGUMENT, FontcParseMarkup("", 0, &markup, 0));
    ASSERT_EQ(FONT_RENDERER_RESULT_INVALID_ARGUMENT, FontcGetMarkupData(0, &data));
    ASSERT_EQ(FONT_RENDERER_RESULT_OK, FontcParseMarkup(0, 0, &markup, &error));
    ASSERT_EQ(FONTC_MARKUP_ERROR_NONE, error.m_Type);
    ASSERT_EQ(FONT_RENDERER_RESULT_INVALID_ARGUMENT, FontcGetMarkupData(markup, 0));
    ASSERT_EQ(FONT_RENDERER_RESULT_OK, FontcGetMarkupData(markup, &data));
    ASSERT_EQ(0u, data.m_SourceLength);
    ASSERT_EQ(0u, data.m_TextLength);
    ASSERT_EQ(1u, data.m_NodeCount);
    ASSERT_EQ(0u, data.m_AttributeCount);
    ASSERT_EQ(0u, data.m_SpanCount);

    FontcDestroyMarkup(markup);
    FontcDestroyMarkup(0);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);

    return jc_test_run_all();
}
