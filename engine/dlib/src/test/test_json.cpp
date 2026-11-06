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
#include <dlib/json.h>
#include <dlib/math.h>
#include <locale.h>
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

// Preserve every value when an array grows beyond its initial capacity;
// subsequent parses must discard the previous document.
TEST(dmJson, InitialCapacityAndGrowingArray)
{
    dmJson::Document json;
    uint32_t initial_capacity = json.m_Nodes.Capacity();
    ASSERT_LT(initial_capacity, 1024u);
    dmArray<char> text;
    Array(text, 1024);
    ASSERT_TRUE(json.Parse(text.Begin(), text.Size()));
    ASSERT_EQ(1025u, json.m_Nodes.Size());
    ASSERT_GT(json.m_Nodes.Capacity(), initial_capacity);
    int value = json.Get(0).m_First;
    for (int i = 0; i < 1024; ++i)
    {
        ASSERT_GE(value, 0);
        ASSERT_EQ(i, json.Integer(value));
        value = json.Get(value).m_Next;
    }
    ASSERT_EQ(-1, value);
    const char* small_json = "{\"value\":42}";
    ASSERT_TRUE(json.Parse(small_json, (uint32_t)strlen(small_json)));
    ASSERT_EQ(2u, json.m_Nodes.Size());
    ASSERT_EQ(42, json.Integer(json.Field(0, "value")));
}

// Explicit reserves, including a zero reserve, must grow safely while mixed node types retain their values.
TEST(dmJson, ReservedCapacityAndMixedValues)
{
    const uint32_t capacities[] = { 0, 1, 16, 512 };
    const char text[] = "{\"array\":[42,\"value\",true,false,null,{},[]],\"empty\":\"\"}";
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(capacities); ++i)
    {
        dmJson::Document json(capacities[i]);
        ASSERT_EQ(capacities[i], json.m_Nodes.Capacity());
        ASSERT_TRUE(json.Parse(text, sizeof(text) - 1));
        int value = json.Get(json.Field(0, "array")).m_First;
        ASSERT_EQ(42, json.Integer(value));
        value = json.Get(value).m_Next;
        ASSERT_STREQ("value", json.String(value));
        ASSERT_EQ(-7, json.Integer(value, -7));
        value = json.Get(value).m_Next;
        ASSERT_TRUE(json.Boolean(value));
        value = json.Get(value).m_Next;
        ASSERT_FALSE(json.Boolean(value, true));
        value = json.Get(value).m_Next;
        ASSERT_EQ(dmJson::TYPE_NULL, json.Get(value).m_Type);
        ASSERT_EQ(-7, json.Integer(value, -7));
        value = json.Get(value).m_Next;
        ASSERT_EQ(dmJson::TYPE_OBJECT, json.Get(value).m_Type);
        ASSERT_EQ(-1, json.Get(value).m_First);
        value = json.Get(value).m_Next;
        ASSERT_EQ(dmJson::TYPE_ARRAY, json.Get(value).m_Type);
        ASSERT_EQ(-1, json.Get(value).m_First);
        ASSERT_EQ(-1, json.Get(value).m_Next);
        ASSERT_STREQ("", json.String(json.Field(0, "empty")));
        ASSERT_EQ(-1, json.Get(-1).m_First);
        ASSERT_EQ(-7, json.Integer(-1, -7));
    }
}

// Field links and lookup results must survive both the small-object/index
// transition and hash-table/node-array growth in large objects.
TEST(dmJson, WideObjectFields)
{
    const uint32_t counts[] = { 16, 17, 32, 33, 512, 5000 };
    dmJson::Document json;
    dmArray<char>    text;
    for (uint32_t c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c)
    {
        text.SetSize(0);
        Append(text, "{");
        Fields(text, counts[c]);
        Append(text, "}");
        ASSERT_TRUE(json.Parse(text.Begin(), text.Size()));
        ASSERT_EQ(counts[c] + 1, json.m_Nodes.Size());
        int field = json.Get(0).m_First;
        for (uint32_t i = 0; i < counts[c]; ++i)
        {
            char name[32];
            snprintf(name, sizeof(name), "k%u", i);
            ASSERT_GE(field, 0);
            ASSERT_STREQ(name, json.Get(field).m_Name);
            ASSERT_EQ((int)i, json.Integer(field));
            field = json.Get(field).m_Next;
        }
        ASSERT_EQ(-1, field);
        ASSERT_EQ(0, json.Integer(json.Field(0, "k0")));
        ASSERT_EQ(-1, json.Field(0, "missing"));
    }
}

