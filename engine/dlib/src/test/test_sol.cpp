#include <stdint.h>
#include <stdio.h>
#include <string>
#include <gtest/gtest.h>
#include "../dlib/sol.h"
#include "../dlib/hashtable.h"
#include "../dlib/array.h"

TEST(dmSol, StrDup)
{
    const char *t = "Test string";
    const char *gc = dmSol::StrDup(t);
    ASSERT_NE(t, gc);
    ASSERT_EQ(0, strcmp(gc, t));
    runtime_unpin((void*)gc);
    ASSERT_EQ(0, dmSol::StrDup(0));
}

TEST(dmSol, Handle)
{
    const uint32_t max = 256;
    dmSol::Handle h[max];

    static uint32_t type_a, type_b;

    for (uint32_t k=0;k<2;k++)
    {
        for (uint32_t i=0;i<max;i++)
        {
            h[i] = dmSol::MakeHandle((void*)i, &type_a);
            ASSERT_EQ(true, dmSol::IsHandleValid(h[i], &type_a));
            ASSERT_EQ(false, dmSol::IsHandleValid(h[i], &type_b));
            ASSERT_EQ(((void*)i), dmSol::GetHandle(h[i], &type_a));
        }

        for (uint32_t i=0;i<max;i++)
        {
            dmSol::FreeHandle(h[i]);
            ASSERT_EQ(false, dmSol::IsHandleValid(h[i], &type_a));
            ASSERT_EQ(false, dmSol::IsHandleValid(h[i], &type_b));
        }
    }
}

extern "C"
{
    ::Any test_sol_alloc_item(int which);
}

TEST(dmSol, SizeOf)
{
    struct InteropStruct1Array
    {
        dmArray<uint32_t> array;
    };
    struct InteropStruct2Array
    {
        dmArray<uint32_t> array[2];
    };
    struct InteropStruct1HashTable
    {
        dmHashTable<uint32_t, uint32_t> hash;
    };
    struct InteropStruct2HashTable
    {
        dmHashTable<uint32_t, uint32_t> hash[2];
    };
    struct InteropStruct1Array1HashTable
    {
        dmArray<uint32_t> array;
        dmHashTable<uint32_t, uint32_t> hash;
    };
    ASSERT_EQ(dmSol::SizeOf(test_sol_alloc_item(0)), sizeof(InteropStruct1Array));
    ASSERT_EQ(dmSol::SizeOf(test_sol_alloc_item(1)), sizeof(InteropStruct2Array));
    ASSERT_EQ(dmSol::SizeOf(test_sol_alloc_item(2)), sizeof(InteropStruct1HashTable));
    ASSERT_EQ(dmSol::SizeOf(test_sol_alloc_item(3)), sizeof(InteropStruct2HashTable));
    ASSERT_EQ(dmSol::SizeOf(test_sol_alloc_item(4)), sizeof(InteropStruct1Array1HashTable));
}

int main(int argc, char **argv)
{
    dmSol::Initialize(256);

    testing::InitGoogleTest(&argc, argv);
    int r = RUN_ALL_TESTS();

    dmSol::FinalizeWithCheck();
    return r;
}
