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

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/json.hpp>
#include <dlib/json_private.hpp>
#include <dlib/json_lexer.h>
#include <dlib/json_number.h>
#include <dlib/math.h>
#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

static void WriteJson(void* context, const char* value, uint32_t length)
{
    dmArray<char>& text = *(dmArray<char>*)context;
    if (text.Size() + length > text.Capacity())
    {
        text.SetCapacity(dmMath::Max(text.Capacity() * 2, text.Size() + length));
    }
    text.PushArray(value, length);
}

static void Append(dmArray<char>& text, const char* value)
{
    WriteJson(&text, value, (uint32_t)strlen(value));
}

static void Fields(dmArray<char>& text, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        char field[32];
        dmSnPrintf(field, sizeof(field), "%s\"k%u\":%u", i ? "," : "", i, i);
        Append(text, field);
    }
}

static void Array(dmArray<char>& text, uint32_t count)
{
    text.SetSize(0);
    text.SetCapacity(count * 8 + 2);
    Append(text, "[");
    for (uint32_t i = 0; i < count; ++i)
    {
        char value[16];
        dmSnPrintf(value, sizeof(value), "%s%u", i ? "," : "", i);
        Append(text, value);
    }
    Append(text, "]");
}

static uint32_t NextRandom(uint32_t* state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static bool ChildCountsMatch(const dmJson::Document& json)
{
    for (uint32_t i = 0; i < json.NodeCount(); ++i)
    {
        const dmJson::Node& node = json.Get(i);
        if (node.m_Type != dmJson::TYPE_ARRAY && node.m_Type != dmJson::TYPE_OBJECT)
        {
            continue;
        }
        uint32_t count = 0;
        for (int child = node.m_Container.m_First; child >= 0; child = json.Get(child).m_Next)
        {
            if ((uint32_t)child >= json.NodeCount() || ++count > json.NodeCount())
            {
                return false;
            }
        }
        if (node.m_Container.m_Count != count)
        {
            return false;
        }
    }
    return true;
}

// Generated C++ functions share C handles, enum identities and borrowed nodes.
TEST(dmJson, CApiInteroperability)
{
    dmJson::HDocument document = dmJson::Create(0);
    const char text[] = "{\"value\":42,\"string\":\"text\",\"flag\":true}";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(document, text, sizeof(text) - 1, 0, dmJson::PARSE_DECODE_VALUES));
    ASSERT_EQ(4u, dmJson::GetNodeCount(document));
    int value = dmJson::Field(document, 0, "value");
    ASSERT_TRUE(dmJson::GetNode(document, value) == JsonGetNode(document, value));
    ASSERT_STREQ("value", dmJson::GetName(document, value));
    ASSERT_EQ(42, dmJson::Integer(document, value, -1));
    uint32_t number = 0;
    ASSERT_TRUE(dmJson::UnsignedInteger(document, value, &number));
    ASSERT_EQ(42u, number);
    ASSERT_STREQ("text", dmJson::String(document, dmJson::Field(document, 0, "string"), 0));
    ASSERT_TRUE(dmJson::Boolean(document, dmJson::Field(document, 0, "flag"), false));
    ASSERT_TRUE(dmJson::GetText(document) == JsonGetText(document));
    dmJson::Destroy(document);

    dmJson::Document scoped(0);
    ASSERT_EQ(JSON_RESULT_OK, JsonParse(scoped.Handle(), "[1]", 3, 0, JSON_PARSE_DECODE_VALUES));
    ASSERT_EQ(1, scoped.Integer(scoped.Get(0).m_Container.m_First, -1));
    ASSERT_FALSE(scoped.Parse("{} x", 4, 0, dmJson::PARSE_DECODE_VALUES));
    ASSERT_FALSE(scoped.Parse("[1,", 3, 0, dmJson::PARSE_DECODE_VALUES));
    ASSERT_TRUE(scoped.Parse("{}", 2, 0, dmJson::PARSE_DECODE_VALUES));
}

// Containers retain direct child counts, including empty containers, in both
// modes. Reusing the document after growth or failure must discard old counts.
TEST(dmJson, DirectChildCounts)
{
    const char text[] = "{\"array\":[1,{\"nested\":[true,null]},[],{}],\"empty\":{},\"string\":\"value\"}";
    dmJson::Document json(0);
    for (uint32_t i = 0; i < 4; ++i)
    {
        dmJson::ParseMode mode = i % 2 ? dmJson::PARSE_PRESERVE_VALUES : dmJson::PARSE_DECODE_VALUES;
        ASSERT_TRUE(json.Parse(text, sizeof(text) - 1, 0, mode));
        ASSERT_TRUE(ChildCountsMatch(json));
        ASSERT_EQ(3u, json.Get(0).m_Container.m_Count);
        int array = json.Field(0, "array");
        ASSERT_EQ(4u, json.Get(array).m_Container.m_Count);
        int object = json.Get(json.Get(array).m_Container.m_First).m_Next;
        ASSERT_EQ(1u, json.Get(object).m_Container.m_Count);
        ASSERT_EQ(2u, json.Get(json.Field(object, "nested")).m_Container.m_Count);
        ASSERT_EQ(0u, json.Get(json.Field(0, "empty")).m_Container.m_Count);

        ASSERT_TRUE(json.Parse("[]", 2, 0, mode));
        ASSERT_EQ(0u, json.Get(0).m_Container.m_Count);
        ASSERT_FALSE(json.Parse("[true,", 6, 0, mode));
        ASSERT_TRUE(json.Parse("[false]", 7, 0, mode));
        ASSERT_EQ(1u, json.Get(0).m_Container.m_Count);
        ASSERT_TRUE(json.Parse("{}", 2, 0, mode));
        ASSERT_EQ(0u, json.Get(0).m_Container.m_Count);
    }
    dmArray<char> text_array;
    Array(text_array, 70000);
    ASSERT_TRUE(json.Parse(text_array.Begin(), text_array.Size(), 0, dmJson::PARSE_DECODE_VALUES));
    ASSERT_EQ(70000u, json.Get(0).m_Container.m_Count);
    ASSERT_TRUE(ChildCountsMatch(json));
    ASSERT_TRUE(json.Parse("[]", 2, 0, dmJson::PARSE_PRESERVE_VALUES));
    ASSERT_EQ(0u, json.Get(0).m_Container.m_Count);
}

// Preserve every value when an array grows beyond its initial capacity;
// subsequent parses must discard the previous document.
TEST(dmJson, InitialCapacityAndGrowingArray)
{
    dmJson::Document json(32);
    uint32_t initial_capacity = json.Handle()->m_Nodes.Capacity();
    ASSERT_LT(initial_capacity, 1024u);
    dmArray<char> text;
    Array(text, 1024);
    ASSERT_TRUE(json.Parse(text.Begin(), text.Size(), 0, dmJson::PARSE_DECODE_VALUES));
    ASSERT_EQ(1025u, json.NodeCount());
    ASSERT_GT(json.Handle()->m_Nodes.Capacity(), initial_capacity);
    int value = json.Get(0).m_Container.m_First;
    for (int i = 0; i < 1024; ++i)
    {
        ASSERT_GE(value, 0);
        ASSERT_EQ(i, json.Integer(value, -1));
        value = json.Get(value).m_Next;
    }
    ASSERT_EQ(-1, value);
    const char* small_json = "{\"value\":42}";
    ASSERT_TRUE(json.Parse(small_json, (uint32_t)strlen(small_json), 0, dmJson::PARSE_DECODE_VALUES));
    ASSERT_EQ(2u, json.NodeCount());
    ASSERT_EQ(42, json.Integer(json.Field(0, "value"), -1));
}

