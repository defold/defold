// Copyright 2026 The Defold Foundation
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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new>
#include <dlib/array.h>
#include <flecs.h>
#include "benchmark_memory.h"

union AllocationHeader
{
    max_align_t m_Alignment;
    struct
    {
        size_t                m_Size;
        BenchmarkMemoryDomain m_Domain;
    } m_Data;
};

static BenchmarkMemoryDomain g_Domain;
static BenchmarkMemoryStats  g_Memory[BENCHMARK_MEMORY_DOMAIN_COUNT];
static BenchmarkMemorySample g_Sample;

static void                  Require(bool condition, const char* message)
{
    if (!condition)
    {
        fprintf(stderr, "Memory benchmark: %s\n", message);
        abort();
    }
}

static void UpdatePeak(BenchmarkMemoryStats* stats)
{
    if (stats->m_Bytes > stats->m_PeakBytes)
        stats->m_PeakBytes = stats->m_Bytes;
    if (stats->m_Blocks > stats->m_PeakBlocks)
        stats->m_PeakBlocks = stats->m_Blocks;
}

static void* Allocate(size_t size, bool reallocation = false)
{
    Require(size <= SIZE_MAX - sizeof(AllocationHeader), "allocation size overflow");
    AllocationHeader* header = (AllocationHeader*)malloc(sizeof(AllocationHeader) + size);
    Require(header != 0, "allocation failed");
    header->m_Data.m_Size = size;
    header->m_Data.m_Domain = g_Domain;
    if (g_Domain)
    {
        BenchmarkMemoryStats* stats = &g_Memory[g_Domain];
        stats->m_Bytes += size;
        ++stats->m_Blocks;
        stats->m_Allocations += !reallocation;
        stats->m_Reallocations += reallocation;
        stats->m_AllocatedBytes += size;
        UpdatePeak(stats);
    }
    return header + 1;
}

static void Release(void* ptr)
{
    if (!ptr)
        return;
    AllocationHeader*     header = (AllocationHeader*)ptr - 1;
    BenchmarkMemoryDomain domain = header->m_Data.m_Domain;
    if (domain)
    {
        BenchmarkMemoryStats* stats = &g_Memory[domain];
        Require(stats->m_Blocks && stats->m_Bytes >= header->m_Data.m_Size, "invalid release accounting");
        stats->m_Bytes -= header->m_Data.m_Size;
        --stats->m_Blocks;
        ++stats->m_Frees;
    }
    free(header);
}

// Realloc stays attributed to its original owner even when freed in another phase.
// Peak bytes track observable requested payloads, not the allocator's temporary copy.
static void* Reallocate(void* ptr, size_t size)
{
    if (!ptr)
        return Allocate(size, true);
    AllocationHeader*     header = (AllocationHeader*)ptr - 1;
    BenchmarkMemoryDomain domain = header->m_Data.m_Domain;
    size_t                old_size = header->m_Data.m_Size;
    if (!size)
    {
        if (domain)
            ++g_Memory[domain].m_Reallocations;
        Release(ptr);
        return 0;
    }
    Require(size <= SIZE_MAX - sizeof(AllocationHeader), "reallocation size overflow");
    header = (AllocationHeader*)realloc(header, sizeof(AllocationHeader) + size);
    Require(header != 0, "reallocation failed");
    header->m_Data.m_Size = size;
    if (domain)
    {
        BenchmarkMemoryStats* stats = &g_Memory[domain];
        stats->m_Bytes = stats->m_Bytes - old_size + size;
        ++stats->m_Reallocations;
        stats->m_AllocatedBytes += size;
        UpdatePeak(stats);
    }
    return header + 1;
}

void* operator new(size_t size)
{
    return Allocate(size);
}
void* operator new[](size_t size)
{
    return Allocate(size);
}
void operator delete(void* ptr) noexcept
{
    Release(ptr);
}
void operator delete[](void* ptr) noexcept
{
    Release(ptr);
}
void operator delete(void* ptr, size_t) noexcept
{
    Release(ptr);
}
void operator delete[](void* ptr, size_t) noexcept
{
    Release(ptr);
}

static void* FlecsMalloc(ecs_size_t size)
{
    Require(size >= 0, "negative Flecs allocation");
    return Allocate((size_t)size);
}

static void* FlecsCalloc(ecs_size_t size)
{
    void* ptr = FlecsMalloc(size);
    memset(ptr, 0, (size_t)size);
    return ptr;
}

static void* FlecsRealloc(void* ptr, ecs_size_t size)
{
    Require(size >= 0, "negative Flecs reallocation");
    return Reallocate(ptr, (size_t)size);
}

static char* FlecsStrdup(const char* string)
{
    size_t size = strlen(string) + 1;
    char*  copy = (char*)Allocate(size);
    memcpy(copy, string, size);
    return copy;
}

void SetBenchmarkMemoryDomain(BenchmarkMemoryDomain domain)
{
    g_Domain = domain;
}

