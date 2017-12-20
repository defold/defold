#include <gtest/gtest.h>

#include "../dlib/log.h"
#include "../dlib/hash.h"
#include "../dlib/buffer.h"

#define RIG_EPSILON 0.0001f

class BufferTest : public ::testing::Test
{
    virtual void SetUp() {
        dmBuffer::NewContext();
    }

    virtual void TearDown() {
        dmBuffer::DeleteContext();
    }
};

class GetDataTest : public ::testing::Test
{
public:
    dmBuffer::HBuffer buffer;
    uint32_t          count;
    uint32_t          stride; // in bytes
    void*             out_stream;
    uint32_t          out_count;
    uint32_t          out_components;
    uint32_t          out_stride;

protected:
    virtual void SetUp() {
        dmBuffer::NewContext();

        dmBuffer::StreamDeclaration streams_decl[] = {
            {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
            {dmHashString64("texcoord"), dmBuffer::VALUE_TYPE_UINT16, 2}
        };

        count = 4;
        stride = sizeof(float)*3 + sizeof(uint16_t)*2;
        dmBuffer::Create(count, streams_decl, 2, &buffer);
    }

    virtual void TearDown() {
        dmBuffer::Destroy(buffer);
        dmBuffer::DeleteContext();
    }
};

struct AlignmentTestParams
{
    uint32_t count;
    dmBuffer::StreamDeclaration streams_decl[dmBuffer::MAX_VALUE_TYPE_COUNT];
    uint32_t streams_decl_count;
    uint32_t expected_size; // expected struct size
    uintptr_t expected_offsets[dmBuffer::MAX_VALUE_TYPE_COUNT];
};

class AlignmentTest : public ::testing::TestWithParam<AlignmentTestParams>
{
public:
    dmBuffer::HBuffer buffer;
    void*             out_stream;
    uint32_t          out_count;
    uint32_t          out_components;
    uint32_t          out_stride;

protected:
    virtual void SetUp() {
        dmBuffer::NewContext();
        buffer = 0x0;
    }

    virtual void TearDown() {
        dmBuffer::Destroy(buffer);
        dmBuffer::DeleteContext();
    }
};

#define CLEAR_OUT_VARS()\
    out_stream = 0x0;\
    out_count = 0;\
    out_components = 0;\
    out_stride = 0;\

TEST_F(BufferTest, InvalidAllocation)
{
    dmBuffer::HBuffer buffer = 0x0;
    dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("dummy1"), dmBuffer::VALUE_TYPE_UINT8, 3},
        {dmHashString64("dummy2"), dmBuffer::VALUE_TYPE_UINT8, 1},
        {dmHashString64("dummy3"), dmBuffer::VALUE_TYPE_UINT8, 0}
    };

    // Empty declaration, zero elements
    dmBuffer::Result r = dmBuffer::Create(0, streams_decl, 0, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_BUFFER_SIZE_ERROR, r);
    ASSERT_EQ(0, buffer);

    // Empty declaration, 1 element
    r = dmBuffer::Create(1, streams_decl, 0, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_STREAM_SIZE_ERROR, r);
    ASSERT_EQ(0, buffer);

    // Valid declaration, zero elements
    r = dmBuffer::Create(0, streams_decl, 2, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_BUFFER_SIZE_ERROR, r);
    ASSERT_EQ(0, buffer);

    // Valid sizes, but invalid valuetype count (see "dummy3" above)
    r = dmBuffer::Create(1, streams_decl, 3, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_STREAM_SIZE_ERROR, r);
    ASSERT_EQ(0, buffer);

    // Valid sizes, but buffer is NULL-pointer
    r = dmBuffer::Create(1, streams_decl, 1, 0x0);
    ASSERT_EQ(dmBuffer::RESULT_ALLOCATION_ERROR, r);
    ASSERT_EQ(0, buffer);

    // Valid sizes, but declaration is NULL-pointer
    r = dmBuffer::Create(1, 0x0, 1, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_ALLOCATION_ERROR, r);
    ASSERT_EQ(0, buffer);
}

