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

#include <stdlib.h>

#include <testmain/testmain.h>
#include <dlib/dstrings.h>

#include "../script.h"
#include "../script_private.h"
#include "test_script.h"

class PushTableLoggerTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        m_Result[PUSH_TABLE_LOGGER_CAPACITY] = '\0';
    }

    char m_Result[PUSH_TABLE_LOGGER_STR_SIZE];
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
    ASSERT_EQ(1, m_Logger.m_Size);

    PushTableLogChar(m_Logger, 'B');
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_STREQ("AB", m_Result);
    ASSERT_EQ(2, m_Logger.m_Size);

    const char tmp[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int i = 0;

    // Fill until 1 char left of capacity
    // "AB" + 1 empty char = 3
    for (; i < PUSH_TABLE_LOGGER_CAPACITY-3; i++)
    {
        PushTableLogChar(m_Logger, tmp[i%sizeof(tmp)]);
    }
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_EQ(PUSH_TABLE_LOGGER_CAPACITY-1, m_Logger.m_Size);
    ASSERT_EQ(0x0, m_Result[PUSH_TABLE_LOGGER_CAPACITY-1]);

    // Add one more char (fills capacity)
    char first_char = m_Result[0];
    PushTableLogChar(m_Logger, '+');
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_EQ(PUSH_TABLE_LOGGER_CAPACITY, m_Logger.m_Size);
    ASSERT_EQ('+', m_Result[PUSH_TABLE_LOGGER_CAPACITY-1]);
    ASSERT_EQ(first_char, m_Result[0]);

    // Add one more char, will throw out first char (replaced with second in line).
    char second_char = m_Result[1];
    PushTableLogChar(m_Logger, '-');
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_EQ(PUSH_TABLE_LOGGER_CAPACITY, m_Logger.m_Size);
    ASSERT_EQ('-', m_Result[PUSH_TABLE_LOGGER_CAPACITY-1]);
    ASSERT_EQ(second_char, m_Result[0]);
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
}

TEST_F(PushTableLoggerTest, FormatAndPrint)
{
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_STREQ("", m_Result);

    PushTableLogString(m_Logger, "A");
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_STREQ("A", m_Result);

    char t_fmt1[] = "B%dC";
    int t_data1 = 1234567890;
    char t_str1[128];
    dmSnPrintf(t_str1, sizeof(t_str1), t_fmt1, t_data1);

    PushTableLogFormat(m_Logger, t_fmt1, t_data1);
    PushTableLogPrint(m_Logger, m_Result);
    ASSERT_EQ('A', m_Result[0]); // 'A' should still be first char
    ASSERT_STREQ(t_str1, m_Result+1); // Formated string should be the rest of the string.
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
    TestMainPlatformInit();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
