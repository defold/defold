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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../data.h"
#include "benchmark_memory.h"

static void Check(bool condition, const char* message)
{
    if (!condition)
    {
        fprintf(stderr, "Blob load benchmark: %s\n", message);
        abort();
    }
}

// Numeric health/position tables isolate loading allocation growth, not gameplay throughput.
static FILE* CreateFile(uint32_t tables, uint32_t rows, uint32_t* out_size)
{
    SetBenchmarkMemoryDomain(BENCHMARK_MEMORY_FIXTURE);
    HDataStore       source = DataCreateStore();
    DataPropertyDesc properties[] = { { 10, DATA_VALUE_TYPE_NUMBER, 0 }, { 20, DATA_VALUE_TYPE_VECTOR3, 8 } };
    DataValue        values[2] = {};
    values[0].m_Type = DATA_VALUE_TYPE_NUMBER;
    values[1].m_Type = DATA_VALUE_TYPE_VECTOR3;
    values[1].m_Value.m_Vector3[1] = 2;
    values[1].m_Value.m_Vector3[2] = 3;
    uint32_t     capacity = (rows + tables - 1) / tables;
    DataRowDesc* input = new DataRowDesc[capacity];
    DataId*      ids = new DataId[capacity];
    for (uint32_t t = 0; t < tables; ++t)
    {
        DataTableDesc desc = { t + 1, 0, 0, properties, 2, 24 }; // Tail padding for the double's eight-byte alignment.
        Check(DataRegisterTable(source, &desc) == DATA_RESULT_OK, "register source table");
        uint32_t count = rows / tables + (t < rows % tables);
        values[0].m_Value.m_Number = 100 + t;
        values[1].m_Value.m_Vector3[0] = (float)t;
        for (uint32_t r = 0; r < count; ++r)
        {
            input[r].m_Owner = r + 1;
            input[r].m_Values = values;
            input[r].m_ValueCount = 2;
            input[r].m_ComponentId = r + 1;
        }
        Check(DataAddRows(source, t + 1, input, count, ids) == DATA_RESULT_OK, "source rows");
    }
    Check(DataWriteBlob(source, 0, 0, out_size) == DATA_RESULT_OK, "file size");
    uint8_t* buffer = new uint8_t[*out_size];
    Check(DataWriteBlob(source, buffer, *out_size, out_size) == DATA_RESULT_OK, "serialize file");
    FILE* file = tmpfile();
    Check(file != 0, "create temporary data file");
    Check(fwrite(buffer, 1, *out_size, file) == *out_size && fflush(file) == 0, "write data file");
    delete[] buffer;
    delete[] input;
    delete[] ids;
    DataDestroyStore(source);
    SetBenchmarkMemoryDomain(BENCHMARK_MEMORY_NONE);
    CheckBenchmarkMemoryReleased();
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
            Check(DataGetPropertyNumber(store, id, 10, &health) == DATA_RESULT_OK && health == 99 + type, "loaded health");
            Check(DataGetPropertyVector3(store, id, 20, &position) == DATA_RESULT_OK, "loaded position");
            Check(position.m_Values[0] == type - 1 && position.m_Values[1] == 2 && position.m_Values[2] == 3, "position components");
            Check(DataRowIterGetOwnerId(&row) == 4242, "runtime owner");
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
        FILE*    file = CreateFile(table_counts[c], row_counts[c], &size);
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
                    Check(DataGetPropertyNumber(store, stale, 10, &health) == DATA_RESULT_NOT_FOUND, "stale IDs after reloading");
                stale = Validate(store, table_counts[c], row_counts[c]);
                Check(GetInstanceTable(instance, 0)->m_Blob == buffer, "caller buffer is borrowed");
                DataDestroyBlob(blob);
                DataRemoveBlob(instance);
                Check(DataGetPropertyNumber(store, stale, 10, &health) == DATA_RESULT_NOT_FOUND, "unloaded ID");
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
