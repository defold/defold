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
#include <dmsdk/dlib/endian.h>

#define TEST_CHECK(_EXPR) do { if (!(_EXPR)) { fprintf(stderr, "TEST_CHECK failed at line %s:%d: %s\n", __FILE__, __LINE__, #_EXPR); return __LINE__; } } while (0)

int dmEndianCTest(void)
{
    TEST_CHECK(EndianToHost16(EndianToNetwork16((uint16_t)0x1234U)) == (uint16_t)0x1234U);
    TEST_CHECK(EndianToHost32(EndianToNetwork32((uint32_t)0x12345678U)) == (uint32_t)0x12345678U);
    TEST_CHECK(EndianToHost64(EndianToNetwork64((uint64_t)0x123456789abcdef0ULL)) == (uint64_t)0x123456789abcdef0ULL);

    TEST_CHECK(EndianSwap16((uint16_t)0x1234U) == (uint16_t)0x3412U);
    TEST_CHECK(EndianSwap32((uint32_t)0x12345678U) == (uint32_t)0x78563412U);
    TEST_CHECK(EndianSwap64((uint64_t)0x123456789abcdef0ULL) == (uint64_t)0xf0debc9a78563412ULL);

#if DM_ENDIAN == DM_ENDIAN_LITTLE
    TEST_CHECK(EndianToNetwork16((uint16_t)0x1234U) == (uint16_t)0x3412U);
    TEST_CHECK(EndianToNetwork32((uint32_t)0x12345678U) == (uint32_t)0x78563412U);
    TEST_CHECK(EndianToNetwork64((uint64_t)0x123456789abcdef0ULL) == (uint64_t)0xf0debc9a78563412ULL);
#elif DM_ENDIAN == DM_ENDIAN_BIG
    TEST_CHECK(EndianToNetwork16((uint16_t)0x1234U) == (uint16_t)0x1234U);
    TEST_CHECK(EndianToNetwork32((uint32_t)0x12345678U) == (uint32_t)0x12345678U);
    TEST_CHECK(EndianToNetwork64((uint64_t)0x123456789abcdef0ULL) == (uint64_t)0x123456789abcdef0ULL);
#endif

    return 0;
}
