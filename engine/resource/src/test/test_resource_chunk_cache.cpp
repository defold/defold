// Copyright 2020-2024 The Defold Foundation
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

#include <stdio.h>
#include <stdint.h>

#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/testutil.h>

#include "../resource_chunk_cache.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

TEST(ResourceChunkCache, Small)
{
    uint32_t chunk_size = 8;
    const char* data = "Chunk 1\0Chunk 2\0Chunk 3\0";
    ResourceCacheChunk chunk1 = {(uint8_t*)data + chunk_size*0, chunk_size*0, chunk_size};
    ResourceCacheChunk chunk2 = {(uint8_t*)data + chunk_size*1, chunk_size*1, chunk_size};
    ResourceCacheChunk chunk3 = {(uint8_t*)data + chunk_size*2, chunk_size*2, chunk_size};
    uint64_t path_hash1 = dmHashString64("fileA");
    ResourceCacheChunk getter;

    HResourceChunkCache cache = ResourceChunkCacheCreate(3*chunk_size);

    // Empty cache
    ASSERT_NE((HResourceChunkCache)0, cache);
    ASSERT_EQ(0u, ResourceChunkCacheGetNumChunks(cache));
    ASSERT_TRUE(ResourceChunkCacheCanFit(cache, chunk_size*3));
    ASSERT_TRUE(ResourceChunkCacheVerify(cache));

    // First chunk
    ASSERT_TRUE(ResourceChunkCachePut(cache, path_hash1, &chunk1));
    ASSERT_EQ(1u, ResourceChunkCacheGetNumChunks(cache));
    ASSERT_FALSE(ResourceChunkCacheCanFit(cache, chunk_size*3));
    ASSERT_TRUE(ResourceChunkCacheCanFit(cache, chunk_size*2));
    ASSERT_TRUE(ResourceChunkCacheVerify(cache));

    // Read chunk 1
    memset(&getter, 0, sizeof(getter));
    ASSERT_TRUE(ResourceChunkCacheGet(cache, path_hash1, 0, &getter));
    ASSERT_EQ(0u, getter.m_Offset);
    ASSERT_EQ(8u, getter.m_Size);
    ASSERT_STREQ("Chunk 1", (const char*)getter.m_Data);

    ASSERT_TRUE(ResourceChunkCacheGet(cache, path_hash1, 7, &getter));
    ASSERT_EQ(0u, getter.m_Offset);
    ASSERT_EQ(8u, getter.m_Size);
    ASSERT_STREQ("Chunk 1", (const char*)getter.m_Data);

    ASSERT_FALSE(ResourceChunkCacheGet(cache, path_hash1, 8, &getter));
    ASSERT_FALSE(ResourceChunkCacheGet(cache, path_hash1, 16, &getter));

    // Second chunk
    ASSERT_TRUE(ResourceChunkCachePut(cache, path_hash1, &chunk2));
    ASSERT_EQ(2u, ResourceChunkCacheGetNumChunks(cache));
    ASSERT_FALSE(ResourceChunkCacheCanFit(cache, chunk_size*2));
    ASSERT_TRUE(ResourceChunkCacheCanFit(cache, chunk_size*1));
    ASSERT_TRUE(ResourceChunkCacheVerify(cache));

    memset(&getter, 0, sizeof(getter));
    ASSERT_TRUE(ResourceChunkCacheGet(cache, path_hash1, 8, &getter));
    ASSERT_EQ(8u, getter.m_Offset);
    ASSERT_EQ(8u, getter.m_Size);
    ASSERT_STREQ("Chunk 2", (const char*)getter.m_Data);

    ASSERT_FALSE(ResourceChunkCacheGet(cache, path_hash1, 16, &getter));

    // Third chunk
    ASSERT_TRUE(ResourceChunkCachePut(cache, path_hash1, &chunk3));
    ASSERT_EQ(3u, ResourceChunkCacheGetNumChunks(cache));
    ASSERT_FALSE(ResourceChunkCacheCanFit(cache, chunk_size*1));
    ASSERT_TRUE(ResourceChunkCacheVerify(cache));

    memset(&getter, 0, sizeof(getter));
    ASSERT_TRUE(ResourceChunkCacheGet(cache, path_hash1, 16, &getter));
    ASSERT_EQ(16u, getter.m_Offset);
    ASSERT_EQ(8u, getter.m_Size);
    ASSERT_STREQ("Chunk 3", (const char*)getter.m_Data);

    // Same chunk
    dmLogWarning("EXPECTED ERROR -->");
    ASSERT_FALSE(ResourceChunkCachePut(cache, path_hash1, &chunk3));
    dmLogWarning("<-- EXPECTED ERROR END");
    ASSERT_EQ(3u, ResourceChunkCacheGetNumChunks(cache));
    ASSERT_FALSE(ResourceChunkCacheCanFit(cache, chunk_size*1));
    ASSERT_TRUE(ResourceChunkCacheVerify(cache));

    // ************************************************************************
    // Evict memory

    // touch chunk 1 and 3
    ASSERT_TRUE(ResourceChunkCacheGet(cache, path_hash1, 16, &getter));
    ASSERT_TRUE(ResourceChunkCacheGet(cache, path_hash1, 0, &getter));

    ResourceChunkCacheDebugChunks(cache);

    ASSERT_EQ(chunk_size*3, ResourceChunkCacheGetUsedMemory(cache));

    // -> should evict chunk 2
    ASSERT_TRUE(ResourceChunkCacheEvictMemory(cache, chunk_size));

    ASSERT_EQ(chunk_size*2, ResourceChunkCacheGetUsedMemory(cache));

    ASSERT_FALSE(ResourceChunkCacheGet(cache, path_hash1, 8, &getter));
    ASSERT_EQ(2u, ResourceChunkCacheGetNumChunks(cache));
    ASSERT_TRUE(ResourceChunkCacheVerify(cache));

    // ************************************************************************
    // Evict with path

    ResourceChunkCacheEvictPathHash(cache, path_hash1);

    ASSERT_FALSE(ResourceChunkCacheGet(cache, path_hash1, 0, &getter));
    ASSERT_FALSE(ResourceChunkCacheGet(cache, path_hash1, 16, &getter));
    ASSERT_EQ(0u, ResourceChunkCacheGetNumChunks(cache));
    ASSERT_TRUE(ResourceChunkCacheVerify(cache));

    // Shutdown
    ASSERT_TRUE(ResourceChunkCacheVerify(cache));
    ResourceChunkCacheDestroy(cache);
}



