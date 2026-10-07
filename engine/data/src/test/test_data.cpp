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
#include <string.h>
#include <dlib/hash.h>
#include <dlib/align.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../data.h"

extern "C" int TestDataFromC(HDataStore store);
extern "C" int TestDataStoreLifecycleFromC(void);
extern "C" int TestDataNativeRowsFromC(void);
extern "C" int TestDataSoARowsFromC(void);
extern "C" int CreateReferenceRowsFromC(HDataStore store, int soa, DataId ids[2]);
extern "C" int CreateMixedListRowsFromC(HDataStore store, int soa, DataId ids[2]);

// Helpers for internal binding/value tests. Public C traversal is covered in test_data_c.c.
static DataResult GetTestFieldNumber(const DataIterator* batch, uint32_t row, uint32_t field, double* value)
{
    DataValue  field_value;
    DataResult result = DataIterGetField(batch, row, field, &field_value);
    if (result == DATA_RESULT_OK)
        *value = field_value.m_Value.m_Number;
    return result;
}

static DataResult SetTestFieldNumber(const DataIterator* batch, uint32_t row, uint32_t field, double value)
{
    DataValue field_value = { .m_Type = DATA_TYPE_NUMBER, .m_Value = { .m_Number = value } };
    return DataIterSetField(batch, row, field, &field_value);
}

static DataResult GetTestFieldBoolean(const DataIterator* batch, uint32_t row, uint32_t field, uint8_t* value)
{
    DataValue  field_value;
    DataResult result = DataIterGetField(batch, row, field, &field_value);
    if (result == DATA_RESULT_OK)
        *value = field_value.m_Value.m_Boolean;
    return result;
}

static DataResult SetTestFieldBoolean(const DataIterator* batch, uint32_t row, uint32_t field, uint8_t value)
{
    DataValue field_value = { .m_Type = DATA_TYPE_BOOLEAN, .m_Value = { .m_Boolean = value } };
    return DataIterSetField(batch, row, field, &field_value);
}

static DataResult GetTestFieldString(const DataIterator* batch, uint32_t row, uint32_t field, const char** value)
{
    DataValue  field_value;
    DataResult result = DataIterGetField(batch, row, field, &field_value);
    if (result == DATA_RESULT_OK)
        *value = field_value.m_Value.m_String;
    return result;
}

static DataResult SetTestFieldString(const DataIterator* batch, uint32_t row, uint32_t field, const char* value)
{
    DataValue field_value = { .m_Type = DATA_TYPE_STRING, .m_Value = { .m_String = value } };
    return DataIterSetField(batch, row, field, &field_value);
}

static DataResult GetTestFieldVector3(const DataIterator* batch, uint32_t row, uint32_t field, DataVector3* value)
{
    DataValue  field_value;
    DataResult result = DataIterGetField(batch, row, field, &field_value);
    if (result == DATA_RESULT_OK)
        memcpy(value->m_Values, field_value.m_Value.m_Vector3, sizeof(value->m_Values));
    return result;
}

static DataResult SetTestFieldVector3(const DataIterator* batch, uint32_t row, uint32_t field, const DataVector3* value)
{
    DataValue field_value = { .m_Type = DATA_TYPE_VECTOR3 };
    memcpy(field_value.m_Value.m_Vector3, value->m_Values, sizeof(value->m_Values));
    return DataIterSetField(batch, row, field, &field_value);
}

static DataValue Number(double v)
{
    DataValue x = {};
    x.m_Type = DATA_TYPE_NUMBER;
    x.m_Value.m_Number = v;
    return x;
}

static DataValue String(const char* v)
{
    DataValue x = {};
    x.m_Type = DATA_TYPE_STRING;
    x.m_Value.m_String = v;
    return x;
}

static DataValue Boolean(uint8_t v)
{
    DataValue x = {};
    x.m_Type = DATA_TYPE_BOOLEAN;
    x.m_Value.m_Boolean = v;
    return x;
}

static DataValue Null()
{
    DataValue x = {};
    x.m_Type = DATA_TYPE_NULL;
    return x;
}

static DataValue List(const DataValue* values, uint32_t count)
{
    DataValue x = {};
    x.m_Type = DATA_TYPE_LIST;
    DataList list = { values, count, 0, 0 };
    x.m_Value.m_List = list;
    return x;
}

static DataValue Struct(const uint64_t* names, const DataValueType* types, const DataValueData* values, uint32_t count)
{
    DataValue x = {};
    x.m_Type = DATA_TYPE_STRUCT;
    DataStruct object = { .m_Count = count, .m_Source = DATA_STRUCT_ARRAY, .m_Array = { .m_Names = names, .m_Types = types, .m_Values = values } };
    x.m_Value.m_Struct = object;
    return x;
}

static DataValue Vector3(float x, float y, float z)
{
    DataValue v = {};
    v.m_Type = DATA_TYPE_VECTOR3;
    v.m_Value.m_Vector3[0] = x;
    v.m_Value.m_Vector3[1] = y;
    v.m_Value.m_Vector3[2] = z;
    return v;
}

static DataValue Matrix()
{
    DataValue v = {};
    v.m_Type = DATA_TYPE_MATRIX4;
    for (uint32_t i = 0; i < 16; ++i)
        v.m_Value.m_Matrix4[i] = (float)i + 0.5f;
    return v;
}

// Keep single-value fixtures concise; bulk-input tests use the separate arrays directly.
static DataResult AddTestRow(HDataStore store, uint64_t type, DataGroupId group, const DataValue* values, uint32_t count, DataId* out_id, uint64_t component_id = 0)
{
    dmArray<DataValueType> types;
    dmArray<DataValueData> payloads;
    types.SetCapacity(count);
    types.SetSize(count);
    payloads.SetCapacity(count);
    payloads.SetSize(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        types[i] = values[i].m_Type;
        payloads[i] = values[i].m_Value;
    }
    return DataAddRow(store, type, group, types.Begin(), payloads.Begin(), count, out_id, component_id);
}

static DataResult Register(HDataStore store, uint64_t type, const uint64_t* tags = 0, uint32_t tag_count = 0)
{
    DataFieldDesc meta[] = { { 10, DATA_TYPE_NUMBER, 0 }, { 20, DATA_TYPE_STRING, 8 } };
    DataTableDesc desc = { type, tags, tag_count, meta, 2, 16 };
    return DataRegisterTable(store, &desc);
}

static DataResult Add(HDataStore store, uint64_t type, DataGroupId group, double number, const char* string, DataId* id)
{
    DataValue values[] = { Number(number), String(string) };
    return AddTestRow(store, type, group, values, 2, id);
}

static DataResult Load(HDataStore store, const uint8_t* bytes, uint32_t size, HDataBlobInstance* instance, DataGroupId group = 0)
{
    HDataBlob  blob;
    DataResult result = DataLoadBlob(bytes, size, &blob);
    if (result != DATA_RESULT_OK)
        return result;
    result = DataAddBlob(store, blob, group, instance);
    DataDestroyBlob(blob);
    return result;
}

static DataId FirstId(HDataStore store)
{
    DataQueryDesc desc = {};
    HDataQuery    query;
    if (DataCreateQuery(store, &desc, &query) != DATA_RESULT_OK)
        return 0;
    DataStoreLock(store);
    DataIterator it = DataQueryIter(query);
    DataId       id = DataIterNext(&it) == DATA_RESULT_OK ? DataIterGetId(&it, 0) : 0;
    DataStoreUnlock(store);
    DataDestroyQuery(query);
    return id;
}

TEST(Data, StringOwnershipAndSelfAssignment)
{
    HDataStore store = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 1));
    char   text[] = "copied";
    DataId id;
    ASSERT_EQ(DATA_RESULT_OK, Add(store, 1, 1, 2.5, text, &id));
    text[0] = 'X';
    DataValue out;
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, id, 20, &out));
    ASSERT_STREQ("copied", out.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, id, 20, &out));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, id, 20, &out));
    ASSERT_STREQ("copied", out.m_Value.m_String);
    DataValue replacement = String(text);
    ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, id, 20, &replacement));
    text[1] = 'Y';
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, id, 20, &out));
    ASSERT_STREQ("Xopied", out.m_Value.m_String);
    replacement = String(0);
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetField(store, id, 20, &replacement));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, GrowthSwapRemovalAndStaleIds)
{
    HDataStore store = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 1));
    DataId ids[257];
    for (uint32_t i = 0; i < 257; ++i)
        ASSERT_EQ(DATA_RESULT_OK, Add(store, 1, i, i, "value", &ids[i]));
    DataValue out;
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGet(store, 0, 10, &out));
    for (uint32_t i = 0; i < 257; i += 2)
    {
        ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[i]));
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataRemoveRow(store, ids[i]));
    }
    for (uint32_t i = 1; i < 257; i += 2)
    {
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[i], 10, &out));
        ASSERT_EQ((double)i, out.m_Value.m_Number);
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[i], 20, &out));
        ASSERT_STREQ("value", out.m_Value.m_String);
    }
    for (uint32_t i = 0; i < 257; i += 2)
    {
        DataId replacement;
        ASSERT_EQ(DATA_RESULT_OK, Add(store, 1, i, -1, "new", &replacement));
        ASSERT_NE(ids[i], replacement);
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGet(store, ids[i], 10, &out));
    }
    ASSERT_EQ(DATA_RESULT_OK, DataUnregisterTable(store, 1));
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 1));
    DataId replacement;
    ASSERT_EQ(DATA_RESULT_OK, Add(store, 1, 0, 42, "reregistered", &replacement));
    for (uint32_t i = 0; i < 257; ++i)
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGet(store, ids[i], 10, &out));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, LiveQueriesTagsGroupsAndCopiedDescriptors)
{
    HDataStore    store = DataCreateStore();
    uint64_t      tags[] = { 100, 200 };
    DataGroupId   groups[] = { 7, 7, 8 };
    DataQueryDesc desc = { groups, 3, tags, 2, 0, 0 };
    HDataQuery    query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    DataStoreLock(store);
    DataIterator it = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_END, DataIterNext(&it));
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 1, tags, 2));
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 2, tags, 1));
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 3, tags, 2));
    tags[0] = 999;
    groups[0] = groups[1] = groups[2] = 999;
    DataId id;
    for (uint64_t type = 1; type <= 3; ++type)
    {
        const DataGroupId row_groups[] = { 7, 7, 0, 8, 0, 7 };
        for (uint32_t row = 0; row < 6; ++row)
            ASSERT_EQ(DATA_RESULT_OK, Add(store, type, row_groups[row], row, "test", &id));
    }
    DataStoreLock(store);
    it = DataQueryIter(query);
    uint32_t   total = 0;
    DataResult result;
    while ((result = DataIterNext(&it)) == DATA_RESULT_OK)
    {
        ASSERT_NE((uint64_t)2, DataIterGetType(&it));
        for (uint32_t row = 0; row < DataIterGetCount(&it); ++row)
        {
            DataGroupId group = DataIterGetGroupId(&it, row);
            ASSERT_TRUE(group == 7 || group == 8);
            DataValue out;
            ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, DataIterGetId(&it, row), 10, &out));
            ++total;
        }
    }
    ASSERT_EQ(DATA_RESULT_END, result);
    ASSERT_EQ(8u, total);
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataUnregisterTable(store, 1));
    ASSERT_EQ(DATA_RESULT_OK, DataUnregisterTable(store, 3));
    DataStoreLock(store);
    it = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_END, DataIterNext(&it));
    uint64_t new_tags[] = { 100, 200 };
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 1, new_tags, 2));
    ASSERT_EQ(DATA_RESULT_OK, Add(store, 1, 7, 1, "new", &id));
    DataStoreLock(store);
    it = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
    ASSERT_EQ(1u, DataIterGetCount(&it));
    ASSERT_EQ(id, DataIterGetId(&it, 0));
    DataDestroyQuery(query);
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, StructuralLockBlocksMutations)
{
    HDataStore store = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 1));
    DataId id;
    ASSERT_EQ(DATA_RESULT_OK, Add(store, 1, 3, 1, "one", &id));
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, bytes, size, &size));
    HDataBlob blob;
    ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
    HDataBlobInstance instance;
    ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(store, blob, 4, &instance));
    DataQueryField field = { .m_Field = 10, .m_Type = DATA_TYPE_NUMBER };
    DataQueryDesc  desc = { .m_Fields = &field, .m_FieldCount = 1 };
    HDataQuery     query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    uint32_t handle = DataQueryFindField(query, &field);
    DataStoreLock(store);
    DataStoreLock(store);
    DataIterator batch = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&batch));
    DataRowIterator rows = DataIterRows(&batch);
    ASSERT_EQ(DATA_RESULT_OK, DataRowIterNext(&rows));
    DataId            unchanged = 123;
    HDataBlobInstance unchanged_instance = instance;
    ASSERT_EQ(DATA_RESULT_LOCKED, Register(store, 2));
    ASSERT_EQ(DATA_RESULT_LOCKED, Add(store, 1, 3, 2, "two", &unchanged));
    DataValue     values[] = { Number(2), String("two") };
    DataValueType types[] = { DATA_TYPE_NUMBER, DATA_TYPE_STRING };
    DataValueData payloads[] = { values[0].m_Value, values[1].m_Value };
    DataRowDesc   add = { .m_Group = 3, .m_Types = types, .m_Values = payloads, .m_ValueCount = 2 };
    ASSERT_EQ(DATA_RESULT_LOCKED, DataAddRows(store, 1, &add, 1, &unchanged));
    ASSERT_EQ((DataId)123, unchanged);
    ASSERT_EQ(DATA_RESULT_LOCKED, DataRemoveRow(store, id));
    ASSERT_EQ(DATA_RESULT_LOCKED, DataUnregisterTable(store, 1));
    ASSERT_EQ(DATA_RESULT_LOCKED, DataAddBlob(store, blob, 5, &unchanged_instance));
    ASSERT_EQ((uintptr_t)instance, (uintptr_t)unchanged_instance);
    ASSERT_EQ(DATA_RESULT_LOCKED, DataRemoveBlob(instance));
    ASSERT_EQ(DATA_RESULT_LOCKED, DataDestroyStore(store));
    *DataRowIterGetNumberMut(&rows, handle) = 9;
    ASSERT_EQ(9.0, *DataRowIterGetNumber(&rows, handle));
    ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, id));
    ASSERT_EQ(1.0, *DataRowIterGetNumber(&rows, handle));
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_LOCKED, DataRemoveRow(store, id));
    ASSERT_EQ(1.0, *DataRowIterGetNumber(&rows, handle));
    // Early traversal exit still releases the outer lock before removing storage.
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, id));
    ASSERT_EQ(DATA_RESULT_OK, DataUnregisterTable(store, 1));
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveBlob(instance));
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 2));
    ASSERT_EQ(DATA_RESULT_OK, Add(store, 2, 3, 2, "two", &id));
    ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(store, blob, 5, &instance));
    DataDestroyQuery(query);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    DataDestroyBlob(blob);
    delete[] bytes;
}

TEST(Data, NestedQueriesAndWritesPreserveBatches)
{
    HDataStore store = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 1));
    DataFieldDesc meta[] = { { 10, DATA_TYPE_NUMBER, 8 }, { 20, DATA_TYPE_STRING, 24 } };
    DataTableDesc table = { 2, 0, 0, meta, 2, 32 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    DataId ids[2][4];
    for (uint32_t t = 0; t < 2; ++t)
        for (uint32_t r = 0; r < 4; ++r)
            ASSERT_EQ(DATA_RESULT_OK, Add(store, t + 1, r == 1 ? 8 : 7, t * 10 + r, "base", &ids[t][r]));

    DataGroupId    group = 7;
    DataQueryField fields[] = { { 10, DATA_TYPE_NUMBER }, { 20, DATA_TYPE_STRING } };
    DataQueryDesc  desc = { &group, 1, 0, 0, fields, 2 };
    HDataQuery     query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    // A second query and iterator must not replace the first iterator's batch state.
    DataQueryField other_fields[] = { fields[1], fields[0] };
    desc.m_Fields = other_fields;
    HDataQuery other_query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &other_query));
    DataStoreLock(store);
    DataIterator it = DataQueryIter(query);
    uint32_t     visited = 0, batches = 0;
    while (DataIterNext(&it) == DATA_RESULT_OK)
    {
        ++batches;
        DataIterator other = DataQueryIter(other_query);
        while (DataIterNext(&other) == DATA_RESULT_OK)
        {
            const char* string;
            ASSERT_EQ(DATA_RESULT_OK, GetTestFieldString(&other, 0, 0, &string));
            ASSERT_STREQ("base", string);
        }
        uint32_t t = (uint32_t)DataIterGetType(&it) - 1;
        for (uint32_t row = 0; row < DataIterGetCount(&it); ++row)
        {
            DataId   id = DataIterGetId(&it, row);
            uint32_t r = 0;
            while (r < 4 && ids[t][r] != id)
                ++r;
            ASSERT_TRUE(r < 4 && r != 1);
            ASSERT_EQ(group, DataIterGetGroupId(&it, row));
            double number = -1;
            ASSERT_EQ(DATA_RESULT_OK, GetTestFieldNumber(&it, row, 0, &number));
            ASSERT_EQ((double)(t * 10 + r), number);
            ASSERT_EQ(DATA_RESULT_OK, SetTestFieldNumber(&it, row, 0, 99));
            ASSERT_EQ(DATA_RESULT_OK, GetTestFieldNumber(&it, row, 0, &number));
            ASSERT_EQ(99.0, number);
            ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, id));
            ASSERT_EQ(DATA_RESULT_OK, GetTestFieldNumber(&it, row, 0, &number));
            ASSERT_EQ((double)(t * 10 + r), number);
            ++visited;
        }
    }
    ASSERT_EQ(6u, visited);
    ASSERT_EQ(4u, batches);
    double number = -1;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, GetTestFieldNumber(&it, 0, 0, &number));
    ASSERT_EQ(-1.0, number);

    DataDestroyQuery(other_query);
    DataDestroyQuery(query);
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, BoundFieldQueries)
{
    HDataStore     store = DataCreateStore();
    uint64_t       tag = 8;
    DataGroupId    group = 7;
    DataQueryField fields[] = { { 20, DATA_TYPE_STRING }, { 10, DATA_TYPE_NUMBER } };
    DataQueryDesc  desc = { &group, 1, &tag, 1, fields, 2 };
    HDataQuery     query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    fields[0].m_Field = 999; // Query owns the original filter/binding order.
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 1, &tag, 1));
    DataFieldDesc swapped[] = { { 20, DATA_TYPE_STRING, 0 }, { 10, DATA_TYPE_NUMBER, 8 } };
    DataTableDesc table = { 2, &tag, 1, swapped, 2, 16 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    DataFieldDesc wrong[] = { { 10, DATA_TYPE_STRING, 0 }, { 20, DATA_TYPE_STRING, 8 } };
    table.m_Type = 3;
    table.m_Fields = wrong;
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    table.m_Type = 4;
    table.m_Fields = swapped;
    table.m_FieldCount = 1;
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table)); // Missing number.
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 5));               // Missing tag.
    DataId ids[6];
    ASSERT_EQ(DATA_RESULT_OK, Add(store, 1, 7, 1, "one", &ids[0]));
    ASSERT_EQ(DATA_RESULT_OK, Add(store, 1, 8, 99, "other group", &ids[1]));
    DataValue values[] = { String("two"), Number(2) };
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(store, 2, 7, values, 2, &ids[2]));
    values[1] = String("wrong kind");
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(store, 3, 7, values, 2, &ids[3]));
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(store, 4, 7, values, 1, &ids[4]));
    ASSERT_EQ(DATA_RESULT_OK, Add(store, 5, 7, 3, "no tag", &ids[5]));
    DataStoreLock(store);
    DataIterator it = DataQueryIter(query);
    DataValue    out = Number(123);
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataIterGetField(&it, 0, 0, &out));
    uint32_t total = 0;
    while (DataIterNext(&it) == DATA_RESULT_OK)
    {
        ASSERT_EQ(1u, DataIterGetCount(&it));
        ASSERT_EQ(DATA_RESULT_OK, DataIterGetField(&it, 0, 1, &out));
        ASSERT_EQ((double)DataIterGetType(&it), out.m_Value.m_Number);
        DataValue changed = Number(44);
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataIterSetField(&it, 0, 0, &changed));
        ASSERT_EQ(DATA_RESULT_OK, DataIterSetField(&it, 0, 1, &changed));
        ASSERT_EQ(DATA_RESULT_OK, DataIterGetFieldByHash(&it, 0, 10, &out));
        ASSERT_EQ(44.0, out.m_Value.m_Number);
        ASSERT_EQ(DATA_RESULT_OK, DataResetField(store, DataIterGetId(&it, 0), 10));
        ASSERT_EQ(DATA_RESULT_OK, DataIterGetField(&it, 0, 1, &out));
        ASSERT_EQ((double)DataIterGetType(&it), out.m_Value.m_Number);
        ASSERT_EQ(DATA_RESULT_OK, DataIterGetField(&it, 0, 0, &out));
        ASSERT_EQ(DATA_RESULT_OK, DataIterSetField(&it, 0, 0, &out)); // Borrowed self-assignment.
        ASSERT_EQ(DATA_RESULT_OK, DataResetTable(store, DataIterGetType(&it)));
        ASSERT_EQ(DATA_RESULT_OK, DataIterGetField(&it, 0, 0, &out));
        ASSERT_STREQ(DataIterGetType(&it) == 1 ? "one" : "two", out.m_Value.m_String);
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataIterGetField(&it, 1, 0, &out));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataIterGetField(&it, 0, 2, &out));
        ++total;
    }
    ASSERT_EQ(2u, total);
    it = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataUnregisterTable(store, 1));
    DataStoreLock(store);
    it = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
    ASSERT_EQ((uint64_t)2, DataIterGetType(&it));
    ASSERT_EQ(DATA_RESULT_OK, DataIterGetField(&it, 0, 1, &out));
    ASSERT_EQ(2.0, out.m_Value.m_Number);
    DataDestroyQuery(query);
    fields[0].m_Type = (DataValueType)99;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataCreateQuery(store, &desc, &query));
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, BoundPackedFields)
{
    HDataStore source = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, Register(source, 1));
    DataId id;
    ASSERT_EQ(DATA_RESULT_OK, Add(source, 1, 0, 5, "packed", &id));
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, bytes, size, &size));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(source));
    HDataStore     store = DataCreateStore();
    DataQueryField fields[] = { { 10, DATA_TYPE_NUMBER }, { 20, DATA_TYPE_STRING } };
    DataQueryDesc  desc = { 0, 0, 0, 0, fields, 2 };
    HDataQuery     query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    HDataBlobInstance instance;
    ASSERT_EQ(DATA_RESULT_OK, Load(store, bytes, size, &instance));
    DataStoreLock(store);
    DataIterator it = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
    DataValue value;
    ASSERT_EQ(DATA_RESULT_OK, DataIterGetField(&it, 0, 1, &value));
    ASSERT_STREQ("packed", value.m_Value.m_String);
    value = String("override");
    ASSERT_EQ(DATA_RESULT_OK, DataIterSetField(&it, 0, 1, &value));
    ASSERT_EQ(DATA_RESULT_OK, DataIterGetField(&it, 0, 1, &value));
    ASSERT_STREQ("override", value.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataIterGetField(&it, 0, 0, &value));
    ASSERT_EQ(5.0, value.m_Value.m_Number);
    DataResetBlob(instance);
    ASSERT_EQ(DATA_RESULT_OK, DataIterGetField(&it, 0, 1, &value));
    ASSERT_STREQ("packed", value.m_Value.m_String);
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveBlob(instance));
    DataStoreLock(store);
    it = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_END, DataIterNext(&it));
    DataDestroyQuery(query);
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    delete[] bytes;
}

