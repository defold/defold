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
#include <stdlib.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../dlib/atomic.h"
#include "../dlib/thread.h"
#include "../dlib/log.h"

TEST(atomic, Increment)
{
    int32_atomic_t x = 0;
    ASSERT_EQ(0, x);
    ASSERT_EQ(0, dmAtomicIncrement32(&x));
    ASSERT_EQ(1, dmAtomicIncrement32(&x));
}

TEST(atomic, Decrement)
{
    int32_atomic_t x = 10;
    ASSERT_EQ(10, dmAtomicDecrement32(&x));
    ASSERT_EQ(9, dmAtomicDecrement32(&x));
}

TEST(atomic, Add)
{
    int32_atomic_t x = 0;
    ASSERT_EQ(0, x);
    ASSERT_EQ(0, dmAtomicAdd32(&x, 10));
    ASSERT_EQ(10, dmAtomicAdd32(&x, 10));
}

TEST(atomic, Sub)
{
    int32_atomic_t x = 10;
    ASSERT_EQ(10, dmAtomicSub32(&x, 2));
    ASSERT_EQ(8, dmAtomicSub32(&x, 2));
}

TEST(atomic, Store)
{
    int32_atomic_t x = 10;
    ASSERT_EQ(10, dmAtomicStore32(&x, 2));
    ASSERT_EQ(2, dmAtomicStore32(&x, 123));
    ASSERT_EQ(123, x);
}

TEST(atomic, CompareStore)
{
    int32_atomic_t x = 10;
    // Nop, (123 != 10)
    ASSERT_EQ(10, dmAtomicCompareStore32(&x, 123, 123));
    ASSERT_EQ(10, x);
    // Return old value but set new (10 == 10)
    ASSERT_EQ(10, dmAtomicCompareStore32(&x, 123, 10));
    ASSERT_EQ(123, x);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}

