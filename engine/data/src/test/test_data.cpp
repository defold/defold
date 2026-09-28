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
#include <string.h>
#include <dlib/hash.h>
#include <dlib/align.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../data.h"

extern "C" int TestDataFromC(HDataStore store);

// Keep existing boundary/value cases concise while exercising only the new public
// typed cursor API. Indexed generic operations below remain internal test coverage.
static DataResult SelectTestField(const DataIterator* batch, uint32_t row_index, uint32_t field_index, DataRowIterator* rows, DataFieldIterator* out_field)
{
    *rows = DataIterRows(batch);
    for (uint32_t r = 0;; ++r)
    {
        DataResult result = DataRowIterNext(rows);
        if (result != DATA_RESULT_OK)
            return result == DATA_RESULT_END ? DATA_RESULT_INVALID_ARGUMENT : result;
        if (r == row_index)
            break;
    }
    *out_field = DataRowIterFields(rows);
    for (uint32_t f = 0;; ++f)
    {
        DataResult result = DataFieldIterNext(out_field);
        if (result != DATA_RESULT_OK)
            return result == DATA_RESULT_END ? DATA_RESULT_INVALID_ARGUMENT : result;
        if (f == field_index)
            return DATA_RESULT_OK;
    }
}

static DataResult GetTestFieldNumber(const DataIterator* batch, uint32_t row, uint32_t field, double* value)
{
    DataRowIterator   rows;
    DataFieldIterator iterator;
    DataResult        result = SelectTestField(batch, row, field, &rows, &iterator);
    return result == DATA_RESULT_OK ? DataFieldIterGetNumber(&iterator, value) : result;
}

static DataResult SetTestFieldNumber(const DataIterator* batch, uint32_t row, uint32_t field, double value)
{
    DataRowIterator   rows;
    DataFieldIterator iterator;
    DataResult        result = SelectTestField(batch, row, field, &rows, &iterator);
    return result == DATA_RESULT_OK ? DataFieldIterSetNumber(&iterator, value) : result;
}

static DataResult GetTestFieldBoolean(const DataIterator* batch, uint32_t row, uint32_t field, uint8_t* value)
{
    DataRowIterator   rows;
    DataFieldIterator iterator;
    DataResult        result = SelectTestField(batch, row, field, &rows, &iterator);
    return result == DATA_RESULT_OK ? DataFieldIterGetBoolean(&iterator, value) : result;
}

static DataResult SetTestFieldBoolean(const DataIterator* batch, uint32_t row, uint32_t field, uint8_t value)
{
    DataRowIterator   rows;
    DataFieldIterator iterator;
    DataResult        result = SelectTestField(batch, row, field, &rows, &iterator);
    return result == DATA_RESULT_OK ? DataFieldIterSetBoolean(&iterator, value) : result;
}

static DataResult GetTestFieldString(const DataIterator* batch, uint32_t row, uint32_t field, const char** value)
{
    DataRowIterator   rows;
    DataFieldIterator iterator;
    DataResult        result = SelectTestField(batch, row, field, &rows, &iterator);
    return result == DATA_RESULT_OK ? DataFieldIterGetString(&iterator, value) : result;
}

static DataResult SetTestFieldString(const DataIterator* batch, uint32_t row, uint32_t field, const char* value)
{
    DataRowIterator   rows;
    DataFieldIterator iterator;
    DataResult        result = SelectTestField(batch, row, field, &rows, &iterator);
    return result == DATA_RESULT_OK ? DataFieldIterSetString(&iterator, value) : result;
}

static DataResult GetTestFieldVector3(const DataIterator* batch, uint32_t row, uint32_t field, DataVector3* value)
{
    DataRowIterator   rows;
    DataFieldIterator iterator;
    DataResult        result = SelectTestField(batch, row, field, &rows, &iterator);
    return result == DATA_RESULT_OK ? DataFieldIterGetVector3(&iterator, value) : result;
}

static DataResult SetTestFieldVector3(const DataIterator* batch, uint32_t row, uint32_t field, const DataVector3* value)
{
    DataRowIterator   rows;
    DataFieldIterator iterator;
    DataResult        result = SelectTestField(batch, row, field, &rows, &iterator);
    return result == DATA_RESULT_OK ? DataFieldIterSetVector3(&iterator, value) : result;
}

static DataResult GetTestStructPropertyVector3(const DataIterator* batch, uint32_t row, uint32_t field, uint64_t property, DataVector3* value)
{
    DataRowIterator   rows;
    DataFieldIterator iterator;
    DataResult        result = SelectTestField(batch, row, field, &rows, &iterator);
    return result == DATA_RESULT_OK ? DataFieldIterGetStructPropertyVector3(&iterator, property, value) : result;
}

static DataValue Number(double v)
{
    DataValue x = {};
    x.m_Type = DATA_VALUE_TYPE_NUMBER;
    x.m_Value.m_Number = v;
    return x;
}

static DataValue String(const char* v)
{
    DataValue x = {};
    x.m_Type = DATA_VALUE_TYPE_STRING;
    x.m_Value.m_String = v;
    return x;
}

static DataValue Boolean(uint8_t v)
{
    DataValue x = {};
    x.m_Type = DATA_VALUE_TYPE_BOOLEAN;
    x.m_Value.m_Boolean = v;
    return x;
}

static DataValue Null()
{
    DataValue x = {};
    x.m_Type = DATA_VALUE_TYPE_NULL;
    return x;
}

static DataValue List(const DataValue* values, uint32_t count)
{
    DataValue x = {};
    x.m_Type = DATA_VALUE_TYPE_LIST;
    DataList list = { values, count, 0, 0 };
    x.m_Value.m_List = list;
    return x;
}

static DataValue Struct(const uint64_t* names, const DataValue* values, uint32_t count)
{
    DataValue x = {};
    x.m_Type = DATA_VALUE_TYPE_STRUCT;
    DataStruct object = { names, values, count, 0, 0 };
    x.m_Value.m_Struct = object;
    return x;
}

static DataValue Vector3(float x, float y, float z)
{
    DataValue v = {};
    v.m_Type = DATA_VALUE_TYPE_VECTOR3;
    v.m_Value.m_Vector3[0] = x;
    v.m_Value.m_Vector3[1] = y;
    v.m_Value.m_Vector3[2] = z;
    return v;
}

static DataValue Matrix()
{
    DataValue v = {};
    v.m_Type = DATA_VALUE_TYPE_MATRIX4;
    for (uint32_t i = 0; i < 16; ++i)
        v.m_Value.m_Matrix4[i] = (float)i + 0.5f;
    return v;
}

static DataResult Register(HDataStore store, uint64_t type, const uint64_t* tags = 0, uint32_t tag_count = 0)
{
    DataPropertyDesc meta[] = { { 10, DATA_VALUE_TYPE_NUMBER, 0 }, { 20, DATA_VALUE_TYPE_STRING, 8 } };
    DataTableDesc    desc = { type, tags, tag_count, meta, 2, 16 };
    return DataRegisterTable(store, &desc);
}

static DataResult Add(HDataStore store, uint64_t type, DataOwnerId owner, double number, const char* string, DataId* id)
{
    DataValue values[] = { Number(number), String(string) };
    return DataAddRow(store, type, owner, values, 2, id);
}

