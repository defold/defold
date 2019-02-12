#include <stdlib.h>
#include <dlib/dstrings.h>
#include <gtest/gtest.h>
#include "../script.h"


class PushTableLoggerTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_Result[PUSH_TABLE_LOGGER_CAPACITY] = '\0';
    }

    char m_Result[PUSH_TABLE_LOGGER_CAPACITY + 1];
    dmScript::PushTableLogger m_Logger;

};

TEST_F(PushTableLoggerTest, EmptyLog)
{
    ASSERT_EQ(0x0, m_Logger.m_Log[0]);
    ASSERT_EQ(0, m_Logger.m_Size);
    ASSERT_EQ(0, m_Logger.m_Cursor);

    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_STREQ("", m_Result);
}

TEST_F(PushTableLoggerTest, AddCharAndPrint)
{
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_STREQ("", m_Result);

    PushTableLogChar(m_Logger, 'A');
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_STREQ("A", m_Result);

    PushTableLogChar(m_Logger, 'B');
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_STREQ("AB", m_Result);

    const char tmp[] = "CDEFGHIJKLMNOPQRSTUVWXYZ";
    int i = 0;

    for (; i < 14; i++)
    {
        PushTableLogChar(m_Logger, tmp[i]);
    }
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_STREQ("ABCDEFGHIJKLMNOP", m_Result);

    for (; i < 24; i++)
    {
        PushTableLogChar(m_Logger, tmp[i]);
    }
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_STREQ("KLMNOPQRSTUVWXYZ", m_Result);
}

TEST_F(PushTableLoggerTest, AddStringAndPrint)
{
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_STREQ("", m_Result);

    PushTableLogString(m_Logger, "A");
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_STREQ("A", m_Result);

    PushTableLogString(m_Logger, "BCDE");
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_STREQ("ABCDE", m_Result);

    PushTableLogString(m_Logger, "FGHIJKLMNOPQRSTUVWXYZ");
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_STREQ("KLMNOPQRSTUVWXYZ", m_Result);
}

TEST_F(PushTableLoggerTest, FormatAndPrint)
{
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_STREQ("", m_Result);

    PushTableLogString(m_Logger, "A");
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_STREQ("A", m_Result);

    PushTableLogFormat(m_Logger, "B%dC", 1234567890);
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_STREQ("AB1234567890C", m_Result);

    PushTableLogFormat(m_Logger, "D%dE", 1234567890);
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_STREQ("890CD1234567890E", m_Result);

    PushTableLogFormat(m_Logger, "F[%d](%d)G", 1234567890, 987654321);
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_STREQ("890](987654321)G", m_Result);
}

static const char g_GuardStr[] = "GUARD";
static const size_t g_GuardStrSize = sizeof(g_GuardStr);

static bool CheckGuardBytes(const char* buf)
{
    return (memcmp(buf, g_GuardStr, sizeof(g_GuardStr)) == 0);
}

TEST_F(PushTableLoggerTest, OOB)
{

    char oob_test_wrap[PUSH_TABLE_LOGGER_STR_SIZE+g_GuardStrSize+g_GuardStrSize];
    char* guard_start = oob_test_wrap;
    char* guard_end = oob_test_wrap+PUSH_TABLE_LOGGER_STR_SIZE+g_GuardStrSize;

    // write guard string to beginning and end of result buffer wrap
    memcpy(guard_start, g_GuardStr, g_GuardStrSize);
    memcpy(guard_end, g_GuardStr, g_GuardStrSize);

    ASSERT_TRUE(CheckGuardBytes(guard_start));
    ASSERT_TRUE(CheckGuardBytes(guard_end));

    for (int i = 0; i < PUSH_TABLE_LOGGER_CAPACITY*2; ++i)
    {
        PushTableLogChar(m_Logger, 'X');
        ASSERT_TRUE(CheckGuardBytes(guard_start));
        ASSERT_TRUE(CheckGuardBytes(guard_end));
    }
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
