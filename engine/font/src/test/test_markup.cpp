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

#include <dmsdk/dlib/array.h>

#include "markup.h"

static void AssertString(HMarkup markup, MarkupString string, const char* expected)
{
    ASSERT_EQ(strlen(expected), string.m_Length);
    ASSERT_EQ(0, memcmp(MarkupGetSource(markup) + string.m_Offset, expected, string.m_Length));
}

static void AssertText(HMarkup markup, const uint32_t* expected, uint32_t expected_count)
{
    ASSERT_EQ(expected_count, MarkupGetTextLength(markup));
    const uint32_t* actual = MarkupGetText(markup);

    for (uint32_t i = 0; i < expected_count; ++i)
    {
        ASSERT_EQ(expected[i], actual[i]);
    }
}

TEST(Markup, PlainText)
{
    const char  text[] = "Hello";
    HMarkup     markup = 0;
    MarkupError error;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(text, sizeof(text) - 1, &markup, &error));
    ASSERT_NE((HMarkup)0, markup);
    ASSERT_EQ(MARKUP_ERROR_NONE, error.m_Type);

    const uint32_t expected[] = { 'H', 'e', 'l', 'l', 'o' };
    AssertText(markup, expected, DM_ARRAY_SIZE(expected));
    ASSERT_EQ(1u, MarkupGetStyleNodeCount(markup));
    ASSERT_EQ(1u, MarkupGetSpanCount(markup));
    ASSERT_EQ(0u, MarkupGetSpans(markup)[0].m_StyleNodeIndex);
    MarkupDestroy(markup);
}

TEST(Markup, NestedTagsAndAttributes)
{
    const char  text[] = "A<wave amplitude=4>B<gradient left=#FF00FF right='#FFFFFF'>CD</gradient>E</wave>F";
    HMarkup     markup = 0;
    MarkupError error;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(text, sizeof(text) - 1, &markup, &error));

    const uint32_t expected[] = { 'A', 'B', 'C', 'D', 'E', 'F' };
    AssertText(markup, expected, DM_ARRAY_SIZE(expected));

    ASSERT_EQ(3u, MarkupGetStyleNodeCount(markup));
    const MarkupStyleNode* nodes = MarkupGetStyleNodes(markup);
    AssertString(markup, nodes[1].m_Tag, "wave");
    ASSERT_EQ((uint8_t)MARKUP_TAG_WAVE,   nodes[1].m_Type);
    ASSERT_EQ(0u,                         nodes[1].m_Parent);
    ASSERT_EQ(1u,                         nodes[1].m_AttributeCount);
    AssertString(markup, nodes[2].m_Tag, "gradient");
    ASSERT_EQ((uint8_t)MARKUP_TAG_GRADIENT,   nodes[2].m_Type);
    ASSERT_EQ(1u,                             nodes[2].m_Parent);
    ASSERT_EQ(2u,                             nodes[2].m_AttributeCount);

    ASSERT_EQ(3u, MarkupGetAttributeCount(markup));
    const MarkupAttribute* attributes = MarkupGetAttributes(markup);
    AssertString(markup, attributes[0].m_Name, "amplitude");
    AssertString(markup, attributes[0].m_Value, "4");
    ASSERT_EQ((uint8_t)MARKUP_ATTRIBUTE_AMPLITUDE, attributes[0].m_Type);
    ASSERT_EQ((uint8_t)MARKUP_CONSTANT_NONE,       attributes[0].m_Constant);
    AssertString(markup, attributes[1].m_Name, "left");
    AssertString(markup, attributes[1].m_Value, "#FF00FF");
    ASSERT_EQ((uint8_t)MARKUP_ATTRIBUTE_LEFT, attributes[1].m_Type);
    AssertString(markup, attributes[2].m_Name, "right");
    AssertString(markup, attributes[2].m_Value, "#FFFFFF");
    ASSERT_EQ((uint8_t)MARKUP_ATTRIBUTE_RIGHT, attributes[2].m_Type);

    ASSERT_EQ(5u, MarkupGetSpanCount(markup));
    const MarkupSpan* spans = MarkupGetSpans(markup);
    ASSERT_EQ(0u, spans[0].m_StyleNodeIndex);
    ASSERT_EQ(1u, spans[1].m_StyleNodeIndex);
    ASSERT_EQ(2u, spans[2].m_StyleNodeIndex);
    ASSERT_EQ(1u, spans[3].m_StyleNodeIndex);
    ASSERT_EQ(0u, spans[4].m_StyleNodeIndex);
    ASSERT_EQ(2u, spans[2].m_TextLength);
    MarkupDestroy(markup);
}