TEST(Data, SharedMetadataAndDenseRows)
{
    HDataStore    store = DataCreateStore();
    DataFieldDesc meta[] = { { 10, DATA_TYPE_VECTOR3, 0 }, { 20, DATA_TYPE_NUMBER, 16 }, { 30, DATA_TYPE_NUMBER, 24 } };
    DataTableDesc desc = { 1, 0, 0, meta, 3, 32 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &desc));
    meta[0].m_Field = 999; // The construction descriptor is copied once.
    DataValue values[] = { Vector3(1, 2, 3), Number(4), Number(10) };
    DataId    ids[3];
    for (uint32_t i = 0; i < 3; ++i)
        ASSERT_EQ(DATA_RESULT_OK, AddTestRow(store, 1, i, values, 3, &ids[i]));
    DataTable* table = store->m_Tables[0];
    ASSERT_EQ(3u, table->m_Owned->m_Fields.Size());
    ASSERT_EQ(96u, table->m_Owned->m_BaseRows.Size()); // Three 32-byte rows, with no per-row names or kind tags.
    ASSERT_EQ(0, memcmp(table->m_Owned->m_BaseRows.Begin(), table->m_Owned->m_BaseRows.Begin() + 32, 32));
    ASSERT_EQ((uintptr_t)0, (uintptr_t)table->m_Owned->m_BaseValues);
    ASSERT_EQ((uintptr_t)0, (uintptr_t)table->m_Payloads);
    DataValue out;
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[1], 10, &out));
    ASSERT_EQ(2.0f, out.m_Value.m_Vector3[1]);
    DataValue changed = Vector3(0.25f, 0.5f, 0.75f);
    ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, ids[1], 10, &changed));
    ASSERT_EQ(96u, table->m_Values.Size());
    ASSERT_EQ(0, memcmp(table->m_Values.Begin(), table->m_Owned->m_BaseRows.Begin(), 32));
    ASSERT_NE(0, memcmp(table->m_Values.Begin() + 32, table->m_Owned->m_BaseRows.Begin() + 32, 32));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[1], 20, &out));
    ASSERT_EQ(4.0, out.m_Value.m_Number); // Updating color preserves the other mutable fields.
    changed = Number(3);
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetField(store, ids[1], 10, &changed));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[1], 10, &out));
    ASSERT_EQ(0.5f, out.m_Value.m_Vector3[1]);
    ASSERT_EQ(DATA_RESULT_OK, DataResetField(store, ids[1], 10));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[1], 10, &out));
    ASSERT_EQ(2.0f, out.m_Value.m_Vector3[1]);
    ASSERT_EQ(DATA_RESULT_OK, DataResetTable(store, 1));
    ASSERT_EQ((uintptr_t)0, (uintptr_t)table->m_Payloads);
    ASSERT_EQ(0, memcmp(table->m_Values.Begin(), table->m_Owned->m_BaseRows.Begin(), 96));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, MetadataAndBulkValidation)
{
    HDataStore    store = DataCreateStore();
    DataFieldDesc meta[] = { { 1, DATA_TYPE_NUMBER, 0 }, { 2, DATA_TYPE_BOOLEAN, 8 } };
    DataTableDesc desc = { 1, 0, 0, meta, 2, 16 };
    meta[1].m_Offset = 7;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &desc));
    meta[1].m_Offset = 16;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &desc));
    meta[1].m_Offset = 8;
    meta[1].m_Field = 1;
    ASSERT_EQ(DATA_RESULT_ALREADY_EXISTS, DataRegisterTable(store, &desc));
    meta[1].m_Field = 2;
    meta[1].m_Type = (DataValueType)99;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &desc));
    meta[1].m_Type = DATA_TYPE_BOOLEAN;
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &desc));
    ASSERT_EQ(DATA_RESULT_ALREADY_EXISTS, DataRegisterTable(store, &desc));
    DataValueType types[] = { DATA_TYPE_NUMBER, DATA_TYPE_BOOLEAN };
    DataValueData values[] = { { .m_Number = 4 }, { .m_Boolean = 1 }, { .m_Number = 5 }, { .m_Boolean = 2 } };
    DataRowDesc   rows[] = {
        { .m_Group = 1, .m_Types = types, .m_Values = values, .m_ValueCount = 2, .m_ComponentId = 7 },
        { .m_Group = 2, .m_Types = types, .m_Values = values + 2, .m_ValueCount = 2, .m_ComponentId = 8 }
    };
    DataId ids[] = { 123, 456 };
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataAddRows(store, 1, rows, 2, ids));
    ASSERT_EQ((DataId)123, ids[0]);
    ASSERT_EQ((DataId)456, ids[1]);
    ASSERT_EQ(0u, store->m_Tables[0]->m_Rows.Size());
    values[3].m_Boolean = 0;
    rows[1].m_ValueCount = 1;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataAddRows(store, 1, rows, 2, ids));
    rows[1].m_ValueCount = 2;
    types[0] = DATA_TYPE_STRING;
    values[2].m_String = "wrong kind";
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataAddRows(store, 1, rows, 2, ids));
    types[0] = DATA_TYPE_NUMBER;
    values[2].m_Number = 5;
    rows[1].m_Types = 0;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataAddRows(store, 1, rows, 2, ids));
    rows[1].m_Types = types;
    rows[1].m_Values = 0;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataAddRows(store, 1, rows, 2, ids));
    rows[1].m_Values = values + 2;
    ASSERT_EQ(DATA_RESULT_OK, DataAddRows(store, 1, rows, 2, ids));
    ASSERT_EQ(32u, store->m_Tables[0]->m_Owned->m_BaseRows.Size());
    values[0].m_Number = 99;
    types[0] = DATA_TYPE_STRING; // Insertion no longer depends on either input array.
    DataValue out = Number(123);
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[0], 1, &out));
    ASSERT_EQ(4.0, out.m_Value.m_Number);
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGet(store, ids[0], 999, &out));
    ASSERT_EQ(4.0, out.m_Value.m_Number);
    ASSERT_EQ(DATA_RESULT_OK, DataAddRows(store, 1, 0, 0, 0));
    ASSERT_EQ(DATA_RESULT_OK, DataAddRows(0, 999, 0, 0, 0));
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataAddRows(store, 1, 0, 1, ids));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, BulkAppendPreservesDefaultsAndReusesSlots)
{
    DataFieldDesc fields[] = {
        { .m_Field = 10, .m_Type = DATA_TYPE_NUMBER, .m_Offset = 0 },
        { .m_Field = 20, .m_Type = DATA_TYPE_BOOLEAN, .m_Offset = 8 },
        { .m_Field = 30, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 12 },
        { .m_Field = 40, .m_Type = DATA_TYPE_VECTOR4, .m_Offset = 24 },
        { .m_Field = 50, .m_Type = DATA_TYPE_MATRIX4, .m_Offset = 40 }
    };
    const DataValueType types[] = { DATA_TYPE_NUMBER, DATA_TYPE_BOOLEAN, DATA_TYPE_VECTOR3, DATA_TYPE_VECTOR4, DATA_TYPE_MATRIX4 };
    DataTableDesc       table = { .m_Type = 1, .m_Fields = fields, .m_FieldCount = 5, .m_RowStride = 104 };
    HDataStore          store = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    DataValueData values[3][5] = {};
    DataRowDesc   rows[3] = {};
    for (uint32_t r = 0; r < 3; ++r)
    {
        values[r][0].m_Number = 100 + r;
        values[r][1].m_Boolean = r & 1;
        for (uint32_t f = 2; f < 5; ++f)
            for (uint32_t v = 0; v < DataTypeSize(types[f]) / sizeof(float); ++v)
                values[r][f].m_Matrix4[v] = (float)(100 * r + 10 * f + v);
        rows[r] = { .m_Group = 20 + r, .m_Types = types, .m_Values = values[r], .m_ValueCount = 5, .m_ComponentId = 30 + r };
    }
    DataId old_ids[2], ids[] = { 123, 456, 789 };
    ASSERT_EQ(DATA_RESULT_OK, DataAddRows(store, 1, rows, 2, old_ids));
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, old_ids[0]));
    values[2][1].m_Boolean = 2;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataAddRows(store, 1, rows, 3, ids));
    ASSERT_EQ((DataId)123, ids[0]);
    ASSERT_EQ((DataId)789, ids[2]);
    ASSERT_EQ(1u, store->m_Tables[0]->m_Rows.Size());
    ASSERT_EQ(2u, store->m_Tables[0]->m_Rows.Size());
    values[2][1].m_Boolean = 0;
    ASSERT_EQ(DATA_RESULT_OK, DataAddRows(store, 1, rows, 3, ids));
    ASSERT_EQ((uint32_t)old_ids[0], (uint32_t)ids[0]); // Reused slot with a new generation, followed by fresh slots.
    ASSERT_NE(old_ids[0], ids[0]);
    ASSERT_EQ(4u, store->m_Slots.Size());
    double number;
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGetNumber(store, old_ids[0], 10, &number));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, old_ids[1], 10, &number));
    ASSERT_EQ(101.0, number);
    for (uint32_t r = 0; r < 3; ++r)
    {
        const DataSlot* slot = FindSlot(store, ids[r]);
        ASSERT_EQ((DataGroupId)(20 + r), slot->m_Table->m_Rows[slot->m_Row].m_Group);
        ASSERT_EQ((uint64_t)(30 + r), DataGetComponentId(store, ids[r]));
        ASSERT_EQ(DATA_RESULT_OK, DataSetFieldNumber(store, ids[r], 10, -1));
        ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, ids[r]));
        for (uint32_t f = 0; f < 5; ++f)
        {
            DataValue value;
            ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[r], fields[f].m_Field, &value));
            ASSERT_EQ(0, memcmp(&values[r][f], &value.m_Value, DataTypeSize(types[f])));
        }
        const uint8_t* bytes = GetFieldBytes(slot->m_Table, &slot->m_Table->m_Rows[slot->m_Row], 0);
        ASSERT_EQ(0, bytes[9] | bytes[10] | bytes[11]); // Padding is initialized for every appended row.
    }
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, InlineBulkValidationIsAtomicAcrossMemberOrders)
{
    DataFieldDesc members[] = {
        { .m_Field = 10, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 0, .m_Name = "10" },
        { .m_Field = 20, .m_Type = DATA_TYPE_BOOLEAN, .m_Offset = 12, .m_Name = "20" }
    };
    DataStructDesc layout = { .m_Fields = members, .m_FieldCount = 2, .m_Size = 16 };
    DataFieldDesc  field = { .m_Field = 1, .m_Type = DATA_TYPE_STRUCT, .m_Struct = &layout, .m_Name = "1" };
    DataTableDesc  table = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = 16 };
    HDataStore     store = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    uint64_t      names[] = { 10, 20, 20, 10 };
    DataValueType types[] = { DATA_TYPE_VECTOR3, DATA_TYPE_BOOLEAN, DATA_TYPE_BOOLEAN, DATA_TYPE_VECTOR3 };
    DataValueData children[] = { { .m_Vector3 = { 1, 2, 3 } }, { .m_Boolean = 1 }, { .m_Boolean = 2 }, { .m_Vector3 = { 4, 5, 6 } } };
    DataValueData values[] = {
        { .m_Struct = { .m_Count = 2, .m_Source = DATA_STRUCT_ARRAY, .m_Array = { .m_Names = names, .m_Types = types, .m_Values = children } } },
        { .m_Struct = { .m_Count = 2, .m_Source = DATA_STRUCT_ARRAY, .m_Array = { .m_Names = names + 2, .m_Types = types + 2, .m_Values = children + 2 } } }
    };
    const DataValueType kind = DATA_TYPE_STRUCT;
    DataRowDesc         rows[] = {
        { .m_Types = &kind, .m_Values = values, .m_ValueCount = 1 },
        { .m_Types = &kind, .m_Values = values + 1, .m_ValueCount = 1 }
    };
    DataId ids[] = { 123, 456 };
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataAddRows(store, 1, rows, 2, ids));
    ASSERT_EQ((DataId)123, ids[0]);
    ASSERT_EQ((DataId)456, ids[1]);
    ASSERT_EQ(0u, store->m_Tables[0]->m_Rows.Size());
    ASSERT_EQ(0u, store->m_Tables[0]->m_Rows.Size());
    children[2].m_Boolean = 0;
    types[3] = DATA_TYPE_NUMBER;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataAddRows(store, 1, rows, 2, ids));
    types[3] = DATA_TYPE_VECTOR3;
    names[3] = 99;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataAddRows(store, 1, rows, 2, ids));
    names[3] = 10;
    ASSERT_EQ((DataId)123, ids[0]);
    ASSERT_EQ((DataId)456, ids[1]);
    ASSERT_EQ(0u, store->m_Tables[0]->m_Rows.Size());
    ASSERT_EQ(0u, store->m_Tables[0]->m_Rows.Size());
    ASSERT_EQ(DATA_RESULT_OK, DataAddRows(store, 1, rows, 2, ids));
    for (uint32_t i = 0; i < 2; ++i)
    {
        DataValue object, color, enabled;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[i], 1, &object));
        ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&object.m_Value.m_Struct, 10, &color));
        ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&object.m_Value.m_Struct, 20, &enabled));
        ASSERT_EQ(i ? 6.0f : 3.0f, color.m_Value.m_Vector3[2]);
        ASSERT_EQ(i ? 0 : 1, enabled.m_Value.m_Boolean);
    }
    // Borrowed inline views keep working through the general input path.
    HDataStore copy = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(copy, &table));
    for (uint32_t i = 0; i < 2; ++i)
    {
        DataValue view;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[i], 1, &view));
        values[i] = view.m_Value;
    }
    DataId copied[2];
    ASSERT_EQ(DATA_RESULT_OK, DataAddRows(copy, 1, rows, 2, copied));
    for (uint32_t i = 0; i < 2; ++i)
    {
        DataValue object, color, enabled;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(copy, copied[i], 1, &object));
        ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&object.m_Value.m_Struct, 10, &color));
        ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&object.m_Value.m_Struct, 20, &enabled));
        ASSERT_EQ(i ? 6.0f : 3.0f, color.m_Value.m_Vector3[2]);
        ASSERT_EQ(i ? 0 : 1, enabled.m_Value.m_Boolean);
    }
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(copy));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, CApiStoreLifecycle)
{
    ASSERT_EQ(0, TestDataStoreLifecycleFromC());
}

TEST(Data, CApiMathAndNestedViews)
{
    HDataStore    store = DataCreateStore();
    DataFieldDesc meta[] = { { 42, DATA_TYPE_NUMBER, 0 }, { 43, DATA_TYPE_VECTOR3, 8 }, { 44, DATA_TYPE_VECTOR4, 20 }, { 45, DATA_TYPE_MATRIX4, 36 }, { 46, DATA_TYPE_STRUCT, 104 }, { 47, DATA_TYPE_BOOLEAN, 112 }, { 48, DATA_TYPE_STRING, 120 } };
    uint64_t      tag = 7;
    DataTableDesc desc = { 1, &tag, 1, meta, 7, 128 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &desc));
    DataValue vec4 = {};
    vec4.m_Type = DATA_TYPE_VECTOR4;
    uint64_t            color_name = 10;
    const DataValueType color_type = DATA_TYPE_VECTOR3;
    DataValueData       color = { .m_Vector3 = { 1.0f, 0.5f, 0.25f } };
    DataValue           values[] = { Number(17), Vector3(1, 1, 1), vec4, Matrix(), Struct(&color_name, &color_type, &color, 1), Boolean(0), String("initial") };
    DataId              id;
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(store, 1, 123, values, 7, &id));
    ASSERT_EQ(0, TestDataFromC(store));
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, bytes, size, &size));
    HDataStore        loaded = DataCreateStore();
    HDataBlobInstance instance;
    ASSERT_EQ(DATA_RESULT_OK, Load(loaded, bytes, size, &instance, 123));
    DataMatrix4 loaded_matrix;
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetMatrix4(loaded, FirstId(loaded), 45, &loaded_matrix));
    ASSERT_EQ(44.0f, loaded_matrix.m_Values[3 * 4 + 2]);
    DataValue out;
    for (uint32_t i = 1; i <= 3; ++i)
    {
        DataValue original;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, id, meta[i].m_Field, &original));
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(loaded, FirstId(loaded), meta[i].m_Field, &out));
        ASSERT_EQ(original.m_Type, out.m_Type);
        ASSERT_EQ(0, memcmp(&original.m_Value, &out.m_Value, DataTypeSize(original.m_Type)));
    }
    ASSERT_EQ(DATA_RESULT_OK, DataResetTable(store, 1));
    for (uint32_t i = 1; i <= 3; ++i)
    {
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, id, meta[i].m_Field, &out));
        ASSERT_EQ(values[i].m_Type, out.m_Type);
        ASSERT_EQ(0, memcmp(&values[i].m_Value, &out.m_Value, DataTypeSize(out.m_Type)));
    }
    ASSERT_EQ(0, TestDataFromC(store));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(loaded));
    delete[] bytes;
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, AlignedMathReadsPreserveFloatBits)
{
    // Zero, negative zero, subnormal, infinities and a NaN with a payload.
    const uint32_t bits[] = { 0, 0x80000000, 1, 0x7f800000, 0xff800000, 0x7fc01234, 0x3f800000, 0xbf800000, 0, 0x80000000, 1, 0x7f800000, 0xff800000, 0x7fc01234, 0x3f800000, 0xbf800000 };
    DataFieldDesc  meta[] = { { 10, DATA_TYPE_VECTOR3, 0 }, { 20, DATA_TYPE_VECTOR4, 12 }, { 30, DATA_TYPE_MATRIX4, 28 } };
    DataTableDesc  desc = { 1, 0, 0, meta, 3, 92 };
    DataValue      values[3] = {};
    for (uint32_t i = 0; i < 3; ++i)
    {
        values[i].m_Type = meta[i].m_Type;
        memcpy(&values[i].m_Value, bits, DataTypeSize(meta[i].m_Type));
    }
    HDataStore source = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &desc));
    DataId id;
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, 1, 0, values, 3, &id));
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, bytes, size, &size));
    HDataStore        loaded = DataCreateStore();
    HDataBlobInstance instance;
    ASSERT_EQ(DATA_RESULT_OK, Load(loaded, bytes, size, &instance));
    HDataStore stores[] = { source, loaded };
    for (uint32_t mode = 0; mode < 2; ++mode)
    {
        HDataStore store = stores[mode];
        DataId     row = FirstId(store);
        // Read defaults, then the same bit patterns through override storage.
        for (uint32_t overridden = 0; overridden < 2; ++overridden)
        {
            for (uint32_t i = 0; i < 3; ++i)
            {
                if (overridden)
                    ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, row, meta[i].m_Field, &values[i]));
                DataValue value;
                ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, row, meta[i].m_Field, &value));
                ASSERT_EQ(meta[i].m_Type, value.m_Type);
                ASSERT_EQ(0, memcmp(bits, &value.m_Value, DataTypeSize(value.m_Type)));
            }
            DataVector3 vector3;
            DataVector4 vector4;
            DataMatrix4 matrix;
            ASSERT_EQ(DATA_RESULT_OK, DataFieldGetVector3(store, row, 10, &vector3));
            ASSERT_EQ(DATA_RESULT_OK, DataFieldGetVector4(store, row, 20, &vector4));
            ASSERT_EQ(DATA_RESULT_OK, DataFieldGetMatrix4(store, row, 30, &matrix));
            ASSERT_EQ(0, memcmp(bits, vector3.m_Values, sizeof(vector3.m_Values)));
            ASSERT_EQ(0, memcmp(bits, vector4.m_Values, sizeof(vector4.m_Values)));
            ASSERT_EQ(0, memcmp(bits, matrix.m_Values, sizeof(matrix.m_Values)));
            DataQueryField fields[] = { { 30, DATA_TYPE_MATRIX4 }, { 10, DATA_TYPE_VECTOR3 }, { 20, DATA_TYPE_VECTOR4 } };
            DataQueryDesc  query_desc = { 0, 0, 0, 0, fields, 3 };
            HDataQuery     query;
            ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &query_desc, &query));
            uint32_t matrix_field = DataQueryFindField(query, &fields[0]);
            uint32_t vector3_field = DataQueryFindField(query, &fields[1]);
            uint32_t vector4_field = DataQueryFindField(query, &fields[2]);
            ASSERT_NE(UINT32_MAX, matrix_field);
            ASSERT_NE(UINT32_MAX, vector3_field);
            ASSERT_NE(UINT32_MAX, vector4_field);
            DataStoreLock(store);
            DataIterator it = DataQueryIter(query);
            ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
            DataRowIterator rows = DataIterRows(&it);
            ASSERT_EQ(DATA_RESULT_OK, DataRowIterNext(&rows));
            ASSERT_EQ(0, memcmp(bits, DataRowIterGetMatrix4(&rows, matrix_field), sizeof(DataMatrix4)));
            ASSERT_EQ(0, memcmp(bits, DataRowIterGetVector3(&rows, vector3_field), sizeof(DataVector3)));
            ASSERT_EQ(0, memcmp(bits, DataRowIterGetVector4(&rows, vector4_field), sizeof(DataVector4)));
            DataStoreUnlock(store);
            DataDestroyQuery(query);
        }
    }
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(loaded));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(source));
    delete[] bytes;
}

