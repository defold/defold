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

#ifndef DM_RESOURCE_CHUNK_CACHE_H
#define DM_RESOURCE_CHUNK_CACHE_H

#include <stdint.h>
#include <dlib/hash.h> // dmhash_t

struct ResourceCacheChunk
{
    uint8_t* m_Data;
    uint32_t m_Offset;
    uint32_t m_Size;
};

enum ResourceChunkCacheFlags
{
    RESOURCE_CHUNK_CACHE_DEFAULT     = 0,
    RESOURCE_CHUNK_CACHE_NO_EVICT    = 1,
};

typedef struct ResourceChunkCache* HResourceChunkCache;

HResourceChunkCache ResourceChunkCacheCreate(uint32_t max_memory);
void                ResourceChunkCacheDestroy(HResourceChunkCache cache);

// Get the chunk that contains the requested offset
// Also updates the timestamp
bool ResourceChunkCacheGet(HResourceChunkCache cache, dmhash_t path_hash, uint32_t offset, ResourceCacheChunk* out);

// Stores a new resoruce chunk. Returns true if successful, false if the operation failed
// Flags are a set of ResourceChunkCacheFlags
bool ResourceChunkCachePut(HResourceChunkCache cache, dmhash_t path_hash, int flags, ResourceCacheChunk* chunk);

// Returns true if the cache is full
bool ResourceChunkCacheCanFit(HResourceChunkCache cache, uint32_t size);

// Evicts the chunks until the requested size will fit
// It will not evict chunks added with the no-evict flag
bool ResourceChunkCacheEvictMemory(HResourceChunkCache cache, uint32_t size);

// Evicts the chunks associated with path_hash
void ResourceChunkCacheEvictPathHash(HResourceChunkCache cache, dmhash_t path_hash);

// Returns number of bytes used by the cache
uint32_t ResourceChunkCacheGetUsedMemory(HResourceChunkCache cache);

// *************************************************************************************
// Unit test functions

// Returns number of chunks stored (unit test only)
uint32_t ResourceChunkCacheGetNumChunks(HResourceChunkCache cache);

// Verifies the stored chunks (unit test only)
bool ResourceChunkCacheVerify(HResourceChunkCache cache);

// Prints out all chunks
void ResourceChunkCacheDebugChunks(HResourceChunkCache cache);

#endif // DM_RESOURCE_CHUNK_CACHE_H