TEST(Markup, ShorthandAndQuotedValue)
{
    const char text[] = "<color=red>R</color><size=\"14\">G</size>";
    HMarkup    markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(text, sizeof(text) - 1, &markup, 0));

    const MarkupStyleNode* nodes = MarkupGetStyleNodes(markup);
    const MarkupAttribute* attributes = MarkupGetAttributes(markup);
    ASSERT_EQ((uint8_t)MARKUP_TAG_COLOR,            nodes[1].m_Type);
    ASSERT_EQ(0u,                                   attributes[nodes[1].m_AttributeIndex].m_Name.m_Length);
    ASSERT_EQ((uint8_t)MARKUP_ATTRIBUTE_SHORTHAND,  attributes[nodes[1].m_AttributeIndex].m_Type);
    AssertString(markup, attributes[nodes[1].m_AttributeIndex].m_Value, "red");
    ASSERT_EQ((uint8_t)MARKUP_TAG_SIZE,             nodes[2].m_Type);
    ASSERT_EQ(0u,                                   attributes[nodes[2].m_AttributeIndex].m_Name.m_Length);
    ASSERT_EQ((uint8_t)MARKUP_ATTRIBUTE_SHORTHAND,  attributes[nodes[2].m_AttributeIndex].m_Type);
    AssertString(markup, attributes[nodes[2].m_AttributeIndex].m_Value, "14");
    MarkupDestroy(markup);
}

TEST(Markup, EntitiesAndUtf8)
{
    const char text[] = "&lt;Hej&gt; \xF0\x9F\x98\x80 &amp;";
    HMarkup    markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(text, sizeof(text) - 1, &markup, 0));
    const uint32_t expected[] = { '<', 'H', 'e', 'j', '>', ' ', 0x1f600, ' ', '&' };
    AssertText(markup, expected, DM_ARRAY_SIZE(expected));
    MarkupDestroy(markup);
}

// Verifies that a literal ampersand in visible text does not cause an entity error (issue #13374).
TEST(Markup, PlainTextWithLiteralAmpersand)
{
    const char* cases[] = { "A & B", "R&D", "R&D; next", "&", "A&", "&&", "&!", "&;", "&unknown;", "&#65;", "A & development" };

    for (uint32_t i = 0; i < DM_ARRAY_SIZE(cases); ++i)
    {
        const uint32_t length = (uint32_t)strlen(cases[i]);
        HMarkup        markup = 0;
        MarkupError    error = {};
        ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(cases[i], length, &markup, &error));
        ASSERT_NE((HMarkup)0, markup);
        ASSERT_EQ(MARKUP_ERROR_NONE, error.m_Type);
        ASSERT_EQ(length, MarkupGetTextLength(markup));

        const uint32_t* actual = MarkupGetText(markup);
        for (uint32_t j = 0; j < length; ++j)
        {
            ASSERT_EQ((uint32_t)cases[i][j], actual[j]);
        }

        ASSERT_EQ(1u, MarkupGetStyleNodeCount(markup));
        ASSERT_EQ(1u, MarkupGetSpanCount(markup));
        ASSERT_EQ(0u, MarkupGetSpans(markup)[0].m_StyleNodeIndex);
        MarkupDestroy(markup);
    }
}

