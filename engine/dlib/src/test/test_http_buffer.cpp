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

#include <dlib/http/http_internal.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

static void CheckTinyAppends(uint32_t initial_capacity)
{
    const uint32_t response_size = 4 * 1024 * 1024;
    dmArray<char> buffer;
    buffer.SetCapacity(initial_capacity);
    uint32_t allocations = initial_capacity ? 1 : 0;
    uint64_t allocated_bytes = initial_capacity;
    const char byte = 'a';

    for (uint32_t i = 0; i < response_size; ++i)
    {
        uint32_t previous_capacity = buffer.Capacity();
        dmHttpService::AppendResponseBuffer(buffer, &byte, 1);
        if (buffer.Capacity() != previous_capacity)
        {
            ++allocations;
            allocated_bytes += buffer.Capacity();
        }
        // Fail early if small appends cause linear growth and quadratic copying.
        ASSERT_LE(allocations, 13U);
    }

    ASSERT_EQ(response_size, buffer.Size());
    ASSERT_LE(allocated_bytes, (uint64_t)response_size * 2);
}

// Verifies logarithmic growth for Lua's initially empty response buffer.
TEST(HttpResponseBuffer, TinyAppends)
{
    CheckTinyAppends(0);
}

// Verifies the legacy service's initial 64 KiB buffer also avoids quadratic growth.
TEST(HttpResponseBuffer, TinyAppendsWithInitialCapacity)
{
    CheckTinyAppends(64 * 1024);
}

// Verifies empty appends and exact fits reuse storage, and exceeding capacity doubles it.
TEST(HttpResponseBuffer, EmptyAndExactFit)
{
    dmArray<char> buffer;
    char data[1024];
    memset(data, 0x5a, sizeof(data));
    ASSERT_TRUE(dmHttpService::AppendResponseBuffer(buffer, 0, 0));
    ASSERT_TRUE(dmHttpService::AppendResponseBuffer(buffer, data, 0));
    ASSERT_EQ(0U, buffer.Size());
    ASSERT_EQ(0U, buffer.Capacity());

    ASSERT_TRUE(dmHttpService::AppendResponseBuffer(buffer, data, 1));
    ASSERT_EQ(1024U, buffer.Capacity());
    const char* storage = buffer.Begin();
    ASSERT_TRUE(dmHttpService::AppendResponseBuffer(buffer, data, sizeof(data) - 1));
    ASSERT_EQ(1024U, buffer.Size());
    ASSERT_EQ(storage, buffer.Begin());
    ASSERT_EQ(1024U, buffer.Capacity());

    ASSERT_TRUE(dmHttpService::AppendResponseBuffer(buffer, data, 1));
    ASSERT_EQ(1025U, buffer.Size());
    ASSERT_EQ(2048U, buffer.Capacity());
}

// Verifies irregular chunks, including chunks larger than doubled capacity, preserve binary data.
TEST(HttpResponseBuffer, IrregularAndLargeChunks)
{
    const uint32_t chunks[] = {1, 1023, 17, 8193, 0, 65536, 31};
    char data[65536];
    dmArray<char> buffer;
    uint32_t received = 0;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(chunks); ++i)
    {
        uint32_t chunk_size = chunks[i];
        for (uint32_t j = 0; j < chunk_size; ++j)
        {
            data[j] = (char)((received + j) % 251);
        }
        uint32_t previous_capacity = buffer.Capacity();
        ASSERT_TRUE(dmHttpService::AppendResponseBuffer(buffer, data, chunk_size));
        received += chunk_size;
        ASSERT_EQ(received, buffer.Size());
        ASSERT_GE(buffer.Capacity(), received);
        if (received > previous_capacity * 2 && received > 1024)
        {
            ASSERT_EQ(received, buffer.Capacity());
        }
    }
    for (uint32_t i = 0; i < received; ++i)
    {
        ASSERT_EQ((char)(i % 251), buffer[i]);
    }
}

// Verifies doubling saturates at the array's capacity limit instead of overflowing.
TEST(HttpResponseBuffer, CapacityLimit)
{
    ASSERT_EQ(0xfffffffeU, dmHttpService::GetResponseBufferCapacity(0x7fffffffU, 0x80000000ULL));
    ASSERT_EQ(UINT32_MAX, dmHttpService::GetResponseBufferCapacity(0x80000000U, 0x80000001ULL));
    ASSERT_EQ(UINT32_MAX, dmHttpService::GetResponseBufferCapacity(0xfffffffeU, (uint64_t)UINT32_MAX));
    ASSERT_EQ(0U, dmHttpService::GetResponseBufferCapacity(UINT32_MAX, (uint64_t)UINT32_MAX + 1));
    ASSERT_EQ(0U, dmHttpService::GetResponseBufferCapacity(UINT32_MAX, UINT64_MAX));
}

// Verifies oversized appends fail without allocating or modifying the existing response.
TEST(HttpResponseBuffer, RejectOversizedAppend)
{
    dmArray<char> buffer;
    const char byte = 'a';
    ASSERT_TRUE(dmHttpService::AppendResponseBuffer(buffer, &byte, 1));
    const char* storage = buffer.Begin();
    uint32_t capacity = buffer.Capacity();

    ASSERT_FALSE(dmHttpService::AppendResponseBuffer(buffer, &byte, UINT32_MAX));
    ASSERT_EQ(1U, buffer.Size());
    ASSERT_EQ(capacity, buffer.Capacity());
    ASSERT_EQ(storage, buffer.Begin());
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
