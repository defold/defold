/*
 * Copyright (C) 2017-2025 Muhammad Tayyab Akram
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cassert>
#include <cstdint>
#include <vector>

#include <SheenBidi/SBBase.h>
#include <SheenBidi/SBCodepointSequence.h>

#include "CodepointSequenceTests.h"

using namespace std;
using namespace SheenBidi;

const SBCodepoint FAULTY = SBCodepointFaulty;

template<class CodeUnitType>
static void encTest(SBStringEncoding encoding, const vector<CodeUnitType> &buffer, const vector<uint32_t> &codepoints)
{
    SBCodepointSequence sequence;
    sequence.stringEncoding = encoding;
    sequence.stringBuffer = (void *)buffer.data();
    sequence.stringLength = buffer.size();

    SBUInteger index = 0;
    SBUInteger token = 0;
    SBCodepoint current;

    /* Forward decoding test. */
    while ((current = SBCodepointSequenceGetCodepointAt(&sequence, &token)) != SBCodepointInvalid) {
        assert(index < codepoints.size() && current == codepoints[index]);
        index++;
    }
    assert(index == codepoints.size());

    SBUInteger count = 0;
    index = codepoints.size() - 1;
    token = sequence.stringLength;

    /* Backward decoding test. */
    while ((current = SBCodepointSequenceGetCodepointBefore(&sequence, &token)) != SBCodepointInvalid) {
        assert(count < codepoints.size() && current == codepoints[index--]);
        count++;
    }
    assert(count == codepoints.size());
}

static void u8Test(const vector<uint8_t> &buffer, const vector<uint32_t> &codepoints)
{
    encTest(SBStringEncodingUTF8, buffer, codepoints);
}

static void u16Test(const vector<uint16_t> &buffer, const vector<uint32_t> &codepoints)
{
    encTest(SBStringEncodingUTF16, buffer, codepoints);
}

static void u32Test(const vector<uint32_t> &buffer, const vector<uint32_t> &codepoints)
{
    encTest(SBStringEncodingUTF32, buffer, codepoints);
}

CodepointSequenceTests::CodepointSequenceTests()
{
}