// Verifies that literal ampersands before and after a color tag preserve rich text, including a trailing escape prefix (issue #13374).
TEST(Markup, LiteralAmpersandPreservesFollowingTag)
{
    const char  text[] = "A & <color=#FF0000>B</color>&a";
    HMarkup     markup = 0;
    MarkupError error = {};
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(text, sizeof(text) - 1, &markup, &error));
    ASSERT_NE((HMarkup)0, markup);
    ASSERT_EQ(MARKUP_ERROR_NONE, error.m_Type);

    const uint32_t expected[] = { 'A', ' ', '&', ' ', 'B', '&', 'a' };
    AssertText(markup, expected, DM_ARRAY_SIZE(expected));
    ASSERT_EQ(2u, MarkupGetStyleNodeCount(markup));
    const MarkupStyleNode& node = MarkupGetStyleNodes(markup)[1];
    ASSERT_EQ((uint8_t)MARKUP_TAG_COLOR, node.m_Type);
    ASSERT_EQ(0u, node.m_Parent);
    ASSERT_EQ(4u, node.m_TextOffset);
    ASSERT_EQ(1u, node.m_TextLength);
    ASSERT_EQ(1u, node.m_AttributeCount);
    const MarkupAttribute& attribute = MarkupGetAttributes(markup)[node.m_AttributeIndex];
    ASSERT_EQ((uint8_t)MARKUP_ATTRIBUTE_SHORTHAND, attribute.m_Type);
    AssertString(markup, attribute.m_Value, "#FF0000");

    ASSERT_EQ(3u, MarkupGetSpanCount(markup));
    const MarkupSpan* spans = MarkupGetSpans(markup);
    ASSERT_EQ(0u, spans[0].m_StyleNodeIndex);
    ASSERT_EQ(0u, spans[0].m_TextOffset);
    ASSERT_EQ(4u, spans[0].m_TextLength);
    ASSERT_EQ(1u, spans[1].m_StyleNodeIndex);
    ASSERT_EQ(4u, spans[1].m_TextOffset);
    ASSERT_EQ(1u, spans[1].m_TextLength);
    ASSERT_EQ(0u, spans[2].m_StyleNodeIndex);
    ASSERT_EQ(5u, spans[2].m_TextOffset);
    ASSERT_EQ(2u, spans[2].m_TextLength);
    MarkupDestroy(markup);
}

// Verifies that literal ampersands inside and after a tag coexist with entity decoding and UTF-8 (issue #13374).
TEST(Markup, LiteralAmpersandsAndEntities)
{
    const char  text[] = "<color=#FF0000>R&D&amp;</color>&lt;&gt;&apos;&quot;&\xF0\x9F\x98\x80&";
    HMarkup     markup = 0;
    MarkupError error = {};
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(text, sizeof(text) - 1, &markup, &error));
    ASSERT_EQ(MARKUP_ERROR_NONE, error.m_Type);

    const uint32_t expected[] = { 'R', '&', 'D', '&', '<', '>', '\'', '"', '&', 0x1f600, '&' };
    AssertText(markup, expected, DM_ARRAY_SIZE(expected));
    ASSERT_EQ(2u, MarkupGetSpanCount(markup));
    const MarkupSpan* spans = MarkupGetSpans(markup);
    ASSERT_EQ(1u, spans[0].m_StyleNodeIndex);
    ASSERT_EQ(0u, spans[0].m_TextOffset);
    ASSERT_EQ(4u, spans[0].m_TextLength);
    ASSERT_EQ(0u, spans[1].m_StyleNodeIndex);
    ASSERT_EQ(4u, spans[1].m_TextOffset);
    ASSERT_EQ(7u, spans[1].m_TextLength);
    MarkupDestroy(markup);
}

