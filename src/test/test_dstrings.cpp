#include <stdint.h>
#include <gtest/gtest.h>
#include <string.h>
#include "dlib/dstrings.h"

TEST(dmStrings, dmStrTok1)
{
    char* string = strdup("");
    char* s, *last;
    s = dmStrTok(string, " ", &last);
    ASSERT_EQ((void*) 0, s);

    free(string);
}

TEST(dmStrings, dmStrTok2)
{
    char* string = strdup("a");
    char* s, *last;
    s = dmStrTok(string, " ", &last);
    ASSERT_NE((void*) 0, s);
    ASSERT_STREQ("a", s);

    s = dmStrTok(0, " ", &last);
    ASSERT_EQ((void*) 0, s);

    free(string);
}

TEST(dmStrings, dmStrTok3)
{
    char* string = strdup("a b c");
    char* s, *last;
    s = dmStrTok(string, " ", &last);
    ASSERT_NE((void*) 0, s);
    ASSERT_STREQ("a", s);

    s = dmStrTok(0, " ", &last);
    ASSERT_NE((void*) 0, s);
    ASSERT_STREQ("b", s);

    s = dmStrTok(0, " ", &last);
    ASSERT_NE((void*) 0, s);
    ASSERT_STREQ("c", s);

    s = dmStrTok(0, " ", &last);
    ASSERT_EQ((void*) 0, s);

    free(string);
}

TEST(dmStrings, dmStrTok4)
{
    char* string = strdup("foo\r\nbar");
    char* s, *last;
    s = dmStrTok(string, "\r\n", &last);
    ASSERT_NE((void*) 0, s);
    ASSERT_STREQ("foo", s);

    s = dmStrTok(0, "\r\n", &last);
    ASSERT_NE((void*) 0, s);
    ASSERT_STREQ("bar", s);

    s = dmStrTok(0, "\r\n", &last);
    ASSERT_EQ((void*) 0, s);

    free(string);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
