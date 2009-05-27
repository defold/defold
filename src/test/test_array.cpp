#include <stdint.h>
#include <gtest/gtest.h>
#include "dlib/array.h"


TEST(Array, Dynamic32)
{
    Array<int32_t> ar;
    ar.SetCapacity(32);

    for(uint32_t i = 0; i < 32; i++)
        ar.PushBack(i);

    ar.EraseSwap(25);

    EXPECT_EQ(31, ar[25]);
    EXPECT_EQ(31, ar.Size());

    ar.SetCapacity(0);
    EXPECT_EQ(0, ar.Size());
}


TEST(Array, UserAllocated32)
{
    int32_t data[32];
    Array<int32_t> ar(data, 0, 32);

    for(uint32_t i = 0; i < 32; i++)
        ar.PushBack(i);

    ar.EraseSwap(25);

    EXPECT_EQ(31, ar[25]);
    EXPECT_EQ(31, ar.Size());

    ar.SetSize(0);
    EXPECT_EQ(0, ar.Size());
}


int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