// Verifies that unfinished escapes remain literal text, using the explicit input length (issue #13374).
TEST(Markup, EntityPrefixesAreLiteral)
{
    const char* entities[] = { "&amp;", "&apos;", "&gt;", "&lt;", "&quot;" };

    for (uint32_t i = 0; i < DM_ARRAY_SIZE(entities); ++i)
    {
        const uint32_t entity_length = (uint32_t)strlen(entities[i]);
        for (uint32_t length = 2; length < entity_length; ++length)
        {
            HMarkup     markup = 0;
            MarkupError error = {};
            ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(entities[i], length, &markup, &error));
            ASSERT_EQ(MARKUP_ERROR_NONE, error.m_Type);
            ASSERT_EQ(length, MarkupGetTextLength(markup));
            const uint32_t* actual = MarkupGetText(markup);
            for (uint32_t j = 0; j < length; ++j)
            {
                ASSERT_EQ((uint32_t)entities[i][j], actual[j]);
            }
            MarkupDestroy(markup);
        }
    }
}

// Verifies that filtering escapes literal ampersands and preserves existing entities and tags (issue #13374).
TEST(Markup, FilterTextWithLiteralAmpersands)
{
    const char source[] = "A&<color=#FF0000>B&amp;C</color>D&";
    const uint32_t allowed[] = { '&' };
    const char expected[] = "&amp;<color=#FF0000>&amp;</color>&amp;";
    char output[5 * (sizeof(source) - 1)];
    uint32_t output_length = 0;
    MarkupError error = {};

    ASSERT_EQ(MARKUP_RESULT_OK,
              MarkupFilterText(source, sizeof(source) - 1,
                               allowed, DM_ARRAY_SIZE(allowed),
                               output, sizeof(output),
                               &output_length, &error));
    ASSERT_EQ(MARKUP_ERROR_NONE, error.m_Type);
    ASSERT_EQ(sizeof(expected) - 1, output_length);
    ASSERT_EQ(0, memcmp(expected, output, output_length));
}

// Verifies that removing an unsupported codepoint cannot turn literal text into a decoded entity.
TEST(Markup, FilterTextCannotIntroduceEntities)
{
    const char source[] = "<color=#FF0000>&am\xE4\xB8\xADp;</color>";
    const uint32_t allowed[] = { '&', 'a', 'm', 'p', ';' };
    const char expected_source[] = "<color=#FF0000>&amp;amp;</color>";
    const uint32_t expected_text[] = { '&', 'a', 'm', 'p', ';' };
    char output[5 * (sizeof(source) - 1)];
    uint32_t output_length = 0;
    MarkupError error = {};

    ASSERT_EQ(MARKUP_RESULT_OK,
              MarkupFilterText(source, sizeof(source) - 1,
                               allowed, DM_ARRAY_SIZE(allowed),
                               output, sizeof(output), &output_length, &error));
    ASSERT_EQ(MARKUP_ERROR_NONE, error.m_Type);
    ASSERT_EQ(sizeof(expected_source) - 1, output_length);
    ASSERT_EQ(0, memcmp(expected_source, output, output_length));

    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(output, output_length, &markup, &error));
    AssertText(markup, expected_text, DM_ARRAY_SIZE(expected_text));
    ASSERT_EQ(1u, MarkupGetSpanCount(markup));
    ASSERT_EQ(1u, MarkupGetSpans(markup)[0].m_StyleNodeIndex);
    ASSERT_EQ(5u, MarkupGetSpans(markup)[0].m_TextLength);
    MarkupDestroy(markup);
}

