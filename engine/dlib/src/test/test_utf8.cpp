#include <stdint.h>
#include <stdio.h>
#include <gtest/gtest.h>
#include "../dlib/utf8.h"

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
const char seq1[] = {0xc3, 0xa5, 0xc3, 0xa4, 0xc3, 0xb6, 0 };

// WHITE RIGHT POINTING INDEX
// Unicode: U+261E, UTF-8: E2 98 9E
const char seq2[] = {0xe2, 0x98, 0x9e, 0};

TEST(dmUtf8, StrLen)
{
    ASSERT_EQ(0U, dmUtf8::StrLen(""));
    ASSERT_EQ(1U, dmUtf8::StrLen("x"));

    ASSERT_EQ(3U, dmUtf8::StrLen(seq1));
    ASSERT_EQ(1U, dmUtf8::StrLen(seq2));
}

TEST(dmUtf8, NextChar)
{
    const char* s;

    s = "";
    ASSERT_EQ((uint32_t) '\0', dmUtf8::NextChar(&s));

    s = "x";
    ASSERT_EQ((uint32_t) 'x', dmUtf8::NextChar(&s));
    ASSERT_EQ((uint32_t) '\0', dmUtf8::NextChar(&s));

    s = seq1;
    ASSERT_EQ(0xe5, dmUtf8::NextChar(&s));
    ASSERT_EQ(0xe4, dmUtf8::NextChar(&s));
    ASSERT_EQ(0xf6, dmUtf8::NextChar(&s));
    ASSERT_EQ(0, dmUtf8::NextChar(&s));

    s = seq2;
    ASSERT_EQ(0x261e, dmUtf8::NextChar(&s));
    ASSERT_EQ(0, dmUtf8::NextChar(&s));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