// Offset-based names retain empty and escaped keys through node/text growth and
// document reuse. Array members and the root must remain unnamed in both modes.
TEST(dmJson, CompactNameOffsets)
{
    ASSERT_EQ((size_t)32, sizeof(dmJson::Node));
    dmJson::Document json(0);
    dmArray<char> values;
    Array(values, 128);
    dmArray<char> text;
    Append(text, "{\"\":1,\"\\u006bey\":{\"nested\":2},\"array\":");
    WriteJson(&text, values.Begin(), values.Size());
    Append(text, "}");
    for (uint32_t pass = 0; pass < 4; ++pass)
    {
        dmJson::ParseMode mode = pass & 1 ? dmJson::PARSE_PRESERVE_VALUES : dmJson::PARSE_DECODE_VALUES;
        ASSERT_TRUE(json.Parse("{\"old\":42}", 10, 0, mode));
        ASSERT_TRUE(json.Parse(text.Begin(), text.Size(), 0, mode));
        ASSERT_GT(json.Handle()->m_Nodes.Capacity(), 32u);
        ASSERT_EQ(-1, json.Field(0, "old"));
        ASSERT_EQ(0u, json.Get(0).m_NameOffset);
        ASSERT_TRUE(json.Name(0) == 0);
        ASSERT_STREQ("", json.Name(json.Field(0, "")));
        int key = json.Field(0, "key");
        ASSERT_GE(key, 0);
        ASSERT_STREQ("key", json.Name(key));
        ASSERT_EQ(8u, json.Get(key).m_NameLength);
        ASSERT_EQ(0, memcmp(text.Begin() + json.Get(key).m_NameOffset, "\\u006bey", 8));
        ASSERT_STREQ("nested", json.Name(json.Field(key, "nested")));
        int array = json.Field(0, "array");
        ASSERT_EQ(128u, json.Get(array).m_Container.m_Count);
        for (int child = json.Get(array).m_Container.m_First; child >= 0; child = json.Get(child).m_Next)
        {
            ASSERT_EQ(0u, json.Get(child).m_NameOffset);
            ASSERT_TRUE(json.Name(child) == 0);
        }
    }
}

// Reused text and nodes must discard previous values across growth, smaller
// documents, parse modes and failed parses without reading stale trailing bytes.
TEST(dmJson, ReusedDocumentStorage)
{
    dmJson::Document json(0);
    ASSERT_EQ(0u, json.Handle()->m_TextCapacity);
    ASSERT_TRUE(json.Text() == 0);
    ASSERT_EQ(0u, json.Handle()->m_Nodes.Capacity());
    dmArray<char> text;
    Array(text, 128);
    ASSERT_TRUE(json.Parse(text.Begin(), text.Size(), 0, dmJson::PARSE_DECODE_VALUES));
    const char* storage = json.Text();
    uint32_t capacity = json.Handle()->m_TextCapacity;
    uint32_t node_capacity = json.Handle()->m_Nodes.Capacity();

    const char small_json[] = "{\"value\":\"a\\nb\"}";
    ASSERT_TRUE(json.Parse(small_json, sizeof(small_json) - 1, 0, dmJson::PARSE_DECODE_VALUES));
    ASSERT_EQ(2u, json.NodeCount());
    ASSERT_STREQ("a\nb", json.String(json.Field(0, "value"), 0));
    const char metadata[] = "{\"value\":\"a\\u0000b\",\"number\":1e400}";
    ASSERT_TRUE(json.Parse(metadata, sizeof(metadata) - 1, 0, dmJson::PARSE_PRESERVE_VALUES));
    ASSERT_EQ(3u, json.NodeCount());
    ASSERT_STREQ("raw", json.String(json.Field(0, "value"), "raw"));
    const dmJson::Node& value = json.Get(json.Field(0, "value"));
    ASSERT_EQ(0, memcmp(metadata + value.m_Offset, json.Text() + value.m_Offset, value.m_Length));

    uint32_t offset;
    ASSERT_FALSE(json.Parse("\"x", 2, &offset, dmJson::PARSE_DECODE_VALUES));
    ASSERT_EQ(2u, offset);
    ASSERT_FALSE(json.Parse("\"\xff\"", 3, &offset, dmJson::PARSE_DECODE_VALUES));
    ASSERT_EQ(1u, offset);
    ASSERT_FALSE(json.Parse("", 0, &offset, dmJson::PARSE_DECODE_VALUES));
    ASSERT_EQ(0u, offset);
    ASSERT_TRUE(json.Parse("{}", 2, 0, dmJson::PARSE_DECODE_VALUES));
    ASSERT_EQ(1u, json.NodeCount());
    ASSERT_EQ(-1, json.Get(0).m_Container.m_First);
    ASSERT_EQ(-1, json.Field(0, "number"));
    ASSERT_TRUE(storage == json.Text());
    ASSERT_EQ(capacity, json.Handle()->m_TextCapacity);
    ASSERT_EQ(node_capacity, json.Handle()->m_Nodes.Capacity());

    uint32_t size = capacity + 17;
    text.SetCapacity(size);
    text.SetSize(size);
    memset(text.Begin(), 'a', size);
    text[0] = text[size - 1] = '"';
    ASSERT_TRUE(json.Parse(text.Begin(), size, 0, dmJson::PARSE_DECODE_VALUES));
    ASSERT_EQ(size - 2, strlen(json.String(0, 0)));
    ASSERT_EQ(size, json.Get(0).m_Length);
    ASSERT_GE(json.Handle()->m_TextCapacity, size + 1);
    storage = json.Text();
    capacity = json.Handle()->m_TextCapacity;
    ASSERT_TRUE(json.Parse("[]", 2, 0, dmJson::PARSE_DECODE_VALUES));
    ASSERT_EQ(1u, json.NodeCount());
    ASSERT_EQ(-1, json.Get(0).m_Container.m_First);
    ASSERT_TRUE(storage == json.Text());
    ASSERT_EQ(capacity, json.Handle()->m_TextCapacity);
    ASSERT_EQ(0, json.Text()[2]);
}

// Grow beyond the former 1 MiB limit, reuse the allocation for smaller inputs,
// and reject a size whose terminating NUL would overflow before reading input.
TEST(dmJson, ReusedTextStorageGrowth)
{
    dmJson::Document json(0);
    dmArray<char> text;
    text.SetCapacity(2 * 1024 * 1024 + 17);
    text.SetSize(text.Capacity());
    const uint32_t sizes[] = { 768 * 1024, 1024 * 1024, 1024 * 1024 + 1, text.Size() };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(sizes); ++i)
    {
        uint32_t size = sizes[i];
        memset(text.Begin(), 'a', size);
        text[0] = text[size - 1] = '"';
        ASSERT_TRUE(json.Parse(text.Begin(), size, 0, dmJson::PARSE_DECODE_VALUES));
        ASSERT_EQ(size - 2, strlen(json.String(0, 0)));
        ASSERT_GE(json.Handle()->m_TextCapacity, size + 1);
    }
    const char* storage = json.Text();
    uint32_t capacity = json.Handle()->m_TextCapacity;
    uint32_t offset;
    ASSERT_FALSE(json.Parse("", UINT32_MAX, &offset, dmJson::PARSE_DECODE_VALUES));
    ASSERT_EQ(0u, offset);
    ASSERT_TRUE(json.Parse("null", 4, 0, dmJson::PARSE_DECODE_VALUES));
    ASSERT_EQ(dmJson::TYPE_NULL, json.Get(0).m_Type);
    ASSERT_TRUE(storage == json.Text());
    ASSERT_EQ(capacity, json.Handle()->m_TextCapacity);
}

