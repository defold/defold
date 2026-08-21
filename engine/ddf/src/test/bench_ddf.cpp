// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <ddf/ddf.h>
#include "test/test_ddf_proto.h"

static volatile uint64_t g_Sink = 0;
static const uint32_t SAMPLE_COUNT = 40;
static const double MIN_SAMPLE_DURATION_NS = 25000000.0;

struct CountContext
{
    uint32_t m_Calls;
    uint32_t m_Bytes;
};

static bool CountWrites(void* context, const void*, uint32_t size)
{
    CountContext* count = (CountContext*)context;
    ++count->m_Calls;
    count->m_Bytes += size;
    return true;
}

struct Stats
{
    double m_MedianNs;
    double m_P05Ns;
    double m_P95Ns;
    double m_P99Ns;
    double m_MadNs;
};

static double Percentile(const std::vector<double>& sorted, double percentile)
{
    double index = percentile * (sorted.size() - 1);
    size_t lower = (size_t) index;
    size_t upper = std::min(lower + 1, sorted.size() - 1);
    return sorted[lower] + (sorted[upper] - sorted[lower]) * (index - lower);
}

static Stats CalculateStats(std::vector<double> samples)
{
    std::sort(samples.begin(), samples.end());
    Stats stats;
    stats.m_MedianNs = Percentile(samples, 0.50);
    stats.m_P05Ns = Percentile(samples, 0.05);
    stats.m_P95Ns = Percentile(samples, 0.95);
    stats.m_P99Ns = Percentile(samples, 0.99);

    std::vector<double> deviations;
    deviations.reserve(samples.size());
    for (double sample : samples)
        deviations.push_back(std::fabs(sample - stats.m_MedianNs));
    std::sort(deviations.begin(), deviations.end());
    stats.m_MadNs = Percentile(deviations, 0.50);
    return stats;
}

template <typename F>
static double Time(F fn, uint32_t repetitions)
{
    auto start = std::chrono::steady_clock::now();
    for (uint32_t i = 0; i < repetitions; ++i)
        fn();
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(end - start).count();
}

template <typename F>
static Stats Bench(const char* name, F fn)
{
    for (uint32_t i = 0; i < 10; ++i)
        fn();

    uint32_t repetitions = 1;
    while (Time(fn, repetitions) < MIN_SAMPLE_DURATION_NS)
    {
        if (repetitions > UINT32_MAX / 2)
            std::abort();
        repetitions *= 2;
    }

    Time(fn, repetitions);
    Time(fn, repetitions);

    std::vector<double> samples;
    samples.reserve(SAMPLE_COUNT);
    for (uint32_t i = 0; i < SAMPLE_COUNT; ++i)
        samples.push_back(Time(fn, repetitions) / repetitions);

    Stats stats = CalculateStats(samples);
    std::printf("%-28s x%-8u med %9.1f ns  p05 %9.1f  p95 %9.1f  p99 %9.1f  MAD %7.1f\n",
                name, repetitions, stats.m_MedianNs, stats.m_P05Ns,
                stats.m_P95Ns, stats.m_P99Ns, stats.m_MadNs);
    return stats;
}

