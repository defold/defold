
#include <stdint.h>
#include <gtest/gtest.h>
#include "dlib/index_pool.h"


const uint32_t pool_size = 32;

TEST(dmIndexPool32, EmptyConstructor)
{
    dmIndexPool32 pool;
    pool.SetCapacity(pool_size);
    EXPECT_EQ((uint32_t) 0, pool.Size());
    EXPECT_EQ(pool_size, pool.Capacity());
    EXPECT_EQ(pool_size, pool.Remaining());
}

TEST(dmIndexPool32, Fill)
{
    dmIndexPool32 pool;
    pool.SetCapacity(pool_size);

    uint32_t val_array[pool_size];
    memset(&val_array[0], 0xffffffff, sizeof(uint32_t)*pool_size);
    for(uint32_t i = 0; i < pool_size; i++)
        val_array[i] = pool.Pop();

    for(uint32_t i = 0; i < pool_size; i++)
        EXPECT_EQ(i, val_array[i]);

    for(uint32_t i = 0; i < pool_size; i++)
        pool.Push(val_array[i]);

    EXPECT_EQ((uint32_t) 0, pool.Size());
    EXPECT_EQ(pool_size, pool.Capacity());
    EXPECT_EQ(pool_size, pool.Remaining());
}

TEST(dmIndexPool32, SimpleTest)
{
    assert(pool_size > 16);

    dmIndexPool32 pool;
    pool.SetCapacity(pool_size);

    uint32_t val_array[pool_size];
    memset(&val_array[0], 0xffffffff, sizeof(uint32_t)*pool_size);
    for(uint32_t i = 0; i < pool_size; i++)
        val_array[i] = pool.Pop();

    uint32_t max_val = 0;
    for(uint32_t i = 0; i < pool_size; i++)
        max_val += val_array[i];

    pool.Push(pool_size/16);
    pool.Push(pool_size/8);
    pool.Push(pool_size/4);
    pool.Push(pool_size/2);
    pool.Pop();
    pool.Pop();
    pool.Pop();
    pool.Pop();

    for(uint32_t i = 0; i < pool_size; i++)
        pool.Push(i);

    for(uint32_t i = 0; i < pool_size; i++)
        val_array[i] = pool.Pop();

    uint32_t max_val_check = 0;
    for(uint32_t i = 0; i < pool_size; i++)
        max_val_check += val_array[i];

    EXPECT_EQ(max_val, max_val_check);
    EXPECT_EQ(pool_size, pool.Size());
    EXPECT_EQ(pool_size, pool.Capacity());
    EXPECT_EQ((uint32_t) 0, pool.Remaining());
}

TEST(dmIndexPool32, Clear)
{
    assert(pool_size > 16);

    dmIndexPool32 pool;
    pool.SetCapacity(pool_size);

    uint32_t val_array[pool_size];
    memset(&val_array[0], 0xffffffff, sizeof(uint32_t)*pool_size);
    for(uint32_t i = 0; i < pool_size; i++)
    {
        val_array[i] = pool.Pop();
        ASSERT_EQ(i, val_array[i]);
    }

    ASSERT_EQ(0u, pool.Remaining());
    pool.Push(val_array[pool_size/2]);
    pool.Clear();
    ASSERT_EQ(pool_size, pool.Remaining());

    for(uint32_t i = 0; i < pool_size; i++)
    {
        val_array[i] = pool.Pop();
        ASSERT_EQ(i, val_array[i]);
    }
}

struct IterateRemainingContext
{
    static const size_t m_Size = 2;
    uint32_t m_Index;
    uint32_t m_ValArray[m_Size];

    void Reset()
    {
        memset(m_ValArray, 0xff, sizeof(m_ValArray));
        m_Index = 0;
    }
};

template <typename INDEX>
static void IterateRemainingCallback(void* context, const INDEX index)
{
    IterateRemainingContext& ic = *((IterateRemainingContext*) context);
    ic.m_ValArray[ic.m_Index++] = index;
}

TEST(dmIndexPool32, IterateRemaining)
{
    dmIndexPool32 pool;
    pool.SetCapacity(IterateRemainingContext::m_Size);

    IterateRemainingContext ic;
    ic.Reset();
    ASSERT_EQ(0xffffffffu, ic.m_ValArray[0]);
    ASSERT_EQ(0xffffffffu, ic.m_ValArray[1]);

    pool.IterateRemaining(IterateRemainingCallback, (void*) &ic);
    ASSERT_EQ(0u, ic.m_ValArray[0]);
    ASSERT_EQ(1u, ic.m_ValArray[1]);

    pool.Pop();
    ic.Reset();
    pool.IterateRemaining(IterateRemainingCallback, (void*) &ic);
    ASSERT_EQ(1u, ic.m_ValArray[0]);
    ASSERT_EQ(0xffffffffu, ic.m_ValArray[1]);

    pool.Pop();
    ic.Reset();
    pool.IterateRemaining(IterateRemainingCallback, (void*) &ic);
    ASSERT_EQ(0xffffffffu, ic.m_ValArray[0]);
    ASSERT_EQ(0xffffffffu, ic.m_ValArray[1]);

    pool.Push(0);
    pool.Push(1);
    ic.Reset();
    pool.IterateRemaining(IterateRemainingCallback, (void*) &ic);
    ASSERT_EQ(1u, ic.m_ValArray[0]);
    ASSERT_EQ(0u, ic.m_ValArray[1]);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