TEST(Data, BinaryGoldenSharedMetadata)
{
    // Two eight-byte rows share one 32-byte metadata entry.
    // clang-format off
    const uint8_t DM_ALIGNED(8) expected[] = {
        'D','M','D','T',1,0,0,0, 1,0,0,0,112,0,0,0, 24,0,0,0,0,0,0,0,
        1,0,0,0,0,0,0,0, 0,0,1,0,2,0,0,0, 8,0,0,0,1,0,0,0,
        10,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,8,0,0,0, 10,0,0,0,0,0,0,0,
        7,0,0,0,0,0,0,0, 8,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0xf8,0x3f, 0,0,0,0,0,0,0,0xc0
    };
    // clang-format on
    HDataStore    store = DataCreateStore();
    DataFieldDesc meta = { 10, DATA_TYPE_NUMBER, 0 };
    DataTableDesc desc = { 1, 0, 0, &meta, 1, 8 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &desc));
    const DataValueType types[] = { DATA_TYPE_NUMBER };
    DataValueData       values[] = { { .m_Number = 1.5 }, { .m_Number = -2 } };
    DataId              ids[2];
    DataRowDesc         rows[] = {
        { .m_Group = 100, .m_Types = types, .m_Values = values, .m_ValueCount = 1, .m_ComponentId = 7 },
        { .m_Group = 200, .m_Types = types, .m_Values = values + 1, .m_ValueCount = 1, .m_ComponentId = 8 }
    };
    ASSERT_EQ(DATA_RESULT_OK, DataAddRows(store, 1, rows, 2, ids));
    uint8_t  DM_ALIGNED(8) bytes[sizeof(expected)];
    uint32_t size = 0;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, 0, 0, &size));
    ASSERT_EQ(sizeof(expected), (size_t)size);
    memset(bytes, 0xff, sizeof(bytes));
    ASSERT_EQ(DATA_RESULT_BUFFER_TOO_SMALL, DataWriteBlob(store, bytes, size - 1, &size));
    for (uint32_t i = 0; i < sizeof(bytes); ++i)
        ASSERT_EQ(0xff, bytes[i]);
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, bytes, size, &size));
    ASSERT_EQ(0, memcmp(expected, bytes, size));
    HDataStore        loaded = DataCreateStore();
    HDataBlobInstance instance;
    ASSERT_EQ(DATA_RESULT_OK, Load(loaded, expected, sizeof(expected), &instance));
    ASSERT_EQ((uintptr_t)(expected + 96), (uintptr_t)(GetInstanceTable(instance, 0)->m_Blob + GetInstanceTable(instance, 0)->m_Offsets.m_Rows));
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(loaded, bytes, size, &size));
    ASSERT_EQ(0, memcmp(expected, bytes, size));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(loaded));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, BlobMutableRowsSharedMetadataAndGroupLifetime)
{
    HDataStore source = DataCreateStore();
    uint64_t   tag = dmHashString64("light");
    ASSERT_EQ(DATA_RESULT_OK, Register(source, 1, &tag, 1));
    ASSERT_EQ(DATA_RESULT_OK, Register(source, 2, &tag, 1));
    DataValue values[] = { Number(1), String("shared") };
    DataId    id;
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, 1, 999, values, 2, &id, 11));
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, 1, 999, values, 2, &id, 12));
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, 2, 999, values, 2, &id, 13));
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, 0, 0, &size));
    uint8_t* memory = new uint8_t[size];
    uint8_t* bytes = memory;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, bytes, size, &size));
    uint8_t* unchanged = new uint8_t[size];
    memcpy(unchanged, bytes, size);
    HDataBlob blob;
    ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
    ASSERT_EQ((uintptr_t)bytes, (uintptr_t)blob->m_Data);
    HDataStore    store = DataCreateStore();
    DataQueryDesc query_desc = { 0, 0, &tag, 1, 0, 0 };
    HDataQuery    query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &query_desc, &query));
    HDataBlobInstance first, second;
    ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(store, blob, 100, &first));
    ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(store, blob, 200, &second));
    ASSERT_EQ(2u, store->m_Tables.Size());
    ASSERT_EQ((uintptr_t)(GetInstanceTable(first, 0)->m_Blob + GetInstanceTable(first, 0)->m_Offsets.m_Table), (uintptr_t)(GetInstanceTable(second, 0)->m_Blob + GetInstanceTable(second, 0)->m_Offsets.m_Table));
    ASSERT_EQ((uintptr_t)(GetInstanceTable(first, 0)->m_Blob + GetInstanceTable(first, 0)->m_Offsets.m_Rows), (uintptr_t)(GetInstanceTable(second, 0)->m_Blob + GetInstanceTable(second, 0)->m_Offsets.m_Rows));
    ASSERT_EQ((uintptr_t)GetInstanceTable(first, 0), (uintptr_t)GetInstanceTable(second, 0));
    ASSERT_EQ(4u, GetInstanceTable(first, 0)->m_Rows.Size());
    for (uint32_t t = 0; t < store->m_Tables.Size(); ++t)
    {
        DataTable* table = store->m_Tables[t];
        ASSERT_TRUE(table->m_Pool != 0);
        ASSERT_EQ((uintptr_t)bytes, (uintptr_t)table->m_Blob);
        ASSERT_EQ((uintptr_t)0, (uintptr_t)table->m_Payloads);
    }
    DataId ids[2][3] = {};
    DataStoreLock(store);
    DataIterator it = DataQueryIter(query);
    uint32_t     total = 0;
    uintptr_t    string = (uintptr_t)(bytes + ReadDataInteger(bytes + 12, 4));
    while (DataIterNext(&it) == DATA_RESULT_OK)
    {
        for (uint32_t r = 0; r < DataIterGetCount(&it); ++r)
        {
            DataId   row = DataIterGetId(&it, r);
            uint64_t component = DataGetComponentId(store, row);
            uint32_t group = DataIterGetGroupId(&it, r) == 100 ? 0 : 1;
            ASSERT_TRUE(component >= 11 && component <= 13);
            ids[group][component - 11] = row;
            DataValue out;
            ASSERT_EQ(DATA_RESULT_OK, DataIterGetFieldByHash(&it, r, 20, &out));
            ASSERT_EQ(string, (uintptr_t)out.m_Value.m_String);
            ++total;
        }
    }
    ASSERT_EQ(6u, total);
    ASSERT_NE(ids[0][0], ids[1][0]);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(source));
    DataDestroyBlob(blob);
    DataValue change = String("override");
    ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, ids[0][0], 20, &change));
    ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, ids[0][1], 20, &change));
    ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, ids[0][2], 20, &change));
    ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, ids[1][0], 20, &change));
    DataValue out;
    it = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
    ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, ids[0][0]));
    ASSERT_EQ(DATA_RESULT_OK, DataIterGetFieldByHash(&it, 0, 20, &out));
    ASSERT_EQ(string, (uintptr_t)out.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[0][1], 20, &out));
    ASSERT_STREQ("override", out.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[0][2], 20, &out));
    ASSERT_STREQ("override", out.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[1][0], 20, &out));
    ASSERT_STREQ("override", out.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, ids[0][0], 20, &change));
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[0][1]));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataResetRow(store, ids[0][1]));
    DataStoreLock(store);
    it = DataQueryIter(query);
    DataResetBlob(first);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGet(store, ids[0][1], 20, &out));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[0][0], 20, &out));
    ASSERT_EQ(string, (uintptr_t)out.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[0][2], 20, &out));
    ASSERT_EQ(string, (uintptr_t)out.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[1][0], 20, &out));
    ASSERT_STREQ("override", out.m_Value.m_String);
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveBlob(first));
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGet(store, ids[0][i], 20, &out));
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[1][i], 20, &out));
    }
    ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, ids[1][0], 20, &out));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[1][0], 20, &out));
    ASSERT_NE(string, (uintptr_t)out.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataResetField(store, ids[1][0], 20));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[1][0], 20, &out));
    ASSERT_EQ(string, (uintptr_t)out.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveBlob(second));
    DataStoreLock(store);
    it = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_END, DataIterNext(&it));
    DataDestroyQuery(query);
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    ASSERT_EQ(0, memcmp(unchanged, bytes, size));
    delete[] unchanged;
    delete[] memory;
}

TEST(Data, NestedViewsAllKindsAndReset)
{
    DataValue vec4 = {};
    vec4.m_Type = DATA_TYPE_VECTOR4;
    vec4.m_Value.m_Vector4[3] = 8.0f;
    DataValue           elements[] = { Null(), Number(1.5), Boolean(1), String("shared"), Vector3(1, 2, 3), vec4, Matrix(), List(0, 0), Struct(0, 0, 0, 0) };
    const DataValueType types[] = { DATA_TYPE_LIST, DATA_TYPE_STRING };
    DataValueData       fields[] = { List(elements, 9).m_Value, String("shared").m_Value };
    uint64_t            names[] = { 10, 20 };
    DataValue           value = Struct(names, types, fields, 2);
    DataFieldDesc       meta = { 1, DATA_TYPE_STRUCT, 0 };
    DataTableDesc       desc = { 1, 0, 0, &meta, 1, 8 };
    HDataStore          source = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &desc));
    DataId id;
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, 1, 0, &value, 1, &id));
    elements[1] = Number(99);
    names[0] = 99;
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    uint8_t* roundtrip = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, bytes, size, &size));
    HDataStore        store = DataCreateStore();
    HDataBlobInstance instance;
    ASSERT_EQ(DATA_RESULT_OK, Load(store, bytes, size, &instance));
    id = FirstId(store);
    DataValue object, list, child, shared;
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, id, 1, &object));
    ASSERT_EQ((uintptr_t)(bytes), (uintptr_t)object.m_Value.m_Struct.m_View.m_Buffer);
    ASSERT_EQ(DATA_STRUCT_PACKED, object.m_Value.m_Struct.m_Source);
    ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&object.m_Value.m_Struct, 10, &list));
    ASSERT_EQ(9u, list.m_Value.m_List.m_Count);
    for (uint32_t i = 0; i < 9; ++i)
    {
        ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(&list.m_Value.m_List, i, &child));
        ASSERT_EQ(elements[i].m_Type, child.m_Type);
        if (i == 1)
            ASSERT_EQ(1.5, child.m_Value.m_Number);
        if (i == 4)
            ASSERT_EQ(3.0f, child.m_Value.m_Vector3[2]);
        if (i == 5)
            ASSERT_EQ(8.0f, child.m_Value.m_Vector4[3]);
        if (i == 6)
            for (uint32_t j = 0; j < 16; ++j)
                ASSERT_EQ((float)j + 0.5f, child.m_Value.m_Matrix4[j]);
    }
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataGetListValue(&list.m_Value.m_List, 9, &child));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataGetStructField(&object.m_Value.m_Struct, 99, &child));
    ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(&list.m_Value.m_List, 3, &child));
    ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&object.m_Value.m_Struct, 20, &shared));
    ASSERT_EQ((uintptr_t)child.m_Value.m_String, (uintptr_t)shared.m_Value.m_String);
    ASSERT_TRUE((const uint8_t*)shared.m_Value.m_String >= bytes && (const uint8_t*)shared.m_Value.m_String < bytes + size);
    ASSERT_EQ((uintptr_t)(bytes), (uintptr_t)GetInstanceTable(instance, 0)->m_Blob); // Values remain borrowed.
    ASSERT_EQ((uintptr_t)0, (uintptr_t)instance->m_Payloads);
    ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, id, 1, &object)); // Copy only when explicitly setting.
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, id, 1, &child));
    ASSERT_TRUE(child.m_Value.m_Struct.m_View.m_Buffer != object.m_Value.m_Struct.m_View.m_Buffer);
    ASSERT_EQ(DATA_STRUCT_ROW, child.m_Value.m_Struct.m_Source);
    ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, id, 1, &child)); // Owned view self-assignment.
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, id, 1, &child));
    ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&child.m_Value.m_Struct, 20, &shared));
    ASSERT_STREQ("shared", shared.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataResetField(store, id, 1));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, id, 1, &child));
    ASSERT_EQ(object.m_Value.m_Struct.m_View.m_Offset, child.m_Value.m_Struct.m_View.m_Offset);
    DataResetBlob(instance);
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, roundtrip, size, &size));
    ASSERT_EQ(0, memcmp(bytes, roundtrip, size));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(source));
    delete[] bytes;
    delete[] roundtrip;
}

TEST(Data, CompactNestedBulkStorageAndBorrowedInsertion)
{
    HDataStore    store = DataCreateStore();
    DataFieldDesc meta = { 1, DATA_TYPE_STRUCT, 0 };
    DataTableDesc desc = { 1, 0, 0, &meta, 1, 8 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &desc));
    uint64_t            names[] = { 10, 20 };
    const DataValueType types[] = { DATA_TYPE_VECTOR3, DATA_TYPE_NUMBER };
    DataValueData       fields[] = { { .m_Vector3 = { 1, 2, 3 } }, { .m_Number = 4 } };
    DataValue           value = Struct(names, types, fields, 2);
    DataRowDesc         rows[128];
    DataId              ids[128];
    for (uint32_t i = 0; i < 128; ++i)
    {
        DataRowDesc row = { .m_Group = i, .m_Types = &value.m_Type, .m_Values = &value.m_Value, .m_ValueCount = 1 };
        rows[i] = row;
    }
    ASSERT_EQ(DATA_RESULT_OK, DataAddRows(store, 1, rows, 128, ids));
    DataMemoryStats memory;
    GetDataMemoryStats(store, &memory);
    ASSERT_EQ((uint64_t)0, memory.m_BaseBlocks);
    ASSERT_EQ((uint64_t)128, memory.m_StructRows);
    ASSERT_EQ(1u, FindTable(store, 1)->m_Structs->m_Tables.Size());
    ASSERT_EQ(24u, FindTable(store, 1)->m_Structs->m_Tables[0]->m_RowStride);
    ASSERT_TRUE(memory.m_StructRowBytes < (uint64_t)(128 * 64));

    DataValue borrowed;
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[0], 1, &borrowed));
    ASSERT_TRUE(borrowed.m_Value.m_Struct.m_View.m_StructTable != 0);
    ASSERT_EQ(DATA_STRUCT_ROW, borrowed.m_Value.m_Struct.m_Source);
    for (uint32_t i = 0; i < 128; ++i)
        rows[i].m_Values = &borrowed.m_Value;
    // Growing row/arena storage must keep the source view valid until copying ends.
    ASSERT_EQ(DATA_RESULT_OK, DataAddRows(store, 1, rows, 128, ids));
    fields[1].m_Number = 99;
    ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, ids[0], 1, &value));
    ASSERT_EQ(DATA_RESULT_OK, DataResetTable(store, 1));
    for (uint32_t i = 0; i < 128; ++i)
    {
        DataValue object, child;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[i], 1, &object));
        ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&object.m_Value.m_Struct, 10, &child));
        ASSERT_EQ(3.0f, child.m_Value.m_Vector3[2]);
        ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&object.m_Value.m_Struct, 20, &child));
        ASSERT_EQ(4.0, child.m_Value.m_Number);
    }
    GetDataMemoryStats(store, &memory);
    ASSERT_EQ((uint64_t)0, memory.m_BaseBlocks);
    ASSERT_EQ((uint64_t)256, memory.m_StructRows);
    ASSERT_EQ((uint64_t)0, memory.m_PayloadBlocks);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, OwnedStructRowsShareLayoutsButNotValues)
{
    HDataStore    store = DataCreateStore();
    DataFieldDesc field = { .m_Field = 1, .m_Type = DATA_TYPE_STRUCT };
    DataTableDesc desc = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = 8 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &desc));
    uint64_t      names[] = { 2, 3 };
    DataValueType types[] = { DATA_TYPE_NUMBER, DATA_TYPE_VECTOR3 };
    DataValueData values[] = { { .m_Number = 10 }, { .m_Vector3 = { 1, 2, 3 } } };
    DataValue     object = Struct(names, types, values, 2);
    DataId        ids[3];
    for (uint32_t i = 0; i < 3; ++i)
        ASSERT_EQ(DATA_RESULT_OK, AddTestRow(store, 1, 0, &object, 1, &ids[i]));
    DataTable* table = FindTable(store, 1);
    ASSERT_EQ(1u, table->m_Structs->m_Tables.Size());
    DataStructTable* children = table->m_Structs->m_Tables[0];
    ASSERT_EQ(3u, children->m_RowCount);
    DataValue first, second, child;
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[0], 1, &first));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[1], 1, &second));
    ASSERT_NE(first.m_Value.m_Struct.m_View.m_Offset, second.m_Value.m_Struct.m_View.m_Offset);

    // Input member order is not part of the shared layout's identity.
    uint64_t      reordered_names[] = { 3, 2 };
    DataValueType reordered_types[] = { DATA_TYPE_VECTOR3, DATA_TYPE_NUMBER };
    DataValueData reordered_values[] = { { .m_Vector3 = { 4, 5, 6 } }, { .m_Number = 20 } };
    DataValue     replacement = Struct(reordered_names, reordered_types, reordered_values, 2);
    ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, ids[0], 1, &replacement));
    ASSERT_EQ(1u, table->m_Structs->m_Tables.Size());
    ASSERT_EQ(4u, children->m_RowCount); // Three defaults and one replacement.
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[0], 1, &first));
    ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&first.m_Value.m_Struct, 2, &child));
    ASSERT_EQ(20.0, child.m_Value.m_Number);
    ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&second.m_Value.m_Struct, 2, &child));
    ASSERT_EQ(10.0, child.m_Value.m_Number);
    ASSERT_EQ(DATA_RESULT_OK, DataResetField(store, ids[0], 1));
    ASSERT_EQ(3u, children->m_RowCount);

    uint32_t capacity = children->m_Values.Capacity();
    uint32_t slots = children->m_Values.Size();
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[1])); // Swaps the parent row only.
    ASSERT_EQ(2u, children->m_RowCount);
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[2], 1, &second));
    ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&second.m_Value.m_Struct, 2, &child));
    ASSERT_EQ(10.0, child.m_Value.m_Number);
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(store, 1, 0, &object, 1, &ids[1]));
    ASSERT_EQ(slots, children->m_Values.Size());
    ASSERT_EQ(capacity, children->m_Values.Capacity());
    for (uint32_t i = 0; i < 3; ++i)
        ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[i]));
    ASSERT_EQ(0u, children->m_RowCount);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, OwnedStructRowsReleaseRecursiveTrees)
{
    HDataStore    store = DataCreateStore();
    DataFieldDesc field = { .m_Field = 1, .m_Type = DATA_TYPE_STRUCT };
    DataTableDesc desc = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = 8 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &desc));
    uint64_t      name = 2;
    DataValueType number_type = DATA_TYPE_NUMBER;
    DataValueType struct_type = DATA_TYPE_STRUCT;
    DataValueData number = { .m_Number = 42 };
    DataValue     leaf = Struct(&name, &number_type, &number, 1);
    DataValue     middle = Struct(&name, &struct_type, &leaf.m_Value, 1);
    DataValue     root = Struct(&name, &struct_type, &middle.m_Value, 1);
    DataId        ids[40];
    for (uint32_t i = 0; i < 40; ++i)
        ASSERT_EQ(DATA_RESULT_OK, AddTestRow(store, 1, 0, &root, 1, &ids[i]));
    DataTable* table = FindTable(store, 1);
    ASSERT_EQ(2u, table->m_Structs->m_Tables.Size());
    ASSERT_EQ(80u, table->m_Structs->m_Tables[0]->m_RowCount);
    ASSERT_EQ(40u, table->m_Structs->m_Tables[1]->m_RowCount);
    DataValue view, child, grandchild;
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[0], 1, &view));
    ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, ids[0], 1, &view)); // Same-layout growth during a borrowed copy.
    ASSERT_EQ(82u, table->m_Structs->m_Tables[0]->m_RowCount);
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[0], 1, &view));
    ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&view.m_Value.m_Struct, name, &child));
    ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&child.m_Value.m_Struct, name, &grandchild));
    ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&grandchild.m_Value.m_Struct, name, &view));
    ASSERT_EQ(42.0, view.m_Value.m_Number);
    ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, ids[0]));
    ASSERT_EQ(80u, table->m_Structs->m_Tables[0]->m_RowCount);
    for (uint32_t i = 0; i < 40; ++i)
        ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[i]));
    for (uint32_t t = 0; t < table->m_Structs->m_Tables.Size(); ++t)
        ASSERT_EQ(0u, table->m_Structs->m_Tables[t]->m_RowCount);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, OwnedStructRowsKeepBlobDefaultsBorrowed)
{
    HDataStore    source = DataCreateStore();
    DataFieldDesc field = { .m_Field = 1, .m_Type = DATA_TYPE_STRUCT };
    DataTableDesc desc = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = 8 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &desc));
    uint64_t      name = 2;
    DataValueType type = DATA_TYPE_NUMBER;
    DataValueData number = { .m_Number = 10 };
    DataValue     object = Struct(&name, &type, &number, 1);
    DataId        id;
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, 1, 0, &object, 1, &id));
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    uint8_t* copy = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, bytes, size, &size));
    memcpy(copy, bytes, size);
    HDataBlob blob;
    ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
    HDataStore        store = DataCreateStore();
    HDataBlobInstance instances[2];
    for (uint32_t i = 0; i < 2; ++i)
        ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(store, blob, i, &instances[i]));
    DataTable* table = GetInstanceTable(instances[0], 0);
    ASSERT_TRUE(table->m_Structs == 0);
    DataId ids[] = { MakeId(store, GetInstanceSlots(instances[0])[0]), MakeId(store, GetInstanceSlots(instances[1])[0]) };
    number.m_Number = 20;
    for (uint32_t i = 0; i < 2; ++i)
        ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, ids[i], 1, &object));
    ASSERT_EQ(2u, table->m_Structs->m_Tables[0]->m_RowCount);
    ASSERT_EQ(DATA_RESULT_OK, DataResetBlob(instances[0]));
    ASSERT_EQ(1u, table->m_Structs->m_Tables[0]->m_RowCount);
    DataValue value, child;
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[0], 1, &value));
    ASSERT_EQ((uintptr_t)bytes, (uintptr_t)value.m_Value.m_Struct.m_View.m_Buffer);
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[1], 1, &value));
    ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&value.m_Value.m_Struct, name, &child));
    ASSERT_EQ(20.0, child.m_Value.m_Number);
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveBlob(instances[1]));
    ASSERT_EQ(0u, table->m_Structs->m_Tables[0]->m_RowCount);
    ASSERT_EQ(0, memcmp(bytes, copy, size));
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveBlob(instances[0]));
    DataDestroyBlob(blob);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(source));
    delete[] copy;
    delete[] bytes;
}

