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

// Benchmark-only C++ allocation accounting. Header ownership follows allocations
// across threads. Counters use an allocation-free lock; timing uses another binary.
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <new>
#include <dlib/atomic.h>
#include "benchmark_data_threaded.h"
#include "benchmark_data_threaded_memory.h"

union ThreadAllocation
{
    max_align_t m_Alignment;
    struct
    {
        size_t             m_Size;
        ThreadMemoryDomain m_Domain;
    } m_Data;
};

static int32_atomic_t                  g_Lock;
static thread_local ThreadMemoryDomain g_Domain = THREAD_MEMORY_JOBS;
static ThreadMemoryStats               g_Stats[THREAD_MEMORY_DOMAIN_COUNT];
static ThreadMemorySample              g_Sample;
static uint64_t                        g_TotalBytes, g_StartBytes, g_PeakBytes;

static void                            Lock()
{
    while (dmAtomicCompareStore32(&g_Lock, 1, 0) != 0)
    {
    }
}
static void Unlock()
{
    dmAtomicCompareStore32(&g_Lock, 0, 1);
}

static void* Allocate(size_t size)
{
    ThreadedCheck(size <= SIZE_MAX - sizeof(ThreadAllocation), "allocation overflow");
    ThreadAllocation* header = (ThreadAllocation*)malloc(sizeof(ThreadAllocation) + size);
    ThreadedCheck(header != 0, "allocation failed");
    header->m_Data.m_Size = size;
    header->m_Data.m_Domain = g_Domain;
    if (g_Domain != THREAD_MEMORY_NONE)
    {
        Lock();
        ThreadMemoryStats* stats = &g_Stats[g_Domain];
        stats->m_Bytes += size;
        ++stats->m_Blocks;
        ++stats->m_Requests;
        g_TotalBytes += size;
        if (g_TotalBytes > g_PeakBytes)
            g_PeakBytes = g_TotalBytes;
        Unlock();
    }
    return header + 1;
}

static void Release(void* pointer)
{
    if (!pointer)
        return;
    ThreadAllocation* header = (ThreadAllocation*)pointer - 1;
    if (header->m_Data.m_Domain != THREAD_MEMORY_NONE)
    {
        Lock();
        ThreadMemoryStats* stats = &g_Stats[header->m_Data.m_Domain];
        ThreadedCheck(stats->m_Blocks && stats->m_Bytes >= header->m_Data.m_Size, "allocation accounting underflow");
        stats->m_Bytes -= header->m_Data.m_Size;
        --stats->m_Blocks;
        g_TotalBytes -= header->m_Data.m_Size;
        Unlock();
    }
    free(header);
}

void* operator new(size_t size)
{
    return Allocate(size);
}
void* operator new[](size_t size)
{
    return Allocate(size);
}
void operator delete(void* pointer) noexcept
{
    Release(pointer);
}
void operator delete[](void* pointer) noexcept
{
    Release(pointer);
}
void operator delete(void* pointer, size_t) noexcept
{
    Release(pointer);
}
void operator delete[](void* pointer, size_t) noexcept
{
    Release(pointer);
}

ThreadMemoryDomain ThreadMemorySetDomain(ThreadMemoryDomain domain)
{
    ThreadMemoryDomain previous = g_Domain;
    g_Domain = domain;
    return previous;
}

void ThreadMemoryBegin()
{
    Lock();
    memcpy(g_Sample.m_Before, g_Stats, sizeof(g_Stats));
    g_StartBytes = g_PeakBytes = g_TotalBytes;
    Unlock();
}

ThreadMemorySample ThreadMemoryEnd()
{
    Lock();
    memcpy(g_Sample.m_After, g_Stats, sizeof(g_Stats));
    g_Sample.m_PeakAdditional = g_PeakBytes - g_StartBytes;
    ThreadMemorySample sample = g_Sample;
    Unlock();
    return sample;
}

void ThreadMemoryCheckReleased()
{
    Lock();
    for (uint32_t i = 1; i < THREAD_MEMORY_DOMAIN_COUNT; ++i)
        ThreadedCheck(!g_Stats[i].m_Bytes && !g_Stats[i].m_Blocks, "tracked allocation survived teardown");
    Unlock();
}

void ThreadMemorySelfTest()
{
    ThreadMemoryScope store(THREAD_MEMORY_STORE);
    ThreadMemoryBegin();
    void* first = ::operator new(16);
    void* second = ::operator new[](32);
    ThreadMemorySetDomain(THREAD_MEMORY_JOBS);
    ::operator delete(first);
    ThreadMemorySample sample = ThreadMemoryEnd();
    ThreadedCheck(sample.m_After[THREAD_MEMORY_STORE].m_Bytes - sample.m_Before[THREAD_MEMORY_STORE].m_Bytes == 32 && sample.m_PeakAdditional == 48, "memory peak and retained bytes");
    ThreadedCheck(sample.m_After[THREAD_MEMORY_STORE].m_Requests - sample.m_Before[THREAD_MEMORY_STORE].m_Requests == 2, "allocation request count");
    ::operator delete[](second);
    ThreadMemoryCheckReleased();
}

#ifdef DATA_THREADED_FLECS
#include <flecs.h>
static void* FlecsMalloc(ecs_size_t size)
{
    ThreadMemoryScope store(THREAD_MEMORY_STORE);
    ThreadedCheck(size >= 0, "Flecs allocation size");
    return Allocate(size);
}
static void* FlecsCalloc(ecs_size_t size)
{
    void* value = FlecsMalloc(size);
    memset(value, 0, size);
    return value;
}
static void* FlecsRealloc(void* pointer, ecs_size_t size)
{
    if (!pointer)
        return FlecsMalloc(size);
    ThreadedCheck(size >= 0, "Flecs reallocation size");
    ThreadAllocation*  header = (ThreadAllocation*)pointer - 1;
    size_t             old_size = header->m_Data.m_Size;
    ThreadMemoryDomain domain = header->m_Data.m_Domain;
    header = (ThreadAllocation*)realloc(header, sizeof(ThreadAllocation) + size);
    ThreadedCheck(header != 0, "Flecs realloc failed");
    header->m_Data.m_Size = size;
    if (domain != THREAD_MEMORY_NONE)
    {
        Lock();
        g_Stats[domain].m_Bytes = g_Stats[domain].m_Bytes - old_size + size;
        ++g_Stats[domain].m_Requests;
        g_TotalBytes = g_TotalBytes - old_size + size;
        if (g_TotalBytes > g_PeakBytes)
            g_PeakBytes = g_TotalBytes;
        Unlock();
    }
    return header + 1;
}
static char* FlecsStrdup(const char* string)
{
    size_t size = strlen(string) + 1;
    char*  value = (char*)FlecsMalloc((ecs_size_t)size);
    memcpy(value, string, size);
    return value;
}
void ThreadMemoryInstallFlecs()
{
    ecs_os_set_api_defaults();
    ecs_os_api_t api = ecs_os_api;
    api.malloc_ = FlecsMalloc;
    api.calloc_ = FlecsCalloc;
    api.realloc_ = FlecsRealloc;
    api.free_ = Release;
    api.strdup_ = FlecsStrdup;
    ecs_os_set_api(&api);
}
#endif
