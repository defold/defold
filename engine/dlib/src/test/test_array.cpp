// Copyright 2020-2024 The Defold Foundation
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

#include "dlib/array.h"

#include <stdint.h>
#include <stdlib.h> // rand
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

const uint32_t array_size = 32;
const uint32_t array_size_test_offset = 7;

TEST(dmArray, Dynamic_int32)
{
    assert(array_size > array_size_test_offset);

    dmArray<uint32_t> ar;
    ar.SetCapacity(array_size);
    for(uint32_t i = 0; i < array_size; i++)
        ar.Push(i);

    ar.EraseSwap(array_size-array_size_test_offset);
    EXPECT_EQ(array_size-1, ar[array_size-array_size_test_offset]);

    EXPECT_EQ(array_size-1, ar.Size());
    EXPECT_EQ(array_size, ar.Capacity());

    ar.SetCapacity(array_size-1);
    EXPECT_EQ(array_size-1, ar.Size());
    EXPECT_EQ(array_size-1, ar.Capacity());

    ar.Pop();
    EXPECT_EQ(array_size-3, ar[ ar.Size()-1]);
    EXPECT_EQ(array_size-2, ar.Size());
    EXPECT_FALSE(ar.Full());
    EXPECT_EQ((uint32_t) 1, ar.Remaining());

    ar.OffsetCapacity(1);
    EXPECT_EQ(array_size, ar.Capacity());
    ar.OffsetCapacity(-1);
    EXPECT_EQ(array_size-1, ar.Capacity());
    EXPECT_EQ(array_size-1, ar[array_size-array_size_test_offset]);

    ar.SetSize(array_size/2);
    EXPECT_EQ(array_size/2, ar.Size());
    ar.SetCapacity(array_size);
    EXPECT_EQ(array_size, ar.Capacity());
    EXPECT_EQ((array_size/2)-1, ar[(array_size/2)-1]);

    const uint32_t const_ref_front = ar[0];
    EXPECT_EQ((uint32_t) 0, const_ref_front);
    const uint32_t const_ref_back = ar[ar.Size()-1];
    EXPECT_EQ((array_size/2)-1, const_ref_back);

    uint32_t ref_front = ar[0];
    EXPECT_EQ((uint32_t) 0, ref_front);
    uint32_t ref_back = ar[ar.Size()-1];
    EXPECT_EQ((array_size/2)-1, ref_back);

    EXPECT_EQ((uint32_t) 0, ar.Front());
    EXPECT_EQ((array_size/2)-1, ar.Back());
    EXPECT_EQ(const_ref_front, ar.Front());
    EXPECT_EQ(const_ref_back, ar.Back());

    uint32_t &ref_adr = ar[0];
    ar.EraseSwapRef(ref_adr);
    EXPECT_EQ((array_size/2)-1, ar[0]);

    ar.SetCapacity(0);
    EXPECT_EQ((uint32_t) 0, ar.Size());
    EXPECT_EQ(true, ar.Empty());
}

TEST(dmArray, Static_int32)
{
    assert(array_size > array_size_test_offset);

    uint32_t array_data[array_size];
    dmArray<uint32_t> ar(array_data, 0, array_size);
    for(uint32_t i = 0; i < array_size; i++)
        ar.Push(i);

    ar.EraseSwap(array_size-array_size_test_offset);
    EXPECT_EQ(array_size-1, ar[array_size-array_size_test_offset]);

    EXPECT_EQ(array_size-1, ar.Size());
    EXPECT_EQ(array_size, ar.Capacity());

    ar.Pop();
    EXPECT_EQ(array_size-3, ar[ ar.Size()-1]);
    EXPECT_EQ(array_size-2, ar.Size());
    EXPECT_FALSE(ar.Full());
    EXPECT_EQ((uint32_t) 2, ar.Remaining());

    ar.SetSize(array_size/2);
    EXPECT_EQ(array_size/2, ar.Size());

    const uint32_t const_ref_front = ar[0];
    EXPECT_EQ((uint32_t) 0, const_ref_front);
    const uint32_t const_ref_back = ar[ar.Size()-1];
    EXPECT_EQ((array_size/2)-1, const_ref_back);

    uint32_t ref_front = ar[0];
    EXPECT_EQ((uint32_t) 0, ref_front);
    uint32_t ref_back = ar[ar.Size()-1];
    EXPECT_EQ((array_size/2)-1, ref_back);

    EXPECT_EQ((uint32_t) 0, ar.Front());
    EXPECT_EQ((array_size/2)-1, ar.Back());
    EXPECT_EQ(const_ref_front, ar.Front());
    EXPECT_EQ(const_ref_back, ar.Back());

    uint32_t &ref_adr = ar[0];
    ar.EraseSwapRef(ref_adr);
    EXPECT_EQ((array_size/2)-1, ar[0]);

    ar.SetSize(0);
    EXPECT_EQ(true, ar.Empty());
}