TEST(Data, NestedValidationAndDepth)
{
    HDataStore    store = DataCreateStore();
    DataFieldDesc meta = { 1, DATA_TYPE_LIST, 0 };
    DataTableDesc desc = { 1, 0, 0, &meta, 1, 8 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &desc));
    DataValue base = List(0, 0);
    DataId    id;
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(store, 1, 0, &base, 1, &id));
    DataValue invalid = List(0, 1);
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetField(store, id, 1, &invalid));
    invalid = List(&invalid, 1);
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetField(store, id, 1, &invalid));
    DataValue bad = Boolean(2);
    invalid = List(&bad, 1);
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetField(store, id, 1, &invalid));
    uint64_t      names[] = { 10, 10 };
    DataValueType types[] = { DATA_TYPE_NULL, DATA_TYPE_NULL };
    DataValueData fields[2] = {};
    DataValue     object = Struct(names, types, fields, 2);
    invalid = List(&object, 1);
    ASSERT_EQ(DATA_RESULT_ALREADY_EXISTS, DataSetField(store, id, 1, &invalid));
    names[1] = 20;
    object.m_Value.m_Struct.m_Array.m_Types = 0;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetField(store, id, 1, &invalid));
    object.m_Value.m_Struct.m_Array.m_Types = types;
    types[1] = (DataValueType)99;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetField(store, id, 1, &invalid));
    types[1] = DATA_TYPE_NULL;
    DataValue values[DATA_MAX_NESTING + 2];
    values[0] = Number(1);
    for (uint32_t i = 1; i < DATA_MAX_NESTING + 2; ++i)
        values[i] = List(&values[i - 1], 1);
    ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, id, 1, &values[DATA_MAX_NESTING]));
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetField(store, id, 1, &values[DATA_MAX_NESTING + 1]));
    uint8_t  DM_ALIGNED(8) bytes[4096];
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, bytes, sizeof(bytes), &size));
    HDataBlob blob;
    ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
    DataDestroyBlob(blob);
    // Last number becomes an empty list, which would add a 65th container.
    uint32_t container = (uint32_t)ReadDataInteger(bytes + 24 + DATA_TABLE_HEADER_SIZE + DATA_FIELD_META_SIZE + 8, 8);
    for (uint32_t i = 1; i < DATA_MAX_NESTING; ++i)
        container = (uint32_t)ReadDataInteger(bytes + container + 12, 4);
    WriteDataInteger(bytes + container + 8, DATA_TYPE_LIST, 4);
    memset(bytes + size - 8, 0, 8);
    bytes[size - 8] = 8;
    ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(bytes, size, &blob));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, MalformedBlobDirectoryLayoutAndValues)
{
    HDataStore    source = DataCreateStore();
    DataFieldDesc meta[] = { { 1, DATA_TYPE_BOOLEAN, 0 }, { 2, DATA_TYPE_STRING, 8 }, { 3, DATA_TYPE_LIST, 16 } };
    DataTableDesc desc = { 1, 0, 0, meta, 3, 24 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &desc));
    desc.m_Type = 2;
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &desc));
    DataValue item = Number(2);
    DataValue values[] = { Boolean(1), String("text"), List(&item, 1) };
    DataId    id;
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, 1, 0, values, 3, &id));
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, 2, 0, values, 3, &id));
    uint8_t  DM_ALIGNED(8) bytes[1024], saved[1024];
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, bytes, sizeof(bytes), &size));
    memcpy(saved, bytes, size);
    HDataBlob sentinel;
    ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(saved, size, &sentinel));
    for (uint32_t length = 0; length < size; ++length)
    {
        HDataBlob output = sentinel;
        ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(bytes, length, &output));
        ASSERT_EQ((uintptr_t)sentinel, (uintptr_t)output);
    }
    uint32_t second = (uint32_t)ReadDataInteger(bytes + 20, 4);
    // First table starts at 24, metadata at 48, row bytes at 152.
    const uint32_t corrupt[] = { 0, 4, 8, 12, 16, 20, 32, 34, 36, 40, 44, 46, 56, 60, 64, 66, 68, 88, 92, 96, 98, 100, 120, 124, 128, 130, 132, 152, 160, 168 };
    for (uint32_t i = 0; i < sizeof(corrupt) / sizeof(corrupt[0]); ++i)
    {
        bytes[corrupt[i]] = 0xff;
        HDataBlob output = sentinel;
        ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(bytes, size, &output));
        ASSERT_EQ((uintptr_t)sentinel, (uintptr_t)output);
        memcpy(bytes, saved, size);
    }
    bytes[second + offsetof(DataTableHeader, m_RowStride)] = 0;
    HDataBlob output = sentinel;
    ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(bytes, size, &output));
    memcpy(bytes, saved, size);
    memcpy(bytes + 80, bytes + 48, 8); // Repeated field name in shared metadata.
    ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(bytes, size, &output));
    memcpy(bytes, saved, size);
    WriteDataInteger(bytes + 92, 0, 4); // String overlaps boolean in every row.
    ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(bytes, size, &output));
    memcpy(bytes, saved, size);
    uint32_t nested = (uint32_t)ReadDataInteger(bytes + 168, 8);
    bytes[nested + 8] = 99;
    ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(bytes, size, &output));
    memcpy(bytes, saved, size);
    bytes[size - 1] = 'X';
    ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(bytes, size, &output));
    memcpy(bytes, saved, size);
    bytes[size] = 0;
    ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(bytes, size + 1, &output));
    ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(0, 0, &output));
    ASSERT_EQ((uintptr_t)sentinel, (uintptr_t)output);
    DataDestroyBlob(sentinel);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(source));
}

TEST(Data, EmptyNullAndBorrowedResourceReuse)
{
    HDataStore source = DataCreateStore();
    uint8_t    DM_ALIGNED(8) bytes[256];
    uint32_t   size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, bytes, sizeof(bytes), &size));
    ASSERT_EQ(16u, size);
    HDataBlob blob;
    ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
    HDataBlobInstance instance;
    HDataStore        store = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(store, blob, 1, &instance));
    DataResetBlob(instance);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    ASSERT_EQ(1u, blob->m_RefCount);
    DataDestroyBlob(blob);
    DataFieldDesc meta = { 1, DATA_TYPE_NULL, 0 };
    DataTableDesc desc = { 1, 0, 0, &meta, 1, 0 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &desc));
    DataValue value = Null();
    DataId    id;
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, 1, 0, &value, 1, &id));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(source, id, 1, &value));
    ASSERT_EQ(DATA_RESULT_OK, DataSetField(source, id, 1, &value));
    DataTableDesc empty = { 2, 0, 0, 0, 0, 0 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &empty));
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, 2, 0, 0, 0, &id));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGet(source, id, 1, &value));
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, bytes, sizeof(bytes), &size));
    ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
    ASSERT_EQ(2u, blob->m_TableCount);
    ASSERT_EQ(2u, blob->m_RowCount);
    store = DataCreateStore();
    HDataStore        another = DataCreateStore();
    HDataBlobInstance second;
    ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(store, blob, 1, &instance));
    ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(another, blob, 2, &second));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    ASSERT_EQ(2u, blob->m_RefCount);
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(another, FirstId(another), 1, &value));
    ASSERT_EQ(DATA_TYPE_NULL, value.m_Type);
    DataDestroyBlob(blob);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(another));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(source));
}

TEST(Data, ResetAddedValuesAllocationBlocksAndDefaults)
{
    HDataStore store = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 1));
    DataId first, second;
    ASSERT_EQ(DATA_RESULT_OK, Add(store, 1, 1, 17, "original", &first));
    DataValue value = String("new baseline");
    ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, first, 20, &value));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, first, 20, &value));
    ASSERT_EQ(DATA_RESULT_OK, Add(store, 1, 2, 23, value.m_Value.m_String, &second));
    char large[8193];
    memset(large, 'L', sizeof(large) - 1);
    large[sizeof(large) - 1] = 0;
    for (uint32_t cycle = 0; cycle < 3; ++cycle)
    {
        value = String(large);
        ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, first, 20, &value));
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, first, 20, &value));
        ASSERT_STREQ(large, value.m_Value.m_String);
        ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, first, 20, &value));
        for (uint32_t i = 0; i < 300; ++i)
        {
            value = String("replacement");
            ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, first, 20, &value));
        }
        ASSERT_EQ(DATA_RESULT_OK, DataResetTable(store, 1));
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, first, 20, &value));
        ASSERT_STREQ("original", value.m_Value.m_String);
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, second, 20, &value));
        ASSERT_STREQ("new baseline", value.m_Value.m_String);
    }
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataResetField(store, 0, 20));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataResetField(store, first, 999));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataResetTable(store, 999));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}

TEST(Data, TypedLookupHashCollisionsAndWideIndexes)
{
    const uint32_t count = 258;
    DataFieldDesc  fields[count];
    DataValue      values[count];
    for (uint32_t i = 0; i < count; ++i)
    {
        fields[i] = { .m_Field = 1 + 16 * i, .m_Type = DATA_TYPE_NUMBER, .m_Offset = i * 16 };
        values[i] = Number(i);
    }
    // Colliding names have different kinds; two direct candidates exceed 255.
    fields[1].m_Type = DATA_TYPE_VECTOR3;
    values[1] = Vector3(1, 2, 3);
    fields[256].m_Field = 2;
    fields[257].m_Field = 0;
    DataTableDesc desc = { .m_Type = 1, .m_Fields = fields, .m_FieldCount = count, .m_RowStride = count * 16 };
    HDataStore    source = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &desc));
    DataId original;
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, 1, 0, values, count, &original));
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, bytes, size, &size));
    HDataStore        loaded = DataCreateStore();
    HDataBlobInstance instance;
    ASSERT_EQ(DATA_RESULT_OK, Load(loaded, bytes, size, &instance));
    HDataStore stores[] = { source, loaded };
    DataId     ids[] = { original, FirstId(loaded) };
    for (uint32_t mode = 0; mode < 2; ++mode)
    {
        HDataStore  store = stores[mode];
        DataId      id = ids[mode];
        double      number;
        DataVector3 vector;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, id, 1, &number));
        ASSERT_EQ(0.0, number);
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetVector3(store, id, 17, &vector));
        ASSERT_EQ(3.0f, vector.m_Values[2]);
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, id, fields[255].m_Field, &number));
        ASSERT_EQ(255.0, number);
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, id, 2, &number));
        ASSERT_EQ(256.0, number);
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, id, 0, &number));
        ASSERT_EQ(257.0, number);
        number = 123;
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataFieldGetNumber(store, id, 17, &number));
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGetNumber(store, id, 1 + 16 * count, &number));
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGetNumber(store, id, 3, &number));
        ASSERT_EQ(123.0, number);
        ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, id));
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGetNumber(store, id, 2, &number));
        ASSERT_EQ(123.0, number);
        ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    }
    delete[] bytes;
}

TEST(Data, TypedAccessErrorsPackedValuesAndReset)
{
    HDataStore source = DataCreateStore();
    // Field order differs from query binding order.
    DataFieldDesc meta[] = { { 10, DATA_TYPE_VECTOR3, 0 }, { 20, DATA_TYPE_STRING, 16 }, { 30, DATA_TYPE_BOOLEAN, 24 } };
    DataTableDesc table = { 1, 0, 0, meta, 3, 32 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &table));
    DataValue values[] = { Vector3(1, 2, 3), String("packed"), Boolean(1) };
    DataId    original;
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, 1, 0, values, 3, &original));
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, bytes, size, &size));
    uint8_t* saved = new uint8_t[size];
    memcpy(saved, bytes, size);

    HDataStore        loaded = DataCreateStore();
    HDataBlobInstance instance;
    ASSERT_EQ(DATA_RESULT_OK, Load(loaded, bytes, size, &instance));
    HDataStore stores[] = { source, loaded };
    for (uint32_t mode = 0; mode < 2; ++mode)
    {
        HDataStore     store = stores[mode];
        DataQueryField fields[] = { { 30, DATA_TYPE_BOOLEAN }, { 10, DATA_TYPE_VECTOR3 }, { 20, DATA_TYPE_STRING } };
        DataQueryDesc  desc = { 0, 0, 0, 0, fields, 3 };
        HDataQuery     query;
        ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
        DataStoreLock(store);
        DataIterator it = DataQueryIter(query);
        DataVector3  vector = { { 7, 8, 9 } };
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, GetTestFieldVector3(&it, 0, 1, &vector));
        ASSERT_EQ(7.0f, vector.m_Values[0]);
        ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
        DataId id = DataIterGetId(&it, 0);
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetVector3(store, id, 10, &vector));
        ASSERT_EQ(1.0f, vector.m_Values[0]);
        ASSERT_EQ(3.0f, vector.m_Values[2]);
        const char* string;
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldString(&it, 0, 2, &string));
        ASSERT_STREQ("packed", string);
        if (mode)
            ASSERT_TRUE(string >= (const char*)bytes && string < (const char*)bytes + size);

        double number = 123;
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataFieldGetNumber(store, id, 10, &number));
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGetNumber(store, 0, 10, &number));
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGetNumber(store, id, 99, &number));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, GetTestFieldNumber(&it, 1, 1, &number));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, GetTestFieldNumber(&it, 0, 3, &number));
        ASSERT_EQ(123.0, number);
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetFieldNumber(store, id, 10, 4));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, SetTestFieldNumber(&it, 0, 1, 4));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, SetTestFieldVector3(&it, 1, 1, &vector));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetFieldBoolean(store, id, 30, 2));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, SetTestFieldBoolean(&it, 0, 0, 2));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetFieldString(store, id, 20, 0));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, SetTestFieldString(&it, 0, 2, 0));
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataSetFieldVector3(store, 0, 10, &vector));
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataSetFieldVector3(store, id, 99, &vector));
        DataMemoryStats memory;
        GetDataMemoryStats(store, &memory);
        ASSERT_EQ((uint64_t)0, memory.m_PayloadBlocks);

        vector.m_Values[1] = 8;
        ASSERT_EQ(DATA_RESULT_OK, DataSetFieldVector3(store, id, 10, &vector));
        vector.m_Values[1] = 99; // Set copied the caller's math value.
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldVector3(&it, 0, 1, &vector));
        ASSERT_EQ(8.0f, vector.m_Values[1]);
        ASSERT_EQ(DATA_RESULT_OK, SetTestFieldBoolean(&it, 0, 0, 0));
        uint8_t boolean = 1;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetBoolean(store, id, 30, &boolean));
        ASSERT_EQ(0, boolean);
        char replacement[] = "changed";
        ASSERT_EQ(DATA_RESULT_OK, DataSetFieldString(store, id, 20, replacement));
        replacement[0] = 'X';
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetString(store, id, 20, &string));
        ASSERT_STREQ("changed", string);
        ASSERT_EQ(DATA_RESULT_OK, SetTestFieldString(&it, 0, 2, string));
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldString(&it, 0, 2, &string));
        ASSERT_STREQ("changed", string);
        ASSERT_EQ(DATA_RESULT_OK, DataResetField(store, id, 10));
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldVector3(&it, 0, 1, &vector));
        ASSERT_EQ(2.0f, vector.m_Values[1]);
        if (mode)
            DataResetBlob(instance);
        else
            ASSERT_EQ(DATA_RESULT_OK, DataResetTable(store, 1));
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldBoolean(&it, 0, 0, &boolean));
        ASSERT_EQ(1, boolean);
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetString(store, id, 20, &string));
        ASSERT_STREQ("packed", string);
        GetDataMemoryStats(store, &memory);
        ASSERT_EQ((uint64_t)0, memory.m_PayloadBlocks);
        DataStoreUnlock(store);
        ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, id));
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGetVector3(store, id, 10, &vector));
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataSetFieldVector3(store, id, 10, &vector));
        ASSERT_EQ(2.0f, vector.m_Values[1]);
        DataDestroyQuery(query);
    }
    ASSERT_EQ(0, memcmp(saved, bytes, size));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(source));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(loaded));
    delete[] saved;
    delete[] bytes;
}

TEST(Data, BoundQueryRegistrationChurn)
{
    HDataStore source = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, Register(source, 1));
    DataId id;
    ASSERT_EQ(DATA_RESULT_OK, Add(source, 1, 0, 5, "packed", &id));
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, bytes, size, &size));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(source));
    HDataBlob blob;
    ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
    HDataStore     store = DataCreateStore();
    DataQueryField fields[] = { { 10, DATA_TYPE_NUMBER }, { 20, DATA_TYPE_STRING } };
    DataQueryDesc  desc = { 0, 0, 0, 0, fields, 2 };
    HDataQuery     query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    // Cross registration pages and grow dense storage, then reuse slots with live queries.
    const uint32_t    count = 600;
    HDataBlobInstance instances[count];
    for (uint32_t cycle = 0; cycle < 3; ++cycle)
    {
        for (uint32_t i = 0; i < count; ++i)
            ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(store, blob, i + 1, &instances[i]));
        DataStoreLock(store);
        DataIterator it = DataQueryIter(query);
        uint32_t     visited = 0;
        while (DataIterNext(&it) == DATA_RESULT_OK)
        {
            ASSERT_EQ(count, DataIterGetCount(&it));
            for (uint32_t r = 0; r < DataIterGetCount(&it); ++r)
            {
                ASSERT_EQ(DATA_RESULT_OK, SetTestFieldNumber(&it, r, 0, (double)DataIterGetGroupId(&it, r)));
                ++visited;
            }
        }
        ASSERT_EQ(count, visited);
        DataStoreUnlock(store);
        DataMemoryStats before, after;
        GetDataMemoryStats(store, &before);
        ASSERT_EQ((uint64_t)1, before.m_Tables);
        ASSERT_EQ((uint64_t)count, before.m_Instances);
        for (uint32_t i = 0; i < count; i += 2)
            ASSERT_EQ(DATA_RESULT_OK, DataRemoveBlob(instances[i]));
        for (uint32_t i = 0; i < count; i += 2)
            ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(store, blob, 1000 + i, &instances[i]));
        GetDataMemoryStats(store, &after);
        ASSERT_EQ(before.m_TotalBytes, after.m_TotalBytes);
        ASSERT_EQ(before.m_QueryBytes, after.m_QueryBytes);
        DataStoreLock(store);
        it = DataQueryIter(query);
        visited = 0;
        while (DataIterNext(&it) == DATA_RESULT_OK)
        {
            for (uint32_t r = 0; r < DataIterGetCount(&it); ++r)
            {
                double      number;
                const char* string;
                DataGroupId group = DataIterGetGroupId(&it, r);
                ASSERT_EQ(DATA_RESULT_OK, GetTestFieldNumber(&it, r, 0, &number));
                ASSERT_EQ(group >= 1000 ? 5.0 : (double)group, number);
                ASSERT_EQ(DATA_RESULT_OK, GetTestFieldString(&it, r, 1, &string));
                ASSERT_STREQ("packed", string);
                ++visited;
            }
        }
        ASSERT_EQ(count, visited);
        DataStoreUnlock(store);
        for (uint32_t i = 0; i < count; ++i)
            ASSERT_EQ(DATA_RESULT_OK, DataRemoveBlob(instances[i]));
        DataStoreLock(store);
        it = DataQueryIter(query);
        ASSERT_EQ(DATA_RESULT_END, DataIterNext(&it));
        DataStoreUnlock(store);
    }
    DataDestroyQuery(query);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    DataDestroyBlob(blob);
    delete[] bytes;
}

extern "C" int TestInlineDataFromC(HDataStore store);

TEST(Data, InlineCompositionQueriesAndRoundtrip)
{
    // Nested fixed layouts preserve C alignment and include variable data.
    DataFieldDesc  light_fields[] = { { .m_Field = 10, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 0, .m_Name = "10" }, { .m_Field = 20, .m_Type = DATA_TYPE_STRING, .m_Offset = 16, .m_Name = "20" }, { .m_Field = 30, .m_Type = DATA_TYPE_LIST, .m_Offset = 24, .m_Name = "30" } };
    DataStructDesc light = { light_fields, 3, 32 };
    DataFieldDesc  wrapper_fields[] = { { .m_Field = 40, .m_Type = DATA_TYPE_STRUCT, .m_Offset = 8, .m_Struct = &light, .m_Name = "40" } };
    DataStructDesc wrapper = { wrapper_fields, 1, 40 };
    DataFieldDesc  fields[] = { { .m_Field = 1, .m_Type = DATA_TYPE_NUMBER, .m_Offset = 0, .m_Name = "1" }, { .m_Field = 2, .m_Type = DATA_TYPE_STRUCT, .m_Offset = 8, .m_Struct = &wrapper, .m_Name = "2" }, { .m_Field = 3, .m_Type = DATA_TYPE_BOOLEAN, .m_Offset = 48, .m_Name = "3" } };
    DataTableDesc  table = { 1, 0, 0, fields, 3, 56 };
    HDataStore     source = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &table));
    // Changing a declaration after registration cannot alter the compiled layout.
    light_fields[0].m_Offset = 999;
    uint64_t            names[] = { 30, 10, 20 };
    DataValue           list_items[] = { Number(7), String("shared") };
    const DataValueType child_types[] = { DATA_TYPE_LIST, DATA_TYPE_VECTOR3, DATA_TYPE_STRING };
    DataValueData       children[] = { List(list_items, 2).m_Value, Vector3(1, 2, 3).m_Value, String("shared").m_Value };
    DataValue           light_value = Struct(names, child_types, children, 3);
    uint64_t            light_name = 40;
    DataValue           values[] = { Number(5), Struct(&light_name, &light_value.m_Type, &light_value.m_Value, 1), Boolean(1) };
    DataId              original;
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, 1, 1, values, 3, &original));
    ASSERT_EQ(7u, source->m_Tables[0]->m_MetadataCount);
    ASSERT_EQ(32u, GetFieldMeta(source->m_Tables[0], 3).m_Size);

    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, 0, 0, &size));
    uint8_t* allocation = new uint8_t[size];
    uint8_t* bytes = allocation;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, bytes, size, &size));
    uint8_t* saved = new uint8_t[size];
    memcpy(saved, bytes, size);
    for (uint32_t packed = 0; packed < 2; ++packed)
    {
        HDataStore        store = packed ? DataCreateStore() : source;
        HDataBlobInstance instance = 0;
        if (packed)
            ASSERT_EQ(DATA_RESULT_OK, Load(store, bytes, size, &instance));
        DataQueryField fields[] = {
            { .m_Field = dmHashString64("2.40.10"), .m_Type = DATA_TYPE_VECTOR3 },
            { .m_Field = dmHashString64("2.40.20"), .m_Type = DATA_TYPE_STRING },
            { .m_Field = dmHashString64("2.40.30"), .m_Type = DATA_TYPE_LIST },
            { 1, DATA_TYPE_NUMBER },
            { 2, DATA_TYPE_STRUCT }
        };
        DataQueryDesc desc = { 0, 0, 0, 0, fields, 5 };
        HDataQuery    query;
        ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
        DataStoreLock(store);
        DataIterator it = DataQueryIter(query);
        ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
        DataId      id = DataIterGetId(&it, 0);
        DataVector3 color;
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(3.0f, color.m_Values[2]);
        const char* string;
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldString(&it, 0, 1, &string));
        ASSERT_STREQ("shared", string);
        if (packed)
            ASSERT_TRUE((const uint8_t*)string >= bytes && (const uint8_t*)string < bytes + size);
        color.m_Values[0] = 42;
        ASSERT_EQ(DATA_RESULT_OK, SetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldString(&it, 0, 1, &string));
        ASSERT_STREQ("shared", string); // Sibling references survive the first leaf override.
        DataValue list, child;
        ASSERT_EQ(DATA_RESULT_OK, DataIterGetField(&it, 0, 2, &list));
        ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(&list.m_Value.m_List, 0, &child));
        ASSERT_EQ(7.0, child.m_Value.m_Number);
        ASSERT_EQ(DATA_RESULT_OK, SetTestFieldString(&it, 0, 1, "changed"));
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(42.0f, color.m_Values[0]);
        // Persist a leaf override with untouched sibling references.
        uint8_t  DM_ALIGNED(8) changed_blob[1024];
        uint32_t changed_size;
        ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, changed_blob, sizeof(changed_blob), &changed_size));
        HDataStore        changed_store = DataCreateStore();
        HDataBlobInstance changed_instance;
        ASSERT_EQ(DATA_RESULT_OK, Load(changed_store, changed_blob, changed_size, &changed_instance));
        DataQueryField changed_fields[] = { { .m_Field = dmHashString64("2.40.10"), .m_Type = DATA_TYPE_VECTOR3 }, { .m_Field = dmHashString64("2.40.20"), .m_Type = DATA_TYPE_STRING } };
        DataQueryDesc  changed_desc = { 0, 0, 0, 0, changed_fields, 2 };
        HDataQuery     changed_query;
        ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(changed_store, &changed_desc, &changed_query));
        DataStoreLock(changed_store);
        DataIterator changed_it = DataQueryIter(changed_query);
        ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&changed_it));
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldVector3(&changed_it, 0, 0, &color));
        ASSERT_EQ(42.0f, color.m_Values[0]);
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldString(&changed_it, 0, 1, &string));
        ASSERT_STREQ("changed", string);
        DataStoreUnlock(changed_store);
        DataDestroyQuery(changed_query);
        ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(changed_store));
        ASSERT_EQ(DATA_RESULT_OK, DataResetField(store, id, 2));
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(1.0f, color.m_Values[0]);
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldString(&it, 0, 1, &string));
        ASSERT_STREQ("shared", string);
        // A write to another root must not change the reference mode of this struct.
        ASSERT_EQ(DATA_RESULT_OK, SetTestFieldNumber(&it, 0, 3, 9));
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldString(&it, 0, 1, &string));
        ASSERT_STREQ("shared", string);
        color.m_Values[0] = 42;
        ASSERT_EQ(DATA_RESULT_OK, SetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(DATA_RESULT_OK, SetTestFieldString(&it, 0, 1, "changed"));
        ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, id));
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(1.0f, color.m_Values[0]);
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldString(&it, 0, 1, &string));
        ASSERT_STREQ("shared", string);
        double number;
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldNumber(&it, 0, 3, &number));
        ASSERT_EQ(5.0, number);
        ASSERT_EQ(DATA_RESULT_OK, SetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldString(&it, 0, 1, &string));
        ASSERT_STREQ("shared", string); // Reusing the reset slot restores packed references correctly.
        if (packed)
            DataResetBlob(instance);
        else
            ASSERT_EQ(DATA_RESULT_OK, DataResetTable(store, 1));
        ASSERT_EQ(0, TestInlineDataFromC(store));
        DataValue view, light_view;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, id, 2, &view));
        ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&view.m_Value.m_Struct, 40, &light_view));
        ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&light_view.m_Value.m_Struct, 10, &child));
        ASSERT_EQ(3.0f, child.m_Value.m_Vector3[2]);
        ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, id, 2, &view)); // Borrowed self-assignment.
        uint8_t* roundtrip = new uint8_t[size];
        uint32_t written;
        ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, roundtrip, size, &written));
        ASSERT_EQ(size, written);
        ASSERT_EQ(0, memcmp(saved, roundtrip, size));
        HDataBlob checked;
        ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(roundtrip, size, &checked));
        DataDestroyBlob(checked);
        delete[] roundtrip;
        DataStoreUnlock(store);
        ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, id));
        DataDestroyQuery(query);
        ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    }
    ASSERT_EQ(0, memcmp(saved, bytes, size));
    delete[] saved;
    delete[] allocation;
}