// Explicit reserves, including a zero reserve, must grow safely while mixed node types retain their values.
TEST(dmJson, ReservedCapacityAndMixedValues)
{
    const uint32_t capacities[] = { 0, 1, 16, 512 };
    const char text[] = "{\"array\":[42,\"value\",true,false,null,{},[]],\"empty\":\"\"}";
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(capacities); ++i)
    {
        dmJson::Document json(capacities[i]);
        ASSERT_EQ(capacities[i], json.Handle()->m_Nodes.Capacity());
        ASSERT_TRUE(json.Parse(text, sizeof(text) - 1, 0, dmJson::PARSE_DECODE_VALUES));
        int value = json.Get(json.Field(0, "array")).m_Container.m_First;
        ASSERT_EQ(42, json.Integer(value, -1));
        value = json.Get(value).m_Next;
        ASSERT_STREQ("value", json.String(value, 0));
        ASSERT_EQ(-7, json.Integer(value, -7));
        value = json.Get(value).m_Next;
        ASSERT_TRUE(json.Boolean(value, false));
        value = json.Get(value).m_Next;
        ASSERT_FALSE(json.Boolean(value, true));
        value = json.Get(value).m_Next;
        ASSERT_EQ(dmJson::TYPE_NULL, json.Get(value).m_Type);
        ASSERT_EQ(-7, json.Integer(value, -7));
        value = json.Get(value).m_Next;
        ASSERT_EQ(dmJson::TYPE_OBJECT, json.Get(value).m_Type);
        ASSERT_EQ(-1, json.Get(value).m_Container.m_First);
        value = json.Get(value).m_Next;
        ASSERT_EQ(dmJson::TYPE_ARRAY, json.Get(value).m_Type);
        ASSERT_EQ(-1, json.Get(value).m_Container.m_First);
        ASSERT_EQ(-1, json.Get(value).m_Next);
        ASSERT_STREQ("", json.String(json.Field(0, "empty"), 0));
        ASSERT_EQ(-1, json.Get(-1).m_Container.m_First);
        ASSERT_EQ(-7, json.Integer(-1, -7));
    }
}

// Field links and lookup results must survive both the small-object/index
// transition and hash-table/node-array growth in large objects.
TEST(dmJson, WideObjectFields)
{
    const uint32_t counts[] = { 16, 17, 32, 33, 512, 5000 };
    dmJson::Document json(32);
    dmArray<char>    text;
    for (uint32_t c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c)
    {
        text.SetSize(0);
        Append(text, "{");
        Fields(text, counts[c]);
        Append(text, "}");
        ASSERT_TRUE(json.Parse(text.Begin(), text.Size(), 0, dmJson::PARSE_DECODE_VALUES));
        ASSERT_EQ(counts[c] + 1, json.NodeCount());
        ASSERT_EQ(counts[c], json.Get(0).m_Container.m_Count);
        int field = json.Get(0).m_Container.m_First;
        for (uint32_t i = 0; i < counts[c]; ++i)
        {
            char name[32];
            snprintf(name, sizeof(name), "k%u", i);
            ASSERT_GE(field, 0);
            ASSERT_STREQ(name, json.Name(field));
            ASSERT_EQ((int)i, json.Integer(field, -1));
            field = json.Get(field).m_Next;
        }
        ASSERT_EQ(-1, field);
        ASSERT_EQ(0, json.Integer(json.Field(0, "k0"), -1));
        ASSERT_EQ(-1, json.Field(0, "missing"));
    }
}

// Detect duplicate decoded names before and after indexing, including early
// and late keys and an escaped spelling that hashes identically after decoding.
TEST(dmJson, DuplicateDecodedKeys)
{
    const uint32_t counts[] = { 1, 16, 17, 512, 5000 };
    dmJson::Document json(32);
    dmArray<char>    text;
    for (uint32_t c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c)
    {
        for (uint32_t variant = 0; variant < 3; ++variant)
        {
            text.SetSize(0);
            Append(text, "{");
            Fields(text, counts[c]);
            if (variant == 0)
            {
                Append(text, ",\"k0\":999}");
            }
            else if (variant == 1)
            {
                char field[32];
                dmSnPrintf(field, sizeof(field), ",\"k%u\":999}", counts[c] - 1);
                Append(text, field);
            }
            else
            {
                Append(text, ",\"\\u006b0\":999}");
            }
            ASSERT_FALSE(json.Parse(text.Begin(), text.Size(), 0, dmJson::PARSE_DECODE_VALUES));
        }
    }
    text.SetSize(0);
    Append(text, "{\"\":1,");
    Fields(text, 32);
    Append(text, ",\"\":2}");
    ASSERT_FALSE(json.Parse(text.Begin(), text.Size(), 0, dmJson::PARSE_DECODE_VALUES));
}

// Identical names in different nested objects are valid. Index state must be
// scoped to each object and released after a failed parse and between parses.
TEST(dmJson, NestedObjectKeyScope)
{
    dmJson::Document json(32);
    dmArray<char>    text;
    Append(text, "{");
    Fields(text, 32);
    Append(text, ",\"nested\":{");
    Fields(text, 32);
    Append(text, "},\"rows\":[{");
    Fields(text, 32);
    Append(text, "},{");
    Fields(text, 32);
    Append(text, "}]}");
    for (uint32_t i = 0; i < 2; ++i)
    {
        ASSERT_TRUE(json.Parse(text.Begin(), text.Size(), 0, dmJson::PARSE_DECODE_VALUES));
        int nested = json.Field(0, "nested");
        ASSERT_EQ(31, json.Integer(json.Field(nested, "k31"), -1));
        int rows = json.Field(0, "rows");
        int first = json.Get(rows).m_Container.m_First;
        ASSERT_EQ(0, json.Integer(json.Field(first, "k0"), -1));
        ASSERT_EQ(31, json.Integer(json.Field(json.Get(first).m_Next, "k31"), -1));
        dmArray<char> invalid;
        Append(invalid, "{\"nested\":{");
        Fields(invalid, 32);
        Append(invalid, ",\"k31\":99}}");
        ASSERT_FALSE(json.Parse(invalid.Begin(), invalid.Size(), 0, dmJson::PARSE_DECODE_VALUES));
    }
}

// Node storage grows beyond the former 65,536-node limit without losing links
// or values, then remains reusable after invalid and smaller documents.
TEST(dmJson, LargeNodeStorage)
{
    dmJson::Document json(32);
    dmArray<char>    text;
    Array(text, 65536 * 2);
    ASSERT_TRUE(json.Parse(text.Begin(), text.Size(), 0, dmJson::PARSE_DECODE_VALUES));
    ASSERT_EQ(65536u * 2 + 1, json.NodeCount());
    ASSERT_EQ(65536u * 2, json.Get(0).m_Container.m_Count);
    int value = json.Get(0).m_Container.m_First;
    for (int i = 0; i < 65536 * 2; ++i)
    {
        ASSERT_GE(value, 0);
        ASSERT_EQ(i, json.Integer(value, -1));
        value = json.Get(value).m_Next;
    }
    ASSERT_EQ(-1, value);
    uint32_t capacity = json.Handle()->m_Nodes.Capacity();
    ASSERT_FALSE(json.Parse("[", 1, 0, dmJson::PARSE_DECODE_VALUES));
    const char* small_json = "{\"valid\":true}";
    ASSERT_TRUE(json.Parse(small_json, (uint32_t)strlen(small_json), 0, dmJson::PARSE_DECODE_VALUES));
    ASSERT_TRUE(json.Boolean(json.Field(0, "valid"), false));
    ASSERT_EQ(capacity, json.Handle()->m_Nodes.Capacity());
}

