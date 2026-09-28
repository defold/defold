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

#ifndef DATA_BENCHMARK_MEMORY_H
#define DATA_BENCHMARK_MEMORY_H

#include <stdint.h>

// Benchmark-only accounting. Ordinary C++ new/delete and Flecs OS allocator hooks
// count requested payload bytes, excluding tracking headers and allocator overhead.
// Single-threaded, like the benchmark. Allocation ownership survives domain changes.
enum BenchmarkMemoryDomain
{
    BENCHMARK_MEMORY_NONE,
    BENCHMARK_MEMORY_BACKEND,
    BENCHMARK_MEMORY_FIXTURE,
    BENCHMARK_MEMORY_RESOURCE,
    BENCHMARK_MEMORY_DOMAIN_COUNT
};

#ifdef DATA_BENCHMARK_MEMORY
struct BenchmarkMemoryStats
{
    uint64_t m_Bytes, m_PeakBytes;
    uint64_t m_Blocks, m_PeakBlocks;
    uint64_t m_Allocations, m_Reallocations, m_Frees, m_AllocatedBytes;
};

struct BenchmarkMemorySample
{
    BenchmarkMemoryStats m_Before, m_After;
    BenchmarkMemoryStats m_Fixture, m_Resource;
};

void                  InitializeBenchmarkMemory();
void                  SetBenchmarkMemoryDomain(BenchmarkMemoryDomain domain);
void                  BeginBenchmarkMemoryBackend();
void                  EndBenchmarkMemoryBackend();
void                  BeginBenchmarkMemoryOperation();
void                  EndBenchmarkMemoryOperation();
BenchmarkMemorySample GetBenchmarkMemorySample();
void                  CheckBenchmarkMemoryReleased();
#else
static inline void InitializeBenchmarkMemory() {}
static inline void SetBenchmarkMemoryDomain(BenchmarkMemoryDomain) {}
static inline void BeginBenchmarkMemoryBackend() {}
static inline void EndBenchmarkMemoryBackend() {}
static inline void BeginBenchmarkMemoryOperation() {}
static inline void EndBenchmarkMemoryOperation() {}
static inline void CheckBenchmarkMemoryReleased() {}
#endif

#endif // DATA_BENCHMARK_MEMORY_H