void CodepointSequenceTests::testUTF8()
{
    /* Based On: https://www.w3.org/2001/06/utf-8-wrong/UTF-8-test.html */

    /* Empty sequence. */
    u8Test({ }, { });

    /* Some correct UTF-8 text. */
    u8Test({ 0xCE, 0xBA, 0xE1, 0xBD, 0xB9, 0xCF, 0x83, 0xCE, 0xBC, 0xCE, 0xB5 },
           { 0x03BA, 0x1F79, 0x03C3, 0x03BC, 0x03B5 });

    /* First possible sequence of a certain length. */
    u8Test({ 0x00 }, { 0x00000000 });
    u8Test({ 0xC2, 0x80 }, { 0x00000080 });
    u8Test({ 0xE0, 0xA0, 0x80 }, { 0x00000800 });
    u8Test({ 0xF0, 0x90, 0x80, 0x80 }, { 0x00010000 });
    u8Test({ 0xF8, 0x88, 0x80, 0x80, 0x80 }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0xFC, 0x84, 0x80, 0x80, 0x80, 0x80 }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });

    /* Last possible sequence of a certain length. */
    u8Test({ 0x7F }, { 0x0000007F });
    u8Test({ 0xDF, 0xBF }, { 0x000007FF });
    u8Test({ 0xEF, 0xBF, 0xBF }, { 0x0000FFFF });
    u8Test({ 0xF7, 0xBF, 0xBF, 0xBF }, { FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0xFB, 0xBF, 0xBF, 0xBF, 0xBF }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0xFD, 0xBF, 0xBF, 0xBF, 0xBF, 0xBF }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });

    /* Other boundary conditions. */
    u8Test({ 0xED, 0x9F, 0xBF }, { 0x0000D7FF });
    u8Test({ 0xEE, 0x80, 0x80 }, { 0x0000E000 });
    u8Test({ 0xEF, 0xBF, 0xBD }, { 0x0000FFFD });
    u8Test({ 0xF4, 0x8F, 0xBF, 0xBF }, { 0x0010FFFF });
    u8Test({ 0xF4, 0x90, 0x80, 0x80 }, { FAULTY, FAULTY, FAULTY, FAULTY });

    /* Unexpected continuation bytes. */
    u8Test({ 0x80 }, { FAULTY });
    u8Test({ 0xBF }, { FAULTY });
    u8Test({ 0x80, 0xBF }, { FAULTY, FAULTY });
    u8Test({ 0x80, 0xBF, 0x80 }, { FAULTY, FAULTY, FAULTY });
    u8Test({ 0x80, 0xBF, 0x80, 0xBF }, { FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0x80, 0xBF, 0x80, 0xBF, 0x80 }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0x80, 0xBF, 0x80, 0xBF, 0x80, 0xBF }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0x80, 0xBF, 0x80, 0xBF, 0x80, 0xBF, 0x80 }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });

    /* Sequence of all 64 possible continuation bytes (0x80-0xBF). */
    u8Test({ 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
             0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
             0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
             0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
             0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
             0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
             0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
             0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF },
           {
             FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY,
             FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY,
             FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY,
             FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY,
             FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY,
             FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY,
             FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY,
             FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY
           });
    /* All 32 first bytes of 2-byte sequences (0xC0-0xDF), each followed by a space character. */
    u8Test({ 0xC0, 0x20, 0xC1, 0x20, 0xC2, 0x20, 0xC3, 0x20,
             0xC4, 0x20, 0xC5, 0x20, 0xC6, 0x20, 0xC7, 0x20,
             0xC8, 0x20, 0xC9, 0x20, 0xCA, 0x20, 0xCB, 0x20,
             0xCC, 0x20, 0xCD, 0x20, 0xCE, 0x20, 0xCF, 0x20,
             0xD0, 0x20, 0xD1, 0x20, 0xD2, 0x20, 0xD3, 0x20,
             0xD4, 0x20, 0xD5, 0x20, 0xD6, 0x20, 0xD7, 0x20,
             0xD8, 0x20, 0xD9, 0x20, 0xDA, 0x20, 0xDB, 0x20,
             0xDC, 0x20, 0xDD, 0x20, 0xDE, 0x20, 0xDF, 0x20 },
           {
             FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020,
             FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020,
             FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020,
             FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020,
             FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020,
             FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020,
             FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020,
             FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020
           });
    /* All 16 first bytes of 3-byte sequences (0xE0-0xEF), each followed by a space character. */
    u8Test({ 0xE0, 0x20, 0xE1, 0x20, 0xE2, 0x20, 0xE3, 0x20,
             0xE4, 0x20, 0xE5, 0x20, 0xE6, 0x20, 0xE7, 0x20,
             0xE8, 0x20, 0xE9, 0x20, 0xEA, 0x20, 0xEB, 0x20,
             0xEC, 0x20, 0xED, 0x20, 0xEE, 0x20, 0xEF, 0x20 },
           {
             FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020,
             FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020,
             FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020,
             FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020
           });
    /* All 8 first bytes of 4-byte sequences (0xF0-0xF7), each followed by a space character. */
    u8Test({ 0xF0, 0x20, 0xF1, 0x20, 0xF2, 0x20, 0xF3, 0x20,
             0xF4, 0x20, 0xF5, 0x20, 0xF6, 0x20, 0xF7, 0x20 },
           {
             FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020,
             FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020
           });
    /* All 4 first bytes of 5-byte sequences (0xF8-0xFB), each followed by a space character. */
    u8Test({ 0xF8, 0x20, 0xF9, 0x20, 0xFA, 0x20, 0xFB, 0x20 },
           { FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020, FAULTY, 0x0020 });
    /* All 2 first bytes of 6-byte sequences (0xFC-0xFD), each followed by a space character. */
    u8Test({ 0xFC, 0x20, 0xFD, 0x20 }, { FAULTY, 0x0020, FAULTY, 0x0020 });

    /* Sequences with last continuation byte missing. */
    u8Test({ 0xC0 }, { FAULTY });
    u8Test({ 0xE0, 0x80 }, { FAULTY, FAULTY });
    u8Test({ 0xF0, 0x80, 0x80 }, { FAULTY, FAULTY, FAULTY });
    u8Test({ 0xF8, 0x80, 0x80, 0x80 }, { FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0xFC, 0x80, 0x80, 0x80, 0x80 }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0xDF }, { FAULTY });
    u8Test({ 0xEF, 0xBF }, { FAULTY });
    u8Test({ 0xF7, 0xBF, 0xBF }, { FAULTY, FAULTY, FAULTY });
    u8Test({ 0xFB, 0xBF, 0xBF, 0xBF }, { FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0xFD, 0xBF, 0xBF, 0xBF, 0xBF }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });

    /* Concatenation of incomplete sequences. */
    u8Test({ 0xC0, 0xE0, 0x80, 0xF0, 0x80, 0x80, 0xF8, 0x80, 0x80, 0x80, 0xFC, 0x80,
             0x80, 0x80, 0x80, 0xDF, 0xEF, 0xBF, 0xF7, 0xBF, 0xBF, 0xFB, 0xBF, 0xBF,
             0xBF, 0xFD, 0xBF, 0xBF, 0xBF, 0xBF },
           { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY,
             FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY,
             FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });

    /* Impossible bytes. */
    u8Test({ 0xFE }, { FAULTY });
    u8Test({ 0xFF }, { FAULTY });
    u8Test({ 0xFE, 0xFE, 0xFF, 0xFF }, { FAULTY, FAULTY, FAULTY, FAULTY });

    /* Overlong ASCII character. */
    u8Test({ 0xC0, 0xAF }, { FAULTY, FAULTY });
    u8Test({ 0xE0, 0x80, 0xAF }, { FAULTY, FAULTY, FAULTY });
    u8Test({ 0xF0, 0x80, 0x80, 0xAF }, { FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0xF8, 0x80, 0x80, 0x80, 0xAF }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0xFC, 0x80, 0x80, 0x80, 0x80, 0xAF }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });

    /* Maximum overlong sequences. */
    u8Test({ 0xC1, 0xBF }, { FAULTY, FAULTY });
    u8Test({ 0xE0, 0x9F, 0xBF }, { FAULTY, FAULTY, FAULTY });
    u8Test({ 0xF0, 0x8F, 0xBF, 0xBF }, { FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0xF8, 0x87, 0xBF, 0xBF, 0xBF }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0xFC, 0x83, 0xBF, 0xBF, 0xBF, 0xBF }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });

    /* Overlong representation of the NUL character. */
    u8Test({ 0xC0, 0x80 }, { FAULTY, FAULTY });
    u8Test({ 0xE0, 0x80, 0x80 }, { FAULTY, FAULTY, FAULTY });
    u8Test({ 0xF0, 0x80, 0x80, 0x80 }, { FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0xF8, 0x80, 0x80, 0x80, 0x80 }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0xFC, 0x80, 0x80, 0x80, 0x80, 0x80 }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });

    /* Single UTF-16 surrogates. */
    u8Test({ 0xED, 0xA0, 0x80 }, { FAULTY, FAULTY, FAULTY });
    u8Test({ 0xED, 0xAD, 0xBF }, { FAULTY, FAULTY, FAULTY });
    u8Test({ 0xED, 0xAE, 0x80 }, { FAULTY, FAULTY, FAULTY });
    u8Test({ 0xED, 0xAF, 0xBF }, { FAULTY, FAULTY, FAULTY });
    u8Test({ 0xED, 0xB0, 0x80 }, { FAULTY, FAULTY, FAULTY });
    u8Test({ 0xED, 0xBE, 0x80 }, { FAULTY, FAULTY, FAULTY });
    u8Test({ 0xED, 0xBF, 0xBF }, { FAULTY, FAULTY, FAULTY });

    /* Paired UTF-16 surrogates. */
    u8Test({ 0xED, 0xA0, 0x80, 0xED, 0xB0, 0x80 }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0xED, 0xA0, 0x80, 0xED, 0xBF, 0xBF }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0xED, 0xAD, 0xBF, 0xED, 0xB0, 0x80 }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0xED, 0xAD, 0xBF, 0xED, 0xBF, 0xBF }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0xED, 0xAE, 0x80, 0xED, 0xB0, 0x80 }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0xED, 0xAE, 0x80, 0xED, 0xBF, 0xBF }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0xED, 0xAF, 0xBF, 0xED, 0xB0, 0x80 }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });
    u8Test({ 0xED, 0xAF, 0xBF, 0xED, 0xBF, 0xBF }, { FAULTY, FAULTY, FAULTY, FAULTY, FAULTY, FAULTY });

    /* Other illegal code positions. */
    u8Test({ 0xEF, 0xBF, 0xBE }, { 0xFFFE });
    u8Test({ 0xEF, 0xBF, 0xBF }, { 0xFFFF });
}