TEST_F(BufferTest, ValidAllocation)
{
    dmBuffer::HBuffer buffer = 0x0;
    dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("position"), dmBuffer::VALUE_TYPE_UINT8, 3},
        {dmHashString64("texcoord"), dmBuffer::VALUE_TYPE_UINT8, 1}
    };

    // Declaration of 2 streams, with 4 elements
    dmBuffer::Result r = dmBuffer::Create(4, streams_decl, 2, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_NE(0, buffer);
    dmBuffer::Destroy(buffer);
    buffer = 0;

    // Allocation with only the first stream declaration
    r = dmBuffer::Create(4, streams_decl, 1, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_NE(0, buffer);
    dmBuffer::Destroy(buffer);
    buffer = 0;

    // Larger allocation
    r = dmBuffer::Create(1024*1024, streams_decl, 2, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_NE(0, buffer);
    dmBuffer::Destroy(buffer);
}

TEST_F(BufferTest, ValueTypes)
{
    dmBuffer::HBuffer buffer = 0;
    dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("dummy"), dmBuffer::VALUE_TYPE_UINT8, 3}
    };
    for (int i = 0; i < dmBuffer::MAX_VALUE_TYPE_COUNT; ++i)
    {
        streams_decl[0].m_Type = (dmBuffer::ValueType)i;

        dmBuffer::Result r = dmBuffer::Create(4, streams_decl, 1, &buffer);
        ASSERT_EQ(dmBuffer::RESULT_OK, r);
        ASSERT_NE(0, buffer);
        dmBuffer::Destroy(buffer);
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
    r = dmBuffer::Create(1, streams_decl, 1, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);

    // Get and write outside buffer
    void *out_stream = 0x0;
    uint32_t count = 0;
    uint32_t components = 0;
    uint32_t stride = 0;
    r = dmBuffer::GetStream(buffer, dmHashString64("dummy"), &out_stream, &count, &components, &stride);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);

    // Get address outside stream boundry
    uint8_t* ptr = (uint8_t*)((uintptr_t)out_stream + count);
    ptr[0] = 0xBA;
    ptr[1] = 0xDC;
    ptr[2] = 0x0D;
    ptr[3] = 0xED;

    // We should have trashed the guard bytes
    r = dmBuffer::ValidateBuffer(buffer);
    ASSERT_EQ(dmBuffer::RESULT_GUARD_INVALID, r);

    dmBuffer::Destroy(buffer);
}

TEST_F(GetDataTest, GetStreamType)
{
    dmBuffer::Result r;
    dmBuffer::ValueType type;
    uint32_t typecount;
    r = GetStreamType(buffer, dmHashString64("position"), &type, &typecount);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(dmBuffer::VALUE_TYPE_FLOAT32, type);
    ASSERT_EQ(3, typecount);

    r = GetStreamType(buffer, dmHashString64("texcoord"), &type, &typecount);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(dmBuffer::VALUE_TYPE_UINT16, type);
    ASSERT_EQ(2, typecount);

    r = GetStreamType(buffer, dmHashString64("foobar"), &type, &typecount);
    ASSERT_EQ(dmBuffer::RESULT_STREAM_MISSING, r);

    r = GetStreamType(0, dmHashString64("foobar"), &type, &typecount);
    ASSERT_EQ(dmBuffer::RESULT_BUFFER_INVALID, r);
}

TEST_F(GetDataTest, InvalidGetData)
{
    dmBuffer::Result r;

    // Invalid buffer pointer
    CLEAR_OUT_VARS();
    r = dmBuffer::GetStream(0, dmHashString64("texcoord"), &out_stream, &out_count, &out_components, &out_stride);
    ASSERT_EQ(dmBuffer::RESULT_BUFFER_INVALID, r);
    ASSERT_EQ(0x0, out_stream);
    ASSERT_EQ(0, out_count);
    ASSERT_EQ(0, out_components);
    ASSERT_EQ(0, out_stride);

    // Get unknown stream
    CLEAR_OUT_VARS();
    r = dmBuffer::GetStream(buffer, dmHashString64("unknown"), &out_stream, &out_count, &out_components, &out_stride);
    ASSERT_EQ(dmBuffer::RESULT_STREAM_MISSING, r);
    ASSERT_EQ(0x0, out_stream);
    ASSERT_EQ(0, out_count);
    ASSERT_EQ(0, out_components);
    ASSERT_EQ(0, out_stride);
}

TEST_F(GetDataTest, ValidGetData)
{
    // Get texcoord stream
    CLEAR_OUT_VARS();
    dmBuffer::Result r = dmBuffer::GetStream(buffer, dmHashString64("texcoord"), &out_stream, &out_count, &out_components, &out_stride);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_NE((void*)0x0, out_stream);
    ASSERT_EQ(4, out_count);
    ASSERT_EQ(2, out_components);
    ASSERT_EQ(stride/sizeof(uint16_t), out_stride);

    uint16_t* ptr = (uint16_t*)out_stream;
    for (int i = 0; i < out_count; ++i)
    {
        ptr[0] = i;
        ptr[1] = i;
        ptr += out_stride;
    }

    // Get stream again
    CLEAR_OUT_VARS();
    r = dmBuffer::GetStream(buffer, dmHashString64("texcoord"), &out_stream, &out_count, &out_components, &out_stride);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_NE((void*)0x0, out_stream);
    ASSERT_EQ(4, out_count);
    ASSERT_EQ(2, out_components);
    ASSERT_EQ(stride/sizeof(uint16_t), out_stride);

    // Verify the previously written data
    ptr = (uint16_t*)out_stream;
    for (int i = 0; i < out_count; ++i)
    {
        ASSERT_EQ(i, ptr[0]);
        ASSERT_EQ(i, ptr[1]);
        ptr += out_stride;
    }
}