TEST(Data, InlineLayoutAndNameValidation)
{
    HDataStore     store = DataCreateStore();
    DataFieldDesc  members[] = { { .m_Field = 10, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 0, .m_Name = "10" }, { .m_Field = 20, .m_Type = DATA_TYPE_NUMBER, .m_Offset = 16, .m_Name = "20" } };
    DataStructDesc light = { members, 2, 24 };
    DataFieldDesc  fields[] = { { .m_Field = 1, .m_Type = DATA_TYPE_STRUCT, .m_Offset = 0, .m_Struct = &light, .m_Name = "1" } };
    DataTableDesc  table = { 1, 0, 0, fields, 1, 24 };
    fields[0].m_Name = 0;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &table));
    fields[0].m_Name = "1";
    members[0].m_Name = "bad.name";
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &table));
    members[0].m_Name = "";
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &table));
    members[0].m_Name = "10";
    light.m_Size = 8;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &table));
    light.m_Size = 24;
    members[1].m_Offset = 4;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &table));
    members[1].m_Offset = 16;
    members[1].m_Field = 10;
    ASSERT_EQ(DATA_RESULT_ALREADY_EXISTS, DataRegisterTable(store, &table));
    members[1].m_Field = 20;
    fields[0].m_Type = DATA_TYPE_VECTOR3;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &table));
    fields[0].m_Type = DATA_TYPE_STRUCT;
    DataStructDesc cycle = { fields, 1, 24 };
    fields[0].m_Struct = &cycle;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &table));
    fields[0].m_Struct = &light;
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    uint64_t      names[] = { 10, 20 };
    DataValueType child_types[] = { DATA_TYPE_VECTOR3, DATA_TYPE_NUMBER };
    DataValueData children[] = { { .m_Vector3 = { 1, 2, 3 } }, { .m_Number = 4 } };
    DataValue     value = Struct(names, child_types, children, 2);
    DataId        id = 0;
    child_types[1] = DATA_TYPE_BOOLEAN;
    children[1].m_Boolean = 1;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, AddTestRow(store, 1, 0, &value, 1, &id));
    child_types[1] = DATA_TYPE_NUMBER;
    children[1].m_Number = 4;
    names[1] = 99;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, AddTestRow(store, 1, 0, &value, 1, &id));
    names[1] = 20;
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(store, 1, 0, &value, 1, &id));
    child_types[1] = DATA_TYPE_BOOLEAN;
    children[1].m_Boolean = 1;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetField(store, id, 1, &value));
    DataQueryField field = { .m_Field = dmHashString64("1.10"), .m_Type = DATA_TYPE_VECTOR3 };
    DataQueryDesc  desc = { 0, 0, 0, 0, &field, 1 };
    HDataQuery     query;
    field.m_Type = (DataValueType)99;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataCreateQuery(store, &desc, &query));
    field.m_Type = DATA_TYPE_VECTOR3;
    field.m_Field = dmHashString64("1.99");
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    DataStoreLock(store);
    DataIterator it = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_END, DataIterNext(&it));
    DataDestroyQuery(query);
    field.m_Field = dmHashString64("1.10");
    field.m_Type = DATA_TYPE_NUMBER;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    it = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_END, DataIterNext(&it));
    DataDestroyQuery(query);

    uint8_t  DM_ALIGNED(8) bytes[1024], saved[1024];
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, bytes, sizeof(bytes), &size));
    memcpy(saved, bytes, size);
    HDataBlob      blob;
    const uint32_t metadata = 24 + DATA_TABLE_HEADER_SIZE;
    const uint32_t corrupt[] = { metadata + offsetof(DataFileFieldMeta, m_ChildIndex), metadata + offsetof(DataFileFieldMeta, m_ChildCount), metadata + offsetof(DataFileFieldMeta, m_ByteSize), metadata + DATA_FIELD_META_SIZE + offsetof(DataFileFieldMeta, m_ChildIndex), metadata + 2 * DATA_FIELD_META_SIZE + offsetof(DataFileFieldMeta, m_ByteOffset) };
    const uint32_t widths[] = { 2, 2, 4, 2, 4 };
    const uint32_t values[] = { 0, UINT32_MAX, 25, 1, 4 };
    for (uint32_t i = 0; i < 5; ++i)
    {
        WriteDataInteger(bytes + corrupt[i], values[i], widths[i]);
        ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(bytes, size, &blob));
        memcpy(bytes, saved, size);
    }
    ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
    DataDestroyBlob(blob);
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, InlineNamesBindDifferentTablesAndTrackRegistration)
{
    HDataStore     store = DataCreateStore();
    DataQueryField field = { .m_Field = dmHashString64("1.10"), .m_Type = DATA_TYPE_VECTOR3 };
    DataQueryDesc  query_desc = { 0, 0, 0, 0, &field, 1 };
    HDataQuery     query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &query_desc, &query));
    DataFieldDesc  members[] = { { .m_Field = 10, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 0, .m_Name = "10" }, { .m_Field = 20, .m_Type = DATA_TYPE_NUMBER, .m_Offset = 16, .m_Name = "20" } };
    DataStructDesc light = { members, 2, 24 };
    uint64_t       names[] = { 10, 20 };
    DataValueType  child_types[] = { DATA_TYPE_VECTOR3, DATA_TYPE_NUMBER };
    DataValueData  children[] = { { .m_Vector3 = { 1, 2, 3 } }, { .m_Number = 4 } };
    DataValue      value = Struct(names, child_types, children, 2);
    DataId         ids[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        DataFieldDesc root = { .m_Field = 1, .m_Type = DATA_TYPE_STRUCT, .m_Offset = i * 8, .m_Struct = i < 2 ? &light : 0, .m_Name = "1" };
        DataTableDesc table = { i + 1, 0, 0, &root, 1, i * 8 + (i < 2 ? 24u : 8u) };
        ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
        ASSERT_EQ(DATA_RESULT_OK, AddTestRow(store, i + 1, i, &value, 1, &ids[i]));
    }
    DataStoreLock(store);
    DataIterator it = DataQueryIter(query);
    uint32_t     visited = 0;
    while (DataIterNext(&it) == DATA_RESULT_OK)
    {
        ASSERT_TRUE(DataIterGetType(&it) < 3); // Dynamic structs cannot promise a member layout.
        DataVector3 color;
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(1.0f, color.m_Values[0]);
        color.m_Values[0] = 8;
        ASSERT_EQ(DATA_RESULT_OK, SetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(8.0f, color.m_Values[0]);
        DataValue object, intensity;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, DataIterGetId(&it, 0), 1, &object));
        ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&object.m_Value.m_Struct, 20, &intensity));
        ASSERT_EQ(4.0, intensity.m_Value.m_Number);
        ++visited;
    }
    ASSERT_EQ(2u, visited);
    ASSERT_EQ(DATA_RESULT_OK, DataResetTable(store, 1));
    it = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
    DataVector3 color;
    ASSERT_EQ(DATA_RESULT_OK, GetTestFieldVector3(&it, 0, 0, &color));
    ASSERT_EQ(1.0f, color.m_Values[0]);
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataUnregisterTable(store, 1));
    DataStoreLock(store);
    it = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
    ASSERT_EQ((uint64_t)2, DataIterGetType(&it));
    ASSERT_EQ(DATA_RESULT_END, DataIterNext(&it));
    DataDestroyQuery(query);
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, ResetOneComponentPreservesOtherRowsAndReusesStorage)
{
    HDataStore    store = DataCreateStore();
    DataFieldDesc fields[10] = {};
    DataValue     values[10];
    for (uint32_t p = 0; p < 10; ++p)
    {
        fields[p].m_Field = p;
        fields[p].m_Type = DATA_TYPE_NUMBER;
        fields[p].m_Offset = p * 8;
        values[p] = Number(p);
    }
    DataTableDesc table = { 1, 0, 0, fields, 10, 80 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    DataId ids[3];
    for (uint32_t r = 0; r < 3; ++r)
        ASSERT_EQ(DATA_RESULT_OK, AddTestRow(store, 1, r == 2 ? 8 : 7, values, 10, &ids[r], r + 11));
    DataMemoryStats before, after;
    GetDataMemoryStats(store, &before);
    ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, ids[0]));
    GetDataMemoryStats(store, &after);
    ASSERT_EQ(before.m_TotalBytes, after.m_TotalBytes);

    for (uint32_t r = 0; r < 3; ++r)
        for (uint32_t p = 0; p < 10; ++p)
            ASSERT_EQ(DATA_RESULT_OK, DataSetFieldNumber(store, ids[r], p, 100 * (r + 1) + p));
    DataGroupId    group = 7;
    DataQueryField field = { 9, DATA_TYPE_NUMBER };
    DataQueryDesc  desc = { &group, 1, 0, 0, &field, 1 };
    HDataQuery     query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    DataStoreLock(store);
    DataIterator it = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
    ASSERT_EQ(2u, DataIterGetCount(&it));
    ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, ids[0]));
    double number;
    for (uint32_t r = 0; r < 3; ++r)
    {
        ASSERT_EQ((uint64_t)(r + 11), DataGetComponentId(store, ids[r]));
        for (uint32_t p = 0; p < 10; ++p)
        {
            ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, ids[r], p, &number));
            ASSERT_EQ((double)(r ? 100 * (r + 1) + p : p), number);
        }
    }
    ASSERT_EQ(DATA_RESULT_OK, GetTestFieldNumber(&it, 0, 0, &number));
    ASSERT_EQ(9.0, number);
    ASSERT_EQ(DATA_RESULT_OK, GetTestFieldNumber(&it, 1, 0, &number));
    ASSERT_EQ(209.0, number);

    GetDataMemoryStats(store, &before);
    for (uint32_t cycle = 0; cycle < 16; ++cycle)
    {
        ASSERT_EQ(DATA_RESULT_OK, DataSetFieldNumber(store, ids[0], 9, 500));
        ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, ids[0]));
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, ids[0], 9, &number));
        ASSERT_EQ(9.0, number);
    }
    GetDataMemoryStats(store, &after);
    ASSERT_EQ(before.m_TotalBytes, after.m_TotalBytes);
    ASSERT_EQ(DATA_RESULT_END, DataIterNext(&it));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataResetRow(store, 0));
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[0]));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataResetRow(store, ids[0]));
    ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, ids[2])); // Swap removal moved this row.
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, ids[2], 9, &number));
    ASSERT_EQ(9.0, number);
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, ids[1], 9, &number));
    ASSERT_EQ(209.0, number);
    DataDestroyQuery(query);

    DataTableDesc empty = { 2, 0, 0, 0, 0, 0 };
    DataId        empty_id;
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &empty));
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(store, 2, 7, 0, 0, &empty_id));
    ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, empty_id));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataResetRow(store, ids[0]));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, RowIterationWithoutFieldFilters)
{
    HDataStore    source = DataCreateStore();
    DataFieldDesc meta[] = { { 10, DATA_TYPE_NUMBER, 0 }, { 20, DATA_TYPE_STRING, 8 }, { 30, DATA_TYPE_VECTOR3, 16 }, { 40, DATA_TYPE_NULL, 0 } };
    DataTableDesc table = { 1, 0, 0, meta, 4, 32 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &table));
    DataValue values[] = { Number(10), String("original"), Vector3(1, 2, 3), Null() };
    DataId    id;
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, 1, 7, values, 4, &id));
    values[0] = Number(20);
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, 1, 8, values, 4, &id));
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, bytes, size, &size));
    HDataStore        loaded = DataCreateStore();
    HDataBlobInstance instance;
    ASSERT_EQ(DATA_RESULT_OK, Load(loaded, bytes, size, &instance, 9));
    HDataStore stores[] = { source, loaded };
    for (uint32_t mode = 0; mode < 2; ++mode)
    {
        HDataStore    store = stores[mode];
        DataQueryDesc desc = {};
        HDataQuery    query;
        ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
        DataStoreLock(store);
        DataIterator    batch = DataQueryIter(query);
        DataRowIterator empty_rows = DataIterRows(&batch);
        ASSERT_EQ(DATA_RESULT_END, DataRowIterNext(&empty_rows));
        ASSERT_EQ((DataId)0, DataRowIterGetId(&empty_rows));
        ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&batch));
        DataRowIterator rows = DataIterRows(&batch);
        uint32_t        visited = 0;
        while (DataRowIterNext(&rows) == DATA_RESULT_OK)
        {
            id = DataRowIterGetId(&rows);
            ASSERT_NE((DataId)0, id);
            ASSERT_EQ((DataGroupId)(mode ? 9 : 7 + visited), DataRowIterGetGroupId(&rows));
            double number;
            ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, id, 10, &number));
            ASSERT_EQ(10.0 + 10.0 * visited, number);
            ++visited;
        }
        ASSERT_EQ(2u, visited);
        ASSERT_EQ(DATA_RESULT_END, DataRowIterNext(&rows));
        ASSERT_EQ((DataId)0, DataRowIterGetId(&rows));
        ASSERT_EQ(DATA_RESULT_END, DataIterNext(&batch));
        DataStoreUnlock(store);
        DataDestroyQuery(query);
    }
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(loaded));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(source));
    delete[] bytes;
}

TEST(Data, RowAccessUsesBatchRelativeRowsAfterRemoval)
{
    HDataStore store = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 1));
    DataId ids[6];
    for (uint32_t row = 0; row < 6; ++row)
        ASSERT_EQ(DATA_RESULT_OK, Add(store, 1, row % 2, row + 10, "base", &ids[row]));
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[1])); // Last row moves; its base bytes do not.
    DataGroupId    group = 1;
    DataQueryField field = { 10, DATA_TYPE_NUMBER };
    for (uint32_t requested = 0; requested < 2; ++requested)
    {
        DataQueryDesc desc = { &group, 1, 0, 0, requested ? &field : 0, requested };
        HDataQuery    query;
        ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
        uint32_t handle = DataQueryFindField(query, &field);
        ASSERT_EQ(requested != 0, handle != UINT32_MAX);
        DataStoreLock(store);
        DataIterator batch = DataQueryIter(query);
        uint32_t     visited = 0;
        while (DataIterNext(&batch) == DATA_RESULT_OK)
        {
            DataRowIterator rows = DataIterRows(&batch);
            while (DataRowIterNext(&rows) == DATA_RESULT_OK)
            {
                DataId id = DataRowIterGetId(&rows);
                ASSERT_TRUE(id == ids[3] || id == ids[5]);
                ASSERT_EQ(group, DataRowIterGetGroupId(&rows));
                double number;
                ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, id, 10, &number));
                ASSERT_EQ(id == ids[3] ? 13.0 : 15.0, number);
                if (requested)
                    ASSERT_EQ(number, *DataRowIterGetNumber(&rows, handle));
                if (requested)
                    *DataRowIterGetNumberMut(&rows, handle) = 99;
                else
                    ASSERT_EQ(DATA_RESULT_OK, DataSetFieldNumber(store, id, 10, 99));
                ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, id, 10, &number));
                ASSERT_EQ(99.0, number);
                ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, id));
                ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, id, 10, &number));
                ASSERT_EQ(id == ids[3] ? 13.0 : 15.0, number);
                if (requested)
                    ASSERT_EQ(number, *DataRowIterGetNumber(&rows, handle));
                ++visited;
            }
        }
        ASSERT_EQ(2u, visited);
        DataStoreUnlock(store);
        DataDestroyQuery(query);
    }
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, DeferredRemovalUsesStableIds)
{
    HDataStore store = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 1));
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 2));
    DataQueryField field = { 10, DATA_TYPE_NUMBER };
    DataQueryDesc  desc = { 0, 0, 0, 0, &field, 1 };
    HDataQuery     query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    uint32_t        handle = DataQueryFindField(query, &field);
    dmArray<DataId> removals;
    removals.SetCapacity(64);
    DataId ids[64];
    // Reuse both the caller's deletion buffer and released store ID slots.
    for (uint32_t cycle = 0; cycle < 2; ++cycle)
    {
        for (uint32_t r = 0; r < 64; ++r)
            ASSERT_EQ(DATA_RESULT_OK, Add(store, 1 + r / 32, r, r, "base", &ids[r]));
        removals.SetSize(0);
        DataStoreLock(store);
        DataIterator batch = DataQueryIter(query);
        uint32_t     visited = 0;
        while (DataIterNext(&batch) == DATA_RESULT_OK)
        {
            DataRowIterator rows = DataIterRows(&batch);
            while (DataRowIterNext(&rows) == DATA_RESULT_OK)
            {
                DataGroupId group = DataRowIterGetGroupId(&rows);
                ASSERT_EQ(ids[group], DataRowIterGetId(&rows));
                ASSERT_EQ((double)group, *DataRowIterGetNumber(&rows, handle));
                if (group % 2 == 0 || group == 63)
                    removals.Push(DataRowIterGetId(&rows));
                ++visited;
            }
        }
        ASSERT_EQ(64u, visited);
        ASSERT_EQ(33u, removals.Size());
        // Queued IDs are still readable before the caller applies its buffer.
        double value;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, removals[0], 10, &value));
        DataStoreUnlock(store);
        for (uint32_t r = 0; r < removals.Size(); ++r)
            ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, removals[r]));
        DataStoreLock(store);
        batch = DataQueryIter(query);
        visited = 0;
        while (DataIterNext(&batch) == DATA_RESULT_OK)
        {
            DataRowIterator rows = DataIterRows(&batch);
            while (DataRowIterNext(&rows) == DATA_RESULT_OK)
            {
                DataGroupId group = DataRowIterGetGroupId(&rows);
                ASSERT_TRUE(group % 2 == 1 && group != 63);
                ASSERT_EQ(ids[group], DataRowIterGetId(&rows));
                ++visited;
            }
        }
        ASSERT_EQ(31u, visited);
        DataStoreUnlock(store);
        for (uint32_t r = 0; r < 64; ++r)
        {
            if (r % 2 == 0 || r == 63)
                ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGetNumber(store, ids[r], 10, &value));
            else
                ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[r]));
        }
        ASSERT_EQ(64u, removals.Capacity());
    }
    DataDestroyQuery(query);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, EmptyRowHasNoFields)
{
    HDataStore    store = DataCreateStore();
    DataTableDesc table = { 1, 0, 0, 0, 0, 0 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    DataId id;
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(store, 1, 7, 0, 0, &id));
    DataQueryDesc desc = {};
    HDataQuery    query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    DataStoreLock(store);
    DataIterator batch = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&batch));
    DataRowIterator rows = DataIterRows(&batch);
    ASSERT_EQ(DATA_RESULT_OK, DataRowIterNext(&rows));
    ASSERT_EQ(id, DataRowIterGetId(&rows));
    ASSERT_EQ(DATA_RESULT_END, DataRowIterNext(&rows));
    ASSERT_EQ(DATA_RESULT_END, DataIterNext(&batch));
    DataDestroyQuery(query);
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, AlignedLayoutValidation)
{
    HDataStore     store = DataCreateStore();
    DataFieldDesc  members[] = { { .m_Field = 1, .m_Type = DATA_TYPE_NUMBER, .m_Offset = 0, .m_Name = "1" }, { .m_Field = 2, .m_Type = DATA_TYPE_BOOLEAN, .m_Offset = 8, .m_Name = "2" } };
    DataStructDesc inner = { members, 2, 16 };
    DataFieldDesc  root = { .m_Field = 3, .m_Type = DATA_TYPE_STRUCT, .m_Offset = 0, .m_Struct = &inner, .m_Name = "3" };
    DataTableDesc  table = { 1, 0, 0, &root, 1, 24 };
    members[0].m_Offset = 1;
    members[1].m_Offset = 9;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &table));
    members[0].m_Offset = 0;
    members[1].m_Offset = 8;
    inner.m_Size = 9;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &table));
    inner.m_Size = 16;
    root.m_Offset = 4;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &table));
    root.m_Offset = 8;
    table.m_RowStride = 25;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &table));
    table.m_RowStride = 24;
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    uint64_t            names[] = { 1, 2 };
    const DataValueType child_types[] = { DATA_TYPE_NUMBER, DATA_TYPE_BOOLEAN };
    DataValueData       children[] = { { .m_Number = 17 }, { .m_Boolean = 1 } };
    DataValue           value = Struct(names, child_types, children, 2);
    DataId              id;
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(store, 1, 0, &value, 1, &id));
    uint8_t  DM_ALIGNED(8) bytes[512], saved[512];
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, bytes, sizeof(bytes), &size));
    memcpy(saved, bytes, size);
    HDataBlob blob;
    ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
    // Reject unaligned input once, before any direct pointer can be obtained.
    uint8_t* unaligned = new uint8_t[size + 1];
    memcpy(unaligned + 1, bytes, size);
    HDataBlob output = blob;
    ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(unaligned + 1, size, &output));
    ASSERT_EQ((uintptr_t)blob, (uintptr_t)output);
    delete[] unaligned;
    uint32_t       table_offset = (uint32_t)ReadDataInteger(bytes + 16, 4);
    uint32_t       metadata = table_offset + DATA_TABLE_HEADER_SIZE;
    const size_t   offsets[] = { 4, table_offset + offsetof(DataTableHeader, m_RowStride), metadata + offsetof(DataFileFieldMeta, m_ByteOffset), metadata + offsetof(DataFileFieldMeta, m_ByteSize), metadata + DATA_FIELD_META_SIZE + offsetof(DataFileFieldMeta, m_ByteOffset) };
    const uint32_t values[] = { 7, 25, 4, 9, 1 };
    for (uint32_t i = 0; i < 5; ++i)
    {
        WriteDataInteger(bytes + offsets[i], values[i], 4);
        ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(bytes, size, &output));
        ASSERT_EQ((uintptr_t)blob, (uintptr_t)output);
        memcpy(bytes, saved, size);
    }
    DataDestroyBlob(blob);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