void CodepointSequenceTests::testUTF16()
{
    /* Empty sequence. */
    u16Test({ }, { });

    /* Some correct UTF-16 text. */
    u16Test({ 0x0645, 0x062D, 0x0641, 0x0648, 0x0638 }, { 0x0645, 0x062D, 0x0641, 0x0648, 0x0638 });

    /* Lone surrogates. */
    u16Test({ 0xD800 }, { FAULTY });
    u16Test({ 0xD9FF }, { FAULTY });
    u16Test({ 0xDBFF }, { FAULTY });
    u16Test({ 0xDC00 }, { FAULTY });
    u16Test({ 0xDDFF }, { FAULTY });
    u16Test({ 0xDFFF }, { FAULTY });

    /* Surrogates before a space. */
    u16Test({ 0xD800, 0x0020 }, { FAULTY, 0x0020 });
    u16Test({ 0xD9FF, 0x0020 }, { FAULTY, 0x0020 });
    u16Test({ 0xDBFF, 0x0020 }, { FAULTY, 0x0020 });
    u16Test({ 0xDC00, 0x0020 }, { FAULTY, 0x0020 });
    u16Test({ 0xDDFF, 0x0020 }, { FAULTY, 0x0020 });
    u16Test({ 0xDFFF, 0x0020 }, { FAULTY, 0x0020 });

    /* Surrogates after a space. */
    u16Test({ 0x0020, 0xD800 }, { 0x0020, FAULTY });
    u16Test({ 0x0020, 0xD9FF }, { 0x0020, FAULTY });
    u16Test({ 0x0020, 0xDBFF }, { 0x0020, FAULTY });
    u16Test({ 0x0020, 0xDC00 }, { 0x0020, FAULTY });
    u16Test({ 0x0020, 0xDDFF }, { 0x0020, FAULTY });
    u16Test({ 0x0020, 0xDFFF }, { 0x0020, FAULTY });

    /* Paired surrogates. */
    u16Test({ 0xD800, 0xDC00 }, { 0x10000 });
    u16Test({ 0xD800, 0xDDFF }, { 0x101FF });
    u16Test({ 0xD800, 0xDFFF }, { 0x103FF });
    u16Test({ 0xD9FF, 0xDC00 }, { 0x8FC00 });
    u16Test({ 0xD9FF, 0xDDFF }, { 0x8FDFF });
    u16Test({ 0xD9FF, 0xDFFF }, { 0x8FFFF });
    u16Test({ 0xDBFF, 0xDC00 }, { 0x10FC00 });
    u16Test({ 0xDBFF, 0xDDFF }, { 0x10FDFF });
    u16Test({ 0xDBFF, 0xDFFF }, { 0x10FFFF });

    /* Un-paired surrogates. */
    u16Test({ 0xD7FF, 0xDC00 }, { 0xD7FF, FAULTY });
    u16Test({ 0xD7FF, 0xDFFF }, { 0xD7FF, FAULTY });
    u16Test({ 0xD800, 0xE000 }, { FAULTY, 0xE000 });
    u16Test({ 0xDBFF, 0xE000 }, { FAULTY, 0xE000 });
}

void CodepointSequenceTests::testUTF32()
{
    /* Empty sequence. */
    u32Test({ }, { });

    /* Some correct UTF-32 text. */
    u32Test({ 0x0645, 0x062D, 0x0641, 0x0648, 0x0638 }, { 0x0645, 0x062D, 0x0641, 0x0648, 0x0638 });

    /* Invalid boundaries. */
    u32Test({ 0xD800 }, { FAULTY });
    u32Test({ 0xDBFF }, { FAULTY });
    u32Test({ 0xDC00 }, { FAULTY });
    u32Test({ 0xDFFF }, { FAULTY });
    u32Test({ 0x110000 }, { FAULTY });

    /* Valid boundaries. */
    u32Test({ 0x0000 }, { 0x0000 });
    u32Test({ 0xD7FF }, { 0xD7FF });
    u32Test({ 0xE000 }, { 0xE000 });
    u32Test({ 0x10FFFF }, { 0x10FFFF });
}

void CodepointSequenceTests::run()
{
    testUTF8();
    testUTF16();
    testUTF32();
}

#ifdef STANDALONE_TESTING

int main(int argc, const char *argv[]) {
    CodepointSequenceTests codepointSequenceTests;
    codepointSequenceTests.run();

    return 0;
}

#endif