TEST(dmArray, PushArray)
{
    const uint32_t n = 512;
    uint32_t tmp[n];
    for (uint32_t i = 0; i < n; ++i)
    {
        tmp[i] = i;
    }

    dmArray<uint32_t> a;
    a.SetCapacity(16);
    a.Push(1111);
    a.PushArray(tmp, 15);
    ASSERT_EQ((uint32_t) 16, a.Capacity());
    ASSERT_EQ((uint32_t) 16, a.Size());
    ASSERT_EQ((uint32_t) 0, a.Remaining());

    for (uint32_t i = 1; i < 16; ++i)
    {
        ASSERT_EQ(i-1, a[i]);
    }
}

TEST(dmArray, Swap)
{
    dmArray<uint32_t> a, cpy_a, b, cpy_b;
    uint32_t size_a = (uint32_t)(rand() % 20 + 20);
    a.SetCapacity(size_a);
    cpy_a.SetCapacity(size_a);
    for (uint32_t i = 0; i < size_a; ++i)
    {
        a.Push(i);
        cpy_a.Push(i);
    }
    uint32_t size_b = (uint32_t)(rand() % 20 + 40);
    b.SetCapacity(size_b);
    cpy_b.SetCapacity(size_b);
    for (uint32_t i = 0; i < size_b; ++i)
    {
        b.Push(size_b - i);
        cpy_b.Push(size_b - i);
    }

    ASSERT_NE(size_a, size_b);

    a.Swap(b);

    ASSERT_EQ(a.Capacity(), cpy_b.Capacity());
    ASSERT_EQ(a.Size(), cpy_b.Size());
    for (uint32_t i = 0; i < a.Size(); ++i)
        ASSERT_EQ(a[i], cpy_b[i]);

    ASSERT_EQ(b.Capacity(), cpy_a.Capacity());
    ASSERT_EQ(b.Size(), cpy_a.Size());
    for (uint32_t i = 0; i < b.Size(); ++i)
        ASSERT_EQ(b[i], cpy_a[i]);
}

TEST(dmArray, UserAllocated)
{
    uint32_t array[2];
    array[0] = 1;
    dmArray<uint32_t> a(array, 1, 1);
    ASSERT_EQ(1u, a[0]);
    array[0] = 2;
    dmArray<uint32_t> b(array, 1, 2);
    ASSERT_EQ(2u, b[0]);
    ASSERT_DEATH(dmArray<uint32_t> c(array, 2, 1), "");
}

static inline void sum_int(int* value, void* _sum)
{
    int* sum = (int*)_sum;
    *sum += *value;
}

TEST(dmArray, Map)
{
    const uint32_t size = 10;
    dmArray<int> a;
    a.SetCapacity(size);
    a.SetSize(size);
    uint32_t expected_sum = 0;
    for ( int i = 0; i < size; ++i)
    {
        expected_sum += i;
        a[i] = i;
    }

    uint32_t sum = 0;
    a.Map(sum_int, (void*)&sum);

    ASSERT_EQ(expected_sum, sum);
}

TEST(Macros, ArraySize)
{
    char a_c1[5] = {};
    char a_c2[7] = {};
    ASSERT_EQ(5u, DM_ARRAY_SIZE(a_c1));
    ASSERT_EQ(7u, DM_ARRAY_SIZE(a_c2));

    uint64_t a_u64_1[21] = {};
    uint64_t a_u64_2[17] = {};
    ASSERT_EQ(21u, DM_ARRAY_SIZE(a_u64_1));
    ASSERT_EQ(17u, DM_ARRAY_SIZE(a_u64_2));
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