// Dynamic document growth must retain the recursive parser's stack guard and
// allow reuse after a document exceeds the supported nesting depth.
TEST(dmJson, NestingDepth)
{
    dmJson::Document json(32);
    dmArray<char> text;
    for (uint32_t depth = 64; depth <= 65; ++depth)
    {
        text.SetSize(0);
        for (uint32_t i = 0; i < depth; ++i)
        {
            Append(text, "[");
        }
        Append(text, "0");
        for (uint32_t i = 0; i < depth; ++i)
        {
            Append(text, "]");
        }
        ASSERT_EQ(depth == 64, json.Parse(text.Begin(), text.Size(), 0, dmJson::PARSE_DECODE_VALUES));
    }
    ASSERT_TRUE(json.Parse("null", 4, 0, dmJson::PARSE_DECODE_VALUES));
}

// The shared formatter writes fixed expected bytes within the caller's span,
// including signed zero, midpoint rounding and the finite binary64 extremes.
TEST(dmJson, WriteNumberBuffer)
{
    const struct
    {
        double      m_Value;
        const char* m_Text;
    } cases[] = {
        { 0.0, "0" },
        { -0.0, "-0" },
        { 1.0, "1" },
        { -1.0, "-1" },
        { 1001.0, "1001" },
        { 99999999999999.0, "99999999999999" },
        { 1.010101010101, "1.010101010101" },
        { 0.0001, "0.0001" },
        { 0.00001, "1e-05" },
        { 1e14, "1e+14" },
        { 100000000000005.0, "1e+14" },
        { 100000000000015.0, "1.0000000000002e+14" },
        { 999999999999995.0, "1e+15" },
        { DBL_MIN, "2.2250738585072e-308" },
        { -DBL_MIN, "-2.2250738585072e-308" },
        { ldexp(1.0, -1074), "4.9406564584125e-324" },
        { -ldexp(1.0, -1074), "-4.9406564584125e-324" },
        { DBL_MAX, "1.7976931348623e+308" },
        { -DBL_MAX, "-1.7976931348623e+308" },
    };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(cases); ++i)
    {
        char storage[dmJson::NUMBER_BUFFER_SIZE + 2];
        memset(storage, '#', sizeof(storage));
        char* text = storage + 1;
        uint32_t size = dmJson::WriteNumber(cases[i].m_Value, text);
        ASSERT_LE(size, dmJson::NUMBER_BUFFER_SIZE);
        ASSERT_EQ(strlen(cases[i].m_Text), size);
        ASSERT_EQ(0, memcmp(cases[i].m_Text, text, size));
        ASSERT_EQ('#', storage[0]);
        ASSERT_EQ('#', storage[sizeof(storage) - 1]);
    }
}

// Integer conversion and its floating-point fallback preserve double precision,
// signed zero and source spans, including values beyond the integer accumulator's range.
TEST(dmJson, NumberConversion)
{
    const struct
    {
        const char* m_Text;
        double      m_Value;
    } cases[] = {
        { "0", 0.0 },
        { "-0", -0.0 },
        { "1", 1.0 },
        { "-1", -1.0 },
        { "2147483647", 2147483647.0 },
        { "-2147483648", -2147483648.0 },
        { "4294967295", 4294967295.0 },
        { "999999999999999", 999999999999999.0 },
        { "-999999999999999", -999999999999999.0 },
        { "1000000000000000", 1000000000000000.0 },
        { "9007199254740991", 9007199254740991.0 },
        { "9007199254740992", 9007199254740992.0 },
        { "9007199254740993", 9007199254740992.0 },
        { "9007199254740995", 9007199254740996.0 },
        { "18446744073709551615", 18446744073709551616.0 },
        { "18446744073709551616", 18446744073709551616.0 },
        { "18446744073709551617", 18446744073709551616.0 },
        { "-18446744073709551617", -18446744073709551616.0 },
        { "-0.0", -0.0 },
        { "-0e10", -0.0 },
        { "999999999999999.5", 999999999999999.5 },
        { "999999999999999e-1", 99999999999999.9 },
    };
    dmJson::Document json(32);
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(cases); ++i)
    {
        uint32_t size = (uint32_t)strlen(cases[i].m_Text);
        uint32_t offset;
        ASSERT_TRUE(json.Parse(cases[i].m_Text, size, &offset, dmJson::PARSE_DECODE_VALUES));
        ASSERT_EQ(size, offset);
        ASSERT_EQ(dmJson::TYPE_NUMBER, json.Get(0).m_Type);
        ASSERT_EQ(cases[i].m_Value, json.Get(0).m_Number);
        ASSERT_EQ(0, memcmp(&cases[i].m_Value, &json.Get(0).m_Number, sizeof(double)));
        ASSERT_EQ(0u, json.Get(0).m_Offset);
        ASSERT_EQ(size, json.Get(0).m_Length);
    }
}

// Decimal conversion rounds halfway cases to even, including subnormal, normal
// and overflow boundaries, while preserve mode accepts every valid number token.
TEST(dmJson, DecimalRoundingBoundaries)
{
    const struct
    {
        const char* m_Text;
        uint64_t    m_Bits;
    } cases[] = {
        // Normal values around rounding midpoints, with carries from low digits.
        { "7540734479557463550e-129", UINT64_C(0x29122280444a178f) },
        { "9110134847227034417e-100", UINT64_C(0x2f1ba730b2d3956a) },
        { "1748112591483723627e261", UINT64_C(0x79e8a75db49aa61a) },
        { "3364025406290504086e106", UINT64_C(0x59c971bfb10557e3) },
        { "1304028409783462827e-159", UINT64_C(0x22afcdb295a07a4c) },
        { "8275182978025965278e-234", UINT64_C(0x1346d24edd088ee6) },
        { "9616741057934083115e61", UINT64_C(0x5089f424cc062de4) },
        { "4960893288971496669e47", UINT64_C(0x4d92d7b496d37959) },
        { "1.00000000000000011102230246251565404236316680908203124", UINT64_C(0x3ff0000000000000) },
        { "1.00000000000000011102230246251565404236316680908203125", UINT64_C(0x3ff0000000000000) },
        { "1.00000000000000011102230246251565404236316680908203126", UINT64_C(0x3ff0000000000001) },
        { "1.00000000000000033306690738754696212708950042724609375", UINT64_C(0x3ff0000000000002) },
        { "2.4703282292062327e-324", UINT64_C(0x0000000000000000) },
        { "-2.4703282292062327e-324", UINT64_C(0x8000000000000000) },
        { "2.4703282292062328e-324", UINT64_C(0x0000000000000001) },
        { "4.9406564584124654e-324", UINT64_C(0x0000000000000001) },
        { "2.2250738585072011e-308", UINT64_C(0x000fffffffffffff) },
        { "2.2250738585072012e-308", UINT64_C(0x0010000000000000) },
        { "1.7976931348623157e308", UINT64_C(0x7fefffffffffffff) },
        { "1.7976931348623158e308", UINT64_C(0x7fefffffffffffff) },
        { "1.7976931348623159e308", UINT64_C(0x7ff0000000000000) },
        { "1e999999999999999999999", UINT64_C(0x7ff0000000000000) },
        { "-1e-999999999999999999999", UINT64_C(0x8000000000000000) },
        { "-0e999999999999999999999", UINT64_C(0x8000000000000000) },
    };
    dmJson::Document json(0);
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(cases); ++i)
    {
        uint32_t size = (uint32_t)strlen(cases[i].m_Text);
        bool finite = (cases[i].m_Bits & UINT64_C(0x7ff0000000000000)) != UINT64_C(0x7ff0000000000000);
        ASSERT_EQ(finite, json.Parse(cases[i].m_Text, size, 0, dmJson::PARSE_DECODE_VALUES));
        if (finite)
        {
            ASSERT_EQ(0, memcmp(&cases[i].m_Bits, &json.Get(0).m_Number, sizeof(double)));
        }
        ASSERT_TRUE(json.Parse(cases[i].m_Text, size, 0, dmJson::PARSE_PRESERVE_VALUES));
    }
}

