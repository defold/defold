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
