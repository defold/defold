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
#include <time.h>

#include <string>
#include <map>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "dlib/hashtable.h"

TEST(dmHashTable, EmtpyConstructor)
{
    dmHashTable32<int> ht;

    EXPECT_EQ(0U, ht.Size());
    EXPECT_EQ(0U, ht.Capacity());
    EXPECT_EQ(true, ht.Full());
    EXPECT_EQ(true, ht.Empty());
}

TEST(dmHashTable, SimplePut)
{
    dmHashTable<uint32_t, uint32_t> ht;
    ht.SetCapacity(10, 10);
    ht.Put(12, 23);

    uint32_t* val = ht.Get(12);
    ASSERT_NE((uintptr_t) 0, (uintptr_t) val);
    EXPECT_EQ((uint32_t) 23, *val);
}

TEST(dmHashTable, SimplePutUserAllocated)
{
    uint8_t *data = new uint8_t[(sizeof(uint16_t)*10) + (sizeof(dmHashTable<uint32_t, uint32_t>::Entry)*10)];
    dmHashTable<uint32_t, uint32_t> ht(data, 10, 10);
    ht.Put(12, 23);

    uint32_t* val = ht.Get(12);
    ASSERT_NE((uintptr_t) 0, (uintptr_t) val);
    EXPECT_EQ((uint32_t) 23, *val);
    delete []data;
}

TEST(dmHashTable, SimpleErase)
{
    for (int table_size = 1; table_size <= 10; ++table_size)
    {
        dmHashTable<uint32_t, uint32_t> ht;
        ht.SetCapacity(table_size, 2);
        ht.Put(1, 10);
        ht.Put(2, 20);

        uint32_t* val;
        val = ht.Get(1);
        ASSERT_NE((uintptr_t) 0, (uintptr_t) val);
        EXPECT_EQ((uint32_t) 10, *val);

        val = ht.Get(2);
        ASSERT_NE((uintptr_t) 0, (uintptr_t) val);
        EXPECT_EQ((uint32_t) 20, *val);

        ht.Verify();
        ht.Erase(1);
        ht.Verify();

        val = ht.Get(2);
        ASSERT_NE((uintptr_t) 0, (uintptr_t) val);
        EXPECT_EQ((uint32_t) 20, *val);

        ht.Erase(2);
        ht.Verify();
    }
}

TEST(dmHashTable, FillEraseFill)
{
    dmHashTable<uint32_t, uint32_t> ht;
    ht.SetCapacity(10, 2);
    ht.Put(1, 10);
    ht.Put(2, 20);
    ASSERT_EQ((uint32_t) 10, *ht.Get(1));
    ASSERT_EQ((uint32_t) 20, *ht.Get(2));

    ht.Verify();
    ht.Erase(1);
    ht.Verify();
    ht.Erase(2);
    ht.Verify();
    ASSERT_EQ((uintptr_t) 0, (uintptr_t) ht.Get(1));
    ASSERT_EQ((uintptr_t) 0, (uintptr_t) ht.Get(2));

    ht.Verify();
    ht.Put(1, 100);
    ht.Verify();
    ht.Put(2, 200);
    ht.Verify();
    ASSERT_EQ((uint32_t) 100, *ht.Get(1));
    ASSERT_EQ((uint32_t) 200, *ht.Get(2));
}

TEST(dmHashTable, SimpleFill)
{
    const int N = 50;
    for (int count = 0; count < N; ++count)
    {
        for (int table_size = 1; table_size <= 2*N; ++table_size)
        {
            dmHashTable<uint32_t, uint32_t> ht;

            ASSERT_TRUE(ht.Empty());
            ht.SetCapacity(table_size, count);
            ASSERT_TRUE(ht.Empty());

            for (int j = 0; j < count; ++j)
            {
                ht.Put(j, j * 10);
            }

            ASSERT_TRUE(ht.Full());

            for (int j = 0; j < count; ++j)
            {
                uint32_t* v = ht.Get(j);
                ASSERT_TRUE(v != 0);
                ASSERT_EQ((uint32_t) j*10, *v);
            }
        }
    }
}