// Every cached decimal power must preserve libc rounding, including derived metadata at the range endpoints.
TEST(dmJson, DecimalPowerRange)
{
    char* previous = strdup(setlocale(LC_NUMERIC, 0));
    setlocale(LC_NUMERIC, "C");
    const char* coefficients[] = { "1", "9007199254740993", "9999999999999999999", "18446744073709551615" };
    dmJson::Document json(0);
    for (int exponent = -342; exponent <= 308; ++exponent)
    {
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(coefficients); ++i)
        {
            char text[64];
            uint32_t size = dmSnPrintf(text, sizeof(text), "%se%d", coefficients[i], exponent);
            double expected = strtod(text, 0);
            bool accepted = json.Parse(text, size, 0, dmJson::PARSE_DECODE_VALUES);
            EXPECT_EQ(!isinf(expected), accepted);
            if (accepted)
            {
                EXPECT_EQ(0, memcmp(&expected, &json.Get(0).m_Number, sizeof(double)));
            }
        }
    }
    setlocale(LC_NUMERIC, previous);
    free(previous);
}

// Digits beyond the exact fallback's retained prefix must distinguish an exact
// halfway value from a value just above it, for both positive and negative input.
TEST(dmJson, LongDecimalRounding)
{
    dmJson::Document json(0);
    dmArray<char> text;
    for (uint32_t negative = 0; negative < 2; ++negative)
    {
        text.SetSize(0);
        Append(text, negative ? "-" : "");
        Append(text, "1.00000000000000011102230246251565404236316680908203125");
        for (uint32_t i = 0; i < 900; ++i)
        {
            Append(text, "0");
        }
        for (uint32_t tail = 0; tail < 2; ++tail)
        {
            if (tail)
            {
                Append(text, "1");
            }
            ASSERT_TRUE(json.Parse(text.Begin(), text.Size(), 0, dmJson::PARSE_DECODE_VALUES));
            uint64_t expected = (UINT64_C(0x3ff0000000000000) + tail) | ((uint64_t)negative << 63);
            ASSERT_EQ(0, memcmp(&expected, &json.Get(0).m_Number, sizeof(double)));
            ASSERT_TRUE(json.Parse(text.Begin(), text.Size(), 0, dmJson::PARSE_PRESERVE_VALUES));
        }
    }
}

// Large exponents can cancel thousands of integer or fractional digits; range
// checks must use the complete decimal position without overflowing arithmetic.
TEST(dmJson, LongDecimalExponents)
{
    dmJson::Document json(0);
    dmArray<char> text;
    const uint32_t sizes[] = { 801, 4096 };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(sizes); ++i)
    {
        for (uint32_t fraction = 0; fraction < 2; ++fraction)
        {
            text.SetSize(0);
            Append(text, fraction ? "0." : "1");
            for (uint32_t n = 0; n < sizes[i]; ++n)
            {
                Append(text, "0");
            }
            char suffix[32];
            dmSnPrintf(suffix, sizeof(suffix), fraction ? "1e%u" : "e-%u", sizes[i] + fraction);
            Append(text, suffix);
            ASSERT_TRUE(json.Parse(text.Begin(), text.Size(), 0, dmJson::PARSE_DECODE_VALUES));
            ASSERT_EQ(1.0, json.Get(0).m_Number);
            ASSERT_TRUE(json.Parse(text.Begin(), text.Size(), 0, dmJson::PARSE_PRESERVE_VALUES));
        }
    }
}

// Number-scan metadata must preserve strtod rounding, signed zero and range
// behavior, including long tokens whose integer accumulator wraps.
TEST(dmJson, NumericDifferential)
{
    char* previous = strdup(setlocale(LC_NUMERIC, 0));
    setlocale(LC_NUMERIC, "C");
    uint32_t random = 0xdeadbeef;
    dmJson::Document json(0);
    for (uint32_t i = 0; i < 12000; ++i)
    {
        char token[128];
        char* out = token;
        if (NextRandom(&random) & 0x100)
        {
            *out++ = '-';
        }
        uint32_t digits = 1 + NextRandom(&random) % 40;
        uint32_t point = NextRandom(&random) % digits;
        *out++ = '1' + NextRandom(&random) % 9;
        for (uint32_t j = 1; j < digits; ++j)
        {
            if (j == point)
            {
                *out++ = '.';
            }
            *out++ = '0' + NextRandom(&random) % 10;
        }
        if (i % 3)
        {
            out += dmSnPrintf(out, sizeof(token) - (size_t)(out - token), "e%d", (int)(NextRandom(&random) % 700) - 370);
        }
        *out = 0;
        double expected = strtod(token, 0);
        bool accepted = json.Parse(token, (uint32_t)(out - token), 0, dmJson::PARSE_DECODE_VALUES);
        // Nonfatal checks ensure the original locale is restored on failure.
        EXPECT_EQ((bool)isfinite(expected), accepted);
        if (accepted)
        {
            EXPECT_EQ(0, memcmp(&expected, &json.Get(0).m_Number, sizeof(double)));
        }
        EXPECT_TRUE(json.Parse(token, (uint32_t)(out - token), 0, dmJson::PARSE_PRESERVE_VALUES));
    }
    setlocale(LC_NUMERIC, previous);
    free(previous);
}

// Conversion remains locale-independent when the process locale changes between parses.
TEST(dmJson, LocaleChangesBetweenParses)
{
    char* previous = strdup(setlocale(LC_NUMERIC, 0));
    dmJson::Document json(32);
    const char* locales[] = { "C", "sv_SE.UTF-8", "C", "de_DE.UTF-8", "C" };
    const char* text = "[1.25,2.5e1,9007199254740993,-0.0]";
    const double expected[] = { 1.25, 25.0, 9007199254740992.0, -0.0 };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(locales); ++i)
    {
        if (!setlocale(LC_NUMERIC, locales[i]))
        {
            continue;
        }
        bool accepted = json.Parse(text, (uint32_t)strlen(text), 0, dmJson::PARSE_DECODE_VALUES);
        EXPECT_TRUE(accepted);
        if (accepted)
        {
            int child = json.Get(0).m_Container.m_First;
            for (uint32_t n = 0; n < DM_ARRAY_SIZE(expected); ++n)
            {
                EXPECT_EQ(0, memcmp(&expected[n], &json.Get(child).m_Number, sizeof(double)));
                child = json.Get(child).m_Next;
            }
        }
    }
    setlocale(LC_NUMERIC, previous);
    free(previous);
}

