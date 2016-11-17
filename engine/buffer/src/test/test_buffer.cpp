#include <gtest/gtest.h>
#include <dlib/log.h>
#include <dlib/hash.h>

#include "../buffer.h"

#define RIG_EPSILON 0.0001f

class BufferTest : public ::testing::Test
{

};

class GetDataTest : public ::testing::Test
{
public:
    dmBuffer::HBuffer buffer;

protected:
    virtual void SetUp() {
        dmBuffer::StreamDeclaration buffer_decl[] = {
            {dmHashString64("position"), dmBuffer::BUFFER_TYPE_FLOAT32, 3},
            {dmHashString64("texcoord"), dmBuffer::BUFFER_TYPE_UINT16, 2}
        };

        dmBuffer::Allocate(4, buffer_decl, 2, &buffer);
    }

    virtual void TearDown() {
        dmBuffer::Free(buffer);
    }
};

TEST_F(BufferTest, InvalidAllocation)
{
    dmBuffer::HBuffer buffer = 0x0;
    dmBuffer::BufferDeclaration buffer_decl_empty = {0};
    dmBuffer::StreamDeclaration buffer_decl_filled[] = {
        {dmHashString64("dummy1"), dmBuffer::BUFFER_TYPE_UINT8, 3},
        {dmHashString64("dummy2"), dmBuffer::BUFFER_TYPE_UINT8, 1}
    };

    // Empty declaration
    dmBuffer::Result r = dmBuffer::Allocate(0, buffer_decl_empty, 0, &buffer);
    ASSERT_NE(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(0x0, buffer);

    // Valid declaration, zero elements
    r = dmBuffer::Allocate(0, buffer_decl_filled, 2, &buffer);
    ASSERT_NE(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(0x0, buffer);

    // Empty declaration, 1 element
    r = dmBuffer::Allocate(1, buffer_decl_empty, 0, &buffer);
    ASSERT_NE(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(0x0, buffer);
}

TEST_F(BufferTest, ValidAllocation)
{
    dmBuffer::HBuffer buffer = 0x0;
    dmBuffer::StreamDeclaration buffer_decl[] = {
        {dmHashString64("position"), dmBuffer::BUFFER_TYPE_UINT8, 3},
        {dmHashString64("texcoord"), dmBuffer::BUFFER_TYPE_UINT8, 1}
    };

    // Declaration of 2 streams, with 4 elements
    dmBuffer::Result r = dmBuffer::Allocate(4, buffer_decl, 2, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_NE((void*)0x0, buffer);
    dmBuffer::Free(buffer);
    buffer = 0x0;

    // Allocation with only the first stream declaration
    r = dmBuffer::Allocate(4, buffer_decl, 1, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_NE((void*)0x0, buffer);
    dmBuffer::Free(buffer);
    buffer = 0x0;

    // Larger allocation
    r = dmBuffer::Allocate(1024*1024, buffer_decl, 2, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_NE((void*)0x0, buffer);
    dmBuffer::Free(buffer);
}

TEST_F(BufferTest, InvalidGetData)
{
    dmBuffer::HBuffer buffer = 0x0;
    dmBuffer::StreamDeclaration buffer_decl[] = {
        {dmHashString64("position"), dmBuffer::BUFFER_TYPE_FLOAT32, 3},
        {dmHashString64("texcoord"), dmBuffer::BUFFER_TYPE_UINT16, 2}
    };

    void *out_stream = 0x0;
    uint32_t out_stride = 0;
    uint32_t out_element_count = 0;

    dmBuffer::Result r = dmBuffer::Allocate(4, buffer_decl, 2, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);

    // Get unknown stream
    r = dmBuffer::GetStream(buffer, dmHashString64("unknown"), dmBuffer::BUFFER_TYPE_UINT8, 2, &out_stream, &out_stride, &out_element_count);
    ASSERT_EQ(dmBuffer::RESULT_STREAM_DOESNT_EXIST, r);
    ASSERT_EQ(0x0, out_stream);
    ASSERT_EQ(0, out_stride);
    ASSERT_EQ(0, out_element_count);

    // Correct stream, but wrong type
    r = dmBuffer::GetStream(buffer, dmHashString64("position"), dmBuffer::BUFFER_TYPE_UINT16, 3, &out_stream, &out_stride, &out_element_count);
    ASSERT_EQ(dmBuffer::RESULT_STREAM_WRONG_TYPE, r);
    ASSERT_EQ(0x0, out_stream);
    ASSERT_EQ(0, out_stride);
    ASSERT_EQ(0, out_element_count);

    // Correct stream, but wrong value count
    r = dmBuffer::GetStream(buffer, dmHashString64("texcoord"), dmBuffer::BUFFER_TYPE_UINT16, 8, &out_stream, &out_stride, &out_element_count);
    ASSERT_EQ(dmBuffer::RESULT_STREAM_WRONG_COUNT, r);
    ASSERT_EQ(0x0, out_stream);
    ASSERT_EQ(0, out_stride);
    ASSERT_EQ(0, out_element_count);

    dmBuffer::Free(buffer);
}

TEST_F(GetDataTest, ValidGetData)
{
    void *out_stream = 0x0;
    uint32_t out_stride = 0;
    uint32_t out_element_count = 0;

    // Get texcoord stream
    dmBuffer::Result r = dmBuffer::GetStream(buffer, dmHashString64("texcoord"), dmBuffer::BUFFER_TYPE_UINT16, 2, &out_stream, &out_stride, &out_element_count);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_NE((void*)0x0, out_stream);
    ASSERT_EQ(2*sizeof(uint16_t), out_stride);
    ASSERT_EQ(4, out_element_count);

    uint16_t* ptr = (uint16_t*)out_stream;
    for (int i = 0; i < out_element_count; ++i)
    {
        ptr[0] = i;
        ptr[1] = i;
        ptr = (uint16_t*)((uintptr_t)ptr + out_stride);
    }

    // Get stream again
    r = dmBuffer::GetStream(buffer, dmHashString64("texcoord"), dmBuffer::BUFFER_TYPE_UINT16, 2, &out_stream, &out_stride, &out_element_count);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_NE((void*)0x0, out_stream);
    ASSERT_EQ(2*sizeof(uint16_t), out_stride);
    ASSERT_EQ(4, out_element_count);

    // Verify the previously written data
    ptr = (uint16_t*)out_stream;
    for (int i = 0; i < out_element_count; ++i)
    {
        ASSERT_EQ(i, ptr[0]);
        ASSERT_EQ(i, ptr[1]);
        ptr = (uint16_t*)((uintptr_t)ptr + out_stride);
    }
}

TEST_F(GetDataTest, WriteOutsideStream)
{
    void *out_stream = 0x0;
    uint32_t out_stride = 0;
    uint32_t out_element_count = 0;

    // Get texcoord stream
    dmBuffer::Result r = dmBuffer::GetStream(buffer, dmHashString64("texcoord"), dmBuffer::BUFFER_TYPE_UINT16, 2, &out_stream, &out_stride, &out_element_count);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_NE((void*)0x0, out_stream);
    ASSERT_EQ(2*sizeof(uint16_t), out_stride);
    ASSERT_EQ(4, out_element_count);

    // Write past the number of elements
    out_element_count = out_element_count + 3;
    uint16_t* ptr = (uint16_t*)out_stream;
    for (int i = 0; i < out_element_count; ++i)
    {
        ptr[0] = i;
        ptr[1] = i;
        ptr = (uint16_t*)((uintptr_t)ptr + out_stride);
    }

    // Get stream again, expecting invalid guard
    r = dmBuffer::GetStream(buffer, dmHashString64("texcoord"), dmBuffer::BUFFER_TYPE_UINT16, 2, &out_stream, &out_stride, &out_element_count);
    ASSERT_EQ(dmBuffer::RESULT_GUARD_INVALID, r);
    ASSERT_NE((void*)0x0, out_stream);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