// Verifies that escaped filtering checks capacity for both visible text and preserved closing tags.
TEST(Markup, FilterTextChecksEscapedCapacity)
{
    struct FilterCase
    {
        const char* m_Source;
        const char* m_Expected;
    };
    const FilterCase cases[] = {
        { "&&", "&amp;&amp;" },
        { "<color=#FF0000>&</color>", "<color=#FF0000>&amp;</color>" },
    };
    const uint32_t allowed[] = { '&' };
    char output[64];

    for (uint32_t i = 0; i < DM_ARRAY_SIZE(cases); ++i)
    {
        const uint32_t source_length = (uint32_t)strlen(cases[i].m_Source);
        const uint32_t expected_length = (uint32_t)strlen(cases[i].m_Expected);
        for (uint32_t capacity = source_length; capacity <= expected_length; ++capacity)
        {
            memset(output, '?', sizeof(output));
            uint32_t output_length = 123;
            MarkupError error = {};
            MarkupResult result = MarkupFilterText(cases[i].m_Source, source_length,
                                                   allowed, DM_ARRAY_SIZE(allowed),
                                                   output, capacity, &output_length, &error);
            ASSERT_EQ('?', output[capacity]);
            if (capacity < expected_length)
            {
                ASSERT_EQ(MARKUP_RESULT_LIMIT_EXCEEDED, result);
                ASSERT_EQ(MARKUP_ERROR_LIMIT_EXCEEDED, error.m_Type);
                ASSERT_EQ(0u, output_length);
            }
            else
            {
                ASSERT_EQ(MARKUP_RESULT_OK, result);
                ASSERT_EQ(MARKUP_ERROR_NONE, error.m_Type);
                ASSERT_EQ(expected_length, output_length);
                ASSERT_EQ(0, memcmp(cases[i].m_Expected, output, output_length));
            }
        }
    }
}

TEST(Markup, FilterTextPreservesSyntax)
{
    const char source[] = "<link id=\"<shadow blur=2>\">A&amp;B\xE4\xB8\xAD<sprite/></link>C";
    const uint32_t allowed[] = { 'A', '&', 'C' };
    const char expected[] = "<link id=\"<shadow blur=2>\">A&amp;<sprite/></link>C";
    char output[sizeof(source) - 1];
    uint32_t output_length = 0;
    MarkupError error = {};

    ASSERT_EQ(MARKUP_RESULT_OK,
              MarkupFilterText(source, sizeof(source) - 1,
                               allowed, DM_ARRAY_SIZE(allowed),
                               output, sizeof(output),
                               &output_length, &error));
    ASSERT_EQ(MARKUP_ERROR_NONE, error.m_Type);
    ASSERT_EQ(sizeof(expected) - 1, output_length);
    ASSERT_EQ(0, memcmp(expected, output, output_length));
}

struct InvalidMarkupCase
{
    const char*     m_Text;
    MarkupResult    m_Result;
    MarkupErrorType m_Error;
};

TEST(Markup, BrokenMarkup)
{
    const InvalidMarkupCase cases[] = {
        { "</color>", MARKUP_RESULT_SYNTAX_ERROR, MARKUP_ERROR_UNEXPECTED_CLOSING_TAG },
        { "<color>red</size>", MARKUP_RESULT_SYNTAX_ERROR, MARKUP_ERROR_MISMATCHED_CLOSING_TAG },
        { "<color=red", MARKUP_RESULT_INCOMPLETE, MARKUP_ERROR_INCOMPLETE_TAG },
        { "<color=\"red>", MARKUP_RESULT_INCOMPLETE, MARKUP_ERROR_INCOMPLETE_TAG },
        { "<color red>text</color>", MARKUP_RESULT_SYNTAX_ERROR, MARKUP_ERROR_INVALID_ATTRIBUTE },
        { "<color=>text</color>", MARKUP_RESULT_SYNTAX_ERROR, MARKUP_ERROR_INVALID_ATTRIBUTE },
        { "<color>text", MARKUP_RESULT_INCOMPLETE, MARKUP_ERROR_UNCLOSED_TAG },
        { "<1bad>text</1bad>", MARKUP_RESULT_SYNTAX_ERROR, MARKUP_ERROR_INVALID_TAG },
    };

    for (uint32_t i = 0; i < DM_ARRAY_SIZE(cases); ++i)
    {
        HMarkup     markup = (HMarkup)(uintptr_t)1;
        MarkupError error;
        ASSERT_EQ(cases[i].m_Result, MarkupCreate(cases[i].m_Text, strlen(cases[i].m_Text), &markup, &error));
        ASSERT_EQ((HMarkup)0, markup);
        ASSERT_EQ(cases[i].m_Error, error.m_Type);
    }
}

