// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <jc_test/jc_test.h>
#include <dlib/array.h>
#include <dlib/math.h>
#include <dlib/zlib.h>
#include <stdlib.h>
#include <string.h>
#include "../texc.h"
#include "../texc_private.h"

static uint32_t g_AllocationCalls;
static uint32_t g_FailedAllocation;
static uint32_t g_LiveAllocations;
static bool g_FailCompression;

static void* AllocateBuffer(size_t size)
{
    if (++g_AllocationCalls == g_FailedAllocation)
    {
        return 0;
    }
    void* data = malloc(size);
    if (data)
    {
        ++g_LiveAllocations;
    }
    return data;
}

static void FreeBuffer(void* data)
{
    if (data)
    {
        --g_LiveAllocations;
    }
    free(data);
}

namespace dmZlib
{
    static Result DeflateBufferForTest(const void* data, uint32_t size, int level, void* context, Writer writer)
    {
        if (g_FailCompression)
        {
            const uint8_t partial[] = { 1, 2, 3 };
            writer(context, partial, sizeof(partial));
            return RESULT_BUF_ERROR;
        }
        return DeflateBuffer(data, size, level, context, writer);
    }
}

// Exercise the real private writer and inject failures without exhausting process memory.
#define CompressBuffer CompressBufferForTest
#define DestroyBuffer DestroyBufferForTest
#define DeflateBuffer DeflateBufferForTest
#define malloc AllocateBuffer
#define free FreeBuffer
#include "../texc_buffer.cpp"
#undef free
#undef malloc
#undef DeflateBuffer
#undef DestroyBuffer
#undef CompressBuffer

class TexcBufferTest : public jc_test_base_class
{
    protected:
    void SetUp()
    {
        g_AllocationCalls = 0;
        g_FailedAllocation = 0;
        g_FailCompression = false;
        ASSERT_EQ(0U, g_LiveAllocations);
    }

    void TearDown()
    {
        ASSERT_EQ(0U, g_LiveAllocations);
    }
};

static bool WriteFixed(void* context, const void* data, uint32_t size)
{
    dmArray<uint8_t>* out = (dmArray<uint8_t>*)context;
    if (size > out->Remaining())
    {
        return false;
    }
    if (size)
    {
        out->PushArray((const uint8_t*)data, size);
    }
    return true;
}

// Verify empty, small, compressible and noisy inputs retain the exact compressed stream and round-trip data.
TEST_F(TexcBufferTest, CompressRoundTrip)
{
    const uint32_t sizes[] = { 0, 1, 32767, 32768, 32769, 1024 * 1024 };
    for (uint32_t s = 0; s < DM_ARRAY_SIZE(sizes); ++s)
    {
        uint32_t size = sizes[s];
        dmArray<uint8_t> input;
        dmArray<uint8_t> delta;
        dmArray<uint8_t> expected;
        dmArray<uint8_t> inflated;
        input.SetCapacity(size);
        input.SetSize(size);
        delta.SetCapacity(size);
        delta.SetSize(size);
        expected.SetCapacity(size * 2 + 1024);
        inflated.SetCapacity(size);
        for (uint32_t noisy = 0; noisy < 2; ++noisy)
        {
            uint32_t random = 1;
            for (uint32_t i = 0; i < size; ++i)
            {
                random ^= random << 13;
                random ^= random >> 17;
                random ^= random << 5;
                input[i] = noisy ? (uint8_t)random : 42;
                delta[i] = input[i] - (i ? input[i - 1] : 0);
            }
            expected.SetSize(0);
            ASSERT_EQ(dmZlib::RESULT_OK, dmZlib::DeflateBuffer(delta.Begin(), size, 9, &expected, WriteFixed));
            dmTexc::Buffer* compressed = dmTexc::CompressBuffer(input.Begin(), size);
            ASSERT_NE((dmTexc::Buffer*)0, compressed);
            ASSERT_TRUE(compressed->m_IsCompressed);
            ASSERT_EQ(expected.Size(), compressed->m_DataCount);
            ASSERT_EQ(0, memcmp(expected.Begin(), compressed->m_Data, expected.Size()));

            inflated.SetSize(0);
            ASSERT_EQ(dmZlib::RESULT_OK, dmZlib::InflateBuffer(compressed->m_Data, compressed->m_DataCount, &inflated, WriteFixed));
            ASSERT_EQ(size, inflated.Size());
            for (uint32_t i = 1; i < size; ++i)
            {
                inflated[i] += inflated[i - 1];
            }
            if (size)
            {
                ASSERT_EQ(0, memcmp(input.Begin(), inflated.Begin(), size));
            }
            dmTexc::DestroyBuffer(compressed);
        }
    }
}

