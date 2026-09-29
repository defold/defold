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
#include <dlib/time.h>
#include <flecs.h>

static const uint64_t  TABLE = 1;
static const uint64_t  X = 10;
static const uint64_t  Y = 20;
static const uint64_t  TAG = 100;
static const uint32_t  MAX_SAMPLES = 31;
static const uint32_t  READ_PASSES = 10;
static const uint32_t  OP_COUNT = 8;
static const char*     OPERATIONS[] = { "add_row", "bulk_load_decoded", "get_field", "set_field", "query_read", "remove_row", "write_binary", "load_binary" };
static volatile double g_Checksum;

struct Input
{
    uint32_t       m_Count;
    DataValueType  m_Types[2];
    DataValueData* m_Values;
    DataRowDesc*   m_Rows;
    DataOwnerId*   m_Owners;
    double*        m_X;
    double*        m_Y;
    uint32_t*      m_Order;
};

static void Check(bool condition)
{
    if (!condition)
    {
        fprintf(stderr, "Benchmark correctness check failed\n");
        exit(1);
    }
}

static double Elapsed(uint64_t start, uint64_t operations)
{
    return (dmTime::GetMonotonicTime() - start) * 1000.0 / operations;
}

static DataValue Number(double number)
{
    DataValue value = { .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Value = { .m_Number = number } };
    return value;
}

static HDataStore CreateData()
{
    HDataStore    store = DataCreateStore();
    DataFieldDesc fields[] = {
        { .m_Field = X, .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Offset = 0 },
        { .m_Field = Y, .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Offset = 8 }
    };
    DataTableDesc desc = {
        .m_Type = TABLE,
        .m_Tags = &TAG,
        .m_TagCount = 1,
        .m_Fields = fields,
        .m_FieldCount = 2,
        .m_RowStride = 16
    };
    Check(DataRegisterTable(store, &desc) == DATA_RESULT_OK);
    return store;
}

