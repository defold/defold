
#include <stdint.h>
#include <gtest/gtest.h>
#include "dlib/circular_array.h"


const uint32_t array_size = 32;
const uint32_t array_size_test_offset = 7;

TEST(dmCircularArray, Dynamic_int32)
{
    assert(array_size > array_size_test_offset);

    dmCircularArray<uint32_t> ar;
    ar.SetCapacity(array_size);
    for(uint32_t i = 0; i < array_size; i++)
        ar.Push(i);

    ar.EraseSwap(array_size-array_size_test_offset);
    EXPECT_EQ(array_size-1, ar[array_size-array_size_test_offset]);

    EXPECT_EQ(array_size-1, ar.Size());
    EXPECT_EQ(array_size, ar.Capacity());

    ar.SetCapacity(array_size-1);
    EXPECT_EQ(array_size-1, ar.Size());
    EXPECT_EQ(array_size-1, ar.Capacity());

    ar.Pop();
    EXPECT_EQ(array_size-3, ar[ ar.Size()-1]);
    EXPECT_EQ(array_size-2, ar.Size());
    EXPECT_EQ(false, ar.Full());
    EXPECT_EQ((uint32_t) 1, ar.Remaining());

    ar.OffsetCapacity(1);
    EXPECT_EQ(array_size, ar.Capacity());
    ar.OffsetCapacity(-1);
    EXPECT_EQ(array_size-1, ar.Capacity());
    EXPECT_EQ(array_size-1, ar[array_size-array_size_test_offset]);

    ar.SetSize(array_size/2);
    EXPECT_EQ(array_size/2, ar.Size());
    ar.SetCapacity(array_size);
    EXPECT_EQ(array_size, ar.Capacity());
    EXPECT_EQ((array_size/2)-1, ar[(array_size/2)-1]);

    const uint32_t const_ref_front = ar[0];
    EXPECT_EQ((uint32_t) 0, const_ref_front);
    const uint32_t const_ref_back = ar[ar.Size()-1];
    EXPECT_EQ((array_size/2)-1, const_ref_back);

    uint32_t ref_front = ar[0];
    EXPECT_EQ((uint32_t) 0, ref_front);
    uint32_t ref_back = ar[ar.Size()-1];
    EXPECT_EQ((array_size/2)-1, ref_back);

    EXPECT_EQ((uint32_t) 0, ar.Front());
    EXPECT_EQ((array_size/2)-1, ar.Back());
    EXPECT_EQ(const_ref_front, ar.Front());
    EXPECT_EQ(const_ref_back, ar.Back());

    uint32_t &ref_adr = ar[0];
    ar.EraseSwapRef(ref_adr);
    EXPECT_EQ((array_size/2)-1, ar[0]);

    ar.SetCapacity(0);
    EXPECT_EQ((uint32_t) 0, ar.Size());
    EXPECT_EQ(true, ar.Empty());
}


TEST(dmCircularArray, Static_int32)
{
    assert(array_size > array_size_test_offset);

    uint32_t array_data[array_size];
    dmCircularArray<uint32_t> ar(array_data, 0, array_size);
    for(uint32_t i = 0; i < array_size; i++)
        ar.Push(i);

    ar.EraseSwap(array_size-array_size_test_offset);
    EXPECT_EQ(array_size-1, ar[array_size-array_size_test_offset]);

    EXPECT_EQ(array_size-1, ar.Size());
    EXPECT_EQ(array_size, ar.Capacity());

    ar.Pop();
    EXPECT_EQ(array_size-3, ar[ ar.Size()-1]);
    EXPECT_EQ(array_size-2, ar.Size());
    EXPECT_EQ(false, ar.Full());
    EXPECT_EQ((uint32_t) 2, ar.Remaining());

    ar.SetSize(array_size/2);
    EXPECT_EQ(array_size/2, ar.Size());

    const uint32_t const_ref_front = ar[0];
    EXPECT_EQ((uint32_t) 0, const_ref_front);
    const uint32_t const_ref_back = ar[ar.Size()-1];
    EXPECT_EQ((array_size/2)-1, const_ref_back);

    uint32_t ref_front = ar[0];
    EXPECT_EQ((uint32_t) 0, ref_front);
    uint32_t ref_back = ar[ar.Size()-1];
    EXPECT_EQ((array_size/2)-1, ref_back);

    EXPECT_EQ((uint32_t) 0, ar.Front());
    EXPECT_EQ((array_size/2)-1, ar.Back());
    EXPECT_EQ(const_ref_front, ar.Front());
    EXPECT_EQ(const_ref_back, ar.Back());

    uint32_t &ref_adr = ar[0];
    ar.EraseSwapRef(ref_adr);
    EXPECT_EQ((array_size/2)-1, ar[0]);

    ar.SetSize(0);
    EXPECT_EQ(true, ar.Empty());
}

TEST(dmCircularArray, Overflow)
{
    dmCircularArray<uint32_t> ar;
    ar.SetCapacity(array_size);
    for(uint32_t i = 0; i < array_size * 2; i++)
        ar.Push(i);

    for (uint32_t i = array_size; i < array_size * 2; ++i)
        EXPECT_EQ(i, ar[i - array_size]);

    ar.SetCapacity(array_size/2);

    for (uint32_t i = array_size; i < array_size * 3 / 2; ++i)
        EXPECT_EQ(i, ar[i - array_size]);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
