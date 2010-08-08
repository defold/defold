#include <stdint.h>
#include <gtest/gtest.h>
#include "../dlib/hash.h"
#include "../dlib/log.h"

TEST(dlib, Hash)
{
    uint32_t h1 = dmHashBuffer32("foo", 3);
    uint64_t h2 = dmHashBuffer64("foo", 3);

    ASSERT_EQ(0xe18f6896, h1);
    ASSERT_EQ(0xe18f6896a8365dfbLL, h2);
}

TEST(dlib, Log)
{
    dmLogWarning("Test warning message. Should have domain DLIB");
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
