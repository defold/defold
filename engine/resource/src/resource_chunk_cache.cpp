// Copyright 2020-2025 The Defold Foundation
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

#include <dlib/static_assert.h>
#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>

#include <stdio.h>   // printf
#include <stddef.h>  // offsetof
#include <algorithm> // lower_bound

struct DLListNode
{
    struct DLListNode* m_Prev;
    struct DLListNode* m_Next;
};

struct DLList
{
    DLListNode m_Head;
    DLListNode m_Tail;
};

struct ResourceInternalDataChunk
{
    DLListNode m_ListNode; // must be first in the struct

    dmhash_t m_PathHash;
    uint8_t* m_Data;
    uint32_t m_Size;        // Size of data
    uint32_t m_Offset:31;   // Offset into the file
    uint32_t m_NoEvict:1;   // Set if the chunk mustn't be evicted
};

#if __cplusplus >= 201103L
    DM_STATIC_ASSERT(offsetof(ResourceInternalDataChunk, m_ListNode) == 0, "m_ListNode must be first in struct!");
#endif

struct ResourceChunkCache
{
    // The array is sorted on (path_hash, offset)
    dmArray<ResourceInternalDataChunk*> m_Chunks;

    uint32_t m_CacheSize;       // The cache size for all streaming sounds
    uint32_t m_CacheSizeUsed;   // The amount of cache currently used
    DLList   m_LRU;             // head is MRU, tail is LRU
    DLList   m_LRUNoEvict;      // head is MRU, tail is LRU
};

// **********************************************************************************

static void ListInit(DLList* list)
{
    list->m_Head.m_Prev = 0;
    list->m_Tail.m_Next = 0;
    list->m_Head.m_Next = &list->m_Tail;
    list->m_Tail.m_Prev = &list->m_Head;
}

static void ListRemove(DLList* list, DLListNode* item)
{
    DLListNode* prev = item->m_Prev;
    DLListNode* next = item->m_Next;
    item->m_Prev = 0;
    item->m_Next = 0;
    prev->m_Next = next;
    next->m_Prev = prev;
}

static void ListAdd(DLList* list, DLListNode* item)
{
    DLListNode* prev = &list->m_Head;
    DLListNode* next = prev->m_Next;
    item->m_Prev = prev;
    item->m_Next = next;
    prev->m_Next = item;
    next->m_Prev = item;
}

static DLListNode* ListGetLast(DLList* list)
{
    DLListNode* last = list->m_Tail.m_Prev;
    return last == &list->m_Head ? 0 : last;
}

// **********************************************************************************

static ResourceInternalDataChunk* AllocChunk(ResourceChunkCache* cache, dmhash_t path_hash, uint8_t* data, uint32_t data_size, uint32_t offset, int flags)
{
    ResourceInternalDataChunk* chunk = new ResourceInternalDataChunk;
    chunk->m_PathHash = path_hash;
    chunk->m_Offset = offset;
    chunk->m_Size = data_size;
    chunk->m_NoEvict = flags & RESOURCE_CHUNK_CACHE_NO_EVICT?1:0;
    chunk->m_Data = new uint8_t[data_size];
    memcpy(chunk->m_Data, data, data_size);

    cache->m_CacheSizeUsed += data_size;
    return chunk;
}

static void FreeChunk(ResourceChunkCache* cache, ResourceInternalDataChunk* chunk)
{
    cache->m_CacheSizeUsed -= chunk->m_Size;
    delete chunk->m_Data;
    delete chunk;
}

// **********************************************************************************

static int ChunkComparePathOffsetFn(const void* a, const void* b)
{
    ResourceInternalDataChunk* chunka = *(ResourceInternalDataChunk**) a;
    ResourceInternalDataChunk* chunkb = *(ResourceInternalDataChunk**) b;
    uint64_t patha = chunka->m_PathHash;
    uint64_t pathb = chunkb->m_PathHash;

    if (patha != pathb)
        return patha < pathb ? -1 : 1;

    uint32_t offseta = chunka->m_Offset;
    uint32_t offsetb = chunkb->m_Offset;
    return (int)offseta - (int)offsetb;
}

static void SortChunksOnPathAndOffset(ResourceChunkCache* cache)
{
    qsort(cache->m_Chunks.Begin(), cache->m_Chunks.Size(), sizeof(cache->m_Chunks[0]), ChunkComparePathOffsetFn);
}

