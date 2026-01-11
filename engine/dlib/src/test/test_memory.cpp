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
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../dlib/memory.h"

TEST(dmMemory, Malloc)
{
    void* memory = malloc(1024*1024);
    ASSERT_TRUE(memory != 0);
    free(memory);//
}

TEST(dmMemory, AlignedMalloc)
{
    void* dummy = 0;
    ASSERT_EQ(dmMemory::RESULT_OK, dmMemory::AlignedMalloc(&dummy, 8, 1024));
    ASSERT_TRUE(dummy != 0);
    ASSERT_EQ(0u, ((unsigned long)dummy % 8));
    dmMemory::AlignedFree(dummy);
    dummy = 0;

    ASSERT_EQ(dmMemory::RESULT_OK, dmMemory::AlignedMalloc(&dummy, 16, 1024));
    ASSERT_TRUE(dummy != 0);
    ASSERT_EQ(0u, ((unsigned long)dummy % 16));
    dmMemory::AlignedFree(dummy);
    dummy = 0;

    ASSERT_EQ(dmMemory::RESULT_OK, dmMemory::AlignedMalloc(&dummy, 1024*16, 1024));
    ASSERT_TRUE(dummy != 0);
    ASSERT_EQ(0u, ((unsigned long)dummy % 1024*16));
    dmMemory::AlignedFree(dummy);
    dummy = 0;

    // Trying to allocate with non-power-of-2 alignment should fail with RESULT_INVAL.
    ASSERT_EQ(dmMemory::RESULT_INVAL, dmMemory::AlignedMalloc(&dummy, 7, 1024));
    dmMemory::AlignedFree(dummy);
    dummy = 0;
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
