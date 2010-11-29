#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <gtest/gtest.h>
#include "../dlib/atomic.h"
#include "../dlib/thread.h"
#include "../dlib/log.h"

TEST(atomic, Increment)
{
    uint32_atomic_t x = 0;
    ASSERT_EQ(0U, x);
    ASSERT_EQ(0U, dmAtomicIncrement32(&x));
    ASSERT_EQ(1U, dmAtomicIncrement32(&x));
}

TEST(atomic, Decrement)
{
    uint32_atomic_t x = 10;
    ASSERT_EQ(10U, dmAtomicDecrement32(&x));
    ASSERT_EQ(9U, dmAtomicDecrement32(&x));
}

TEST(atomic, Add)
{
    uint32_atomic_t x = 0;
    ASSERT_EQ(0U, x);
    ASSERT_EQ(0U, dmAtomicAdd32(&x, 10));
    ASSERT_EQ(10U, dmAtomicAdd32(&x, 10));
}

TEST(atomic, Sub)
{
    uint32_atomic_t x = 10;
    ASSERT_EQ(10U, dmAtomicSub32(&x, 2));
    ASSERT_EQ(8U, dmAtomicSub32(&x, 2));
}

TEST(atomic, Store)
{
    uint32_atomic_t x = 10;
    ASSERT_EQ(10U, dmAtomicStore32(&x, 2));
    ASSERT_EQ(2U, dmAtomicStore32(&x, 123));
    ASSERT_EQ(123U, x);
}

TEST(atomic, CompareStore)
{
    uint32_atomic_t x = 10;
    // Nop, (123 != 10)
    ASSERT_EQ(10U, dmAtomicCompareStore32(&x, 123, 123));
    ASSERT_EQ(10U, x);
    // Return old value but set new (10 == 10)
    ASSERT_EQ(10U, dmAtomicCompareStore32(&x, 123U, 10U));
    ASSERT_EQ(123U, x);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

