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

#include "resource_chunk_cache.h"

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/time.h>

#include <stdio.h>   // printf
#include <algorithm> // lower_bound

// struct ResourceCacheChunk
// {
//     uint8_t* m_Data;
//     uint32_t m_Offset;
//     uint32_t m_Size;
// };

struct ResourceInternalDataChunk
{
    uint64_t m_Time;    // Last timestamp when it was read
    uint64_t m_PathHash;
    uint32_t m_Size;    // Size of the chunk
    uint32_t m_Offset;  // Offset into the file
    uint8_t* m_Data;
};

struct ResourceChunkCache
{
    // The array is sorted on (path_hash, offset)
    dmArray<ResourceInternalDataChunk*> m_Chunks;

    uint32_t m_CacheSize;    // The cache size for all streaming sounds
    uint32_t m_ChunkSize;    // The chunk size when streaming in more data
    uint32_t m_CacheSizeUsed;// The amount of cache currently used
};

// **********************************************************************************

static ResourceInternalDataChunk* AllocChunk(ResourceChunkCache* cache, uint64_t path_hash, uint8_t* data, uint32_t data_size, uint32_t offset)
{
    ResourceInternalDataChunk* chunk = new ResourceInternalDataChunk;
    chunk->m_PathHash = path_hash;
    chunk->m_Size   = data_size;
    chunk->m_Offset = offset;
    chunk->m_Data = new uint8_t[data_size];
    memcpy(chunk->m_Data, data, data_size);

    cache->m_CacheSizeUsed += chunk->m_Size;
    return chunk;
}

static void FreeChunk(ResourceChunkCache* cache, ResourceInternalDataChunk* chunk)
{
    cache->m_CacheSizeUsed -= chunk->m_Size;
    delete chunk->m_Data;
    delete chunk;
}

static int ChunkComparePathOffsetFn(const void* a, const void* b)
{
    ResourceInternalDataChunk* chunka = *(ResourceInternalDataChunk**) a;
    ResourceInternalDataChunk* chunkb = *(ResourceInternalDataChunk**) b;
    uint64_t patha = chunka->m_PathHash;
    uint64_t pathb = chunkb->m_PathHash;

    // printf(" cmp\n");
    // printf("   chunka: %p  %s %llu  off: %u \n", chunka, dmHashReverseSafe64(chunka->m_PathHash), chunka->m_PathHash, chunka->m_Offset);
    // printf("   chunkb: %p  %s %llu  off: %u \n", chunkb, dmHashReverseSafe64(chunkb->m_PathHash), chunkb->m_PathHash, chunkb->m_Offset);

    if (patha != pathb)
        return patha < pathb ? -1 : 1;

    uint32_t offseta = chunka->m_Offset;
    uint32_t offsetb = chunkb->m_Offset;
    return (int)offseta - (int)offsetb;
}

static void SortChunksOnPathAndOffset(ResourceChunkCache* cache)
{
    // ResourceChunkCacheDebugChunks(cache);
    // printf("COMPARE: %u\n", cache->m_Chunks.Size());
    qsort(cache->m_Chunks.Begin(), cache->m_Chunks.Size(), sizeof(cache->m_Chunks[0]), ChunkComparePathOffsetFn);
}

static bool ChunkComparePathFn(const ResourceInternalDataChunk* chunk, const uint64_t path_hash)
{
    //printf(" cmp path: %s %llu == %s %llu \n", dmHashReverseSafe64(chunk->m_PathHash), chunk->m_PathHash, dmHashReverseSafe64(path_hash), path_hash);
    return chunk->m_PathHash < path_hash;
}

static void RemoveChunk(ResourceChunkCache* cache, ResourceInternalDataChunk* chunk)
{
}

void ResourceChunkCacheDebugChunks(ResourceChunkCache* cache)
{
    uint32_t num_chunks = cache->m_Chunks.Size();
    printf("NUM CHUNKS: %u\n", num_chunks);
    for (uint32_t i = 0; i < num_chunks; ++i)
    {
        ResourceInternalDataChunk* chunk = cache->m_Chunks[i];
        printf("  chunk %4u: offset: %8u  size: %8u  data: %p '%s' %p\n", i, chunk->m_Offset, chunk->m_Size, chunk->m_Data, dmHashReverseSafe64(chunk->m_PathHash), chunk);
    }
}

// **********************************************************************************

HResourceChunkCache ResourceChunkCacheCreate(uint32_t max_memory, uint32_t chunk_size)
{
    ResourceChunkCache* cache = new ResourceChunkCache;
    cache->m_Chunks.SetCapacity(max_memory / chunk_size);
    cache->m_CacheSize = max_memory;
    cache->m_ChunkSize = chunk_size;
    cache->m_CacheSizeUsed = 0;
    return cache;
}

void ResourceChunkCacheDestroy(HResourceChunkCache cache)
{
    uint32_t num_chunks = cache->m_Chunks.Size();
    for (uint32_t i = 0; i < num_chunks; ++i)
    {
        FreeChunk(cache, cache->m_Chunks[i]);
    }
    delete cache;
}

// **********************************************************************************

static ResourceInternalDataChunk* ResourceChunkCacheFind(ResourceChunkCache* cache, uint64_t path_hash, uint32_t offset)
{
    ResourceInternalDataChunk** begin = cache->m_Chunks.Begin();
    ResourceInternalDataChunk** end = cache->m_Chunks.End();
    ResourceInternalDataChunk** lbound = std::lower_bound(begin, end, path_hash, ChunkComparePathFn);
    if (lbound != end)
    {
        ResourceInternalDataChunk* chunk = *lbound;
        if (chunk->m_PathHash == path_hash && chunk->m_Offset == offset)
            return chunk;
    }
    return 0;
}