static void RunData(const Input* input, double* timings)
{
    uint32_t   count = input->m_Count;
    DataId*    ids = new DataId[count];
    HDataStore store = CreateData();
    uint64_t   start = dmTime::GetMonotonicTime();
    for (uint32_t row = 0; row < count; ++row)
        Check(DataAddRow(store, TABLE, input->m_Owners[row], input->m_Types, &input->m_Values[row * 2], 2, &ids[row], row + 1) == DATA_RESULT_OK);
    timings[0] = Elapsed(start, count);
    DataDestroyStore(store);

    store = CreateData();
    start = dmTime::GetMonotonicTime();
    Check(DataAddRows(store, TABLE, input->m_Rows, count, ids) == DATA_RESULT_OK);
    timings[1] = Elapsed(start, count);
    double sum = 0;
    start = dmTime::GetMonotonicTime();
    for (uint32_t pass = 0; pass < READ_PASSES; ++pass)
    {
        for (uint32_t row = 0; row < count; ++row)
        {
            double value;
            Check(DataFieldGetNumber(store, ids[input->m_Order[row]], X, &value) == DATA_RESULT_OK);
            sum += value;
        }
    }
    timings[2] = Elapsed(start, (uint64_t)count * READ_PASSES);
    Check(sum == READ_PASSES * ((double)count * (count - 1) / 2));
    g_Checksum = sum;

    start = dmTime::GetMonotonicTime();
    for (uint32_t row = 0; row < count; ++row)
    {
        uint32_t index = input->m_Order[row];
        Check(DataSetFieldNumber(store, ids[index], X, input->m_X[index] + 1) == DATA_RESULT_OK);
    }
    timings[3] = Elapsed(start, count);

    DataQueryField field = { .m_Field = X, .m_Type = DATA_VALUE_TYPE_NUMBER };
    DataQueryDesc  desc = { .m_AllTags = &TAG, .m_AllTagCount = 1, .m_Fields = &field, .m_FieldCount = 1 };
    HDataQuery     query;
    Check(DataCreateQuery(store, &desc, &query) == DATA_RESULT_OK);
    sum = 0;
    uint64_t visited = 0;
    start = dmTime::GetMonotonicTime();
    for (uint32_t pass = 0; pass < READ_PASSES; ++pass)
    {
        DataStoreLock(store);
        DataIterator it = DataQueryIter(query);
        DataResult   result;
        while ((result = DataIterNext(&it)) == DATA_RESULT_OK)
        {
            DataRowIterator rows = DataIterRows(&it);
            while (DataRowIterNext(&rows) == DATA_RESULT_OK)
            {
                DataFieldIterator field = DataRowIterFields(&rows);
                double            value;
                Check(DataFieldIterNext(&field) == DATA_RESULT_OK);
                Check(DataFieldIterGetNumber(&field, &value) == DATA_RESULT_OK);
                sum += value;
                ++visited;
            }
        }
        Check(result == DATA_RESULT_END);
        DataStoreUnlock(store);
    }
    timings[4] = Elapsed(start, (uint64_t)count * READ_PASSES);
    Check(visited == (uint64_t)count * READ_PASSES);
    Check(sum == READ_PASSES * ((double)count * (count + 1) / 2));
    g_Checksum = sum;
    DataDestroyQuery(query);

    uint32_t size;
    Check(DataWriteBlob(store, 0, 0, &size) == DATA_RESULT_OK);
    uint8_t* bytes = new uint8_t[size];
    start = dmTime::GetMonotonicTime();
    Check(DataWriteBlob(store, bytes, size, &size) == DATA_RESULT_OK);
    timings[6] = Elapsed(start, count);
    HDataStore loaded = DataCreateStore();
    start = dmTime::GetMonotonicTime();
    HDataBlob         blob;
    HDataBlobInstance instance;
    Check(DataLoadBlob(bytes, size, &blob) == DATA_RESULT_OK);
    Check(DataAddBlob(loaded, blob, 1, &instance) == DATA_RESULT_OK);
    DataDestroyBlob(blob);
    timings[7] = Elapsed(start, count);
    Check(DataCreateQuery(loaded, &desc, &query) == DATA_RESULT_OK);
    DataStoreLock(loaded);
    DataIterator it = DataQueryIter(query);
    Check(DataIterNext(&it) == DATA_RESULT_OK);
    DataRowIterator rows = DataIterRows(&it);
    sum = 0;
    visited = 0;
    while (DataRowIterNext(&rows) == DATA_RESULT_OK)
    {
        DataFieldIterator field = DataRowIterFields(&rows);
        double            value;
        Check(DataFieldIterNext(&field) == DATA_RESULT_OK);
        Check(DataFieldIterGetNumber(&field, &value) == DATA_RESULT_OK);
        sum += value;
        Check(DataRowIterGetOwnerId(&rows) == 1);
        ++visited;
    }
    Check(visited == count);
    Check(sum == (double)count * (count + 1) / 2);
    DataStoreUnlock(loaded);
    DataDestroyQuery(query);
    DataDestroyStore(loaded);
    delete[] bytes;

    start = dmTime::GetMonotonicTime();
    for (uint32_t row = 0; row < count; ++row)
        Check(DataRemoveRow(store, ids[input->m_Order[row]]) == DATA_RESULT_OK);
    timings[5] = Elapsed(start, count);
    DataDestroyStore(store);
    delete[] ids;
}

struct FlecsStore
{
    ecs_world_t* m_World;
    ecs_entity_t m_X;
    ecs_entity_t m_Y;
    ecs_entity_t m_Owner;
    ecs_entity_t m_Tag;
};

static FlecsStore CreateFlecs()
{
    FlecsStore           store = { .m_World = ecs_mini() };
    ecs_component_desc_t desc = { .type = { .size = sizeof(double), .alignment = sizeof(double) } };
    store.m_X = ecs_component_init(store.m_World, &desc);
    store.m_Y = ecs_component_init(store.m_World, &desc);
    desc.type.size = sizeof(uint64_t);
    desc.type.alignment = sizeof(uint64_t);
    store.m_Owner = ecs_component_init(store.m_World, &desc);
    store.m_Tag = ecs_new(store.m_World);
    return store;
}