int main()
{
    const uint8_t simple_wire[] = {0x08, 0x01};
    std::printf("Each of %u samples is auto-calibrated to at least %.0f ms.\n",
                SAMPLE_COUNT, MIN_SAMPLE_DURATION_NS / 1000000.0);

    Bench("load simple (2 bytes)", [&]() {
        DUMMY::TestDDF::Simple* msg = 0;
        dmDDF::Result r = dmDDF::LoadMessage(simple_wire, sizeof(simple_wire), &msg);
        if (r != dmDDF::RESULT_OK) std::abort();
        g_Sink += msg->m_A;
        dmDDF::FreeMessage(msg);
    });

    DUMMY::TestDDF::Simple simple = {};
    simple.m_A = 1;
    dmArray<uint8_t> simple_encoded;
    simple_encoded.SetCapacity(32);
    Bench("save simple to reused array", [&]() {
        dmDDF::Result r = dmDDF::SaveMessageToArray(&simple, simple.m_DDFDescriptor, simple_encoded);
        if (r != dmDDF::RESULT_OK) std::abort();
        g_Sink += simple_encoded.Size();
    });

    const uint32_t vertex_count = 4096;
    const uint32_t index_count = 2048;
    std::vector<float> vertices(vertex_count);
    std::vector<uint32_t> indices(index_count);
    for (uint32_t i = 0; i < vertex_count; ++i) vertices[i] = i * 0.25f;
    for (uint32_t i = 0; i < index_count; ++i) indices[i] = i;

    DUMMY::TestDDF::Mesh mesh = {};
    mesh.m_Name = "benchmark mesh";
    mesh.m_Vertices.m_Data = vertices.data();
    mesh.m_Vertices.m_Count = vertex_count;
    mesh.m_Indices.m_Data = indices.data();
    mesh.m_Indices.m_Count = index_count;
    mesh.m_PrimitiveCount = index_count / 3;
    mesh.m_PrimitiveType = DUMMY::TestDDF::Mesh::TRIANGLES;

    dmArray<uint8_t> mesh_encoded;
    mesh_encoded.SetCapacity(32768);
    if (dmDDF::SaveMessageToArray(&mesh, mesh.m_DDFDescriptor, mesh_encoded) != dmDDF::RESULT_OK)
        return 2;
    std::printf("mesh wire bytes: %u\n", mesh_encoded.Size());
    CountContext mesh_writes = {};
    dmDDF::SaveMessage(&mesh, mesh.m_DDFDescriptor, &mesh_writes, CountWrites);
    std::printf("mesh output callbacks: %u\n", mesh_writes.m_Calls);

    Bench("load mesh repeated", [&]() {
        DUMMY::TestDDF::Mesh* msg = 0;
        dmDDF::Result r = dmDDF::LoadMessage(mesh_encoded.Begin(), mesh_encoded.Size(), &msg);
        if (r != dmDDF::RESULT_OK) std::abort();
        g_Sink += msg->m_Indices.m_Count;
        dmDDF::FreeMessage(msg);
    });

    dmArray<uint8_t> mesh_reencoded;
    mesh_reencoded.SetCapacity(mesh_encoded.Size());
    Bench("save mesh to reused array", [&]() {
        dmDDF::Result r = dmDDF::SaveMessageToArray(&mesh, mesh.m_DDFDescriptor, mesh_reencoded);
        if (r != dmDDF::RESULT_OK) std::abort();
        g_Sink += mesh_reencoded.Size();
    });
    Bench("save mesh to fresh array", [&]() {
        dmArray<uint8_t> fresh;
        fresh.SetCapacity(sizeof(mesh));
        dmDDF::Result r = dmDDF::SaveMessageToArray(&mesh, mesh.m_DDFDescriptor, fresh);
        if (r != dmDDF::RESULT_OK) std::abort();
        g_Sink += fresh.Size();
    });

    std::vector<DUMMY::TestDDF::MessageRecursiveB> recursive(6);
    for (uint32_t i = 0; i < recursive.size(); ++i)
    {
        std::memset(&recursive[i], 0, sizeof(recursive[i]));
        recursive[i].m_ValB = i + 10;
        recursive[i].m_MyA.m_ValA = i + 20;
        recursive[i].m_MyA.m_MyB = i + 1 < recursive.size() ? &recursive[i + 1] : 0;
    }
    DUMMY::TestDDF::MessageRecursiveA recursive_root = {};
    recursive_root.m_ValA = 1;
    dmArray<uint8_t> recursive_encoded;
    recursive_encoded.SetCapacity(1024);
    for (uint32_t levels = 0; levels <= recursive.size(); ++levels)
    {
        recursive_root.m_MyB = levels == 0 ? 0 : &recursive[recursive.size() - levels];
        char label[64];
        std::snprintf(label, sizeof(label), "save recursive depth %u", levels * 2 + 1);
        Bench(label, [&]() {
            dmDDF::Result r = dmDDF::SaveMessageToArray(&recursive_root, recursive_root.m_DDFDescriptor, recursive_encoded);
            if (r != dmDDF::RESULT_OK) std::abort();
            g_Sink += recursive_encoded.Size();
        });
    }

    std::printf("sink: %llu\n", (unsigned long long)g_Sink);
    return 0;
}
