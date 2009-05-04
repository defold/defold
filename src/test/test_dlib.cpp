#include <stdint.h>
#include <gtest/gtest.h>
#include "dlib/hash.h"

TEST(dlib, Hash)
{
    uint32_t h1 = HashBuffer32("foo", 3);
    uint64_t h2 = HashBuffer64("foo", 3);

    ASSERT_EQ(0xe18f6896, h1);
    ASSERT_EQ(0x68bf54180bc9f0b5LL, h2);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
