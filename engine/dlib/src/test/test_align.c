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

#include <dmsdk/dlib/align.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

// File-scope initializers require C constant expressions and a self-contained SDK header.
static const uint64_t g_AlignedSize = DM_ALIGN64(UINT32_MAX, 16);
static const uint64_t g_AlignedLargeSize = DM_ALIGN64(UINT64_C(0x100000001), 1 << 4);

// Verify the public macro is usable from C without truncating sizes above 32 bits.
TEST(dmAlignC, Align64ConstantExpressions)
{
    ASSERT_EQ(UINT64_C(0x100000000), g_AlignedSize);
    ASSERT_EQ(UINT64_C(0x100000010), g_AlignedLargeSize);
    ASSERT_EQ(UINT64_MAX, DM_ALIGN64(UINT64_MAX, 1));
}

// Keep the existing pointer alignment macro usable from the C SDK header.
TEST(dmAlignC, AlignPointer)
{
    void* p = (void*)(uintptr_t)0xaabb7;
    p = (void*)DM_ALIGN(p, 16);
    ASSERT_EQ((uintptr_t)0xaabc0, (uintptr_t)p);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