// Guard against quadratic allocation/copying when the codec emits many small chunks.
TEST_F(TexcBufferTest, WriterGrowthIsLinear)
{
    uint8_t chunk[16 * 1024];
    dmArray<uint8_t> output;
    output.SetCapacity(32 * 1024);
    uint64_t allocated = output.Capacity();
    uint64_t copied = 0;
    for (uint32_t i = 0; i < 64; ++i)
    {
        memset(chunk, i, sizeof(chunk));
        uint32_t old_capacity = output.Capacity();
        uint32_t old_size = output.Size();
        ASSERT_TRUE(dmTexc::DeflateWriter(&output, chunk, sizeof(chunk)));
        if (output.Capacity() != old_capacity)
        {
            allocated += output.Capacity();
            copied += old_size;
        }
    }
    ASSERT_EQ(64U * sizeof(chunk), output.Size());
    ASSERT_LE(allocated, (uint64_t)output.Size() * 4);
    ASSERT_LE(copied, (uint64_t)output.Size() * 2);
    for (uint32_t i = 0; i < 64; ++i)
    {
        memset(chunk, i, sizeof(chunk));
        ASSERT_EQ(0, memcmp(output.Begin() + i * sizeof(chunk), chunk, sizeof(chunk)));
    }
}

// A single chunk larger than the growth increment must fit and preserve previously written data.
TEST_F(TexcBufferTest, WriterAcceptsLargeChunk)
{
    uint8_t chunk[100];
    memset(chunk, 42, sizeof(chunk));
    dmArray<uint8_t> output;
    output.SetCapacity(8);
    output.Push(7);
    ASSERT_TRUE(dmTexc::DeflateWriter(&output, chunk, sizeof(chunk)));
    ASSERT_EQ(101U, output.Size());
    ASSERT_EQ(7, output[0]);
    ASSERT_EQ(0, memcmp(output.Begin() + 1, chunk, sizeof(chunk)));
}

// Reject overflowing output sizes before allocating or touching the supplied chunk.
TEST_F(TexcBufferTest, WriterRejectsOverflow)
{
    dmArray<uint8_t> output;
    output.SetCapacity(8);
    output.Push(7);
    uint8_t byte = 42;
    ASSERT_FALSE(dmTexc::DeflateWriter(&output, &byte, UINT32_MAX));
    ASSERT_EQ(1U, output.Size());
    ASSERT_EQ(8U, output.Capacity());
    ASSERT_EQ(7, output[0]);
}

// A codec failure after writing partial output must release scratch memory and return no result.
TEST_F(TexcBufferTest, CompressionFailure)
{
    uint8_t input[64] = {};
    g_FailCompression = true;
    ASSERT_EQ((dmTexc::Buffer*)0, dmTexc::CompressBufferForTest(input, sizeof(input)));
    ASSERT_EQ(1U, g_AllocationCalls);
    ASSERT_EQ(0U, g_LiveAllocations);
}

// Failure to allocate either scratch or final output must release any earlier owned allocation.
TEST_F(TexcBufferTest, AllocationFailure)
{
    uint8_t input[64] = {};
    for (uint32_t i = 1; i <= 2; ++i)
    {
        g_AllocationCalls = 0;
        g_FailedAllocation = i;
        ASSERT_EQ((dmTexc::Buffer*)0, dmTexc::CompressBufferForTest(input, sizeof(input)));
        ASSERT_EQ(i, g_AllocationCalls);
        ASSERT_EQ(0U, g_LiveAllocations);
    }
}
