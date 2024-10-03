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

#include "dlib/set.h"

#include <stdint.h>
#include <stdlib.h> // rand
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <set>

TEST(dmSet, Constructor)
{
    dmSet<uint32_t> s;
    ASSERT_EQ(0u, s.Capacity());
    ASSERT_EQ(0u, s.Size());
    ASSERT_EQ(true, s.Full());
    ASSERT_EQ(true, s.Empty());
    ASSERT_EQ((uint32_t*)0, s.Begin());
    ASSERT_EQ((uint32_t*)0, s.End());
}

TEST(dmSet, Simple)
{
    dmSet<uint32_t> s;
    s.SetCapacity(1);
    ASSERT_EQ(1u, s.Capacity());
    s.OffsetCapacity(3);
    ASSERT_EQ(4u, s.Capacity());
    ASSERT_EQ(0u, s.Size());
    ASSERT_EQ(false, s.Full());

    for (uint32_t i = 0; i < s.Capacity(); ++i)
    {
        ASSERT_EQ(true, s.Add(10 + i));
        ASSERT_EQ(i+1, s.Size());
    }
    ASSERT_EQ(false, s.Empty());
    ASSERT_EQ(true, s.Full());


    for (uint32_t i = 0; i < s.Capacity(); ++i)
    {
        ASSERT_EQ(true, s.Contains(10 + i));
        ASSERT_EQ(false, s.Contains(i));
    }
}

TEST(dmSet, StressTest)
{
    for (uint32_t i = 0; i < 2; ++i)
    {
        uint32_t cap = i * 1000;

        std::set<uint32_t> scpp;
        dmSet<uint32_t> s;

        s.SetCapacity(cap);

        ASSERT_EQ(cap, s.Capacity());
        ASSERT_EQ(0u, s.Size());
        ASSERT_EQ(true, s.Empty());
        ASSERT_EQ(cap==0, s.Full());

        for (uint32_t k = 0; k < cap; ++k)
        {
            scpp.insert(k);
            s.Add(k);
        }

        // Compare the sets
        ASSERT_EQ(scpp.size(), s.Size());
        for (uint32_t k = 0; k < cap + 100; ++k)
        {
            std::set<uint32_t>::iterator iter = scpp.find(k);
            bool cpp_contains = iter != scpp.end();

            ASSERT_EQ(cpp_contains, s.Contains(k));
        }

        // Remove entries
        for (uint32_t k = 0; k < cap/2; ++k)
        {
            scpp.erase(k);
            s.Remove(k);
        }

        // Compare sets again
        ASSERT_EQ(scpp.size(), s.Size());
        for (uint32_t k = 0; k < cap + 100; ++k)
        {
            std::set<uint32_t>::iterator iter = scpp.find(k);
            bool cpp_contains = iter != scpp.end();

            ASSERT_EQ(cpp_contains, s.Contains(k));
        }

    }


    // s.SetCapacity(1);
    // ASSERT_EQ(1u, s.Capacity());
    // s.OffsetCapacity(3);
    // ASSERT_EQ(4u, s.Capacity());
    // ASSERT_EQ(0u, s.Size());
    // ASSERT_EQ(false, s.Full());

    // for (uint32_t i = 0; i < s.Capacity(); ++i)
    // {
    //     ASSERT_EQ(true, s.Add(10 + i));
    //     ASSERT_EQ(i+1, s.Size());
    // }
    // ASSERT_EQ(false, s.Empty());
    // ASSERT_EQ(true, s.Full());


    // for (uint32_t i = 0; i < s.Capacity(); ++i)
    // {
    //     ASSERT_EQ(true, s.Contains(10 + i));
    //     ASSERT_EQ(false, s.Contains(i));
    // }
}

// TEST(dmSet, int32)
// {
//     assert(array_size > array_size_test_offset);

//     dmArray<uint32_t> ar;
//     ar.SetCapacity(array_size);
//     for(uint32_t i = 0; i < array_size; i++)
//         ar.Push(i);

//     ar.EraseSwap(array_size-array_size_test_offset);
//     EXPECT_EQ(array_size-1, ar[array_size-array_size_test_offset]);

//     EXPECT_EQ(array_size-1, ar.Size());
//     EXPECT_EQ(array_size, ar.Capacity());

//     ar.SetCapacity(array_size-1);
//     EXPECT_EQ(array_size-1, ar.Size());
//     EXPECT_EQ(array_size-1, ar.Capacity());

//     ar.Pop();
//     EXPECT_EQ(array_size-3, ar[ ar.Size()-1]);
//     EXPECT_EQ(array_size-2, ar.Size());
//     EXPECT_FALSE(ar.Full());
//     EXPECT_EQ((uint32_t) 1, ar.Remaining());

//     ar.OffsetCapacity(1);
//     EXPECT_EQ(array_size, ar.Capacity());
//     ar.OffsetCapacity(-1);
//     EXPECT_EQ(array_size-1, ar.Capacity());
//     EXPECT_EQ(array_size-1, ar[array_size-array_size_test_offset]);

//     ar.SetSize(array_size/2);
//     EXPECT_EQ(array_size/2, ar.Size());
//     ar.SetCapacity(array_size);
//     EXPECT_EQ(array_size, ar.Capacity());
//     EXPECT_EQ((array_size/2)-1, ar[(array_size/2)-1]);