// Detect duplicate decoded names before and after indexing, including early
// and late keys and an escaped spelling that hashes identically after decoding.
TEST(dmJson, DuplicateDecodedKeys)
{
    const uint32_t counts[] = { 1, 16, 17, 512, 5000 };
    dmJson::Document json;
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
            ASSERT_FALSE(json.Parse(text.Begin(), text.Size()));
        }
    }
    text.SetSize(0);
    Append(text, "{\"\":1,");
    Fields(text, 32);
    Append(text, ",\"\":2}");
    ASSERT_FALSE(json.Parse(text.Begin(), text.Size()));
}

// Identical names in different nested objects are valid. Index state must be
// scoped to each object and released after a failed parse and between parses.
TEST(dmJson, NestedObjectKeyScope)
{
    dmJson::Document json;
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
        ASSERT_TRUE(json.Parse(text.Begin(), text.Size()));
        int nested = json.Field(0, "nested");
        ASSERT_EQ(31, json.Integer(json.Field(nested, "k31")));
        int rows = json.Field(0, "rows");
        int first = json.Get(rows).m_First;
        ASSERT_EQ(0, json.Integer(json.Field(first, "k0")));
        ASSERT_EQ(31, json.Integer(json.Field(json.Get(first).m_Next, "k31")));
        dmArray<char> invalid;
        Append(invalid, "{\"nested\":{");
        Fields(invalid, 32);
        Append(invalid, ",\"k31\":99}}");
        ASSERT_FALSE(json.Parse(invalid.Begin(), invalid.Size()));
    }
}

