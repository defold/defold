#include <stdint.h>
#include <stdlib.h>
#include <string>
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
    return JC_TEST_RUN_ALL();
}