static void RunFlecs(const Input* input, double* timings)
{
    uint32_t      count = input->m_Count;
    ecs_entity_t* ids = new ecs_entity_t[count];
    FlecsStore    store = CreateFlecs();
    uint64_t      start = dmTime::GetMonotonicTime();
    for (uint32_t row = 0; row < count; ++row)
    {
        ecs_value_t values[] = {
            { .type = store.m_X, .ptr = &input->m_X[row] },
            { .type = store.m_Y, .ptr = &input->m_Y[row] },
            { .type = store.m_Owner, .ptr = &input->m_Owners[row] },
            { .type = store.m_Tag, .ptr = 0 },
            {}
        };
        ids[row] = ecs_insert_w_values(store.m_World, values);
        Check(ids[row] != 0);
    }
    timings[0] = Elapsed(start, count);
    ecs_fini(store.m_World);

    store = CreateFlecs();
    void*           values[] = { input->m_X, input->m_Y, input->m_Owners, 0 };
    ecs_bulk_desc_t bulk = {
        .count = (int32_t)count,
        .ids = { store.m_X, store.m_Y, store.m_Owner, store.m_Tag },
        .data = values
    };
    start = dmTime::GetMonotonicTime();
    const ecs_entity_t* inserted = ecs_bulk_init(store.m_World, &bulk);
    Check(inserted != 0);
    memcpy(ids, inserted, count * sizeof(ecs_entity_t));
    timings[1] = Elapsed(start, count);
    double sum = 0;
    start = dmTime::GetMonotonicTime();
    for (uint32_t pass = 0; pass < READ_PASSES; ++pass)
    {
        for (uint32_t row = 0; row < count; ++row)
        {
            const double* value = (const double*)ecs_get_id(store.m_World, ids[input->m_Order[row]], store.m_X);
            Check(value != 0);
            sum += *value;
        }
    }
    timings[2] = Elapsed(start, (uint64_t)count * READ_PASSES);
    Check(sum == READ_PASSES * ((double)count * (count - 1) / 2));
    g_Checksum = sum;
    start = dmTime::GetMonotonicTime();
    for (uint32_t row = 0; row < count; ++row)
    {
        uint32_t index = input->m_Order[row];
        double   value = input->m_X[index] + 1;
        ecs_set_id(store.m_World, ids[index], store.m_X, sizeof(value), &value);
    }
    timings[3] = Elapsed(start, count);

    ecs_query_desc_t desc = {
        .terms = { { .id = store.m_X, .inout = EcsIn }, { .id = store.m_Tag } },
        .cache_kind = EcsQueryCacheAuto,
    };
    ecs_query_t* query = ecs_query_init(store.m_World, &desc);
    Check(query != 0);
    sum = 0;
    uint64_t visited = 0;
    start = dmTime::GetMonotonicTime();
    for (uint32_t pass = 0; pass < READ_PASSES; ++pass)
    {
        ecs_iter_t it = ecs_query_iter(store.m_World, query);
        while (ecs_query_next(&it))
        {
            const double* x = (const double*)ecs_field_w_size(&it, sizeof(double), 0);
            for (int32_t row = 0; row < it.count; ++row)
                sum += x[row];
            visited += it.count;
        }
    }
    timings[4] = Elapsed(start, (uint64_t)count * READ_PASSES);
    Check(visited == (uint64_t)count * READ_PASSES);
    Check(sum == READ_PASSES * ((double)count * (count + 1) / 2));
    g_Checksum = sum;
    ecs_query_fini(query);
    start = dmTime::GetMonotonicTime();
    for (uint32_t row = 0; row < count; ++row)
        ecs_delete(store.m_World, ids[input->m_Order[row]]);
    timings[5] = Elapsed(start, count);
    ecs_fini(store.m_World);
    delete[] ids;
}

static int Compare(const void* a, const void* b)
{
    double x = *(const double*)a;
    double y = *(const double*)b;
    return x < y ? -1 : x > y;
}

