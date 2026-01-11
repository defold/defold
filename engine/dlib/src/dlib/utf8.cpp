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

/**
 * utf-8 support
 *
 * It's assumed that the supplied utf-8 sequences are well-formed and
 * invalid sequences are not detected. It's however guaranteed that
 * the library function will not read outside the buffer and the terminating
 * '\0' character is always respected.
 */
namespace dmUtf8
{
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