//     const uint32_t const_ref_front = ar[0];
//     EXPECT_EQ((uint32_t) 0, const_ref_front);
//     const uint32_t const_ref_back = ar[ar.Size()-1];
//     EXPECT_EQ((array_size/2)-1, const_ref_back);

//     uint32_t ref_front = ar[0];
//     EXPECT_EQ((uint32_t) 0, ref_front);
//     uint32_t ref_back = ar[ar.Size()-1];
//     EXPECT_EQ((array_size/2)-1, ref_back);

//     EXPECT_EQ((uint32_t) 0, ar.Front());
//     EXPECT_EQ((array_size/2)-1, ar.Back());
//     EXPECT_EQ(const_ref_front, ar.Front());
//     EXPECT_EQ(const_ref_back, ar.Back());

//     uint32_t &ref_adr = ar[0];
//     ar.EraseSwapRef(ref_adr);
//     EXPECT_EQ((array_size/2)-1, ar[0]);

//     ar.SetCapacity(0);
//     EXPECT_EQ((uint32_t) 0, ar.Size());
//     EXPECT_EQ(true, ar.Empty());
// }

// TEST(dmSet, Static_int32)
// {
//     assert(array_size > array_size_test_offset);

//     uint32_t array_data[array_size];
//     dmArray<uint32_t> ar(array_data, 0, array_size);
//     for(uint32_t i = 0; i < array_size; i++)
//         ar.Push(i);

//     ar.EraseSwap(array_size-array_size_test_offset);
//     EXPECT_EQ(array_size-1, ar[array_size-array_size_test_offset]);

//     EXPECT_EQ(array_size-1, ar.Size());
//     EXPECT_EQ(array_size, ar.Capacity());

//     ar.Pop();
//     EXPECT_EQ(array_size-3, ar[ ar.Size()-1]);
//     EXPECT_EQ(array_size-2, ar.Size());
//     EXPECT_FALSE(ar.Full());
//     EXPECT_EQ((uint32_t) 2, ar.Remaining());

//     ar.SetSize(array_size/2);
//     EXPECT_EQ(array_size/2, ar.Size());

//     const uint32_t const_ref_front = ar[0];
//     EXPECT_EQ((uint32_t) 0, const_ref_front);
//     const uint32_t const_ref_back = ar[ar.Size()-1];
//     EXPECT_EQ((array_size/2)-1, const_ref_back);

//     uint32_t ref_front = ar[0];
//     EXPECT_EQ((uint32_t) 0, ref_front);
//     uint32_t ref_back = ar[ar.Size()-1];
//     EXPECT_EQ((array_size/2)-1, ref_back);

//     EXPECT_EQ((uint32_t) 0, ar.Front());
//     EXPECT_EQ((array_size/2)-1, ar.Back());
//     EXPECT_EQ(const_ref_front, ar.Front());
//     EXPECT_EQ(const_ref_back, ar.Back());

//     uint32_t &ref_adr = ar[0];
//     ar.EraseSwapRef(ref_adr);
//     EXPECT_EQ((array_size/2)-1, ar[0]);

//     ar.SetSize(0);
//     EXPECT_EQ(true, ar.Empty());
// }

// TEST(dmSet, PushArray)
// {
//     const uint32_t n = 512;
//     uint32_t tmp[n];
//     for (uint32_t i = 0; i < n; ++i)
//     {
//         tmp[i] = i;
//     }

//     dmArray<uint32_t> a;
//     a.SetCapacity(16);
//     a.Push(1111);
//     a.PushArray(tmp, 15);
//     ASSERT_EQ((uint32_t) 16, a.Capacity());
//     ASSERT_EQ((uint32_t) 16, a.Size());
//     ASSERT_EQ((uint32_t) 0, a.Remaining());

//     for (uint32_t i = 1; i < 16; ++i)
//     {
//         ASSERT_EQ(i-1, a[i]);
//     }
// }

// TEST(dmSet, Swap)
// {
//     dmArray<uint32_t> a, cpy_a, b, cpy_b;
//     uint32_t size_a = (uint32_t)(rand() % 20 + 20);
//     a.SetCapacity(size_a);
//     cpy_a.SetCapacity(size_a);
//     for (uint32_t i = 0; i < size_a; ++i)
//     {
//         a.Push(i);
//         cpy_a.Push(i);
//     }
//     uint32_t size_b = (uint32_t)(rand() % 20 + 40);
//     b.SetCapacity(size_b);
//     cpy_b.SetCapacity(size_b);
//     for (uint32_t i = 0; i < size_b; ++i)
//     {
//         b.Push(size_b - i);
//         cpy_b.Push(size_b - i);
//     }

//     ASSERT_NE(size_a, size_b);

//     a.Swap(b);

//     ASSERT_EQ(a.Capacity(), cpy_b.Capacity());
//     ASSERT_EQ(a.Size(), cpy_b.Size());
//     for (uint32_t i = 0; i < a.Size(); ++i)
//         ASSERT_EQ(a[i], cpy_b[i]);

//     ASSERT_EQ(b.Capacity(), cpy_a.Capacity());
//     ASSERT_EQ(b.Size(), cpy_a.Size());
//     for (uint32_t i = 0; i < b.Size(); ++i)
//         ASSERT_EQ(b[i], cpy_a[i]);
// }

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
