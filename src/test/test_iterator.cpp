#include <stdint.h>
#include <gtest/gtest.h>
#include "dlib/iterator.h"


TEST(Iterator, FillArray)
{ 
    const uint32_t list_size = 10;
    uint32_t list[list_size];
    Iterator<uint32_t> begin(&list[0], &list[0], &list[list_size]);
    Iterator<uint32_t> end(&list[list_size], &list[0], &list[list_size]);

    for(uint32_t i = 0; begin != end; begin++, ++i)
        list[i] = i;        
    EXPECT_EQ(list[list_size-1], list_size-1);
    begin -= 2;
    EXPECT_EQ(*begin, list_size-2);
}

TEST(Iterator, TestOperators)
{ 
    const uint32_t list_size = 10;
    uint32_t list[list_size];
    memset(&list[0], 0x0, list_size*sizeof(uint32_t));
    Iterator<uint32_t> begin(&list[0], &list[0], &list[list_size]);
    Iterator<uint32_t> end(&list[list_size], &list[0], &list[list_size]);

    begin ++;
    EXPECT_EQ(true, begin == end-(list_size-1));
    begin --;
    EXPECT_EQ(true, begin == end-list_size);

    begin += 2;
    EXPECT_EQ(true, begin+2 == end-(list_size-4));
    begin -= 2;
    EXPECT_EQ(true, begin+2 == end-(list_size-2));

    EXPECT_EQ(true, begin < end);
    EXPECT_EQ(true, begin <= end);
    EXPECT_EQ(false, begin > end);
    EXPECT_EQ(false, begin >= end);
    EXPECT_EQ(false, begin == end);
    EXPECT_EQ(true, begin != end);
    begin = end;
    EXPECT_EQ(true, begin == end);
}


int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
