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
    void*             out_stream;
    uint32_t          out_stride;
    uint32_t          out_element_count;

protected:
    virtual void SetUp() {
        dmBuffer::StreamDeclaration streams_decl[] = {
            {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
            {dmHashString64("texcoord"), dmBuffer::VALUE_TYPE_UINT16, 2}
        };

        dmBuffer::Allocate(4, streams_decl, 2, &buffer);
    }

    virtual void TearDown() {
        dmBuffer::Free(buffer);
    }
};

#define CLEAR_OUT_VARS(out_stream, out_stride, out_element_count)\
    out_stream = 0x0;\
    out_stride = 0;\
    out_element_count = 0;\

TEST_F(BufferTest, InvalidAllocation)
{
    dmBuffer::HBuffer buffer = 0x0;
    dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("dummy1"), dmBuffer::VALUE_TYPE_UINT8, 3},
        {dmHashString64("dummy2"), dmBuffer::VALUE_TYPE_UINT8, 1},
        {dmHashString64("dummy3"), dmBuffer::VALUE_TYPE_UINT8, 0}
    };

    // Empty declaration, zero elements
    dmBuffer::Result r = dmBuffer::Allocate(0, streams_decl, 0, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_BUFFER_SIZE_ERROR, r);
    ASSERT_EQ(0x0, buffer);

    // Empty declaration, 1 element
    r = dmBuffer::Allocate(1, streams_decl, 0, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_STREAM_SIZE_ERROR, r);
    ASSERT_EQ(0x0, buffer);

    // Valid declaration, zero elements
    r = dmBuffer::Allocate(0, streams_decl, 2, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_BUFFER_SIZE_ERROR, r);
    ASSERT_EQ(0x0, buffer);

    // Valid sizes, but invalid valuetype count (see "dummy3" above)
    r = dmBuffer::Allocate(1, streams_decl, 3, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_STREAM_SIZE_ERROR, r);
    ASSERT_EQ(0x0, buffer);

    // Valid sizes, but buffer is NULL-pointer
    r = dmBuffer::Allocate(1, streams_decl, 1, 0x0);
    ASSERT_EQ(dmBuffer::RESULT_ALLOCATION_ERROR, r);
    ASSERT_EQ(0x0, buffer);

    // Valid sizes, but declaration is NULL-pointer
    r = dmBuffer::Allocate(1, 0x0, 1, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_ALLOCATION_ERROR, r);
    ASSERT_EQ(0x0, buffer);
}

TEST_F(BufferTest, ValidAllocation)
{
    dmBuffer::HBuffer buffer = 0x0;
    dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("position"), dmBuffer::VALUE_TYPE_UINT8, 3},
        {dmHashString64("texcoord"), dmBuffer::VALUE_TYPE_UINT8, 1}
    };

    // Declaration of 2 streams, with 4 elements
    dmBuffer::Result r = dmBuffer::Allocate(4, streams_decl, 2, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_NE((void*)0x0, buffer);
    dmBuffer::Free(buffer);
    buffer = 0x0;

    // Allocation with only the first stream declaration
    r = dmBuffer::Allocate(4, streams_decl, 1, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_NE((void*)0x0, buffer);
    dmBuffer::Free(buffer);
    buffer = 0x0;

    // Larger allocation
    r = dmBuffer::Allocate(1024*1024, streams_decl, 2, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_NE((void*)0x0, buffer);
    dmBuffer::Free(buffer);
}

TEST_F(BufferTest, ValueTypes)
{
    dmBuffer::HBuffer buffer = 0x0;
    dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("dummy"), dmBuffer::VALUE_TYPE_UINT8, 3}
    };
    for (int i = 0; i < dmBuffer::MAX_VALUE_TYPE_COUNT; ++i)
    {
        streams_decl[0].m_ValueType = (dmBuffer::ValueType)i;

        dmBuffer::Result r = dmBuffer::Allocate(4, streams_decl, 1, &buffer);
        ASSERT_EQ(dmBuffer::RESULT_OK, r);
        ASSERT_NE((void*)0x0, buffer);
        dmBuffer::Free(buffer);
        buffer = 0x0;
    }
}

TEST_F(BufferTest, ValidateBuffer)
{
    dmBuffer::HBuffer buffer = 0x0;
    dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("dummy"), dmBuffer::VALUE_TYPE_UINT8, 1}
    };

    // NULL pointer buffer is not valid
    dmBuffer::Result r = dmBuffer::ValidateBuffer(buffer);
    ASSERT_EQ(dmBuffer::RESULT_BUFFER_INVALID, r);

    // Create valid buffer, but write outside stream boundry
    // Should not be a valid buffer after.
    r = dmBuffer::Allocate(1, streams_decl, 1, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);

    // Get and write outside buffer
    void *out_stream = 0x0;
    uint32_t out_stride = 0;
    uint32_t out_element_count = 0;
    r = dmBuffer::GetStream(buffer, dmHashString64("dummy"), dmBuffer::VALUE_TYPE_UINT8, 1, &out_stream, &out_stride, &out_element_count);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);

    // Get address outside stream boundry
    uint8_t* ptr = (uint8_t*)((uintptr_t)out_stream + out_stride);
    ptr[0] = 0xBA;
    ptr[1] = 0xDC;
    ptr[2] = 0x0D;
    ptr[3] = 0xED;

    // We should have trashed the guard bytes
    r = dmBuffer::ValidateBuffer(buffer);
    ASSERT_EQ(dmBuffer::RESULT_GUARD_INVALID, r);

    dmBuffer::Free(buffer);
}



