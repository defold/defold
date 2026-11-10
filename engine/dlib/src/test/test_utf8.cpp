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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
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
const char seq1[] = {(char) 0xc3, (char) 0xa5, (char) 0xc3, (char) 0xa4, (char) 0xc3, (char) 0xb6, 0 };

// WHITE RIGHT POINTING INDEX
// Unicode: U+261E, UTF-8: E2 98 9E
const char seq2[] = {(char) 0xe2, (char) 0x98, (char) 0x9e, 0};

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

    s = "\x00\x80";
    ASSERT_EQ((uint32_t) '\0', dmUtf8::NextChar(&s));

    s = "x";
    ASSERT_EQ((uint32_t) 'x', dmUtf8::NextChar(&s));
    ASSERT_EQ((uint32_t) '\0', dmUtf8::NextChar(&s));

    s = seq1;
    ASSERT_EQ(0xe5U, dmUtf8::NextChar(&s));
    ASSERT_EQ(0xe4U, dmUtf8::NextChar(&s));
    ASSERT_EQ(0xf6U, dmUtf8::NextChar(&s));
    ASSERT_EQ(0U, dmUtf8::NextChar(&s));

    s = seq2;
    ASSERT_EQ(0x261eU, dmUtf8::NextChar(&s));
    ASSERT_EQ(0U, dmUtf8::NextChar(&s));
}

TEST(dmUtf8, ToUtf8)
{
    char buf[4];
    uint32_t n;

    n = dmUtf8::ToUtf8('x', buf);
    ASSERT_EQ(1U, n);
    ASSERT_EQ('x', (int16_t) buf[0]);

    // ä
    n = dmUtf8::ToUtf8(0xe4, buf);
    ASSERT_EQ(2U, n);
    ASSERT_EQ(0xc3U, (uint8_t) buf[0]);
    ASSERT_EQ(0xa4U, (uint8_t) buf[1]);

    // See seq2
    n = dmUtf8::ToUtf8(0x261e, buf);
    ASSERT_EQ(3U, n);
    ASSERT_EQ(0xe2U, (uint8_t) buf[0]);
    ASSERT_EQ(0x98U, (uint8_t) buf[1]);
    ASSERT_EQ(0x9eU, (uint8_t) buf[2]);
}

// Validate scalar-value boundaries without treating NUL as a string terminator
// or consuming bytes from the next sequence.
TEST(dmUtf8, ValidateSequence)
{
    const struct
    {
        const char* m_Text;
        uint32_t    m_Size;
    } valid[] = {
        { "\x00", 1 }, { "\x7f", 1 },
        { "\xc2\x80", 2 }, { "\xdf\xbf", 2 },
        { "\xe0\xa0\x80", 3 }, { "\xed\x9f\xbf", 3 },
        { "\xee\x80\x80", 3 }, { "\xef\xbf\xbf", 3 },
        { "\xf0\x90\x80\x80", 4 }, { "\xf4\x8f\xbf\xbf", 4 }
    };
    ASSERT_EQ(0u, dmUtf8::ValidateSequence(0, 0));
    for (uint32_t i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i)
    {
        ASSERT_EQ(valid[i].m_Size, dmUtf8::ValidateSequence(valid[i].m_Text, valid[i].m_Size));
        ASSERT_EQ(valid[i].m_Size, dmUtf8::ValidateSequence(valid[i].m_Text, valid[i].m_Size + 1));
    }
}

// Reject invalid leaders and continuations, overlong encodings, UTF-16
// surrogates and values above U+10FFFF consistently for JSON and the debugger.
TEST(dmUtf8, InvalidSequences)
{
    const char* invalid[] = {
        "\x80", "\xbf", "\xc0\x80", "\xc1\xbf", "\xc2\x7f",
        "\xe0\x9f\xbf", "\xe1\x80\x7f", "\xed\xa0\x80", "\xed\xbf\xbf",
        "\xf0\x8f\xbf\xbf", "\xf1\x80\x80\x7f", "\xf4\x90\x80\x80",
        "\xf5\x80\x80\x80", "\xf8\x88\x80\x80\x80", "\xff"
    };
    for (uint32_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
    {
        ASSERT_EQ(0u, dmUtf8::ValidateSequence(invalid[i], (uint32_t)strlen(invalid[i])));
    }
}

// Every truncated prefix must fail without reading past its allocation;
// exact-sized buffers let ASan detect accidental lookahead beyond size.
TEST(dmUtf8, TruncatedSequences)
{
    const char* sequences[] = { "\xc2\x80", "\xe0\xa0\x80", "\xf0\x90\x80\x80" };
    for (uint32_t i = 0; i < sizeof(sequences) / sizeof(sequences[0]); ++i)
    {
        uint32_t size = (uint32_t)strlen(sequences[i]);
        for (uint32_t length = 1; length < size; ++length)
        {
            char* prefix = (char*)malloc(length);
            memcpy(prefix, sequences[i], length);
            uint32_t result = dmUtf8::ValidateSequence(prefix, length);
            free(prefix);
            ASSERT_EQ(0u, result);
        }
    }
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