static bool ChunkComparePathFn(const ResourceInternalDataChunk* chunk, dmhash_t path_hash)
{
    return chunk->m_PathHash < path_hash;
}

static void PrintChunk(ResourceChunkCache* cache, ResourceInternalDataChunk* chunk)
{
    printf("  offset: %8u  size: %8u  data: %p  noevict: %s  file: '%s'\n", chunk->m_Offset, chunk->m_Size, chunk->m_Data, chunk->m_NoEvict?"true":"false", dmHashReverseSafe64(chunk->m_PathHash));
}

static void PrintList(ResourceChunkCache* cache, DLList* list)
{
    DLListNode* item = list->m_Head.m_Next;
    DLListNode* end = &list->m_Tail;
    while (item != end)
    {
        ResourceInternalDataChunk* chunk = (ResourceInternalDataChunk*)item;
        PrintChunk(cache, chunk);

        item = item->m_Next;
    }
}

void ResourceChunkCacheDebugChunks(ResourceChunkCache* cache)
{
    uint32_t num_chunks = cache->m_Chunks.Size();
    printf("CACHE: size: %u  used: %u\n", cache->m_CacheSize, cache->m_CacheSizeUsed);
    printf("NUM CHUNKS: %u\n", num_chunks);
    for (uint32_t i = 0; i < num_chunks; ++i)
    {
        ResourceInternalDataChunk* chunk = cache->m_Chunks[i];
        PrintChunk(cache, chunk);
    }
    printf("LRU:\n");
    PrintList(cache, &cache->m_LRU);
    printf("LRU (no evict):\n");
    PrintList(cache, &cache->m_LRUNoEvict);
}

// **********************************************************************************

