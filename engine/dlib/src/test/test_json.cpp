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

TEST_F(dmJsonTest, DEF1653)
{
    const char* json = "{ response = \"ok\" }";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(4, doc.m_NodeCount);
    ASSERT_EQ(dmJson::TYPE_OBJECT, doc.m_Nodes[0].m_Type);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[1].m_Type);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[2].m_Type);
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[3].m_Type);
}

TEST_F(dmJsonTest, NullPointer)
{
    const char* json = 0x0;
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(0, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Empty)
{
    const char* json = "";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(0, doc.m_NodeCount);
}

TEST_F(dmJsonTest, MultiRoot)
{
    const char* json = "[1],[2]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
}

TEST_F(dmJsonTest, Root_Number)
{
    const char* json = "10";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(1, doc.m_NodeCount);
    ASSERT_EQ(0, doc.m_Nodes[0].m_Start);
    ASSERT_EQ(2, doc.m_Nodes[0].m_End);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[0].m_Type);
}

TEST_F(dmJsonTest, Root_Float)
{
    const char* json = "10.05";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(1, doc.m_NodeCount);
    ASSERT_EQ(0, doc.m_Nodes[0].m_Start);
    ASSERT_EQ(5, doc.m_Nodes[0].m_End);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[0].m_Type);
}

TEST_F(dmJsonTest, Root_Boolean)
{
    const char* json = "true";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(1, doc.m_NodeCount);
    ASSERT_EQ(0, doc.m_Nodes[0].m_Start);
    ASSERT_EQ(4, doc.m_Nodes[0].m_End);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[0].m_Type);
}

TEST_F(dmJsonTest, Root_Null)
{
    const char* json = "null";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(1, doc.m_NodeCount);
    ASSERT_EQ(0, doc.m_Nodes[0].m_Start);
    ASSERT_EQ(4, doc.m_Nodes[0].m_End);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[0].m_Type);
}

TEST_F(dmJsonTest, Root_String)
{
    const char* json = "\"defold\"";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(1, doc.m_NodeCount);
    ASSERT_EQ(1, doc.m_Nodes[0].m_Start);
    ASSERT_EQ(7, doc.m_Nodes[0].m_End);
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[0].m_Type);
}

TEST_F(dmJsonTest, Root_Array)
{
    const char* json = "[1]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
    // These checks are for the array object
    ASSERT_EQ(0, doc.m_Nodes[0].m_Start);
    ASSERT_EQ(3, doc.m_Nodes[0].m_End);
    ASSERT_EQ(dmJson::TYPE_ARRAY, doc.m_Nodes[0].m_Type);
    // These checks are for the primitive object within the array
    ASSERT_EQ(1, doc.m_Nodes[1].m_Start);
    ASSERT_EQ(2, doc.m_Nodes[1].m_End);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[1].m_Type);
}

TEST_F(dmJsonTest, Root_Object)
{
    const char* json = "{\"key\": \"value\"}";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(3, doc.m_NodeCount);
    // These checks are for the object
    ASSERT_EQ(0, doc.m_Nodes[0].m_Start);
    ASSERT_EQ(16, doc.m_Nodes[0].m_End);
    ASSERT_EQ(dmJson::TYPE_OBJECT, doc.m_Nodes[0].m_Type);
    // These checks are for the first string within the object
    ASSERT_EQ(2, doc.m_Nodes[1].m_Start);
    ASSERT_EQ(5, doc.m_Nodes[1].m_End);
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[1].m_Type);
    // These checks are for the second string within the object
    ASSERT_EQ(9, doc.m_Nodes[2].m_Start);
    ASSERT_EQ(14, doc.m_Nodes[2].m_End);
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[2].m_Type);
}

