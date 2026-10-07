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

// Offline resource preparation. This executable is not linked into benchmarks.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../data.h"
#include "benchmark_data_threaded.h"
#include <dlib/hash.h>
#ifdef DATA_FIXTURE_MIXED
#include "benchmark_data_common.h"
#endif

void Check(bool ok, const char* message)
{
    if (!ok)
    {
        fprintf(stderr, "Fixture writer: %s\n", message);
        exit(1);
    }
}

struct ThreadedFixture
{
    uint8_t*             m_Bytes[2];
    uint32_t             m_ByteCount[2];
    ThreadedReferenceRow m_Defaults[2][THREAD_BUNDLE_ROWS];
};

static DataValue Number(double value)
{
    DataValue out = { .m_Type = DATA_TYPE_NUMBER, .m_Value = { .m_Number = value } };
    return out;
}

static DataValue Vector(const DataVector3& value)
{
    DataValue out = { .m_Type = DATA_TYPE_VECTOR3 };
    memcpy(out.m_Value.m_Vector3, value.m_Values, sizeof(value));
    return out;
}

// Each serialized bundle contains 100 rows: either six tables in the fixture's
// 10/15/1/24/25/25 proportions, or one Enemy table for the additional waves.
static void BuildPrototype(ThreadedFixture* fixture, bool enemies)
{
    HDataStore    source = DataCreateStore();
    DataFieldDesc light_fields[] = {
        { .m_Field = THREAD_COLOR, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 0, .m_Name = "color" },
        { .m_Field = THREAD_INTENSITY, .m_Type = DATA_TYPE_NUMBER, .m_Offset = 16, .m_Name = "intensity" }
    };
    DataStructDesc light = { .m_Fields = light_fields, .m_FieldCount = 2, .m_Size = 24 };
    DataFieldDesc  fields[THREAD_TYPE_COUNT][6] = {
        { { .m_Field = THREAD_POSITION, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 0 },
           { .m_Field = THREAD_LIGHT, .m_Type = DATA_TYPE_STRUCT, .m_Offset = 16, .m_Struct = &light, .m_Name = "light" },
           { .m_Field = 7, .m_Type = DATA_TYPE_NUMBER, .m_Offset = 40 },
           { .m_Field = 8, .m_Type = DATA_TYPE_NUMBER, .m_Offset = 48 },
           { .m_Field = 9, .m_Type = DATA_TYPE_NUMBER, .m_Offset = 56 } },
        { { .m_Field = THREAD_LIGHT, .m_Type = DATA_TYPE_STRUCT, .m_Offset = 0, .m_Struct = &light, .m_Name = "light" },
           { .m_Field = THREAD_POSITION, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 24 },
           { .m_Field = 7, .m_Type = DATA_TYPE_NUMBER, .m_Offset = 40 } },
        { { .m_Field = THREAD_POSITION, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 0 },
           { .m_Field = THREAD_HEALTH, .m_Type = DATA_TYPE_NUMBER, .m_Offset = 16 },
           { .m_Field = THREAD_VELOCITY, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 24 } },
        { { .m_Field = THREAD_HEALTH, .m_Type = DATA_TYPE_NUMBER, .m_Offset = 0 },
           { .m_Field = THREAD_VELOCITY, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 8 },
           { .m_Field = THREAD_POSITION, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 20 } },
        { { .m_Field = THREAD_POSITION, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 0 },
           { .m_Field = 10, .m_Type = DATA_TYPE_NUMBER, .m_Offset = 16 } },
        { { .m_Field = THREAD_HEALTH, .m_Type = DATA_TYPE_NUMBER, .m_Offset = 0 },
           { .m_Field = THREAD_POSITION, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 8 } }
    };
    const uint32_t field_counts[] = { 5, 3, 3, 3, 2, 2 };
    const uint32_t strides[] = { 64, 48, 40, 32, 24, 24 };
    uint64_t       names[] = { THREAD_COLOR, THREAD_INTENSITY };
    uint32_t       component = 0;
    ThreadedDefaults(fixture->m_Defaults[enemies], enemies);
    for (uint32_t t = 0; t < THREAD_TYPE_COUNT; ++t)
    {
        uint32_t count = ThreadedTypeRows(t, enemies);
        if (!count)
            continue;
        uint64_t      tags[] = { 100 + t, dmHashBufferNoReverse64("light", sizeof("light") - 1) };
        DataTableDesc table = {
            .m_Type = 1000 + t,
            .m_Tags = tags,
            .m_TagCount = t < 2 ? 2u : 1u,
            .m_Fields = fields[t],
            .m_FieldCount = field_counts[t],
            .m_RowStride = strides[t]
        };
        ThreadedCheck(DataRegisterTable(source, &table) == DATA_RESULT_OK, "prototype table");
        DataRowDesc         rows[THREAD_BUNDLE_ROWS] = {};
        DataValueType       types[6];
        const DataValueType child_types[] = { DATA_TYPE_VECTOR3, DATA_TYPE_NUMBER };
        for (uint32_t f = 0; f < field_counts[t]; ++f)
            types[f] = fields[t][f].m_Type;
        DataValueData values[THREAD_BUNDLE_ROWS][6] = {}, children[THREAD_BUNDLE_ROWS][2] = {};
        DataId        ids[THREAD_BUNDLE_ROWS];
        for (uint32_t r = 0; r < count; ++r, ++component)
        {
            ThreadedReferenceRow* row = &fixture->m_Defaults[enemies][component];
            children[r][0] = Vector(row->m_Color).m_Value;
            children[r][1] = Number(row->m_Intensity).m_Value;
            for (uint32_t f = 0; f < field_counts[t]; ++f)
            {
                uint64_t  name = fields[t][f].m_Field;
                DataValue value = Number(10);
                if (name == THREAD_POSITION)
                    value = Vector(row->m_Position);
                else if (name == THREAD_VELOCITY)
                    value = Vector(row->m_Velocity);
                else if (name == THREAD_HEALTH)
                    value = Number(row->m_Health);
                else if (name == THREAD_LIGHT)
                {
                    value.m_Type = DATA_TYPE_STRUCT;
                    value.m_Value.m_Struct = {
                        .m_Count = 2,
                        .m_Source = DATA_STRUCT_ARRAY,
                        .m_Array = { .m_Names = names, .m_Types = child_types, .m_Values = children[r] }
                    };
                }
                values[r][f] = value.m_Value;
            }
            rows[r].m_Types = types;
            rows[r].m_Values = values[r];
            rows[r].m_ValueCount = field_counts[t];
            rows[r].m_ComponentId = component + 1;
        }
        ThreadedCheck(DataAddRows(source, table.m_Type, rows, count, ids) == DATA_RESULT_OK, "prototype rows");
    }
    ThreadedCheck(DataWriteBlob(source, 0, 0, &fixture->m_ByteCount[enemies]) == DATA_RESULT_OK, "prototype size");
    {
        fixture->m_Bytes[enemies] = new uint8_t[fixture->m_ByteCount[enemies]];
    }
    ThreadedCheck(DataWriteBlob(source, fixture->m_Bytes[enemies], fixture->m_ByteCount[enemies], &fixture->m_ByteCount[enemies]) == DATA_RESULT_OK, "serialize prototype");
    ThreadedCheck(DataDestroyStore(source) == DATA_RESULT_OK, "destroy prototype builder");
}