TEST_F(BufferTest, InvalidGetData)
{
    dmBuffer::HBuffer buffer = 0x0;
    dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
        {dmHashString64("texcoord"), dmBuffer::VALUE_TYPE_UINT16, 2}
    };

    void *out_stream = 0x0;
    uint32_t out_stride = 0;
    uint32_t out_element_count = 0;

    dmBuffer::Result r = dmBuffer::Allocate(4, streams_decl, 2, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);

    // Invalid buffer pointer
    CLEAR_OUT_VARS(out_stream, out_stride, out_element_count);
    r = dmBuffer::GetStream(0, dmHashString64("texcoord"), dmBuffer::VALUE_TYPE_UINT16, 8, &out_stream, &out_stride, &out_element_count);
    ASSERT_EQ(dmBuffer::RESULT_BUFFER_INVALID, r);
    ASSERT_EQ(0x0, out_stream);
    ASSERT_EQ(0, out_stride);
    ASSERT_EQ(0, out_element_count);

    // Get unknown stream
    CLEAR_OUT_VARS(out_stream, out_stride, out_element_count);
    r = dmBuffer::GetStream(buffer, dmHashString64("unknown"), dmBuffer::VALUE_TYPE_UINT8, 2, &out_stream, &out_stride, &out_element_count);
    ASSERT_EQ(dmBuffer::RESULT_STREAM_DOESNT_EXIST, r);
    ASSERT_EQ(0x0, out_stream);
    ASSERT_EQ(0, out_stride);
    ASSERT_EQ(0, out_element_count);

    // Correct stream, but wrong type
    CLEAR_OUT_VARS(out_stream, out_stride, out_element_count);
    r = dmBuffer::GetStream(buffer, dmHashString64("position"), dmBuffer::VALUE_TYPE_UINT16, 3, &out_stream, &out_stride, &out_element_count);
    ASSERT_EQ(dmBuffer::RESULT_STREAM_WRONG_TYPE, r);
    ASSERT_EQ(0x0, out_stream);
    ASSERT_EQ(0, out_stride);
    ASSERT_EQ(0, out_element_count);

    // Correct stream, but wrong value count
    CLEAR_OUT_VARS(out_stream, out_stride, out_element_count);
    r = dmBuffer::GetStream(buffer, dmHashString64("texcoord"), dmBuffer::VALUE_TYPE_UINT16, 8, &out_stream, &out_stride, &out_element_count);
    ASSERT_EQ(dmBuffer::RESULT_STREAM_WRONG_COUNT, r);
    ASSERT_EQ(0x0, out_stream);
    ASSERT_EQ(0, out_stride);
    ASSERT_EQ(0, out_element_count);

    dmBuffer::Free(buffer);
}

TEST_F(GetDataTest, ValidGetData)
{
    // Get texcoord stream
    CLEAR_OUT_VARS(out_stream, out_stride, out_element_count);
    dmBuffer::Result r = dmBuffer::GetStream(buffer, dmHashString64("texcoord"), dmBuffer::VALUE_TYPE_UINT16, 2, &out_stream, &out_stride, &out_element_count);
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
    CLEAR_OUT_VARS(out_stream, out_stride, out_element_count);
    r = dmBuffer::GetStream(buffer, dmHashString64("texcoord"), dmBuffer::VALUE_TYPE_UINT16, 2, &out_stream, &out_stride, &out_element_count);
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
    // Get texcoord stream
    CLEAR_OUT_VARS(out_stream, out_stride, out_element_count);
    dmBuffer::Result r = dmBuffer::GetStream(buffer, dmHashString64("texcoord"), dmBuffer::VALUE_TYPE_UINT16, 2, &out_stream, &out_stride, &out_element_count);
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
    r = dmBuffer::GetStream(buffer, dmHashString64("texcoord"), dmBuffer::VALUE_TYPE_UINT16, 2, &out_stream, &out_stride, &out_element_count);
    ASSERT_EQ(dmBuffer::RESULT_GUARD_INVALID, r);
    ASSERT_NE((void*)0x0, out_stream);
}

TEST_F(GetDataTest, Alignment)
{
    CLEAR_OUT_VARS(out_stream, out_stride, out_element_count);
    dmBuffer::Result r = dmBuffer::GetStream(buffer, dmHashString64("texcoord"), dmBuffer::VALUE_TYPE_UINT16, 2, &out_stream, &out_stride, &out_element_count);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(0, ((uintptr_t)out_stream) % 16);

    r = dmBuffer::GetStream(buffer, dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3, &out_stream, &out_stride, &out_element_count);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(0, ((uintptr_t)out_stream) % 16);
}

#undef CLEAR_OUT_VARS

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
