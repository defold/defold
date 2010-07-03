#include <stdint.h>
#include <gtest/gtest.h>
#include <string.h>
#include "dlib/dstrings.h"

TEST(dmStrings, dmStrSep1)
{
    char* string = strdup("");
    char* s;
    s = dmStrSep(&string, " ");
    ASSERT_NE((void*) 0, s);
    ASSERT_STREQ("", s);

    s = dmStrSep(&string, " ");
    ASSERT_EQ((void*) 0, s);

    free(string);
}

TEST(dmStrings, dmStrSep2)
{
    char* string = strdup("a");
    char* s;
    s = dmStrSep(&string, " ");
    ASSERT_NE((void*) 0, s);
    ASSERT_STREQ("a", s);

    s = dmStrSep(&string, " ");
    ASSERT_EQ((void*) 0, s);

    free(string);
}

TEST(dmStrings, dmStrSep3)
{
    char* string = strdup("a b c");
    char* s;
    s = dmStrSep(&string, " ");
    ASSERT_NE((void*) 0, s);
    ASSERT_STREQ("a", s);

    s = dmStrSep(&string, " ");
    ASSERT_NE((void*) 0, s);
    ASSERT_STREQ("b", s);

    s = dmStrSep(&string, " ");
    ASSERT_NE((void*) 0, s);
    ASSERT_STREQ("c", s);

    s = dmStrSep(&string, " ");
    ASSERT_EQ((void*) 0, s);

    free(string);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