static DataResult Load(HDataStore store, const uint8_t* bytes, uint32_t size, HDataBlobInstance* instance, DataOwnerId owner = 0)
{
    HDataBlob  blob;
    DataResult result = DataLoadBlob(bytes, size, &blob);
    if (result != DATA_RESULT_OK)
        return result;
    result = DataAddBlob(store, blob, owner, instance);
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
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, id, 20, &out));
    ASSERT_STREQ("copied", out.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, id, 20, &out));
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, id, 20, &out));
    ASSERT_STREQ("copied", out.m_Value.m_String);
    DataValue replacement = String(text);
    ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, id, 20, &replacement));
    text[1] = 'Y';
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, id, 20, &out));
    ASSERT_STREQ("Xopied", out.m_Value.m_String);
    replacement = String(0);
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetProperty(store, id, 20, &replacement));
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
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataGetProperty(store, 0, 10, &out));
    for (uint32_t i = 0; i < 257; i += 2)
    {
        ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[i]));
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataRemoveRow(store, ids[i]));
    }
    for (uint32_t i = 1; i < 257; i += 2)
    {
        ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, ids[i], 10, &out));
        ASSERT_EQ((double)i, out.m_Value.m_Number);
        ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, ids[i], 20, &out));
        ASSERT_STREQ("value", out.m_Value.m_String);
    }
    for (uint32_t i = 0; i < 257; i += 2)
    {
        DataId replacement;
        ASSERT_EQ(DATA_RESULT_OK, Add(store, 1, i, -1, "new", &replacement));
        ASSERT_NE(ids[i], replacement);
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataGetProperty(store, ids[i], 10, &out));
    }
    ASSERT_EQ(DATA_RESULT_OK, DataUnregisterTable(store, 1));
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 1));
    DataId replacement;
    ASSERT_EQ(DATA_RESULT_OK, Add(store, 1, 0, 42, "reregistered", &replacement));
    for (uint32_t i = 0; i < 257; ++i)
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataGetProperty(store, ids[i], 10, &out));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, LiveQueriesTagsOwnersAndCopiedDescriptors)
{
    HDataStore    store = DataCreateStore();
    uint64_t      tags[] = { 100, 200 };
    DataOwnerId   owners[] = { 7, 7, 8 };
    DataQueryDesc desc = { owners, 3, tags, 2, 0, 0 };
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
    owners[0] = owners[1] = owners[2] = 999;
    DataId id;
    for (uint64_t type = 1; type <= 3; ++type)
    {
        const DataOwnerId row_owners[] = { 7, 7, 0, 8, 0, 7 };
        for (uint32_t row = 0; row < 6; ++row)
            ASSERT_EQ(DATA_RESULT_OK, Add(store, type, row_owners[row], row, "test", &id));
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
            DataOwnerId owner = DataIterGetOwnerId(&it, row);
            ASSERT_TRUE(owner == 7 || owner == 8);
            DataValue out;
            ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, DataIterGetId(&it, row), 10, &out));
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
    DataQueryDesc desc = {};
    HDataQuery    query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    DataStoreLock(store);
    DataStoreLock(store);
    DataIterator batch = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&batch));
    DataRowIterator rows = DataIterRows(&batch);
    ASSERT_EQ(DATA_RESULT_OK, DataRowIterNext(&rows));
    DataFieldIterator field = DataRowIterFields(&rows);
    ASSERT_EQ(DATA_RESULT_OK, DataFieldIterNext(&field));
    DataId            unchanged = 123;
    HDataBlobInstance unchanged_instance = instance;
    ASSERT_EQ(DATA_RESULT_LOCKED, Register(store, 2));
    ASSERT_EQ(DATA_RESULT_LOCKED, Add(store, 1, 3, 2, "two", &unchanged));
    DataValue   values[] = { Number(2), String("two") };
    DataRowDesc add = { 3, values, 2, 0 };
    ASSERT_EQ(DATA_RESULT_LOCKED, DataAddRows(store, 1, &add, 1, &unchanged));
    ASSERT_EQ((DataId)123, unchanged);
    ASSERT_EQ(DATA_RESULT_LOCKED, DataRemoveRow(store, id));
    ASSERT_EQ(DATA_RESULT_LOCKED, DataUnregisterTable(store, 1));
    ASSERT_EQ(DATA_RESULT_LOCKED, DataAddBlob(store, blob, 5, &unchanged_instance));
    ASSERT_EQ((uintptr_t)instance, (uintptr_t)unchanged_instance);
    ASSERT_EQ(DATA_RESULT_LOCKED, DataRemoveBlob(instance));
    ASSERT_EQ(DATA_RESULT_LOCKED, DataDestroyStore(store));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldIterSetNumber(&field, 9));
    double value = 0;
    ASSERT_EQ(DATA_RESULT_OK, DataFieldIterGetNumber(&field, &value));
    ASSERT_EQ(9.0, value);
    ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, id));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldIterGetNumber(&field, &value));
    ASSERT_EQ(1.0, value);
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_LOCKED, DataRemoveRow(store, id));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldIterGetNumber(&field, &value));
    ASSERT_EQ(1.0, value);
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
    DataPropertyDesc meta[] = { { 10, DATA_VALUE_TYPE_NUMBER, 8 }, { 20, DATA_VALUE_TYPE_STRING, 24 } };
    DataTableDesc    table = { 2, 0, 0, meta, 2, 32 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    DataId ids[2][4];
    for (uint32_t t = 0; t < 2; ++t)
        for (uint32_t r = 0; r < 4; ++r)
            ASSERT_EQ(DATA_RESULT_OK, Add(store, t + 1, r == 1 ? 8 : 7, t * 10 + r, "base", &ids[t][r]));

    DataOwnerId       owner = 7;
    DataQueryProperty fields[] = { { 10, DATA_VALUE_TYPE_NUMBER }, { 20, DATA_VALUE_TYPE_STRING } };
    DataQueryDesc     desc = { &owner, 1, 0, 0, fields, 2 };
    HDataQuery        query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    // A second query and iterator must not replace the first iterator's batch state.
    DataQueryProperty other_fields[] = { fields[1], fields[0] };
    desc.m_Properties = other_fields;
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
            ASSERT_EQ(owner, DataIterGetOwnerId(&it, row));
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

TEST(Data, BoundPropertyQueries)
{
    HDataStore        store = DataCreateStore();
    uint64_t          tag = 8;
    DataOwnerId       owner = 7;
    DataQueryProperty fields[] = { { 20, DATA_VALUE_TYPE_STRING }, { 10, DATA_VALUE_TYPE_NUMBER } };
    DataQueryDesc     desc = { &owner, 1, &tag, 1, fields, 2 };
    HDataQuery        query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    fields[0].m_Property = 999; // Query owns the original filter/binding order.
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 1, &tag, 1));
    DataPropertyDesc swapped[] = { { 20, DATA_VALUE_TYPE_STRING, 0 }, { 10, DATA_VALUE_TYPE_NUMBER, 8 } };
    DataTableDesc    table = { 2, &tag, 1, swapped, 2, 16 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    DataPropertyDesc wrong[] = { { 10, DATA_VALUE_TYPE_STRING, 0 }, { 20, DATA_VALUE_TYPE_STRING, 8 } };
    table.m_Type = 3;
    table.m_Properties = wrong;
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    table.m_Type = 4;
    table.m_Properties = swapped;
    table.m_PropertyCount = 1;
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table)); // Missing number.
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 5));               // Missing tag.
    DataId ids[6];
    ASSERT_EQ(DATA_RESULT_OK, Add(store, 1, 7, 1, "one", &ids[0]));
    ASSERT_EQ(DATA_RESULT_OK, Add(store, 1, 8, 99, "other owner", &ids[1]));
    DataValue values[] = { String("two"), Number(2) };
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(store, 2, 7, values, 2, &ids[2]));
    values[1] = String("wrong kind");
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(store, 3, 7, values, 2, &ids[3]));
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(store, 4, 7, values, 1, &ids[4]));
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
        ASSERT_EQ(DATA_RESULT_OK, DataIterGetProperty(&it, 0, 10, &out));
        ASSERT_EQ(44.0, out.m_Value.m_Number);
        ASSERT_EQ(DATA_RESULT_OK, DataResetProperty(store, DataIterGetId(&it, 0), 10));
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
    desc.m_Properties = 0;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataCreateQuery(store, &desc, &query));
    desc.m_Properties = fields;
    fields[0].m_Type = (DataValueType)99;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataCreateQuery(store, &desc, &query));
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, BoundPackedProperties)
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
    HDataStore        store = DataCreateStore();
    DataQueryProperty fields[] = { { 10, DATA_VALUE_TYPE_NUMBER }, { 20, DATA_VALUE_TYPE_STRING } };
    DataQueryDesc     desc = { 0, 0, 0, 0, fields, 2 };
    HDataQuery        query;
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
    HDataStore       store = DataCreateStore();
    DataPropertyDesc meta[] = { { 10, DATA_VALUE_TYPE_VECTOR3, 0 }, { 20, DATA_VALUE_TYPE_NUMBER, 16 }, { 30, DATA_VALUE_TYPE_NUMBER, 24 } };
    DataTableDesc    desc = { 1, 0, 0, meta, 3, 32 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &desc));
    meta[0].m_Property = 999; // The construction descriptor is copied once.
    DataValue values[] = { Vector3(1, 2, 3), Number(4), Number(10) };
    DataId    ids[3];
    for (uint32_t i = 0; i < 3; ++i)
        ASSERT_EQ(DATA_RESULT_OK, DataAddRow(store, 1, i, values, 3, &ids[i]));
    DataTable* table = store->m_Tables[0];
    ASSERT_EQ(3u, table->m_Owned->m_Properties.Size());
    ASSERT_EQ(96u, table->m_Owned->m_BaseRows.Size()); // Three 32-byte rows, with no per-row names or kind tags.
    ASSERT_EQ(0, memcmp(table->m_Owned->m_BaseRows.Begin(), table->m_Owned->m_BaseRows.Begin() + 32, 32));
    ASSERT_EQ((uintptr_t)0, (uintptr_t)table->m_Owned->m_BaseValues);
    ASSERT_EQ((uintptr_t)0, (uintptr_t)table->m_Payloads);
    DataValue out;
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, ids[1], 10, &out));
    ASSERT_EQ(2.0f, out.m_Value.m_Vector3[1]);
    DataValue changed = Vector3(0.25f, 0.5f, 0.75f);
    ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, ids[1], 10, &changed));
    ASSERT_EQ(96u, table->m_Values.Size());
    ASSERT_EQ(0, memcmp(table->m_Values.Begin(), table->m_Owned->m_BaseRows.Begin(), 32));
    ASSERT_NE(0, memcmp(table->m_Values.Begin() + 32, table->m_Owned->m_BaseRows.Begin() + 32, 32));
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, ids[1], 20, &out));
    ASSERT_EQ(4.0, out.m_Value.m_Number); // Updating color preserves the other mutable fields.
    changed = Number(3);
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetProperty(store, ids[1], 10, &changed));
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, ids[1], 10, &out));
    ASSERT_EQ(0.5f, out.m_Value.m_Vector3[1]);
    ASSERT_EQ(DATA_RESULT_OK, DataResetProperty(store, ids[1], 10));
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, ids[1], 10, &out));
    ASSERT_EQ(2.0f, out.m_Value.m_Vector3[1]);
    ASSERT_EQ(DATA_RESULT_OK, DataResetTable(store, 1));
    ASSERT_EQ((uintptr_t)0, (uintptr_t)table->m_Payloads);
    ASSERT_EQ(0, memcmp(table->m_Values.Begin(), table->m_Owned->m_BaseRows.Begin(), 96));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, MetadataAndBulkValidation)
{
    HDataStore       store = DataCreateStore();
    DataPropertyDesc meta[] = { { 1, DATA_VALUE_TYPE_NUMBER, 0 }, { 2, DATA_VALUE_TYPE_BOOLEAN, 8 } };
    DataTableDesc    desc = { 1, 0, 0, meta, 2, 16 };
    meta[1].m_Offset = 7;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &desc));
    meta[1].m_Offset = 16;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &desc));
    meta[1].m_Offset = 8;
    meta[1].m_Property = 1;
    ASSERT_EQ(DATA_RESULT_ALREADY_EXISTS, DataRegisterTable(store, &desc));
    meta[1].m_Property = 2;
    meta[1].m_Type = (DataValueType)99;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &desc));
    meta[1].m_Type = DATA_VALUE_TYPE_BOOLEAN;
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &desc));
    ASSERT_EQ(DATA_RESULT_ALREADY_EXISTS, DataRegisterTable(store, &desc));
    DataValue   values[] = { Number(4), Boolean(1), Number(5), Boolean(2) };
    DataRowDesc rows[] = { { 1, values, 2, 7 }, { 2, values + 2, 2, 8 } };
    DataId      ids[] = { 123, 456 };
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataAddRows(store, 1, rows, 2, ids));
    ASSERT_EQ((DataId)123, ids[0]);
    ASSERT_EQ((DataId)456, ids[1]);
    ASSERT_EQ(0u, store->m_Tables[0]->m_Rows.Size());
    values[3] = Boolean(0);
    rows[1].m_ValueCount = 1;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataAddRows(store, 1, rows, 2, ids));
    rows[1].m_ValueCount = 2;
    values[2] = String("wrong kind");
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataAddRows(store, 1, rows, 2, ids));
    values[2] = Number(5);
    ASSERT_EQ(DATA_RESULT_OK, DataAddRows(store, 1, rows, 2, ids));
    ASSERT_EQ(32u, store->m_Tables[0]->m_Owned->m_BaseRows.Size());
    values[0] = Number(99);
    DataValue out = Number(123);
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, ids[0], 1, &out));
    ASSERT_EQ(4.0, out.m_Value.m_Number);
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataGetProperty(store, ids[0], 999, &out));
    ASSERT_EQ(4.0, out.m_Value.m_Number);
    ASSERT_EQ(DATA_RESULT_OK, DataAddRows(store, 1, 0, 0, 0));
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataAddRows(store, 1, 0, 1, ids));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, CApiMathAndNestedViews)
{
    HDataStore       store = DataCreateStore();
    DataPropertyDesc meta[] = { { 42, DATA_VALUE_TYPE_NUMBER, 0 }, { 43, DATA_VALUE_TYPE_VECTOR3, 8 }, { 44, DATA_VALUE_TYPE_VECTOR4, 20 }, { 45, DATA_VALUE_TYPE_MATRIX4, 36 }, { 46, DATA_VALUE_TYPE_STRUCT, 104 }, { 47, DATA_VALUE_TYPE_BOOLEAN, 112 }, { 48, DATA_VALUE_TYPE_STRING, 120 } };
    uint64_t         tag = 7;
    DataTableDesc    desc = { 1, &tag, 1, meta, 7, 128 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &desc));
    DataValue vec4 = {};
    vec4.m_Type = DATA_VALUE_TYPE_VECTOR4;
    uint64_t  color_name = 10;
    DataValue color = Vector3(1.0f, 0.5f, 0.25f);
    DataValue values[] = { Number(17), Vector3(1, 1, 1), vec4, Matrix(), Struct(&color_name, &color, 1), Boolean(0), String("initial") };
    DataId    id;
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(store, 1, 123, values, 7, &id));
    ASSERT_EQ(0, TestDataFromC(store));
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, bytes, size, &size));
    HDataStore        loaded = DataCreateStore();
    HDataBlobInstance instance;
    ASSERT_EQ(DATA_RESULT_OK, Load(loaded, bytes, size, &instance, 123));
    DataMatrix4 loaded_matrix;
    ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyMatrix4(loaded, FirstId(loaded), 45, &loaded_matrix));
    ASSERT_EQ(44.0f, loaded_matrix.m_Values[3 * 4 + 2]);
    DataValue out;
    for (uint32_t i = 1; i <= 3; ++i)
    {
        DataValue original;
        ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, id, meta[i].m_Property, &original));
        ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(loaded, FirstId(loaded), meta[i].m_Property, &out));
        ASSERT_EQ(original.m_Type, out.m_Type);
        ASSERT_EQ(0, memcmp(&original.m_Value, &out.m_Value, DataTypeSize(original.m_Type)));
    }
    ASSERT_EQ(DATA_RESULT_OK, DataResetTable(store, 1));
    for (uint32_t i = 1; i <= 3; ++i)
    {
        ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, id, meta[i].m_Property, &out));
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
    const uint32_t   bits[] = { 0, 0x80000000, 1, 0x7f800000, 0xff800000, 0x7fc01234, 0x3f800000, 0xbf800000, 0, 0x80000000, 1, 0x7f800000, 0xff800000, 0x7fc01234, 0x3f800000, 0xbf800000 };
    DataPropertyDesc meta[] = { { 10, DATA_VALUE_TYPE_VECTOR3, 0 }, { 20, DATA_VALUE_TYPE_VECTOR4, 12 }, { 30, DATA_VALUE_TYPE_MATRIX4, 28 } };
    DataTableDesc    desc = { 1, 0, 0, meta, 3, 92 };
    DataValue        values[3] = {};
    for (uint32_t i = 0; i < 3; ++i)
    {
        values[i].m_Type = meta[i].m_Type;
        memcpy(&values[i].m_Value, bits, DataTypeSize(meta[i].m_Type));
    }
    HDataStore source = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &desc));
    DataId id;
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(source, 1, 0, values, 3, &id));
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
                    ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, row, meta[i].m_Property, &values[i]));
                DataValue value;
                ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, row, meta[i].m_Property, &value));
                ASSERT_EQ(meta[i].m_Type, value.m_Type);
                ASSERT_EQ(0, memcmp(bits, &value.m_Value, DataTypeSize(value.m_Type)));
            }
            DataVector3 vector3;
            DataVector4 vector4;
            DataMatrix4 matrix;
            ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyVector3(store, row, 10, &vector3));
            ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyVector4(store, row, 20, &vector4));
            ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyMatrix4(store, row, 30, &matrix));
            ASSERT_EQ(0, memcmp(bits, vector3.m_Values, sizeof(vector3.m_Values)));
            ASSERT_EQ(0, memcmp(bits, vector4.m_Values, sizeof(vector4.m_Values)));
            ASSERT_EQ(0, memcmp(bits, matrix.m_Values, sizeof(matrix.m_Values)));
            DataQueryProperty fields[] = { { 30, DATA_VALUE_TYPE_MATRIX4 }, { 10, DATA_VALUE_TYPE_VECTOR3 }, { 20, DATA_VALUE_TYPE_VECTOR4 } };
            DataQueryDesc     query_desc = { 0, 0, 0, 0, fields, 3 };
            HDataQuery        query;
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
            ASSERT_EQ(0, memcmp(bits, DataFieldGetMatrix4(&rows, matrix_field), sizeof(DataMatrix4)));
            ASSERT_EQ(0, memcmp(bits, DataFieldGetVector3(&rows, vector3_field), sizeof(DataVector3)));
            ASSERT_EQ(0, memcmp(bits, DataFieldGetVector4(&rows, vector4_field), sizeof(DataVector4)));
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
        'D','M','D','T',8,0,0,0, 1,0,0,0,120,0,0,0, 24,0,0,0,0,0,0,0,
        1,0,0,0,0,0,0,0, 0,0,0,0,1,0,0,0, 2,0,0,0,8,0,0,0, 1,0,0,0,0,0,0,0,
        10,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 8,0,0,0,0,0,0,0,
        7,0,0,0,0,0,0,0, 8,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0xf8,0x3f, 0,0,0,0,0,0,0,0xc0
    };
    // clang-format on
    HDataStore       store = DataCreateStore();
    DataPropertyDesc meta = { 10, DATA_VALUE_TYPE_NUMBER, 0 };
    DataTableDesc    desc = { 1, 0, 0, &meta, 1, 8 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &desc));
    DataValue   values[] = { Number(1.5), Number(-2) };
    DataId      ids[2];
    DataRowDesc rows[] = { { 100, values, 1, 7 }, { 200, values + 1, 1, 8 } };
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
    ASSERT_EQ((uintptr_t)(expected + 104), (uintptr_t)(GetInstanceTable(instance, 0)->m_Blob + GetInstanceTable(instance, 0)->m_Offsets.m_Rows));
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
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(source, 1, 999, values, 2, &id, 11));
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(source, 1, 999, values, 2, &id, 12));
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(source, 2, 999, values, 2, &id, 13));
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
            uint32_t owner = DataIterGetOwnerId(&it, r) == 100 ? 0 : 1;
            ASSERT_TRUE(component >= 11 && component <= 13);
            ids[owner][component - 11] = row;
            DataValue out;
            ASSERT_EQ(DATA_RESULT_OK, DataIterGetProperty(&it, r, 20, &out));
            ASSERT_EQ(string, (uintptr_t)out.m_Value.m_String);
            ++total;
        }
    }
    ASSERT_EQ(6u, total);
    ASSERT_NE(ids[0][0], ids[1][0]);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(source));
    DataDestroyBlob(blob);
    DataValue change = String("override");
    ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, ids[0][0], 20, &change));
    ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, ids[0][1], 20, &change));
    ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, ids[0][2], 20, &change));
    ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, ids[1][0], 20, &change));
    DataValue out;
    it = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
    ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, ids[0][0]));
    ASSERT_EQ(DATA_RESULT_OK, DataIterGetProperty(&it, 0, 20, &out));
    ASSERT_EQ(string, (uintptr_t)out.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, ids[0][1], 20, &out));
    ASSERT_STREQ("override", out.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, ids[0][2], 20, &out));
    ASSERT_STREQ("override", out.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, ids[1][0], 20, &out));
    ASSERT_STREQ("override", out.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, ids[0][0], 20, &change));
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[0][1]));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataResetRow(store, ids[0][1]));
    DataStoreLock(store);
    it = DataQueryIter(query);
    DataResetBlob(first);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataGetProperty(store, ids[0][1], 20, &out));
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, ids[0][0], 20, &out));
    ASSERT_EQ(string, (uintptr_t)out.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, ids[0][2], 20, &out));
    ASSERT_EQ(string, (uintptr_t)out.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, ids[1][0], 20, &out));
    ASSERT_STREQ("override", out.m_Value.m_String);
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveBlob(first));
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataGetProperty(store, ids[0][i], 20, &out));
        ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, ids[1][i], 20, &out));
    }
    ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, ids[1][0], 20, &out));
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, ids[1][0], 20, &out));
    ASSERT_NE(string, (uintptr_t)out.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataResetProperty(store, ids[1][0], 20));
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, ids[1][0], 20, &out));
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
    vec4.m_Type = DATA_VALUE_TYPE_VECTOR4;
    vec4.m_Value.m_Vector4[3] = 8.0f;
    DataValue        elements[] = { Null(), Number(1.5), Boolean(1), String("shared"), Vector3(1, 2, 3), vec4, Matrix(), List(0, 0), Struct(0, 0, 0) };
    DataValue        fields[] = { List(elements, 9), String("shared") };
    uint64_t         names[] = { 10, 20 };
    DataValue        value = Struct(names, fields, 2);
    DataPropertyDesc meta = { 1, DATA_VALUE_TYPE_STRUCT, 0 };
    DataTableDesc    desc = { 1, 0, 0, &meta, 1, 8 };
    HDataStore       source = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &desc));
    DataId id;
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(source, 1, 0, &value, 1, &id));
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
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, id, 1, &object));
    ASSERT_EQ((uintptr_t)(bytes), (uintptr_t)object.m_Value.m_Struct.m_Buffer);
    ASSERT_EQ((uintptr_t)0, (uintptr_t)object.m_Value.m_Struct.m_Values);
    ASSERT_EQ(DATA_RESULT_OK, DataGetStructProperty(&object.m_Value.m_Struct, 10, &list));
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
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataGetStructProperty(&object.m_Value.m_Struct, 99, &child));
    ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(&list.m_Value.m_List, 3, &child));
    ASSERT_EQ(DATA_RESULT_OK, DataGetStructProperty(&object.m_Value.m_Struct, 20, &shared));
    ASSERT_EQ((uintptr_t)child.m_Value.m_String, (uintptr_t)shared.m_Value.m_String);
    ASSERT_TRUE((const uint8_t*)shared.m_Value.m_String >= bytes && (const uint8_t*)shared.m_Value.m_String < bytes + size);
    ASSERT_EQ((uintptr_t)(bytes), (uintptr_t)GetInstanceTable(instance, 0)->m_Blob); // Values remain borrowed.
    ASSERT_EQ((uintptr_t)0, (uintptr_t)instance->m_Payloads);
    ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, id, 1, &object)); // Copy only when explicitly setting.
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, id, 1, &child));
    ASSERT_TRUE(child.m_Value.m_Struct.m_Buffer != object.m_Value.m_Struct.m_Buffer);
    ASSERT_EQ((uintptr_t)0, (uintptr_t)child.m_Value.m_Struct.m_Values);
    ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, id, 1, &child)); // Owned view self-assignment.
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, id, 1, &child));
    ASSERT_EQ(DATA_RESULT_OK, DataGetStructProperty(&child.m_Value.m_Struct, 20, &shared));
    ASSERT_STREQ("shared", shared.m_Value.m_String);
    ASSERT_EQ(DATA_RESULT_OK, DataResetProperty(store, id, 1));
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, id, 1, &child));
    ASSERT_EQ(object.m_Value.m_Struct.m_Offset, child.m_Value.m_Struct.m_Offset);
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
    HDataStore       store = DataCreateStore();
    DataPropertyDesc meta = { 1, DATA_VALUE_TYPE_STRUCT, 0 };
    DataTableDesc    desc = { 1, 0, 0, &meta, 1, 8 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &desc));
    uint64_t    names[] = { 10, 20 };
    DataValue   fields[] = { Vector3(1, 2, 3), Number(4) };
    DataValue   value = Struct(names, fields, 2);
    DataRowDesc rows[128];
    DataId      ids[128];
    for (uint32_t i = 0; i < 128; ++i)
    {
        DataRowDesc row = { i, &value, 1, 0 };
        rows[i] = row;
    }
    ASSERT_EQ(DATA_RESULT_OK, DataAddRows(store, 1, rows, 128, ids));
    DataMemoryStats memory;
    GetDataMemoryStats(store, &memory);
    ASSERT_EQ((uint64_t)1, memory.m_BaseBlocks);
    ASSERT_EQ((uint64_t)(128 * 64), memory.m_BaseUsed);
    ASSERT_EQ(memory.m_BaseUsed, memory.m_BaseCapacity);

    DataValue borrowed;
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, ids[0], 1, &borrowed));
    ASSERT_TRUE(borrowed.m_Value.m_Struct.m_Buffer != 0);
    ASSERT_EQ((uintptr_t)0, (uintptr_t)borrowed.m_Value.m_Struct.m_Values);
    for (uint32_t i = 0; i < 128; ++i)
        rows[i].m_Values = &borrowed;
    // Growing row/arena storage must keep the source view valid until copying ends.
    ASSERT_EQ(DATA_RESULT_OK, DataAddRows(store, 1, rows, 128, ids));
    fields[1] = Number(99);
    ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, ids[0], 1, &value));
    ASSERT_EQ(DATA_RESULT_OK, DataResetTable(store, 1));
    for (uint32_t i = 0; i < 128; ++i)
    {
        DataValue object, child;
        ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, ids[i], 1, &object));
        ASSERT_EQ(DATA_RESULT_OK, DataGetStructProperty(&object.m_Value.m_Struct, 10, &child));
        ASSERT_EQ(3.0f, child.m_Value.m_Vector3[2]);
        ASSERT_EQ(DATA_RESULT_OK, DataGetStructProperty(&object.m_Value.m_Struct, 20, &child));
        ASSERT_EQ(4.0, child.m_Value.m_Number);
    }
    GetDataMemoryStats(store, &memory);
    ASSERT_EQ((uint64_t)2, memory.m_BaseBlocks);
    ASSERT_EQ((uint64_t)0, memory.m_PayloadBlocks);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, NestedValidationAndDepth)
{
    HDataStore       store = DataCreateStore();
    DataPropertyDesc meta = { 1, DATA_VALUE_TYPE_LIST, 0 };
    DataTableDesc    desc = { 1, 0, 0, &meta, 1, 8 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &desc));
    DataValue base = List(0, 0);
    DataId    id;
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(store, 1, 0, &base, 1, &id));
    DataValue invalid = List(0, 1);
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetProperty(store, id, 1, &invalid));
    invalid = List(&invalid, 1);
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetProperty(store, id, 1, &invalid));
    DataValue bad = Boolean(2);
    invalid = List(&bad, 1);
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetProperty(store, id, 1, &invalid));
    uint64_t  names[] = { 10, 10 };
    DataValue fields[] = { Null(), Null() };
    DataValue object = Struct(names, fields, 2);
    invalid = List(&object, 1);
    ASSERT_EQ(DATA_RESULT_ALREADY_EXISTS, DataSetProperty(store, id, 1, &invalid));
    DataValue values[DATA_MAX_NESTING + 2];
    values[0] = Number(1);
    for (uint32_t i = 1; i < DATA_MAX_NESTING + 2; ++i)
        values[i] = List(&values[i - 1], 1);
    ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, id, 1, &values[DATA_MAX_NESTING]));
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetProperty(store, id, 1, &values[DATA_MAX_NESTING + 1]));
    uint8_t  DM_ALIGNED(8) bytes[4096];
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, bytes, sizeof(bytes), &size));
    HDataBlob blob;
    ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
    DataDestroyBlob(blob);
    // Last number becomes an empty list, which would add a 65th container.
    uint32_t container = (uint32_t)ReadDataInteger(bytes + 96, 8);
    for (uint32_t i = 1; i < DATA_MAX_NESTING; ++i)
        container = (uint32_t)ReadDataInteger(bytes + container + 12, 4);
    WriteDataInteger(bytes + container + 8, DATA_VALUE_TYPE_LIST, 4);
    memset(bytes + size - 8, 0, 8);
    bytes[size - 8] = 8;
    ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(bytes, size, &blob));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, MalformedBlobDirectoryLayoutAndValues)
{
    HDataStore       source = DataCreateStore();
    DataPropertyDesc meta[] = { { 1, DATA_VALUE_TYPE_BOOLEAN, 0 }, { 2, DATA_VALUE_TYPE_STRING, 8 }, { 3, DATA_VALUE_TYPE_LIST, 16 } };
    DataTableDesc    desc = { 1, 0, 0, meta, 3, 24 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &desc));
    desc.m_Type = 2;
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &desc));
    DataValue item = Number(2);
    DataValue values[] = { Boolean(1), String("text"), List(&item, 1) };
    DataId    id;
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(source, 1, 0, values, 3, &id));
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(source, 2, 0, values, 3, &id));
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
    // First table starts at 24, metadata at 56, row bytes at 160.
    const uint32_t corrupt[] = { 0, 4, 8, 12, 16, 20, 32, 36, 40, 44, 48, 52, 64, 68, 72, 76, 80, 84, 96, 100, 104, 108, 112, 116, 128, 132, 136, 140, 144, 148, 160, 168, 176 };
    for (uint32_t i = 0; i < sizeof(corrupt) / sizeof(corrupt[0]); ++i)
    {
        bytes[corrupt[i]] = 0xff;
        HDataBlob output = sentinel;
        ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(bytes, size, &output));
        ASSERT_EQ((uintptr_t)sentinel, (uintptr_t)output);
        memcpy(bytes, saved, size);
    }
    bytes[second + 20] = 0;
    HDataBlob output = sentinel;
    ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(bytes, size, &output));
    memcpy(bytes, saved, size);
    memcpy(bytes + 88, bytes + 56, 8); // Repeated property name in shared metadata.
    ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(bytes, size, &output));
    memcpy(bytes, saved, size);
    WriteDataInteger(bytes + 100, 0, 4); // String overlaps boolean in every row.
    ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(bytes, size, &output));
    memcpy(bytes, saved, size);
    uint32_t nested = (uint32_t)ReadDataInteger(bytes + 176, 8);
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
    DataPropertyDesc meta = { 1, DATA_VALUE_TYPE_NULL, 0 };
    DataTableDesc    desc = { 1, 0, 0, &meta, 1, 0 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &desc));
    DataValue value = Null();
    DataId    id;
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(source, 1, 0, &value, 1, &id));
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(source, id, 1, &value));
    ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(source, id, 1, &value));
    DataTableDesc empty = { 2, 0, 0, 0, 0, 0 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &empty));
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(source, 2, 0, 0, 0, &id));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataGetProperty(source, id, 1, &value));
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
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(another, FirstId(another), 1, &value));
    ASSERT_EQ(DATA_VALUE_TYPE_NULL, value.m_Type);
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
    ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, first, 20, &value));
    ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, first, 20, &value));
    ASSERT_EQ(DATA_RESULT_OK, Add(store, 1, 2, 23, value.m_Value.m_String, &second));
    char large[8193];
    memset(large, 'L', sizeof(large) - 1);
    large[sizeof(large) - 1] = 0;
    for (uint32_t cycle = 0; cycle < 3; ++cycle)
    {
        value = String(large);
        ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, first, 20, &value));
        ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, first, 20, &value));
        ASSERT_STREQ(large, value.m_Value.m_String);
        ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, first, 20, &value));
        for (uint32_t i = 0; i < 300; ++i)
        {
            value = String("replacement");
            ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, first, 20, &value));
        }
        ASSERT_EQ(DATA_RESULT_OK, DataResetTable(store, 1));
        ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, first, 20, &value));
        ASSERT_STREQ("original", value.m_Value.m_String);
        ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, second, 20, &value));
        ASSERT_STREQ("new baseline", value.m_Value.m_String);
    }
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataResetProperty(store, 0, 20));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataResetProperty(store, first, 999));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataResetTable(store, 999));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}

