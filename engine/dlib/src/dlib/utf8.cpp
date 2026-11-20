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

#include "utf8.h"
#include <dlib/endian.h>
#include <simd/simd.h>

/**
 * utf-8 support
 *
 * Except for Validate and ValidateSequence, supplied utf-8 sequences must be
 * well-formed; invalid sequences are not detected. It's however guaranteed that
 * the library function will not read outside the buffer and the terminating
 * '\0' character is always respected.
 */
namespace dmUtf8
{
#if DM_ENDIAN == DM_ENDIAN_LITTLE && !DM_SIMD_FORCE_SCALAR && (defined(DM_SIMD_NEON) || defined(DM_SIMD_WASM))
    // Nibble tables adapted from simdjson's utf8_lookup4_algorithm.h (MIT).
    // Copyright 2018-2025 The simdjson authors
    //
    // Permission is hereby granted, free of charge, to any person obtaining a copy of
    // this software and associated documentation files (the "Software"), to deal in
    // the Software without restriction, including without limitation the rights to
    // use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
    // the Software, and to permit persons to whom the Software is furnished to do so,
    // subject to the following conditions:
    //
    // The above copyright notice and this permission notice shall be included in all
    // copies or substantial portions of the Software.
    //
    // THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    // IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
    // FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
    // COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
    // IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
    // CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
    // The block starts at a sequence boundary. The caller rechecks any
    // trailing partial sequence instead of consuming its leading bytes.
    static bool ValidateBlock(dsimd_uint4_t bytes)
    {
        // Table bits describe illegal adjacent-byte combinations. The final
        // continuation check cancels bits required by three/four-byte sequences.
        enum
        {
            SHORT         = 1,
            LONG          = 2,
            OVERLONG3     = 4,
            LARGE         = 8,
            SURROGATE     = 16,
            OVERLONG2     = 32,
            OVERLONG4     = 64,
            CONTINUATIONS = 128,
            CARRY         = SHORT | LONG | CONTINUATIONS
        };
        static const uint8_t previous_high[16] = {
            LONG, LONG, LONG, LONG, LONG, LONG, LONG, LONG, CONTINUATIONS, CONTINUATIONS, CONTINUATIONS, CONTINUATIONS, SHORT | OVERLONG2, SHORT, SHORT | OVERLONG3 | SURROGATE, SHORT | LARGE | OVERLONG4
        };
        static const uint8_t previous_low[16] = {
            CARRY | OVERLONG3 | OVERLONG2 | OVERLONG4, CARRY | OVERLONG2, CARRY, CARRY, CARRY | LARGE, CARRY | LARGE | OVERLONG4, CARRY | LARGE | OVERLONG4, CARRY | LARGE | OVERLONG4, CARRY | LARGE | OVERLONG4, CARRY | LARGE | OVERLONG4, CARRY | LARGE | OVERLONG4, CARRY | LARGE | OVERLONG4, CARRY | LARGE | OVERLONG4, CARRY | LARGE | OVERLONG4 | SURROGATE, CARRY | LARGE | OVERLONG4, CARRY | LARGE | OVERLONG4
        };
        static const uint8_t current_high[16] = {
            SHORT, SHORT, SHORT, SHORT, SHORT, SHORT, SHORT, SHORT, LONG | OVERLONG2 | CONTINUATIONS | OVERLONG3 | OVERLONG4, LONG | OVERLONG2 | CONTINUATIONS | OVERLONG3 | LARGE, LONG | OVERLONG2 | CONTINUATIONS | SURROGATE | LARGE, LONG | OVERLONG2 | CONTINUATIONS | SURROGATE | LARGE, SHORT, SHORT, SHORT, SHORT
        };
        // Each call starts at a sequence boundary; prior bytes are ASCII zeros.
        static const uint8_t shift1[16] = { 255, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };
        static const uint8_t shift2[16] = { 255, 255, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };
        static const uint8_t shift3[16] = { 255, 255, 255, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
        // Align the previous three bytes with each current byte. Shuffle
        // indices outside this block supply zeros because it starts at a boundary.
        dsimd_byte16_t input = SimdUintToBytes(bytes);
        dsimd_uint4_t previous1 = SimdBytesToUint(SimdShuffleBytes(input, SimdLoadBytes(shift1)));
        dsimd_uint4_t previous2 = SimdBytesToUint(SimdShuffleBytes(input, SimdLoadBytes(shift2)));
        dsimd_uint4_t previous3 = SimdBytesToUint(SimdShuffleBytes(input, SimdLoadBytes(shift3)));
        dsimd_uint4_t nibble_mask = SimdSplatUint(0x0f0f0f0f);
        dsimd_byte16_t high1 = SimdUintToBytes(SimdAndUint(SimdShiftRightUint(previous1, 4), nibble_mask));
        dsimd_byte16_t low1 = SimdUintToBytes(SimdAndUint(previous1, nibble_mask));
        dsimd_byte16_t high = SimdUintToBytes(SimdAndUint(SimdShiftRightUint(bytes, 4), nibble_mask));
        // Intersect the previous byte's high/low nibble classes with the
        // current high nibble to detect invalid pairs in parallel.
        dsimd_uint4_t special = SimdAndUint(SimdBytesToUint(SimdShuffleBytes(SimdLoadBytes(previous_high), high1)),
                                            SimdBytesToUint(SimdShuffleBytes(SimdLoadBytes(previous_low), low1)));
        special = SimdAndUint(special, SimdBytesToUint(SimdShuffleBytes(SimdLoadBytes(current_high), high)));
        // These masked additions cannot carry between bytes. A high bit is set
        // only for E0..FF two bytes back, or F0..FF three bytes back.
        dsimd_uint4_t third = SimdAndUint(previous2, SimdAddUint(SimdAndUint(previous2, SimdSplatUint(0x60606060)), SimdSplatUint(0x20202020)));
        dsimd_uint4_t fourth = SimdAndUint(previous3, SimdAddUint(SimdAndUint(previous3, SimdSplatUint(0x70707070)), SimdSplatUint(0x10101010)));
        dsimd_uint4_t required = SimdAndUint(SimdOrUint(third, fourth), SimdSplatUint(0x80808080));
        return SimdAll(SimdEqualUint(SimdXorUint(special, required), SimdSplatUint(0)));
    }
#endif

    uint32_t ValidateSequence(const char* text, uint32_t size)
    {
        if (size == 0)
        {
            return 0;
        }
        unsigned char first = (unsigned char)text[0];
        if (first < 0x80)
        {
            return 1;
        }
        uint32_t count = first >= 0xf0 ? 4 : first >= 0xe0 ? 3 : 2;
        if (first < 0xc2 || first > 0xf4 || size < count)
        {
            return 0;
        }
        uint32_t cp = first & (0x7f >> count);
        for (uint32_t i = 1; i < count; ++i)
        {
            unsigned char c = text[i];
            if ((c & 0xc0) != 0x80)
            {
                return 0;
            }
            cp = (cp << 6) | (c & 63);
        }
        if ((count == 3 && cp < 0x800) || (count == 4 && cp < 0x10000) || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
        {
            return 0;
        }
        return count;
    }

    // One bulk call lets the compiler inline sequence validation for mixed
    // blocks, while retaining the existing SIMD ASCII fast path.
    uint32_t Validate(const char* text, uint32_t size)
    {
        for (uint32_t i = 0; i < size;)
        {
            // Check four blocks together to amortize the ASCII reduction.
            if (size - i >= 64)
            {
                dsimd_uint4_t a = SimdOrUint(SimdLoadUint(text + i), SimdLoadUint(text + i + 16));
                dsimd_uint4_t b = SimdOrUint(SimdLoadUint(text + i + 32), SimdLoadUint(text + i + 48));
                dsimd_uint4_t high = SimdAndUint(SimdOrUint(a, b), SimdSplatUint(0x80808080));
                if (SimdAll(SimdEqualUint(high, SimdSplatUint(0))))
                {
                    i += 64;
                    continue;
                }
            }
            uint32_t end = size - i > 16 ? i + 16 : size;
            if (size - i >= 16)
            {
                dsimd_uint4_t bytes = SimdLoadUint(text + i);
                dsimd_uint4_t high = SimdAndUint(bytes, SimdSplatUint(0x80808080));
                if (SimdAll(SimdEqualUint(high, SimdSplatUint(0))))
                {
                    i += 16;
                    continue;
                }
#if DM_ENDIAN == DM_ENDIAN_LITTLE && !DM_SIMD_FORCE_SCALAR && (defined(DM_SIMD_NEON) || defined(DM_SIMD_WASM))
                if (ValidateBlock(bytes))
                {
                    // Leave any incomplete trailing sequence for the next block
                    // or scalar tail. Failure always recovers from a boundary.
                    uint32_t count = (uint8_t)text[i + 15] >= 0xc0 ? 15 :
                                     (uint8_t)text[i + 14] >= 0xe0 ? 14 :
                                     (uint8_t)text[i + 13] >= 0xf0 ? 13 :
                                     16;
                    i += count;
                    continue;
                }
#endif
            }
            // Validate a mixed block once instead of reloading it for every
            // byte. A UTF-8 sequence may extend beyond the block boundary.
            do
            {
                uint32_t bytes = ValidateSequence(text + i, size - i);
                if (!bytes)
                {
                    return i;
                }
                i += bytes;
            } while (i < end);
        }
        return size;
    }

    static const uint32_t offsets[6] = {
        0x00000000UL,
        0x00003080UL,
        0x000e2080UL,
        0x03c82080UL,
        0xfa082080UL,
        0x82082080UL
    };

    uint32_t NextChar(const char** str) {
        uint32_t c = 0;
        int i = 0;
        const char* s = *str;

        // Edge-case for termination, see doc for behavior
        // Not so pretty perhaps
        if (*s == 0) {
            return 0;
        }

        do {
            c <<= 6;
            c += (uint8_t)(*s);
            ++s;
            ++i;
        } while (*s && (*s & 0xc0) == 0x80);

        c -= offsets[i-1];
        *str = s;
        return c;
    }

    uint32_t ToUtf8(uint16_t chr, char* buf)
    {
        if (chr < 0x80) {
            buf[0] = (char) chr;
            return 1;
        } else if (chr < 0x800) {
            buf[0] = (chr >> 6) | 0xc0;
            buf[1] = (chr & 0x3f) | 0x80;
            return 2;
        } else {
            buf[0] = (chr >> 12) | 0xe0;
            buf[1] = ((chr >> 6) & 0x3f) | 0x80;
            buf[2] = (chr & 0x3f) | 0x80;
            return 3;
        }
        return 0;
    }

    uint32_t StrLen(const char* str) {
        int i = 0, j = 0;
        while (str[i]) {
            if ((str[i] & 0xc0) != 0x80) j++;
            i++;
        }
        return j;
    }

}
