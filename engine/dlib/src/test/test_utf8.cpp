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

// Bulk validation must match the sequence API's first invalid offset for
// deterministic random bytes, without reading outside exact-sized allocations.
TEST(dmUtf8, BulkValidationDifferential)
{
    uint32_t random = 0x12345678;
    for (uint32_t n = 0; n < 20000; ++n)
    {
        random = random * 1664525u + 1013904223u;
        uint32_t size = random >> 24;
        char* text = (char*)malloc(size ? size : 1);
        for (uint32_t i = 0; i < size; ++i)
        {
            random = random * 1664525u + 1013904223u;
            text[i] = (char)(random >> 24);
        }
        uint32_t valid = 0;
        while (valid < size)
        {
            uint32_t length = dmUtf8::ValidateSequence(text + valid, size - valid);
            if (!length)
            {
                break;
            }
            valid += length;
        }
        EXPECT_EQ(valid, dmUtf8::Validate(text, size));
        free(text);
    }
}

// ASCII fast paths must preserve invalid offsets and handle NUL, multibyte and
// truncated sequences across 16- and 64-byte block boundaries without overread.
TEST(dmUtf8, BulkValidationBoundaries)
{
    const struct
    {
        const char* m_Text;
        uint32_t    m_Size;
        bool        m_Valid;
    } cases[] = {
        { "\x00", 1, true }, { "\xc2\x80", 2, true },
        { "\xe0\xa0\x80", 3, true }, { "\xf4\x8f\xbf\xbf", 4, true },
        { "\x80", 1, false }, { "\xc0\x80", 2, false },
        { "\xed\xa0\x80", 3, false }, { "\xf4\x90\x80\x80", 4, false },
        { "\xc2", 1, false }, { "\xe0\xa0", 2, false },
        { "\xf0\x90\x80", 3, false }
    };
    ASSERT_EQ(0u, dmUtf8::Validate(0, 0));
    for (uint32_t prefix = 0; prefix < 128; ++prefix)
    {
        for (uint32_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        {
            for (uint32_t suffix = 0; suffix <= 16; suffix += 16)
            {
                uint32_t size = prefix + cases[i].m_Size + suffix;
                char* text = (char*)malloc(size);
                memset(text, 'a', size);
                memcpy(text + prefix, cases[i].m_Text, cases[i].m_Size);
                uint32_t result = dmUtf8::Validate(text, size);
                free(text);
                ASSERT_EQ(cases[i].m_Valid ? size : prefix, result);
            }
        }
    }
}

static uint32_t ReferencePrefix(const char* text, uint32_t size)
{
    uint32_t valid = 0;
    while (valid < size)
    {
        uint32_t length = dmUtf8::ValidateSequence(text + valid, size - valid);
        if (!length)
        {
            break;
        }
        valid += length;
    }
    return valid;
}

static uint32_t EncodeScalar(uint32_t cp, char* text)
{
    if (cp < 0x80)
    {
        text[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800)
    {
        text[0] = (char)(0xc0 | (cp >> 6));
        text[1] = (char)(0x80 | (cp & 63));
        return 2;
    }
    if (cp < 0x10000)
    {
        text[0] = (char)(0xe0 | (cp >> 12));
        text[1] = (char)(0x80 | ((cp >> 6) & 63));
        text[2] = (char)(0x80 | (cp & 63));
        return 3;
    }
    text[0] = (char)(0xf0 | (cp >> 18));
    text[1] = (char)(0x80 | ((cp >> 12) & 63));
    text[2] = (char)(0x80 | ((cp >> 6) & 63));
    text[3] = (char)(0x80 | (cp & 63));
    return 4;
}

// Every Unicode scalar must validate across block boundaries, while each
// truncated multibyte prefix reports the original sequence's starting offset.
TEST(dmUtf8, VectorScalarBoundaries)
{
    for (uint32_t cp = 0; cp <= 0x10ffff; ++cp)
    {
        if (cp >= 0xd800 && cp <= 0xdfff)
        {
            continue;
        }
        char text[64];
        memset(text, 'a', sizeof(text));
        uint32_t prefix = cp & 31;
        uint32_t count = EncodeScalar(cp, text + prefix);
        ASSERT_EQ((uint32_t)sizeof(text), dmUtf8::Validate(text, sizeof(text)));
        for (uint32_t length = 1; length < count; ++length)
        {
            ASSERT_EQ(prefix, dmUtf8::Validate(text, prefix + length));
        }
    }
}

// Exhaustive adjacent byte pairs exercise nibble-table edge cases and carry
// cancellation on both sides of the 16- and 64-byte boundaries.
TEST(dmUtf8, VectorBytePairs)
{
    const uint32_t offsets[] = { 14, 15, 62, 63 };
    for (uint32_t alignment = 0; alignment < 2; ++alignment)
    {
        char* allocation = (char*)malloc(96 + alignment);
        char* text = allocation + alignment;
        for (uint32_t offset = 0; offset < sizeof(offsets) / sizeof(offsets[0]); ++offset)
        {
            for (uint32_t pair = 0; pair < 65536; ++pair)
            {
                memset(text, 'a', 96);
                uint32_t p = offsets[offset];
                text[p] = (char)(pair >> 8);
                text[p + 1] = (char)pair;
                text[p + 2] = text[p + 3] = (char)0x80;
                uint32_t expected = ReferencePrefix(text, 96);
                ASSERT_EQ(expected, dmUtf8::Validate(text, 96));
            }
        }
        free(allocation);
    }
}

// Mutations deep inside valid mixed UTF-8 must retain the scalar error offset.
// Exact-sized, unaligned allocations also expose vector lookahead to ASan.
TEST(dmUtf8, VectorMutationOffsets)
{
    uint32_t random = 0x6d2b79f5;
    for (uint32_t trial = 0; trial < 20000; ++trial)
    {
        random = random * 1664525u + 1013904223u;
        uint32_t size = 16 + (random >> 24);
        uint32_t alignment = trial & 15;
        char* allocation = (char*)malloc(size + alignment);
        char* text = allocation + alignment;
        uint32_t cursor = 0;
        while (cursor < size)
        {
            random = random * 1664525u + 1013904223u;
            uint32_t cp = random % 0x110000;
            if (cp >= 0xd800 && cp <= 0xdfff)
            {
                cp = 'a';
            }
            char sequence[4];
            uint32_t count = EncodeScalar(cp, sequence);
            uint32_t available = size - cursor;
            count = count < available ? count : available;
            memcpy(text + cursor, sequence, count);
            cursor += count;
        }
        random = random * 1664525u + 1013904223u;
        text[random % size] = (char)(random >> 24);
        uint32_t expected = ReferencePrefix(text, size);
        uint32_t actual = dmUtf8::Validate(text, size);
        free(allocation);
        ASSERT_EQ(expected, actual);
    }
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