extern "C" int TestDataPointersFromC(HDataStore store, int write);

TEST(Data, NativeFieldPointersAndReset)
{
    struct NativeRow
    {
        double      m_Number;
        uint8_t     m_Boolean;
        DataVector3 m_Vector3;
        DataVector4 m_Vector4;
        DataMatrix4 m_Matrix4;
    };
    DataFieldDesc fields[] = {
        { 1, DATA_TYPE_NUMBER, offsetof(NativeRow, m_Number) },
        { 2, DATA_TYPE_BOOLEAN, offsetof(NativeRow, m_Boolean) },
        { 3, DATA_TYPE_VECTOR3, offsetof(NativeRow, m_Vector3) },
        { 4, DATA_TYPE_VECTOR4, offsetof(NativeRow, m_Vector4) },
        { 5, DATA_TYPE_MATRIX4, offsetof(NativeRow, m_Matrix4) }
    };
    DataTableDesc table = { 1, 0, 0, fields, 5, sizeof(NativeRow) };
    HDataStore    source = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &table));
    DataValue values[] = { Number(17), Boolean(0), Vector3(1, 2, 3), {}, Matrix() };
    values[3].m_Type = DATA_TYPE_VECTOR4;
    values[3].m_Value.m_Vector4[3] = 4;
    for (uint32_t i = 0; i < 16; ++i)
        values[4].m_Value.m_Matrix4[i] = (float)i;
    DataId ids[3];
    for (uint32_t i = 0; i < 3; ++i)
        ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, 1, i, values, 5, &ids[i]));
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    uint8_t* saved = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, bytes, size, &size));
    memcpy(saved, bytes, size);
    HDataStore        loaded = DataCreateStore();
    HDataBlobInstance instance;
    ASSERT_EQ(DATA_RESULT_OK, Load(loaded, bytes, size, &instance));
    HDataStore stores[] = { source, loaded };
    for (uint32_t packed = 0; packed < 2; ++packed)
    {
        HDataStore store = stores[packed];
        ASSERT_EQ(0, TestDataPointersFromC(store, 0));
        DataMemoryStats before, after;
        GetDataMemoryStats(store, &before);
        ASSERT_GT(before.m_ValueBytes, (uint64_t)0);
        ASSERT_EQ((uint64_t)0, before.m_PayloadBlocks);
        ASSERT_EQ(0, TestDataPointersFromC(store, 1));
        GetDataMemoryStats(store, &after);
        ASSERT_EQ(before.m_ValueBytes, after.m_ValueBytes);
        ASSERT_EQ(before.m_TotalBytes, after.m_TotalBytes);
        ASSERT_EQ((uint64_t)0, after.m_PayloadBlocks);
        DataId id = FirstId(store);
        ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, id));
        double number;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, id, 1, &number));
        ASSERT_EQ(17.0, number);
        if (packed)
            DataResetBlob(instance);
        else
            ASSERT_EQ(DATA_RESULT_OK, DataResetTable(store, 1));
        ASSERT_EQ(0, TestDataPointersFromC(store, 0));
        ASSERT_EQ(0, memcmp(saved, bytes, size));
    }
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(loaded));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(source));
    delete[] saved;
    delete[] bytes;
}

TEST(Data, QueryFieldHandlesSurviveTableChanges)
{
    HDataStore     store = DataCreateStore();
    DataQueryField fields[] = { { 30, DATA_TYPE_VECTOR3 }, { 10, DATA_TYPE_NUMBER } };
    DataQueryDesc  desc = { 0, 0, 0, 0, fields, 2 };
    HDataQuery     query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    uint32_t vector_field = DataQueryFindField(query, &fields[0]);
    uint32_t number_field = DataQueryFindField(query, &fields[1]);
    ASSERT_NE(UINT32_MAX, vector_field);
    ASSERT_NE(UINT32_MAX, number_field);
    ASSERT_NE(vector_field, number_field);
    DataQueryField invalid = { .m_Field = 99, .m_Type = DATA_TYPE_VECTOR3 };
    ASSERT_EQ(UINT32_MAX, DataQueryFindField(query, &invalid));
    invalid.m_Field = 30;
    invalid.m_Type = DATA_TYPE_NUMBER;
    ASSERT_EQ(UINT32_MAX, DataQueryFindField(query, &invalid));

    DataStoreLock(store);
    DataIterator empty = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_END, DataIterNext(&empty));
    DataStoreUnlock(store);

    // Reuse the handles after END and across registration/removal with new offsets.
    for (uint32_t pass = 0; pass < 3; ++pass)
    {
        DataFieldDesc meta[] = { { 10, DATA_TYPE_NUMBER, pass * 8 }, { 30, DATA_TYPE_VECTOR3, pass * 8 + 8 } };
        DataTableDesc table = { 1, 0, 0, meta, 2, pass * 8 + 24 };
        ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
        DataValue values[] = { Number(10 + pass), Vector3(1, 2, 3) };
        DataId    id;
        ASSERT_EQ(DATA_RESULT_OK, AddTestRow(store, 1, 0, values, 2, &id));
        ASSERT_EQ(vector_field, DataQueryFindField(query, &fields[0]));
        ASSERT_EQ(number_field, DataQueryFindField(query, &fields[1]));
        for (uint32_t traversal = 0; traversal < 2; ++traversal)
        {
            DataStoreLock(store);
            DataIterator it = DataQueryIter(query);
            ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
            DataRowIterator rows = DataIterRows(&it);
            ASSERT_EQ(DATA_RESULT_OK, DataRowIterNext(&rows));
            ASSERT_EQ(id, DataRowIterGetId(&rows));
            ASSERT_EQ(10.0 + pass, *DataRowIterGetNumber(&rows, number_field));
            ASSERT_EQ(3.0f, DataRowIterGetVector3(&rows, vector_field)->m_Values[2]);
            *DataRowIterGetNumberMut(&rows, number_field) = 99;
            ASSERT_EQ(99.0, *DataRowIterGetNumber(&rows, number_field));
            ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, id));
            ASSERT_EQ(DATA_RESULT_END, DataRowIterNext(&rows));
            ASSERT_EQ(DATA_RESULT_END, DataIterNext(&it));
            DataStoreUnlock(store);
        }
        ASSERT_EQ(DATA_RESULT_OK, DataUnregisterTable(store, 1));
    }
    DataDestroyQuery(query);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, NestedFieldPointersSharePayloadsAndPreserveSiblings)
{
    DataFieldDesc       members[] = { { .m_Field = 10, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 0, .m_Name = "10" }, { .m_Field = 20, .m_Type = DATA_TYPE_NUMBER, .m_Offset = 16, .m_Name = "20" }, { .m_Field = 30, .m_Type = DATA_TYPE_STRING, .m_Offset = 24, .m_Name = "30" } };
    DataStructDesc      light = { members, 3, 32 };
    uint64_t            names[] = { 10, 20, 30 };
    const DataValueType types[] = { DATA_TYPE_VECTOR3, DATA_TYPE_NUMBER, DATA_TYPE_STRING };
    DataValueData       first[] = { { .m_Vector3 = { 1, 2, 3 } }, { .m_Number = 7 }, { .m_String = "shared" } };
    DataValueData       second[] = { { .m_Vector3 = { 9, 8, 7 } }, { .m_Number = 6 }, { .m_String = "other" } };
    DataValue           values[] = { Struct(names, types, first, 3), Struct(names, types, second, 3) };
    HDataStore          source = DataCreateStore();
    for (uint32_t i = 0; i < 2; ++i)
    {
        DataFieldDesc roots[] = { { .m_Field = 1, .m_Type = DATA_TYPE_STRUCT, .m_Offset = i * 8, .m_Struct = &light, .m_Name = "1" }, { .m_Field = 2, .m_Type = DATA_TYPE_STRUCT, .m_Offset = i * 8 + 32, .m_Struct = &light, .m_Name = "2" } };
        DataTableDesc table = { i + 1, 0, 0, roots, 2, i * 8 + 64 };
        ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &table));
        DataId id;
        ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, i + 1, i, values, 2, &id));
    }
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    uint8_t* saved = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, bytes, size, &size));
    memcpy(saved, bytes, size);
    HDataStore        loaded = DataCreateStore();
    HDataBlobInstance instance;
    ASSERT_EQ(DATA_RESULT_OK, Load(loaded, bytes, size, &instance));
    HDataStore stores[] = { source, loaded };
    for (uint32_t packed = 0; packed < 2; ++packed)
    {
        HDataStore     store = stores[packed];
        DataQueryField fields[] = { { .m_Field = dmHashString64("2.10"), .m_Type = DATA_TYPE_VECTOR3 }, { .m_Field = dmHashString64("1.10"), .m_Type = DATA_TYPE_VECTOR3 } };
        DataQueryDesc  desc = { 0, 0, 0, 0, fields, 2 };
        HDataQuery     query;
        ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
        // Lookup uses full-name hashes even when the descriptors change order.
        DataQueryField swap = fields[0];
        fields[0] = fields[1];
        fields[1] = swap;
        uint32_t field = DataQueryFindField(query, &fields[0]);
        uint32_t other = DataQueryFindField(query, &fields[1]);
        ASSERT_NE(UINT32_MAX, field);
        ASSERT_NE(UINT32_MAX, other);
        ASSERT_NE(field, other);
        DataStoreLock(store);
        DataIterator it = DataQueryIter(query);
        uint32_t     batches = 0;
        while (DataIterNext(&it) == DATA_RESULT_OK)
        {
            DataRowIterator rows = DataIterRows(&it);
            ASSERT_EQ(DATA_RESULT_OK, DataRowIterNext(&rows));
            DataId             id = DataRowIterGetId(&rows);
            const DataVector3* original = DataRowIterGetVector3(&rows, field);
            ASSERT_EQ(1.0f, original->m_Values[0]);
            ASSERT_EQ(9.0f, DataRowIterGetVector3(&rows, other)->m_Values[0]);
            if (packed)
            {
                uint32_t         table_index = (uint32_t)DataIterGetType(&it) - 1;
                const DataTable* table = GetInstanceTable(instance, table_index);
                ASSERT_EQ((uintptr_t)(table->m_Values.Begin() + table_index * 8), (uintptr_t)original);
            }
            // The same full-name hash works through ID access, including packed metadata.
            DataVector3 copy;
            uint64_t    color_hash = dmHashString64("1.10");
            ASSERT_EQ(DATA_RESULT_OK, DataFieldGetVector3(store, id, color_hash, &copy));
            ASSERT_EQ(1.0f, copy.m_Values[0]);
            ASSERT_EQ(DATA_RESULT_OK, DataFieldGetVector3Batch(store, 1, &id, color_hash, &copy));
            copy.m_Values[0] = 17;
            ASSERT_EQ(DATA_RESULT_OK, DataSetFieldVector3(store, id, color_hash, &copy));
            ASSERT_EQ(17.0f, DataRowIterGetVector3(&rows, field)->m_Values[0]);
            ASSERT_EQ(DATA_RESULT_OK, DataResetField(store, id, color_hash));
            ASSERT_EQ(1.0f, DataRowIterGetVector3(&rows, field)->m_Values[0]);
            ASSERT_EQ(DATA_RESULT_OK, DataSetFieldString(store, id, dmHashString64("1.30"), "changed"));
            ASSERT_EQ(DATA_RESULT_OK, DataResetField(store, id, dmHashString64("1.30")));
            DataRowIterGetVector3Mut(&rows, field)->m_Values[0] = 42;
            ASSERT_EQ(42.0f, DataRowIterGetVector3(&rows, field)->m_Values[0]);
            ASSERT_EQ(9.0f, DataRowIterGetVector3(&rows, other)->m_Values[0]);
            DataValue root, child;
            ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, id, 1, &root));
            ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&root.m_Value.m_Struct, 20, &child));
            ASSERT_EQ(7.0, child.m_Value.m_Number);
            ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&root.m_Value.m_Struct, 30, &child));
            ASSERT_STREQ("shared", child.m_Value.m_String);
            DataMemoryStats before, after;
            GetDataMemoryStats(store, &before);
            DataRowIterGetVector3Mut(&rows, field)->m_Values[1] = 43;
            GetDataMemoryStats(store, &after);
            ASSERT_EQ(before.m_PayloadUsed, after.m_PayloadUsed);
            ASSERT_EQ(before.m_PayloadBlocks, after.m_PayloadBlocks);
            ASSERT_EQ(DATA_RESULT_OK, DataResetField(store, id, 1));
            ASSERT_EQ(2.0f, DataRowIterGetVector3(&rows, field)->m_Values[1]);
            DataRowIterGetVector3Mut(&rows, field)->m_Values[2] = 44;
            ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, id));
            ASSERT_EQ(3.0f, DataRowIterGetVector3(&rows, field)->m_Values[2]);
            ++batches;
        }
        ASSERT_EQ(2u, batches);
        DataStoreUnlock(store);
        DataDestroyQuery(query);
    }
    ASSERT_EQ(0, memcmp(saved, bytes, size));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(loaded));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(source));
    delete[] saved;
    delete[] bytes;
}

TEST(Data, MutableInlineReferencesAndInstanceResetAfterRemoval)
{
    DataFieldDesc  members[] = { { .m_Field = 10, .m_Type = DATA_TYPE_NUMBER, .m_Offset = 0, .m_Name = "10" }, { .m_Field = 20, .m_Type = DATA_TYPE_STRING, .m_Offset = 8, .m_Name = "20" }, { .m_Field = 30, .m_Type = DATA_TYPE_LIST, .m_Offset = 16, .m_Name = "30" } };
    DataStructDesc layout = { members, 3, 24 };
    DataFieldDesc  root = { .m_Field = 1, .m_Type = DATA_TYPE_STRUCT, .m_Offset = 0, .m_Struct = &layout, .m_Name = "1" };
    DataTableDesc  table = { 1, 0, 0, &root, 1, 24 };
    HDataStore     source = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &table));
    uint64_t            names[] = { 10, 20, 30 };
    DataValue           list_values[] = { String("nested"), Number(7) };
    const DataValueType types[] = { DATA_TYPE_NUMBER, DATA_TYPE_STRING, DATA_TYPE_LIST };
    DataValueData       values[] = { Number(0).m_Value, String("shared").m_Value, List(list_values, 2).m_Value };
    DataValue           object = Struct(names, types, values, 3);
    for (uint32_t r = 0; r < 3; ++r)
    {
        values[0].m_Number = 10 + r;
        DataId id;
        ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, 1, 0, &object, 1, &id, r));
    }
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    uint8_t* saved = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, bytes, size, &size));
    memcpy(saved, bytes, size);
    HDataBlob blob;
    ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
    HDataStore        store = DataCreateStore();
    HDataBlobInstance first, second;
    ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(store, blob, 1, &first));
    ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(store, blob, 2, &second));
    DataDestroyBlob(blob);
    DataQueryField fields[] = { { .m_Field = dmHashString64("1.10"), .m_Type = DATA_TYPE_NUMBER }, { .m_Field = dmHashString64("1.20"), .m_Type = DATA_TYPE_STRING } };
    DataQueryDesc  desc = { 0, 0, 0, 0, fields, 2 };
    HDataQuery     query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    uint32_t number_field = DataQueryFindField(query, &fields[0]);
    ASSERT_NE(UINT32_MAX, number_field);
    DataId      ids[2][3] = {};
    const char* shared_list_string = 0;
    DataStoreLock(store);
    DataIterator it = DataQueryIter(query);
    while (DataIterNext(&it) == DATA_RESULT_OK)
    {
        DataRowIterator rows = DataIterRows(&it);
        while (DataRowIterNext(&rows) == DATA_RESULT_OK)
        {
            DataId   id = DataRowIterGetId(&rows);
            uint32_t group = (uint32_t)DataRowIterGetGroupId(&rows) - 1;
            uint32_t component = (uint32_t)DataGetComponentId(store, id);
            ids[group][component] = id;
            ASSERT_EQ(10.0 + component, *DataRowIterGetNumber(&rows, number_field));
            ASSERT_EQ((uintptr_t)DataRowIterGetNumber(&rows, number_field), (uintptr_t)DataRowIterGetNumberMut(&rows, number_field));
            if (!group)
                *DataRowIterGetNumberMut(&rows, number_field) += 100;
            DataValue current, child, element;
            ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, id, 1, &current));
            ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&current.m_Value.m_Struct, 20, &child));
            ASSERT_STREQ("shared", child.m_Value.m_String);
            ASSERT_TRUE((uintptr_t)child.m_Value.m_String >= (uintptr_t)bytes && (uintptr_t)child.m_Value.m_String < (uintptr_t)(bytes + size));
            if (!group)
                ASSERT_EQ(DATA_RESULT_OK, SetTestFieldString(&it, rows.m_Index, 1, "replacement"));
            ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, id, 1, &current));
            ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&current.m_Value.m_Struct, 30, &child));
            ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(&child.m_Value.m_List, 0, &element));
            ASSERT_STREQ("nested", element.m_Value.m_String);
            if (!shared_list_string)
                shared_list_string = element.m_Value.m_String;
            ASSERT_EQ((uintptr_t)shared_list_string, (uintptr_t)element.m_Value.m_String);
        }
    }
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[0][0]));
    DataResetBlob(first);
    ASSERT_EQ((uintptr_t)0, (uintptr_t)first->m_Payloads);
    for (uint32_t group = 0; group < 2; ++group)
        for (uint32_t component = group ? 0 : 1; component < 3; ++component)
        {
            DataValue current, child;
            ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[group][component], 1, &current));
            ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&current.m_Value.m_Struct, 10, &child));
            ASSERT_EQ(10.0 + component, child.m_Value.m_Number);
            ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&current.m_Value.m_Struct, 20, &child));
            ASSERT_STREQ("shared", child.m_Value.m_String);
        }
    ASSERT_EQ(0, memcmp(saved, bytes, size));
    DataDestroyQuery(query);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(source));
    delete[] saved;
    delete[] bytes;
}

TEST(Data, PooledRegistrationResetAndRemovalAfterSlotReuse)
{
    HDataStore source = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, Register(source, 1));
    DataTableDesc empty = { 2, 0, 0, 0, 0, 0 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &empty));
    DataValue values[] = { Number(10), String("shared") };
    DataId    id;
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, 1, 0, values, 2, &id, 11));
    values[0] = Number(20);
    ASSERT_EQ(DATA_RESULT_OK, AddTestRow(source, 1, 0, values, 2, &id, 12));
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, bytes, size, &size));
    HDataBlob blob;
    ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
    HDataStore        store = DataCreateStore();
    HDataBlobInstance first, second;
    ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(store, blob, 77, &first));
    ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(store, blob, 77, &second));
    DataDestroyBlob(blob);
    ASSERT_EQ((uintptr_t)first->m_Pool, (uintptr_t)second->m_Pool);
    ASSERT_EQ(2u, store->m_Tables.Size());
    ASSERT_EQ(0u, GetInstanceTable(first, 1)->m_Rows.Size());
    DataId            ids[2][2];
    HDataBlobInstance instances[] = { first, second };
    for (uint32_t i = 0; i < 2; ++i)
        for (uint32_t r = 0; r < 2; ++r)
        {
            uint32_t slot = GetInstanceSlots(instances[i])[r];
            ids[i][r] = ((uint64_t)store->m_Slots[slot].m_Generation << 32) | slot;
            ASSERT_EQ((uint64_t)(11 + r), DataGetComponentId(store, ids[i][r]));
            ASSERT_EQ(DATA_RESULT_OK, DataSetFieldNumber(store, ids[i][r], 10, 100 + i));
            ASSERT_EQ(DATA_RESULT_OK, DataSetFieldString(store, ids[i][r], 20, i ? "second" : "first"));
        }
    ASSERT_TRUE(first->m_Payloads != 0 && second->m_Payloads != 0);
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[0][0]));
    ASSERT_EQ(UINT32_MAX, GetInstanceSlots(first)[0]);
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 3));
    DataId reused;
    ASSERT_EQ(DATA_RESULT_OK, Add(store, 3, 77, 500, "independent", &reused));
    ASSERT_EQ((uint32_t)ids[0][0], (uint32_t)reused);
    ASSERT_NE(ids[0][0], reused);
    DataResetBlob(first);
    ASSERT_EQ((uintptr_t)0, (uintptr_t)first->m_Payloads);
    ASSERT_TRUE(second->m_Payloads != 0);
    double      number;
    const char* string;
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, ids[0][1], 10, &number));
    ASSERT_EQ(20.0, number);
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetString(store, ids[0][1], 20, &string));
    ASSERT_STREQ("shared", string);
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveBlob(first));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGetNumber(store, ids[0][1], 10, &number));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, reused, 10, &number));
    ASSERT_EQ(500.0, number);
    for (uint32_t r = 0; r < 2; ++r)
    {
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, ids[1][r], 10, &number));
        ASSERT_EQ(101.0, number);
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetString(store, ids[1][r], 20, &string));
        ASSERT_STREQ("second", string);
    }
    DataResetBlob(second);
    for (uint32_t r = 0; r < 2; ++r)
    {
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, ids[1][r], 10, &number));
        ASSERT_EQ(10.0 + r * 10, number);
        ASSERT_EQ((uint64_t)(11 + r), DataGetComponentId(store, ids[1][r]));
    }
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveBlob(second));
    ASSERT_EQ((uintptr_t)0, (uintptr_t)store->m_Pools);
    ASSERT_EQ(1u, store->m_Tables.Size());
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetString(store, reused, 20, &string));
    ASSERT_STREQ("independent", string);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(source));
    delete[] bytes;
}