// Growth must stop at the existing 65,536-node bound, reject the next node,
// and allow the same parser to process a valid document after rejection.
TEST(dmJson, NodeLimit)
{
    dmJson::Document json;
    dmArray<char>    text;
    Array(text, dmJson::MAX_NODES - 1);
    ASSERT_TRUE(json.Parse(text.Begin(), text.Size()));
    ASSERT_EQ(dmJson::MAX_NODES, json.m_Nodes.Size());
    ASSERT_LE(json.m_Nodes.Capacity(), dmJson::MAX_NODES);
    Array(text, dmJson::MAX_NODES);
    ASSERT_FALSE(json.Parse(text.Begin(), text.Size()));
    ASSERT_EQ(dmJson::MAX_NODES, json.m_Nodes.Size());
    const char* small_json = "{\"valid\":true}";
    ASSERT_TRUE(json.Parse(small_json, (uint32_t)strlen(small_json)));
    ASSERT_TRUE(json.Boolean(json.Field(0, "valid")));
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
    dmJson::Document json;
    const char* text = "[1,2,640.0,3.6e2,-1.25,1.2e-3,1.2345678901234567e100,2147483647.0]";
    const double values[] = { 1.0, 2.0, 640.0, 360.0, -1.25, 0.0012, 1.2345678901234567e100, 2147483647.0 };
    // Nonfatal checks ensure the original locale is restored even if parsing regresses.
    EXPECT_TRUE(json.Parse(text, (uint32_t)strlen(text)));
    int node = json.Get(0).m_First;
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

// Unsigned IDs retain all 32 bits and reject out-of-range, fractional and nonnumeric values.
TEST(dmJson, UnsignedIntegers)
{
    const char* valid[] = { "0", "2147483648", "4294967295", "4294967295.0", "1e2" };
    const uint32_t expected[] = { 0u, 2147483648u, 4294967295u, 4294967295u, 100u };
    const char* invalid[] = { "-1", "4294967296", "1.5", "true", "\"1\"", "null" };
    dmJson::Document json;
    uint32_t value;
    for (uint32_t i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i)
    {
        ASSERT_TRUE(json.Parse(valid[i], (uint32_t)strlen(valid[i])));
        ASSERT_TRUE(json.UnsignedInteger(0, &value));
        ASSERT_EQ(expected[i], value);
    }
    for (uint32_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
    {
        ASSERT_TRUE(json.Parse(invalid[i], (uint32_t)strlen(invalid[i])));
        ASSERT_FALSE(json.UnsignedInteger(0, &value));
    }
    ASSERT_FALSE(json.UnsignedInteger(-1, &value));
}

// Source spans must refer to the encoded input even when strings and field names shrink during decoding.
TEST(dmJson, SourceLocations)
{
    const char text[] = " \n{\"\\u006bey\": [\"a\\nb\", {}, []]} \t";
    dmJson::Document json;
    ASSERT_TRUE(json.Parse(text, sizeof(text) - 1));
    ASSERT_EQ(2u, json.Get(0).m_Offset);
    ASSERT_EQ(sizeof(text) - 5, json.Get(0).m_Length);
    int field = json.Field(0, "key");
    ASSERT_GE(field, 0);
    const dmJson::Node& array = json.Get(field);
    ASSERT_EQ(8u, array.m_NameLength);
    ASSERT_EQ(0, memcmp(text + (array.m_Name - json.m_Text), "\\u006bey", array.m_NameLength));
    const char encoded[] = "[\"a\\nb\", {}, []]";
    ASSERT_EQ(sizeof(encoded) - 1, array.m_Length);
    ASSERT_EQ(0, memcmp(text + array.m_Offset, encoded, array.m_Length));
    const dmJson::Node& string = json.Get(array.m_First);
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
    dmJson::Document json;
    ASSERT_FALSE(json.Parse(text, sizeof(text) - 1));
    ASSERT_FALSE(json.Parse("1e400", 5));
    ASSERT_TRUE(json.Parse(text, sizeof(text) - 1, 0, dmJson::PARSE_PRESERVE_VALUES));
    int string = json.Field(0, "key");
    int number = json.Field(0, "number");
    ASSERT_GE(string, 0);
    ASSERT_GE(number, 0);
    ASSERT_EQ(dmJson::TYPE_STRING, json.Get(string).m_Type);
    ASSERT_EQ(dmJson::TYPE_NUMBER, json.Get(number).m_Type);
    ASSERT_EQ(0, memcmp(text + json.Get(string).m_Offset, json.m_Text + json.Get(string).m_Offset, json.Get(string).m_Length));
    ASSERT_EQ(0, memcmp(text + json.Get(number).m_Offset, json.m_Text + json.Get(number).m_Offset, json.Get(number).m_Length));
    ASSERT_STREQ("fallback", json.String(string, "fallback"));
    ASSERT_EQ(-1, json.Integer(number));
    uint32_t value;
    ASSERT_FALSE(json.UnsignedInteger(number, &value));
    ASSERT_TRUE(json.Boolean(json.Field(0, "valid")));
    ASSERT_TRUE(json.Parse("42", 2));
    ASSERT_EQ(42, json.Integer(0));
}

// Preserving values must still validate JSON syntax, Unicode escapes, UTF-8 and decoded object keys.
TEST(dmJson, PreserveValuesValidation)
{
    const char* invalid[] = { "1e+", "01", "1.", "NaN", "Infinity", "\"\\q\"", "\"\\ud800\"", "\"\\udc00\"", "\"\xff\"", "{\"a\\u0000b\":0}", "{\"key\":0,\"\\u006bey\":1}" };
    dmJson::Document json;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(invalid); ++i)
    {
        ASSERT_FALSE(json.Parse(invalid[i], (uint32_t)strlen(invalid[i]), 0, dmJson::PARSE_PRESERVE_VALUES));
    }
}

// Diagnostics distinguish trailing data from syntax and UTF-8 failures at original input offsets.
TEST(dmJson, ErrorOffsets)
{
    dmJson::Document json;
    uint32_t offset;
    ASSERT_FALSE(json.Parse("{} \tx", 5, &offset));
    ASSERT_TRUE(json.m_Valid);
    ASSERT_EQ(4u, offset);
    ASSERT_FALSE(json.Parse("[1,", 3, &offset));
    ASSERT_FALSE(json.m_Valid);
    ASSERT_EQ(3u, offset);
    ASSERT_FALSE(json.Parse("\"a\xff\"", 4, &offset));
    ASSERT_FALSE(json.m_Valid);
    ASSERT_EQ(2u, offset);
}

// UTF-8 validation must handle unaligned blocks, every tail length and sequences crossing SIMD boundaries.
TEST(dmJson, Utf8BlockBoundaries)
{
    const char* sequences[] = { "\xc3\xa9", "\xe2\x82\xac", "\xf0\x9f\x98\x80", "\xc0\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\x80", "\xc3" };
    dmJson::Document json;
    for (uint32_t prefix = 0; prefix < 32; ++prefix)
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
                bool valid = json.Parse(text, size, &offset);
                free(text);
                ASSERT_EQ(s < 3, valid);
                if (!valid)
                {
                    ASSERT_EQ(prefix + 1, offset);
                }
            }
        }
    }
    for (uint32_t length = 0; length < 64; ++length)
    {
        char* storage = (char*)malloc(length + 3);
        char* text = storage + 1;
        memset(text, 'a', length + 2);
        text[0] = text[length + 1] = '"';
        ASSERT_TRUE(json.Parse(text, length + 2));
        ASSERT_EQ(length, strlen(json.String(0)));
        free(storage);
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
    dmJson::Document json;
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
                ASSERT_TRUE(json.Parse(expected.Begin(), expected.Size()));
                expected.SetSize(0);
                WriteJson(&expected, text, prefix);
                Append(expected, cases[i].m_Decoded);
                WriteJson(&expected, text + prefix + cases[i].m_Size, 65);
                WriteJson(&expected, "", 1);
                ASSERT_STREQ(expected.Begin(), json.String(0));
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
    dmJson::Document json;
    ASSERT_TRUE(json.Parse(output.Begin(), output.Size()));
    ASSERT_STREQ(roundtrip, json.String(0));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
