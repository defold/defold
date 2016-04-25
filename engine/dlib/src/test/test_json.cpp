#include <stdint.h>
#include <gtest/gtest.h>
#include <string.h>
#include <string>
#include "../dlib/json.h"
#include "data/flickr.json.embed.h"

class dmJsonTest: public ::testing::Test
{

public:
    virtual void SetUp() {}

    virtual void TearDown()
    {

    }

    dmJson::Document doc;

};

TEST_F(dmJsonTest, Empty)
{
    dmJson::Result r = dmJson::Parse(0x0, &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
    ASSERT_EQ(0, doc.m_NodeCount);
    r = dmJson::Parse("", &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
    ASSERT_EQ(0, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Incomplete)
{
    const char* json = "[";
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_EQ(dmJson::RESULT_INCOMPLETE, r);
}

TEST_F(dmJsonTest, Number)
{
    // A primitive cannot be a root according to the json format.
    const char* json = "123";
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_NE(dmJson::RESULT_OK, r);
}

TEST_F(dmJsonTest, Float)
{
    // A primitive cannot be a root according to the json format.
    const char* json = "123.456";
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_NE(dmJson::RESULT_OK, r);
}

TEST_F(dmJsonTest, Boolean)
{
    // A primitive cannot be a root according to the json format.
    const char* json = "true";
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_NE(dmJson::RESULT_OK, r);
}

TEST_F(dmJsonTest, Null)
{
    // A primitive cannot be a root according to the json format.
    const char* json = "null";
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_NE(dmJson::RESULT_OK, r);
}

TEST_F(dmJsonTest, String)
{
    // A string is not considered a primitive, and can be a root according to
    // the json format.
    const char* json = "\"json\"";
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
    ASSERT_EQ(1, doc.m_NodeCount);
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[0].m_Type);
    ASSERT_EQ(1, doc.m_Nodes[0].m_Start);
    ASSERT_EQ(5, doc.m_Nodes[0].m_End);
}

TEST_F(dmJsonTest, Escape1)
{
    const char* json = "\"\\n\"";
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
    ASSERT_EQ(1, doc.m_NodeCount);
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[0].m_Type);
    ASSERT_EQ(1, doc.m_Nodes[0].m_Start);
    ASSERT_EQ(2, doc.m_Nodes[0].m_End);
    ASSERT_EQ('\n', doc.m_Json[doc.m_Nodes[0].m_Start]);
}

TEST_F(dmJsonTest, TableValid)
{
    const unsigned int entries = 11;
    const char* json[entries] = {
        "{\"key1\": \"value1\"}",              // String key, String value
        "{\"key2\": 1}",                       // String key, Primitive value
        "{\"key3\": {\"key4\": true}}",        // String key, Object value
        "{\"key5\": [2, 3, 4]}",               // String key, Array value
        "{1: \"value2\"}",                     // Primitive key, String value
        "{2: null}",                           // Primitive key, Primitive value
        "{3: {\"key6\": 5.5}}",                // Primitive key, Object value
        "{4: [6, 7, 8]}",                      // Primitive key, Array value
        "{\"key7\": 9, \"key8\": \"value3\"}", // Multiple String key, Mixed values
        "{10: 11, 12: \"value4\"}",            // Multiple Primitive key, Mixed values
        "{13: \"value5\", \"key9\": 14}",      // Multiple mixed key, Mixed values
    };

    for (unsigned int i = 0; i < entries; ++i)
    {
        dmJson::Result result = dmJson::Parse(json[i], &doc);
        ASSERT_EQ(dmJson::RESULT_OK, result);
    }
}

TEST_F(dmJsonTest, TableInvalid)
{
    const unsigned int entries = 11;
    const char* json[entries] = {
        "{response: \"ok\"}",       // Missing quotes around key
        "{\"response\": ok}",       // Missing quotes around value
        "{\"response\"= \"ok\"}",   // Use incorrect assignment
        "{\"response\" = \"ok\"}",  // Use incorrect assignment
        "{\"response\" =\"ok\"}",   // Use incorrect assignment
        "{response: ok}",           // Missing quotes around key & value
        "{response : ok}",          // Missing quotes around key & value
        "{response :ok}",           // Missing quotes around key & value
        "{response= ok}",           // Missing quotes around key & value, and incorrect assignment
        "{response = ok}",          // Missing quotes around key & value, and incorrect assignment
        "{response =ok}"            // Missing quotes around key & value, and incorrect assignment
    };

    for (unsigned int i = 0; i < entries; ++i)
    {
        dmJson::Result result = dmJson::Parse(json[i], &doc);
        ASSERT_NE(dmJson::RESULT_OK, result);
    }
}

TEST_F(dmJsonTest, Escape2)
{
    const char* json = "\"foo\\tbar\\n\"";
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
    ASSERT_EQ(1, doc.m_NodeCount);
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[0].m_Type);
    dmJson::Node* node = &doc.m_Nodes[0];
    int n = node->m_End - node->m_Start;
    char* s = (char*) malloc(n + 1);
    memcpy(s, doc.m_Json + node->m_Start, n);
    s[n] = '\0';
    ASSERT_STREQ("foo\tbar\n", s);
    free(s);
}