TEST(Data, TypedAccessErrorsPackedValuesAndReset)
{
    HDataStore source = DataCreateStore();
    // Field order differs from query binding order.
    DataPropertyDesc meta[] = { { 10, DATA_VALUE_TYPE_VECTOR3, 0 }, { 20, DATA_VALUE_TYPE_STRING, 16 }, { 30, DATA_VALUE_TYPE_BOOLEAN, 24 } };
    DataTableDesc    table = { 1, 0, 0, meta, 3, 32 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &table));
    DataValue values[] = { Vector3(1, 2, 3), String("packed"), Boolean(1) };
    DataId    original;
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(source, 1, 0, values, 3, &original));
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
        HDataStore        store = stores[mode];
        DataQueryProperty fields[] = { { 30, DATA_VALUE_TYPE_BOOLEAN }, { 10, DATA_VALUE_TYPE_VECTOR3 }, { 20, DATA_VALUE_TYPE_STRING } };
        DataQueryDesc     desc = { 0, 0, 0, 0, fields, 3 };
        HDataQuery        query;
        ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
        DataStoreLock(store);
        DataIterator it = DataQueryIter(query);
        DataVector3  vector = { { 7, 8, 9 } };
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, GetTestFieldVector3(&it, 0, 1, &vector));
        ASSERT_EQ(7.0f, vector.m_Values[0]);
        ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
        DataId id = DataIterGetId(&it, 0);
        ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyVector3(store, id, 10, &vector));
        ASSERT_EQ(1.0f, vector.m_Values[0]);
        ASSERT_EQ(3.0f, vector.m_Values[2]);
        const char* string;
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldString(&it, 0, 2, &string));
        ASSERT_STREQ("packed", string);
        if (mode)
            ASSERT_TRUE(string >= (const char*)bytes && string < (const char*)bytes + size);

        double number = 123;
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataGetPropertyNumber(store, id, 10, &number));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, GetTestFieldNumber(&it, 0, 1, &number));
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataGetPropertyNumber(store, 0, 10, &number));
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataGetPropertyNumber(store, id, 99, &number));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, GetTestFieldNumber(&it, 1, 1, &number));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, GetTestFieldNumber(&it, 0, 3, &number));
        ASSERT_EQ(123.0, number);
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetPropertyNumber(store, id, 10, 4));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, SetTestFieldNumber(&it, 0, 1, 4));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, SetTestFieldVector3(&it, 1, 1, &vector));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetPropertyBoolean(store, id, 30, 2));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, SetTestFieldBoolean(&it, 0, 0, 2));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetPropertyString(store, id, 20, 0));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, SetTestFieldString(&it, 0, 2, 0));
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataSetPropertyVector3(store, 0, 10, &vector));
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataSetPropertyVector3(store, id, 99, &vector));
        DataMemoryStats memory;
        GetDataMemoryStats(store, &memory);
        ASSERT_EQ((uint64_t)0, memory.m_PayloadBlocks);

        vector.m_Values[1] = 8;
        ASSERT_EQ(DATA_RESULT_OK, DataSetPropertyVector3(store, id, 10, &vector));
        vector.m_Values[1] = 99; // Set copied the caller's math value.
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldVector3(&it, 0, 1, &vector));
        ASSERT_EQ(8.0f, vector.m_Values[1]);
        ASSERT_EQ(DATA_RESULT_OK, SetTestFieldBoolean(&it, 0, 0, 0));
        uint8_t boolean = 1;
        ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyBoolean(store, id, 30, &boolean));
        ASSERT_EQ(0, boolean);
        char replacement[] = "changed";
        ASSERT_EQ(DATA_RESULT_OK, DataSetPropertyString(store, id, 20, replacement));
        replacement[0] = 'X';
        ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyString(store, id, 20, &string));
        ASSERT_STREQ("changed", string);
        ASSERT_EQ(DATA_RESULT_OK, SetTestFieldString(&it, 0, 2, string));
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldString(&it, 0, 2, &string));
        ASSERT_STREQ("changed", string);
        ASSERT_EQ(DATA_RESULT_OK, DataResetProperty(store, id, 10));
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldVector3(&it, 0, 1, &vector));
        ASSERT_EQ(2.0f, vector.m_Values[1]);
        if (mode)
            DataResetBlob(instance);
        else
            ASSERT_EQ(DATA_RESULT_OK, DataResetTable(store, 1));
        ASSERT_EQ(DATA_RESULT_OK, GetTestFieldBoolean(&it, 0, 0, &boolean));
        ASSERT_EQ(1, boolean);
        ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyString(store, id, 20, &string));
        ASSERT_STREQ("packed", string);
        GetDataMemoryStats(store, &memory);
        ASSERT_EQ((uint64_t)0, memory.m_PayloadBlocks);
        DataStoreUnlock(store);
        ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, id));
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataGetPropertyVector3(store, id, 10, &vector));
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataSetPropertyVector3(store, id, 10, &vector));
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
    HDataStore        store = DataCreateStore();
    DataQueryProperty fields[] = { { 10, DATA_VALUE_TYPE_NUMBER }, { 20, DATA_VALUE_TYPE_STRING } };
    DataQueryDesc     desc = { 0, 0, 0, 0, fields, 2 };
    HDataQuery        query;
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
                ASSERT_EQ(DATA_RESULT_OK, SetTestFieldNumber(&it, r, 0, (double)DataIterGetOwnerId(&it, r)));
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
                DataOwnerId owner = DataIterGetOwnerId(&it, r);
                ASSERT_EQ(DATA_RESULT_OK, GetTestFieldNumber(&it, r, 0, &number));
                ASSERT_EQ(owner >= 1000 ? 5.0 : (double)owner, number);
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

