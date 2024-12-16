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
    const char* data = "Chunk 1\0Chunk 2\0Chunk 3\0";
    ResourceCacheChunk chunk1 = {(uint8_t*)data + 0, 0, 8};
    ResourceCacheChunk chunk2 = {(uint8_t*)data + 8, 8, 8};
    ResourceCacheChunk chunk3 = {(uint8_t*)data +16, 16, 8};
    uint64_t path_hash1 = dmHashString64("fileA");

    HResourceChunkCache cache = ResourceChunkCacheCreate(24, 8);

    // Empty cache
    ASSERT_NE((HResourceChunkCache)0, cache);
    ASSERT_EQ(0u, ResourceChunkCacheGetNumChunks(cache));
    ASSERT_FALSE(ResourceChunkCacheFull(cache));
    ASSERT_TRUE(ResourceChunkCacheVerify(cache));

    // First chunk
    ASSERT_TRUE(ResourceChunkCachePut(cache, path_hash1, &chunk1));
    ASSERT_EQ(1u, ResourceChunkCacheGetNumChunks(cache));
    ASSERT_FALSE(ResourceChunkCacheFull(cache));
    ASSERT_TRUE(ResourceChunkCacheVerify(cache));

    // Second chunk
    ASSERT_TRUE(ResourceChunkCachePut(cache, path_hash1, &chunk2));
    ASSERT_EQ(2u, ResourceChunkCacheGetNumChunks(cache));
    ASSERT_FALSE(ResourceChunkCacheFull(cache));
    ASSERT_TRUE(ResourceChunkCacheVerify(cache));

    // Third chunk
    ASSERT_TRUE(ResourceChunkCachePut(cache, path_hash1, &chunk3));
    ASSERT_EQ(3u, ResourceChunkCacheGetNumChunks(cache));
    ASSERT_TRUE(ResourceChunkCacheFull(cache));
    ASSERT_TRUE(ResourceChunkCacheVerify(cache));

    // Same chunk
    ASSERT_FALSE(ResourceChunkCachePut(cache, path_hash1, &chunk3));
    ASSERT_EQ(3u, ResourceChunkCacheGetNumChunks(cache));
    ASSERT_TRUE(ResourceChunkCacheFull(cache));
    ASSERT_TRUE(ResourceChunkCacheVerify(cache));

    ResourceChunkCacheDebugChunks(cache);

    // Second chunk
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