TEST_F(dmJsonTest, Unicode)
{
    /*
        å
        LATIN SMALL LETTER A WITH RING ABOVE
        Unicode: U+00E5, UTF-8: C3 A5

        ä
        LATIN SMALL LETTER A WITH DIAERESIS
        Unicode: U+00E4, UTF-8: C3 A4

        ö
        LATIN SMALL LETTER O WITH DIAERESIS
        Unicode: U+00F6, UTF-8: C3 B6
     */
    const char* json = "\"\\u00e5\\u00e4\\u00f6foo\"";
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
    ASSERT_EQ(1, doc.m_NodeCount);
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[0].m_Type);
    dmJson::Node* node = &doc.m_Nodes[0];

    ASSERT_EQ(0xc3u, (uint8_t) doc.m_Json[node->m_Start + 0]);
    ASSERT_EQ(0xa5u, (uint8_t) doc.m_Json[node->m_Start + 1]);
    ASSERT_EQ(0xc3u, (uint8_t) doc.m_Json[node->m_Start + 2]);
    ASSERT_EQ(0xa4u, (uint8_t) doc.m_Json[node->m_Start + 3]);
    ASSERT_EQ(0xc3u, (uint8_t) doc.m_Json[node->m_Start + 4]);
    ASSERT_EQ(0xb6u, (uint8_t) doc.m_Json[node->m_Start + 5]);

    ASSERT_EQ('f', (uint8_t) doc.m_Json[node->m_Start + 6]);
    ASSERT_EQ('o', (uint8_t) doc.m_Json[node->m_Start + 7]);
    ASSERT_EQ('o', (uint8_t) doc.m_Json[node->m_Start + 8]);
}

TEST_F(dmJsonTest, Array)
{
    const char* json = "[10,20]";
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
    ASSERT_EQ(3, doc.m_NodeCount);
    ASSERT_EQ(dmJson::TYPE_ARRAY, doc.m_Nodes[0].m_Type);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[1].m_Type);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[2].m_Type);
    ASSERT_EQ(2, doc.m_Nodes[0].m_Size);
    ASSERT_EQ(0, doc.m_Nodes[0].m_Start);
    ASSERT_EQ(7, doc.m_Nodes[0].m_End);
    ASSERT_EQ(1, doc.m_Nodes[1].m_Start);
    ASSERT_EQ(3, doc.m_Nodes[1].m_End);
    ASSERT_EQ(4, doc.m_Nodes[2].m_Start);
    ASSERT_EQ(6, doc.m_Nodes[2].m_End);
}

TEST_F(dmJsonTest, Object)
{
    const char* json = "{\"x\": 10, \"y\": 20}";
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
    ASSERT_EQ(5, doc.m_NodeCount);

    ASSERT_EQ(dmJson::TYPE_OBJECT, doc.m_Nodes[0].m_Type);
    ASSERT_EQ(4, doc.m_Nodes[0].m_Size);
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[1].m_Type);
    ASSERT_EQ(3, doc.m_Nodes[1].m_Sibling);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[2].m_Type);
    ASSERT_EQ(-1, doc.m_Nodes[2].m_Sibling);
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[3].m_Type);
    ASSERT_EQ(-1, doc.m_Nodes[3].m_Sibling);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[4].m_Type);
    ASSERT_EQ(-1, doc.m_Nodes[4].m_Sibling);
}

TEST_F(dmJsonTest, MultiRoot)
{
    // A json document can only contain a single root according to
    // the json format
    const char* json = "10,20";
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_NE(dmJson::RESULT_OK, r);
}

TEST_F(dmJsonTest, NestedArray)
{
    const char* json = "[[10,\"x\", 20],[30]]";
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
    ASSERT_EQ(7, doc.m_NodeCount);
    ASSERT_EQ(dmJson::TYPE_ARRAY, doc.m_Nodes[0].m_Type);
    ASSERT_EQ(2, doc.m_Nodes[0].m_Size);

    ASSERT_EQ(dmJson::TYPE_ARRAY, doc.m_Nodes[1].m_Type);
    ASSERT_EQ(5, doc.m_Nodes[1].m_Sibling);
    ASSERT_EQ(3, doc.m_Nodes[1].m_Size);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[2].m_Type);
    ASSERT_EQ(3, doc.m_Nodes[2].m_Sibling);
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[3].m_Type);
    ASSERT_EQ(4, doc.m_Nodes[3].m_Sibling);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[4].m_Type);
    ASSERT_EQ(-1, doc.m_Nodes[4].m_Sibling);

    ASSERT_EQ(dmJson::TYPE_ARRAY, doc.m_Nodes[5].m_Type);
    ASSERT_EQ(1, doc.m_Nodes[5].m_Size);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[6].m_Type);
    ASSERT_EQ(-1, doc.m_Nodes[6].m_Sibling);
}

TEST_F(dmJsonTest, Large)
{
    std::string json = "[";
    const int N = 2048;
    for (int i = 0; i < N; ++i)
    {
        json += "0";
        if (i != N - 1)
            json += ",";
    }
    json += "]";
    dmJson::Result r = dmJson::Parse(json.c_str(), &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
}

TEST_F(dmJsonTest, Flickr)
{
    // Flickr escape ' with \'
    // This is invalid json so we replace \' with ' before parsing
    std::string json((const char*) FLICKR_JSON, (size_t) FLICKR_JSON_SIZE);
    size_t pos = 0;
    std::string search = "\\'";
    std::string replace = "'";
    while ((pos = json.find(search, pos)) != std::string::npos) {
        json.replace(pos, search.length(), replace);
        pos += replace.length();
    }
    dmJson::Result r = dmJson::Parse(json.c_str(), &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