TEST(Data, TypedStructVector3Reads)
{
    HDataStore       source = DataCreateStore();
    DataPropertyDesc meta[] = { { 1, DATA_VALUE_TYPE_NUMBER, 0 }, { 2, DATA_VALUE_TYPE_STRUCT, 8 } };
    DataTableDesc    table = { 1, 0, 0, meta, 2, 16 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &table));
    uint64_t       names[] = { 20, 10 };
    uint64_t       reversed_names[] = { 10, 20 };
    const uint32_t bits[] = { 0x80000000, 0x7f800000, 0x7fc01234 };
    DataValue      color = Vector3(0, 0, 0);
    memcpy(color.m_Value.m_Vector3, bits, sizeof(bits));
    DataValue children[] = { Number(5), color };
    DataValue reversed_children[] = { color, Number(5) };
    DataValue wrong_kind = Number(5);
    DataValue objects[] = { Struct(names, children, 2), Struct(reversed_names, reversed_children, 2), Struct(0, 0, 0), Struct(reversed_names, &wrong_kind, 1) };
    for (uint32_t r = 0; r < 4; ++r)
    {
        DataValue values[] = { Number(r), objects[r] };
        DataId    id;
        ASSERT_EQ(DATA_RESULT_OK, DataAddRow(source, 1, r, values, 2, &id));
    }
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(source, bytes, size, &size));

    for (uint32_t packed = 0; packed < 2; ++packed)
    {
        HDataStore        store = packed ? DataCreateStore() : source;
        HDataBlobInstance instance = 0;
        if (packed)
            ASSERT_EQ(DATA_RESULT_OK, Load(store, bytes, size, &instance));
        DataQueryProperty fields[] = { { 2, DATA_VALUE_TYPE_STRUCT }, { 1, DATA_VALUE_TYPE_NUMBER } };
        DataQueryDesc     desc = { 0, 0, 0, 0, fields, 2 };
        HDataQuery        query;
        ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
        DataStoreLock(store);
        DataIterator it = DataQueryIter(query);
        DataVector3  out = { { 9, 8, 7 } };
        DataVector3  unchanged = out;
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, GetTestStructPropertyVector3(&it, 0, 0, 10, &out));
        ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
        ASSERT_EQ(4u, DataIterGetCount(&it));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, GetTestStructPropertyVector3(&it, 4, 0, 10, &out));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, GetTestStructPropertyVector3(&it, 0, 2, 10, &out));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, GetTestStructPropertyVector3(&it, 0, 1, 10, &out));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, GetTestStructPropertyVector3(&it, 0, 0, 20, &out));
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, GetTestStructPropertyVector3(&it, 0, 0, 99, &out));
        ASSERT_EQ(DATA_RESULT_NOT_FOUND, GetTestStructPropertyVector3(&it, 2, 0, 10, &out));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, GetTestStructPropertyVector3(&it, 3, 0, 10, &out));
        ASSERT_EQ(0, memcmp(&unchanged, &out, sizeof(out)));
        for (uint32_t r = 0; r < 2; ++r)
        {
            ASSERT_EQ(DATA_RESULT_OK, GetTestStructPropertyVector3(&it, r, 0, 10, &out));
            ASSERT_EQ(0, memcmp(bits, out.m_Values, sizeof(bits)));
        }

        // Unfiltered field iteration retrieves container metadata from the table.
        DataQueryDesc all_desc = {};
        HDataQuery    all_query;
        ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &all_desc, &all_query));
        DataIterator all = DataQueryIter(all_query);
        ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&all));
        ASSERT_EQ(DATA_RESULT_OK, GetTestStructPropertyVector3(&all, 0, 1, 10, &out));
        ASSERT_EQ(0, memcmp(bits, out.m_Values, sizeof(bits)));
        DataDestroyQuery(all_query);

        DataId    id = DataIterGetId(&it, 1);
        DataValue replacement_children[] = { Number(8), Vector3(4, 5, 6) };
        DataValue replacement = Struct(names, replacement_children, 2);
        ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, id, 2, &replacement));
        ASSERT_EQ(DATA_RESULT_OK, GetTestStructPropertyVector3(&it, 1, 0, 10, &out));
        ASSERT_EQ(0, memcmp(replacement_children[1].m_Value.m_Vector3, out.m_Values, sizeof(out)));
        ASSERT_EQ(DATA_RESULT_OK, DataResetProperty(store, id, 2));
        ASSERT_EQ(DATA_RESULT_OK, GetTestStructPropertyVector3(&it, 1, 0, 10, &out));
        ASSERT_EQ(0, memcmp(bits, out.m_Values, sizeof(bits)));
        ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, id, 2, &replacement));
        if (packed)
            DataResetBlob(instance);
        else
            ASSERT_EQ(DATA_RESULT_OK, DataResetTable(store, 1));
        ASSERT_EQ(DATA_RESULT_OK, GetTestStructPropertyVector3(&it, 1, 0, 10, &out));
        ASSERT_EQ(0, memcmp(bits, out.m_Values, sizeof(bits)));

        if (!packed)
        {
            // Owner filtering starts a batch at a nonzero row index.
            DataOwnerId owner = 1;
            desc.m_OwnerIds = &owner;
            desc.m_OwnerIdCount = 1;
            HDataQuery filtered;
            ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &filtered));
            DataIterator selected = DataQueryIter(filtered);
            ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&selected));
            ASSERT_EQ(1u, DataIterGetCount(&selected));
            ASSERT_EQ(DATA_RESULT_OK, GetTestStructPropertyVector3(&selected, 0, 0, 10, &out));
            ASSERT_EQ(0, memcmp(bits, out.m_Values, sizeof(bits)));
            DataDestroyQuery(filtered);
        }
        ASSERT_EQ(DATA_RESULT_END, DataIterNext(&it));
        out = unchanged;
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, GetTestStructPropertyVector3(&it, 0, 0, 10, &out));
        it = DataQueryIter(query);
        ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
        DataStoreUnlock(store);
        ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, id));
        ASSERT_EQ(0, memcmp(&unchanged, &out, sizeof(out)));
        DataDestroyQuery(query);
        ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    }
    delete[] bytes;
}

