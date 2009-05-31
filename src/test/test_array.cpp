
#include <stdint.h>
#include <gtest/gtest.h>
#include "dlib/array.h"


const uint32_t array_size = 32;
const uint32_t array_size_test_offset = 7;

TEST(Array, Dynamic_int32)
{
    assert(array_size > array_size_test_offset);

    Array<int32_t> ar;
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
    EXPECT_EQ(1, ar.Remaining());

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
    EXPECT_EQ(0, const_ref_front);
    const uint32_t const_ref_back = ar[ar.Size()-1];
    EXPECT_EQ((array_size/2)-1, const_ref_back);
    
    uint32_t ref_front = ar[0];
    EXPECT_EQ(0, ref_front);
    uint32_t ref_back = ar[ar.Size()-1];
    EXPECT_EQ((array_size/2)-1, ref_back);

    EXPECT_EQ(0, ar.Front());
    EXPECT_EQ((array_size/2)-1, ar.Back());
    EXPECT_EQ(const_ref_front, ar.Front());
    EXPECT_EQ(const_ref_back, ar.Back());

    int32_t &ref_adr = ar[0];
    ar.EraseSwapRef(ref_adr);
    EXPECT_EQ((array_size/2)-1, ar[0]);

    ar.SetCapacity(0);
    EXPECT_EQ(0, ar.Size());
    EXPECT_EQ(true, ar.Empty());
}


TEST(Array, Static_int32)
{
    assert(array_size > array_size_test_offset);

    int32_t array_data[array_size];
    Array<int32_t> ar(array_data, 0, array_size);
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
    EXPECT_EQ(2, ar.Remaining());

    ar.SetSize(array_size/2);
    EXPECT_EQ(array_size/2, ar.Size());

    const uint32_t const_ref_front = ar[0];
    EXPECT_EQ(0, const_ref_front);
    const uint32_t const_ref_back = ar[ar.Size()-1];
    EXPECT_EQ((array_size/2)-1, const_ref_back);
    
    uint32_t ref_front = ar[0];
    EXPECT_EQ(0, ref_front);
    uint32_t ref_back = ar[ar.Size()-1];
    EXPECT_EQ((array_size/2)-1, ref_back);

    EXPECT_EQ(0, ar.Front());
    EXPECT_EQ((array_size/2)-1, ar.Back());
    EXPECT_EQ(const_ref_front, ar.Front());
    EXPECT_EQ(const_ref_back, ar.Back());

    int32_t &ref_adr = ar[0];
    ar.EraseSwapRef(ref_adr);
    EXPECT_EQ((array_size/2)-1, ar[0]);

    ar.SetSize(0);
    EXPECT_EQ(true, ar.Empty());
}


int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