static HDataStore BuildLoadStore(uint32_t tables, uint32_t rows)
{
    HDataStore    source = DataCreateStore();
    DataFieldDesc fields[] = {
        { .m_Field = 10, .m_Type = DATA_TYPE_NUMBER, .m_Offset = 0 },
        { .m_Field = 20, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 8 }
    };
    const DataValueType types[] = { DATA_TYPE_NUMBER, DATA_TYPE_VECTOR3 };
    DataValueData       values[] = { { .m_Number = 0 }, { .m_Vector3 = { 0, 2, 3 } } };
    uint32_t            capacity = (rows + tables - 1) / tables;
    DataRowDesc*        input = new DataRowDesc[capacity];
    DataId*             ids = new DataId[capacity];
    for (uint32_t t = 0; t < tables; ++t)
    {
        DataTableDesc desc = {
            .m_Type = t + 1,
            .m_Fields = fields,
            .m_FieldCount = 2,
            .m_RowStride = 24
        }; // Tail padding for the double's eight-byte alignment.
        Check(DataRegisterTable(source, &desc) == DATA_RESULT_OK, "register source table");
        uint32_t count = rows / tables + (t < rows % tables);
        values[0].m_Number = 100 + t;
        values[1].m_Vector3[0] = (float)t;
        for (uint32_t r = 0; r < count; ++r)
        {
            input[r].m_Group = r + 1;
            input[r].m_Types = types;
            input[r].m_Values = values;
            input[r].m_ValueCount = 2;
            input[r].m_ComponentId = r + 1;
        }
        Check(DataAddRows(source, t + 1, input, count, ids) == DATA_RESULT_OK, "source rows");
    }
    delete[] input;
    delete[] ids;
    return source;
}

