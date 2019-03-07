#include <gtest/gtest.h>

#include <stdlib.h>

#include "../block_allocator.h"

TEST(BlockAllocator, CreateDestroy)
{
    dmBlockAllocator::HContext context = dmBlockAllocator::CreateContext();
    ASSERT_NE((dmBlockAllocator::HContext)0, context);
    dmBlockAllocator::DeleteContext(context);
}

TEST(BlockAllocator, TestReuse)
{
    dmBlockAllocator::HContext context = dmBlockAllocator::CreateContext();
    ASSERT_NE((dmBlockAllocator::HContext)0, context);
    uintptr_t ptrs[16];
    for (uint32_t i = 0; i < 16; ++i)
    {
        ptrs[i] = (uintptr_t)dmBlockAllocator::Allocate(context, 32);
    }
    dmBlockAllocator::Free(context, (void*)ptrs[0], 32);
    uintptr_t reuse_first = (uintptr_t)dmBlockAllocator::Allocate(context, 32);
    ASSERT_EQ(ptrs[0], reuse_first);

    dmBlockAllocator::Free(context, (void*)ptrs[15], 32);
    uintptr_t reuse_last = (uintptr_t)dmBlockAllocator::Allocate(context, 32);
    ASSERT_EQ(ptrs[15], reuse_last);

    dmBlockAllocator::Free(context, (void*)ptrs[7], 32);
    uintptr_t cant_reuse = (uintptr_t)dmBlockAllocator::Allocate(context, 32);
    ASSERT_NE(ptrs[7], cant_reuse);
    ptrs[7] = cant_reuse;

    for (uint32_t i = 0; i < 16; ++i)
    {
        dmBlockAllocator::Free(context, (void*)ptrs[0], 32);
    }

    dmBlockAllocator::DeleteContext(context);
}

TEST(BlockAllocator, TestLargeBlock)
{
    dmBlockAllocator::HContext context = dmBlockAllocator::CreateContext();
    void* ptr = dmBlockAllocator::Allocate(context, 1024 * 1024);
    // It should be allocated outside of block allocator so we should be able to free it
    // Do some magic so we get the correct pointer
    ptr = &((uint16_t*)ptr)[-1];
    free(ptr);

    // We should be fine with deleting the context since it does not really
    // have any allocations of its own
    dmBlockAllocator::DeleteContext(context);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
