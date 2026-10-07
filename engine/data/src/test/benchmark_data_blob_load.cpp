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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dmsdk/data/data.h>
#include "benchmark_memory.h"

static void Check(bool condition, const char* message)
{
    if (!condition)
    {
        fprintf(stderr, "Blob load benchmark: %s\n", message);
        exit(1);
    }
}

// Numeric health/position tables isolate loading allocation growth, not gameplay throughput.
static FILE* OpenFixture(uint32_t tables, uint32_t rows, uint32_t* out_size)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/load-%u-%u.datac", DATA_BENCHMARK_FIXTURE_DIR, tables, rows);
    FILE* file = fopen(path, "rb");
    Check(file != 0, "open prebuilt file");
    Check(fseek(file, 0, SEEK_END) == 0, "size fixture");
    *out_size = (uint32_t)ftell(file);
    rewind(file);
    return file;
}

static DataId Validate(HDataStore store, uint32_t expected_tables, uint32_t expected_rows)
{
    DataQueryDesc desc = {};
    HDataQuery    query;
    Check(DataCreateQuery(store, &desc, &query) == DATA_RESULT_OK, "validation query");
    DataStoreLock(store);
    DataIterator it = DataQueryIter(query);
    DataId       first = 0;
    uint32_t     tables = 0, rows = 0;
    while (DataIterNext(&it) == DATA_RESULT_OK)
    {
        uint64_t        type = DataIterGetType(&it);
        DataRowIterator row = DataIterRows(&it);
        uint32_t        r = 0;
        while (DataRowIterNext(&row) == DATA_RESULT_OK)
        {
            DataId      id = DataRowIterGetId(&row);
            double      health;
            DataVector3 position;
            Check(DataFieldGetNumber(store, id, 10, &health) == DATA_RESULT_OK && health == 99 + type, "loaded health");
            Check(DataFieldGetVector3(store, id, 20, &position) == DATA_RESULT_OK, "loaded position");
            Check(position.m_Values[0] == type - 1 && position.m_Values[1] == 2 && position.m_Values[2] == 3, "position components");
            Check(DataRowIterGetGroupId(&row) == 4242, "runtime owner");
            Check(DataGetComponentId(store, id) == r + 1, "component identity");
            if (!first)
                first = id;
            ++r;
        }
        ++tables;
        rows += r;
    }
    Check(tables == expected_tables && rows == expected_rows, "complete blob registration");
    DataStoreUnlock(store);
    DataDestroyQuery(query);
    return first;
}

int main(int argc, char** argv)
{
    bool bounded = argc == 2 && !strcmp(argv[1], "--expect-bounded");
    Check(argc == 1 || bounded, "usage: benchmark_data_blob_load [--expect-bounded]");
    InitializeBenchmarkMemory();
    printf("# File-to-store allocation experiment; no live queries during loading; 7 samples after one warmup\n");
    printf("# Includes caller file buffer, blob handle, registration metadata and store growth; excludes prior store creation, fixture generation and validation queries\n");
    printf("# Counts C++/dlib heap requests only, not stdio/system allocator internals or RSS; no disk-throughput timing; fixture file may be cached\n");
    printf("tables,rows,sample,store_state,file_bytes,before_bytes,live_bytes,peak_bytes,allocations,reallocations,frees,live_blocks\n");
    const uint32_t table_counts[] = { 1, 6, 1024, 6 };
    const uint32_t row_counts[] = { 1, 6, 1024, 1000000 };
    for (uint32_t c = 0; c < sizeof(table_counts) / sizeof(table_counts[0]); ++c)
    {
        uint32_t size;
        FILE*    file = OpenFixture(table_counts[c], row_counts[c], &size);
        for (uint32_t sample = 0; sample <= 7; ++sample)
        {
            BeginBenchmarkMemoryBackend();
            HDataStore store = DataCreateStore();
            DataId     stale = 0;
            for (uint32_t reuse = 0; reuse < 2; ++reuse)
            {
                Check(fseek(file, 0, SEEK_SET) == 0, "seek file");
                BeginBenchmarkMemoryOperation();
                uint8_t* buffer = new uint8_t[size];
                Check(fread(buffer, 1, size, file) == size, "read directly into caller buffer");
                HDataBlob blob;
                Check(DataLoadBlob(buffer, size, &blob) == DATA_RESULT_OK, "validate borrowed blob");
                HDataBlobInstance instance;
                Check(DataAddBlob(store, blob, 4242, &instance) == DATA_RESULT_OK, "register complete blob");
                EndBenchmarkMemoryOperation();
                BenchmarkMemorySample memory = GetBenchmarkMemorySample();
                uint64_t              allocations = memory.m_After.m_Allocations - memory.m_Before.m_Allocations;
                uint64_t              reallocations = memory.m_After.m_Reallocations - memory.m_Before.m_Reallocations;
                if (bounded)
                    Check(allocations + reallocations <= (reuse ? 3u : 5u), "bounded allocations per loaded blob");
                if (sample)
                    printf("%u,%u,%u,%s,%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n", table_counts[c], row_counts[c], sample, reuse ? "reused" : "fresh", size, (unsigned long long)memory.m_Before.m_Bytes, (unsigned long long)memory.m_After.m_Bytes, (unsigned long long)memory.m_After.m_PeakBytes, (unsigned long long)allocations, (unsigned long long)reallocations, (unsigned long long)(memory.m_After.m_Frees - memory.m_Before.m_Frees), (unsigned long long)memory.m_After.m_Blocks);
                double health;
                if (stale)
                    Check(DataFieldGetNumber(store, stale, 10, &health) == DATA_RESULT_NOT_FOUND, "stale IDs after reloading");
                stale = Validate(store, table_counts[c], row_counts[c]);
                DataDestroyBlob(blob);
                DataRemoveBlob(instance);
                Check(DataFieldGetNumber(store, stale, 10, &health) == DATA_RESULT_NOT_FOUND, "unloaded ID");
                delete[] buffer;
            }
            DataDestroyStore(store);
            EndBenchmarkMemoryBackend();
            CheckBenchmarkMemoryReleased();
        }
        fclose(file);
    }
    return 0;
}