TEST(dmHashTable, Exhaustive1)
{
    const int N = 10;
    for (int count = 1; count < N; ++count)
    {
        for (int table_size = 1; table_size <= 2*N; ++table_size)
        {
            std::map<uint32_t, uint32_t> map;
            dmHashTable<uint32_t, uint32_t> ht;
            ht.SetCapacity(table_size, count);

            ASSERT_TRUE(ht.Empty());
            for (int i = 0; i < count; ++i)
            {
                map[i] = i*10;
                ht.Put(i, i*10);
            }
            ASSERT_TRUE(ht.Full());

            int half = count / 2;
            int j = 0;

            // Erase first half
            for (j = 0; j < half; j++)
            {
                assert(map.find(j) != map.end());
                map.erase(map.find(j));
                ht.Erase(j);
                ht.Verify();
            }

            // Test second half
            for (int j = half; j < count; j++)
            {
                uint32_t*v = ht.Get(j);
                ASSERT_TRUE(v != 0);
                ASSERT_EQ(map[j], *v);
            }

            // Remove second half
            for (int j = half; j < count; j++)
            {
                assert(map.find(j) != map.end());
                map.erase(map.find(j));
                ht.Erase(j);
                ht.Verify();
            }
            ASSERT_EQ(0U, ht.Size());

            // Fill again
            for (int i = 0; i < count; ++i)
            {
                map[i] = i*100;
                ht.Put(i, i*100);
                ht.Verify();
            }

            for (int i = 0; i < count; ++i)
            {
                uint32_t*v = ht.Get(i);
                ASSERT_TRUE(v != 0);
                ASSERT_EQ(map[i], *v);
            }
        }
    }
}

// This was a stupid bug where Put() didn't return in if (entry != 0)...
TEST(dmHashTable, TestBug1)
{
    std::map<uint32_t, uint32_t> map;
    dmHashTable<uint32_t, uint32_t> ht;
    ht.SetCapacity(122, 3);
    ht.Put(487, 0);
    ht.Put(487, 0);
    ASSERT_EQ(1U, ht.Size());
}

TEST(dmHashTable, Exhaustive2)
{
    const int N = 30;
    for (int count = 1; count < N; ++count)
    {
        for (int table_size = 1; table_size <= 2*N; ++table_size)
        {
            std::map<uint32_t, uint32_t> map;
            dmHashTable<uint32_t, uint32_t> ht;
            ht.SetCapacity(table_size, count);

            // Fill table
            while(map.size() < (uint32_t) count)
            {
                uint32_t key = rand() & 0x3ff; // keys up to 1023...
                uint32_t val = rand();
                map[key] = val;
                ht.Put(key, val);
            }

            // Compare and remove
            std::map<uint32_t, uint32_t>::iterator iter;
            for( iter = map.begin(); iter != map.end(); ++iter)
            {
                uint32_t key = iter->first;
                ASSERT_NE((void*) 0, ht.Get(key));
                ASSERT_EQ(iter->second, *ht.Get(key));
                ht.Erase(key);
                ht.Verify();
            }

            map.clear();

            // Fill again but now count/2
            while(map.size() < (uint32_t) count/2)
            {
                uint32_t key = rand() & 0x3ff; // keys up to 1023...
                uint32_t val = rand();
                map[key] = val;
                ht.Put(key, val);
            }

            // Compare again
            for( iter = map.begin(); iter != map.end(); ++iter)
            {
                uint32_t key = iter->first;
                ASSERT_NE((void*) 0, ht.Get(key));
                ASSERT_EQ(iter->second, *ht.Get(key));
                ht.Verify();
            }
        }
    }
}


TEST(dmHashTable, Exhaustive3)
{
    const int N = 20;
    for (int count = 1; count < N; ++count)
    {
        for (int table_size = 1; table_size <= 2*N; ++table_size)
        {
            std::map<uint32_t, uint32_t> map;
            dmHashTable<uint32_t, uint32_t> ht;
            ht.SetCapacity(table_size, count);

            const uint32_t grow_shrink_iter_count = 20;
            for (uint32_t grow_shrink_iter = 0; grow_shrink_iter < grow_shrink_iter_count; ++grow_shrink_iter)
            {
                uint32_t target_size = uint32_t(rand() % (count + 1));
                if (grow_shrink_iter == grow_shrink_iter_count/2)
                {
                    // Fill completely
                    target_size = count;
                }

                while (map.size() != target_size)
                {
                    if (map.size() < target_size)
                    {
                        uint32_t key = rand() & 0x3ff; // keys up to 1023...
                        uint32_t val = rand();

                        map[key] = val;
                        ht.Put(key, val);

                    }
                    else
                    {
                        uint32_t key = map.begin()->first;
                        map.erase(map.begin());
                        ht.Erase(key);
                    }
                    ASSERT_EQ(map.size(), ht.Size());
                    ht.Verify();
                }
            }
            // Compare
            std::map<uint32_t, uint32_t>::iterator iter;
            for( iter = map.begin(); iter != map.end(); ++iter)
            {
                uint32_t key = iter->first;
                ASSERT_NE((void*) 0, ht.Get(key));
                ASSERT_EQ(iter->second, *ht.Get(key));
            }
        }
    }
}

#if 0
// Problems with VirtualBox and performance...
TEST(dmHashTable, Performance)
{
    const int N = 0xffff-1;

    std::map<uint32_t, uint32_t> map;
    dmHashTable<uint32_t, uint32_t> ht;
    ht.SetCapacity((N*2)/3, N);

    clock_t start_map = clock();
    for (int i = 0; i < N; ++i)
    {
        map[i] = i*10;
    }
    clock_t end_map = clock();

    clock_t start_ht = clock();
    for (int i = 0; i < N; ++i)
    {
        ht.Put(i, i*10);
    }
    clock_t end_ht = clock();

    clock_t map_diff = end_map - start_map;
    clock_t ht_diff = end_ht - start_ht;

    // std::map at least 3 times slower... :-)
    EXPECT_GE(map_diff, ht_diff*3);
}
#endif