// JSON decimals and exponents retain full precision, and commas remain token boundaries, after a locale change.
TEST(dmJson, NumericLocale)
{
    char* previous = strdup(setlocale(LC_NUMERIC, 0));
    const char* names[] = { "sv_SE.UTF-8", "sv_SE.utf8", "Swedish_Sweden.1252", "de_DE.UTF-8" };
    bool comma_locale = false;
    for (uint32_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        if (setlocale(LC_NUMERIC, names[i]) && !strcmp(localeconv()->decimal_point, ","))
        {
            comma_locale = true;
            break;
        }
    }
    if (!comma_locale)
    {
        setlocale(LC_NUMERIC, previous);
        free(previous);
        SKIP();
    }
    dmJson::Document json(32);
    const char* text = "[1,2,640.0,3.6e2,-1.25,1.2e-3,1.2345678901234567e100,2147483647.0]";
    const double values[] = { 1.0, 2.0, 640.0, 360.0, -1.25, 0.0012, 1.2345678901234567e100, 2147483647.0 };
    // Nonfatal checks ensure the original locale is restored even if parsing regresses.
    EXPECT_TRUE(json.Parse(text, (uint32_t)strlen(text), 0, dmJson::PARSE_DECODE_VALUES));
    int node = json.Get(0).m_Container.m_First;
    for (uint32_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
    {
        EXPECT_EQ(dmJson::TYPE_NUMBER, json.Get(node).m_Type);
        EXPECT_EQ(values[i], json.Get(node).m_Number);
        node = json.Get(node).m_Next;
    }
    EXPECT_EQ(-1, node);
    EXPECT_STREQ(",", localeconv()->decimal_point);
    setlocale(LC_NUMERIC, previous);
    free(previous);
}

// Signed integer boundaries must survive float-to-int lowering, including Emscripten's legacy browser path.
TEST(dmJson, SignedIntegerBoundaries)
{
    const char text[] = "[-2147483648,-2147483647,-2147483646,2147483646,2147483647]";
    const int expected[] = { INT32_MIN, INT32_MIN + 1, INT32_MIN + 2, INT32_MAX - 1, INT32_MAX };
    dmJson::Document json(32);
    ASSERT_TRUE(json.Parse(text, sizeof(text) - 1, 0, dmJson::PARSE_DECODE_VALUES));
    int node = json.Get(0).m_Container.m_First;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(expected); ++i)
    {
        ASSERT_EQ(expected[i], json.Integer(node, 0));
        node = json.Get(node).m_Next;
    }
    ASSERT_EQ(-1, node);
}

// Unsigned IDs retain all 32 bits and reject out-of-range, fractional and nonnumeric values.
TEST(dmJson, UnsignedIntegers)
{
    const char* valid[] = { "0", "2147483648", "4294967295", "4294967295.0", "1e2" };
    const uint32_t expected[] = { 0u, 2147483648u, 4294967295u, 4294967295u, 100u };
    const char* invalid[] = { "-1", "4294967296", "1.5", "true", "\"1\"", "null" };
    dmJson::Document json(32);
    uint32_t value;
    for (uint32_t i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i)
    {
        ASSERT_TRUE(json.Parse(valid[i], (uint32_t)strlen(valid[i]), 0, dmJson::PARSE_DECODE_VALUES));
        ASSERT_TRUE(json.UnsignedInteger(0, &value));
        ASSERT_EQ(expected[i], value);
    }
    for (uint32_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
    {
        ASSERT_TRUE(json.Parse(invalid[i], (uint32_t)strlen(invalid[i]), 0, dmJson::PARSE_DECODE_VALUES));
        ASSERT_FALSE(json.UnsignedInteger(0, &value));
    }
    ASSERT_FALSE(json.UnsignedInteger(-1, &value));
}

// Source spans must refer to the encoded input even when strings and field names shrink during decoding.
TEST(dmJson, SourceLocations)
{
    const char text[] = " \n{\"\\u006bey\": [\"a\\nb\", {}, []]} \t";
    dmJson::Document json(32);
    ASSERT_TRUE(json.Parse(text, sizeof(text) - 1, 0, dmJson::PARSE_DECODE_VALUES));
    ASSERT_EQ(2u, json.Get(0).m_Offset);
    ASSERT_EQ(sizeof(text) - 5, json.Get(0).m_Length);
    int field = json.Field(0, "key");
    ASSERT_GE(field, 0);
    const dmJson::Node& array = json.Get(field);
    ASSERT_EQ(8u, array.m_NameLength);
    ASSERT_EQ(0, memcmp(text + array.m_NameOffset, "\\u006bey", array.m_NameLength));
    const char encoded[] = "[\"a\\nb\", {}, []]";
    ASSERT_EQ(sizeof(encoded) - 1, array.m_Length);
    ASSERT_EQ(0, memcmp(text + array.m_Offset, encoded, array.m_Length));
    const dmJson::Node& string = json.Get(array.m_Container.m_First);
    ASSERT_STREQ("a\nb", string.m_String);
    ASSERT_EQ(6u, string.m_Length);
    ASSERT_EQ(0, memcmp(text + string.m_Offset, "\"a\\nb\"", string.m_Length));
    int object = string.m_Next;
    ASSERT_EQ(2u, json.Get(object).m_Length);
    ASSERT_EQ(2u, json.Get(json.Get(object).m_Next).m_Length);
}

// Metadata values retain their encoded tokens even when they cannot be decoded as C strings or doubles.
TEST(dmJson, PreserveValues)
{
    const char text[] = "{\"\\u006bey\":\"a\\u0000b\",\"number\":1e400,\"valid\":true}";
    dmJson::Document json(32);
    ASSERT_FALSE(json.Parse(text, sizeof(text) - 1, 0, dmJson::PARSE_DECODE_VALUES));
    ASSERT_FALSE(json.Parse("1e400", 5, 0, dmJson::PARSE_DECODE_VALUES));
    ASSERT_TRUE(json.Parse(text, sizeof(text) - 1, 0, dmJson::PARSE_PRESERVE_VALUES));
    int string = json.Field(0, "key");
    int number = json.Field(0, "number");
    ASSERT_GE(string, 0);
    ASSERT_GE(number, 0);
    ASSERT_EQ(dmJson::TYPE_STRING, json.Get(string).m_Type);
    ASSERT_EQ(dmJson::TYPE_NUMBER, json.Get(number).m_Type);
    ASSERT_EQ(0, memcmp(text + json.Get(string).m_Offset, json.Text() + json.Get(string).m_Offset, json.Get(string).m_Length));
    ASSERT_EQ(0, memcmp(text + json.Get(number).m_Offset, json.Text() + json.Get(number).m_Offset, json.Get(number).m_Length));
    ASSERT_STREQ("fallback", json.String(string, "fallback"));
    ASSERT_EQ(-1, json.Integer(number, -1));
    uint32_t value;
    ASSERT_FALSE(json.UnsignedInteger(number, &value));
    ASSERT_TRUE(json.Boolean(json.Field(0, "valid"), false));
    ASSERT_TRUE(json.Parse("42", 2, 0, dmJson::PARSE_DECODE_VALUES));
    ASSERT_EQ(42, json.Integer(0, -1));
}

// Preserving values must still validate JSON syntax, Unicode escapes, UTF-8 and decoded object keys.
TEST(dmJson, PreserveValuesValidation)
{
    const char* invalid[] = { "1e+", "01", "1.", "NaN", "Infinity", "\"\\q\"", "\"\\ud800\"", "\"\\udc00\"", "\"\xff\"", "{\"a\\u0000b\":0}", "{\"key\":0,\"\\u006bey\":1}" };
    dmJson::Document json(32);
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(invalid); ++i)
    {
        ASSERT_FALSE(json.Parse(invalid[i], (uint32_t)strlen(invalid[i]), 0, dmJson::PARSE_PRESERVE_VALUES));
    }
}

