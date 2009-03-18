#include <stdint.h>
#include <time.h>

#include <string>
#include <map>

#include <gtest/gtest.h>

#include "dlib/hashtable.h"

TEST(HashTable, EmtpyConstructor)
{
    THashTable32<int> ht;

    EXPECT_EQ(0, ht.Size());
    EXPECT_EQ(0, ht.Capacity());
    EXPECT_EQ(true, ht.Full());
    EXPECT_EQ(true, ht.Empty());
}

TEST(HashTable, SimplePut)
{
    THashTable<uint32_t, uint32_t> ht(10);
    ht.SetCapacity(10);
    ht.Put(12, 23);

    uint32_t* val = ht.Get(12);
    ASSERT_NE((uintptr_t) 0, (uintptr_t) val);
    EXPECT_EQ((uint32_t) 23, *val);
}

TEST(HashTable, SimpleErase)
{
    for (int table_size = 1; table_size <= 10; ++table_size)
    {
        THashTable<uint32_t, uint32_t> ht(table_size);
        ht.SetCapacity(2);
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

TEST(HashTable, FillEraseFill)
{
    THashTable<uint32_t, uint32_t> ht(10);
    ht.SetCapacity(2);
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

TEST(HashTable, SimpleFill)
{
    const int N = 10;
    for (int count = 0; count < N; ++count)
    {
        for (int table_size = 1; table_size <= 2*N; ++table_size)
        {
            THashTable<uint32_t, uint32_t> ht(table_size);

            ASSERT_TRUE(ht.Empty());
            ht.SetCapacity(count);
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

TEST(HashTable, Exhaustive1)
{
    const int N = 10;
    for (int count = 1; count < N; ++count)
    {
        for (int table_size = 1; table_size <= 2*N; ++table_size)
        {
            std::map<uint32_t, uint32_t> map;
            THashTable<uint32_t, uint32_t> ht(table_size);
            ht.SetCapacity(count);

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
            ASSERT_EQ(0, ht.Size());

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

TEST(HashTable, Performance)
{
    const int N = 0xffff-1;

    std::map<uint32_t, uint32_t> map;
    THashTable<uint32_t, uint32_t> ht((N*2)/3);
    ht.SetCapacity(N);

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

    // std::map at least 5 times slower... :-)
    EXPECT_GE(map_diff, ht_diff*5);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