extern "C" int TestInlineDataFromC(HDataStore store);

TEST(Data, InlineCompositionQueriesAndRoundtrip)
{
    // Nested fixed layouts preserve C alignment and include variable data.
    DataPropertyDesc light_fields[] = { { 10, DATA_VALUE_TYPE_VECTOR3, 0 }, { 20, DATA_VALUE_TYPE_STRING, 16 }, { 30, DATA_VALUE_TYPE_LIST, 24 } };
    DataStructDesc   light = { light_fields, 3, 32 };
    DataPropertyDesc wrapper_fields[] = { { 40, DATA_VALUE_TYPE_STRUCT, 8, &light } };
    DataStructDesc   wrapper = { wrapper_fields, 1, 40 };
    DataPropertyDesc fields[] = { { 1, DATA_VALUE_TYPE_NUMBER, 0 }, { 2, DATA_VALUE_TYPE_STRUCT, 8, &wrapper }, { 3, DATA_VALUE_TYPE_BOOLEAN, 48 } };
    DataTableDesc    table = { 1, 0, 0, fields, 3, 56 };
    HDataStore       source = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &table));
    // Changing a declaration after registration cannot alter the compiled layout.
    light_fields[0].m_Offset = 999;
    uint64_t  names[] = { 30, 10, 20 };
    DataValue list_items[] = { Number(7), String("shared") };
    DataValue children[] = { List(list_items, 2), Vector3(1, 2, 3), String("shared") };
    DataValue light_value = Struct(names, children, 3);
    uint64_t  light_name = 40;
    DataValue values[] = { Number(5), Struct(&light_name, &light_value, 1), Boolean(1) };
    DataId    original;
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(source, 1, 1, values, 3, &original));
    ASSERT_EQ(7u, source->m_Tables[0]->m_MetadataCount);
    ASSERT_EQ(32u, GetPropertyMeta(source->m_Tables[0], 3).m_Size);

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
        uint64_t          color_path[] = { 40, 10 }, string_path[] = { 40, 20 }, list_path[] = { 40, 30 };
        DataQueryProperty properties[] = {
            { 2, DATA_VALUE_TYPE_VECTOR3, color_path, 2 },
            { 2, DATA_VALUE_TYPE_STRING, string_path, 2 },
            { 2, DATA_VALUE_TYPE_LIST, list_path, 2 },
            { 1, DATA_VALUE_TYPE_NUMBER },
            { 2, DATA_VALUE_TYPE_STRUCT }
        };
        DataQueryDesc desc = { 0, 0, 0, 0, properties, 5 };
        HDataQuery    query;
        ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
        color_path[0] = string_path[0] = 99; // Query owns its path hashes.
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
        double wrong_kind_output = 17;
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, GetTestFieldNumber(&it, 0, 0, &wrong_kind_output));
        ASSERT_EQ(17.0, wrong_kind_output);
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
        uint64_t          changed_paths[] = { 40, 10, 40, 20 };
        DataQueryProperty changed_fields[] = { { 2, DATA_VALUE_TYPE_VECTOR3, changed_paths, 2 }, { 2, DATA_VALUE_TYPE_STRING, changed_paths + 2, 2 } };
        DataQueryDesc     changed_desc = { 0, 0, 0, 0, changed_fields, 2 };
        HDataQuery        changed_query;
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
        ASSERT_EQ(DATA_RESULT_OK, DataResetProperty(store, id, 2));
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
        // A selected struct path keeps its child metadata in the shared binding.
        DataQueryProperty light_property = { 2, DATA_VALUE_TYPE_STRUCT, &light_name, 1 };
        DataQueryDesc     light_desc = { 0, 0, 0, 0, &light_property, 1 };
        HDataQuery        light_query;
        ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &light_desc, &light_query));
        DataIterator light_it = DataQueryIter(light_query);
        ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&light_it));
        ASSERT_EQ(DATA_RESULT_OK, GetTestStructPropertyVector3(&light_it, 0, 0, 10, &color));
        ASSERT_EQ(3.0f, color.m_Values[2]);
        DataDestroyQuery(light_query);
        DataValue view, light_view;
        ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, id, 2, &view));
        ASSERT_EQ(DATA_RESULT_OK, DataGetStructProperty(&view.m_Value.m_Struct, 40, &light_view));
        ASSERT_EQ(DATA_RESULT_OK, DataGetStructProperty(&light_view.m_Value.m_Struct, 10, &child));
        ASSERT_EQ(3.0f, child.m_Value.m_Vector3[2]);
        ASSERT_EQ(DATA_RESULT_OK, DataSetProperty(store, id, 2, &view)); // Borrowed self-assignment.
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