ResourceInternalDataChunk* ResourceChunkCacheGet(ResourceChunkCache* cache, uint32_t path_hash, uint32_t offset)
{
    ResourceInternalDataChunk** begin = cache->m_Chunks.Begin();
    ResourceInternalDataChunk** end = cache->m_Chunks.End();
    ResourceInternalDataChunk** lbound = std::lower_bound(begin, end, path_hash, ChunkComparePathFn);
    if (lbound != end)
        return 0;

    uint32_t start_index = lbound - begin;

    // We assume the array is sorted on path and offset in ascending order
    uint32_t num_chunks = cache->m_Chunks.Size();
    for (uint32_t i = start_index; i < num_chunks; ++i)
    {
        ResourceInternalDataChunk* chunk = begin[i];
        uint32_t chunk_path_hash = chunk->m_PathHash;

        if (path_hash < chunk_path_hash)
            return 0;

        if (chunk_path_hash < path_hash)
            continue;

        // we got the correct file
        uint32_t chunk_offset = chunk->m_Offset;
        uint32_t chunk_size = chunk->m_Size;

        if (offset < chunk_offset)
            return 0; // We currently don't have the chunk in question, and need to request it

        if (offset < (chunk_offset + chunk_size))
            return chunk; // The offset is within the bounds of the chunk
    }
    return 0;
}

// Stores a new resoruce chunk
bool ResourceChunkCachePut(HResourceChunkCache cache, uint64_t path_hash, ResourceCacheChunk* _chunk)
{
//printf("put '%s': '%s' sz: %u  off: %u\n", dmHashReverseSafe64(path_hash), (const char*)_chunk->m_Data, _chunk->m_Size, _chunk->m_Offset);
    if (ResourceChunkCacheFull(cache))
    {
        dmLogWarning("Cache is full. Failed to add chunk: '%s' size: %u, offset: %u", dmHashReverseSafe64(path_hash), _chunk->m_Size, _chunk->m_Offset);
        return false;
    }

    ResourceInternalDataChunk* prev = ResourceChunkCacheFind(cache, path_hash, _chunk->m_Offset);
    if (prev)
    {
        dmLogError("Chunk already exists: '%s' size: %u, offset: %u", dmHashReverseSafe64(path_hash), prev->m_Size, prev->m_Offset);
        return false;
    }

    ResourceInternalDataChunk* chunk = AllocChunk(cache, path_hash, _chunk->m_Data, _chunk->m_Size, _chunk->m_Offset);
    cache->m_Chunks.Push(chunk);
    SortChunksOnPathAndOffset(cache);
    return true;
}

// Returns true if the cache is full
bool ResourceChunkCacheFull(HResourceChunkCache cache)
{
    return cache->m_Chunks.Full();
}

// Returns number of bytes used by the cache
uint32_t ResourceChunkCacheGetUsedMemory(HResourceChunkCache cache)
{
    return cache->m_CacheSizeUsed;
}

// Returns number of chunks stored (unit test only)
uint32_t ResourceChunkCacheGetNumChunks(HResourceChunkCache cache)
{
    return cache->m_Chunks.Size();
}

// Evicts the least recently used item from the cache
void ChunkCacheEvictOne(HResourceChunkCache cache)
{
    assert(false);
}

// Evicts the chunks associated with path_hash
void ChunkCacheEvictPathHash(HResourceChunkCache cache, uint64_t path_hash)
{
    ResourceInternalDataChunk** begin = cache->m_Chunks.Begin();
    ResourceInternalDataChunk** end = cache->m_Chunks.End();
    ResourceInternalDataChunk** lbound = std::lower_bound(begin, end, path_hash, ChunkComparePathFn);
    if (!lbound)
        return;

    uint32_t count = 0;
    uint32_t start_index = lbound - begin;
    uint32_t num_chunks = cache->m_Chunks.Size();
    for (uint32_t i = start_index; i < num_chunks; ++i)
    {
        ResourceInternalDataChunk* chunk = begin[i];
        uint32_t chunk_path_hash = chunk->m_PathHash;
        if (chunk_path_hash != path_hash)
            break;

        FreeChunk(cache, chunk);
    }

    // Shift the elements in the array
    uint32_t num_to_move = num_chunks - (start_index + count);
    memmove(begin + start_index + count, begin + start_index, num_to_move * sizeof(ResourceInternalDataChunk*));
}

// Unit test only
bool ResourceChunkCacheVerify(HResourceChunkCache cache)
{
    // Verify the sorting of the chunks
    ResourceInternalDataChunk** chunks = cache->m_Chunks.Begin();
    uint32_t num_chunks = cache->m_Chunks.Size();
    uint64_t prev_hash = 0;
    uint32_t prev_offset = 0;
    for (uint32_t i = 0; i < num_chunks; ++i)
    {
        ResourceInternalDataChunk* chunk = chunks[i];
        uint64_t chunk_path_hash = chunk->m_PathHash;
        uint32_t chunk_offset = chunk->m_Offset;
        if (prev_hash != chunk_path_hash)
            prev_offset = 0;

        if (prev_hash > chunk_path_hash || prev_offset > chunk_offset)
        {
            dmLogError("Chunk %u is out of order", i);
            ResourceChunkCacheDebugChunks(cache);
            return false;
        }
        prev_hash = chunk_path_hash;
        prev_offset = chunk_offset;
    }
    return true;
}