static uint32_t ParseCount(const char* text, uint32_t maximum)
{
    char*         end;
    unsigned long value = strtoul(text, &end, 10);
    if (!*text || *end || !value || value > maximum)
    {
        fprintf(stderr, "Expected an integer between 1 and %u\n", maximum);
        exit(1);
    }
    return (uint32_t)value;
}

int main(int argc, char** argv)
{
    if (argc > 3)
    {
        fprintf(stderr, "Usage: benchmark_data [rows=100000] [samples=7, max=31]\n");
        return 1;
    }
    Input input;
    input.m_Count = argc > 1 ? ParseCount(argv[1], 10000000) : 100000;
    uint32_t samples = argc > 2 ? ParseCount(argv[2], MAX_SAMPLES) : 7;
    uint32_t count = input.m_Count;
    input.m_Types[0] = input.m_Types[1] = DATA_VALUE_TYPE_NUMBER;
    input.m_Values = new DataValueData[count * 2];
    input.m_Rows = new DataRowDesc[count];
    input.m_Owners = new DataOwnerId[count];
    input.m_X = new double[count];
    input.m_Y = new double[count];
    input.m_Order = new uint32_t[count];
    for (uint32_t row = 0; row < count; ++row)
    {
        input.m_Values[row * 2] = Number(row).m_Value;
        input.m_Values[row * 2 + 1] = Number(row * 2).m_Value;
        input.m_Rows[row].m_Owner = row + 1;
        input.m_Rows[row].m_Types = input.m_Types;
        input.m_Rows[row].m_Values = &input.m_Values[row * 2];
        input.m_Rows[row].m_ValueCount = 2;
        input.m_Rows[row].m_ComponentId = row + 1;
        input.m_Owners[row] = row + 1;
        input.m_X[row] = row;
        input.m_Y[row] = row * 2;
        input.m_Order[row] = row;
    }
    uint32_t random = 0x12345678;
    for (uint32_t i = count - 1; i > 0; --i)
    {
        random = random * 1664525 + 1013904223;
        uint32_t j = random % (i + 1);
        uint32_t swap = input.m_Order[i];
        input.m_Order[i] = input.m_Order[j];
        input.m_Order[j] = swap;
    }
    printf("# Flecs %s; rows=%u; samples=%u; one warmup; read_passes=%u; shuffled get/set/remove\n", FLECS_VERSION, count, samples, READ_PASSES);
    printf("# numeric fields x/y + uint64 owner + tag; query: Data checked row API / Flecs direct column\n");
    printf("# binary operations are Data-only; they do not represent a Flecs serialization comparison\n");
    double results[2][OP_COUNT][MAX_SAMPLES] = {};
    for (uint32_t sample = 0; sample <= samples; ++sample)
    {
        // Alternate library order to reduce systematic warm-cache/frequency bias.
        for (uint32_t run = 0; run < 2; ++run)
        {
            uint32_t library = (sample + run) % 2;
            double   timings[OP_COUNT] = {};
            if (library == 0)
                RunData(&input, timings);
            else
                RunFlecs(&input, timings);
            if (sample)
            {
                for (uint32_t op = 0; op < OP_COUNT; ++op)
                    results[library][op][sample - 1] = timings[op];
            }
        }
    }
    printf("library,operation,rows,median_ns_per_row,min_ns_per_row\n");
    for (uint32_t op = 0; op < OP_COUNT; ++op)
    {
        for (uint32_t library = 0; library < (op < 6 ? 2u : 1u); ++library)
        {
            double* values = results[library][op];
            qsort(values, samples, sizeof(double), Compare);
            double median = (values[(samples - 1) / 2] + values[samples / 2]) / 2;
            printf("%s,%s,%u,%.3f,%.3f\n", library ? "flecs" : "data", OPERATIONS[op], count, median, values[0]);
        }
    }
    delete[] input.m_Order;
    delete[] input.m_Y;
    delete[] input.m_X;
    delete[] input.m_Owners;
    delete[] input.m_Rows;
    delete[] input.m_Values;
    return 0;
}