// Diagnostics distinguish trailing data from syntax and UTF-8 failures at original input offsets.
TEST(dmJson, ErrorOffsets)
{
    dmJson::Document json(32);
    uint32_t offset;
    ASSERT_EQ(dmJson::RESULT_TRAILING_DATA, dmJson::Parse(json.Handle(), "{} \tx", 5, &offset, dmJson::PARSE_DECODE_VALUES));
    ASSERT_EQ(4u, offset);
    ASSERT_EQ(dmJson::RESULT_SYNTAX_ERROR, dmJson::Parse(json.Handle(), "[1,", 3, &offset, dmJson::PARSE_DECODE_VALUES));
    ASSERT_EQ(3u, offset);
    ASSERT_EQ(dmJson::RESULT_SYNTAX_ERROR, dmJson::Parse(json.Handle(), "\"a\xff\"", 4, &offset, dmJson::PARSE_DECODE_VALUES));
    ASSERT_EQ(2u, offset);
}

// UTF-8 validation must handle unaligned blocks, every tail length and sequences
// crossing 16-byte and 64-byte boundaries without overreads or changed error offsets.
TEST(dmJson, Utf8BlockBoundaries)
{
    const char* sequences[] = { "\xc3\xa9", "\xe2\x82\xac", "\xf0\x9f\x98\x80", "\xc0\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\x80", "\xc3" };
    dmJson::Document json(32);
    for (uint32_t prefix = 0; prefix < 128; ++prefix)
    {
        for (uint32_t suffix = 0; suffix < 16; ++suffix)
        {
            for (uint32_t s = 0; s < DM_ARRAY_SIZE(sequences); ++s)
            {
                uint32_t length = (uint32_t)strlen(sequences[s]);
                uint32_t size = prefix + length + suffix + 2;
                // No terminating NUL or padding: full loads must stay within size.
                char* text = (char*)malloc(size);
                memset(text, 'a', size);
                text[0] = text[size - 1] = '"';
                memcpy(text + prefix + 1, sequences[s], length);
                uint32_t offset;
                bool valid = json.Parse(text, size, &offset, dmJson::PARSE_DECODE_VALUES);
                free(text);
                ASSERT_EQ(s < 3, valid);
                if (!valid)
                {
                    ASSERT_EQ(prefix + 1, offset);
                }
            }
        }
    }
    for (uint32_t length = 0; length < 192; ++length)
    {
        char* storage = (char*)malloc(length + 3);
        char* text = storage + 1;
        memset(text, 'a', length + 2);
        text[0] = text[length + 1] = '"';
        ASSERT_TRUE(json.Parse(text, length + 2, 0, dmJson::PARSE_DECODE_VALUES));
        ASSERT_EQ(length, strlen(json.String(0, 0)));
        free(storage);
    }
}

// Every escape byte must retain the same validity in decoded, buffered, and preserved strings.
TEST(dmJson, SimpleEscapeBytes)
{
    const char encoded[] = "\"\\/bfnrt";
    const char decoded[] = "\"\\/\b\f\n\r\t";
    dmJson::StringBuffer buffer;
    for (uint32_t mode = 0; mode < 3; ++mode)
    {
        for (uint32_t byte = 0; byte < 256; ++byte)
        {
            char input[] = { '"', '\\', (char)byte, '"' };
            const char* escape = byte ? strchr(encoded, byte) : 0;
            dmJson::Lexer lexer(input, input + sizeof(input));
            char* value = lexer.ReadString(mode != 0, mode == 2 ? &buffer : 0);
            ASSERT_EQ(escape != 0, lexer.m_Valid);
            if (escape)
            {
                ASSERT_EQ(mode ? 1u : 2u, lexer.m_StringLength);
                ASSERT_EQ(mode ? decoded[escape - encoded] : '\\', value[0]);
            }
        }
    }
}

// Consecutive Unicode escapes must grow buffered output safely, preserve input and reject every truncation.
TEST(dmJson, BufferedStringBoundaries)
{
    const char suffix[] = "\\n\\u0041\\u041f\\u4e2d\\u0000\\ud83d\\ude00abcdefghijklmnopqrstuvwxyz0123456789\\t\"";
    const char decoded[] = "\nA\xd0\x9f\xe4\xb8\xad\0\xf0\x9f\x98\x80" "abcdefghijklmnopqrstuvwxyz0123456789\t";
    dmJson::StringBuffer buffer;
    for (uint32_t prefix = 0; prefix < 530; ++prefix)
    {
        uint32_t size = prefix + sizeof(suffix);
        char* input = (char*)malloc(size);
        input[0] = '"';
        memset(input + 1, 'a', prefix);
        memcpy(input + prefix + 1, suffix, sizeof(suffix) - 1);
        dmJson::Lexer lexer(input, input + size);
        lexer.m_AllowNul = true;
        char* result = lexer.ReadString(true, &buffer);
        ASSERT_TRUE(lexer.m_Valid);
        ASSERT_EQ(prefix + sizeof(decoded) - 1, lexer.m_StringLength);
        ASSERT_EQ(0, memcmp(result, input + 1, prefix));
        ASSERT_EQ(0, memcmp(result + prefix, decoded, sizeof(decoded) - 1));
        ASSERT_EQ(0, memcmp(input + prefix + 1, suffix, sizeof(suffix) - 1));
        ASSERT_EQ(input + size, lexer.m_Cursor);
        for (uint32_t length = 1; length < sizeof(suffix) - 1; ++length)
        {
            dmJson::Lexer truncated(input, input + prefix + 1 + length);
            truncated.m_AllowNul = true;
            ASSERT_EQ((char*)0, truncated.ReadString(true, &buffer));
            ASSERT_FALSE(truncated.m_Valid);
        }
        free(input);
    }
}

// Buffered spans must safely grow across plain text, raw controls and escapes producing up to four bytes.
TEST(dmJson, BufferedStringSpans)
{
    const char escaped[] = "\\t\\u0041\\u041f\\u4e2d\\u0000\\ud83d\\ude00\\\\\1";
    const char decoded[] = "\tA\xd0\x9f\xe4\xb8\xad\0\xf0\x9f\x98\x80\\\1";
    dmArray<char> input;
    dmArray<char> expected;
    Append(input, "\"\\n");
    Append(expected, "\n");
    for (uint32_t length = 0; length < 64; ++length)
    {
        for (uint32_t i = 0; i < length; ++i)
        {
            Append(input, "a");
            Append(expected, "a");
        }
        Append(input, escaped);
        WriteJson(&expected, decoded, sizeof(decoded) - 1);
    }
    Append(input, "\"");
    dmArray<char> original;
    WriteJson(&original, input.Begin(), input.Size());
    for (uint32_t capacity = 4; capacity < 530; ++capacity)
    {
        dmJson::StringBuffer buffer;
        buffer.m_Data = (char*)malloc(capacity);
        buffer.m_Capacity = capacity;
        dmJson::Lexer lexer(input.Begin(), input.End());
        lexer.m_AllowNul = true;
        lexer.m_AllowControl = true;
        char* result = lexer.ReadString(true, &buffer);
        ASSERT_TRUE(lexer.m_Valid);
        ASSERT_EQ(expected.Size(), lexer.m_StringLength);
        ASSERT_EQ(0, memcmp(result, expected.Begin(), expected.Size()));
        ASSERT_EQ(input.End(), lexer.m_Cursor);
        ASSERT_EQ(0, memcmp(input.Begin(), original.Begin(), input.Size()));
    }
}