void IterateCallback(int* context, const uint32_t* key, int* value)
{
    *context += *value;
}

TEST(dmHashTable, Iterate)
{
    for (uint32_t table_size = 1; table_size < 27; ++table_size)
    {
        for (uint32_t capacity = 1; capacity < 100; ++capacity)
        {
            dmHashTable<uint32_t, int> ht;
            ht.SetCapacity(table_size, capacity);

            int sum = 0;

            for (uint32_t i = 0; i < capacity; ++i)
            {
                int x = rand();
                ht.Put(i, x);
                sum += x;
            }
            int context = 0;
            ht.Iterate(IterateCallback, &context);
            ASSERT_EQ(sum, context);
        }
    }
}

TEST(dmHashTable, Iterator)
{
    for (uint32_t table_size = 1; table_size < 27; ++table_size)
    {
        for (uint32_t capacity = 1; capacity < 100; ++capacity)
        {
            dmHashTable<uint32_t, int> ht;
            ht.SetCapacity(table_size, capacity);

            int sum = 0;
            uint32_t key_sum = 0;
            for (uint32_t i = 0; i < capacity; ++i)
            {
                int x = rand();
                ht.Put(i, x);
                sum += x;
                key_sum += i;
            }

            int result = 0;
            int keyresult = 0;
            dmHashTable<uint32_t, int>::Iterator iter = ht.GetIterator();
            while(iter.Next())
            {
                keyresult += iter.GetKey();
                result += iter.GetValue();
            }
            ASSERT_EQ(sum, result);
            ASSERT_EQ(key_sum, keyresult);
        }
    }
}

TEST(dmHashTable, Grow)
{
    dmHashTable<uint32_t, int> ht;
    std::map<uint32_t, int> map;

    for (uint32_t table_size = 1; table_size < 41; ++table_size)
    {
        uint32_t n = rand() % 97;
        for (uint32_t iter = 0; iter < n; ++iter)
        {
            if (ht.Full())
            {
                ht.SetCapacity(table_size, ht.Capacity() + (rand() % 4) + 1);
            }
            uint32_t key = rand();
            int val = rand();
            ht.Put(key, val);
            map[key] = val;
        }

        ASSERT_EQ(map.size(), ht.Size());
        std::map<uint32_t, int>::iterator iter = map.begin();
        for (iter = map.begin(); iter != map.end(); ++iter)
        {
            uint32_t key = iter->first;
            ASSERT_EQ(map[key], *ht.Get(key));
        }
    }
}

uint32_t g_ClearCount;

void ClearCallback(int* context, const uint32_t* key, int* value)
{
    g_ClearCount++;
}

TEST(dmHashTable, Clear)
{
    for (uint32_t table_size = 1; table_size < 41; ++table_size)
    {
        dmHashTable<uint32_t, int> ht;
        std::map<uint32_t, int> map;

        ht.SetCapacity(table_size, table_size * 2);

        for (uint32_t iter = 0; iter < 4; ++iter)
        {
            for (uint32_t i = 0; i < table_size * 2; ++i)
            {
                uint32_t key = (uint32_t) rand();
                int value = rand();
                ht.Put(key, value);
                map[key] = value;
                ASSERT_EQ(map.size(), ht.Size());
            }

            std::map<uint32_t, int>::iterator iter2 = map.begin();
            for (iter2 = map.begin(); iter2 != map.end(); ++iter2)
            {
                uint32_t key = iter2->first;
                ASSERT_EQ(map[key], *ht.Get(key));
            }

            ht.Clear();
            map.clear();

            ht.Iterate(ClearCallback, (int*) 0);
            ASSERT_EQ(0U, g_ClearCount);

        }
    }
}

TEST(dmHashTable, Swap)
{
    dmHashTable<int, int> h1;
    dmHashTable<int, int> h2;
    h1.SetCapacity(37, 10);
    h2.SetCapacity(37, 10);

    h1.Put(1, 10);
    h1.Put(2, 20);
    h1.Put(3, 30);

    h2.Put(10, 100);
    h2.Put(20, 200);
    h2.Put(30, 300);

    h1.Swap(h2);

    ASSERT_EQ(10, *h2.Get(1));
    ASSERT_EQ(20, *h2.Get(2));
    ASSERT_EQ(30, *h2.Get(3));

    ASSERT_EQ(100, *h1.Get(10));
    ASSERT_EQ(200, *h1.Get(20));
    ASSERT_EQ(300, *h1.Get(30));
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
