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

#include <dlib/json.h>
#include <string.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

// Compile and link the public API as C, covering ownership, traversal and typed values.
TEST(JsonC, DecodeAndTraverse)
{
    const char text[] = "{\"array\":[42,\"a\\nb\",true,null],\"unsigned\":4294967295,\"\\u006bey\":false}";
    HJsonDocument document = JsonCreate(0);
    uint32_t offset = 0;
    ASSERT_EQ(JSON_RESULT_OK, JsonParse(document, text, sizeof(text) - 1, &offset, JSON_PARSE_DECODE_VALUES));
    ASSERT_EQ(sizeof(text) - 1, offset);
    ASSERT_EQ(8u, JsonGetNodeCount(document));
    ASSERT_EQ(JSON_TYPE_OBJECT, JsonGetNode(document, 0)->m_Type);
    ASSERT_EQ(3u, JsonGetNode(document, 0)->m_Container.m_Count);
    ASSERT_TRUE(JsonGetName(document, 0) == NULL);

    int array = JsonField(document, 0, "array");
    const JsonNode* node = JsonGetNode(document, array);
    ASSERT_STREQ("array", JsonGetName(document, array));
    ASSERT_EQ(4u, node->m_Container.m_Count);
    int value = node->m_Container.m_First;
    ASSERT_EQ(42, JsonInteger(document, value, -1));
    value = JsonGetNode(document, value)->m_Next;
    ASSERT_STREQ("a\nb", JsonString(document, value, NULL));
    value = JsonGetNode(document, value)->m_Next;
    ASSERT_TRUE(JsonBoolean(document, value, false));
    value = JsonGetNode(document, value)->m_Next;
    ASSERT_EQ(JSON_TYPE_NULL, JsonGetNode(document, value)->m_Type);
    ASSERT_EQ(-1, JsonGetNode(document, value)->m_Next);
    ASSERT_EQ(-7, JsonInteger(document, value, -7));

    uint32_t number = 17;
    ASSERT_TRUE(JsonUnsignedInteger(document, JsonField(document, 0, "unsigned"), &number));
    ASSERT_EQ(UINT32_MAX, number);
    ASSERT_FALSE(JsonUnsignedInteger(document, value, &number));
    ASSERT_EQ(UINT32_MAX, number);
    int key = JsonField(document, 0, "key");
    ASSERT_STREQ("key", JsonGetName(document, key));
    ASSERT_FALSE(JsonBoolean(document, key, true));
    ASSERT_EQ(-1, JsonField(document, array, "missing"));
    ASSERT_EQ(JSON_TYPE_NULL, JsonGetNode(document, -1)->m_Type);
    ASSERT_EQ(-1, JsonGetNode(document, 1000)->m_Container.m_First);
    ASSERT_STREQ("fallback", JsonString(document, -1, "fallback"));
    JsonDestroy(document);
}

// C callers can preserve encoded spans and reuse a document after syntax or trailing-data errors.
TEST(JsonC, PreserveAndReuse)
{
    const char text[] = "{\"text\":\"a\\u0000b\",\"number\":1e400,\"flag\":true}";
    HJsonDocument document = JsonCreate(1);
    ASSERT_EQ(JSON_RESULT_OK, JsonParse(document, text, sizeof(text) - 1, NULL, JSON_PARSE_PRESERVE_VALUES));
    int field = JsonField(document, 0, "text");
    const JsonNode* node = JsonGetNode(document, field);
    ASSERT_STREQ("text", JsonGetName(document, field));
    ASSERT_EQ(0, memcmp(text + node->m_Offset, JsonGetText(document) + node->m_Offset, node->m_Length));
    ASSERT_STREQ("raw", JsonString(document, field, "raw"));
    field = JsonField(document, 0, "number");
    ASSERT_EQ(-1, JsonInteger(document, field, -1));
    uint32_t number = 13;
    ASSERT_FALSE(JsonUnsignedInteger(document, field, &number));
    ASSERT_EQ(13u, number);
    ASSERT_TRUE(JsonBoolean(document, JsonField(document, 0, "flag"), false));

    uint32_t offset = 0;
    ASSERT_EQ(JSON_RESULT_SYNTAX_ERROR, JsonParse(document, "[1,", 3, &offset, JSON_PARSE_DECODE_VALUES));
    ASSERT_EQ(3u, offset);
    ASSERT_EQ(JSON_RESULT_TRAILING_DATA, JsonParse(document, "{} x", 4, &offset, JSON_PARSE_DECODE_VALUES));
    ASSERT_EQ(3u, offset);
    const char bounded[] = {
        '[', '7', ']'
    };
    ASSERT_EQ(JSON_RESULT_OK, JsonParse(document, bounded, sizeof(bounded), &offset, JSON_PARSE_DECODE_VALUES));
    ASSERT_EQ(3u, offset);
    ASSERT_EQ(2u, JsonGetNodeCount(document));
    ASSERT_EQ(7, JsonInteger(document, JsonGetNode(document, 0)->m_Container.m_First, -1));
    ASSERT_EQ(0, memcmp(bounded, "[7]", sizeof(bounded)));
    JsonDestroy(document);
}

typedef struct Output
{
    char     m_Text[128];
    uint32_t m_Size;
} Output;

static void Write(void* context, const char* text, uint32_t size)
{
    Output* output = (Output*)context;
    memcpy(output->m_Text + output->m_Size, text, size);
    output->m_Size += size;
    output->m_Text[output->m_Size] = 0;
}

// A C callback receives the same escaped JSON text as the C++ string writer.
TEST(JsonC, WriteString)
{
    Output output = { {
                          0
                      }, 0 };
    const char text[] = {
        'a', '\n', '"', '\\', '\0', (char)0xff
    };
    JsonWriteString(text, sizeof(text), Write, &output);
    ASSERT_STREQ("\"a\\n\\\"\\\\\\u0000\\u00ff\"", output.m_Text);
    HJsonDocument document = JsonCreate(0);
    ASSERT_EQ(JSON_RESULT_OK, JsonParse(document, output.m_Text, output.m_Size, NULL, JSON_PARSE_PRESERVE_VALUES));
    JsonDestroy(document);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