void BeginBenchmarkMemoryBackend()
{
    Require(!g_Memory[BENCHMARK_MEMORY_BACKEND].m_Blocks, "backend allocations survived teardown");
    g_Memory[BENCHMARK_MEMORY_BACKEND] = BenchmarkMemoryStats();
    g_Domain = BENCHMARK_MEMORY_BACKEND;
}

void EndBenchmarkMemoryBackend()
{
    Require(!g_Memory[BENCHMARK_MEMORY_BACKEND].m_Blocks && !g_Memory[BENCHMARK_MEMORY_BACKEND].m_Bytes, "backend allocations survived teardown");
    g_Domain = BENCHMARK_MEMORY_NONE;
}

void BeginBenchmarkMemoryOperation()
{
    BenchmarkMemoryStats* stats = &g_Memory[BENCHMARK_MEMORY_BACKEND];
    stats->m_PeakBytes = stats->m_Bytes;
    stats->m_PeakBlocks = stats->m_Blocks;
    g_Sample.m_Before = *stats;
}

void EndBenchmarkMemoryOperation()
{
    g_Sample.m_After = g_Memory[BENCHMARK_MEMORY_BACKEND];
    g_Sample.m_Fixture = g_Memory[BENCHMARK_MEMORY_FIXTURE];
    g_Sample.m_Resource = g_Memory[BENCHMARK_MEMORY_RESOURCE];
}

BenchmarkMemorySample GetBenchmarkMemorySample()
{
    return g_Sample;
}

void CheckBenchmarkMemoryReleased()
{
    for (uint32_t i = BENCHMARK_MEMORY_BACKEND; i < BENCHMARK_MEMORY_DOMAIN_COUNT; ++i)
        Require(!g_Memory[i].m_Blocks && !g_Memory[i].m_Bytes, "tracked allocations survived benchmark");
}

void InitializeBenchmarkMemory()
{
    ecs_os_set_api_defaults();
    ecs_os_api_t api = ecs_os_get_api();
    api.malloc_ = FlecsMalloc;
    api.calloc_ = FlecsCalloc;
    api.realloc_ = FlecsRealloc;
    api.free_ = Release;
    api.strdup_ = FlecsStrdup;
    ecs_os_set_api(&api);

    // Exercise allocator arithmetic and ownership before trusting benchmark output.
    BeginBenchmarkMemoryBackend();
    BeginBenchmarkMemoryOperation();
    uint8_t* first = (uint8_t*)api.malloc_(16);
    uint8_t* second = (uint8_t*)api.calloc_(24);
    Require(!second[0] && !second[23], "calloc did not zero memory");
    first[0] = 42;
    first = (uint8_t*)api.realloc_(first, 64);
    Require(first[0] == 42, "realloc did not preserve bytes");
    first = (uint8_t*)api.realloc_(first, 8);
    SetBenchmarkMemoryDomain(BENCHMARK_MEMORY_FIXTURE);
    api.free_(first);
    api.free_(second);
    EndBenchmarkMemoryOperation();
    BenchmarkMemoryStats stats = g_Sample.m_After;
    Require(!stats.m_Bytes && !stats.m_Blocks && stats.m_PeakBytes == 88 && stats.m_PeakBlocks == 2, "live/peak accounting");
    Require(stats.m_Allocations == 2 && stats.m_Reallocations == 2 && stats.m_Frees == 2 && stats.m_AllocatedBytes == 112, "allocation call accounting");
    EndBenchmarkMemoryBackend();

    BeginBenchmarkMemoryBackend();
    {
        dmArray<uint8_t> array;
        array.SetCapacity(32);
        Require(g_Memory[BENCHMARK_MEMORY_BACKEND].m_Bytes == 32, "dlib allocations were not intercepted");
        array.SetCapacity(64);
        Require(g_Memory[BENCHMARK_MEMORY_BACKEND].m_Bytes == 64 && g_Memory[BENCHMARK_MEMORY_BACKEND].m_PeakBytes == 96, "array growth peak");
        Require(g_Memory[BENCHMARK_MEMORY_BACKEND].m_Allocations == 2 && g_Memory[BENCHMARK_MEMORY_BACKEND].m_Frees == 1, "array allocation calls");
    }
    EndBenchmarkMemoryBackend();
    BeginBenchmarkMemoryBackend();
    void* ptr = api.realloc_(0, 7);
    Require(api.realloc_(ptr, 0) == 0, "zero-size realloc");
    char* string = api.strdup_("abc");
    Require(!strcmp(string, "abc"), "strdup contents");
    api.free_(string);
    void* scalar = ::operator new(11);
    ::operator delete(scalar);
    void* array = ::operator new[](13);
    ::operator delete[](array);
    Require(g_Memory[BENCHMARK_MEMORY_BACKEND].m_Allocations == 3 && g_Memory[BENCHMARK_MEMORY_BACKEND].m_Reallocations == 2 && g_Memory[BENCHMARK_MEMORY_BACKEND].m_Frees == 4, "allocator entry points");
    EndBenchmarkMemoryBackend();
    CheckBenchmarkMemoryReleased();
}
