#include <stdint.h>
#include <gtest/gtest.h>
#include <string.h>
#include <string>
#include "../dlib/json.h"

class dmJsonTest: public ::testing::Test
{

public:
    virtual void SetUp() {}

    virtual void TearDown()
    {
        dmJson::Free(&doc);
    }
    dmJson::Document doc;

};

TEST_F(dmJsonTest, Empty)
{
    const char* json = "";
    dmJson::Result r = dmJson::Parse(json, &doc);
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
    const char* json = "123";
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
    ASSERT_EQ(1, doc.m_NodeCount);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[0].m_Type);
    ASSERT_EQ(0, doc.m_Nodes[0].m_Start);
    ASSERT_EQ(3, doc.m_Nodes[0].m_End);
}

TEST_F(dmJsonTest, Float)
{
    const char* json = "123.456";
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
    ASSERT_EQ(1, doc.m_NodeCount);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[0].m_Type);
    ASSERT_EQ(0, doc.m_Nodes[0].m_Start);
    ASSERT_EQ(7, doc.m_Nodes[0].m_End);
}

TEST_F(dmJsonTest, Boolean)
{
    const char* json = "true";
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
    ASSERT_EQ(1, doc.m_NodeCount);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[0].m_Type);
    ASSERT_EQ(0, doc.m_Nodes[0].m_Start);
    ASSERT_EQ(4, doc.m_Nodes[0].m_End);
}

TEST_F(dmJsonTest, Null)
{
    const char* json = "null";
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
    ASSERT_EQ(1, doc.m_NodeCount);
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[0].m_Type);
    ASSERT_EQ(0, doc.m_Nodes[0].m_Start);
    ASSERT_EQ(4, doc.m_Nodes[0].m_End);
}

TEST_F(dmJsonTest, String)
{
    const char* json = "\"json\"";
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
    ASSERT_EQ(1, doc.m_NodeCount);
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[0].m_Type);
    ASSERT_EQ(1, doc.m_Nodes[0].m_Start);
    ASSERT_EQ(5, doc.m_Nodes[0].m_End);
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
    const char* json = "10,20";
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
    ASSERT_EQ(1, doc.m_NodeCount);
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

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