TEST_F(GetDataTest, ValidGetBytes)
{
    // Get texcoord stream
    CLEAR_OUT_VARS();
    dmBuffer::Result r = dmBuffer::GetStream(buffer, dmHashString64("position"), &out_stream, &out_count, &out_components, &out_stride);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_NE((void*)0x0, out_stream);
    ASSERT_EQ(4, out_count);
    ASSERT_EQ(3, out_components);
    ASSERT_EQ(stride/sizeof(float), out_stride);

    uint8_t* ptr = (uint8_t*)out_stream;
    for (uint8_t i = 0; i < out_count*stride; ++i)
    {
        ptr[0] = (uint8_t)(i & 0xff);
        ++ptr;
    }

    // Get stream again
    uint8_t* data = 0;
    uint32_t datasize = 0;
    r = dmBuffer::GetBytes(buffer, (void**)&data, &datasize);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_NE((void*)0x0, data);
    ASSERT_EQ(out_stream, data);
    ASSERT_EQ(out_count*stride, datasize);

    // Verify the previously written data
    ptr = (uint8_t*)out_stream;
    for (int i = 0; i < datasize; ++i)
    {
        ASSERT_EQ( (uint8_t)(i & 0xFF), ptr[0]);
        ++ptr;
    }
}

TEST_F(GetDataTest, WriteOutsideStream)
{
    // Get texcoord stream
    CLEAR_OUT_VARS();
    dmBuffer::Result r = dmBuffer::GetStream(buffer, dmHashString64("texcoord"), &out_stream, &out_count, &out_components, &out_stride);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_NE((void*)0x0, out_stream);
    ASSERT_EQ(4, out_count);
    ASSERT_EQ(2, out_components);
    ASSERT_EQ(stride/sizeof(uint16_t), out_stride);

    // Write past the number of elements
    uint16_t* ptr = (uint16_t*)out_stream;
    for (int i = 0; i < out_count+1; ++i)
    {
        ptr[0] = i;
        ptr[1] = i;
        ptr += out_stride;
    }

    // Get stream again, expecting invalid guard
    CLEAR_OUT_VARS();
    r = dmBuffer::GetStream(buffer, dmHashString64("texcoord"), &out_stream, &out_count, &out_components, &out_stride);
    ASSERT_EQ(dmBuffer::RESULT_GUARD_INVALID, r);
    ASSERT_EQ((void*)0x0, out_stream);
}

TEST_F(GetDataTest, CheckUniquePointers)
{
    // Get position stream
    CLEAR_OUT_VARS();
    dmBuffer::Result r = dmBuffer::GetStream(buffer, dmHashString64("position"), &out_stream, &out_count, &out_components, &out_stride);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    void* position_ptr = out_stream;

    // Get texcoord again
    CLEAR_OUT_VARS();
    r = dmBuffer::GetStream(buffer, dmHashString64("texcoord"), &out_stream, &out_count, &out_components, &out_stride);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    void* texcoord_ptr = out_stream;

    ASSERT_NE(texcoord_ptr, position_ptr);
}

TEST_F(GetDataTest, CheckOverwrite)
{
    const uint16_t magic_number = 0xC0DE;

    // Get texcoord stream
    CLEAR_OUT_VARS();
    dmBuffer::Result r = dmBuffer::GetStream(buffer, dmHashString64("texcoord"), &out_stream, &out_count, &out_components, &out_stride);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);

    // Write magic number to texcoord stream
    uint16_t* ptr_u16 = (uint16_t*)out_stream;
    for (int i = 0; i < out_count; ++i)
    {
        ptr_u16[0] = magic_number;
        ptr_u16[1] = magic_number;
        ptr_u16 += out_stride;
    }

    // Get position stream
    CLEAR_OUT_VARS();
    r = dmBuffer::GetStream(buffer, dmHashString64("position"), &out_stream, &out_count, &out_components, &out_stride);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);

    // Write different data to position stream
    float* ptr_f32 = (float*)out_stream;
    for (int i = 0; i < out_count; ++i)
    {
        ptr_f32[0] = 1.0f;
        ptr_f32[1] = 2.0f;
        ptr_f32[1] = -3.0f;
        ptr_f32 += out_stride;
    }

    // Read back texcoord stream and verify magic number
    CLEAR_OUT_VARS();
    r = dmBuffer::GetStream(buffer, dmHashString64("texcoord"), &out_stream, &out_count, &out_components, &out_stride);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);

    // Write magic number to texcoord stream
    ptr_u16 = (uint16_t*)out_stream;
    for (int i = 0; i < out_count; ++i)
    {
        ASSERT_EQ(magic_number, ptr_u16[0]);
        ASSERT_EQ(magic_number, ptr_u16[1]);
        ptr_u16 += out_stride;
    }
}