static void WriteFile(const char* directory, const char* name, const void* bytes, uint32_t size)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", directory, name);
    FILE* file = fopen(path, "wb");
    Check(file != 0, "open output");
    Check(fwrite(bytes, 1, size, file) == size, "write output");
    Check(fclose(file) == 0, "close output");
}

static void WriteStore(const char* directory, const char* name, HDataStore store)
{
    uint32_t size;
    Check(DataWriteBlob(store, 0, 0, &size) == DATA_RESULT_OK, "blob size");
    uint8_t* bytes = new uint8_t[size];
    Check(DataWriteBlob(store, bytes, size, &size) == DATA_RESULT_OK, "serialize fixture");
    WriteFile(directory, name, bytes, size);
    delete[] bytes;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: data_fixture_writer output-directory\n");
        return 1;
    }
    ThreadedFixture threaded = {};
    BuildPrototype(&threaded, false);
    BuildPrototype(&threaded, true);
    WriteFile(argv[1], "threaded-mixed.datac", threaded.m_Bytes[0], threaded.m_ByteCount[0]);
    WriteFile(argv[1], "threaded-enemies.datac", threaded.m_Bytes[1], threaded.m_ByteCount[1]);
    delete[] threaded.m_Bytes[0];
    delete[] threaded.m_Bytes[1];
#ifdef DATA_FIXTURE_MIXED
    InitFixtureMetadata();
    const uint32_t groups[] = { 1, 4, 16 };
    for (uint32_t g = 0; g < 3; ++g)
    {
        Fixture input = { .m_GroupSize = groups[g] };
        InitFixture(&input, 16000);
        for (uint32_t ti = 0; ti < TYPE_COUNT; ++ti)
        {
            const TypeInput* t = &input.m_Types[ti];
            HDataStore       source = DataCreateStore();
            DataTableDesc    desc = { .m_Type = t->m_Type, .m_Tags = t->m_Tags, .m_TagCount = t->m_TagCount, .m_Fields = t->m_Metadata, .m_FieldCount = t->m_FieldCount, .m_RowStride = t->m_Stride };
            Check(DataRegisterTable(source, &desc) == DATA_RESULT_OK, "packed layout");
            DataId ids[16];
            Check(DataCreateRows(source, t->m_Type, 0, groups[g], t->m_Native, ids) == DATA_RESULT_OK, "packed rows");
            // Only the offline resource builder assigns serialized component names.
            DataTable* table = FindTable(source, t->m_Type);
            for (uint32_t r = 0; r < groups[g]; ++r)
                table->m_Rows[r].m_ComponentId = t->m_ComponentIds[r];
            char name[64];
            snprintf(name, sizeof(name), "packed-%u-%u.datac", groups[g], ti);
            WriteStore(argv[1], name, source);
            DataDestroyStore(source);
        }
        DeleteFixture(&input);
    }
#endif
    const uint32_t tables[] = { 1, 6, 1024, 6 };
    const uint32_t rows[] = { 1, 6, 1024, 1000000 };
    for (uint32_t i = 0; i < 4; ++i)
    {
        HDataStore store = BuildLoadStore(tables[i], rows[i]);
        char       name[64];
        snprintf(name, sizeof(name), "load-%u-%u.datac", tables[i], rows[i]);
        WriteStore(argv[1], name, store);
        DataDestroyStore(store);
    }
    return 0;
}