TEST(Data, CompactMetadataLimitsRoundTrip)
{
    // A reusable binary tree reaches 65,535 entries with only two siblings per
    // level, keeping validation linear instead of constructing one huge flat type.
    DataStructDesc layouts[15] = {};
    DataFieldDesc  members[15][2] = {};
    for (uint32_t depth = 0; depth < 15; ++depth)
    {
        for (uint32_t i = 0; i < 2; ++i)
        {
            DataFieldDesc field = {
                .m_Field = i + 1,
                .m_Type = depth == 14 ? DATA_TYPE_NULL : DATA_TYPE_STRUCT,
                .m_Struct = depth == 14 ? 0 : &layouts[depth + 1],
                .m_Name = i ? "right" : "left"
            };
            members[depth][i] = field;
        }
        layouts[depth].m_Fields = members[depth];
        layouts[depth].m_FieldCount = 2;
    }
    DataFieldDesc roots[] = {
        { .m_Field = 1, .m_Type = DATA_TYPE_STRUCT, .m_Struct = layouts, .m_Name = "1" },
        { .m_Field = 2, .m_Type = DATA_TYPE_NULL, .m_Name = "2" }
    };
    uint64_t* tags = new uint64_t[DATA_MAX_TAG_COUNT];
    for (uint32_t i = 0; i < DATA_MAX_TAG_COUNT; ++i)
        tags[i] = i;
    HDataStore    store = DataCreateStore();
    DataTableDesc desc = { .m_Type = 1, .m_Tags = tags, .m_TagCount = DATA_MAX_TAG_COUNT + 1, .m_Fields = roots, .m_FieldCount = 1 };
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &desc));
    desc.m_TagCount = DATA_MAX_TAG_COUNT;
    desc.m_FieldCount = DATA_MAX_METADATA_COUNT + 1;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &desc));
    desc.m_FieldCount = 2;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &desc)); // Nested total is 65,536.
    ASSERT_EQ(0u, store->m_Tables.Size());
    desc.m_FieldCount = 1;
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &desc));
    ASSERT_EQ(DATA_MAX_METADATA_COUNT, (uint32_t)store->m_Tables[0]->m_MetadataCount);
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, bytes, size, &size));
    HDataStore        loaded = DataCreateStore();
    HDataBlobInstance instance;
    ASSERT_EQ(DATA_RESULT_OK, Load(loaded, bytes, size, &instance));
    DataTable* table = GetInstanceTable(instance, 0);
    ASSERT_EQ(DATA_MAX_METADATA_COUNT, (uint32_t)table->m_MetadataCount);
    ASSERT_EQ(DATA_MAX_TAG_COUNT, (uint32_t)table->m_TagCount);
    ASSERT_EQ((uint64_t)(DATA_MAX_TAG_COUNT - 1), GetTableTag(table, DATA_MAX_TAG_COUNT - 1));
    for (uint32_t i = 0; i < DATA_MAX_METADATA_COUNT; ++i)
    {
        DataFieldMeta expected = GetFieldMeta(store->m_Tables[0], i);
        DataFieldMeta actual = GetFieldMeta(table, i);
        ASSERT_EQ(expected.m_Field, actual.m_Field);
        ASSERT_EQ(expected.m_Name, actual.m_Name);
        ASSERT_EQ(expected.m_Offset, actual.m_Offset);
        ASSERT_EQ(expected.m_ChildIndex, actual.m_ChildIndex);
        ASSERT_EQ(expected.m_ChildCount, actual.m_ChildCount);
    }
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(loaded));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    delete[] bytes;
    delete[] tags;
}

TEST(Data, CompactMetadataPreservesWideByteOffsets)
{
    HDataStore    store = DataCreateStore();
    DataFieldDesc field = { .m_Field = 10, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 65536 };
    DataTableDesc table = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = 65548 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    DataValueType type = DATA_TYPE_VECTOR3;
    DataValueData value = { .m_Vector3 = { 3, 5, 7 } };
    DataId        id;
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(store, 1, 42, &type, &value, 1, &id));
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, bytes, size, &size));
    HDataStore        loaded = DataCreateStore();
    HDataBlobInstance instance;
    ASSERT_EQ(DATA_RESULT_OK, Load(loaded, bytes, size, &instance));
    DataQueryField requested = { .m_Field = 10, .m_Type = DATA_TYPE_VECTOR3 };
    DataQueryDesc  desc = { .m_Fields = &requested, .m_FieldCount = 1 };
    HDataQuery     query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(loaded, &desc, &query));
    uint32_t handle = DataQueryFindField(query, &requested);
    DataStoreLock(loaded);
    DataIterator it = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
    DataRowIterator rows = DataIterRows(&it);
    ASSERT_EQ(DATA_RESULT_OK, DataRowIterNext(&rows));
    const DataVector3* actual = DataRowIterGetVector3(&rows, handle);
    ASSERT_EQ(0, memcmp(value.m_Vector3, actual, sizeof(DataVector3)));
    ASSERT_EQ(65548u, it.m_RowStride);
    DataStoreUnlock(loaded);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(query));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(loaded));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    delete[] bytes;
}

TEST(Data, CompactQueryFieldsValidateBeforeNarrowing)
{
    HDataStore     store = DataCreateStore();
    DataQueryField field = { .m_Field = 1, .m_Type = DATA_TYPE_VECTOR3, .m_Access = DATA_ACCESS_READ_WRITE };
    DataQueryDesc  desc = { .m_Fields = &field, .m_FieldCount = 1 };
    HDataQuery     query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    ASSERT_EQ(0u, DataQueryFindField(query, &field));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(query));
    field.m_Type = (DataValueType)256;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataCreateQuery(store, &desc, &query));
    field.m_Type = DATA_TYPE_VECTOR3;
    field.m_Access = (DataAccess)256;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataCreateQuery(store, &desc, &query));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, FreeSlotCountExcludesRetiredGenerations)
{
    HDataStore    store = DataCreateStore();
    DataTableDesc table = { .m_Type = 1 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    DataRowDesc rows[6] = {};
    DataId      ids[6];
    ASSERT_EQ(DATA_RESULT_OK, DataAddRows(store, 1, rows, 4, ids));
    ASSERT_EQ(0u, store->m_FreeSlotCount);
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[0]));
    ASSERT_EQ(1u, store->m_FreeSlotCount);
    // An exhausted generation is retired permanently rather than reusable.
    store->m_Slots[(uint32_t)ids[1]].m_Generation = UINT32_MAX;
    DataId retired = ((uint64_t)UINT32_MAX << 32) | (uint32_t)ids[1];
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, retired));
    ASSERT_EQ(1u, store->m_FreeSlotCount);
    DataId added[2];
    ASSERT_EQ(DATA_RESULT_OK, DataAddRows(store, 1, rows, 2, added));
    ASSERT_EQ(0u, store->m_FreeSlotCount);
    ASSERT_EQ(5u, store->m_Slots.Size());
    ASSERT_NE(ids[0], added[0]);
    ASSERT_EQ((uint32_t)ids[0], (uint32_t)added[0]);
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataRemoveRow(store, ids[0]));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataRemoveRow(store, retired));
    ASSERT_EQ(DATA_RESULT_OK, DataUnregisterTable(store, 1));
    ASSERT_EQ(4u, store->m_FreeSlotCount);
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    ASSERT_EQ(DATA_RESULT_OK, DataAddRows(store, 1, rows, 6, ids));
    ASSERT_EQ(0u, store->m_FreeSlotCount);
    ASSERT_EQ(7u, store->m_Slots.Size());
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, BatchVector3MatchesScalarGetter)
{
    HDataStore    store = DataCreateStore();
    DataFieldDesc vector = { .m_Field = 42, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 0 };
    DataFieldDesc number = { .m_Field = 42, .m_Type = DATA_TYPE_NUMBER, .m_Offset = 0 };
    DataFieldDesc missing = { .m_Field = 43, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 0 };
    DataFieldDesc collision[] = {
        { .m_Field = 58, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 0 },
        { .m_Field = 42, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 12 }
    };
    DataTableDesc tables[] = {
        { .m_Type = 1, .m_Fields = &vector, .m_FieldCount = 1, .m_RowStride = 12 },
        { .m_Type = 2, .m_Fields = &number, .m_FieldCount = 1, .m_RowStride = 8 },
        { .m_Type = 3, .m_Fields = &missing, .m_FieldCount = 1, .m_RowStride = 12 },
        { .m_Type = 4, .m_Fields = collision, .m_FieldCount = 2, .m_RowStride = 24 }
    };
    DataValueType vector_types[] = { DATA_TYPE_VECTOR3, DATA_TYPE_VECTOR3 };
    DataValueType number_type = DATA_TYPE_NUMBER;
    DataValueData vectors[] = { { .m_Vector3 = { 1, 2, 3 } }, { .m_Vector3 = { 4, 5, 6 } } };
    DataValueData scalar = { .m_Number = 1 };
    DataId        valid[5];
    for (uint32_t i = 0; i < 4; ++i)
    {
        ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &tables[i]));
        ASSERT_EQ(DATA_RESULT_OK, DataAddRow(store, tables[i].m_Type, i, i == 1 ? &number_type : vector_types, i == 1 ? &scalar : vectors, tables[i].m_FieldCount, &valid[i]));
    }
    DataId stale = valid[0];
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, stale));
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(store, 1, 77, vector_types, vectors, 1, &valid[0]));
    // Include a packed row alongside independent rows in the same store.
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, bytes, size, &size));
    HDataBlob         blob;
    HDataBlobInstance instance;
    ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
    ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(store, blob, 88, &instance));
    valid[4] = MakeId(store, GetInstanceSlots(instance)[0]);
    DataId      ids[83];
    DataVector3 actual[83];
    DataId      successful[] = { valid[0], valid[3], valid[4] };
    for (uint32_t i = 0; i < 83; ++i)
        ids[i] = successful[i % 3];
    DataStoreLock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetVector3Batch(store, 83, ids, 42, actual));
    for (uint32_t i = 0; i < 83; ++i)
    {
        DataVector3 expected;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetVector3(store, ids[i], 42, &expected));
        ASSERT_EQ(0, memcmp(&actual[i], &expected, sizeof(expected)));
    }

    // Failures before/after an internal batch boundary return the scalar error.
    // Output on error is deliberately not inspected: partial writes are allowed.
    DataId   bad[] = { valid[1], valid[2], stale, UINT64_MAX, 0 };
    uint32_t positions[] = { 0, 1, 63, 64, 82 };
    for (uint32_t b = 0; b < sizeof(bad) / sizeof(bad[0]); ++b)
    {
        DataVector3 ignored;
        DataResult  expected = DataFieldGetVector3(store, bad[b], 42, &ignored);
        ASSERT_NE(DATA_RESULT_OK, expected);
        for (uint32_t p = 0; p < sizeof(positions) / sizeof(positions[0]); ++p)
        {
            uint32_t index = positions[p];
            DataId   saved = ids[index];
            ids[index] = bad[b];
            ASSERT_EQ(expected, DataFieldGetVector3Batch(store, 83, ids, 42, actual));
            ids[index] = saved;
        }
    }
    // The first error follows input order even when later errors differ.
    ids[3] = valid[2];
    ids[80] = valid[1];
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGetVector3Batch(store, 83, ids, 42, actual));
    ids[3] = valid[1];
    ids[80] = valid[2];
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataFieldGetVector3Batch(store, 83, ids, 42, actual));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetVector3Batch(0, 0, 0, 42, 0));
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveBlob(instance));
    DataDestroyBlob(blob);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    delete[] bytes;
}

TEST(Data, NativeRowsShareGroupsWithoutSplittingTables)
{
    HDataStore    store = DataCreateStore();
    DataFieldDesc field = { .m_Field = 10, .m_Type = DATA_TYPE_NUMBER };
    DataTableDesc table = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(double) };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    DataGroupId   group = 77;
    DataQueryDesc desc = { .m_GroupIds = &group, .m_GroupIdCount = 1 };
    HDataQuery    query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    double values[] = { 1, 2, 3 };
    DataId first[3], other[3], last[3];
    ASSERT_EQ(DATA_RESULT_OK, DataCreateRows(store, 1, group, 3, values, first));
    ASSERT_EQ(DATA_RESULT_OK, DataCreateRows(store, 1, 0, 3, values, other));
    ASSERT_EQ(DATA_RESULT_OK, DataCreateRows(store, 1, group, 3, values, last));
    ASSERT_EQ(1u, store->m_Tables.Size());
    // Removing the first row swaps the last row into its slot; group filters survive.
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, first[0]));
    for (uint32_t pass = 0; pass < 2; ++pass)
    {
        ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(query));
        ASSERT_EQ(5u, DataQueryGetRowCount(query));
        DataIterator it = DataQueryIterRange(query, 0, 5);
        uint32_t     visited = 0;
        while (DataIterNext(&it) == DATA_RESULT_OK)
        {
            DataRowIterator rows = DataIterRows(&it);
            while (DataRowIterNext(&rows) == DATA_RESULT_OK)
            {
                ASSERT_EQ(group, DataRowIterGetGroupId(&rows));
                DataId id = DataRowIterGetId(&rows);
                ASSERT_NE(first[0], id);
                for (uint32_t i = 0; i < 3; ++i)
                    ASSERT_NE(other[i], id);
                ++visited;
            }
        }
        ASSERT_EQ(5u, visited);
        DataQueryEnd(query);
        // A reset changes only values, leaving the shared group intact.
        ASSERT_EQ(DATA_RESULT_OK, DataSetFieldNumber(store, last[2], 10, 100));
        ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, last[2]));
    }
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(query));
    desc.m_GroupIds = 0;
    desc.m_GroupIdCount = 0;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    DataStoreLock(store);
    DataIterator it = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
    DataRowIterator rows = DataIterRows(&it);
    uint32_t        zero_group = 0;
    while (DataRowIterNext(&rows) == DATA_RESULT_OK)
        zero_group += DataRowIterGetGroupId(&rows) == 0;
    ASSERT_EQ(3u, zero_group);
    ASSERT_EQ(DATA_RESULT_END, DataIterNext(&it));
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(query));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, CreateRowsWithoutReturningIds)
{
    HDataStore    store = DataCreateStore();
    DataFieldDesc field = { .m_Field = 10, .m_Type = DATA_TYPE_NUMBER };
    DataTableDesc table = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(double) };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    DataQueryField query_field = { .m_Field = 10, .m_Type = DATA_TYPE_NUMBER };
    DataQueryDesc  desc = { .m_Fields = &query_field, .m_FieldCount = 1 };
    HDataQuery     query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    uint32_t       field_index = DataQueryFindField(query, &query_field);
    double         values[] = { 1, 2, 3, 4 };
    DataFieldArray column = { .m_Field = 10, .m_Values = values + 2 };
    DataValueType  type = DATA_TYPE_NUMBER;
    DataValueData  decoded = { .m_Number = 5 };
    DataRowDesc    row = { .m_Group = 42, .m_Types = &type, .m_Values = &decoded, .m_ValueCount = 1 };
    DataId         ids[6] = {};
    for (uint32_t pass = 0; pass < 2; ++pass)
    {
        ASSERT_EQ(DATA_RESULT_OK, DataCreateRows(store, 1, 42, 2, values, 0));
        ASSERT_EQ(DATA_RESULT_OK, DataCreateRowsSoA(store, 1, 42, 2, 1, &column, 0));
        decoded.m_Number = 5;
        ASSERT_EQ(DATA_RESULT_OK, DataAddRows(store, 1, &row, 1, 0));
        decoded.m_Number = 6;
        ASSERT_EQ(DATA_RESULT_OK, DataAddRow(store, 1, 42, &type, &decoded, 1, 0));

        ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(query));
        ASSERT_EQ(6u, DataQueryGetRowCount(query));
        DataIterator it = DataQueryIterRange(query, 0, 6);
        uint32_t     seen = 0;
        while (DataIterNext(&it) == DATA_RESULT_OK)
        {
            DataRowIterator rows = DataIterRows(&it);
            while (DataRowIterNext(&rows) == DATA_RESULT_OK)
            {
                double value = *DataRowIterGetNumber(&rows, field_index);
                ASSERT_TRUE(value >= 1 && value <= 6);
                uint32_t index = (uint32_t)value - 1;
                ASSERT_EQ(0u, seen & (1u << index));
                seen |= 1u << index;
                ASSERT_EQ(42u, DataRowIterGetGroupId(&rows));
                DataId id = DataRowIterGetId(&rows);
                ASSERT_NE(ids[index], id);
                ids[index] = id;
            }
        }
        ASSERT_EQ(63u, seen);
        DataQueryEnd(query);
        // Query-discovered IDs support lookup and removal, including reused slots.
        for (uint32_t i = 0; i < 6; ++i)
        {
            double value;
            ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, ids[i], 10, &value));
            ASSERT_EQ((double)(i + 1), value);
            ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[i]));
            ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGetNumber(store, ids[i], 10, &value));
        }
    }
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(query));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, NativeRowsValidateBeforeWritingAcrossBlocks)
{
    const uint32_t count = 1031;
    struct NativeRow
    {
        uint8_t     m_Enabled;
        DataVector3 m_Position;
    };
    NativeRow input[count] = {};
    DataId    ids[count];
    for (uint32_t i = 0; i < count; ++i)
    {
        input[i].m_Enabled = i & 1;
        input[i].m_Position.m_Values[0] = (float)i;
        input[i].m_Position.m_Values[1] = 2;
        input[i].m_Position.m_Values[2] = 3;
        ids[i] = 99;
    }
    DataFieldDesc fields[] = {
        { .m_Field = 1, .m_Type = DATA_TYPE_BOOLEAN, .m_Offset = offsetof(NativeRow, m_Enabled) },
        { .m_Field = 2, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = offsetof(NativeRow, m_Position) }
    };
    DataTableDesc table = { .m_Type = 1, .m_Fields = fields, .m_FieldCount = 2, .m_RowStride = sizeof(NativeRow) };
    HDataStore    store = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    input[count - 1].m_Enabled = 2;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataCreateRows(store, 1, 42, count, input, ids));
    ASSERT_TRUE(FindTable(store, 1)->m_Rows.Empty());
    for (uint32_t i = 0; i < count; ++i)
        ASSERT_EQ(99u, ids[i]);

    input[count - 1].m_Enabled = 0;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateRows(store, 1, 42, count, input, ids));
    for (uint32_t i = 0; i < count; ++i)
    {
        uint8_t     enabled;
        DataVector3 position;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetBoolean(store, ids[i], 1, &enabled));
        ASSERT_EQ(input[i].m_Enabled, enabled);
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetVector3(store, ids[i], 2, &position));
        ASSERT_EQ(0, memcmp(&input[i].m_Position, &position, sizeof(position)));
    }
    DataVector3 changed = { .m_Values = { -1, -2, -3 } };
    ASSERT_EQ(DATA_RESULT_OK, DataSetFieldVector3(store, ids[count - 1], 2, &changed));
    ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, ids[count - 1]));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetVector3(store, ids[count - 1], 2, &changed));
    ASSERT_EQ((float)(count - 1), changed.m_Values[0]);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, BatchStringsAcrossOwnedAndPackedRows)
{
    HDataStore store = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 1));
    DataId ids[131];
    char   text[32];
    for (uint32_t i = 0; i < 131; ++i)
    {
        snprintf(text, sizeof(text), "value-%u", i);
        ASSERT_EQ(DATA_RESULT_OK, Add(store, 1, i, i, text, &ids[i]));
    }
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, bytes, size, &size));
    HDataBlob         blob;
    HDataBlobInstance instance;
    ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
    ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(store, blob, 999, &instance));
    for (uint32_t i = 0; i < 131; i += 2)
        ids[i] = MakeId(store, GetInstanceSlots(instance)[i]);
    const char* strings[131];
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetStringBatch(store, 131, ids, 20, strings));
    for (uint32_t i = 0; i < 131; ++i)
    {
        snprintf(text, sizeof(text), "value-%u", i);
        ASSERT_STREQ(text, strings[i]);
    }
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataFieldGetStringBatch(store, 131, ids, 10, strings));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGetStringBatch(store, 131, ids, 999, strings));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetStringBatch(0, 0, 0, 20, 0));
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveBlob(instance));
    DataDestroyBlob(blob);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    delete[] bytes;
}

TEST(Data, NativeRowLifecycleFromC)
{
    ASSERT_EQ(0, TestDataNativeRowsFromC());
}

TEST(Data, SoARowLifecycleFromC)
{
    ASSERT_EQ(0, TestDataSoARowsFromC());
}

TEST(Data, SoARowsWithManyFieldsValidateBeforePublishing)
{
    const uint32_t field_count = 40;
    const uint32_t count = 1031;
    struct Row
    {
        double  m_Values[field_count];
        uint8_t m_Enabled;
    };
    double         values[field_count][count];
    uint8_t        enabled[count];
    DataFieldDesc  layout[field_count + 1] = {};
    DataFieldArray fields[field_count + 1] = {};
    DataId         ids[count];
    for (uint32_t f = 0; f < field_count; ++f)
    {
        layout[f] = { .m_Field = 100 + f, .m_Type = DATA_TYPE_NUMBER, .m_Offset = (uint32_t)(f * sizeof(double)) };
        fields[field_count - f] = { .m_Field = 100 + f, .m_Values = values[f] };
        for (uint32_t r = 0; r < count; ++r)
            values[f][r] = f * count + r;
    }
    layout[field_count] = { .m_Field = 200, .m_Type = DATA_TYPE_BOOLEAN, .m_Offset = offsetof(Row, m_Enabled) };
    fields[0] = { .m_Field = 200, .m_Values = enabled };
    for (uint32_t r = 0; r < count; ++r)
    {
        enabled[r] = r & 1;
        ids[r] = 99;
    }
    HDataStore    store = DataCreateStore();
    DataTableDesc desc = { .m_Type = 1, .m_Fields = layout, .m_FieldCount = field_count + 1, .m_RowStride = sizeof(Row) };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &desc));
    DataTable* table = FindTable(store, 1);
    uint64_t   revision = store->m_Revision;
    enabled[count - 1] = 2;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataCreateRowsSoA(store, 1, 42, count, field_count + 1, fields, ids));
    ASSERT_EQ(revision, store->m_Revision);
    ASSERT_TRUE(table->m_Rows.Empty());
    ASSERT_TRUE(table->m_Owned->m_BaseRows.Empty());
    for (uint32_t r = 0; r < count; ++r)
        ASSERT_EQ(99u, ids[r]);

    enabled[count - 1] = 0;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateRowsSoA(store, 1, 42, count, field_count + 1, fields, ids));
    for (uint32_t f = 0; f < field_count; ++f)
    {
        for (uint32_t r = 0; r < count; ++r)
        {
            double value;
            ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, ids[r], 100 + f, &value));
            ASSERT_EQ(values[f][r], value);
        }
    }
    // Append after swap removal and verify earlier rows/defaults remain intact.
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[0]));
    DataId added;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateRowsSoA(store, 1, 0, 1, field_count + 1, fields, &added));
    ASSERT_NE(ids[0], added);
    values[3][count - 1] = -1;
    ASSERT_EQ(DATA_RESULT_OK, DataSetFieldNumber(store, ids[count - 1], 103, 0));
    ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, ids[count - 1]));
    double value;
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, ids[count - 1], 103, &value));
    ASSERT_EQ((double)(4 * count - 1), value);
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, added, 103, &value));
    ASSERT_EQ((double)(3 * count), value);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

