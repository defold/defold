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

#include "dap.h"
#include <stdio.h>
#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

using namespace dmDebugger;

static void Fields(Buffer& text, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
        text.Format("%s\"k%u\":%u", i ? "," : "", i, i);
}

static void Array(Buffer& text, uint32_t count)
{
    text.Clear();
    text.m_Data.SetCapacity(count * 8 + 2);
    text.Add("[");
    for (uint32_t i = 0; i < count; ++i)
        text.Format("%s%u", i ? "," : "", i);
    text.Add("]");
}

// Preallocate 512 nodes and preserve every value when an array grows beyond
// that capacity; subsequent parses must discard the previous document.
TEST(DapJson, InitialCapacityAndGrowingArray)
{
    Json json;
    ASSERT_EQ(512u, json.m_Nodes.Capacity());
    Buffer text;
    Array(text, 1024);
    ASSERT_TRUE(json.Parse(text.Data(), text.Size()));
    ASSERT_EQ(1025u, json.m_Nodes.Size());
    ASSERT_GT(json.m_Nodes.Capacity(), 512u);
    int value = json.Get(0).m_First;
    for (int i = 0; i < 1024; ++i)
    {
        ASSERT_GE(value, 0);
        ASSERT_EQ(i, json.Integer(value));
        value = json.Get(value).m_Next;
    }
    ASSERT_EQ(-1, value);
    const char* small = "{\"value\":42}";
    ASSERT_TRUE(json.Parse(small, (uint32_t)strlen(small)));
    ASSERT_EQ(2u, json.m_Nodes.Size());
    ASSERT_EQ(42, json.Integer(json.Field(0, "value")));
}

// Field links and lookup results must survive both the small-object/index
// transition and hash-table/node-array growth in large objects.
TEST(DapJson, WideObjectFields)
{
    const uint32_t counts[] = { 16, 17, 32, 33, 512, 5000 };
    Json           json;
    Buffer         text;
    for (uint32_t c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c)
    {
        text.Clear();
        text.Add("{");
        Fields(text, counts[c]);
        text.Add("}");
        ASSERT_TRUE(json.Parse(text.Data(), text.Size()));
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
TEST(DapJson, DuplicateDecodedKeys)
{
    const uint32_t counts[] = { 1, 16, 17, 512, 5000 };
    Json           json;
    Buffer         text;
    for (uint32_t c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c)
    {
        for (uint32_t variant = 0; variant < 3; ++variant)
        {
            text.Clear();
            text.Add("{");
            Fields(text, counts[c]);
            if (variant == 0)
                text.Add(",\"k0\":999}");
            else if (variant == 1)
                text.Format(",\"k%u\":999}", counts[c] - 1);
            else
                text.Add(",\"\\u006b0\":999}");
            ASSERT_FALSE(json.Parse(text.Data(), text.Size()));
        }
    }
    text.Clear();
    text.Add("{\"\":1,");
    Fields(text, 32);
    text.Add(",\"\":2}");
    ASSERT_FALSE(json.Parse(text.Data(), text.Size()));
}

// Identical names in different nested objects are valid. Index state must be
// scoped to each object and released after a failed parse and between parses.
TEST(DapJson, NestedObjectKeyScope)
{
    Json   json;
    Buffer text;
    text.Add("{");
    Fields(text, 32);
    text.Add(",\"nested\":{");
    Fields(text, 32);
    text.Add("},\"rows\":[{");
    Fields(text, 32);
    text.Add("},{");
    Fields(text, 32);
    text.Add("}]}");
    for (uint32_t i = 0; i < 2; ++i)
    {
        ASSERT_TRUE(json.Parse(text.Data(), text.Size()));
        int nested = json.Field(0, "nested");
        ASSERT_EQ(31, json.Integer(json.Field(nested, "k31")));
        int rows = json.Field(0, "rows");
        int first = json.Get(rows).m_First;
        ASSERT_EQ(0, json.Integer(json.Field(first, "k0")));
        ASSERT_EQ(31, json.Integer(json.Field(json.Get(first).m_Next, "k31")));
        Buffer invalid;
        invalid.Add("{\"nested\":{");
        Fields(invalid, 32);
        invalid.Add(",\"k31\":99}}");
        ASSERT_FALSE(json.Parse(invalid.Data(), invalid.Size()));
    }
}

// Growth must stop at the existing 65,536-node bound, reject the next node,
// and allow the same parser to process a valid document after rejection.
TEST(DapJson, NodeLimit)
{
    Json   json;
    Buffer text;
    Array(text, MAX_JSON_NODES - 1);
    ASSERT_TRUE(json.Parse(text.Data(), text.Size()));
    ASSERT_EQ(MAX_JSON_NODES, json.m_Nodes.Size());
    ASSERT_LE(json.m_Nodes.Capacity(), MAX_JSON_NODES);
    Array(text, MAX_JSON_NODES);
    ASSERT_FALSE(json.Parse(text.Data(), text.Size()));
    ASSERT_EQ(MAX_JSON_NODES, json.m_Nodes.Size());
    const char* small = "{\"valid\":true}";
    ASSERT_TRUE(json.Parse(small, (uint32_t)strlen(small)));
    ASSERT_TRUE(json.Boolean(json.Field(0, "valid")));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