// String scans must stop at every special byte position and compact long runs safely after decoding an escape.
TEST(dmJson, StringBlockBoundaries)
{
    const struct
    {
        const char* m_Text;
        uint32_t    m_Size;
        const char* m_Encoded;
        const char* m_Decoded;
    } cases[] = {
        { "\0", 1, "\\u0000", 0 },
        { "\1", 1, "\\u0001", "\1" },
        { "\x1f", 1, "\\u001f", "\x1f" },
        { "\b", 1, "\\b", "\b" },
        { "\f", 1, "\\f", "\f" },
        { "\n", 1, "\\n", "\n" },
        { "\r", 1, "\\r", "\r" },
        { "\t", 1, "\\t", "\t" },
        { " ", 1, " ", " " },
        { "!", 1, "!", "!" },
        { "\"", 1, "\\\"", "\"" },
        { "#", 1, "#", "#" },
        { "[", 1, "[", "[" },
        { "\\", 1, "\\\\", "\\" },
        { "]", 1, "]", "]" },
        { "\x7f", 1, "\x7f", "\x7f" },
        { "\x80", 1, "\\u0080", "\xc2\x80" },
        { "\xc0", 1, "\\u00c0", "\xc3\x80" },
        { "\xff", 1, "\\u00ff", "\xc3\xbf" },
        { "\xc3(", 2, "\\u00c3(", "\xc3\x83(" },
        { "\xc3\xa9", 2, "\xc3\xa9", "\xc3\xa9" },
        { "\xf0\x9f\x98\x80", 4, "\xf0\x9f\x98\x80", "\xf0\x9f\x98\x80" },
    };
    dmJson::Document json(32);
    for (uint32_t prefix = 0; prefix < 48; ++prefix)
    {
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(cases); ++i)
        {
            char text[128];
            memset(text, 'a', prefix);
            memcpy(text + prefix, cases[i].m_Text, cases[i].m_Size);
            memset(text + prefix + cases[i].m_Size, 'b', 65);
            dmArray<char> output;
            dmJson::WriteString(text, prefix + cases[i].m_Size + 65, WriteJson, &output);
            dmArray<char> expected;
            Append(expected, "\"");
            WriteJson(&expected, text, prefix);
            Append(expected, cases[i].m_Encoded);
            WriteJson(&expected, text + prefix + cases[i].m_Size, 65);
            Append(expected, "\"");
            ASSERT_EQ(expected.Size(), output.Size());
            ASSERT_EQ(0, memcmp(expected.Begin(), output.Begin(), output.Size()));
            if (cases[i].m_Decoded)
            {
                ASSERT_TRUE(json.Parse(expected.Begin(), expected.Size(), 0, dmJson::PARSE_DECODE_VALUES));
                expected.SetSize(0);
                WriteJson(&expected, text, prefix);
                Append(expected, cases[i].m_Decoded);
                WriteJson(&expected, text + prefix + cases[i].m_Size, 65);
                WriteJson(&expected, "", 1);
                ASSERT_STREQ(expected.Begin(), json.String(0, 0));
            }
        }
    }
}

// Escape control characters, embedded NUL and invalid bytes while preserving valid UTF-8 sequences.
TEST(dmJson, WriteStrings)
{
    const char text[] = "\0\b\f\n\r\t\1\"\\/\xc3\xa9\xf0\x9f\x98\x80\xff\xc0";
    dmArray<char> output;
    dmJson::WriteString(text, sizeof(text) - 1, WriteJson, &output);
    WriteJson(&output, "", 1);
    ASSERT_STREQ("\"\\u0000\\b\\f\\n\\r\\t\\u0001\\\"\\\\/\xc3\xa9\xf0\x9f\x98\x80\\u00ff\\u00c0\"", output.Begin());
    output.SetSize(0);
    dmJson::WriteString("", 0, WriteJson, &output);
    WriteJson(&output, "", 1);
    ASSERT_STREQ("\"\"", output.Begin());
    output.SetSize(0);
    const char roundtrip[] = "a\"\\\b\f\n\r\t\xc3\xa9\xf0\x9f\x98\x80";
    dmJson::WriteString(roundtrip, sizeof(roundtrip) - 1, WriteJson, &output);
    dmJson::Document json(32);
    ASSERT_TRUE(json.Parse(output.Begin(), output.Size(), 0, dmJson::PARSE_DECODE_VALUES));
    ASSERT_STREQ(roundtrip, json.String(0, 0));
}

// The corpus runner starts a fresh process per fixture so crashes and hangs
// identify the offending input. Reuse the document between both parse modes.
static int ParseCorpusFile(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        return 2;
    }
    if (fseek(file, 0, SEEK_END))
    {
        fclose(file);
        return 2;
    }
    long size = ftell(file);
    if (size < 0 || (uint64_t)size >= UINT32_MAX || fseek(file, 0, SEEK_SET))
    {
        fclose(file);
        return 2;
    }
    dmArray<char> text;
    text.SetCapacity((uint32_t)size + 1);
    size_t read = fread(text.Begin(), 1, (size_t)size, file);
    fclose(file);
    if (read != (size_t)size)
    {
        return 2;
    }
    dmJson::Document json(32);
    bool decoded = json.Parse(text.Begin(), (uint32_t)size, 0, dmJson::PARSE_DECODE_VALUES);
    if (decoded && !ChildCountsMatch(json))
    {
        return 3;
    }
    bool preserved = json.Parse(text.Begin(), (uint32_t)size, 0, dmJson::PARSE_PRESERVE_VALUES);
    if (preserved && !ChildCountsMatch(json))
    {
        return 3;
    }
    printf("%d %d\n", (int)decoded, (int)preserved);
    return 0;
}

// The native runner supplies independent expected binary64 bits in little-endian
// records: uint32 length, uint64 bits, then the raw JSON number. Both modes reuse
// the document; the runner checks the exact record count and enforces a timeout.
static int ParseNumberCases(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        return 2;
    }
    dmJson::Document json(0);
    dmArray<char> text;
    uint32_t count = 0;
    unsigned char header[12];
    size_t read;
    while ((read = fread(header, 1, sizeof(header), file)) == sizeof(header))
    {
        uint32_t size = 0;
        uint64_t expected = 0;
        for (uint32_t i = 0; i < 4; ++i)
        {
            size |= (uint32_t)header[i] << (8 * i);
        }
        for (uint32_t i = 0; i < 8; ++i)
        {
            expected |= (uint64_t)header[i + 4] << (8 * i);
        }
        if (!size || size == UINT32_MAX)
        {
            fclose(file);
            return 2;
        }
        if (size + 1 > text.Capacity())
        {
            text.SetCapacity(size + 1);
        }
        if (fread(text.Begin(), 1, size, file) != size)
        {
            fclose(file);
            return 2;
        }
        text.Begin()[size] = 0;
        bool decoded = json.Parse(text.Begin(), size, 0, dmJson::PARSE_DECODE_VALUES);
        bool finite = (expected & UINT64_C(0x7ff0000000000000)) != UINT64_C(0x7ff0000000000000);
        uint64_t actual = 0;
        if (decoded)
        {
            memcpy(&actual, &json.Get(0).m_Number, sizeof(actual));
        }
        if (decoded != finite || (decoded && (actual != expected || json.Get(0).m_Type != dmJson::TYPE_NUMBER))
            || !json.Parse(text.Begin(), size, 0, dmJson::PARSE_PRESERVE_VALUES))
        {
            fprintf(stderr, "Number case %u: %.80s (length %u), expected %016llx got %016llx, decoded=%d\n",
                    count, text.Begin(), size, (unsigned long long)expected, (unsigned long long)actual, decoded);
            fclose(file);
            return 3;
        }
        ++count;
    }
    bool complete = read == 0 && !ferror(file);
    fclose(file);
    if (!complete)
    {
        return 2;
    }
    printf("%u\n", count);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc == 3 && !strcmp(argv[1], "--corpus"))
    {
        return ParseCorpusFile(argv[2]);
    }
    if (argc == 3 && !strcmp(argv[1], "--numbers"))
    {
        return ParseNumberCases(argv[2]);
    }
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