TEST(Data, InlineLayoutAndPathValidation)
{
    HDataStore       store = DataCreateStore();
    DataPropertyDesc members[] = { { 10, DATA_VALUE_TYPE_VECTOR3, 0 }, { 20, DATA_VALUE_TYPE_NUMBER, 16 } };
    DataStructDesc   light = { members, 2, 24 };
    DataPropertyDesc fields[] = { { 1, DATA_VALUE_TYPE_STRUCT, 0, &light } };
    DataTableDesc    table = { 1, 0, 0, fields, 1, 24 };
    light.m_Size = 8;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &table));
    light.m_Size = 24;
    members[1].m_Offset = 4;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &table));
    members[1].m_Offset = 16;
    members[1].m_Property = 10;
    ASSERT_EQ(DATA_RESULT_ALREADY_EXISTS, DataRegisterTable(store, &table));
    members[1].m_Property = 20;
    fields[0].m_Type = DATA_VALUE_TYPE_VECTOR3;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &table));
    fields[0].m_Type = DATA_VALUE_TYPE_STRUCT;
    DataStructDesc cycle = { fields, 1, 24 };
    fields[0].m_Struct = &cycle;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRegisterTable(store, &table));
    fields[0].m_Struct = &light;
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    uint64_t  names[] = { 10, 20 };
    DataValue children[] = { Vector3(1, 2, 3), Number(4) };
    DataValue value = Struct(names, children, 2);
    DataId    id = 0;
    children[1] = Boolean(1);
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataAddRow(store, 1, 0, &value, 1, &id));
    children[1] = Number(4);
    names[1] = 99;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataAddRow(store, 1, 0, &value, 1, &id));
    names[1] = 20;
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(store, 1, 0, &value, 1, &id));
    children[1] = Boolean(1);
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataSetProperty(store, id, 1, &value));
    uint64_t          path = 10;
    DataQueryProperty property = { 1, DATA_VALUE_TYPE_VECTOR3, &path, 1 };
    DataQueryDesc     desc = { 0, 0, 0, 0, &property, 1 };
    HDataQuery        query;
    property.m_Path = 0;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataCreateQuery(store, &desc, &query));
    property.m_Path = &path;
    property.m_PathCount = DATA_MAX_NESTING + 1;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataCreateQuery(store, &desc, &query));
    property.m_PathCount = 1;
    path = 99;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    DataStoreLock(store);
    DataIterator it = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_END, DataIterNext(&it));
    DataDestroyQuery(query);
    path = 10;
    property.m_Type = DATA_VALUE_TYPE_NUMBER;
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
    const uint32_t corrupt[] = { metadata + 16, metadata + 20, metadata + 24, metadata + DATA_PROPERTY_META_SIZE + 16, metadata + 2 * DATA_PROPERTY_META_SIZE + 12 };
    const uint32_t values[] = { 0, UINT32_MAX, 25, 1, 4 };
    for (uint32_t i = 0; i < 5; ++i)
    {
        WriteDataInteger(bytes + corrupt[i], values[i], 4);
        ASSERT_EQ(DATA_RESULT_INVALID_FORMAT, DataLoadBlob(bytes, size, &blob));
        memcpy(bytes, saved, size);
    }
    ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
    DataDestroyBlob(blob);
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, InlinePathsBindDifferentTablesAndTrackRegistration)
{
    HDataStore        store = DataCreateStore();
    uint64_t          path = 10;
    DataQueryProperty field = { 1, DATA_VALUE_TYPE_VECTOR3, &path, 1 };
    DataQueryDesc     query_desc = { 0, 0, 0, 0, &field, 1 };
    HDataQuery        query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &query_desc, &query));
    DataPropertyDesc members[] = { { 10, DATA_VALUE_TYPE_VECTOR3, 0 }, { 20, DATA_VALUE_TYPE_NUMBER, 16 } };
    DataStructDesc   light = { members, 2, 24 };
    uint64_t         names[] = { 10, 20 };
    DataValue        children[] = { Vector3(1, 2, 3), Number(4) };
    DataValue        value = Struct(names, children, 2);
    DataId           ids[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        DataPropertyDesc root = { 1, DATA_VALUE_TYPE_STRUCT, i * 8, i < 2 ? &light : 0 };
        DataTableDesc    table = { i + 1, 0, 0, &root, 1, i * 8 + (i < 2 ? 24u : 8u) };
        ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
        ASSERT_EQ(DATA_RESULT_OK, DataAddRow(store, i + 1, i, &value, 1, &ids[i]));
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
        ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, DataIterGetId(&it, 0), 1, &object));
        ASSERT_EQ(DATA_RESULT_OK, DataGetStructProperty(&object.m_Value.m_Struct, 20, &intensity));
        ASSERT_EQ(4.0, intensity.m_Value.m_Number);
        ++visited;
    }
    ASSERT_EQ(2u, visited);
    ASSERT_EQ(DATA_RESULT_OK, DataResetTable(store, 1));
    field.m_Type = DATA_VALUE_TYPE_STRUCT;
    field.m_PathCount = 0;
    HDataQuery roots;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &query_desc, &roots));
    it = DataQueryIter(roots);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
    DataVector3 color;
    ASSERT_EQ(DATA_RESULT_OK, GetTestStructPropertyVector3(&it, 0, 0, 10, &color));
    ASSERT_EQ(1.0f, color.m_Values[0]);
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, GetTestStructPropertyVector3(&it, 0, 0, 99, &color));
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, GetTestStructPropertyVector3(&it, 0, 0, 20, &color));
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataUnregisterTable(store, 1));
    DataStoreLock(store);
    it = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
    ASSERT_EQ((uint64_t)2, DataIterGetType(&it));
    ASSERT_EQ(DATA_RESULT_END, DataIterNext(&it));
    DataDestroyQuery(roots);
    DataDestroyQuery(query);
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, ResetOneComponentPreservesOtherRowsAndReusesStorage)
{
    HDataStore       store = DataCreateStore();
    DataPropertyDesc fields[10] = {};
    DataValue        values[10];
    for (uint32_t p = 0; p < 10; ++p)
    {
        fields[p].m_Property = p;
        fields[p].m_Type = DATA_VALUE_TYPE_NUMBER;
        fields[p].m_Offset = p * 8;
        values[p] = Number(p);
    }
    DataTableDesc table = { 1, 0, 0, fields, 10, 80 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    DataId ids[3];
    for (uint32_t r = 0; r < 3; ++r)
        ASSERT_EQ(DATA_RESULT_OK, DataAddRow(store, 1, r == 2 ? 8 : 7, values, 10, &ids[r], r + 11));
    DataMemoryStats before, after;
    GetDataMemoryStats(store, &before);
    ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, ids[0]));
    GetDataMemoryStats(store, &after);
    ASSERT_EQ(before.m_TotalBytes, after.m_TotalBytes);

    for (uint32_t r = 0; r < 3; ++r)
        for (uint32_t p = 0; p < 10; ++p)
            ASSERT_EQ(DATA_RESULT_OK, DataSetPropertyNumber(store, ids[r], p, 100 * (r + 1) + p));
    DataOwnerId       owner = 7;
    DataQueryProperty field = { 9, DATA_VALUE_TYPE_NUMBER };
    DataQueryDesc     desc = { &owner, 1, 0, 0, &field, 1 };
    HDataQuery        query;
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
            ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyNumber(store, ids[r], p, &number));
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
        ASSERT_EQ(DATA_RESULT_OK, DataSetPropertyNumber(store, ids[0], 9, 500));
        ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, ids[0]));
        ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyNumber(store, ids[0], 9, &number));
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
    ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyNumber(store, ids[2], 9, &number));
    ASSERT_EQ(9.0, number);
    ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyNumber(store, ids[1], 9, &number));
    ASSERT_EQ(209.0, number);
    DataDestroyQuery(query);

    DataTableDesc empty = { 2, 0, 0, 0, 0, 0 };
    DataId        empty_id;
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &empty));
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(store, 2, 7, 0, 0, &empty_id));
    ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, empty_id));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataResetRow(store, ids[0]));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

extern "C" int TestDataEmptyFieldFromC(const DataFieldIterator* field);