static void CheckNativeReferences(HDataStore store, const DataId ids[2])
{
    for (uint32_t i = 0; i < 2; ++i)
    {
        DataValue object;
        DataValue child;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[i], 11, &object));
        ASSERT_EQ(DATA_TYPE_STRUCT, object.m_Type);
        ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&object.m_Value.m_Struct, 1, &child));
        ASSERT_STREQ(i ? "bandits" : "guards", child.m_Value.m_String);
        ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&object.m_Value.m_Struct, 2, &child));
        ASSERT_EQ(i ? 25.0 : 15.0, child.m_Value.m_Number);
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[i], 12, &object));
        ASSERT_EQ(DATA_TYPE_LIST, object.m_Type);
        ASSERT_EQ(2u, object.m_Value.m_List.m_Count);
        ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(&object.m_Value.m_List, 0, &child));
        if (i)
        {
            ASSERT_EQ(DATA_TYPE_STRUCT, child.m_Type);
            DataValue faction;
            ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&child.m_Value.m_Struct, 1, &faction));
            ASSERT_STREQ("guards", faction.m_Value.m_String);
        }
        else
        {
            ASSERT_EQ(DATA_TYPE_STRING, child.m_Type);
            ASSERT_STREQ("coin", child.m_Value.m_String);
        }
    }
}

TEST(Data, NativeReferencesFromC)
{
    for (uint32_t soa = 0; soa < 2; ++soa)
    {
        HDataStore store = DataCreateStore();
        DataId     ids[2];
        ASSERT_EQ(0, CreateReferenceRowsFromC(store, soa, ids));
        CheckNativeReferences(store, ids);
        DataTable* table = FindTable(store, 93);
        ASSERT_TRUE(table->m_Owned->m_BaseValues != 0);
        ASSERT_EQ((DataBlock*)0, table->m_Owned->m_BaseValues->m_Next); // One payload allocation for the batch.

        uint8_t  DM_ALIGNED(8) bytes[4096];
        uint32_t size;
        ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, bytes, sizeof(bytes), &size));
        HDataBlob blob;
        ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
        HDataStore        loaded = DataCreateStore();
        HDataBlobInstance instance;
        ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(loaded, blob, 42, &instance));
        DataDestroyBlob(blob);
        DataId loaded_ids[] = { MakeId(loaded, 0), MakeId(loaded, 1) };
        CheckNativeReferences(loaded, loaded_ids);
        ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(loaded));
        ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[0]));
        ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, ids[1]));
        const char* name;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetString(store, ids[1], 10, &name));
        ASSERT_STREQ("Bandit", name);
        ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    }
}

TEST(Data, NativeReferenceValidation)
{
    for (uint32_t soa = 0; soa < 2; ++soa)
    {
        HDataStore    store = DataCreateStore();
        DataFieldDesc field = { .m_Field = 1, .m_Type = DATA_TYPE_LIST };
        DataTableDesc table = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(DataReference) };
        ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
        double        numbers[] = { 1, 2 };
        DataListInput lists[] = {
            { .m_Type = DATA_TYPE_NUMBER, .m_Count = 2, .m_Values = numbers },
            { .m_Type = DATA_TYPE_NUMBER, .m_Count = 1 }
        };
        DataReference  rows[] = { { .m_List = &lists[0] }, { .m_List = &lists[1] } };
        DataFieldArray column = { .m_Field = 1, .m_Values = rows };
        DataId         ids[] = { 123, 456 };
        for (uint32_t invalid = 0; invalid < 5; ++invalid)
        {
            switch (invalid)
            {
                case 1:
                    lists[1] = { .m_Type = (DataValueType)UINT32_MAX };
                    break;
                case 2:
                    lists[1] = { .m_Type = DATA_TYPE_LIST, .m_Count = 1, .m_Values = &rows[1] }; // Cycle exceeds depth limit.
                    break;
                case 3:
                    rows[1].m_List = 0;
                    break;
                case 4:
                    lists[1] = { .m_Type = DATA_TYPE_STRING, .m_Count = 1, .m_Values = &rows[1] }; // NULL string.
                    rows[0].m_List = &lists[1];
                    break;
            }
            DataResult result = soa ? DataCreateRowsSoA(store, 1, 0, 2, 1, &column, ids) : DataCreateRows(store, 1, 0, 2, rows, ids);
            ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, result);
            ASSERT_EQ((DataId)123, ids[0]);
            ASSERT_EQ((DataId)456, ids[1]);
            DataTable* stored = FindTable(store, 1);
            ASSERT_TRUE(stored->m_Rows.Empty());
            ASSERT_TRUE(stored->m_Owned->m_BaseRows.Empty());
            ASSERT_EQ((DataBlock*)0, stored->m_Owned->m_BaseValues);
        }
        lists[1] = { .m_Type = DATA_TYPE_NUMBER }; // Empty lists need no values array.
        rows[0].m_List = &lists[0];
        rows[1].m_List = &lists[1];
        DataResult result = soa ? DataCreateRowsSoA(store, 1, 0, 2, 1, &column, 0) : DataCreateRows(store, 1, 0, 2, rows, 0);
        ASSERT_EQ(DATA_RESULT_OK, result);
        ASSERT_EQ(2u, FindTable(store, 1)->m_Rows.Size());
        ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    }
}

TEST(Data, NativeDynamicStructAndNestedListInput)
{
    HDataStore    store = DataCreateStore();
    DataFieldDesc field = { .m_Field = 1, .m_Type = DATA_TYPE_STRUCT };
    DataTableDesc table = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(DataReference) };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    uint64_t      bytes[] = { 2, 0 };
    DataFieldDesc members[] = {
        { .m_Field = 1, .m_Type = DATA_TYPE_BOOLEAN },
        { .m_Field = 2, .m_Type = DATA_TYPE_NULL, .m_Offset = 8 }
    };
    DataStructDesc  layout = { .m_Fields = members, .m_FieldCount = 2, .m_Size = sizeof(bytes) };
    DataStructInput object = { .m_Layout = &layout, .m_Values = bytes };
    DataReference   row = { .m_Struct = &object };
    DataId          id = 123;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataCreateRows(store, 1, 0, 1, &row, &id)); // Invalid Boolean.
    bytes[0] = 1;
    members[1].m_Field = 1;
    ASSERT_EQ(DATA_RESULT_ALREADY_EXISTS, DataCreateRows(store, 1, 0, 1, &row, &id));
    members[1].m_Field = 2;
    members[1].m_Offset = 17;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataCreateRows(store, 1, 0, 1, &row, &id));
    object.m_Layout = 0;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataCreateRows(store, 1, 0, 1, &row, &id));
    ASSERT_EQ((DataId)123, id);
    object.m_Layout = &layout;
    layout = {};
    object.m_Values = 0;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateRows(store, 1, 0, 1, &row, &id)); // Empty dynamic struct.
    DataValue value;
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, id, 1, &value));
    ASSERT_EQ(0u, value.m_Value.m_Struct.m_Count);

    field.m_Type = DATA_TYPE_LIST;
    table.m_Type = 2;
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    DataVector3   vector = { .m_Values = { 1, 2, 3 } };
    DataListInput child = { .m_Type = DATA_TYPE_VECTOR3, .m_Count = 1, .m_Values = &vector };
    DataReference element = { .m_List = &child };
    DataListInput parent = { .m_Type = DATA_TYPE_LIST, .m_Count = 1, .m_Values = &element };
    row.m_List = &parent;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateRows(store, 2, 0, 1, &row, &id));
    vector.m_Values[2] = 99;
    DataValue nested;
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, id, 1, &value));
    ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(&value.m_Value.m_List, 0, &nested));
    ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(&nested.m_Value.m_List, 0, &value));
    ASSERT_EQ(DATA_TYPE_VECTOR3, value.m_Type);
    ASSERT_EQ(3.0f, value.m_Value.m_Vector3[2]);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

static void CheckMixedList(const DataList* list)
{
    ASSERT_EQ(9u, list->m_Count);
    const DataValueType types[] = {
        DATA_TYPE_NUMBER, DATA_TYPE_STRING, DATA_TYPE_BOOLEAN, DATA_TYPE_STRUCT, DATA_TYPE_LIST, DATA_TYPE_NULL, DATA_TYPE_VECTOR3, DATA_TYPE_VECTOR4, DATA_TYPE_MATRIX4
    };
    for (uint32_t i = 0; i < list->m_Count; ++i)
    {
        DataValue value;
        ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(list, i, &value));
        ASSERT_EQ(types[i], value.m_Type);
    }
    DataValue value;
    ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(list, 0, &value));
    ASSERT_EQ(100.0, value.m_Value.m_Number);
    ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(list, 1, &value));
    ASSERT_STREQ("coin", value.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(list, 2, &value));
    ASSERT_EQ(1, value.m_Value.m_Boolean);
    ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(list, 3, &value));
    DataValue child;
    ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&value.m_Value.m_Struct, 42, &child));
    ASSERT_EQ(100.0, child.m_Value.m_Number);
    ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(list, 4, &value));
    ASSERT_EQ(2u, value.m_Value.m_List.m_Count);
    ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(&value.m_Value.m_List, 0, &child));
    ASSERT_EQ(1.0, child.m_Value.m_Number);
    ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(list, 6, &value));
    ASSERT_EQ(3.0f, value.m_Value.m_Vector3[2]);
    ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(list, 7, &value));
    ASSERT_EQ(7.0f, value.m_Value.m_Vector4[3]);
    ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(list, 8, &value));
    ASSERT_EQ(1.0f, value.m_Value.m_Matrix4[15]);
}

TEST(Data, MixedListCreationFromC)
{
    for (uint32_t soa = 0; soa < 2; ++soa)
    {
        HDataStore store = DataCreateStore();
        DataId     ids[2];
        ASSERT_EQ(0, CreateMixedListRowsFromC(store, soa, ids));
        DataValue value;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[0], 1, &value));
        CheckMixedList(&value.m_Value.m_List);
        DataValue parent;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[1], 1, &parent));
        ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(&parent.m_Value.m_List, 0, &value));
        CheckMixedList(&value.m_Value.m_List);
        DataTable* table = FindTable(store, 94);
        ASSERT_EQ((DataBlock*)0, table->m_Owned->m_BaseValues->m_Next);

        DataValue empty = { .m_Type = DATA_TYPE_LIST };
        ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, ids[0], 1, &empty));
        ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, ids[0]));
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[0], 1, &value));
        CheckMixedList(&value.m_Value.m_List);

        uint8_t  DM_ALIGNED(8) bytes[4096];
        uint32_t size;
        ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, bytes, sizeof(bytes), &size));
        HDataBlob blob;
        ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
        HDataStore        loaded = DataCreateStore();
        HDataBlobInstance instance;
        ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(loaded, blob, 42, &instance));
        DataDestroyBlob(blob);
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(loaded, MakeId(loaded, 0), 1, &value));
        CheckMixedList(&value.m_Value.m_List);
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(loaded, MakeId(loaded, 1), 1, &parent));
        ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(&parent.m_Value.m_List, 0, &value));
        CheckMixedList(&value.m_Value.m_List);
        ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(loaded));
        ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    }
}

TEST(Data, MixedListValidationBeforePublishing)
{
    for (uint32_t soa = 0; soa < 2; ++soa)
    {
        HDataStore    store = DataCreateStore();
        DataFieldDesc field = { .m_Field = 1, .m_Type = DATA_TYPE_LIST };
        DataTableDesc table = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(DataReference) };
        ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
        DataValueType  type = DATA_TYPE_NUMBER;
        const void*    payload = 0;
        DataListInput  mixed = { .m_Count = 1, .m_Values = &payload, .m_Types = &type };
        DataListInput  empty = { .m_Type = DATA_TYPE_NULL };
        DataReference  rows[] = { { .m_List = &empty }, { .m_List = &mixed } };
        DataFieldArray column = { .m_Field = 1, .m_Values = rows };
        DataId         ids[] = { 123, 456 };
        uint8_t        boolean = 2;
        for (uint32_t invalid = 0; invalid < 5; ++invalid)
        {
            switch (invalid)
            {
                case 1:
                    type = (DataValueType)UINT32_MAX;
                    break;
                case 2:
                    type = DATA_TYPE_BOOLEAN;
                    payload = &boolean;
                    break;
                case 3:
                    type = DATA_TYPE_LIST;
                    payload = &mixed; // Mixed-list cycle reaches the existing depth limit.
                    break;
                case 4:
                    mixed.m_Values = 0;
                    break;
            }
            DataResult result = soa ? DataCreateRowsSoA(store, 1, 0, 2, 1, &column, ids) : DataCreateRows(store, 1, 0, 2, rows, ids);
            ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, result);
            ASSERT_EQ((DataId)123, ids[0]);
            ASSERT_EQ((DataId)456, ids[1]);
            DataTable* stored = FindTable(store, 1);
            ASSERT_TRUE(stored->m_Rows.Empty());
            ASSERT_EQ((DataBlock*)0, stored->m_Owned->m_BaseValues);
        }
        mixed.m_Count = 0; // Empty mixed lists need no payload pointer array.
        ASSERT_EQ(DATA_RESULT_OK, DataCreateRows(store, 1, 0, 1, rows + 1, 0));
        mixed.m_Count = 1;
        mixed.m_Values = &payload;
        type = DATA_TYPE_NULL;
        payload = 0;
        ASSERT_EQ(DATA_RESULT_OK, DataCreateRowsSoA(store, 1, 0, 2, 1, &column, 0));
        ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    }
}

// Root aliases must not hide inline members during registration or blob loading.
TEST(Data, DuplicateFullFieldNamesAreRejected)
{
    HDataStore     store = DataCreateStore();
    uint64_t       color = dmHashString64("light.color");
    DataFieldDesc  member = { .m_Field = dmHashString64("color"), .m_Type = DATA_TYPE_NUMBER, .m_Name = "color" };
    DataStructDesc light = { .m_Fields = &member, .m_FieldCount = 1, .m_Size = sizeof(double) };
    DataFieldDesc  fields[] = {
        { .m_Field = dmHashString64("light"), .m_Type = DATA_TYPE_STRUCT, .m_Struct = &light, .m_Name = "light" },
        { .m_Field = color, .m_Type = DATA_TYPE_NUMBER, .m_Offset = sizeof(double) }
    };
    DataTableDesc  desc = { .m_Type = 1, .m_Fields = fields, .m_FieldCount = 2, .m_RowStride = 2 * sizeof(double) };
    DataQueryField field = { .m_Field = color, .m_Type = DATA_TYPE_NUMBER };
    DataQueryDesc  query_desc = { .m_Fields = &field, .m_FieldCount = 1 };
    HDataQuery     query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &query_desc, &query));
    uint64_t revision = store->m_Revision;
    ASSERT_EQ(DATA_RESULT_ALREADY_EXISTS, DataRegisterTable(store, &desc));
    ASSERT_EQ(revision, store->m_Revision);
    ASSERT_TRUE(store->m_Tables.Empty());
    ASSERT_TRUE(query->m_Tables.Empty());

    fields[1].m_Field = dmHashString64("intensity");
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &desc));
    double values[] = { 3, 7 };
    DataId id;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateRows(store, 1, 0, 1, values, &id));
    double value;
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, id, color, &value));
    ASSERT_EQ(3.0, value);

    uint8_t  DM_ALIGNED(8) bytes[512];
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, bytes, sizeof(bytes), &size));
    HDataBlob blob;
    ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
    DataDestroyBlob(blob);
    uint32_t table_offset = (uint32_t)ReadDataInteger(bytes + DATA_TABLE_OFFSETS_OFFSET, 4);
    uint8_t* root = bytes + table_offset + DATA_TABLE_HEADER_SIZE + DATA_FIELD_META_SIZE;
    WriteDataInteger(root + offsetof(DataFileFieldMeta, m_NameHash), color, 8);
    ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(bytes, size, &blob));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(query));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

// Native AoS/SoA creation and nested setters must share the blob reader's depth budget.
TEST(Data, InlineAndDynamicNestingShareOneLimit)
{
    DataListInput lists[DATA_MAX_NESTING];
    DataReference references[DATA_MAX_NESTING] = {};
    double        number = 7;
    for (uint32_t i = 0; i < DATA_MAX_NESTING; ++i)
    {
        lists[i] = { .m_Type = i ? DATA_TYPE_LIST : DATA_TYPE_NUMBER, .m_Count = 1, .m_Values = i ? (const void*)&references[i - 1] : &number };
        references[i].m_List = &lists[i];
    }
    DataFieldDesc  member = { .m_Field = dmHashString64("items"), .m_Type = DATA_TYPE_LIST, .m_Name = "items" };
    DataStructDesc layout = { .m_Fields = &member, .m_FieldCount = 1, .m_Size = sizeof(DataReference) };
    DataFieldDesc  root = { .m_Field = dmHashString64("state"), .m_Type = DATA_TYPE_STRUCT, .m_Struct = &layout, .m_Name = "state" };
    DataTableDesc  desc = { .m_Type = 1, .m_Fields = &root, .m_FieldCount = 1, .m_RowStride = sizeof(DataReference) };
    for (uint32_t soa = 0; soa < 2; ++soa)
    {
        HDataStore store = DataCreateStore();
        ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &desc));
        DataReference  rows[] = { references[DATA_MAX_NESTING - 2], references[DATA_MAX_NESTING - 1] };
        DataFieldArray column = { .m_Field = root.m_Field, .m_Values = rows };
        DataId         ids[] = { 123, 456 };
        DataResult     result = soa ? DataCreateRowsSoA(store, 1, 0, 2, 1, &column, ids) : DataCreateRows(store, 1, 0, 2, rows, ids);
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, result);
        ASSERT_EQ((DataId)123, ids[0]);
        ASSERT_EQ((DataId)456, ids[1]);
        ASSERT_TRUE(FindTable(store, 1)->m_Rows.Empty());
        result = soa ? DataCreateRowsSoA(store, 1, 0, 1, 1, &column, ids) : DataCreateRows(store, 1, 0, 1, rows, ids);
        ASSERT_EQ(DATA_RESULT_OK, result);

        DataValue nested[DATA_MAX_NESTING + 1];
        nested[0] = Number(number);
        for (uint32_t i = 1; i <= DATA_MAX_NESTING; ++i)
            nested[i] = List(&nested[i - 1], 1);
        uint64_t field = dmHashString64("state.items");
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetField(store, ids[0], field, &nested[DATA_MAX_NESTING]));
        ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, ids[0], field, &nested[DATA_MAX_NESTING - 1]));
        uint8_t  DM_ALIGNED(8) bytes[4096];
        uint32_t size;
        ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, bytes, sizeof(bytes), &size));
        HDataBlob blob;
        ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
        DataDestroyBlob(blob);
        ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    }
}

// Repeated waves reuse default storage, and swap removal preserves surviving reset values.
TEST(Data, NativeDefaultsRemainDenseAcrossWaves)
{
    HDataStore    store = DataCreateStore();
    DataFieldDesc field = { .m_Field = 1, .m_Type = DATA_TYPE_NUMBER };
    DataTableDesc desc = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(double) };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &desc));
    DataTable* table = FindTable(store, 1);
    double     permanent_value = 99;
    DataId     permanent;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateRows(store, 1, 0, 1, &permanent_value, &permanent));
    double   values[128];
    DataId   ids[128];
    uint32_t capacity = 0;
    for (uint32_t wave = 0; wave < 32; ++wave)
    {
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(values); ++i)
            values[i] = wave * 128 + i;
        ASSERT_EQ(DATA_RESULT_OK, DataCreateRows(store, 1, wave, 128, values, ids));
        if (!wave)
            capacity = table->m_Owned->m_BaseRows.Capacity();
        ASSERT_EQ(capacity, table->m_Owned->m_BaseRows.Capacity());
        for (uint32_t i = 0; i < 128; ++i)
        {
            ASSERT_EQ(DATA_RESULT_OK, DataSetFieldNumber(store, ids[i], 1, -1));
            ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, ids[i]));
            double value;
            ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, ids[i], 1, &value));
            ASSERT_EQ(values[i], value);
            ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[i]));
            ASSERT_EQ(table->m_Rows.Size() * sizeof(double), table->m_Owned->m_BaseRows.Size());
            for (uint32_t row = 0; row < table->m_Rows.Size(); ++row)
                ASSERT_EQ(row, table->m_Rows[row].m_BaseIndex);
        }
        ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, permanent));
        double value;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, permanent, 1, &value));
        ASSERT_EQ(permanent_value, value);
    }
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, permanent));
    ASSERT_TRUE(table->m_Owned->m_BaseRows.Empty());
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

// Emptying a native table releases string/list arenas while keeping child slots reusable.
TEST(Data, EmptyNativeTableReleasesPayloadArenas)
{
    HDataStore    store = DataCreateStore();
    DataFieldDesc root = { .m_Field = 1, .m_Type = DATA_TYPE_STRUCT };
    DataTableDesc table_desc = { .m_Type = 1, .m_Fields = &root, .m_FieldCount = 1, .m_RowStride = sizeof(DataReference) };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table_desc));
    DataFieldDesc fields[] = {
        { .m_Field = 10, .m_Type = DATA_TYPE_STRING },
        { .m_Field = 20, .m_Type = DATA_TYPE_LIST, .m_Offset = sizeof(DataReference) }
    };
    DataStructDesc  layout = { .m_Fields = fields, .m_FieldCount = 2, .m_Size = 2 * sizeof(DataReference) };
    double          numbers[] = { 3, 7 };
    DataListInput   list = { .m_Type = DATA_TYPE_NUMBER, .m_Count = 2, .m_Values = numbers };
    DataReference   members[] = { { .m_String = "original" }, { .m_List = &list } };
    DataStructInput input = { .m_Layout = &layout, .m_Values = members };
    DataReference   rows[] = { { .m_Struct = &input }, { .m_Struct = &input } };
    DataTable*      table = FindTable(store, 1);
    for (uint32_t wave = 0; wave < 4; ++wave)
    {
        DataId ids[2];
        ASSERT_EQ(DATA_RESULT_OK, DataCreateRows(store, 1, 0, 2, rows, ids));
        DataValue value;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[1], 1, &value));
        ASSERT_EQ(DATA_RESULT_OK, DataSetField(store, ids[1], 1, &value));
        ASSERT_TRUE(table->m_Payloads != 0);
        ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[0]));
        ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, ids[1]));
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, ids[1], 1, &value));
        DataValue child;
        ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&value.m_Value.m_Struct, 10, &child));
        ASSERT_STREQ("original", child.m_Value.m_String);
        ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[1]));
        ASSERT_TRUE(table->m_Owned->m_BaseRows.Empty());
        ASSERT_EQ((DataBlock*)0, table->m_Owned->m_BaseValues);
        ASSERT_EQ((DataBlock*)0, table->m_Payloads);
        ASSERT_EQ(0u, table->m_Structs->m_Tables[0]->m_RowCount);
    }
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}