TEST(Markup, RejectsUnknownTagsAttributesAndConstants)
{
    struct UnsupportedMarkupCase
    {
        const char*     m_Text;
        MarkupErrorType m_Error;
        uint32_t        m_ByteOffset;
    };
    const UnsupportedMarkupCase cases[] = {
        { "<s ize=14>Text</size>", MARKUP_ERROR_UNKNOWN_TAG, 1 },
        { "<a href=https://defold.com>Defold</a>", MARKUP_ERROR_UNKNOWN_TAG, 1 },
        { "<unknown>Text</unknown>", MARKUP_ERROR_UNKNOWN_TAG, 1 },
        { "<size sdf=14>Text</size>", MARKUP_ERROR_UNKNOWN_ATTRIBUTE, 6 },
        { "<wave fit=word>Text</wave>", MARKUP_ERROR_INVALID_ATTRIBUTE_VALUE, 10 },
        { "<wave direction=backward>Text</wave>", MARKUP_ERROR_INVALID_ATTRIBUTE_VALUE, 16 },
        { "<ul pattern=dotted>Text</ul>", MARKUP_ERROR_INVALID_ATTRIBUTE_VALUE, 12 },
    };

    for (uint32_t i = 0; i < DM_ARRAY_SIZE(cases); ++i)
    {
        HMarkup     markup = (HMarkup)(uintptr_t)1;
        MarkupError error = {};
        ASSERT_EQ(MARKUP_RESULT_SYNTAX_ERROR, MarkupCreate(cases[i].m_Text, (uint32_t)strlen(cases[i].m_Text), &markup, &error));
        ASSERT_EQ((HMarkup)0, markup);
        ASSERT_EQ(cases[i].m_Error, error.m_Type);
        ASSERT_EQ(cases[i].m_ByteOffset, error.m_ByteOffset);
    }
}

TEST(Markup, AllowsApplicationDefinedObjectAttributes)
{
    const char text[] = "<sprite src=/logo.atlas animation=defold/><link src=https://defold.com data=manual>Defold</link>";
    HMarkup    markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(text, sizeof(text) - 1, &markup, 0));
    MarkupDestroy(markup);
}

TEST(Markup, SelfClosingTag)
{
    const char text[] = "A<sprite src=/icon.atlas width=2em/>B";
    HMarkup    markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(text, sizeof(text) - 1, &markup, 0));
    const uint32_t expected[] = { 'A', 0xfffc, 'B' };
    AssertText(markup, expected, DM_ARRAY_SIZE(expected));

    ASSERT_EQ(2u, MarkupGetStyleNodeCount(markup));
    const MarkupStyleNode& node = MarkupGetStyleNodes(markup)[1];
    AssertString(markup, node.m_Tag, "sprite");
    ASSERT_EQ((uint8_t)MARKUP_TAG_SPRITE,  node.m_Type);
    ASSERT_EQ(1u,                          node.m_TextOffset);
    ASSERT_EQ(1u,                          node.m_TextLength);
    ASSERT_EQ(2u,                          node.m_AttributeCount);
    const MarkupAttribute* attributes = MarkupGetAttributes(markup);
    ASSERT_EQ((uint8_t)MARKUP_ATTRIBUTE_CUSTOM, attributes[node.m_AttributeIndex].m_Type);
    ASSERT_EQ((uint8_t)MARKUP_ATTRIBUTE_WIDTH,  attributes[node.m_AttributeIndex + 1].m_Type);
    MarkupDestroy(markup);
}

