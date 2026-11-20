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

#include <dlib/align.h>

#include <stdio.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

struct AlignStruct
{
    char DM_ALIGNED(128) x;
};

char DM_ALIGNED(256) g_Array[] = "foobar";

TEST(dmAlign, Alignment)
{
    ASSERT_EQ(128U, sizeof(AlignStruct));
    ASSERT_EQ(0U, ((uintptr_t) &g_Array[0]) & 255U);
}

TEST(dmAlign, Align)
{
    void* p = (void*) 0xaabb7;

    p = (void*) DM_ALIGN(p, 16);

    ASSERT_EQ(0xaabc0U, (uintptr_t) p);
}

// Verify zero, aligned values and rounding up without advancing an already aligned value.
TEST(dmAlign, Align64)
{
    ASSERT_EQ(UINT64_C(0), DM_ALIGN64(0, 16));
    ASSERT_EQ(UINT64_C(24), DM_ALIGN64(24, 1));
    ASSERT_EQ(UINT64_C(32), DM_ALIGN64(32, 16));
    ASSERT_EQ(UINT64_C(32), DM_ALIGN64(24, 16));
    ASSERT_EQ(UINT64_C(48), DM_ALIGN64(33, 16));
}

// Preserve both carry across the 32-bit boundary and existing high bits on 32-bit targets.
TEST(dmAlign, Align64Beyond32Bits)
{
    ASSERT_EQ(UINT64_C(0x100000000), DM_ALIGN64(UINT32_MAX, 16));
    ASSERT_EQ(UINT64_C(0x100000000), DM_ALIGN64(UINT64_C(0x100000000), 16));
    ASSERT_EQ(UINT64_C(0x100000010), DM_ALIGN64(UINT64_C(0x100000001), 16));
    ASSERT_EQ(UINT64_C(0x123456789abcde10), DM_ALIGN64(UINT64_C(0x123456789abcde01), 16));
}

// Verify the largest representable results and a boundary that itself needs 64 bits.
TEST(dmAlign, Align64Limits)
{
    ASSERT_EQ(UINT64_MAX, DM_ALIGN64(UINT64_MAX, 1));
    ASSERT_EQ(UINT64_MAX - 15, DM_ALIGN64(UINT64_MAX - 15, 16));
    ASSERT_EQ(UINT64_MAX - 15, DM_ALIGN64(UINT64_MAX - 16, 16));
    ASSERT_EQ(UINT64_C(1) << 63, DM_ALIGN64(1, UINT64_C(1) << 63));
}

// Expressions in either argument must keep their grouping inside the macro.
TEST(dmAlign, Align64Expressions)
{
    ASSERT_EQ(UINT64_C(32), DM_ALIGN64(20 + 4, 1 << 4));
    ASSERT_EQ(UINT64_C(32), DM_ALIGN64(23 | 1, 8 + 8));
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