TEST(Data, FieldIterationWithoutPropertyFilters)
{
    HDataStore       source = DataCreateStore();
    DataPropertyDesc meta[] = { { 10, DATA_VALUE_TYPE_NUMBER, 0 }, { 20, DATA_VALUE_TYPE_STRING, 8 }, { 30, DATA_VALUE_TYPE_VECTOR3, 16 }, { 40, DATA_VALUE_TYPE_NULL, 0 } };
    DataTableDesc    table = { 1, 0, 0, meta, 4, 32 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &table));
    DataValue values[] = { Number(10), String("original"), Vector3(1, 2, 3), Null() };
    DataId    id;
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(source, 1, 7, values, 4, &id));
    values[0] = Number(20);
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(source, 1, 8, values, 4, &id));
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
        DataRowIterator   rows = DataIterRows(&batch);
        DataFieldIterator empty_fields = DataRowIterFields(&rows);
        ASSERT_EQ(DATA_RESULT_END, DataFieldIterNext(&empty_fields));
        ASSERT_EQ(0, TestDataEmptyFieldFromC(&empty_fields));
        double number = -1;
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataFieldIterGetNumber(&empty_fields, &number));
        ASSERT_EQ(-1.0, number);
        ASSERT_EQ(DATA_RESULT_OK, DataRowIterNext(&rows));
        id = DataRowIterGetId(&rows);
        ASSERT_EQ((DataOwnerId)(mode ? 9 : 7), DataRowIterGetOwnerId(&rows));
        DataFieldIterator field = DataRowIterFields(&rows);
        ASSERT_EQ(UINT32_MAX, field.m_Index);
        ASSERT_EQ(0, TestDataEmptyFieldFromC(&field));
        ASSERT_EQ(DATA_VALUE_TYPE_NULL, DataFieldIterGetType(&field));
        ASSERT_EQ((uint64_t)0, DataFieldIterGetNameHash(&field));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataFieldIterSetNumber(&field, 99));
        ASSERT_EQ(DATA_RESULT_OK, DataFieldIterNext(&field));
        ASSERT_EQ(0u, field.m_Index);
        ASSERT_EQ((uint64_t)10, DataFieldIterGetNameHash(&field));
        ASSERT_EQ(DATA_VALUE_TYPE_NUMBER, DataFieldIterGetType(&field));
        ASSERT_EQ(DATA_RESULT_OK, DataFieldIterGetNumber(&field, &number));
        ASSERT_EQ(10.0, number);
        ASSERT_EQ(DATA_RESULT_OK, DataFieldIterSetNumber(&field, 35));
        ASSERT_EQ(DATA_RESULT_OK, DataFieldIterGetNumber(&field, &number));
        ASSERT_EQ(35.0, number);
        ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, id));
        ASSERT_EQ(DATA_RESULT_OK, DataFieldIterGetNumber(&field, &number));
        ASSERT_EQ(10.0, number);
        ASSERT_EQ(DATA_RESULT_OK, DataFieldIterNext(&field));
        ASSERT_EQ(1u, field.m_Index);
        ASSERT_EQ((uint64_t)20, DataFieldIterGetNameHash(&field));
        ASSERT_EQ(DATA_VALUE_TYPE_STRING, DataFieldIterGetType(&field));
        const char* string;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldIterGetString(&field, &string));
        ASSERT_STREQ("original", string);
        ASSERT_EQ(DATA_RESULT_OK, DataFieldIterSetString(&field, "replacement"));
        ASSERT_EQ(DATA_RESULT_OK, DataFieldIterGetString(&field, &string));
        ASSERT_STREQ("replacement", string);
        if (mode)
            DataResetBlob(instance);
        else
            ASSERT_EQ(DATA_RESULT_OK, DataResetTable(store, 1));
        // Reset freed the override blocks; the cursor must re-read the default bytes.
        ASSERT_EQ(DATA_RESULT_OK, DataFieldIterGetString(&field, &string));
        ASSERT_STREQ("original", string);
        ASSERT_EQ(DATA_RESULT_OK, DataFieldIterNext(&field));
        ASSERT_EQ(2u, field.m_Index);
        ASSERT_EQ((uint64_t)30, DataFieldIterGetNameHash(&field));
        ASSERT_EQ(DATA_VALUE_TYPE_VECTOR3, DataFieldIterGetType(&field));
        DataVector3 vector;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldIterGetVector3(&field, &vector));
        ASSERT_EQ(3.0f, vector.m_Values[2]);
        ASSERT_EQ(DATA_RESULT_OK, DataFieldIterNext(&field));
        ASSERT_EQ(3u, field.m_Index);
        ASSERT_EQ((uint64_t)40, DataFieldIterGetNameHash(&field));
        ASSERT_EQ(DATA_VALUE_TYPE_NULL, DataFieldIterGetType(&field));
        number = -1;
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataFieldIterGetNumber(&field, &number));
        ASSERT_EQ(-1.0, number);
        ASSERT_EQ(DATA_RESULT_END, DataFieldIterNext(&field));
        ASSERT_EQ(UINT32_MAX, field.m_Index);
        ASSERT_EQ(0, TestDataEmptyFieldFromC(&field));
        ASSERT_EQ(DATA_VALUE_TYPE_NULL, DataFieldIterGetType(&field));
        ASSERT_EQ((uint64_t)0, DataFieldIterGetNameHash(&field));
        ASSERT_EQ(DATA_RESULT_END, DataFieldIterNext(&field));
        ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataFieldIterSetNumber(&field, 99));
        ASSERT_EQ(DATA_RESULT_OK, DataRowIterNext(&rows));
        field = DataRowIterFields(&rows);
        ASSERT_EQ(DATA_RESULT_OK, DataFieldIterNext(&field));
        ASSERT_EQ(DATA_RESULT_OK, DataFieldIterGetNumber(&field, &number));
        ASSERT_EQ(20.0, number);
        ASSERT_EQ(DATA_RESULT_END, DataRowIterNext(&rows));
        ASSERT_EQ(DATA_RESULT_END, DataRowIterNext(&rows));
        ASSERT_EQ((DataId)0, DataRowIterGetId(&rows));
        ASSERT_EQ(DATA_RESULT_END, DataIterNext(&batch));
        DataDestroyQuery(query);

        // Treat requested fields as a set, identifying values by name in either descriptor order.
        DataQueryProperty requested[][2] = {
            { { 30, DATA_VALUE_TYPE_VECTOR3 }, { 10, DATA_VALUE_TYPE_NUMBER } },
            { { 10, DATA_VALUE_TYPE_NUMBER }, { 30, DATA_VALUE_TYPE_VECTOR3 } }
        };
        for (uint32_t order = 0; order < 2; ++order)
        {
            desc.m_Properties = requested[order];
            desc.m_PropertyCount = 2;
            ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
            batch = DataQueryIter(query);
            ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&batch));
            rows = DataIterRows(&batch);
            ASSERT_EQ(DATA_RESULT_OK, DataRowIterNext(&rows));
            field = DataRowIterFields(&rows);
            ASSERT_EQ(DATA_VALUE_TYPE_NULL, DataFieldIterGetType(&field));
            ASSERT_EQ((uint64_t)0, DataFieldIterGetNameHash(&field));
            uint32_t seen = 0, count = 0;
            while (DataFieldIterNext(&field) == DATA_RESULT_OK)
            {
                ASSERT_EQ(count++, field.m_Index);
                uint64_t name = DataFieldIterGetNameHash(&field);
                if (name == 30)
                {
                    ASSERT_EQ(DATA_VALUE_TYPE_VECTOR3, DataFieldIterGetType(&field));
                    ASSERT_EQ(DATA_RESULT_OK, DataFieldIterGetVector3(&field, &vector));
                    ASSERT_EQ(3.0f, vector.m_Values[2]);
                    seen |= 1;
                }
                else
                {
                    ASSERT_EQ((uint64_t)10, name);
                    ASSERT_EQ(DATA_VALUE_TYPE_NUMBER, DataFieldIterGetType(&field));
                    ASSERT_EQ(DATA_RESULT_OK, DataFieldIterGetNumber(&field, &number));
                    ASSERT_EQ(10.0, number);
                    seen |= 2;
                }
            }
            ASSERT_EQ(2u, count);
            ASSERT_EQ(3u, seen);
            ASSERT_EQ(DATA_VALUE_TYPE_NULL, DataFieldIterGetType(&field));
            ASSERT_EQ((uint64_t)0, DataFieldIterGetNameHash(&field));
            DataDestroyQuery(query);
        }
        DataStoreUnlock(store);
    }
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(loaded));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(source));
    delete[] bytes;
}

TEST(Data, FieldCursorUsesBatchRelativeRowsAfterRemoval)
{
    HDataStore store = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, Register(store, 1));
    DataId ids[6];
    for (uint32_t row = 0; row < 6; ++row)
        ASSERT_EQ(DATA_RESULT_OK, Add(store, 1, row % 2, row + 10, "base", &ids[row]));
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[1])); // Last row moves; its base bytes do not.
    DataOwnerId       owner = 1;
    DataQueryProperty property = { 10, DATA_VALUE_TYPE_NUMBER };
    for (uint32_t requested = 0; requested < 2; ++requested)
    {
        DataQueryDesc desc = { &owner, 1, 0, 0, requested ? &property : 0, requested };
        HDataQuery    query;
        ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
        uint32_t handle = DataQueryFindField(query, &property);
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
                ASSERT_EQ(owner, DataRowIterGetOwnerId(&rows));
                DataFieldIterator field = DataRowIterFields(&rows);
                ASSERT_EQ(DATA_RESULT_OK, DataFieldIterNext(&field));
                ASSERT_EQ((uint64_t)10, DataFieldIterGetNameHash(&field));
                double number;
                ASSERT_EQ(DATA_RESULT_OK, DataFieldIterGetNumber(&field, &number));
                ASSERT_EQ(id == ids[3] ? 13.0 : 15.0, number);
                if (requested)
                    ASSERT_EQ(number, *DataFieldGetNumber(&rows, handle));
                ASSERT_EQ(DATA_RESULT_OK, DataFieldIterSetNumber(&field, 99));
                ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyNumber(store, id, 10, &number));
                ASSERT_EQ(99.0, number);
                ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, id));
                ASSERT_EQ(DATA_RESULT_OK, DataFieldIterGetNumber(&field, &number));
                ASSERT_EQ(id == ids[3] ? 13.0 : 15.0, number);
                if (requested)
                    ASSERT_EQ(number, *DataFieldGetNumber(&rows, handle));
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
    DataQueryProperty property = { 10, DATA_VALUE_TYPE_NUMBER };
    DataQueryDesc     desc = { 0, 0, 0, 0, &property, 1 };
    HDataQuery        query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
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
                DataOwnerId owner = DataRowIterGetOwnerId(&rows);
                ASSERT_EQ(ids[owner], DataRowIterGetId(&rows));
                DataFieldIterator field = DataRowIterFields(&rows);
                ASSERT_EQ(DATA_RESULT_OK, DataFieldIterNext(&field));
                double value;
                ASSERT_EQ(DATA_RESULT_OK, DataFieldIterGetNumber(&field, &value));
                ASSERT_EQ((double)owner, value);
                if (owner % 2 == 0 || owner == 63)
                    removals.Push(DataRowIterGetId(&rows));
                ++visited;
            }
        }
        ASSERT_EQ(64u, visited);
        ASSERT_EQ(33u, removals.Size());
        // Queued IDs are still readable before the caller applies its buffer.
        double value;
        ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyNumber(store, removals[0], 10, &value));
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
                DataOwnerId owner = DataRowIterGetOwnerId(&rows);
                ASSERT_TRUE(owner % 2 == 1 && owner != 63);
                ASSERT_EQ(ids[owner], DataRowIterGetId(&rows));
                ++visited;
            }
        }
        ASSERT_EQ(31u, visited);
        DataStoreUnlock(store);
        for (uint32_t r = 0; r < 64; ++r)
        {
            if (r % 2 == 0 || r == 63)
                ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataGetPropertyNumber(store, ids[r], 10, &value));
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
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(store, 1, 7, 0, 0, &id));
    DataQueryDesc desc = {};
    HDataQuery    query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    DataStoreLock(store);
    DataIterator batch = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&batch));
    DataRowIterator rows = DataIterRows(&batch);
    ASSERT_EQ(DATA_RESULT_OK, DataRowIterNext(&rows));
    ASSERT_EQ(id, DataRowIterGetId(&rows));
    DataFieldIterator field = DataRowIterFields(&rows);
    ASSERT_EQ(DATA_RESULT_END, DataFieldIterNext(&field));
    ASSERT_EQ(DATA_RESULT_END, DataRowIterNext(&rows));
    ASSERT_EQ(DATA_RESULT_END, DataIterNext(&batch));
    DataDestroyQuery(query);
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