TEST(Markup, EmptyPairedSpriteMatchesSelfClosingTag)
{
    const char self_closing_source[] = "A<sprite src=/icon.atlas width=2em/>B";
    const char paired_source[]       = "A<sprite src=/icon.atlas width=2em></sprite>B";
    HMarkup   self_closing_markup    = 0;
    HMarkup   paired_markup          = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(self_closing_source, sizeof(self_closing_source) - 1, &self_closing_markup, 0));
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(paired_source, sizeof(paired_source) - 1, &paired_markup, 0));

    const uint32_t expected[] = { 'A', 0xfffc, 'B' };
    AssertText(self_closing_markup, expected, DM_ARRAY_SIZE(expected));
    AssertText(paired_markup, expected, DM_ARRAY_SIZE(expected));
    ASSERT_EQ(MarkupGetSpanCount(self_closing_markup), MarkupGetSpanCount(paired_markup));
    ASSERT_EQ(MarkupGetStyleNodeCount(self_closing_markup), MarkupGetStyleNodeCount(paired_markup));

    const MarkupStyleNode& self_closing_node = MarkupGetStyleNodes(self_closing_markup)[1];
    const MarkupStyleNode& paired_node       = MarkupGetStyleNodes(paired_markup)[1];
    ASSERT_EQ(self_closing_node.m_Type, paired_node.m_Type);
    ASSERT_EQ(self_closing_node.m_Parent, paired_node.m_Parent);
    ASSERT_EQ(self_closing_node.m_TextOffset, paired_node.m_TextOffset);
    ASSERT_EQ(self_closing_node.m_TextLength, paired_node.m_TextLength);
    ASSERT_EQ(self_closing_node.m_AttributeCount, paired_node.m_AttributeCount);

    MarkupDestroy(self_closing_markup);
    MarkupDestroy(paired_markup);
}

TEST(Markup, NamedConstants)
{
    const char text[] = "<wave fit=span direction=reverse>X</wave><ul pattern=dashed>Y</ul>";
    HMarkup    markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(text, sizeof(text) - 1, &markup, 0));

    const MarkupStyleNode* nodes = MarkupGetStyleNodes(markup);
    const MarkupAttribute* attributes = MarkupGetAttributes(markup);
    ASSERT_EQ((uint8_t)MARKUP_CONSTANT_SPAN,    attributes[nodes[1].m_AttributeIndex].m_Constant);
    ASSERT_EQ((uint8_t)MARKUP_CONSTANT_REVERSE, attributes[nodes[1].m_AttributeIndex + 1].m_Constant);
    ASSERT_EQ((uint8_t)MARKUP_CONSTANT_DASHED,  attributes[nodes[2].m_AttributeIndex].m_Constant);
    MarkupDestroy(markup);
}

TEST(Markup, StrictLifo)
{
    const char  text[] = "<color=red><size=20>Hello</color></size>";
    HMarkup     markup = 0;
    MarkupError error;
    ASSERT_EQ(MARKUP_RESULT_SYNTAX_ERROR, MarkupCreate(text, sizeof(text) - 1, &markup, &error));
    ASSERT_EQ(MARKUP_ERROR_MISMATCHED_CLOSING_TAG, error.m_Type);
}

TEST(Markup, StyleFragmentAcceptsOpeningTagsOnly)
{
    const char source[] = "<color=#ff0000>\n    <shake amplitude=2>";
    HMarkup markup = 0;
    MarkupError error = {};
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreateStyleFragment(source, sizeof(source) - 1, &markup, &error));
    ASSERT_EQ(MARKUP_ERROR_NONE, error.m_Type);
    ASSERT_EQ(1u, MarkupGetTextLength(markup));
    ASSERT_EQ(1u, MarkupGetSpanCount(markup));
    ASSERT_EQ(3u, MarkupGetStyleNodeCount(markup));
    MarkupDestroy(markup);

    const char closing[] = "<color=#fff></color>";
    ASSERT_EQ(MARKUP_RESULT_SYNTAX_ERROR, MarkupCreateStyleFragment(closing, sizeof(closing) - 1, &markup, &error));
    ASSERT_EQ(MARKUP_ERROR_UNEXPECTED_CLOSING_TAG, error.m_Type);
    ASSERT_EQ((HMarkup)0, markup);
    const char text[] = "<color=#fff>text";
    ASSERT_EQ(MARKUP_RESULT_SYNTAX_ERROR, MarkupCreateStyleFragment(text, sizeof(text) - 1, &markup, &error));
    ASSERT_EQ(MARKUP_ERROR_INVALID_TAG, error.m_Type);
}