TEST_P(AlignmentTest, CheckAlignment)
{
    const AlignmentTestParams& p = GetParam();

    dmBuffer::Result r = dmBuffer::Create(p.count, p.streams_decl, p.streams_decl_count, &buffer);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(0, ((uintptr_t)buffer) % 16);

    ASSERT_EQ(p.expected_size, dmBuffer::GetStructSize(buffer));

    uint32_t datasize = 0;
    uint8_t* data = 0;
    r = dmBuffer::GetBytes(buffer, (void**)&data, &datasize);
    ASSERT_EQ(p.count * p.expected_size, datasize);

    ASSERT_EQ(dmBuffer::RESULT_OK, r);

    for (uint32_t i = 0; i < p.streams_decl_count; ++i)
    {
        CLEAR_OUT_VARS();
        const dmBuffer::StreamDeclaration& stream_decl = p.streams_decl[i];
        r = dmBuffer::GetStream(buffer, stream_decl.m_Name, &out_stream, &out_count, &out_components, &out_stride);
        ASSERT_EQ(dmBuffer::RESULT_OK, r);
        ASSERT_EQ(p.count, out_count);

        // Offset from start
        uintptr_t offset = (uintptr_t)out_stream - (uintptr_t)data;
        ASSERT_EQ(p.expected_offsets[i], offset);

        uint32_t value_size = dmBuffer::GetSizeForValueType(stream_decl.m_Type);
        ASSERT_EQ(0, ((uintptr_t)out_stream) % value_size);

        ASSERT_EQ(stream_decl.m_Count, out_components);
        ASSERT_EQ(p.expected_size / value_size, out_stride);
    }

    // No destroy of buffer, it's handled by the AlignmentTest class
}


struct AlignTestA
{
    float       x, y, z;
    uint8_t     r, g, b;
};

struct AlignTestB
{
    float       x, y, z;
    float       nx, ny, nz;
    float       u, v;
};

struct AlignTestC
{
    uint8_t     r, g, b;
    float       a;
};

struct AlignTestD
{
    uint8_t     r, g, b;
};

struct AlignTestE
{
    float       x, y, z;
    float       nx, ny, nz;
    float       uv0[2];
    float       uv1[3];
};


const AlignmentTestParams valid_stream_setups[] = {
    {4, {
            {dmHashString64("pos"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
            {dmHashString64("rgb"),  dmBuffer::VALUE_TYPE_UINT8,  3},
        }, 2, sizeof(AlignTestA),
            {   offsetof(struct AlignTestA, x),
                offsetof(struct AlignTestA, r) }
    },
    {4, {
            {dmHashString64("pos"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
            {dmHashString64("normal"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
            {dmHashString64("texcoord"),  dmBuffer::VALUE_TYPE_FLOAT32,  2},
        }, 3, sizeof(AlignTestB),
            {   offsetof(struct AlignTestB, x),
                offsetof(struct AlignTestB, nx),
                offsetof(struct AlignTestB, u) }
    },
    {4, {
            {dmHashString64("rgb"), dmBuffer::VALUE_TYPE_UINT8, 3},
        }, 1, sizeof(AlignTestD),
            {   offsetof(struct AlignTestD, r) }
    },
    {4, {
            {dmHashString64("pos"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
            {dmHashString64("normal"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
            {dmHashString64("texcoord0"),  dmBuffer::VALUE_TYPE_FLOAT32,  2},
            {dmHashString64("texcoord1"),  dmBuffer::VALUE_TYPE_FLOAT32,  3},
        }, 4, sizeof(AlignTestE),
            {   offsetof(struct AlignTestE, x),
                offsetof(struct AlignTestE, nx),
                offsetof(struct AlignTestE, uv0),
                offsetof(struct AlignTestE, uv1) }
    },
};

INSTANTIATE_TEST_CASE_P(AlignmentSequence, AlignmentTest, ::testing::ValuesIn(valid_stream_setups));

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}

#undef CLEAR_OUT_VARS
