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

#ifndef DM_BENCHMARK_DATA_THREADED_MEMORY_H
#define DM_BENCHMARK_DATA_THREADED_MEMORY_H
#include <stdint.h>

enum ThreadMemoryDomain
{
    THREAD_MEMORY_NONE,
    THREAD_MEMORY_STORE,
    THREAD_MEMORY_JOBS,
    THREAD_MEMORY_CALLER,
    THREAD_MEMORY_RESOURCE,
    THREAD_MEMORY_DOMAIN_COUNT
};
struct ThreadMemoryStats
{
    uint64_t m_Bytes, m_Blocks, m_Requests;
};
struct ThreadMemorySample
{
    ThreadMemoryStats m_Before[THREAD_MEMORY_DOMAIN_COUNT], m_After[THREAD_MEMORY_DOMAIN_COUNT];
    uint64_t          m_PeakAdditional;
};
#ifdef DATA_THREADED_MEMORY
ThreadMemoryDomain ThreadMemorySetDomain(ThreadMemoryDomain domain);
void               ThreadMemoryBegin();
ThreadMemorySample ThreadMemoryEnd();
void               ThreadMemoryCheckReleased();
void               ThreadMemorySelfTest();
#else
static inline ThreadMemoryDomain ThreadMemorySetDomain(ThreadMemoryDomain)
{
    return THREAD_MEMORY_NONE;
}
static inline void               ThreadMemoryBegin() {}
static inline ThreadMemorySample ThreadMemoryEnd()
{
    ThreadMemorySample sample = {};
    return sample;
}
static inline void ThreadMemoryCheckReleased() {}
static inline void ThreadMemorySelfTest() {}
#endif
#ifdef DATA_THREADED_FLECS
#ifdef DATA_THREADED_MEMORY
void ThreadMemoryInstallFlecs();
#else
static inline void ThreadMemoryInstallFlecs() {}
#endif
#endif

struct ThreadMemoryScope
{
    ThreadMemoryDomain m_Previous;
    ThreadMemoryScope(ThreadMemoryDomain domain)
        : m_Previous(ThreadMemorySetDomain(domain))
    {
    }
    ~ThreadMemoryScope()
    {
        ThreadMemorySetDomain(m_Previous);
    }
};
#endif