TEST_F(dmJsonTest, Primitive_Number_Positive)
{
    const char* json = "[10]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Primitive_Number_Negative)
{
    const char* json = "[-10]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Primitive_Number_Zero)
{
    const char* json = "[0]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Primitive_Number_NegativeZero)
{
    const char* json = "[-0]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Primitive_Number_LeadingZero)
{
    const char* json = "[010]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Primitive_Float_Positive)
{
    const char* json = "[10.05]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Primitive_Float_Negative)
{
    const char* json = "[-10.05]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Primitive_Float_Zero)
{
    const char* json = "[0.0]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Primitive_Float_NegativeZero)
{
    const char* json = "[-0.0]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Primitive_Float_LeadingZero)
{
    const char* json = "[010.05]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Primitive_Boolean_TrueLowercase)
{
    const char* json = "[true]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Primitive_Boolean_TrueUppercase)
{
    const char* json = "[TRUE]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Primitive_Boolean_TrueTitlecase)
{
    const char* json = "[True]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Primitive_Boolean_FalseLowercase)
{
    const char* json = "[false]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Primitive_Boolean_FalseUppercase)
{
    const char* json = "[FALSE]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Primitive_Boolean_FalseTitlecase)
{
    const char* json = "[False]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Primitive_Null_Lowercase)
{
    const char* json = "[null]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Primitive_Null_Uppercase)
{
    const char* json = "[NULL]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Primitive_Null_Titlecase)
{
    const char* json = "[Null]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Primitive_Invalid_Characters)
{
    const char* json = "[defold]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[1].m_Type);
}

TEST_F(dmJsonTest, Primitive_Invalid_Symbols)
{
    const char* json = "[==&%]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[1].m_Type);
}

TEST_F(dmJsonTest, Primitive_Invalid_Mixed)
{
    const char* json = "[def%%%$old]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[1].m_Type);
}

TEST_F(dmJsonTest, String_Empty)
{
    const char* json = "\"\"";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(1, doc.m_NodeCount);
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[0].m_Type);
    ASSERT_EQ(1, doc.m_Nodes[0].m_Start);
    ASSERT_EQ(1, doc.m_Nodes[0].m_End);
}

TEST_F(dmJsonTest, String_NotTerminated)
{
    const char* json = "\"defold";
    ASSERT_EQ(dmJson::RESULT_INCOMPLETE, dmJson::Parse(json, &doc));
    ASSERT_EQ(0, doc.m_NodeCount);
}

TEST_F(dmJsonTest, String_OverTerminated)
{
    const char* json = "\"defold\"\"";
    ASSERT_EQ(dmJson::RESULT_INCOMPLETE, dmJson::Parse(json, &doc));
    ASSERT_EQ(0, doc.m_NodeCount);
}

TEST_F(dmJsonTest, String_EscapedQuote)
{
    const char* json = "\"- \\\"defold\\\" -\"";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
}

TEST_F(dmJsonTest, String_EscapedUnicode)
{    /*
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
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(1, doc.m_NodeCount);
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[0].m_Type);
    ASSERT_EQ(0xc3u, (uint8_t) doc.m_Json[(&doc.m_Nodes[0])->m_Start + 0]);
    ASSERT_EQ(0xa5u, (uint8_t) doc.m_Json[(&doc.m_Nodes[0])->m_Start + 1]);
    ASSERT_EQ(0xb6u, (uint8_t) doc.m_Json[(&doc.m_Nodes[0])->m_Start + 5]);
    ASSERT_EQ('f', (uint8_t) doc.m_Json[(&doc.m_Nodes[0])->m_Start + 6]);
    ASSERT_EQ('o', (uint8_t) doc.m_Json[(&doc.m_Nodes[0])->m_Start + 7]);
    ASSERT_EQ('o', (uint8_t) doc.m_Json[(&doc.m_Nodes[0])->m_Start + 8]);
}

TEST_F(dmJsonTest, Array_Empty)
{
    const char* json = "[]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(1, doc.m_NodeCount);
    ASSERT_EQ(0, doc.m_Nodes[0].m_Start);
    ASSERT_EQ(2, doc.m_Nodes[0].m_End);
    ASSERT_EQ(dmJson::TYPE_ARRAY, doc.m_Nodes[0].m_Type);
}

TEST_F(dmJsonTest, Array_Primitives)
{
    const char* json = "[1, 2, 3,4, 5, 6,6,7]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(9, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Array_Strings)
{
    const char* json = "[\"alpha\", \"and\",\"omega\"]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(4, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Array_Nested)
{
    const char* json = "[[[],[],[],[[]]],[]]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(8, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Array_Mixed)
{
    const char* json = "[1, \"two\", [3],{\"four\": 5}]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(8, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Array_NotTerminated)
{
    const char* json = "[1, 2, 3, 4";
    ASSERT_EQ(dmJson::RESULT_INCOMPLETE, dmJson::Parse(json, &doc));
    ASSERT_EQ(0, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Array_OverTerminated)
{
    const char* json = "[1, 2, 3]]";
    ASSERT_EQ(dmJson::RESULT_SYNTAX_ERROR, dmJson::Parse(json, &doc));
    ASSERT_EQ(0, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Array_Whitespace)
{
    const char* json = "[1      ,       4        ]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(3, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Array_Linebreaks)
{
    const char* json = "\n\r[1,\n\r3\r\n,5]\r\n";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(4, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Array_SeparatorInvalid)
{
    const char* json = "[1 : 2 : 3 : 4 ]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(5, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Array_SeparatorMissing)
{
    const char* json = "[1 2 3 4]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(5, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Array_SeparatorMultiple)
{
    const char* json = "[1,,,2,,,3,,,4]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(5, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Array_SeparatorLeading)
{
    const char* json = "[,1,4]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(3, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Array_SeparatorTrailing)
{
    const char* json = "[1,4,]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(3, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Array_ObjectIntertwined)
{
    const char* json = "[1, 2, {\"three\": 4]}";
    ASSERT_EQ(dmJson::RESULT_SYNTAX_ERROR, dmJson::Parse(json, &doc));
    ASSERT_EQ(0, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Array_Large)
{
    std::string json = "[";
    const int N = 32768;
    for (int i = 0; i < N; ++i)
    {
        json += "0";
        if (i != N - 1)
            json += ",";
    }
    json += "]";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json.c_str(), &doc));
    ASSERT_EQ(1 + N, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Object_Empty)
{
    const char* json = "{}";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(1, doc.m_NodeCount);
    ASSERT_EQ(0, doc.m_Nodes[0].m_Start);
    ASSERT_EQ(2, doc.m_Nodes[0].m_End);
    ASSERT_EQ(dmJson::TYPE_OBJECT, doc.m_Nodes[0].m_Type);
}

TEST_F(dmJsonTest, Object_Primitives)
{
    const char* json = "{1:2, 3:4,4:5, 5: 6, 7 : 8 }";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(11, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Object_Strings)
{
    const char* json = "{\"one\": \"two\", \"three\": \"four\", \"five\": \"six\"}";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(7, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Object_Nested)
{
    const char* json = "{1: {2: {3: {4: 5}}}}";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(9, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Object_Mixed)
{
    const char* json = "{1: \"two\", \"three\": 4}";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(5, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Object_NotTerminated)
{
    const char* json = "{1: 2, 3: 4";
    ASSERT_EQ(dmJson::RESULT_INCOMPLETE, dmJson::Parse(json, &doc));
    ASSERT_EQ(0, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Object_OverTerminated)
{
    const char* json = "{1: 2, 3: 4}}";
    ASSERT_EQ(dmJson::RESULT_SYNTAX_ERROR, dmJson::Parse(json, &doc));
}

TEST_F(dmJsonTest, Object_Whitespace)
{
    const char* json = "{    1      :      2     ,    3   :      4   }   ";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(5, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Object_Linebreaks)
{
    const char* json = "\r\n{\n\r\r1:\r\r\n2,3:\n\n\r4\r\n}\n\n";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(5, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Object_AssignmentInvalid)
{
    const char* json = "{1 = 2}";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(4, doc.m_NodeCount); // The equal sign is counted as a primitive
    ASSERT_EQ(dmJson::TYPE_OBJECT, doc.m_Nodes[0].m_Type);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[1].m_Type);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[2].m_Type);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[3].m_Type);
}

TEST_F(dmJsonTest, Object_AssignmentMissing)
{
    const char* json = "{1 2}";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(3, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Object_AssignmentMultiple)
{
    const char* json = "{1 ::::::::: 2}";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(3, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Object_SeparatorInvalid)
{
    const char* json = "{1 : 2 : 3 : 4}";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(5, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Object_SeparatorMissing)
{
    const char* json = "{1:2 3:4}";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(5, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Object_SeparatorMultiple)
{
    const char* json = "{1:2,,,,,,,,,,,3:4}";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(5, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Object_ArrayIntertwined)
{
    const char* json = "{1: [2, 3}, 4]";
    ASSERT_EQ(dmJson::RESULT_SYNTAX_ERROR, dmJson::Parse(json, &doc));
    ASSERT_EQ(0, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Object_ObjectKey)
{
    const char* json = "{{1: 2}: 3}";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(5, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Object_MissingValue)
{
    const char* json = "{1: }";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(2, doc.m_NodeCount);
}

TEST_F(dmJsonTest, Object_MultipleMissingValue)
{
    const char* json = "{1: , 2: , 3: }";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(4, doc.m_NodeCount);
    ASSERT_EQ(3, doc.m_Nodes[0].m_Size);
}

TEST_F(dmJsonTest, Object_SeparatorLeading)
{
    const char* json = "{,,,,,,,,,,1: 2, 3: 4}";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(5, doc.m_NodeCount);
    ASSERT_EQ(4, doc.m_Nodes[0].m_Size);
}

TEST_F(dmJsonTest, Object_SeparatorTrailing)
{
    const char* json = "{1: 2, 3: 4,,,,,,,,,,}";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(5, doc.m_NodeCount);
    ASSERT_EQ(4, doc.m_Nodes[0].m_Size);
}

TEST_F(dmJsonTest, Object_MultipleValue)
{
    const char* json = "{1: 2, 3, 4}";
    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json, &doc));
    ASSERT_EQ(5, doc.m_NodeCount);
    ASSERT_EQ(4, doc.m_Nodes[0].m_Size);
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

    ASSERT_EQ(dmJson::RESULT_OK, dmJson::Parse(json.c_str(), &doc));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