HResourceChunkCache ResourceChunkCacheCreate(uint32_t max_memory)
{
    ResourceChunkCache* cache = new ResourceChunkCache;
    cache->m_CacheSize = max_memory;
    cache->m_CacheSizeUsed = 0;

    ListInit(&cache->m_LRU);
    ListInit(&cache->m_LRUNoEvict);
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

static ResourceInternalDataChunk* ResourceChunkCacheFind(ResourceChunkCache* cache, dmhash_t path_hash, uint32_t offset)
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

static uint32_t ResourceChunkCacheFindIndex(ResourceChunkCache* cache, ResourceInternalDataChunk* _chunk)
{
    dmhash_t path_hash = _chunk->m_PathHash;
    uint32_t offset = _chunk->m_Offset;

    ResourceInternalDataChunk** begin = cache->m_Chunks.Begin();
    ResourceInternalDataChunk** end = cache->m_Chunks.End();
    ResourceInternalDataChunk** lbound = std::lower_bound(begin, end, path_hash, ChunkComparePathFn);
    if (lbound != end)
    {
        uint32_t start_index = lbound - begin;

        uint32_t num_chunks = cache->m_Chunks.Size();
        for (uint32_t i = start_index; i < num_chunks; ++i)
        {
            ResourceInternalDataChunk* chunk = begin[i];
            if (path_hash == chunk->m_PathHash && offset == chunk->m_Offset)
            {
                return i;
            }
        }
    }
    assert(false);
    return cache->m_Chunks.Size();
}


bool ResourceChunkCacheGet(HResourceChunkCache cache, dmhash_t path_hash, uint32_t offset, ResourceCacheChunk* out)
{
    ResourceInternalDataChunk** begin = cache->m_Chunks.Begin();
    ResourceInternalDataChunk** end = cache->m_Chunks.End();
    ResourceInternalDataChunk** lbound = std::lower_bound(begin, end, path_hash, ChunkComparePathFn);
    if (lbound == end)
        return false;

    uint32_t start_index = lbound - begin;

    // We assume the array is sorted on path and offset in ascending order
    uint32_t num_chunks = cache->m_Chunks.Size();
    for (uint32_t i = start_index; i < num_chunks; ++i)
    {
        ResourceInternalDataChunk* chunk = begin[i];
        dmhash_t chunk_path_hash = chunk->m_PathHash;
        uint32_t chunk_size = chunk->m_Size;

        if (path_hash < chunk_path_hash)
            return false;

        if (chunk_path_hash < path_hash)
            continue;

        // we got the correct file
        uint32_t chunk_offset = chunk->m_Offset;

        if (offset < chunk_offset)
            return false; // We currently don't have the chunk in question, and need to request it

        // The offset is within the bounds of the chunk
        if (offset < (chunk_offset + chunk_size))
        {
            out->m_Data = chunk->m_Data;
            out->m_Offset = chunk->m_Offset;
            out->m_Size = chunk_size;

            // Update the LRU by placing the item first in the queue
            DLList* list = &cache->m_LRU;
            if (chunk->m_NoEvict)
                list = &cache->m_LRUNoEvict;

            ListRemove(list, (DLListNode*)chunk);
            ListAdd(list, (DLListNode*)chunk);
            return true;
        }
    }
    return false;
}

// Stores a new resoruce chunk
bool ResourceChunkCachePut(HResourceChunkCache cache, uint64_t path_hash, int flags, ResourceCacheChunk* _chunk)
{
    if (!ResourceChunkCacheCanFit(cache, _chunk->m_Size))
    {
        dmLogError("Cache is full. Failed to add chunk: '%s' size: %u, offset: %u", dmHashReverseSafe64(path_hash), _chunk->m_Size, _chunk->m_Offset);
        return false;
    }

    ResourceInternalDataChunk* prev = ResourceChunkCacheFind(cache, path_hash, _chunk->m_Offset);
    if (prev)
    {
        dmLogError("Chunk already exists: '%s' size: %u, offset: %u", dmHashReverseSafe64(path_hash), prev->m_Size, prev->m_Offset);
        return false;
    }

    if (cache->m_Chunks.Full())
    {
        cache->m_Chunks.OffsetCapacity(16);
    }

    ResourceInternalDataChunk* chunk = AllocChunk(cache, path_hash, _chunk->m_Data, _chunk->m_Size, _chunk->m_Offset, flags);
    cache->m_Chunks.Push(chunk);
    SortChunksOnPathAndOffset(cache);

    DLList* list = &cache->m_LRU;
    if (flags & RESOURCE_CHUNK_CACHE_NO_EVICT)
        list = &cache->m_LRUNoEvict;
    ListAdd(list, (DLListNode*)chunk); // add it first in the LRU
    return true;
}

// Returns true if the cache can fit the chunk
bool ResourceChunkCacheCanFit(HResourceChunkCache cache, uint32_t size)
{
    //printf("  size: %u  m_CacheSize: %u  m_CacheSizeUsed: %u  -> %d\n", size, cache->m_CacheSize, cache->m_CacheSizeUsed, size <= (cache->m_CacheSize - cache->m_CacheSizeUsed));
    return size <= (cache->m_CacheSize - cache->m_CacheSizeUsed);
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

static void RemoveChunks(HResourceChunkCache cache, uint32_t index, uint32_t count)
{
    uint32_t num_chunks = cache->m_Chunks.Size();
    ResourceInternalDataChunk** begin = cache->m_Chunks.Begin();
    // Shift the elements in the array
    uint32_t num_to_move = num_chunks - (index + count);
    memmove(begin + index, begin + index + count, num_to_move * sizeof(ResourceInternalDataChunk*));
    cache->m_Chunks.SetSize(num_chunks - count);
}

bool ResourceChunkCacheEvictMemory(HResourceChunkCache cache, uint32_t size)
{
    DLList* list = &cache->m_LRU;
    while (!ResourceChunkCacheCanFit(cache, size))
    {
        DLListNode* last = ListGetLast(list);
        if (!last)
            break;

        ListRemove(list, last);

        ResourceInternalDataChunk* chunk = (ResourceInternalDataChunk*)last;
        uint32_t index = ResourceChunkCacheFindIndex(cache, chunk);
        RemoveChunks(cache, index, 1);
        FreeChunk(cache, chunk);
    }

    return ResourceChunkCacheCanFit(cache, size);
}

// Evicts the chunks associated with path_hash
void ResourceChunkCacheEvictPathHash(HResourceChunkCache cache, uint64_t path_hash)
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
        dmhash_t chunk_path_hash = chunk->m_PathHash;
        if (chunk_path_hash != path_hash)
            break;

        ++count;

        DLList* list = &cache->m_LRU;
        if (chunk->m_NoEvict)
            list = &cache->m_LRUNoEvict;

        ListRemove(list, (DLListNode*)chunk);
        FreeChunk(cache, chunk);
    }

    RemoveChunks(cache, start_index, count);
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
        assert(chunk);
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