TEST(Markup, InvalidUtf8)
{
    const char  text[] = { (char)0xc0, (char)0x80 };
    HMarkup     markup = 0;
    MarkupError error;
    ASSERT_EQ(MARKUP_RESULT_INVALID_UTF8, MarkupCreate(text, sizeof(text), &markup, &error));
    ASSERT_EQ(MARKUP_ERROR_INVALID_UTF8, error.m_Type);
    ASSERT_EQ(0u, error.m_ByteOffset);
}

TEST(Markup, EveryPrefixOfLongValidText)
{
    const char text[] =
    "Start <wave amplitude=4 hz=2 wavelength=6>"
    "This <gradient left=#FF00FF right=#FFFFFF>Whole</gradient> Text Wobbles!"
    "</wave> End \xF0\x9F\x98\x80";
    const uint32_t text_length = sizeof(text) - 1;

    for (uint32_t length = 0; length <= text_length; ++length)
    {
        HMarkup      markup = 0;
        MarkupError  error;
        MarkupResult result = MarkupCreate(text, length, &markup, &error);

        if (result != MARKUP_RESULT_OK && result != MARKUP_RESULT_INCOMPLETE)
        {
            printf("Unexpected prefix result at byte %u: result=%d error=%d offset=%u\n", length, result, error.m_Type, error.m_ByteOffset);
        }

        ASSERT_TRUE(result == MARKUP_RESULT_OK || result == MARKUP_RESULT_INCOMPLETE);

        if (result == MARKUP_RESULT_OK)
        {
            MarkupDestroy(markup);
        }
        else
        {
            ASSERT_EQ((HMarkup)0, markup);
        }
    }

    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(text, text_length, &markup, 0));
    MarkupDestroy(markup);
}

TEST(Markup, ExplicitLengthDoesNotRequireTerminator)
{
    const char text[] = { '<', 'u', 'l', '>', 'O', 'K', '<', '/', 'u', 'l', '>' };
    HMarkup    markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(text, sizeof(text), &markup, 0));
    const uint32_t expected[] = { 'O', 'K' };
    AssertText(markup, expected, DM_ARRAY_SIZE(expected));
    MarkupDestroy(markup);
}

TEST(Markup, EveryPrefixOfLongTextWithManyTags)
{
    static const char segment[] =
    "<wave amplitude=4>A<gradient left=#FF00FF right=#FFFFFF>BC</gradient>D</wave>";
    static const uint32_t segment_count = 64;
    static const uint32_t segment_length = sizeof(segment) - 1;
    char                  text[segment_count * segment_length];

    for (uint32_t i = 0; i < segment_count; ++i)
    {
        memcpy(text + i * segment_length, segment, segment_length);
    }

    for (uint32_t length = 0; length <= sizeof(text); ++length)
    {
        HMarkup      markup = 0;
        MarkupResult result = MarkupCreate(text, length, &markup, 0);
        ASSERT_TRUE(result == MARKUP_RESULT_OK || result == MARKUP_RESULT_INCOMPLETE);

        if (markup)
        {
            MarkupDestroy(markup);
        }
    }

    HMarkup markup = 0;
    ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(text, sizeof(text), &markup, 0));
    ASSERT_EQ(segment_count * 4, MarkupGetTextLength(markup));
    ASSERT_EQ(1u + segment_count * 2, MarkupGetStyleNodeCount(markup));
    MarkupDestroy(markup);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);

    return jc_test_run_all();
}