TEST(Data, AlignedLayoutValidation)
{
    HDataStore       store = DataCreateStore();
    DataPropertyDesc members[] = { { 1, DATA_VALUE_TYPE_NUMBER, 0 }, { 2, DATA_VALUE_TYPE_BOOLEAN, 8 } };
    DataStructDesc   inner = { members, 2, 16 };
    DataPropertyDesc root = { 3, DATA_VALUE_TYPE_STRUCT, 0, &inner };
    DataTableDesc    table = { 1, 0, 0, &root, 1, 24 };
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
    uint64_t  names[] = { 1, 2 };
    DataValue children[] = { Number(17), Boolean(1) };
    DataValue value = Struct(names, children, 2);
    DataId    id;
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(store, 1, 0, &value, 1, &id));
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
    const uint32_t offsets[] = { 4, table_offset + 20, metadata + 12, metadata + 24, metadata + DATA_PROPERTY_META_SIZE + 12 };
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
    DataPropertyDesc fields[] = {
        { 1, DATA_VALUE_TYPE_NUMBER, offsetof(NativeRow, m_Number) },
        { 2, DATA_VALUE_TYPE_BOOLEAN, offsetof(NativeRow, m_Boolean) },
        { 3, DATA_VALUE_TYPE_VECTOR3, offsetof(NativeRow, m_Vector3) },
        { 4, DATA_VALUE_TYPE_VECTOR4, offsetof(NativeRow, m_Vector4) },
        { 5, DATA_VALUE_TYPE_MATRIX4, offsetof(NativeRow, m_Matrix4) }
    };
    DataTableDesc table = { 1, 0, 0, fields, 5, sizeof(NativeRow) };
    HDataStore    source = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &table));
    DataValue values[] = { Number(17), Boolean(0), Vector3(1, 2, 3), {}, Matrix() };
    values[3].m_Type = DATA_VALUE_TYPE_VECTOR4;
    values[3].m_Value.m_Vector4[3] = 4;
    for (uint32_t i = 0; i < 16; ++i)
        values[4].m_Value.m_Matrix4[i] = (float)i;
    DataId ids[3];
    for (uint32_t i = 0; i < 3; ++i)
        ASSERT_EQ(DATA_RESULT_OK, DataAddRow(source, 1, i, values, 5, &ids[i]));
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
        ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyNumber(store, id, 1, &number));
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
    HDataStore        store = DataCreateStore();
    DataQueryProperty properties[] = { { 30, DATA_VALUE_TYPE_VECTOR3 }, { 10, DATA_VALUE_TYPE_NUMBER } };
    DataQueryDesc     desc = { 0, 0, 0, 0, properties, 2 };
    HDataQuery        query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    uint32_t vector_field = DataQueryFindField(query, &properties[0]);
    uint32_t number_field = DataQueryFindField(query, &properties[1]);
    ASSERT_NE(UINT32_MAX, vector_field);
    ASSERT_NE(UINT32_MAX, number_field);
    ASSERT_NE(vector_field, number_field);
    DataQueryProperty invalid = { 30, DATA_VALUE_TYPE_VECTOR3, 0, 1 };
    ASSERT_EQ(UINT32_MAX, DataQueryFindField(query, &invalid));
    invalid.m_PathCount = DATA_MAX_NESTING + 1;
    ASSERT_EQ(UINT32_MAX, DataQueryFindField(query, &invalid));

    DataStoreLock(store);
    DataIterator empty = DataQueryIter(query);
    ASSERT_EQ(DATA_RESULT_END, DataIterNext(&empty));
    DataStoreUnlock(store);

    // Reuse the handles after END and across registration/removal with new offsets.
    for (uint32_t pass = 0; pass < 3; ++pass)
    {
        DataPropertyDesc meta[] = { { 10, DATA_VALUE_TYPE_NUMBER, pass * 8 }, { 30, DATA_VALUE_TYPE_VECTOR3, pass * 8 + 8 } };
        DataTableDesc    table = { 1, 0, 0, meta, 2, pass * 8 + 24 };
        ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
        DataValue values[] = { Number(10 + pass), Vector3(1, 2, 3) };
        DataId    id;
        ASSERT_EQ(DATA_RESULT_OK, DataAddRow(store, 1, 0, values, 2, &id));
        ASSERT_EQ(vector_field, DataQueryFindField(query, &properties[0]));
        ASSERT_EQ(number_field, DataQueryFindField(query, &properties[1]));
        for (uint32_t traversal = 0; traversal < 2; ++traversal)
        {
            DataStoreLock(store);
            DataIterator it = DataQueryIter(query);
            ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
            DataRowIterator rows = DataIterRows(&it);
            ASSERT_EQ(DATA_RESULT_OK, DataRowIterNext(&rows));
            ASSERT_EQ(id, DataRowIterGetId(&rows));
            ASSERT_EQ(10.0 + pass, *DataFieldGetNumber(&rows, number_field));
            ASSERT_EQ(3.0f, DataFieldGetVector3(&rows, vector_field)->m_Values[2]);
            *DataFieldGetNumberMut(&rows, number_field) = 99;
            ASSERT_EQ(99.0, *DataFieldGetNumber(&rows, number_field));
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
    DataPropertyDesc members[] = { { 10, DATA_VALUE_TYPE_VECTOR3, 0 }, { 20, DATA_VALUE_TYPE_NUMBER, 16 }, { 30, DATA_VALUE_TYPE_STRING, 24 } };
    DataStructDesc   light = { members, 3, 32 };
    uint64_t         names[] = { 10, 20, 30 };
    DataValue        first[] = { Vector3(1, 2, 3), Number(7), String("shared") };
    DataValue        second[] = { Vector3(9, 8, 7), Number(6), String("other") };
    DataValue        values[] = { Struct(names, first, 3), Struct(names, second, 3) };
    HDataStore       source = DataCreateStore();
    for (uint32_t i = 0; i < 2; ++i)
    {
        DataPropertyDesc roots[] = { { 1, DATA_VALUE_TYPE_STRUCT, i * 8, &light }, { 2, DATA_VALUE_TYPE_STRUCT, i * 8 + 32, &light } };
        DataTableDesc    table = { i + 1, 0, 0, roots, 2, i * 8 + 64 };
        ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &table));
        DataId id;
        ASSERT_EQ(DATA_RESULT_OK, DataAddRow(source, i + 1, i, values, 2, &id));
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
        HDataStore        store = stores[packed];
        uint64_t          color = 10;
        DataQueryProperty properties[] = { { 2, DATA_VALUE_TYPE_VECTOR3, &color, 1 }, { 1, DATA_VALUE_TYPE_VECTOR3, &color, 1 } };
        DataQueryDesc     desc = { 0, 0, 0, 0, properties, 2 };
        HDataQuery        query;
        ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
        // Lookup uses names/paths even when the descriptors change order.
        DataQueryProperty swap = properties[0];
        properties[0] = properties[1];
        properties[1] = swap;
        uint32_t field = DataQueryFindField(query, &properties[0]);
        uint32_t other = DataQueryFindField(query, &properties[1]);
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
            const DataVector3* original = DataFieldGetVector3(&rows, field);
            ASSERT_EQ(1.0f, original->m_Values[0]);
            ASSERT_EQ(9.0f, DataFieldGetVector3(&rows, other)->m_Values[0]);
            if (packed)
            {
                uint32_t         table_index = (uint32_t)DataIterGetType(&it) - 1;
                const DataTable* table = GetInstanceTable(instance, table_index);
                ASSERT_EQ((uintptr_t)(table->m_Values.Begin() + table_index * 8), (uintptr_t)original);
            }
            DataFieldGetVector3Mut(&rows, field)->m_Values[0] = 42;
            ASSERT_EQ(42.0f, DataFieldGetVector3(&rows, field)->m_Values[0]);
            ASSERT_EQ(9.0f, DataFieldGetVector3(&rows, other)->m_Values[0]);
            DataValue root, child;
            ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, id, 1, &root));
            ASSERT_EQ(DATA_RESULT_OK, DataGetStructProperty(&root.m_Value.m_Struct, 20, &child));
            ASSERT_EQ(7.0, child.m_Value.m_Number);
            ASSERT_EQ(DATA_RESULT_OK, DataGetStructProperty(&root.m_Value.m_Struct, 30, &child));
            ASSERT_STREQ("shared", child.m_Value.m_String);
            DataMemoryStats before, after;
            GetDataMemoryStats(store, &before);
            DataFieldGetVector3Mut(&rows, field)->m_Values[1] = 43;
            GetDataMemoryStats(store, &after);
            ASSERT_EQ(before.m_PayloadUsed, after.m_PayloadUsed);
            ASSERT_EQ(before.m_PayloadBlocks, after.m_PayloadBlocks);
            ASSERT_EQ(DATA_RESULT_OK, DataResetProperty(store, id, 1));
            ASSERT_EQ(2.0f, DataFieldGetVector3(&rows, field)->m_Values[1]);
            DataFieldGetVector3Mut(&rows, field)->m_Values[2] = 44;
            ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, id));
            ASSERT_EQ(3.0f, DataFieldGetVector3(&rows, field)->m_Values[2]);
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
    DataPropertyDesc members[] = { { 10, DATA_VALUE_TYPE_NUMBER, 0 }, { 20, DATA_VALUE_TYPE_STRING, 8 }, { 30, DATA_VALUE_TYPE_LIST, 16 } };
    DataStructDesc   layout = { members, 3, 24 };
    DataPropertyDesc root = { 1, DATA_VALUE_TYPE_STRUCT, 0, &layout };
    DataTableDesc    table = { 1, 0, 0, &root, 1, 24 };
    HDataStore       source = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(source, &table));
    uint64_t  names[] = { 10, 20, 30 };
    DataValue list_values[] = { String("nested"), Number(7) };
    DataValue values[] = { Number(0), String("shared"), List(list_values, 2) };
    DataValue object = Struct(names, values, 3);
    for (uint32_t r = 0; r < 3; ++r)
    {
        values[0] = Number(10 + r);
        DataId id;
        ASSERT_EQ(DATA_RESULT_OK, DataAddRow(source, 1, 0, &object, 1, &id, r));
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
    uint64_t          number_name = 10, string_name = 20;
    DataQueryProperty properties[] = { { 1, DATA_VALUE_TYPE_NUMBER, &number_name, 1 }, { 1, DATA_VALUE_TYPE_STRING, &string_name, 1 } };
    DataQueryDesc     desc = { 0, 0, 0, 0, properties, 2 };
    HDataQuery        query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    uint32_t number_field = DataQueryFindField(query, &properties[0]);
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
            uint32_t owner = (uint32_t)DataRowIterGetOwnerId(&rows) - 1;
            uint32_t component = (uint32_t)DataGetComponentId(store, id);
            ids[owner][component] = id;
            ASSERT_EQ(10.0 + component, *DataFieldGetNumber(&rows, number_field));
            ASSERT_EQ((uintptr_t)DataFieldGetNumber(&rows, number_field), (uintptr_t)DataFieldGetNumberMut(&rows, number_field));
            if (!owner)
                *DataFieldGetNumberMut(&rows, number_field) += 100;
            DataValue current, child, element;
            ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, id, 1, &current));
            ASSERT_EQ(DATA_RESULT_OK, DataGetStructProperty(&current.m_Value.m_Struct, 20, &child));
            ASSERT_STREQ("shared", child.m_Value.m_String);
            ASSERT_TRUE((uintptr_t)child.m_Value.m_String >= (uintptr_t)bytes && (uintptr_t)child.m_Value.m_String < (uintptr_t)(bytes + size));
            DataFieldIterator field = DataRowIterFields(&rows);
            while (DataFieldIterNext(&field) == DATA_RESULT_OK)
                if (!owner && DataFieldIterGetNameHash(&field) == 20)
                    ASSERT_EQ(DATA_RESULT_OK, DataFieldIterSetString(&field, "replacement"));
            ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, id, 1, &current));
            ASSERT_EQ(DATA_RESULT_OK, DataGetStructProperty(&current.m_Value.m_Struct, 30, &child));
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
    for (uint32_t owner = 0; owner < 2; ++owner)
        for (uint32_t component = owner ? 0 : 1; component < 3; ++component)
        {
            DataValue current, child;
            ASSERT_EQ(DATA_RESULT_OK, DataGetProperty(store, ids[owner][component], 1, &current));
            ASSERT_EQ(DATA_RESULT_OK, DataGetStructProperty(&current.m_Value.m_Struct, 10, &child));
            ASSERT_EQ(10.0 + component, child.m_Value.m_Number);
            ASSERT_EQ(DATA_RESULT_OK, DataGetStructProperty(&current.m_Value.m_Struct, 20, &child));
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
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(source, 1, 0, values, 2, &id, 11));
    values[0] = Number(20);
    ASSERT_EQ(DATA_RESULT_OK, DataAddRow(source, 1, 0, values, 2, &id, 12));
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
            ASSERT_EQ(DATA_RESULT_OK, DataSetPropertyNumber(store, ids[i][r], 10, 100 + i));
            ASSERT_EQ(DATA_RESULT_OK, DataSetPropertyString(store, ids[i][r], 20, i ? "second" : "first"));
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
    ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyNumber(store, ids[0][1], 10, &number));
    ASSERT_EQ(20.0, number);
    ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyString(store, ids[0][1], 20, &string));
    ASSERT_STREQ("shared", string);
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveBlob(first));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataGetPropertyNumber(store, ids[0][1], 10, &number));
    ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyNumber(store, reused, 10, &number));
    ASSERT_EQ(500.0, number);
    for (uint32_t r = 0; r < 2; ++r)
    {
        ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyNumber(store, ids[1][r], 10, &number));
        ASSERT_EQ(101.0, number);
        ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyString(store, ids[1][r], 20, &string));
        ASSERT_STREQ("second", string);
    }
    DataResetBlob(second);
    for (uint32_t r = 0; r < 2; ++r)
    {
        ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyNumber(store, ids[1][r], 10, &number));
        ASSERT_EQ(10.0 + r * 10, number);
        ASSERT_EQ((uint64_t)(11 + r), DataGetComponentId(store, ids[1][r]));
    }
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveBlob(second));
    ASSERT_EQ((uintptr_t)0, (uintptr_t)store->m_Pools);
    ASSERT_EQ(1u, store->m_Tables.Size());
    ASSERT_EQ(DATA_RESULT_OK, DataGetPropertyString(store, reused, 20, &string));
    ASSERT_STREQ("independent", string);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(source));
    delete[] bytes;
}
