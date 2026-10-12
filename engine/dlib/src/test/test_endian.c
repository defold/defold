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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <dmsdk/dlib/endian.h>

TEST(Endian, Conversion)
{
    ASSERT_EQ((uint16_t)0x1234U, EndianToHost16(EndianToNetwork16((uint16_t)0x1234U)));
    ASSERT_EQ((uint32_t)0x12345678U, EndianToHost32(EndianToNetwork32((uint32_t)0x12345678U)));
    ASSERT_EQ((uint64_t)0x123456789abcdef0ULL, EndianToHost64(EndianToNetwork64((uint64_t)0x123456789abcdef0ULL)));

    ASSERT_EQ((uint16_t)0x3412U, EndianSwap16((uint16_t)0x1234U));
    ASSERT_EQ((uint32_t)0x78563412U, EndianSwap32((uint32_t)0x12345678U));
    ASSERT_EQ((uint64_t)0xf0debc9a78563412ULL, EndianSwap64((uint64_t)0x123456789abcdef0ULL));

#if DM_ENDIAN == DM_ENDIAN_LITTLE
    ASSERT_EQ((uint16_t)0x3412U, EndianToNetwork16((uint16_t)0x1234U));
    ASSERT_EQ((uint32_t)0x78563412U, EndianToNetwork32((uint32_t)0x12345678U));
    ASSERT_EQ((uint64_t)0xf0debc9a78563412ULL, EndianToNetwork64((uint64_t)0x123456789abcdef0ULL));
#elif DM_ENDIAN == DM_ENDIAN_BIG
    ASSERT_EQ((uint16_t)0x1234U, EndianToNetwork16((uint16_t)0x1234U));
    ASSERT_EQ((uint32_t)0x12345678U, EndianToNetwork32((uint32_t)0x12345678U));
    ASSERT_EQ((uint64_t)0x123456789abcdef0ULL, EndianToNetwork64((uint64_t)0x123456789abcdef0ULL));
#endif
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