TEST(ResourceChunkCache, Multiple)
{
    const uint32_t num_files = 3;
    const uint32_t num_chunks_per_file = 3;
    uint32_t chunk_size = 16;

    // Each data segment is 16 bytes for easy verification

    const char* data1 = "File1: Chunk: 1\0File1: Chunk: 2\0File1: Chunk: 3\0";
    ResourceCacheChunk f1ch1 = {(uint8_t*)data1 +chunk_size*0, chunk_size*0, chunk_size};
    ResourceCacheChunk f1ch2 = {(uint8_t*)data1 +chunk_size*1, chunk_size*1, chunk_size};
    ResourceCacheChunk f1ch3 = {(uint8_t*)data1 +chunk_size*2, chunk_size*2, chunk_size};
    uint64_t path_hash1 = dmHashString64("file1");

    const char* data2 = "File2: Chunk: 1\0File2: Chunk: 2\0File2: Chunk: 3\0";
    ResourceCacheChunk f2ch1 = {(uint8_t*)data2 +chunk_size*0, chunk_size*0, chunk_size};
    ResourceCacheChunk f2ch2 = {(uint8_t*)data2 +chunk_size*1, chunk_size*1, chunk_size};
    ResourceCacheChunk f2ch3 = {(uint8_t*)data2 +chunk_size*2, chunk_size*2, chunk_size};
    uint64_t path_hash2 = dmHashString64("file2");

    const char* data3 = "File3: Chunk: 1\0File3: Chunk: 2\0File3: Chunk: 3\0";
    ResourceCacheChunk f3ch1 = {(uint8_t*)data3 +chunk_size*0, chunk_size*0, chunk_size};
    ResourceCacheChunk f3ch2 = {(uint8_t*)data3 +chunk_size*1, chunk_size*1, chunk_size};
    ResourceCacheChunk f3ch3 = {(uint8_t*)data3 +chunk_size*2, chunk_size*2, chunk_size};
    uint64_t path_hash3 = dmHashString64("file3");


    // Let's only fit 3 chunks at the same time
    HResourceChunkCache cache = ResourceChunkCacheCreate(num_files*chunk_size);
    ASSERT_NE((HResourceChunkCache)0, cache);

    ResourceCacheChunk* expected_chunks[num_files][num_chunks_per_file] = {
        {&f1ch1, &f1ch2, &f1ch3},
        {&f2ch1, &f2ch2, &f2ch3},
        {&f3ch1, &f3ch2, &f3ch3},
    };

    dmhash_t path_hashes[] = {path_hash1, path_hash2, path_hash3};

    // We basically want to add chunks in a looping
    // fashion, and check that the collected data is the correct one
    for (uint32_t i = 0; i < 30; ++i)
    {
        uint32_t file_index = i % num_files;
        uint32_t chunk_index = (i / num_files) % num_chunks_per_file;

        dmhash_t path_hash = path_hashes[file_index];

        printf("********************************************\n");
        printf("LOOP: %u  file: %s  chunk: %u\n", i, dmHashReverseSafe64(path_hash), chunk_index);

        if (i < num_files)
        {
            ASSERT_EQ(chunk_size*i, ResourceChunkCacheGetUsedMemory(cache));
            ASSERT_TRUE(ResourceChunkCacheCanFit(cache, chunk_size*(num_files-i)));
        }
        else
        {
            ASSERT_FALSE(ResourceChunkCacheCanFit(cache, chunk_size));

            ResourceChunkCacheEvictMemory(cache, chunk_size);

            ASSERT_TRUE(ResourceChunkCacheCanFit(cache, chunk_size));
        }

        ResourceCacheChunk* expected_chunk = expected_chunks[file_index][chunk_index];
        ASSERT_TRUE(ResourceChunkCachePut(cache, path_hash, expected_chunk));

        ResourceChunkCacheDebugChunks(cache);

        uint32_t expected_num_chunks = i < num_files ? i+1 : num_files;
        ASSERT_EQ(expected_num_chunks, ResourceChunkCacheGetNumChunks(cache));

        if (expected_num_chunks == 3)
        {
            ASSERT_FALSE(ResourceChunkCacheCanFit(cache, chunk_size));
        }
        else
        {
            ASSERT_TRUE(ResourceChunkCacheCanFit(cache, chunk_size));
        }

        ResourceCacheChunk getter = {0};

        ASSERT_TRUE(ResourceChunkCacheGet(cache, path_hash, expected_chunk->m_Offset, &getter));
        ASSERT_EQ(expected_chunk->m_Offset, getter.m_Offset);
        ASSERT_EQ(expected_chunk->m_Size, getter.m_Size);
        ASSERT_STREQ((const char*)expected_chunk->m_Data, (const char*)getter.m_Data);
    }

    // Shutdown
    ASSERT_TRUE(ResourceChunkCacheVerify(cache));
    ResourceChunkCacheDestroy(cache);
}


extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    dmHashEnableReverseHash(true);
    dmLog::LogParams logparams;
    dmLog::LogInitialize(&logparams);

    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
