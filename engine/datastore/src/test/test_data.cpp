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
#include "test_data_memory.h"

extern "C" int TestDataFromC(HDataStoreContext store, DataStoreId group);
extern "C" int TestDataStoreLifecycleFromC(void);
extern "C" int TestDataQuerySetupFromC(void);
extern "C" int TestDataNativeRowsFromC(void);
extern "C" int TestDataSoARowsFromC(void);
extern "C" int CreateReferenceRowsFromC(HDataStoreContext store, int soa, DataStoreId ids[2]);
extern "C" int CreateMixedListRowsFromC(HDataStoreContext store, int soa, DataStoreId ids[2]);

// Helpers for internal binding/value tests. Public C traversal is covered in test_data_c.c.
static DataStoreResult GetTestFieldNumber(const DataStoreIterator* batch, uint32_t row, uint32_t field, double* value)
{
    DataStoreValue  field_value;
    DataStoreResult result = DataStoreIterGetField(batch, row, field, &field_value);
    if (result == DATASTORE_RESULT_OK)
    {
        *value = field_value.m_Value.m_Number;
    }
    return result;
}

static DataStoreResult SetTestFieldNumber(const DataStoreIterator* batch, uint32_t row, uint32_t field, double value)
{
    DataStoreValue field_value = { DATASTORE_TYPE_NUMBER, { value } };
    return DataStoreIterSetField(batch, row, field, &field_value);
}

static DataStoreResult SetTestFieldBoolean(const DataStoreIterator* batch, uint32_t row, uint32_t field, uint8_t value)
{
    DataStoreValue field_value = {};
    field_value.m_Type = DATASTORE_TYPE_BOOLEAN;
    field_value.m_Value.m_Boolean = value;
    return DataStoreIterSetField(batch, row, field, &field_value);
}

static DataStoreResult GetTestFieldString(const DataStoreIterator* batch, uint32_t row, uint32_t field, const char** value)
{
    DataStoreValue  field_value;
    DataStoreResult result = DataStoreIterGetField(batch, row, field, &field_value);
    if (result == DATASTORE_RESULT_OK)
    {
        *value = field_value.m_Value.m_String;
    }
    return result;
}

static DataStoreResult SetTestFieldString(const DataStoreIterator* batch, uint32_t row, uint32_t field, const char* value)
{
    DataStoreValue field_value = {};
    field_value.m_Type = DATASTORE_TYPE_STRING;
    field_value.m_Value.m_String = value;
    return DataStoreIterSetField(batch, row, field, &field_value);
}

static DataStoreResult GetTestFieldVector3(const DataStoreIterator* batch, uint32_t row, uint32_t field, DataStoreVector3* value)
{
    DataStoreValue  field_value;
    DataStoreResult result = DataStoreIterGetField(batch, row, field, &field_value);
    if (result == DATASTORE_RESULT_OK)
    {
        memcpy(value->m_Values, field_value.m_Value.m_Vector3, sizeof(value->m_Values));
    }
    return result;
}

static DataStoreResult SetTestFieldVector3(const DataStoreIterator* batch, uint32_t row, uint32_t field, const DataStoreVector3* value)
{
    DataStoreValue field_value = { DATASTORE_TYPE_VECTOR3 };
    memcpy(field_value.m_Value.m_Vector3, value->m_Values, sizeof(value->m_Values));
    return DataStoreIterSetField(batch, row, field, &field_value);
}

static DataStoreValue Number(double v)
{
    DataStoreValue x = {};
    x.m_Type = DATASTORE_TYPE_NUMBER;
    x.m_Value.m_Number = v;
    return x;
}

static DataStoreValue String(const char* v)
{
    DataStoreValue x = {};
    x.m_Type = DATASTORE_TYPE_STRING;
    x.m_Value.m_String = v;
    return x;
}

static DataStoreValue Boolean(uint8_t v)
{
    DataStoreValue x = {};
    x.m_Type = DATASTORE_TYPE_BOOLEAN;
    x.m_Value.m_Boolean = v;
    return x;
}

static DataStoreValue Null()
{
    DataStoreValue x = {};
    x.m_Type = DATASTORE_TYPE_NULL;
    return x;
}

static DataStoreValue List(const DataStoreValue* values, uint32_t count)
{
    DataStoreValue x = {};
    x.m_Type = DATASTORE_TYPE_LIST;
    DataStoreList list = { values, count, 0, 0 };
    x.m_Value.m_List = list;
    return x;
}

static DataStoreValue Struct(const uint64_t* names, const DataStoreValueType* types, const DataStoreValueData* values, uint32_t count)
{
    DataStoreValue x = {};
    x.m_Type = DATASTORE_TYPE_STRUCT;
    DataStoreStruct object = { count, DATASTORE_STRUCT_ARRAY, { { names, types, values } } };
    x.m_Value.m_Struct = object;
    return x;
}

static DataStoreValue Vector3(float x, float y, float z)
{
    DataStoreValue v = {};
    v.m_Type = DATASTORE_TYPE_VECTOR3;
    v.m_Value.m_Vector3[0] = x;
    v.m_Value.m_Vector3[1] = y;
    v.m_Value.m_Vector3[2] = z;
    return v;
}

static DataStoreValue Matrix()
{
    DataStoreValue v = {};
    v.m_Type = DATASTORE_TYPE_MATRIX4;
    for (uint32_t i = 0; i < 16; ++i)
    {
        v.m_Value.m_Matrix4[i] = (float)i + 0.5f;
    }
    return v;
}

// Keep single-value fixtures concise; bulk-input tests use the separate arrays directly.
static DataStoreResult AddTestRow(HDataStoreContext store, uint64_t type, DataStoreId group, const DataStoreValue* values, uint32_t count, DataStoreId* out_id, uint64_t component_id)
{
    dmArray<DataStoreValueType> types;
    dmArray<DataStoreValueData> payloads;
    types.SetCapacity(count);
    types.SetSize(count);
    payloads.SetCapacity(count);
    payloads.SetSize(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        types[i] = values[i].m_Type;
        payloads[i] = values[i].m_Value;
    }
    return DataStoreRowAdd(store, type, group, types.Begin(), payloads.Begin(), count, out_id, component_id);
}

static DataStoreResult Register(HDataStoreContext store, uint64_t type, const uint64_t* tags, uint32_t tag_count)
{
    DataStoreFieldDesc meta[] = { { 10, DATASTORE_TYPE_NUMBER, 0 }, { 20, DATASTORE_TYPE_STRING, 8 } };
    DataStoreTableDesc desc = { type, tags, tag_count, meta, 2, 16, DATASTORE_RESET_RETAIN };
    return DataStoreTableAdd(store, &desc);
}

static DataStoreResult Add(HDataStoreContext store, uint64_t type, DataStoreId group, double number, const char* string, DataStoreId* id)
{
    DataStoreValue values[] = { Number(number), String(string) };
    return AddTestRow(store, type, group, values, 2, id, 0);
}

static DataStoreResult Load(HDataStoreContext store, const uint8_t* bytes, uint32_t size, HDataStoreBlobInstance* instance, DataStoreId group)
{
    HDataStoreBlob  blob;
    DataStoreResult result = DataStoreBlobLoad(bytes, size, &blob);
    if (result != DATASTORE_RESULT_OK)
    {
        return result;
    }
    result = DataStoreBlobAdd(store, blob, group, instance);
    DataStoreBlobDestroy(blob);
    return result;
}

static DataStoreId FirstId(HDataStoreContext store)
{
    DataStoreQueryDesc desc = {};
    HDataStoreQuery    query;
    if (DataStoreQueryCreate(store, &desc, &query) != DATASTORE_RESULT_OK)
    {
        return 0;
    }
    if (DataStoreQueryTryBegin(query) != DATASTORE_RESULT_OK)
    {
        DataStoreQueryDestroy(query);
        return 0;
    }
    DataStoreIterator it = DataStoreQueryIter(query);
    DataStoreId       id = DataStoreIterNext(&it) ? DataStoreIterGetId(&it, 0) : 0;
    DataStoreQueryEnd(query);
    DataStoreQueryDestroy(query);
    return id;
}

// Row insertion and string replacement own their bytes, including self-assignment.
TEST(Data, StringOwnershipAndSelfAssignment)
{
    HDataStoreContext store = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(store, 1, 0, 0));
    char text[] = "copied";
    DataStoreId id;
    ASSERT_EQ(DATASTORE_RESULT_OK, Add(store, 1, 0, 2.5, text, &id));
    text[0] = 'X';
    DataStoreValue out;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, id, 20, &out));
    ASSERT_STREQ("copied", out.m_Value.m_String);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, id, 20, &out));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, id, 20, &out));
    ASSERT_STREQ("copied", out.m_Value.m_String);
    DataStoreValue replacement = String(text);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, id, 20, &replacement));
    text[1] = 'Y';
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, id, 20, &out));
    ASSERT_STREQ("Xopied", out.m_Value.m_String);
    replacement = String(0);
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFieldSet(store, id, 20, &replacement));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Growth and swap removal preserve live IDs while reused slots invalidate old generations.
TEST(Data, GrowthSwapRemovalAndStaleIds)
{
    HDataStoreContext store = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(store, 1, 0, 0));
    DataStoreId ids[257];
    for (uint32_t i = 0; i < 257; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, Add(store, 1, 0, i, "value", &ids[i]));
    }
    DataStoreValue out;
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGet(store, 0, 10, &out));
    for (uint32_t i = 0; i < 257; i += 2)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[i]));
        ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreRowRemove(store, ids[i]));
    }
    for (uint32_t i = 1; i < 257; i += 2)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[i], 10, &out));
        ASSERT_EQ((double)i, out.m_Value.m_Number);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[i], 20, &out));
        ASSERT_STREQ("value", out.m_Value.m_String);
    }
    for (uint32_t i = 0; i < 257; i += 2)
    {
        DataStoreId replacement;
        ASSERT_EQ(DATASTORE_RESULT_OK, Add(store, 1, 0, -1, "new", &replacement));
        ASSERT_NE(ids[i], replacement);
        ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGet(store, ids[i], 10, &out));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableRemove(store, 1));
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(store, 1, 0, 0));
    DataStoreId replacement;
    ASSERT_EQ(DATASTORE_RESULT_OK, Add(store, 1, 0, 42, "reregistered", &replacement));
    for (uint32_t i = 0; i < 257; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGet(store, ids[i], 10, &out));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Live queries copy tag/group filters and track later table and row changes.
TEST(Data, LiveQueriesTagsGroupsAndCopiedDescriptors)
{
    HDataStoreContext  store = DataStoreCreate(0);
    uint64_t           tags[] = { 100, 200 };
    DataStoreId        owners[] = { NextTestGroupId(), NextTestGroupId() };
    DataStoreId        groups[] = { owners[0], owners[0], owners[1] };
    DataStoreQueryDesc desc = {
        { { tags[0], DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_TAG },
            { tags[1], DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_TAG } },
        groups,
        3,
        0
    };
    HDataStoreQuery query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    DataStoreIterator it = DataStoreQueryIter(query);
    ASSERT_FALSE(DataStoreIterNext(&it));
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(store, 1, tags, 2));
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(store, 2, tags, 1));
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(store, 3, tags, 2));
    tags[0] = 999;
    groups[0] = groups[1] = groups[2] = 999;
    DataStoreId id;
    for (uint64_t type = 1; type <= 3; ++type)
    {
        const DataStoreId row_groups[] = { owners[0], owners[0], 0, owners[1], 0, owners[0] };
        for (uint32_t row = 0; row < 6; ++row)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, Add(store, type, row_groups[row], row, "test", &id));
        }
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    it = DataStoreQueryIter(query);
    uint32_t total = 0;
    while (DataStoreIterNext(&it))
    {
        ASSERT_NE((uint64_t)2, DataStoreIterGetType(&it));
        for (uint32_t row = 0; row < DataStoreIterGetCount(&it); ++row)
        {
            DataStoreId group = DataStoreIterGetGroupId(&it, row);
            ASSERT_TRUE(group == owners[0] || group == owners[1]);
            ++total;
        }
    }
    ASSERT_FALSE(DataStoreIterNext(&it));
    ASSERT_EQ(8u, total);
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableRemove(store, 1));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableRemove(store, 3));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    it = DataStoreQueryIter(query);
    ASSERT_FALSE(DataStoreIterNext(&it));
    uint64_t new_tags[] = { 100, 200 };
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(store, 1, new_tags, 2));
    ASSERT_EQ(DATASTORE_RESULT_OK, Add(store, 1, owners[0], 1, "new", &id));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    it = DataStoreQueryIter(query);
    ASSERT_TRUE(DataStoreIterNext(&it));
    ASSERT_EQ(1u, DataStoreIterGetCount(&it));
    ASSERT_EQ(id, DataStoreIterGetId(&it, 0));
    DataStoreQueryEnd(query);
    DataStoreQueryDestroy(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Serial iteration reserves declared writes and blocks structural changes until End.
TEST(Data, QueryReservationBlocksMutations)
{
    HDataStoreContext store = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(store, 1, 0, 0));
    DataStoreId id;
    ASSERT_EQ(DATASTORE_RESULT_OK, Add(store, 1, 0, 1, "one", &id));
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, bytes, size, &size));
    HDataStoreBlob blob;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
    HDataStoreBlobInstance instance;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(store, blob, 0, &instance));
    DataStoreQueryItem field = { 10, DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryDesc desc = { { field }, 0, 0, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    DataStoreFieldId handle = DataStoreQueryFindField(query, &field);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreQueryTryBegin(query));
    DataStoreIterator batch = DataStoreQueryIter(query);
    ASSERT_TRUE(DataStoreIterNext(&batch));
    DataStoreRowIterator rows = DataStoreIterRows(&batch);
    ASSERT_TRUE(DataStoreRowIterNext(&rows));
    DataStoreId            unchanged = 123;
    HDataStoreBlobInstance unchanged_instance = instance;
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, Register(store, 2, 0, 0));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, Add(store, 1, 0, 2, "two", &unchanged));
    DataStoreValue     values[] = { Number(2), String("two") };
    DataStoreValueType types[] = { DATASTORE_TYPE_NUMBER, DATASTORE_TYPE_STRING };
    DataStoreValueData payloads[] = { values[0].m_Value, values[1].m_Value };
    DataStoreRowDesc   add = { 0, types, payloads, 2 };
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreRowAddBatch(store, 1, &add, 1, &unchanged));
    ASSERT_EQ((DataStoreId)123, unchanged);
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreRowRemove(store, id));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreTableRemove(store, 1));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreBlobAdd(store, blob, 0, &unchanged_instance));
    ASSERT_EQ((uintptr_t)instance, (uintptr_t)unchanged_instance);
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreBlobRemove(instance));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreDestroy(store));
    *DataStoreRowIterGetNumberMut(&rows, handle) = 9;
    ASSERT_EQ(9.0, *DataStoreRowIterGetNumber(&rows, handle));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreRowReset(store, id));
    ASSERT_EQ(9.0, *DataStoreRowIterGetNumber(&rows, handle));
    // An early traversal exit must release the reservation before mutation.
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, id));
    double number;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, id, 10, &number));
    ASSERT_EQ(1.0, number);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, id));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableRemove(store, 1));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobRemove(instance));
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(store, 2, 0, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, Add(store, 2, 0, 2, "two", &id));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(store, blob, 0, &instance));
    DataStoreQueryDestroy(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    DataStoreBlobDestroy(blob);
    delete[] bytes;
}

// Nested query iteration and writes leave each iterator's batch state independent.
TEST(Data, NestedQueriesAndWritesPreserveBatches)
{
    HDataStoreContext store = DataStoreCreate(0);
    DataStoreId       group = NextTestGroupId();
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(store, 1, 0, 0));
    DataStoreFieldDesc meta[] = { { 10, DATASTORE_TYPE_NUMBER, 8 }, { 20, DATASTORE_TYPE_STRING, 24 } };
    DataStoreTableDesc table = { 2, 0, 0, meta, 2, 32 };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    DataStoreId ids[2][4];
    for (uint32_t t = 0; t < 2; ++t)
    {
        for (uint32_t r = 0; r < 4; ++r)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, Add(store, t + 1, r == 1 ? 0 : group, t * 10 + r, "base", &ids[t][r]));
        }
    }

    DataStoreQueryItem fields[] = {
        { 10, DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD },
        { 20, DATASTORE_TYPE_STRING, 0, DATASTORE_QUERY_ITEM_FIELD }
    };
    DataStoreQueryDesc desc = { { fields[0], fields[1] }, &group, 1, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    // A second query and iterator must not replace the first iterator's batch state.
    DataStoreQueryDesc other_desc = { { fields[1] }, &group, 1, 0 };
    HDataStoreQuery    other_query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &other_desc, &other_query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(other_query));
    DataStoreIterator it      = DataStoreQueryIter(query);
    uint32_t          visited = 0, batches = 0;
    while (DataStoreIterNext(&it))
    {
        ++batches;
        DataStoreIterator other = DataStoreQueryIter(other_query);
        while (DataStoreIterNext(&other))
        {
            const char* string;
            ASSERT_EQ(DATASTORE_RESULT_OK, GetTestFieldString(&other, 0, 0, &string));
            ASSERT_STREQ("base", string);
        }
        uint32_t t = (uint32_t)DataStoreIterGetType(&it) - 1;
        for (uint32_t row = 0; row < DataStoreIterGetCount(&it); ++row)
        {
            DataStoreId id = DataStoreIterGetId(&it, row);
            uint32_t    r  = 0;
            while (r < 4 && ids[t][r] != id)
            {
                ++r;
            }
            ASSERT_TRUE(r < 4 && r != 1);
            ASSERT_EQ(group, DataStoreIterGetGroupId(&it, row));
            double number = -1;
            ASSERT_EQ(DATASTORE_RESULT_OK, GetTestFieldNumber(&it, row, 0, &number));
            ASSERT_EQ((double)(t * 10 + r), number);
            ASSERT_EQ(DATASTORE_RESULT_OK, SetTestFieldNumber(&it, row, 0, 99));
            ASSERT_EQ(DATASTORE_RESULT_OK, GetTestFieldNumber(&it, row, 0, &number));
            ASSERT_EQ(99.0, number);
            ++visited;
        }
    }
    ASSERT_EQ(6u, visited);
    ASSERT_EQ(4u, batches);
    double number = -1;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, GetTestFieldNumber(&it, 0, 0, &number));
    ASSERT_EQ(-1.0, number);

    DataStoreQueryEnd(other_query);
    DataStoreQueryEnd(query);
    DataStoreQueryDestroy(other_query);
    DataStoreQueryDestroy(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Bound field handles match name/type filters across different native row layouts.
TEST(Data, BoundFieldQueries)
{
    HDataStoreContext  store = DataStoreCreate(0);
    uint64_t           tag = 8;
    DataStoreId        group = NextTestGroupId();
    DataStoreQueryItem fields[] = {
        { 20, DATASTORE_TYPE_STRING, 0, DATASTORE_QUERY_ITEM_FIELD },
        { 10, DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD }
    };
    DataStoreQueryDesc desc = { { fields[0], fields[1], { tag, DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_TAG } }, &group, 1, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    fields[0].m_Hash = 999; // Query owns the original filter/binding order.
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(store, 1, &tag, 1));
    DataStoreFieldDesc swapped[] = { { 20, DATASTORE_TYPE_STRING, 0 }, { 10, DATASTORE_TYPE_NUMBER, 8 } };
    DataStoreTableDesc table = { 2, &tag, 1, swapped, 2, 16, DATASTORE_RESET_RETAIN };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    DataStoreFieldDesc wrong[] = { { 10, DATASTORE_TYPE_STRING, 0 }, { 20, DATASTORE_TYPE_STRING, 8 } };
    table.m_Type = 3;
    table.m_Fields = wrong;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    table.m_Type = 4;
    table.m_Fields = swapped;
    table.m_FieldCount = 1;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table)); // Missing number.
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(store, 5, 0, 0));             // Missing tag.
    DataStoreId ids[6];
    ASSERT_EQ(DATASTORE_RESULT_OK, Add(store, 1, group, 1, "one", &ids[0]));
    ASSERT_EQ(DATASTORE_RESULT_OK, Add(store, 1, 0, 99, "other group", &ids[1]));
    DataStoreValue values[] = { String("two"), Number(2) };
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(store, 2, group, values, 2, &ids[2], 0));
    values[1] = String("wrong kind");
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(store, 3, group, values, 2, &ids[3], 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(store, 4, group, values, 1, &ids[4], 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, Add(store, 5, group, 3, "no tag", &ids[5]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    DataStoreIterator it = DataStoreQueryIter(query);
    DataStoreValue    out = Number(123);
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreIterGetField(&it, 0, 0, &out));
    uint32_t total = 0;
    while (DataStoreIterNext(&it))
    {
        ASSERT_EQ(1u, DataStoreIterGetCount(&it));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreIterGetField(&it, 0, 1, &out));
        ASSERT_EQ((double)DataStoreIterGetType(&it), out.m_Value.m_Number);
        DataStoreValue changed = Number(44);
        ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreIterSetField(&it, 0, 0, &changed));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreIterSetField(&it, 0, 1, &changed));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreIterGetFieldByHash(&it, 0, 10, &out));
        ASSERT_EQ(44.0, out.m_Value.m_Number);
        ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreFieldReset(store, DataStoreIterGetId(&it, 0), 10));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreIterGetField(&it, 0, 0, &out));
        ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreIterSetField(&it, 0, 0, &out));
        ASSERT_STREQ(DataStoreIterGetType(&it) == 1 ? "one" : "two", out.m_Value.m_String);
        ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreIterGetField(&it, 1, 0, &out));
        ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreIterGetField(&it, 0, 2, &out));
        ++total;
    }
    ASSERT_EQ(2u, total);
    it = DataStoreQueryIter(query);
    ASSERT_TRUE(DataStoreIterNext(&it));
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableReset(store, 2));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableRemove(store, 1));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    it = DataStoreQueryIter(query);
    ASSERT_TRUE(DataStoreIterNext(&it));
    ASSERT_EQ((uint64_t)2, DataStoreIterGetType(&it));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreIterGetField(&it, 0, 1, &out));
    ASSERT_EQ(2.0, out.m_Value.m_Number);
    DataStoreQueryEnd(query);
    DataStoreQueryDestroy(query);
    desc.m_Items[0].m_Type = (DataStoreValueType)99;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreQueryCreate(store, &desc, &query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Bound queries read packed fields and stop matching after their blob is unloaded.
TEST(Data, BoundPackedFields)
{
    HDataStoreContext source = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(source, 1, 0, 0));
    DataStoreId id;
    ASSERT_EQ(DATASTORE_RESULT_OK, Add(source, 1, 0, 5, "packed", &id));
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, size, &size));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(source));
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreQueryItem fields[] = {
        { 10, DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_FIELD },
        { 20, DATASTORE_TYPE_STRING, 0, DATASTORE_QUERY_ITEM_FIELD }
    };
    DataStoreQueryDesc desc = { { fields[0], fields[1] }, 0, 0, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    HDataStoreBlobInstance instance;
    ASSERT_EQ(DATASTORE_RESULT_OK, Load(store, bytes, size, &instance, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    DataStoreIterator it = DataStoreQueryIter(query);
    ASSERT_TRUE(DataStoreIterNext(&it));
    DataStoreValue value;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreIterGetField(&it, 0, 1, &value));
    ASSERT_STREQ("packed", value.m_Value.m_String);
    id = DataStoreIterGetId(&it, 0);
    DataStoreQueryEnd(query);
    value = String("override");
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, id, 20, &value));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    it = DataStoreQueryIter(query);
    ASSERT_TRUE(DataStoreIterNext(&it));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreIterGetField(&it, 0, 1, &value));
    ASSERT_STREQ("override", value.m_Value.m_String);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreIterGetField(&it, 0, 0, &value));
    ASSERT_EQ(5.0, value.m_Value.m_Number);
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobReset(instance));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    it = DataStoreQueryIter(query);
    ASSERT_TRUE(DataStoreIterNext(&it));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreIterGetField(&it, 0, 1, &value));
    ASSERT_STREQ("packed", value.m_Value.m_String);
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobRemove(instance));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    it = DataStoreQueryIter(query);
    ASSERT_FALSE(DataStoreIterNext(&it));
    DataStoreQueryEnd(query);
    DataStoreQueryDestroy(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    delete[] bytes;
}

// Rows share copied metadata and fixed layouts while reset restores retained defaults.
TEST(Data, SharedMetadataAndDenseRows)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreFieldDesc meta[] = { { 10, DATASTORE_TYPE_VECTOR3, 0 }, { 20, DATASTORE_TYPE_NUMBER, 16 }, { 30, DATASTORE_TYPE_NUMBER, 24 } };
    DataStoreTableDesc desc = { 1, 0, 0, meta, 3, 32, DATASTORE_RESET_RETAIN };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &desc));
    meta[0].m_Field = 999; // The construction descriptor is copied once.
    DataStoreValue values[] = { Vector3(1, 2, 3), Number(4), Number(10) };
    DataStoreId    ids[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(store, 1, 0, values, 3, &ids[i], 0));
    }
    DataStoreTable* table = store->m_Tables[0];
    ASSERT_EQ(3u, table->m_Owned->m_Fields.Size());
    ASSERT_EQ(32u, table->m_RowStride); // No per-row names or kind tags in value storage.
    ASSERT_EQ(0, memcmp(GetNativeDefaultRow(table, 0), GetNativeDefaultRow(table, 1), 32));
    ASSERT_EQ((uintptr_t)0, (uintptr_t)table->m_Owned->m_BaseValues);
    ASSERT_EQ((uintptr_t)0, (uintptr_t)table->m_Payloads);
    DataStoreValue out;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[1], 10, &out));
    ASSERT_EQ(2.0f, out.m_Value.m_Vector3[1]);
    DataStoreValue changed = Vector3(0.25f, 0.5f, 0.75f);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, ids[1], 10, &changed));
    ASSERT_EQ(3u, table->m_Rows.Size());
    ASSERT_EQ(0, memcmp(GetRowBytes(table, 0), GetNativeDefaultRow(table, 0), 32));
    ASSERT_NE(0, memcmp(GetRowBytes(table, 1), GetNativeDefaultRow(table, 1), 32));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[1], 20, &out));
    ASSERT_EQ(4.0, out.m_Value.m_Number); // Updating color preserves the other mutable fields.
    changed = Number(3);
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFieldSet(store, ids[1], 10, &changed));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[1], 10, &out));
    ASSERT_EQ(0.5f, out.m_Value.m_Vector3[1]);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldReset(store, ids[1], 10));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[1], 10, &out));
    ASSERT_EQ(2.0f, out.m_Value.m_Vector3[1]);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableReset(store, 1));
    ASSERT_EQ((uintptr_t)0, (uintptr_t)table->m_Payloads);
    for (uint32_t r = 0; r < 3; ++r)
    {
        ASSERT_EQ(0, memcmp(GetRowBytes(table, r), GetNativeDefaultRow(table, r), 32));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Invalid layouts and decoded batches publish no rows and preserve output IDs.
TEST(Data, MetadataAndBulkValidation)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreFieldDesc meta[] = { { 1, DATASTORE_TYPE_NUMBER, 0 }, { 2, DATASTORE_TYPE_BOOLEAN, 8 } };
    DataStoreTableDesc desc = { 1, 0, 0, meta, 2, 16 };
    meta[1].m_Offset = 7;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableAdd(store, &desc));
    meta[1].m_Offset = 16;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableAdd(store, &desc));
    meta[1].m_Offset = 8;
    meta[1].m_Field = 1;
    ASSERT_EQ(DATASTORE_RESULT_ALREADY_EXISTS, DataStoreTableAdd(store, &desc));
    meta[1].m_Field = 2;
    meta[1].m_Type = (DataStoreValueType)99;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableAdd(store, &desc));
    meta[1].m_Type = DATASTORE_TYPE_BOOLEAN;
    desc.m_ResetPolicy = (DataStoreResetPolicy) - 1;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableAdd(store, &desc));
    desc.m_ResetPolicy = (DataStoreResetPolicy)2;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableAdd(store, &desc));
    desc.m_ResetPolicy = DATASTORE_RESET_NONE;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &desc));
    ASSERT_EQ(DATASTORE_RESULT_ALREADY_EXISTS, DataStoreTableAdd(store, &desc));
    DataStoreValueType types[] = { DATASTORE_TYPE_NUMBER, DATASTORE_TYPE_BOOLEAN };
    DataStoreValueData values[4] = {};
    values[0].m_Number = 4;
    values[1].m_Boolean = 1;
    values[2].m_Number = 5;
    values[3].m_Boolean = 2;
    DataStoreRowDesc rows[] = {
        { 0, types, values, 2, 7 },
        { 0, types, values + 2, 2, 8 }
    };
    DataStoreId ids[] = { 123, 456 };
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowAddBatch(store, 1, rows, 2, ids));
    ASSERT_EQ((DataStoreId)123, ids[0]);
    ASSERT_EQ((DataStoreId)456, ids[1]);
    ASSERT_EQ(0u, store->m_Tables[0]->m_Rows.Size());
    values[3].m_Boolean = 0;
    rows[1].m_ValueCount = 1;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowAddBatch(store, 1, rows, 2, ids));
    rows[1].m_ValueCount = 2;
    types[0] = DATASTORE_TYPE_STRING;
    values[2].m_String = "wrong kind";
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowAddBatch(store, 1, rows, 2, ids));
    types[0] = DATASTORE_TYPE_NUMBER;
    values[2].m_Number = 5;
    rows[1].m_Types = 0;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowAddBatch(store, 1, rows, 2, ids));
    rows[1].m_Types = types;
    rows[1].m_Values = 0;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowAddBatch(store, 1, rows, 2, ids));
    rows[1].m_Values = values + 2;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowAddBatch(store, 1, rows, 2, ids));
    ASSERT_EQ(2u, store->m_Tables[0]->m_Rows.Size());
    values[0].m_Number = 99;
    types[0] = DATASTORE_TYPE_STRING; // Insertion no longer depends on either input array.
    DataStoreValue out = Number(123);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[0], 1, &out));
    ASSERT_EQ(4.0, out.m_Value.m_Number);
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGet(store, ids[0], 999, &out));
    ASSERT_EQ(4.0, out.m_Value.m_Number);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowAddBatch(store, 1, 0, 0, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowAddBatch(0, 999, 0, 0, 0));
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowAddBatch(store, 1, 0, 1, ids));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Bulk appends reuse slots without changing existing rows, padding or retained defaults.
TEST(Data, BulkAppendPreservesDefaultsAndReusesSlots)
{
    DataStoreFieldDesc fields[] = {
        { 10, DATASTORE_TYPE_NUMBER, 0 },
        { 20, DATASTORE_TYPE_BOOLEAN, 8 },
        { 30, DATASTORE_TYPE_VECTOR3, 12 },
        { 40, DATASTORE_TYPE_VECTOR4, 24 },
        { 50, DATASTORE_TYPE_MATRIX4, 40 }
    };
    const DataStoreValueType types[] = { DATASTORE_TYPE_NUMBER, DATASTORE_TYPE_BOOLEAN, DATASTORE_TYPE_VECTOR3, DATASTORE_TYPE_VECTOR4, DATASTORE_TYPE_MATRIX4 };
    DataStoreTableDesc       table = { 1, 0, 0, fields, 5, 104, DATASTORE_RESET_RETAIN };
    HDataStoreContext        store = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    DataStoreValueData values[3][5] = {};
    DataStoreRowDesc   rows[3] = {};
    for (uint32_t r = 0; r < 3; ++r)
    {
        values[r][0].m_Number = 100 + r;
        values[r][1].m_Boolean = r & 1;
        for (uint32_t f = 2; f < 5; ++f)
        {
            for (uint32_t v = 0; v < DataStoreTypeSize(types[f]) / sizeof(float); ++v)
            {
                values[r][f].m_Matrix4[v] = (float)(100 * r + 10 * f + v);
            }
        }
        rows[r] = { NextTestGroupId(), types, values[r], 5, 30 + r };
    }
    DataStoreId old_ids[2], ids[] = { 123, 456, 789 };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowAddBatch(store, 1, rows, 2, old_ids));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, old_ids[0]));
    values[2][1].m_Boolean = 2;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowAddBatch(store, 1, rows, 3, ids));
    ASSERT_EQ((DataStoreId)123, ids[0]);
    ASSERT_EQ((DataStoreId)789, ids[2]);
    ASSERT_EQ(1u, store->m_Tables[0]->m_Rows.Size());
    values[2][1].m_Boolean = 0;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowAddBatch(store, 1, rows, 3, ids));
    ASSERT_EQ((uint32_t)old_ids[0], (uint32_t)ids[0]); // Reused slot with a new generation, followed by fresh slots.
    ASSERT_NE(old_ids[0], ids[0]);
    ASSERT_EQ(4u, store->m_Slots.Size());
    double number;
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGetNumber(store, old_ids[0], 10, &number));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, old_ids[1], 10, &number));
    ASSERT_EQ(101.0, number);
    for (uint32_t r = 0; r < 3; ++r)
    {
        const DataStoreSlot* slot = FindSlot(store, ids[r]);
        ASSERT_EQ(rows[r].m_Group, slot->m_Table->m_Rows[slot->m_Row].m_Group);
        ASSERT_EQ((uint64_t)(30 + r), DataStoreRowGetComponentId(store, ids[r]));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetNumber(store, ids[r], 10, -1));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[r]));
        for (uint32_t f = 0; f < 5; ++f)
        {
            DataStoreValue value;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[r], fields[f].m_Field, &value));
            ASSERT_EQ(0, memcmp(&values[r][f], &value.m_Value, DataStoreTypeSize(types[f])));
        }
        const uint8_t* bytes = GetFieldBytes(slot->m_Table, &slot->m_Table->m_Rows[slot->m_Row], 0);
        ASSERT_EQ(0, bytes[9] | bytes[10] | bytes[11]); // Padding is initialized for every appended row.
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Inline batches validate all members before publishing, regardless of input field order.
TEST(Data, InlineBulkValidationIsAtomicAcrossMemberOrders)
{
    DataStoreFieldDesc members[] = {
        { 10, DATASTORE_TYPE_VECTOR3, 0, 0, "10" },
        { 20, DATASTORE_TYPE_BOOLEAN, 12, 0, "20" }
    };
    DataStoreStructDesc layout = { members, 2, 16 };
    DataStoreFieldDesc  field = { 1, DATASTORE_TYPE_STRUCT, 0, &layout, "1" };
    DataStoreTableDesc  table = { 1, 0, 0, &field, 1, 16 };
    HDataStoreContext   store = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    uint64_t           names[] = { 10, 20, 20, 10 };
    DataStoreValueType types[] = { DATASTORE_TYPE_VECTOR3, DATASTORE_TYPE_BOOLEAN, DATASTORE_TYPE_BOOLEAN, DATASTORE_TYPE_VECTOR3 };
    DataStoreValueData children[4] = {};
    children[0].m_Vector3[0] = 1;
    children[0].m_Vector3[1] = 2;
    children[0].m_Vector3[2] = 3;
    children[1].m_Boolean = 1;
    children[2].m_Boolean = 2;
    children[3].m_Vector3[0] = 4;
    children[3].m_Vector3[1] = 5;
    children[3].m_Vector3[2] = 6;
    DataStoreValueData values[2] = {};
    values[0].m_Struct = { 2, DATASTORE_STRUCT_ARRAY, { { names, types, children } } };
    values[1].m_Struct = { 2, DATASTORE_STRUCT_ARRAY, { { names + 2, types + 2, children + 2 } } };
    const DataStoreValueType kind = DATASTORE_TYPE_STRUCT;
    DataStoreRowDesc         rows[] = {
        { 0, &kind, values, 1 },
        { 0, &kind, values + 1, 1 }
    };
    DataStoreId ids[] = { 123, 456 };
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowAddBatch(store, 1, rows, 2, ids));
    ASSERT_EQ((DataStoreId)123, ids[0]);
    ASSERT_EQ((DataStoreId)456, ids[1]);
    ASSERT_EQ(0u, store->m_Tables[0]->m_Rows.Size());
    ASSERT_EQ(0u, store->m_Tables[0]->m_Rows.Size());
    children[2].m_Boolean = 0;
    types[3] = DATASTORE_TYPE_NUMBER;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowAddBatch(store, 1, rows, 2, ids));
    types[3] = DATASTORE_TYPE_VECTOR3;
    names[3] = 99;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowAddBatch(store, 1, rows, 2, ids));
    names[3] = 10;
    ASSERT_EQ((DataStoreId)123, ids[0]);
    ASSERT_EQ((DataStoreId)456, ids[1]);
    ASSERT_EQ(0u, store->m_Tables[0]->m_Rows.Size());
    ASSERT_EQ(0u, store->m_Tables[0]->m_Rows.Size());
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowAddBatch(store, 1, rows, 2, ids));
    for (uint32_t i = 0; i < 2; ++i)
    {
        DataStoreValue object, color, enabled;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[i], 1, &object));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&object.m_Value.m_Struct, 10, &color));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&object.m_Value.m_Struct, 20, &enabled));
        ASSERT_EQ(i ? 6.0f : 3.0f, color.m_Value.m_Vector3[2]);
        ASSERT_EQ(i ? 0 : 1, enabled.m_Value.m_Boolean);
    }
    // Borrowed inline views keep working through the general input path.
    HDataStoreContext copy = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(copy, &table));
    for (uint32_t i = 0; i < 2; ++i)
    {
        DataStoreValue view;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[i], 1, &view));
        values[i] = view.m_Value;
    }
    DataStoreId copied[2];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowAddBatch(copy, 1, rows, 2, copied));
    for (uint32_t i = 0; i < 2; ++i)
    {
        DataStoreValue object, color, enabled;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(copy, copied[i], 1, &object));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&object.m_Value.m_Struct, 10, &color));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&object.m_Value.m_Struct, 20, &enabled));
        ASSERT_EQ(i ? 6.0f : 3.0f, color.m_Value.m_Vector3[2]);
        ASSERT_EQ(i ? 0 : 1, enabled.m_Value.m_Boolean);
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(copy));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// The C API creates and destroys a datastore without C++ lifecycle helpers.
TEST(Data, CApiStoreLifecycle)
{
    ASSERT_EQ(0, TestDataStoreLifecycleFromC());
}

// The concise descriptor and field helper work from C without C++ initializer features.
TEST(Data, CApiQuerySetup)
{
    ASSERT_EQ(0, TestDataQuerySetupFromC());
}

// C callers read math and nested values consistently across blob loading and reset.
TEST(Data, CApiMathAndNestedViews)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreId        group = NextTestGroupId();
    DataStoreFieldDesc meta[] = { { 42, DATASTORE_TYPE_NUMBER, 0 }, { 43, DATASTORE_TYPE_VECTOR3, 8 }, { 44, DATASTORE_TYPE_VECTOR4, 20 }, { 45, DATASTORE_TYPE_MATRIX4, 36 }, { 46, DATASTORE_TYPE_STRUCT, 104 }, { 47, DATASTORE_TYPE_BOOLEAN, 112 }, { 48, DATASTORE_TYPE_STRING, 120 } };
    uint64_t           tag = 7;
    DataStoreTableDesc desc = { 1, &tag, 1, meta, 7, 128, DATASTORE_RESET_RETAIN };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &desc));
    DataStoreValue vec4 = {};
    vec4.m_Type = DATASTORE_TYPE_VECTOR4;
    uint64_t                 color_name = 10;
    const DataStoreValueType color_type = DATASTORE_TYPE_VECTOR3;
    DataStoreValueData       color      = {};
    color.m_Vector3[0] = 1.0f;
    color.m_Vector3[1] = 0.5f;
    color.m_Vector3[2] = 0.25f;
    DataStoreValue           values[] = { Number(17), Vector3(1, 1, 1), vec4, Matrix(), Struct(&color_name, &color_type, &color, 1), Boolean(0), String("initial") };
    DataStoreId              id;
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(store, 1, group, values, 7, &id, 0));
    ASSERT_EQ(0, TestDataFromC(store, group));
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, bytes, size, &size));
    HDataStoreContext      loaded = DataStoreCreate(0);
    HDataStoreBlobInstance instance;
    ASSERT_EQ(DATASTORE_RESULT_OK, Load(loaded, bytes, size, &instance, NextTestGroupId()));
    DataStoreMatrix4 loaded_matrix;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetMatrix4(loaded, FirstId(loaded), 45, &loaded_matrix));
    ASSERT_EQ(44.0f, loaded_matrix.m_Values[3 * 4 + 2]);
    DataStoreValue out;
    for (uint32_t i = 1; i <= 3; ++i)
    {
        DataStoreValue original;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, id, meta[i].m_Field, &original));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(loaded, FirstId(loaded), meta[i].m_Field, &out));
        ASSERT_EQ(original.m_Type, out.m_Type);
        ASSERT_EQ(0, memcmp(&original.m_Value, &out.m_Value, DataStoreTypeSize(original.m_Type)));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableReset(store, 1));
    for (uint32_t i = 1; i <= 3; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, id, meta[i].m_Field, &out));
        ASSERT_EQ(values[i].m_Type, out.m_Type);
        ASSERT_EQ(0, memcmp(&values[i].m_Value, &out.m_Value, DataStoreTypeSize(out.m_Type)));
    }
    ASSERT_EQ(0, TestDataFromC(store, group));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(loaded));
    delete[] bytes;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Typed math reads preserve float bits in native, packed and writable storage.
TEST(Data, AlignedMathReadsPreserveFloatBits)
{
    // Zero, negative zero, subnormal, infinities and a NaN with a payload.
    const uint32_t     bits[] = { 0, 0x80000000, 1, 0x7f800000, 0xff800000, 0x7fc01234, 0x3f800000, 0xbf800000, 0, 0x80000000, 1, 0x7f800000, 0xff800000, 0x7fc01234, 0x3f800000, 0xbf800000 };
    DataStoreFieldDesc meta[] = { { 10, DATASTORE_TYPE_VECTOR3, 0 }, { 20, DATASTORE_TYPE_VECTOR4, 12 }, { 30, DATASTORE_TYPE_MATRIX4, 28 } };
    DataStoreTableDesc desc = { 1, 0, 0, meta, 3, 92 };
    DataStoreValue     values[3] = {};
    for (uint32_t i = 0; i < 3; ++i)
    {
        values[i].m_Type = meta[i].m_Type;
        memcpy(&values[i].m_Value, bits, DataStoreTypeSize(meta[i].m_Type));
    }
    HDataStoreContext source = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(source, &desc));
    DataStoreId id;
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, 1, 0, values, 3, &id, 0));
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, size, &size));
    HDataStoreContext      loaded = DataStoreCreate(0);
    HDataStoreBlobInstance instance;
    ASSERT_EQ(DATASTORE_RESULT_OK, Load(loaded, bytes, size, &instance, 0));
    HDataStoreContext stores[] = { source, loaded };
    for (uint32_t mode = 0; mode < 2; ++mode)
    {
        HDataStoreContext store = stores[mode];
        DataStoreId       row = FirstId(store);
        // Read defaults, then the same bit patterns through override storage.
        for (uint32_t overridden = 0; overridden < 2; ++overridden)
        {
            for (uint32_t i = 0; i < 3; ++i)
            {
                if (overridden)
                {
                    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, row, meta[i].m_Field, &values[i]));
                }
                DataStoreValue value;
                ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, row, meta[i].m_Field, &value));
                ASSERT_EQ(meta[i].m_Type, value.m_Type);
                ASSERT_EQ(0, memcmp(bits, &value.m_Value, DataStoreTypeSize(value.m_Type)));
            }
            DataStoreVector3 vector3;
            DataStoreVector4 vector4;
            DataStoreMatrix4 matrix;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3(store, row, 10, &vector3));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector4(store, row, 20, &vector4));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetMatrix4(store, row, 30, &matrix));
            ASSERT_EQ(0, memcmp(bits, vector3.m_Values, sizeof(vector3.m_Values)));
            ASSERT_EQ(0, memcmp(bits, vector4.m_Values, sizeof(vector4.m_Values)));
            ASSERT_EQ(0, memcmp(bits, matrix.m_Values, sizeof(matrix.m_Values)));
            DataStoreQueryItem fields[] = {
                { 30, DATASTORE_TYPE_MATRIX4, 0, DATASTORE_QUERY_ITEM_FIELD },
                { 10, DATASTORE_TYPE_VECTOR3, 0, DATASTORE_QUERY_ITEM_FIELD },
                { 20, DATASTORE_TYPE_VECTOR4, 0, DATASTORE_QUERY_ITEM_FIELD }
            };
            DataStoreQueryDesc query_desc = { { fields[0], fields[1], fields[2] }, 0, 0, 0 };
            HDataStoreQuery    query;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &query_desc, &query));
            DataStoreFieldId matrix_field = DataStoreQueryFindField(query, &fields[0]);
            DataStoreFieldId vector3_field = DataStoreQueryFindField(query, &fields[1]);
            DataStoreFieldId vector4_field = DataStoreQueryFindField(query, &fields[2]);
            ASSERT_NE(UINT32_MAX, matrix_field);
            ASSERT_NE(UINT32_MAX, vector3_field);
            ASSERT_NE(UINT32_MAX, vector4_field);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
            DataStoreIterator it = DataStoreQueryIter(query);
            ASSERT_TRUE(DataStoreIterNext(&it));
            DataStoreRowIterator rows = DataStoreIterRows(&it);
            ASSERT_TRUE(DataStoreRowIterNext(&rows));
            ASSERT_EQ(0, memcmp(bits, DataStoreRowIterGetMatrix4(&rows, matrix_field), sizeof(DataStoreMatrix4)));
            ASSERT_EQ(0, memcmp(bits, DataStoreRowIterGetVector3(&rows, vector3_field), sizeof(DataStoreVector3)));
            ASSERT_EQ(0, memcmp(bits, DataStoreRowIterGetVector4(&rows, vector4_field), sizeof(DataStoreVector4)));
            DataStoreQueryEnd(query);
            DataStoreQueryDestroy(query);
        }
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(loaded));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(source));
    delete[] bytes;
}

// Row records and value rows use separate arrays; runtime group/slot indices
// must not leak into a blob, including when serializing an instantiated blob.
TEST(Data, BinaryGoldenSharedMetadata)
{
    // Two eight-byte rows share one 32-byte metadata entry.
    // clang-format off
    const uint8_t DM_ALIGNED(8) expected[] = {
        'D','F','D','S',1,0,0,0, 1,0,0,0,160,0,0,0, 24,0,0,0,0,0,0,0,
        1,0,0,0,0,0,0,0, 0,0,1,0,2,0,0,0, 8,0,0,0,1,0,0,0,
        10,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,8,0,0,0, 10,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0, 7,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0, 8,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0xf8,0x3f, 0,0,0,0,0,0,0,0xc0
    };
    // clang-format on
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreFieldDesc meta = { 10, DATASTORE_TYPE_NUMBER, 0 };
    DataStoreTableDesc desc = { 1, 0, 0, &meta, 1, 8 };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &desc));
    const DataStoreValueType types[] = { DATASTORE_TYPE_NUMBER };
    DataStoreValueData       values[] = { { 1.5 }, { -2 } };
    DataStoreId              ids[2];
    DataStoreId              group = NextTestGroupId();
    DataStoreRowDesc         rows[] = {
        { group, types, values, 1, 7 },
        { group, types, values + 1, 1, 8 }
    };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowAddBatch(store, 1, rows, 2, ids));
    uint8_t DM_ALIGNED(8) bytes[sizeof(expected)];
    uint32_t size = 0;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, 0, 0, &size));
    ASSERT_EQ(sizeof(expected), (size_t)size);
    memset(bytes, 0xff, sizeof(bytes));
    ASSERT_EQ(DATASTORE_RESULT_BUFFER_TOO_SMALL, DataStoreBlobWrite(store, bytes, size - 1, &size));
    for (uint32_t i = 0; i < sizeof(bytes); ++i)
    {
        ASSERT_EQ(0xff, bytes[i]);
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, bytes, size, &size));
    ASSERT_EQ(0, memcmp(expected, bytes, size));
    HDataStoreContext      loaded = DataStoreCreate(0);
    HDataStoreBlobInstance instance;
    ASSERT_EQ(DATASTORE_RESULT_OK, Load(loaded, expected, sizeof(expected), &instance, 0));
    ASSERT_EQ((uintptr_t)(expected + 144), (uintptr_t)(GetInstanceTable(instance, 0)->m_Blob + GetInstanceTable(instance, 0)->m_Offsets.m_Rows));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(loaded, bytes, size, &size));
    ASSERT_EQ(0, memcmp(expected, bytes, size));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(loaded));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Blob instances share immutable data while row changes and group lifetimes stay separate.
TEST(Data, BlobMutableRowsSharedMetadataAndGroupLifetime)
{
    HDataStoreContext source = DataStoreCreate(0);
    uint64_t          tag    = dmHashString64("light");
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(source, 1, &tag, 1));
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(source, 2, &tag, 1));
    DataStoreValue values[] = { Number(1), String("shared") };
    DataStoreId    id;
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, 1, 0, values, 2, &id, 11));
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, 1, 0, values, 2, &id, 12));
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, 2, 0, values, 2, &id, 13));
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, 0, 0, &size));
    uint8_t* memory = new uint8_t[size];
    uint8_t* bytes = memory;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, size, &size));
    uint8_t* unchanged = new uint8_t[size];
    memcpy(unchanged, bytes, size);
    HDataStoreBlob blob;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
    ASSERT_EQ((uintptr_t)bytes, (uintptr_t)blob->m_Data);
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreQueryDesc query_desc = { { { tag, DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_TAG }, { 20, DATASTORE_TYPE_STRING, 0, DATASTORE_QUERY_ITEM_FIELD } }, 0, 0, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &query_desc, &query));
    DataStoreId            groups[] = { NextTestGroupId(), NextTestGroupId() };
    HDataStoreBlobInstance first, second;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(store, blob, groups[0], &first));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(store, blob, groups[1], &second));
    ASSERT_EQ(2u, store->m_Tables.Size());
    ASSERT_EQ((uintptr_t)(GetInstanceTable(first, 0)->m_Blob + GetInstanceTable(first, 0)->m_Offsets.m_Table), (uintptr_t)(GetInstanceTable(second, 0)->m_Blob + GetInstanceTable(second, 0)->m_Offsets.m_Table));
    ASSERT_EQ((uintptr_t)(GetInstanceTable(first, 0)->m_Blob + GetInstanceTable(first, 0)->m_Offsets.m_Rows), (uintptr_t)(GetInstanceTable(second, 0)->m_Blob + GetInstanceTable(second, 0)->m_Offsets.m_Rows));
    ASSERT_EQ((uintptr_t)GetInstanceTable(first, 0), (uintptr_t)GetInstanceTable(second, 0));
    ASSERT_EQ(4u, GetInstanceTable(first, 0)->m_Rows.Size());
    for (uint32_t t = 0; t < store->m_Tables.Size(); ++t)
    {
        DataStoreTable* table = store->m_Tables[t];
        ASSERT_TRUE(table->m_Pool != 0);
        ASSERT_EQ((uintptr_t)bytes, (uintptr_t)table->m_Blob);
        ASSERT_EQ((uintptr_t)0, (uintptr_t)table->m_Payloads);
        uint32_t            source_count = table->m_Rows.Size() / 2;
        const DataStoreRow* records      = (const DataStoreRow*)(bytes + table->m_Offsets.m_Rows) - source_count;
        for (uint32_t r = 0; r < table->m_Rows.Size(); ++r)
        {
            const DataStoreRow& row = table->m_Rows[r];
            ASSERT_EQ(groups[r < source_count ? 0 : 1], row.m_Group);
            ASSERT_EQ(records[r % source_count].m_ComponentId, row.m_ComponentId);
            ASSERT_EQ(r % source_count, row.m_BaseIndex);
            ASSERT_EQ(r < source_count ? first->m_Index : second->m_Index, row.m_InstanceIndex);
            ASSERT_EQ(r, store->m_Slots[row.m_Slot].m_Row);
            ASSERT_EQ((uintptr_t)table, (uintptr_t)store->m_Slots[row.m_Slot].m_Table);
        }
    }
    DataStoreId ids[2][3] = {};
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    DataStoreIterator it     = DataStoreQueryIter(query);
    uint32_t          total  = 0;
    uintptr_t         string = (uintptr_t)(bytes + ReadDataInteger(bytes + 12, 4));
    while (DataStoreIterNext(&it))
    {
        for (uint32_t r = 0; r < DataStoreIterGetCount(&it); ++r)
        {
            DataStoreId row       = DataStoreIterGetId(&it, r);
            uint64_t    component = DataStoreRowGetComponentId(store, row);
            uint32_t    group     = DataStoreIterGetGroupId(&it, r) == groups[0] ? 0 : 1;
            ASSERT_TRUE(component >= 11 && component <= 13);
            ids[group][component - 11] = row;
            DataStoreValue out;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreIterGetFieldByHash(&it, r, 20, &out));
            ASSERT_EQ(string, (uintptr_t)out.m_Value.m_String);
            ++total;
        }
    }
    DataStoreQueryEnd(query);
    ASSERT_EQ(6u, total);
    ASSERT_NE(ids[0][0], ids[1][0]);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(source));
    DataStoreBlobDestroy(blob);
    DataStoreValue change = String("override");
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, ids[0][0], 20, &change));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, ids[0][1], 20, &change));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, ids[0][2], 20, &change));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, ids[1][0], 20, &change));
    DataStoreValue out;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[0][0]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[0][0], 20, &out));
    ASSERT_EQ(string, (uintptr_t)out.m_Value.m_String);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[0][1], 20, &out));
    ASSERT_STREQ("override", out.m_Value.m_String);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[0][2], 20, &out));
    ASSERT_STREQ("override", out.m_Value.m_String);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[1][0], 20, &out));
    ASSERT_STREQ("override", out.m_Value.m_String);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, ids[0][0], 20, &change));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[0][1]));
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreRowReset(store, ids[0][1]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobReset(first));
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGet(store, ids[0][1], 20, &out));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[0][0], 20, &out));
    ASSERT_EQ(string, (uintptr_t)out.m_Value.m_String);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[0][2], 20, &out));
    ASSERT_EQ(string, (uintptr_t)out.m_Value.m_String);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[1][0], 20, &out));
    ASSERT_STREQ("override", out.m_Value.m_String);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobRemove(first));
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGet(store, ids[0][i], 20, &out));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[1][i], 20, &out));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, ids[1][0], 20, &out));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[1][0], 20, &out));
    ASSERT_NE(string, (uintptr_t)out.m_Value.m_String);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldReset(store, ids[1][0], 20));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[1][0], 20, &out));
    ASSERT_EQ(string, (uintptr_t)out.m_Value.m_String);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobRemove(second));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    it = DataStoreQueryIter(query);
    ASSERT_FALSE(DataStoreIterNext(&it));
    DataStoreQueryEnd(query);
    DataStoreQueryDestroy(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    ASSERT_EQ(0, memcmp(unchanged, bytes, size));
    delete[] unchanged;
    delete[] memory;
}

// Nested views cover every value kind and return to borrowed blob defaults on reset.
TEST(Data, NestedViewsAllKindsAndReset)
{
    DataStoreValue vec4 = {};
    vec4.m_Type = DATASTORE_TYPE_VECTOR4;
    vec4.m_Value.m_Vector4[3] = 8.0f;
    DataStoreValue           elements[] = { Null(), Number(1.5), Boolean(1), String("shared"), Vector3(1, 2, 3), vec4, Matrix(), List(0, 0), Struct(0, 0, 0, 0) };
    const DataStoreValueType types[] = { DATASTORE_TYPE_LIST, DATASTORE_TYPE_STRING };
    DataStoreValueData       fields[] = { List(elements, 9).m_Value, String("shared").m_Value };
    uint64_t                 names[] = { 10, 20 };
    DataStoreValue           value = Struct(names, types, fields, 2);
    DataStoreFieldDesc       meta = { 1, DATASTORE_TYPE_STRUCT, 0 };
    DataStoreTableDesc       desc = { 1, 0, 0, &meta, 1, 8 };
    HDataStoreContext        source = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(source, &desc));
    DataStoreId id;
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, 1, 0, &value, 1, &id, 0));
    elements[1] = Number(99);
    names[0] = 99;
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    uint8_t* roundtrip = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, size, &size));
    HDataStoreContext      store = DataStoreCreate(0);
    HDataStoreBlobInstance instance;
    ASSERT_EQ(DATASTORE_RESULT_OK, Load(store, bytes, size, &instance, 0));
    id = FirstId(store);
    DataStoreValue object, list, child, shared;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, id, 1, &object));
    ASSERT_EQ((uintptr_t)(bytes), (uintptr_t)object.m_Value.m_Struct.m_View.m_Buffer);
    ASSERT_EQ(DATASTORE_STRUCT_PACKED, object.m_Value.m_Struct.m_Source);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&object.m_Value.m_Struct, 10, &list));
    ASSERT_EQ(9u, list.m_Value.m_List.m_Count);
    for (uint32_t i = 0; i < 9; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(&list.m_Value.m_List, i, &child));
        ASSERT_EQ(elements[i].m_Type, child.m_Type);
        if (i == 1)
        {
            ASSERT_EQ(1.5, child.m_Value.m_Number);
        }
        if (i == 4)
        {
            ASSERT_EQ(3.0f, child.m_Value.m_Vector3[2]);
        }
        if (i == 5)
        {
            ASSERT_EQ(8.0f, child.m_Value.m_Vector4[3]);
        }
        if (i == 6)
        {
            for (uint32_t j = 0; j < 16; ++j)
            {
                ASSERT_EQ((float)j + 0.5f, child.m_Value.m_Matrix4[j]);
            }
        }
    }
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreListGetValue(&list.m_Value.m_List, 9, &child));
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreStructGetField(&object.m_Value.m_Struct, 99, &child));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(&list.m_Value.m_List, 3, &child));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&object.m_Value.m_Struct, 20, &shared));
    ASSERT_EQ((uintptr_t)child.m_Value.m_String, (uintptr_t)shared.m_Value.m_String);
    ASSERT_TRUE((const uint8_t*)shared.m_Value.m_String >= bytes && (const uint8_t*)shared.m_Value.m_String < bytes + size);
    ASSERT_EQ((uintptr_t)(bytes), (uintptr_t)GetInstanceTable(instance, 0)->m_Blob); // Values remain borrowed.
    ASSERT_EQ((uintptr_t)0, (uintptr_t)instance->m_Payloads);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, id, 1, &object)); // Copy only when explicitly setting.
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, id, 1, &child));
    ASSERT_TRUE(child.m_Value.m_Struct.m_View.m_Buffer != object.m_Value.m_Struct.m_View.m_Buffer);
    ASSERT_EQ(DATASTORE_STRUCT_ROW, child.m_Value.m_Struct.m_Source);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, id, 1, &child)); // Owned view self-assignment.
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, id, 1, &child));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&child.m_Value.m_Struct, 20, &shared));
    ASSERT_STREQ("shared", shared.m_Value.m_String);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldReset(store, id, 1));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, id, 1, &child));
    ASSERT_EQ(object.m_Value.m_Struct.m_View.m_Offset, child.m_Value.m_Struct.m_View.m_Offset);
    DataStoreBlobReset(instance);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, roundtrip, size, &size));
    ASSERT_EQ(0, memcmp(bytes, roundtrip, size));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(source));
    delete[] bytes;
    delete[] roundtrip;
}

// Nested bulk storage stays compact while insertion copies values from borrowed views.
TEST(Data, CompactNestedBulkStorageAndBorrowedInsertion)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreFieldDesc meta = { 1, DATASTORE_TYPE_STRUCT, 0 };
    DataStoreTableDesc desc = { 1, 0, 0, &meta, 1, 8, DATASTORE_RESET_RETAIN };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &desc));
    uint64_t                 names[] = { 10, 20 };
    const DataStoreValueType types[] = { DATASTORE_TYPE_VECTOR3, DATASTORE_TYPE_NUMBER };
    DataStoreValueData       fields[2] = {};
    fields[0].m_Vector3[0] = 1;
    fields[0].m_Vector3[1] = 2;
    fields[0].m_Vector3[2] = 3;
    fields[1].m_Number = 4;
    DataStoreValue           value = Struct(names, types, fields, 2);
    DataStoreRowDesc         rows[128];
    DataStoreId              ids[128];
    for (uint32_t i = 0; i < 128; ++i)
    {
        DataStoreRowDesc row = { 0, &value.m_Type, &value.m_Value, 1 };
        rows[i] = row;
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowAddBatch(store, 1, rows, 128, ids));
    DataStoreMemoryStats memory;
    GetDataMemoryStats(store, &memory);
    ASSERT_EQ((uint64_t)0, memory.m_BaseBlocks);
    ASSERT_EQ((uint64_t)128, memory.m_StructRows);
    ASSERT_EQ(1u, FindTable(store, 1)->m_Structs->m_Tables.Size());
    ASSERT_EQ(24u, FindTable(store, 1)->m_Structs->m_Tables[0]->m_RowStride);
    ASSERT_TRUE(memory.m_StructRowBytes < (uint64_t)(128 * 64));

    DataStoreValue borrowed;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[0], 1, &borrowed));
    ASSERT_TRUE(borrowed.m_Value.m_Struct.m_View.m_StructTable != 0);
    ASSERT_EQ(DATASTORE_STRUCT_ROW, borrowed.m_Value.m_Struct.m_Source);
    for (uint32_t i = 0; i < 128; ++i)
    {
        rows[i].m_Values = &borrowed.m_Value;
    }
    // Growing row/arena storage must keep the source view valid until copying ends.
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowAddBatch(store, 1, rows, 128, ids));
    fields[1].m_Number = 99;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, ids[0], 1, &value));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableReset(store, 1));
    for (uint32_t i = 0; i < 128; ++i)
    {
        DataStoreValue object, child;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[i], 1, &object));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&object.m_Value.m_Struct, 10, &child));
        ASSERT_EQ(3.0f, child.m_Value.m_Vector3[2]);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&object.m_Value.m_Struct, 20, &child));
        ASSERT_EQ(4.0, child.m_Value.m_Number);
    }
    GetDataMemoryStats(store, &memory);
    ASSERT_EQ((uint64_t)0, memory.m_BaseBlocks);
    ASSERT_EQ((uint64_t)256, memory.m_StructRows);
    ASSERT_EQ((uint64_t)0, memory.m_PayloadBlocks);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Child slots are reused without moving survivors; empty buffers are freed and
// subsequent insertions reuse the layout without following stale free slots.
TEST(Data, OwnedStructRowsShareLayoutsButNotValues)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreFieldDesc field = { 1, DATASTORE_TYPE_STRUCT };
    DataStoreTableDesc desc = { 1, 0, 0, &field, 1, 8, DATASTORE_RESET_RETAIN };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &desc));
    uint64_t           names[] = { 2, 3 };
    DataStoreValueType types[] = { DATASTORE_TYPE_NUMBER, DATASTORE_TYPE_VECTOR3 };
    DataStoreValueData values[2] = {};
    values[0].m_Number = 10;
    values[1].m_Vector3[0] = 1;
    values[1].m_Vector3[1] = 2;
    values[1].m_Vector3[2] = 3;
    DataStoreValue     object = Struct(names, types, values, 2);
    DataStoreId        ids[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(store, 1, 0, &object, 1, &ids[i], 0));
    }
    DataStoreTable* table = FindTable(store, 1);
    ASSERT_EQ(1u, table->m_Structs->m_Tables.Size());
    DataStoreStructTable* children = table->m_Structs->m_Tables[0];
    ASSERT_EQ(3u, children->m_RowCount);
    DataStoreValue first, second, child;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[0], 1, &first));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[1], 1, &second));
    ASSERT_NE(first.m_Value.m_Struct.m_View.m_Offset, second.m_Value.m_Struct.m_View.m_Offset);

    // Input member order is not part of the shared layout's identity.
    uint64_t           reordered_names[] = { 3, 2 };
    DataStoreValueType reordered_types[] = { DATASTORE_TYPE_VECTOR3, DATASTORE_TYPE_NUMBER };
    DataStoreValueData reordered_values[2] = {};
    reordered_values[0].m_Vector3[0] = 4;
    reordered_values[0].m_Vector3[1] = 5;
    reordered_values[0].m_Vector3[2] = 6;
    reordered_values[1].m_Number = 20;
    DataStoreValue replacement = Struct(reordered_names, reordered_types, reordered_values, 2);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, ids[0], 1, &replacement));
    ASSERT_EQ(1u, table->m_Structs->m_Tables.Size());
    ASSERT_EQ(4u, children->m_RowCount); // Three defaults and one replacement.
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[0], 1, &first));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&first.m_Value.m_Struct, 2, &child));
    ASSERT_EQ(20.0, child.m_Value.m_Number);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&second.m_Value.m_Struct, 2, &child));
    ASSERT_EQ(10.0, child.m_Value.m_Number);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldReset(store, ids[0], 1));
    ASSERT_EQ(3u, children->m_RowCount);

    uint32_t capacity = children->m_Values.Capacity();
    uint32_t slots = children->m_Values.Size();
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[1])); // Swaps the parent row only.
    ASSERT_EQ(2u, children->m_RowCount);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[2], 1, &second));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&second.m_Value.m_Struct, 2, &child));
    ASSERT_EQ(10.0, child.m_Value.m_Number);
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(store, 1, 0, &object, 1, &ids[1], 0));
    ASSERT_EQ(slots, children->m_Values.Size());
    ASSERT_EQ(capacity, children->m_Values.Capacity());
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[i]));
    }
    ASSERT_EQ(0u, children->m_RowCount);
    ASSERT_EQ(0u, children->m_Values.Capacity());
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(store, 1, 0, &object, 1, &ids[i], 0));
    }
    ASSERT_EQ(1u, table->m_Structs->m_Tables.Size());
    ASSERT_EQ(3u, children->m_RowCount);
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[i], 1, &first));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&first.m_Value.m_Struct, 2, &child));
        ASSERT_EQ(10.0, child.m_Value.m_Number);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[i]));
    }
    ASSERT_EQ(0u, children->m_Values.Capacity());
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Recursive children can share a layout with their parents. Its buffer must
// remain live until every descendant and retained default has been released.
TEST(Data, OwnedStructRowsReleaseRecursiveTrees)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreFieldDesc field = { 1, DATASTORE_TYPE_STRUCT };
    DataStoreTableDesc desc = { 1, 0, 0, &field, 1, 8, DATASTORE_RESET_RETAIN };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &desc));
    uint64_t           name = 2;
    DataStoreValueType number_type = DATASTORE_TYPE_NUMBER;
    DataStoreValueType struct_type = DATASTORE_TYPE_STRUCT;
    DataStoreValueData number = { 42 };
    DataStoreValue     leaf = Struct(&name, &number_type, &number, 1);
    DataStoreValue     middle = Struct(&name, &struct_type, &leaf.m_Value, 1);
    DataStoreValue     root = Struct(&name, &struct_type, &middle.m_Value, 1);
    DataStoreId        ids[40];
    for (uint32_t i = 0; i < 40; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(store, 1, 0, &root, 1, &ids[i], 0));
    }
    DataStoreTable* table = FindTable(store, 1);
    ASSERT_EQ(2u, table->m_Structs->m_Tables.Size());
    ASSERT_EQ(80u, table->m_Structs->m_Tables[0]->m_RowCount);
    ASSERT_EQ(40u, table->m_Structs->m_Tables[1]->m_RowCount);
    DataStoreValue view, child, grandchild;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[0], 1, &view));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, ids[0], 1, &view)); // Same-layout growth during a borrowed copy.
    ASSERT_EQ(82u, table->m_Structs->m_Tables[0]->m_RowCount);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[0], 1, &view));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&view.m_Value.m_Struct, name, &child));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&child.m_Value.m_Struct, name, &grandchild));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&grandchild.m_Value.m_Struct, name, &view));
    ASSERT_EQ(42.0, view.m_Value.m_Number);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[0]));
    ASSERT_EQ(80u, table->m_Structs->m_Tables[0]->m_RowCount);
    for (uint32_t i = 0; i < 40; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[i]));
    }
    for (uint32_t t = 0; t < table->m_Structs->m_Tables.Size(); ++t)
    {
        ASSERT_EQ(0u, table->m_Structs->m_Tables[t]->m_RowCount);
        ASSERT_EQ(0u, table->m_Structs->m_Tables[t]->m_Values.Capacity());
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Owned dynamic-struct rows preserve borrowed blob defaults through mutation and reset.
TEST(Data, OwnedStructRowsKeepBlobDefaultsBorrowed)
{
    HDataStoreContext  source = DataStoreCreate(0);
    DataStoreFieldDesc field = { 1, DATASTORE_TYPE_STRUCT };
    DataStoreTableDesc desc = { 1, 0, 0, &field, 1, 8 };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(source, &desc));
    uint64_t           name = 2;
    DataStoreValueType type = DATASTORE_TYPE_NUMBER;
    DataStoreValueData number = { 10 };
    DataStoreValue     object = Struct(&name, &type, &number, 1);
    DataStoreId        id;
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, 1, 0, &object, 1, &id, 0));
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    uint8_t* copy = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, size, &size));
    memcpy(copy, bytes, size);
    HDataStoreBlob blob;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
    HDataStoreContext      store = DataStoreCreate(0);
    HDataStoreBlobInstance instances[2];
    for (uint32_t i = 0; i < 2; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(store, blob, 0, &instances[i]));
    }
    DataStoreTable* table = GetInstanceTable(instances[0], 0);
    ASSERT_TRUE(table->m_Structs == 0);
    DataStoreId ids[] = { MakeId(store, GetInstanceSlots(instances[0])[0]), MakeId(store, GetInstanceSlots(instances[1])[0]) };
    number.m_Number = 20;
    for (uint32_t i = 0; i < 2; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, ids[i], 1, &object));
    }
    ASSERT_EQ(2u, table->m_Structs->m_Tables[0]->m_RowCount);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobReset(instances[0]));
    ASSERT_EQ(1u, table->m_Structs->m_Tables[0]->m_RowCount);
    DataStoreValue value, child;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[0], 1, &value));
    ASSERT_EQ((uintptr_t)bytes, (uintptr_t)value.m_Value.m_Struct.m_View.m_Buffer);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[1], 1, &value));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&value.m_Value.m_Struct, name, &child));
    ASSERT_EQ(20.0, child.m_Value.m_Number);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobRemove(instances[1]));
    ASSERT_EQ(0u, table->m_Structs->m_Tables[0]->m_RowCount);
    ASSERT_EQ(0, memcmp(bytes, copy, size));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobRemove(instances[0]));
    DataStoreBlobDestroy(blob);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(source));
    delete[] copy;
    delete[] bytes;
}

// Invalid nested values and excessive depth cannot replace existing row values.
TEST(Data, NestedValidationAndDepth)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreFieldDesc meta = { 1, DATASTORE_TYPE_LIST, 0 };
    DataStoreTableDesc desc = { 1, 0, 0, &meta, 1, 8 };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &desc));
    DataStoreValue base = List(0, 0);
    DataStoreId    id;
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(store, 1, 0, &base, 1, &id, 0));
    DataStoreValue invalid = List(0, 1);
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFieldSet(store, id, 1, &invalid));
    invalid = List(&invalid, 1);
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFieldSet(store, id, 1, &invalid));
    DataStoreValue bad = Boolean(2);
    invalid = List(&bad, 1);
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFieldSet(store, id, 1, &invalid));
    uint64_t           names[] = { 10, 10 };
    DataStoreValueType types[] = { DATASTORE_TYPE_NULL, DATASTORE_TYPE_NULL };
    DataStoreValueData fields[2] = {};
    DataStoreValue     object = Struct(names, types, fields, 2);
    invalid = List(&object, 1);
    ASSERT_EQ(DATASTORE_RESULT_ALREADY_EXISTS, DataStoreFieldSet(store, id, 1, &invalid));
    names[1] = 20;
    object.m_Value.m_Struct.m_Array.m_Types = 0;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFieldSet(store, id, 1, &invalid));
    object.m_Value.m_Struct.m_Array.m_Types = types;
    types[1] = (DataStoreValueType)99;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFieldSet(store, id, 1, &invalid));
    types[1] = DATASTORE_TYPE_NULL;
    DataStoreValue values[DATA_MAX_NESTING + 2];
    values[0] = Number(1);
    for (uint32_t i = 1; i < DATA_MAX_NESTING + 2; ++i)
    {
        values[i] = List(&values[i - 1], 1);
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, id, 1, &values[DATA_MAX_NESTING]));
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFieldSet(store, id, 1, &values[DATA_MAX_NESTING + 1]));
    uint8_t DM_ALIGNED(8) bytes[4096];
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, bytes, sizeof(bytes), &size));
    HDataStoreBlob blob;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
    DataStoreBlobDestroy(blob);
    // Last number becomes an empty list, which would add a 65th container.
    uint32_t container = (uint32_t)ReadDataInteger(bytes + 24 + DATA_TABLE_HEADER_SIZE + DATA_FIELD_META_SIZE + sizeof(DataStoreRow), 8);
    for (uint32_t i = 1; i < DATA_MAX_NESTING; ++i)
    {
        container = (uint32_t)ReadDataInteger(bytes + container + 12, 4);
    }
    WriteDataInteger(bytes + container + 8, DATASTORE_TYPE_LIST, 4);
    memset(bytes + size - 8, 0, 8);
    bytes[size - 8] = 8;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_FORMAT, DataStoreBlobLoad(bytes, size, &blob));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Malformed blob directories, layouts and payloads leave the output handle unchanged.
TEST(Data, MalformedBlobDirectoryLayoutAndValues)
{
    HDataStoreContext  source = DataStoreCreate(0);
    DataStoreFieldDesc meta[] = { { 1, DATASTORE_TYPE_BOOLEAN, 0 }, { 2, DATASTORE_TYPE_STRING, 8 }, { 3, DATASTORE_TYPE_LIST, 16 } };
    DataStoreTableDesc desc = { 1, 0, 0, meta, 3, 24 };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(source, &desc));
    desc.m_Type = 2;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(source, &desc));
    DataStoreValue item = Number(2);
    DataStoreValue values[] = { Boolean(1), String("text"), List(&item, 1) };
    DataStoreId    id;
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, 1, 0, values, 3, &id, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, 2, 0, values, 3, &id, 0));
    uint8_t DM_ALIGNED(8) bytes[1024], saved[1024];
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, sizeof(bytes), &size));
    memcpy(saved, bytes, size);
    HDataStoreBlob sentinel;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(saved, size, &sentinel));
    for (uint32_t length = 0; length < size; ++length)
    {
        HDataStoreBlob output = sentinel;
        ASSERT_EQ(DATASTORE_RESULT_INVALID_FORMAT, DataStoreBlobLoad(bytes, length, &output));
        ASSERT_EQ((uintptr_t)sentinel, (uintptr_t)output);
    }
    uint32_t second = (uint32_t)ReadDataInteger(bytes + 20, 4);
    // First table starts at 24, metadata at 48, row record at 144, values at 176.
    // The record's group, slot, base/instance indices and reserved bytes must be zero.
    const uint32_t corrupt[] = { 0, 4, 8, 12, 16, 20, 32, 34, 36, 40, 44, 46, 56, 60, 64, 66, 68, 88, 92, 96, 98, 100, 120, 124, 128, 130, 132, 144, 160, 164, 168, 172, 176, 184, 192 };
    for (uint32_t i = 0; i < sizeof(corrupt) / sizeof(corrupt[0]); ++i)
    {
        bytes[corrupt[i]] = 0xff;
        HDataStoreBlob output = sentinel;
        ASSERT_EQ(DATASTORE_RESULT_INVALID_FORMAT, DataStoreBlobLoad(bytes, size, &output));
        ASSERT_EQ((uintptr_t)sentinel, (uintptr_t)output);
        memcpy(bytes, saved, size);
    }
    bytes[second + offsetof(DataStoreTableHeader, m_RowStride)] = 0;
    HDataStoreBlob output = sentinel;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_FORMAT, DataStoreBlobLoad(bytes, size, &output));
    memcpy(bytes, saved, size);
    memcpy(bytes + 80, bytes + 48, 8); // Repeated field name in shared metadata.
    ASSERT_EQ(DATASTORE_RESULT_INVALID_FORMAT, DataStoreBlobLoad(bytes, size, &output));
    memcpy(bytes, saved, size);
    WriteDataInteger(bytes + 92, 0, 4); // String overlaps boolean in every row.
    ASSERT_EQ(DATASTORE_RESULT_INVALID_FORMAT, DataStoreBlobLoad(bytes, size, &output));
    memcpy(bytes, saved, size);
    uint32_t nested = (uint32_t)ReadDataInteger(bytes + 192, 8);
    bytes[nested + 8] = 99;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_FORMAT, DataStoreBlobLoad(bytes, size, &output));
    memcpy(bytes, saved, size);
    bytes[size - 1] = 'X';
    ASSERT_EQ(DATASTORE_RESULT_INVALID_FORMAT, DataStoreBlobLoad(bytes, size, &output));
    memcpy(bytes, saved, size);
    bytes[size] = 0;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_FORMAT, DataStoreBlobLoad(bytes, size + 1, &output));
    ASSERT_EQ(DATASTORE_RESULT_INVALID_FORMAT, DataStoreBlobLoad(0, 0, &output));
    ASSERT_EQ((uintptr_t)sentinel, (uintptr_t)output);
    DataStoreBlobDestroy(sentinel);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(source));
}

// Empty blobs and NULL values remain reusable while their source bytes are borrowed.
TEST(Data, EmptyNullAndBorrowedResourceReuse)
{
    HDataStoreContext source = DataStoreCreate(0);
    uint8_t DM_ALIGNED(8) bytes[256];
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, sizeof(bytes), &size));
    ASSERT_EQ(16u, size);
    HDataStoreBlob blob;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
    HDataStoreBlobInstance instance;
    HDataStoreContext      store = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(store, blob, 0, &instance));
    DataStoreBlobReset(instance);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    ASSERT_EQ(1u, blob->m_RefCount);
    DataStoreBlobDestroy(blob);
    DataStoreFieldDesc meta = { 1, DATASTORE_TYPE_NULL, 0 };
    DataStoreTableDesc desc = { 1, 0, 0, &meta, 1, 0, DATASTORE_RESET_RETAIN };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(source, &desc));
    DataStoreValue value = Null();
    DataStoreId    id;
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, 1, 0, &value, 1, &id, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(source, id, 1, &value));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(source, id, 1, &value));
    DataStoreTableDesc empty = { 2, 0, 0, 0, 0, 0, DATASTORE_RESET_RETAIN };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(source, &empty));
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, 2, 0, 0, 0, &id, 0));
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGet(source, id, 1, &value));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, sizeof(bytes), &size));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
    ASSERT_EQ(2u, blob->m_TableCount);
    ASSERT_EQ(2u, blob->m_RowCount);
    store = DataStoreCreate(0);
    HDataStoreContext      another = DataStoreCreate(0);
    HDataStoreBlobInstance second;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(store, blob, 0, &instance));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(another, blob, 0, &second));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    ASSERT_EQ(2u, blob->m_RefCount);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(another, FirstId(another), 1, &value));
    ASSERT_EQ(DATASTORE_TYPE_NULL, value.m_Type);
    DataStoreBlobDestroy(blob);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(another));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(source));
}

// Reset retains insertion defaults across later edits and releases their override storage.
TEST(Data, ResetAddedValuesAllocationBlocksAndDefaults)
{
    HDataStoreContext store = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(store, 1, 0, 0));
    DataStoreId first, second;
    ASSERT_EQ(DATASTORE_RESULT_OK, Add(store, 1, 0, 17, "original", &first));
    DataStoreValue value = String("new baseline");
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, first, 20, &value));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, first, 20, &value));
    ASSERT_EQ(DATASTORE_RESULT_OK, Add(store, 1, 0, 23, value.m_Value.m_String, &second));
    char large[8193];
    memset(large, 'L', sizeof(large) - 1);
    large[sizeof(large) - 1] = 0;
    for (uint32_t cycle = 0; cycle < 3; ++cycle)
    {
        value = String(large);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, first, 20, &value));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, first, 20, &value));
        ASSERT_STREQ(large, value.m_Value.m_String);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, first, 20, &value));
        for (uint32_t i = 0; i < 300; ++i)
        {
            value = String("replacement");
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, first, 20, &value));
        }
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableReset(store, 1));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, first, 20, &value));
        ASSERT_STREQ("original", value.m_Value.m_String);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, second, 20, &value));
        ASSERT_STREQ("new baseline", value.m_Value.m_String);
    }
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldReset(store, 0, 20));
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldReset(store, first, 999));
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreTableReset(store, 999));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}

// Typed lookup resolves hash collisions and metadata indices above 255 without truncation.
TEST(Data, TypedLookupHashCollisionsAndWideIndexes)
{
    const uint32_t     count = 258;
    DataStoreFieldDesc fields[count];
    DataStoreValue     values[count];
    for (uint32_t i = 0; i < count; ++i)
    {
        fields[i] = { 1 + 16 * i, DATASTORE_TYPE_NUMBER, i* 16 };
        values[i] = Number(i);
    }
    // Colliding names have different kinds; two direct candidates exceed 255.
    fields[1].m_Type = DATASTORE_TYPE_VECTOR3;
    values[1] = Vector3(1, 2, 3);
    fields[256].m_Field = 2;
    fields[257].m_Field = 0;
    DataStoreTableDesc desc = { 1, 0, 0, fields, count, count* 16 };
    HDataStoreContext  source = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(source, &desc));
    DataStoreId original;
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, 1, 0, values, count, &original, 0));
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, size, &size));
    HDataStoreContext      loaded = DataStoreCreate(0);
    HDataStoreBlobInstance instance;
    ASSERT_EQ(DATASTORE_RESULT_OK, Load(loaded, bytes, size, &instance, 0));
    HDataStoreContext stores[] = { source, loaded };
    DataStoreId       ids[] = { original, FirstId(loaded) };
    for (uint32_t mode = 0; mode < 2; ++mode)
    {
        HDataStoreContext store = stores[mode];
        DataStoreId       id = ids[mode];
        double            number;
        DataStoreVector3  vector;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, id, 1, &number));
        ASSERT_EQ(0.0, number);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3(store, id, 17, &vector));
        ASSERT_EQ(3.0f, vector.m_Values[2]);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, id, fields[255].m_Field, &number));
        ASSERT_EQ(255.0, number);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, id, 2, &number));
        ASSERT_EQ(256.0, number);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, id, 0, &number));
        ASSERT_EQ(257.0, number);
        number = 123;
        ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFieldGetNumber(store, id, 17, &number));
        ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGetNumber(store, id, 1 + 16 * count, &number));
        ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGetNumber(store, id, 3, &number));
        ASSERT_EQ(123.0, number);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, id));
        ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGetNumber(store, id, 2, &number));
        ASSERT_EQ(123.0, number);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    }
    delete[] bytes;
}

// Typed errors preserve outputs; packed writes and resets leave borrowed blob bytes intact.
TEST(Data, TypedAccessErrorsPackedValuesAndReset)
{
    HDataStoreContext source = DataStoreCreate(0);
    // Field order differs from query binding order.
    DataStoreFieldDesc meta[] = { { 10, DATASTORE_TYPE_VECTOR3, 0 }, { 20, DATASTORE_TYPE_STRING, 16 }, { 30, DATASTORE_TYPE_BOOLEAN, 24 } };
    DataStoreTableDesc table = { 1, 0, 0, meta, 3, 32, DATASTORE_RESET_RETAIN };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(source, &table));
    DataStoreValue values[] = { Vector3(1, 2, 3), String("packed"), Boolean(1) };
    DataStoreId    original;
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, 1, 0, values, 3, &original, 0));
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, size, &size));
    uint8_t* saved = new uint8_t[size];
    memcpy(saved, bytes, size);

    HDataStoreContext      loaded = DataStoreCreate(0);
    HDataStoreBlobInstance instance;
    ASSERT_EQ(DATASTORE_RESULT_OK, Load(loaded, bytes, size, &instance, 0));
    HDataStoreContext stores[] = { source, loaded };
    for (uint32_t mode = 0; mode < 2; ++mode)
    {
        HDataStoreContext  store = stores[mode];
        DataStoreQueryItem fields[] = {
            { 30, DATASTORE_TYPE_BOOLEAN, 0, DATASTORE_QUERY_ITEM_FIELD },
            { 10, DATASTORE_TYPE_VECTOR3, 0, DATASTORE_QUERY_ITEM_FIELD },
            { 20, DATASTORE_TYPE_STRING, 0, DATASTORE_QUERY_ITEM_FIELD }
        };
        DataStoreQueryDesc desc = { { fields[0], fields[1], fields[2] }, 0, 0, 0 };
        HDataStoreQuery    query;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
        DataStoreIterator it = DataStoreQueryIter(query);
        DataStoreVector3  vector = { { 7, 8, 9 } };
        ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, GetTestFieldVector3(&it, 0, 1, &vector));
        ASSERT_EQ(7.0f, vector.m_Values[0]);
        ASSERT_TRUE(DataStoreIterNext(&it));
        DataStoreId id = DataStoreIterGetId(&it, 0);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3(store, id, 10, &vector));
        ASSERT_EQ(1.0f, vector.m_Values[0]);
        ASSERT_EQ(3.0f, vector.m_Values[2]);
        const char* string;
        ASSERT_EQ(DATASTORE_RESULT_OK, GetTestFieldString(&it, 0, 2, &string));
        ASSERT_STREQ("packed", string);
        if (mode)
        {
            ASSERT_TRUE(string >= (const char*)bytes && string < (const char*)bytes + size);
        }

        double number = 123;
        ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, GetTestFieldNumber(&it, 1, 1, &number));
        ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, GetTestFieldNumber(&it, 0, 3, &number));
        ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, SetTestFieldNumber(&it, 0, 1, 4));
        ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, SetTestFieldVector3(&it, 1, 1, &vector));
        ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, SetTestFieldBoolean(&it, 0, 0, 2));
        ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, SetTestFieldString(&it, 0, 2, 0));
        DataStoreQueryEnd(query);
        ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFieldGetNumber(store, id, 10, &number));
        ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGetNumber(store, 0, 10, &number));
        ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGetNumber(store, id, 99, &number));
        ASSERT_EQ(123.0, number);
        ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFieldSetNumber(store, id, 10, 4));
        ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFieldSetBoolean(store, id, 30, 2));
        ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFieldSetString(store, id, 20, 0));
        ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldSetVector3(store, 0, 10, &vector));
        ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldSetVector3(store, id, 99, &vector));
        DataStoreMemoryStats memory;
        GetDataMemoryStats(store, &memory);
        ASSERT_EQ((uint64_t)0, memory.m_PayloadBlocks);

        vector.m_Values[1] = 8;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetVector3(store, id, 10, &vector));
        vector.m_Values[1] = 99; // Set copied the caller's math value.
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3(store, id, 10, &vector));
        ASSERT_EQ(8.0f, vector.m_Values[1]);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetBoolean(store, id, 30, 0));
        uint8_t boolean = 1;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetBoolean(store, id, 30, &boolean));
        ASSERT_EQ(0, boolean);
        char replacement[] = "changed";
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetString(store, id, 20, replacement));
        replacement[0] = 'X';
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetString(store, id, 20, &string));
        ASSERT_STREQ("changed", string);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetString(store, id, 20, string));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetString(store, id, 20, &string));
        ASSERT_STREQ("changed", string);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldReset(store, id, 10));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3(store, id, 10, &vector));
        ASSERT_EQ(2.0f, vector.m_Values[1]);
        if (mode)
        {
            DataStoreBlobReset(instance);
        }
        else
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableReset(store, 1));
        }
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetBoolean(store, id, 30, &boolean));
        ASSERT_EQ(1, boolean);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetString(store, id, 20, &string));
        ASSERT_STREQ("packed", string);
        GetDataMemoryStats(store, &memory);
        ASSERT_EQ((uint64_t)0, memory.m_PayloadBlocks);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, id));
        ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGetVector3(store, id, 10, &vector));
        ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldSetVector3(store, id, 10, &vector));
        ASSERT_EQ(2.0f, vector.m_Values[1]);
        DataStoreQueryDestroy(query);
    }
    ASSERT_EQ(0, memcmp(saved, bytes, size));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(source));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(loaded));
    delete[] saved;
    delete[] bytes;
}

// Bound queries track repeated native and blob registrations without retaining stale matches.
TEST(Data, BoundQueryRegistrationChurn)
{
    HDataStoreContext source = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(source, 1, 0, 0));
    DataStoreId id;
    ASSERT_EQ(DATASTORE_RESULT_OK, Add(source, 1, 0, 5, "packed", &id));
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, size, &size));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(source));
    HDataStoreBlob blob;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreQueryItem fields[] = {
        { 10, DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD },
        { 20, DATASTORE_TYPE_STRING, 0, DATASTORE_QUERY_ITEM_FIELD }
    };
    DataStoreQueryDesc desc = { { fields[0], fields[1] }, 0, 0, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    // Cross registration pages and grow dense storage, then reuse slots with live queries.
    const uint32_t         count = 600;
    HDataStoreBlobInstance instances[count];
    DataStoreId            original = NextTestGroupId(), replacement = NextTestGroupId();
    for (uint32_t cycle = 0; cycle < 3; ++cycle)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(store, blob, original, &instances[i]));
        }
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
        DataStoreIterator it      = DataStoreQueryIter(query);
        uint32_t          visited = 0;
        while (DataStoreIterNext(&it))
        {
            ASSERT_EQ(count, DataStoreIterGetCount(&it));
            for (uint32_t r = 0; r < DataStoreIterGetCount(&it); ++r)
            {
                ASSERT_EQ(DATASTORE_RESULT_OK, SetTestFieldNumber(&it, r, 0, (double)DataStoreIterGetGroupId(&it, r)));
                ++visited;
            }
        }
        ASSERT_EQ(count, visited);
        DataStoreQueryEnd(query);
        DataStoreMemoryStats before, after;
        GetDataMemoryStats(store, &before);
        ASSERT_EQ((uint64_t)1, before.m_Tables);
        ASSERT_EQ((uint64_t)count, before.m_Instances);
        for (uint32_t i = 0; i < count; i += 2)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobRemove(instances[i]));
        }
        for (uint32_t i = 0; i < count; i += 2)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(store, blob, replacement, &instances[i]));
        }
        GetDataMemoryStats(store, &after);
        ASSERT_EQ(before.m_TotalBytes, after.m_TotalBytes);
        ASSERT_EQ(before.m_QueryBytes, after.m_QueryBytes);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
        it = DataStoreQueryIter(query);
        visited = 0;
        while (DataStoreIterNext(&it))
        {
            for (uint32_t r = 0; r < DataStoreIterGetCount(&it); ++r)
            {
                double      number;
                const char* string;
                DataStoreId group = DataStoreIterGetGroupId(&it, r);
                ASSERT_EQ(DATASTORE_RESULT_OK, GetTestFieldNumber(&it, r, 0, &number));
                ASSERT_EQ(group == replacement ? 5.0 : (double)group, number);
                ASSERT_EQ(DATASTORE_RESULT_OK, GetTestFieldString(&it, r, 1, &string));
                ASSERT_STREQ("packed", string);
                ++visited;
            }
        }
        ASSERT_EQ(count, visited);
        DataStoreQueryEnd(query);
        for (uint32_t i = 0; i < count; ++i)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobRemove(instances[i]));
        }
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
        it = DataStoreQueryIter(query);
        ASSERT_FALSE(DataStoreIterNext(&it));
        DataStoreQueryEnd(query);
    }
    DataStoreQueryDestroy(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    DataStoreBlobDestroy(blob);
    delete[] bytes;
}

extern "C" int TestInlineDataFromC(HDataStoreContext store);

// Inline composition preserves C offsets, nested field access and blob round trips.
TEST(Data, InlineCompositionQueriesAndRoundtrip)
{
    // Nested fixed layouts preserve C alignment and include variable data.
    DataStoreFieldDesc  light_fields[] = { { 10, DATASTORE_TYPE_VECTOR3, 0, 0, "10" }, { 20, DATASTORE_TYPE_STRING, 16, 0, "20" }, { 30, DATASTORE_TYPE_LIST, 24, 0, "30" } };
    DataStoreStructDesc light = { light_fields, 3, 32 };
    DataStoreFieldDesc  wrapper_fields[] = { { 40, DATASTORE_TYPE_STRUCT, 8, &light, "40" } };
    DataStoreStructDesc wrapper = { wrapper_fields, 1, 40 };
    DataStoreFieldDesc  fields[] = { { 1, DATASTORE_TYPE_NUMBER, 0, 0, "1" }, { 2, DATASTORE_TYPE_STRUCT, 8, &wrapper, "2" }, { 3, DATASTORE_TYPE_BOOLEAN, 48, 0, "3" } };
    DataStoreTableDesc  table = { 1, 0, 0, fields, 3, 56, DATASTORE_RESET_RETAIN };
    HDataStoreContext   source = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(source, &table));
    // Changing a declaration after registration cannot alter the compiled layout.
    light_fields[0].m_Offset = 999;
    uint64_t                 names[] = { 30, 10, 20 };
    DataStoreValue           list_items[] = { Number(7), String("shared") };
    const DataStoreValueType child_types[] = { DATASTORE_TYPE_LIST, DATASTORE_TYPE_VECTOR3, DATASTORE_TYPE_STRING };
    DataStoreValueData       children[] = { List(list_items, 2).m_Value, Vector3(1, 2, 3).m_Value, String("shared").m_Value };
    DataStoreValue           light_value = Struct(names, child_types, children, 3);
    uint64_t                 light_name = 40;
    DataStoreValue           values[] = { Number(5), Struct(&light_name, &light_value.m_Type, &light_value.m_Value, 1), Boolean(1) };
    DataStoreId              original;
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, 1, 0, values, 3, &original, 0));
    ASSERT_EQ(7u, source->m_Tables[0]->m_MetadataCount);
    ASSERT_EQ(32u, GetFieldMeta(source->m_Tables[0], 3).m_Size);

    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, 0, 0, &size));
    uint8_t* allocation = new uint8_t[size];
    uint8_t* bytes = allocation;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, size, &size));
    uint8_t* saved = new uint8_t[size];
    memcpy(saved, bytes, size);
    for (uint32_t packed = 0; packed < 2; ++packed)
    {
        HDataStoreContext      store = packed ? DataStoreCreate(0) : source;
        HDataStoreBlobInstance instance = 0;
        if (packed)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, Load(store, bytes, size, &instance, 0));
        }
        DataStoreQueryItem fields[] = {
            { dmHashString64("2.40.10"), DATASTORE_TYPE_VECTOR3, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD },
            { dmHashString64("2.40.20"), DATASTORE_TYPE_STRING, 0, DATASTORE_QUERY_ITEM_FIELD },
            { dmHashString64("2.40.30"), DATASTORE_TYPE_LIST, 0, DATASTORE_QUERY_ITEM_FIELD },
            { 1, DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_FIELD },
            { 2, DATASTORE_TYPE_STRUCT, 0, DATASTORE_QUERY_ITEM_FIELD }
        };
        DataStoreQueryDesc desc = { { fields[0], fields[1], fields[2], fields[3], fields[4] }, 0, 0, 0 };
        HDataStoreQuery    query;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
        DataStoreIterator it = DataStoreQueryIter(query);
        ASSERT_TRUE(DataStoreIterNext(&it));
        DataStoreId      id = DataStoreIterGetId(&it, 0);
        DataStoreVector3 color;
        ASSERT_EQ(DATASTORE_RESULT_OK, GetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(3.0f, color.m_Values[2]);
        const char* string;
        ASSERT_EQ(DATASTORE_RESULT_OK, GetTestFieldString(&it, 0, 1, &string));
        ASSERT_STREQ("shared", string);
        if (packed)
        {
            ASSERT_TRUE((const uint8_t*)string >= bytes && (const uint8_t*)string < bytes + size);
        }
        color.m_Values[0] = 42;
        ASSERT_EQ(DATASTORE_RESULT_OK, SetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(DATASTORE_RESULT_OK, GetTestFieldString(&it, 0, 1, &string));
        ASSERT_STREQ("shared", string); // Sibling references survive the first leaf override.
        DataStoreValue list, child;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreIterGetField(&it, 0, 2, &list));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(&list.m_Value.m_List, 0, &child));
        ASSERT_EQ(7.0, child.m_Value.m_Number);
        DataStoreQueryEnd(query);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetString(store, id, dmHashString64("2.40.20"), "changed"));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3(store, id, dmHashString64("2.40.10"), &color));
        ASSERT_EQ(42.0f, color.m_Values[0]);
        // Persist a leaf override with untouched sibling references.
        uint8_t DM_ALIGNED(8) changed_blob[1024];
        uint32_t changed_size;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, changed_blob, sizeof(changed_blob), &changed_size));
        HDataStoreContext      changed_store = DataStoreCreate(0);
        HDataStoreBlobInstance changed_instance;
        ASSERT_EQ(DATASTORE_RESULT_OK, Load(changed_store, changed_blob, changed_size, &changed_instance, 0));
        DataStoreQueryItem changed_fields[] = {
            { dmHashString64("2.40.10"), DATASTORE_TYPE_VECTOR3, 0, DATASTORE_QUERY_ITEM_FIELD },
            { dmHashString64("2.40.20"), DATASTORE_TYPE_STRING, 0, DATASTORE_QUERY_ITEM_FIELD }
        };
        DataStoreQueryDesc changed_desc = { { changed_fields[0], changed_fields[1] }, 0, 0, 0 };
        HDataStoreQuery    changed_query;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(changed_store, &changed_desc, &changed_query));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(changed_query));
        DataStoreIterator changed_it = DataStoreQueryIter(changed_query);
        ASSERT_TRUE(DataStoreIterNext(&changed_it));
        ASSERT_EQ(DATASTORE_RESULT_OK, GetTestFieldVector3(&changed_it, 0, 0, &color));
        ASSERT_EQ(42.0f, color.m_Values[0]);
        ASSERT_EQ(DATASTORE_RESULT_OK, GetTestFieldString(&changed_it, 0, 1, &string));
        ASSERT_STREQ("changed", string);
        DataStoreQueryEnd(changed_query);
        DataStoreQueryDestroy(changed_query);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(changed_store));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldReset(store, id, 2));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3(store, id, dmHashString64("2.40.10"), &color));
        ASSERT_EQ(1.0f, color.m_Values[0]);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetString(store, id, dmHashString64("2.40.20"), &string));
        ASSERT_STREQ("shared", string);
        // A write to another root must not change the reference mode of this struct.
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetNumber(store, id, 1, 9));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetString(store, id, dmHashString64("2.40.20"), &string));
        ASSERT_STREQ("shared", string);
        color.m_Values[0] = 42;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetVector3(store, id, dmHashString64("2.40.10"), &color));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetString(store, id, dmHashString64("2.40.20"), "changed"));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, id));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3(store, id, dmHashString64("2.40.10"), &color));
        ASSERT_EQ(1.0f, color.m_Values[0]);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetString(store, id, dmHashString64("2.40.20"), &string));
        ASSERT_STREQ("shared", string);
        double number;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, id, 1, &number));
        ASSERT_EQ(5.0, number);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetVector3(store, id, dmHashString64("2.40.10"), &color));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetString(store, id, dmHashString64("2.40.20"), &string));
        ASSERT_STREQ("shared", string); // Reusing the reset slot restores packed references correctly.
        if (packed)
        {
            DataStoreBlobReset(instance);
        }
        else
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableReset(store, 1));
        }
        ASSERT_EQ(0, TestInlineDataFromC(store));
        DataStoreValue view, light_view;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, id, 2, &view));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&view.m_Value.m_Struct, 40, &light_view));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&light_view.m_Value.m_Struct, 10, &child));
        ASSERT_EQ(3.0f, child.m_Value.m_Vector3[2]);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, id, 2, &view)); // Borrowed self-assignment.
        uint8_t* roundtrip = new uint8_t[size];
        uint32_t written;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, roundtrip, size, &written));
        ASSERT_EQ(size, written);
        ASSERT_EQ(0, memcmp(saved, roundtrip, size));
        HDataStoreBlob checked;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(roundtrip, size, &checked));
        DataStoreBlobDestroy(checked);
        delete[] roundtrip;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, id));
        DataStoreQueryDestroy(query);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    }
    ASSERT_EQ(0, memcmp(saved, bytes, size));
    delete[] saved;
    delete[] allocation;
}

// Invalid inline layouts and ambiguous dotted names fail during registration or loading.
TEST(Data, InlineLayoutAndNameValidation)
{
    HDataStoreContext   store = DataStoreCreate(0);
    DataStoreFieldDesc  members[] = { { 10, DATASTORE_TYPE_VECTOR3, 0, 0, "10" }, { 20, DATASTORE_TYPE_NUMBER, 16, 0, "20" } };
    DataStoreStructDesc light = { members, 2, 24 };
    DataStoreFieldDesc  fields[] = { { 1, DATASTORE_TYPE_STRUCT, 0, &light, "1" } };
    DataStoreTableDesc  table = { 1, 0, 0, fields, 1, 24 };
    fields[0].m_Name = 0;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableAdd(store, &table));
    fields[0].m_Name = "1";
    members[0].m_Name = "bad.name";
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableAdd(store, &table));
    members[0].m_Name = "";
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableAdd(store, &table));
    members[0].m_Name = "10";
    light.m_Size = 8;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableAdd(store, &table));
    light.m_Size = 24;
    members[1].m_Offset = 4;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableAdd(store, &table));
    members[1].m_Offset = 16;
    members[1].m_Field = 10;
    ASSERT_EQ(DATASTORE_RESULT_ALREADY_EXISTS, DataStoreTableAdd(store, &table));
    members[1].m_Field = 20;
    fields[0].m_Type = DATASTORE_TYPE_VECTOR3;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableAdd(store, &table));
    fields[0].m_Type = DATASTORE_TYPE_STRUCT;
    DataStoreStructDesc cycle = { fields, 1, 24 };
    fields[0].m_Struct = &cycle;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableAdd(store, &table));
    fields[0].m_Struct = &light;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    uint64_t           names[] = { 10, 20 };
    DataStoreValueType child_types[] = { DATASTORE_TYPE_VECTOR3, DATASTORE_TYPE_NUMBER };
    DataStoreValueData children[2] = {};
    children[0].m_Vector3[0] = 1;
    children[0].m_Vector3[1] = 2;
    children[0].m_Vector3[2] = 3;
    children[1].m_Number = 4;
    DataStoreValue     value = Struct(names, child_types, children, 2);
    DataStoreId        id = 0;
    child_types[1] = DATASTORE_TYPE_BOOLEAN;
    children[1].m_Boolean = 1;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, AddTestRow(store, 1, 0, &value, 1, &id, 0));
    child_types[1] = DATASTORE_TYPE_NUMBER;
    children[1].m_Number = 4;
    names[1] = 99;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, AddTestRow(store, 1, 0, &value, 1, &id, 0));
    names[1] = 20;
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(store, 1, 0, &value, 1, &id, 0));
    child_types[1] = DATASTORE_TYPE_BOOLEAN;
    children[1].m_Boolean = 1;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFieldSet(store, id, 1, &value));
    DataStoreQueryDesc  desc = { { { dmHashString64("1.10"), DATASTORE_TYPE_VECTOR3, 0, DATASTORE_QUERY_ITEM_FIELD } }, 0, 0, 0 };
    DataStoreQueryItem& field = desc.m_Items[0];
    HDataStoreQuery     query;
    field.m_Type = (DataStoreValueType)99;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreQueryCreate(store, &desc, &query));
    field.m_Type = DATASTORE_TYPE_VECTOR3;
    field.m_Hash = dmHashString64("1.99");
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    DataStoreIterator it = DataStoreQueryIter(query);
    ASSERT_FALSE(DataStoreIterNext(&it));
    DataStoreQueryEnd(query);
    DataStoreQueryDestroy(query);
    field.m_Hash = dmHashString64("1.10");
    field.m_Type = DATASTORE_TYPE_NUMBER;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    it = DataStoreQueryIter(query);
    ASSERT_FALSE(DataStoreIterNext(&it));
    DataStoreQueryEnd(query);
    DataStoreQueryDestroy(query);

    uint8_t DM_ALIGNED(8) bytes[1024], saved[1024];
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, bytes, sizeof(bytes), &size));
    memcpy(saved, bytes, size);
    HDataStoreBlob blob;
    const uint32_t metadata = 24 + DATA_TABLE_HEADER_SIZE;
    const uint32_t corrupt[] = { metadata + offsetof(DataStoreFileFieldMeta, m_ChildIndex), metadata + offsetof(DataStoreFileFieldMeta, m_ChildCount), metadata + offsetof(DataStoreFileFieldMeta, m_ByteSize), metadata + DATA_FIELD_META_SIZE + offsetof(DataStoreFileFieldMeta, m_ChildIndex), metadata + 2 * DATA_FIELD_META_SIZE + offsetof(DataStoreFileFieldMeta, m_ByteOffset) };
    const uint32_t widths[] = { 2, 2, 4, 2, 4 };
    const uint32_t values[] = { 0, UINT32_MAX, 25, 1, 4 };
    for (uint32_t i = 0; i < 5; ++i)
    {
        WriteDataInteger(bytes + corrupt[i], values[i], widths[i]);
        ASSERT_EQ(DATASTORE_RESULT_INVALID_FORMAT, DataStoreBlobLoad(bytes, size, &blob));
        memcpy(bytes, saved, size);
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
    DataStoreBlobDestroy(blob);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Dotted field names bind across layouts and keep matching after table re-registration.
TEST(Data, InlineNamesBindDifferentTablesAndTrackRegistration)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreQueryItem field = { dmHashString64("1.10"), DATASTORE_TYPE_VECTOR3, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryDesc query_desc = { { field }, 0, 0, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &query_desc, &query));
    DataStoreFieldDesc  members[] = { { 10, DATASTORE_TYPE_VECTOR3, 0, 0, "10" }, { 20, DATASTORE_TYPE_NUMBER, 16, 0, "20" } };
    DataStoreStructDesc light = { members, 2, 24 };
    uint64_t            names[] = { 10, 20 };
    DataStoreValueType  child_types[] = { DATASTORE_TYPE_VECTOR3, DATASTORE_TYPE_NUMBER };
    DataStoreValueData  children[2] = {};
    children[0].m_Vector3[0] = 1;
    children[0].m_Vector3[1] = 2;
    children[0].m_Vector3[2] = 3;
    children[1].m_Number = 4;
    DataStoreValue      value = Struct(names, child_types, children, 2);
    DataStoreId         ids[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        DataStoreFieldDesc root = { 1, DATASTORE_TYPE_STRUCT, i* 8, i < 2 ? &light : 0, "1" };
        DataStoreTableDesc table = { i + 1, 0, 0, &root, 1, i* 8 + (i < 2 ? 24u : 8u), DATASTORE_RESET_RETAIN };
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
        ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(store, i + 1, 0, &value, 1, &ids[i], 0));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    DataStoreIterator it      = DataStoreQueryIter(query);
    uint32_t          visited = 0;
    while (DataStoreIterNext(&it))
    {
        ASSERT_TRUE(DataStoreIterGetType(&it) < 3); // Dynamic structs cannot promise a member layout.
        DataStoreVector3 color;
        ASSERT_EQ(DATASTORE_RESULT_OK, GetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(1.0f, color.m_Values[0]);
        color.m_Values[0] = 8;
        ASSERT_EQ(DATASTORE_RESULT_OK, SetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(DATASTORE_RESULT_OK, GetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(8.0f, color.m_Values[0]);
        ++visited;
    }
    ASSERT_EQ(2u, visited);
    DataStoreQueryEnd(query);
    DataStoreValue object, intensity;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[0], 1, &object));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&object.m_Value.m_Struct, 20, &intensity));
    ASSERT_EQ(4.0, intensity.m_Value.m_Number);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableReset(store, 1));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    it = DataStoreQueryIter(query);
    ASSERT_TRUE(DataStoreIterNext(&it));
    DataStoreVector3 color;
    ASSERT_EQ(DATASTORE_RESULT_OK, GetTestFieldVector3(&it, 0, 0, &color));
    ASSERT_EQ(1.0f, color.m_Values[0]);
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableRemove(store, 1));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    it = DataStoreQueryIter(query);
    ASSERT_TRUE(DataStoreIterNext(&it));
    ASSERT_EQ((uint64_t)2, DataStoreIterGetType(&it));
    ASSERT_FALSE(DataStoreIterNext(&it));
    DataStoreQueryEnd(query);
    DataStoreQueryDestroy(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Component reset leaves other rows unchanged and reuses the existing value storage.
TEST(Data, ResetOneComponentPreservesOtherRowsAndReusesStorage)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreId        group = NextTestGroupId();
    DataStoreFieldDesc fields[10] = {};
    DataStoreValue     values[10];
    for (uint32_t p = 0; p < 10; ++p)
    {
        fields[p].m_Field = p;
        fields[p].m_Type = DATASTORE_TYPE_NUMBER;
        fields[p].m_Offset = p * 8;
        values[p] = Number(p);
    }
    DataStoreTableDesc table = { 1, 0, 0, fields, 10, 80, DATASTORE_RESET_RETAIN };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    DataStoreId ids[3];
    for (uint32_t r = 0; r < 3; ++r)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(store, 1, r == 2 ? 0 : group, values, 10, &ids[r], r + 11));
    }
    DataStoreMemoryStats before, after;
    GetDataMemoryStats(store, &before);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[0]));
    GetDataMemoryStats(store, &after);
    ASSERT_EQ(before.m_TotalBytes, after.m_TotalBytes);

    for (uint32_t r = 0; r < 3; ++r)
    {
        for (uint32_t p = 0; p < 10; ++p)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetNumber(store, ids[r], p, 100 * (r + 1) + p));
        }
    }
    DataStoreQueryItem field = { 9, DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryDesc desc = { { field }, &group, 1, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[0]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    DataStoreIterator it = DataStoreQueryIter(query);
    ASSERT_TRUE(DataStoreIterNext(&it));
    ASSERT_EQ(2u, DataStoreIterGetCount(&it));
    double number;
    ASSERT_EQ(DATASTORE_RESULT_OK, GetTestFieldNumber(&it, 0, 0, &number));
    ASSERT_EQ(9.0, number);
    ASSERT_EQ(DATASTORE_RESULT_OK, GetTestFieldNumber(&it, 1, 0, &number));
    ASSERT_EQ(209.0, number);

    ASSERT_FALSE(DataStoreIterNext(&it));
    DataStoreQueryEnd(query);
    for (uint32_t r = 0; r < 3; ++r)
    {
        ASSERT_EQ((uint64_t)(r + 11), DataStoreRowGetComponentId(store, ids[r]));
        for (uint32_t p = 0; p < 10; ++p)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, ids[r], p, &number));
            ASSERT_EQ((double)(r ? 100 * (r + 1) + p : p), number);
        }
    }
    GetDataMemoryStats(store, &before);
    for (uint32_t cycle = 0; cycle < 16; ++cycle)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetNumber(store, ids[0], 9, 500));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[0]));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, ids[0], 9, &number));
        ASSERT_EQ(9.0, number);
    }
    GetDataMemoryStats(store, &after);
    ASSERT_EQ(before.m_TotalBytes, after.m_TotalBytes);
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreRowReset(store, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[0]));
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreRowReset(store, ids[0]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[2])); // Swap removal moved this row.
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, ids[2], 9, &number));
    ASSERT_EQ(9.0, number);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, ids[1], 9, &number));
    ASSERT_EQ(209.0, number);
    DataStoreQueryDestroy(query);

    DataStoreTableDesc empty = { 2, 0, 0, 0, 0, 0, DATASTORE_RESET_RETAIN };
    DataStoreId        empty_id;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &empty));
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(store, 2, 0, 0, 0, &empty_id, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, empty_id));
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreRowReset(store, ids[0]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Queries without field filters still visit native and packed rows with correct IDs/groups.
TEST(Data, RowIterationWithoutFieldFilters)
{
    HDataStoreContext  source = DataStoreCreate(0);
    DataStoreFieldDesc meta[] = { { 10, DATASTORE_TYPE_NUMBER, 0 }, { 20, DATASTORE_TYPE_STRING, 8 }, { 30, DATASTORE_TYPE_VECTOR3, 16 }, { 40, DATASTORE_TYPE_NULL, 0 } };
    DataStoreTableDesc table = { 1, 0, 0, meta, 4, 32 };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(source, &table));
    DataStoreValue values[] = { Number(10), String("original"), Vector3(1, 2, 3), Null() };
    DataStoreId    id;
    DataStoreId    groups[] = { NextTestGroupId(), NextTestGroupId() };
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, 1, groups[0], values, 4, &id, 0));
    values[0] = Number(20);
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, 1, groups[1], values, 4, &id, 0));
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, size, &size));
    HDataStoreContext      loaded = DataStoreCreate(0);
    HDataStoreBlobInstance instance;
    ASSERT_EQ(DATASTORE_RESULT_OK, Load(loaded, bytes, size, &instance, 0));
    HDataStoreContext stores[] = { source, loaded };
    for (uint32_t mode = 0; mode < 2; ++mode)
    {
        HDataStoreContext  store = stores[mode];
        DataStoreQueryDesc desc = {};
        HDataStoreQuery    query;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
        DataStoreIterator    batch = DataStoreQueryIter(query);
        DataStoreRowIterator empty_rows = DataStoreIterRows(&batch);
        ASSERT_FALSE(DataStoreRowIterNext(&empty_rows));
        ASSERT_EQ((DataStoreId)0, DataStoreRowIterGetId(&empty_rows));
        ASSERT_TRUE(DataStoreIterNext(&batch));
        DataStoreRowIterator rows    = DataStoreIterRows(&batch);
        uint32_t             visited = 0;
        while (DataStoreRowIterNext(&rows))
        {
            id = DataStoreRowIterGetId(&rows);
            ASSERT_NE((DataStoreId)0, id);
            ASSERT_EQ(mode ? (DataStoreId)0 : groups[visited], DataStoreRowIterGetGroupId(&rows));
            double number;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, id, 10, &number));
            ASSERT_EQ(10.0 + 10.0 * visited, number);
            ++visited;
        }
        ASSERT_EQ(2u, visited);
        ASSERT_FALSE(DataStoreRowIterNext(&rows));
        ASSERT_EQ((DataStoreId)0, DataStoreRowIterGetId(&rows));
        ASSERT_FALSE(DataStoreIterNext(&batch));
        DataStoreQueryEnd(query);
        DataStoreQueryDestroy(query);
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(loaded));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(source));
    delete[] bytes;
}

// Row access stays relative to its query batch after swap removal moves row records.
TEST(Data, RowAccessUsesBatchRelativeRowsAfterRemoval)
{
    HDataStoreContext store = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(store, 1, 0, 0));
    DataStoreId group = NextTestGroupId();
    DataStoreId ids[6];
    for (uint32_t row = 0; row < 6; ++row)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, Add(store, 1, row % 2 ? group : 0, row + 10, "base", &ids[row]));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[1])); // Last row moves; its base bytes do not.
    DataStoreQueryItem field = { 10, DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD };
    for (uint32_t requested = 0; requested < 2; ++requested)
    {
        DataStoreQueryDesc desc = { { 0 }, &group, 1, 0 };
        if (requested)
        {
            desc.m_Items[0] = field;
        }
        HDataStoreQuery query;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
        DataStoreFieldId handle = DataStoreQueryFindField(query, &field);
        ASSERT_EQ(requested != 0, handle != UINT32_MAX);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
        DataStoreIterator batch   = DataStoreQueryIter(query);
        uint32_t          visited = 0;
        while (DataStoreIterNext(&batch))
        {
            DataStoreRowIterator rows = DataStoreIterRows(&batch);
            while (DataStoreRowIterNext(&rows))
            {
                DataStoreId id = DataStoreRowIterGetId(&rows);
                ASSERT_TRUE(id == ids[3] || id == ids[5]);
                ASSERT_EQ(group, DataStoreRowIterGetGroupId(&rows));
                if (requested)
                {
                    ASSERT_EQ(id == ids[3] ? 13.0 : 15.0, *DataStoreRowIterGetNumber(&rows, handle));
                    *DataStoreRowIterGetNumberMut(&rows, handle) = 99;
                    ASSERT_EQ(99.0, *DataStoreRowIterGetNumber(&rows, handle));
                }
                ++visited;
            }
        }
        ASSERT_EQ(2u, visited);
        DataStoreQueryEnd(query);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[3]));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[5]));
        DataStoreQueryDestroy(query);
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// IDs collected during queries support deferred deletion and reject rows already removed.
TEST(Data, DeferredRemovalUsesStableIds)
{
    HDataStoreContext store = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(store, 1, 0, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(store, 2, 0, 0));
    DataStoreQueryItem field = { 10, DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryDesc desc = { { field }, 0, 0, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    DataStoreFieldId     handle = DataStoreQueryFindField(query, &field);
    dmArray<DataStoreId> removals;
    removals.SetCapacity(64);
    DataStoreId ids[64];
    // Reuse both the caller's deletion buffer and released store ID slots.
    for (uint32_t cycle = 0; cycle < 2; ++cycle)
    {
        for (uint32_t r = 0; r < 64; ++r)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, Add(store, 1 + r / 32, 0, r, "base", &ids[r]));
        }
        removals.SetSize(0);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
        DataStoreIterator batch   = DataStoreQueryIter(query);
        uint32_t          visited = 0;
        while (DataStoreIterNext(&batch))
        {
            DataStoreRowIterator rows = DataStoreIterRows(&batch);
            while (DataStoreRowIterNext(&rows))
            {
                uint32_t index = (uint32_t)*DataStoreRowIterGetNumber(&rows, handle);
                ASSERT_EQ(ids[index], DataStoreRowIterGetId(&rows));
                ASSERT_EQ((double)index, *DataStoreRowIterGetNumber(&rows, handle));
                if (index % 2 == 0 || index == 63)
                {
                    removals.Push(DataStoreRowIterGetId(&rows));
                }
                ++visited;
            }
        }
        ASSERT_EQ(64u, visited);
        ASSERT_EQ(33u, removals.Size());
        // Queued IDs are still readable before the caller applies its buffer.
        double value;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, removals[0], 10, &value));
        DataStoreQueryEnd(query);
        for (uint32_t r = 0; r < removals.Size(); ++r)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, removals[r]));
        }
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
        batch = DataStoreQueryIter(query);
        visited = 0;
        while (DataStoreIterNext(&batch))
        {
            DataStoreRowIterator rows = DataStoreIterRows(&batch);
            while (DataStoreRowIterNext(&rows))
            {
                uint32_t index = (uint32_t)*DataStoreRowIterGetNumber(&rows, handle);
                ASSERT_TRUE(index % 2 == 1 && index != 63);
                ASSERT_EQ(ids[index], DataStoreRowIterGetId(&rows));
                ++visited;
            }
        }
        ASSERT_EQ(31u, visited);
        DataStoreQueryEnd(query);
        for (uint32_t r = 0; r < 64; ++r)
        {
            if (r % 2 == 0 || r == 63)
            {
                ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGetNumber(store, ids[r], 10, &value));
            }
            else
            {
                ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[r]));
            }
        }
        ASSERT_EQ(64u, removals.Capacity());
    }
    DataStoreQueryDestroy(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Rows with no fields are valid and appear once in row iteration.
TEST(Data, EmptyRowHasNoFields)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreTableDesc table = { 1, 0, 0, 0, 0, 0 };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    DataStoreId id;
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(store, 1, 0, 0, 0, &id, 0));
    DataStoreQueryDesc desc = {};
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    DataStoreIterator batch = DataStoreQueryIter(query);
    ASSERT_TRUE(DataStoreIterNext(&batch));
    DataStoreRowIterator rows = DataStoreIterRows(&batch);
    ASSERT_TRUE(DataStoreRowIterNext(&rows));
    ASSERT_EQ(id, DataStoreRowIterGetId(&rows));
    ASSERT_FALSE(DataStoreRowIterNext(&rows));
    ASSERT_FALSE(DataStoreIterNext(&batch));
    DataStoreQueryEnd(query);
    DataStoreQueryDestroy(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Native registration and blob loading reject layouts that violate C alignment.
TEST(Data, AlignedLayoutValidation)
{
    HDataStoreContext   store = DataStoreCreate(0);
    DataStoreFieldDesc  members[] = { { 1, DATASTORE_TYPE_NUMBER, 0, 0, "1" }, { 2, DATASTORE_TYPE_BOOLEAN, 8, 0, "2" } };
    DataStoreStructDesc inner = { members, 2, 16 };
    DataStoreFieldDesc  root = { 3, DATASTORE_TYPE_STRUCT, 0, &inner, "3" };
    DataStoreTableDesc  table = { 1, 0, 0, &root, 1, 24 };
    members[0].m_Offset = 1;
    members[1].m_Offset = 9;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableAdd(store, &table));
    members[0].m_Offset = 0;
    members[1].m_Offset = 8;
    inner.m_Size = 9;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableAdd(store, &table));
    inner.m_Size = 16;
    root.m_Offset = 4;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableAdd(store, &table));
    root.m_Offset = 8;
    table.m_RowStride = 25;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableAdd(store, &table));
    table.m_RowStride = 24;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    uint64_t                 names[] = { 1, 2 };
    const DataStoreValueType child_types[] = { DATASTORE_TYPE_NUMBER, DATASTORE_TYPE_BOOLEAN };
    DataStoreValueData       children[2] = {};
    children[0].m_Number = 17;
    children[1].m_Boolean = 1;
    DataStoreValue           value = Struct(names, child_types, children, 2);
    DataStoreId              id;
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(store, 1, 0, &value, 1, &id, 0));
    uint8_t DM_ALIGNED(8) bytes[512], saved[512];
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, bytes, sizeof(bytes), &size));
    memcpy(saved, bytes, size);
    HDataStoreBlob blob;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
    // Reject unaligned input once, before any direct pointer can be obtained.
    uint8_t* unaligned = new uint8_t[size + 1];
    memcpy(unaligned + 1, bytes, size);
    HDataStoreBlob output = blob;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_FORMAT, DataStoreBlobLoad(unaligned + 1, size, &output));
    ASSERT_EQ((uintptr_t)blob, (uintptr_t)output);
    delete[] unaligned;
    uint32_t       table_offset = (uint32_t)ReadDataInteger(bytes + 16, 4);
    uint32_t       metadata = table_offset + DATA_TABLE_HEADER_SIZE;
    const size_t   offsets[] = { 4, table_offset + offsetof(DataStoreTableHeader, m_RowStride), metadata + offsetof(DataStoreFileFieldMeta, m_ByteOffset), metadata + offsetof(DataStoreFileFieldMeta, m_ByteSize), metadata + DATA_FIELD_META_SIZE + offsetof(DataStoreFileFieldMeta, m_ByteOffset) };
    const uint32_t values[] = { 7, 25, 4, 9, 1 };
    for (uint32_t i = 0; i < 5; ++i)
    {
        WriteDataInteger(bytes + offsets[i], values[i], 4);
        ASSERT_EQ(DATASTORE_RESULT_INVALID_FORMAT, DataStoreBlobLoad(bytes, size, &output));
        ASSERT_EQ((uintptr_t)blob, (uintptr_t)output);
        memcpy(bytes, saved, size);
    }
    DataStoreBlobDestroy(blob);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

extern "C" int TestDataPointersFromC(HDataStoreContext store, int write);

// Native pointers retain typed values while edits and reset leave borrowed blob bytes intact.
TEST(Data, NativeFieldPointersAndReset)
{
    struct NativeRow
    {
        double           m_Number;
        uint8_t          m_Boolean;
        DataStoreVector3 m_Vector3;
        DataStoreVector4 m_Vector4;
        DataStoreMatrix4 m_Matrix4;
    };
    DataStoreFieldDesc fields[] = {
        { 1, DATASTORE_TYPE_NUMBER, offsetof(NativeRow, m_Number) },
        { 2, DATASTORE_TYPE_BOOLEAN, offsetof(NativeRow, m_Boolean) },
        { 3, DATASTORE_TYPE_VECTOR3, offsetof(NativeRow, m_Vector3) },
        { 4, DATASTORE_TYPE_VECTOR4, offsetof(NativeRow, m_Vector4) },
        { 5, DATASTORE_TYPE_MATRIX4, offsetof(NativeRow, m_Matrix4) }
    };
    DataStoreTableDesc table = { 1, 0, 0, fields, 5, sizeof(NativeRow), DATASTORE_RESET_RETAIN };
    HDataStoreContext  source = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(source, &table));
    DataStoreValue values[] = { Number(17), Boolean(0), Vector3(1, 2, 3), {}, Matrix() };
    values[3].m_Type = DATASTORE_TYPE_VECTOR4;
    values[3].m_Value.m_Vector4[3] = 4;
    for (uint32_t i = 0; i < 16; ++i)
    {
        values[4].m_Value.m_Matrix4[i] = (float)i;
    }
    DataStoreId ids[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, 1, 0, values, 5, &ids[i], 0));
    }
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    uint8_t* saved = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, size, &size));
    memcpy(saved, bytes, size);
    HDataStoreContext      loaded = DataStoreCreate(0);
    HDataStoreBlobInstance instance;
    ASSERT_EQ(DATASTORE_RESULT_OK, Load(loaded, bytes, size, &instance, 0));
    HDataStoreContext stores[] = { source, loaded };
    for (uint32_t packed = 0; packed < 2; ++packed)
    {
        HDataStoreContext store = stores[packed];
        ASSERT_EQ(0, TestDataPointersFromC(store, 0));
        DataStoreMemoryStats before, after;
        GetDataMemoryStats(store, &before);
        ASSERT_GT(before.m_ValueBytes, (uint64_t)0);
        ASSERT_EQ((uint64_t)0, before.m_PayloadBlocks);
        ASSERT_EQ(0, TestDataPointersFromC(store, 1));
        GetDataMemoryStats(store, &after);
        ASSERT_EQ(before.m_ValueBytes, after.m_ValueBytes);
        ASSERT_EQ(before.m_TotalBytes, after.m_TotalBytes);
        ASSERT_EQ((uint64_t)0, after.m_PayloadBlocks);
        DataStoreId id = FirstId(store);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, id));
        double number;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, id, 1, &number));
        ASSERT_EQ(17.0, number);
        if (packed)
        {
            DataStoreBlobReset(instance);
        }
        else
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableReset(store, 1));
        }
        ASSERT_EQ(0, TestDataPointersFromC(store, 0));
        ASSERT_EQ(0, memcmp(saved, bytes, size));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(loaded));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(source));
    delete[] saved;
    delete[] bytes;
}

// Bound field handles remain stable while matching tables are added, removed and reset.
TEST(Data, QueryFieldHandlesSurviveTableChanges)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreQueryItem fields[] = {
        { 30, DATASTORE_TYPE_VECTOR3, 0, DATASTORE_QUERY_ITEM_FIELD },
        { 10, DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD }
    };
    DataStoreQueryDesc desc = { { fields[0], fields[1] }, 0, 0, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    DataStoreFieldId vector_field = DataStoreQueryFindField(query, &fields[0]);
    DataStoreFieldId number_field = DataStoreQueryFindField(query, &fields[1]);
    ASSERT_NE(UINT32_MAX, vector_field);
    ASSERT_NE(UINT32_MAX, number_field);
    ASSERT_NE(vector_field, number_field);
    DataStoreQueryItem invalid = { 99, DATASTORE_TYPE_VECTOR3, 0, DATASTORE_QUERY_ITEM_FIELD };
    ASSERT_EQ(UINT32_MAX, DataStoreQueryFindField(query, &invalid));
    invalid.m_Hash = 30;
    invalid.m_Type = DATASTORE_TYPE_NUMBER;
    ASSERT_EQ(UINT32_MAX, DataStoreQueryFindField(query, &invalid));

    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    DataStoreIterator empty = DataStoreQueryIter(query);
    ASSERT_FALSE(DataStoreIterNext(&empty));
    DataStoreQueryEnd(query);

    // Reuse the handles after END and across registration/removal with new offsets.
    for (uint32_t pass = 0; pass < 3; ++pass)
    {
        DataStoreFieldDesc meta[] = { { 10, DATASTORE_TYPE_NUMBER, pass* 8 }, { 30, DATASTORE_TYPE_VECTOR3, pass* 8 + 8 } };
        DataStoreTableDesc table = { 1, 0, 0, meta, 2, pass* 8 + 24, DATASTORE_RESET_RETAIN };
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
        DataStoreValue values[] = { Number(10 + pass), Vector3(1, 2, 3) };
        DataStoreId    id;
        ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(store, 1, 0, values, 2, &id, 0));
        ASSERT_EQ(vector_field, DataStoreQueryFindField(query, &fields[0]));
        ASSERT_EQ(number_field, DataStoreQueryFindField(query, &fields[1]));
        for (uint32_t traversal = 0; traversal < 2; ++traversal)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
            DataStoreIterator it = DataStoreQueryIter(query);
            ASSERT_TRUE(DataStoreIterNext(&it));
            DataStoreRowIterator rows = DataStoreIterRows(&it);
            ASSERT_TRUE(DataStoreRowIterNext(&rows));
            ASSERT_EQ(id, DataStoreRowIterGetId(&rows));
            ASSERT_EQ(10.0 + pass, *DataStoreRowIterGetNumber(&rows, number_field));
            ASSERT_EQ(3.0f, DataStoreRowIterGetVector3(&rows, vector_field)->m_Values[2]);
            *DataStoreRowIterGetNumberMut(&rows, number_field) = 99;
            ASSERT_EQ(99.0, *DataStoreRowIterGetNumber(&rows, number_field));
            ASSERT_FALSE(DataStoreRowIterNext(&rows));
            ASSERT_FALSE(DataStoreIterNext(&it));
            DataStoreQueryEnd(query);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, id));
        }
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableRemove(store, 1));
    }
    DataStoreQueryDestroy(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Nested field pointers share payload storage without overwriting sibling members.
TEST(Data, NestedFieldPointersSharePayloadsAndPreserveSiblings)
{
    DataStoreFieldDesc       members[] = { { 10, DATASTORE_TYPE_VECTOR3, 0, 0, "10" }, { 20, DATASTORE_TYPE_NUMBER, 16, 0, "20" }, { 30, DATASTORE_TYPE_STRING, 24, 0, "30" } };
    DataStoreStructDesc      light = { members, 3, 32 };
    uint64_t                 names[] = { 10, 20, 30 };
    const DataStoreValueType types[] = { DATASTORE_TYPE_VECTOR3, DATASTORE_TYPE_NUMBER, DATASTORE_TYPE_STRING };
    DataStoreValueData       first[3] = {};
    first[0].m_Vector3[0] = 1;
    first[0].m_Vector3[1] = 2;
    first[0].m_Vector3[2] = 3;
    first[1].m_Number = 7;
    first[2].m_String = "shared";
    DataStoreValueData second[3] = {};
    second[0].m_Vector3[0] = 9;
    second[0].m_Vector3[1] = 8;
    second[0].m_Vector3[2] = 7;
    second[1].m_Number = 6;
    second[2].m_String = "other";
    DataStoreValue           values[] = { Struct(names, types, first, 3), Struct(names, types, second, 3) };
    HDataStoreContext        source = DataStoreCreate(0);
    for (uint32_t i = 0; i < 2; ++i)
    {
        DataStoreFieldDesc roots[] = { { 1, DATASTORE_TYPE_STRUCT, i* 8, &light, "1" }, { 2, DATASTORE_TYPE_STRUCT, i* 8 + 32, &light, "2" } };
        DataStoreTableDesc table = { i + 1, 0, 0, roots, 2, i* 8 + 64, DATASTORE_RESET_RETAIN };
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(source, &table));
        DataStoreId id;
        ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, i + 1, 0, values, 2, &id, 0));
    }
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    uint8_t* saved = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, size, &size));
    memcpy(saved, bytes, size);
    HDataStoreContext      loaded = DataStoreCreate(0);
    HDataStoreBlobInstance instance;
    ASSERT_EQ(DATASTORE_RESULT_OK, Load(loaded, bytes, size, &instance, 0));
    HDataStoreContext stores[] = { source, loaded };
    for (uint32_t packed = 0; packed < 2; ++packed)
    {
        HDataStoreContext  store = stores[packed];
        DataStoreQueryItem fields[] = {
            { dmHashString64("2.10"), DATASTORE_TYPE_VECTOR3, 0, DATASTORE_QUERY_ITEM_FIELD },
            { dmHashString64("1.10"), DATASTORE_TYPE_VECTOR3, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD }
        };
        DataStoreQueryDesc desc = { { fields[0], fields[1] }, 0, 0, 0 };
        HDataStoreQuery    query;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
        // Lookup uses full-name hashes even when the descriptors change order.
        DataStoreQueryItem swap = fields[0];
        fields[0] = fields[1];
        fields[1] = swap;
        DataStoreFieldId field = DataStoreQueryFindField(query, &fields[0]);
        DataStoreFieldId other = DataStoreQueryFindField(query, &fields[1]);
        ASSERT_NE(UINT32_MAX, field);
        ASSERT_NE(UINT32_MAX, other);
        ASSERT_NE(field, other);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
        DataStoreIterator it = DataStoreQueryIter(query);
        uint32_t          batches = 0;
        DataStoreId       ids[2];
        while (DataStoreIterNext(&it))
        {
            DataStoreRowIterator rows = DataStoreIterRows(&it);
            ASSERT_TRUE(DataStoreRowIterNext(&rows));
            DataStoreId             id = DataStoreRowIterGetId(&rows);
            const DataStoreVector3* original = DataStoreRowIterGetVector3(&rows, field);
            ASSERT_EQ(1.0f, original->m_Values[0]);
            ASSERT_EQ(9.0f, DataStoreRowIterGetVector3(&rows, other)->m_Values[0]);
            if (packed)
            {
                uint32_t              table_index = (uint32_t)DataStoreIterGetType(&it) - 1;
                const DataStoreTable* table       = GetInstanceTable(instance, table_index);
                ASSERT_EQ((uintptr_t)(GetRowBytes(table, 0) + table_index * 8), (uintptr_t)original);
            }
            ids[batches] = id;
            DataStoreRowIterGetVector3Mut(&rows, field)->m_Values[0] = 42;
            ASSERT_EQ(42.0f, DataStoreRowIterGetVector3(&rows, field)->m_Values[0]);
            ASSERT_EQ(9.0f, DataStoreRowIterGetVector3(&rows, other)->m_Values[0]);
            DataStoreMemoryStats before, after;
            GetDataMemoryStats(store, &before);
            DataStoreRowIterGetVector3Mut(&rows, field)->m_Values[1] = 43;
            GetDataMemoryStats(store, &after);
            ASSERT_EQ(before.m_PayloadUsed, after.m_PayloadUsed);
            ASSERT_EQ(before.m_PayloadBlocks, after.m_PayloadBlocks);
            DataStoreRowIterGetVector3Mut(&rows, field)->m_Values[2] = 44;
            ++batches;
        }
        ASSERT_EQ(2u, batches);
        DataStoreQueryEnd(query);
        for (uint32_t r = 0; r < 2; ++r)
        {
            DataStoreId    id = ids[r];
            DataStoreValue root, child;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, id, 1, &root));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&root.m_Value.m_Struct, 20, &child));
            ASSERT_EQ(7.0, child.m_Value.m_Number);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&root.m_Value.m_Struct, 30, &child));
            ASSERT_STREQ("shared", child.m_Value.m_String);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldReset(store, id, 1));
            // The same full-name hash works through ID access, including packed metadata.
            DataStoreVector3 copy;
            uint64_t         color_hash = dmHashString64("1.10");
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3(store, id, color_hash, &copy));
            ASSERT_EQ(1.0f, copy.m_Values[0]);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3Batch(store, 1, &id, color_hash, &copy));
            copy.m_Values[0] = 17;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetVector3(store, id, color_hash, &copy));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldReset(store, id, color_hash));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetString(store, id, dmHashString64("1.30"), "changed"));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldReset(store, id, dmHashString64("1.30")));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, id));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3(store, id, color_hash, &copy));
            ASSERT_EQ(2.0f, copy.m_Values[1]);
            ASSERT_EQ(3.0f, copy.m_Values[2]);
        }
        DataStoreQueryDestroy(query);
    }
    ASSERT_EQ(0, memcmp(saved, bytes, size));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(loaded));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(source));
    delete[] saved;
    delete[] bytes;
}

// Mutable inline references reset correctly after rows are removed from their blob instance.
TEST(Data, MutableInlineReferencesAndInstanceResetAfterRemoval)
{
    DataStoreFieldDesc  members[] = { { 10, DATASTORE_TYPE_NUMBER, 0, 0, "10" }, { 20, DATASTORE_TYPE_STRING, 8, 0, "20" }, { 30, DATASTORE_TYPE_LIST, 16, 0, "30" } };
    DataStoreStructDesc layout = { members, 3, 24 };
    DataStoreFieldDesc  root = { 1, DATASTORE_TYPE_STRUCT, 0, &layout, "1" };
    DataStoreTableDesc  table = { 1, 0, 0, &root, 1, 24 };
    HDataStoreContext   source = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(source, &table));
    uint64_t                 names[] = { 10, 20, 30 };
    DataStoreValue           list_values[] = { String("nested"), Number(7) };
    const DataStoreValueType types[] = { DATASTORE_TYPE_NUMBER, DATASTORE_TYPE_STRING, DATASTORE_TYPE_LIST };
    DataStoreValueData       values[] = { Number(0).m_Value, String("shared").m_Value, List(list_values, 2).m_Value };
    DataStoreValue           object = Struct(names, types, values, 3);
    for (uint32_t r = 0; r < 3; ++r)
    {
        values[0].m_Number = 10 + r;
        DataStoreId id;
        ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, 1, 0, &object, 1, &id, r));
    }
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    uint8_t* saved = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, size, &size));
    memcpy(saved, bytes, size);
    HDataStoreBlob blob;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
    HDataStoreContext      store = DataStoreCreate(0);
    DataStoreId            groups[] = { NextTestGroupId(), NextTestGroupId() };
    HDataStoreBlobInstance first, second;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(store, blob, groups[0], &first));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(store, blob, groups[1], &second));
    DataStoreBlobDestroy(blob);
    DataStoreQueryItem fields[] = {
        { dmHashString64("1.10"), DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD },
        { dmHashString64("1.20"), DATASTORE_TYPE_STRING, 0, DATASTORE_QUERY_ITEM_FIELD }
    };
    DataStoreQueryDesc desc = { { fields[0], fields[1] }, 0, 0, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    DataStoreFieldId number_field = DataStoreQueryFindField(query, &fields[0]);
    ASSERT_NE(UINT32_MAX, number_field);
    DataStoreId ids[2][3] = {};
    const char* shared_list_string = 0;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    DataStoreIterator it = DataStoreQueryIter(query);
    while (DataStoreIterNext(&it))
    {
        DataStoreRowIterator rows = DataStoreIterRows(&it);
        while (DataStoreRowIterNext(&rows))
        {
            DataStoreId id        = DataStoreRowIterGetId(&rows);
            uint32_t    group     = DataStoreRowIterGetGroupId(&rows) == groups[0] ? 0 : 1;
            uint32_t    component = (uint32_t)DataStoreRowGetComponentId(store, id);
            ids[group][component] = id;
            ASSERT_EQ(10.0 + component, *DataStoreRowIterGetNumber(&rows, number_field));
            ASSERT_EQ((uintptr_t)DataStoreRowIterGetNumber(&rows, number_field), (uintptr_t)DataStoreRowIterGetNumberMut(&rows, number_field));
            if (!group)
            {
                *DataStoreRowIterGetNumberMut(&rows, number_field) += 100;
            }
            DataStoreValue current, child, element;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, id, 1, &current));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&current.m_Value.m_Struct, 20, &child));
            ASSERT_STREQ("shared", child.m_Value.m_String);
            ASSERT_TRUE((uintptr_t)child.m_Value.m_String >= (uintptr_t)bytes && (uintptr_t)child.m_Value.m_String < (uintptr_t)(bytes + size));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, id, 1, &current));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&current.m_Value.m_Struct, 30, &child));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(&child.m_Value.m_List, 0, &element));
            ASSERT_STREQ("nested", element.m_Value.m_String);
            if (!shared_list_string)
            {
                shared_list_string = element.m_Value.m_String;
            }
            ASSERT_EQ((uintptr_t)shared_list_string, (uintptr_t)element.m_Value.m_String);
        }
    }
    DataStoreQueryEnd(query);
    for (uint32_t component = 0; component < 3; ++component)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetString(store, ids[0][component], dmHashString64("1.20"), "replacement"));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[0][0]));
    DataStoreBlobReset(first);
    ASSERT_EQ((uintptr_t)0, (uintptr_t)first->m_Payloads);
    for (uint32_t group = 0; group < 2; ++group)
    {
        for (uint32_t component = group ? 0 : 1; component < 3; ++component)
        {
            DataStoreValue current, child;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[group][component], 1, &current));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&current.m_Value.m_Struct, 10, &child));
            ASSERT_EQ(10.0 + component, child.m_Value.m_Number);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&current.m_Value.m_Struct, 20, &child));
            ASSERT_STREQ("shared", child.m_Value.m_String);
        }
    }
    ASSERT_EQ(0, memcmp(saved, bytes, size));
    DataStoreQueryDestroy(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(source));
    delete[] saved;
    delete[] bytes;
}

// Reset and unload leave independent rows that reused pooled registration slots untouched.
TEST(Data, PooledRegistrationResetAndRemovalAfterSlotReuse)
{
    HDataStoreContext source = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(source, 1, 0, 0));
    DataStoreTableDesc empty = { 2, 0, 0, 0, 0, 0 };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(source, &empty));
    DataStoreValue values[] = { Number(10), String("shared") };
    DataStoreId    id;
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, 1, 0, values, 2, &id, 11));
    values[0] = Number(20);
    ASSERT_EQ(DATASTORE_RESULT_OK, AddTestRow(source, 1, 0, values, 2, &id, 12));
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, size, &size));
    HDataStoreBlob blob;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
    HDataStoreContext      store = DataStoreCreate(0);
    HDataStoreBlobInstance first, second;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(store, blob, 0, &first));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(store, blob, 0, &second));
    DataStoreBlobDestroy(blob);
    ASSERT_EQ((uintptr_t)first->m_Pool, (uintptr_t)second->m_Pool);
    ASSERT_EQ(2u, store->m_Tables.Size());
    ASSERT_EQ(0u, GetInstanceTable(first, 1)->m_Rows.Size());
    DataStoreId            ids[2][2];
    HDataStoreBlobInstance instances[] = { first, second };
    for (uint32_t i = 0; i < 2; ++i)
    {
        for (uint32_t r = 0; r < 2; ++r)
        {
            uint32_t slot = GetInstanceSlots(instances[i])[r];
            ids[i][r] = ((uint64_t)store->m_Slots[slot].m_Generation << 32) | slot;
            ASSERT_EQ((uint64_t)(11 + r), DataStoreRowGetComponentId(store, ids[i][r]));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetNumber(store, ids[i][r], 10, 100 + i));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetString(store, ids[i][r], 20, i ? "second" : "first"));
        }
    }
    ASSERT_TRUE(first->m_Payloads != 0 && second->m_Payloads != 0);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[0][0]));
    ASSERT_EQ(UINT32_MAX, GetInstanceSlots(first)[0]);
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(store, 3, 0, 0));
    DataStoreId reused;
    ASSERT_EQ(DATASTORE_RESULT_OK, Add(store, 3, 0, 500, "independent", &reused));
    ASSERT_EQ((uint32_t)ids[0][0], (uint32_t)reused);
    ASSERT_NE(ids[0][0], reused);
    DataStoreBlobReset(first);
    ASSERT_EQ((uintptr_t)0, (uintptr_t)first->m_Payloads);
    ASSERT_TRUE(second->m_Payloads != 0);
    double      number;
    const char* string;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, ids[0][1], 10, &number));
    ASSERT_EQ(20.0, number);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetString(store, ids[0][1], 20, &string));
    ASSERT_STREQ("shared", string);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobRemove(first));
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGetNumber(store, ids[0][1], 10, &number));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, reused, 10, &number));
    ASSERT_EQ(500.0, number);
    for (uint32_t r = 0; r < 2; ++r)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, ids[1][r], 10, &number));
        ASSERT_EQ(101.0, number);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetString(store, ids[1][r], 20, &string));
        ASSERT_STREQ("second", string);
    }
    DataStoreBlobReset(second);
    for (uint32_t r = 0; r < 2; ++r)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, ids[1][r], 10, &number));
        ASSERT_EQ(10.0 + r * 10, number);
        ASSERT_EQ((uint64_t)(11 + r), DataStoreRowGetComponentId(store, ids[1][r]));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobRemove(second));
    ASSERT_EQ((uintptr_t)0, (uintptr_t)store->m_Pools);
    ASSERT_EQ(1u, store->m_Tables.Size());
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetString(store, reused, 20, &string));
    ASSERT_STREQ("independent", string);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(source));
    delete[] bytes;
}

// Metadata and tag counts round-trip at the 65,535-entry limit without truncation.
TEST(Data, CompactMetadataLimitsRoundTrip)
{
    // A reusable binary tree reaches 65,535 entries with only two siblings per
    // level, keeping validation linear instead of constructing one huge flat type.
    DataStoreStructDesc layouts[15] = {};
    DataStoreFieldDesc  members[15][2] = {};
    for (uint32_t depth = 0; depth < 15; ++depth)
    {
        for (uint32_t i = 0; i < 2; ++i)
        {
            DataStoreFieldDesc field = {
                i + 1,
                depth == 14 ? DATASTORE_TYPE_NULL : DATASTORE_TYPE_STRUCT,
                0,
                depth == 14 ? 0 : &layouts[depth + 1],
                i ? "right" : "left"
            };
            members[depth][i] = field;
        }
        layouts[depth].m_Fields = members[depth];
        layouts[depth].m_FieldCount = 2;
    }
    DataStoreFieldDesc roots[] = {
        { 1, DATASTORE_TYPE_STRUCT, 0, layouts, "1" },
        { 2, DATASTORE_TYPE_NULL, 0, 0, "2" }
    };
    uint64_t* tags = new uint64_t[DATA_MAX_TAG_COUNT];
    for (uint32_t i = 0; i < DATA_MAX_TAG_COUNT; ++i)
    {
        tags[i] = i;
    }
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreTableDesc desc = { 1, tags, DATA_MAX_TAG_COUNT + 1, roots, 1 };
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableAdd(store, &desc));
    desc.m_TagCount = DATA_MAX_TAG_COUNT;
    desc.m_FieldCount = DATA_MAX_METADATA_COUNT + 1;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableAdd(store, &desc));
    desc.m_FieldCount = 2;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableAdd(store, &desc)); // Nested total is 65,536.
    ASSERT_EQ(0u, store->m_Tables.Size());
    desc.m_FieldCount = 1;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &desc));
    ASSERT_EQ(DATA_MAX_METADATA_COUNT, (uint32_t)store->m_Tables[0]->m_MetadataCount);
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, bytes, size, &size));
    HDataStoreContext      loaded = DataStoreCreate(0);
    HDataStoreBlobInstance instance;
    ASSERT_EQ(DATASTORE_RESULT_OK, Load(loaded, bytes, size, &instance, 0));
    DataStoreTable* table = GetInstanceTable(instance, 0);
    ASSERT_EQ(DATA_MAX_METADATA_COUNT, (uint32_t)table->m_MetadataCount);
    ASSERT_EQ(DATA_MAX_TAG_COUNT, (uint32_t)table->m_TagCount);
    ASSERT_EQ((uint64_t)(DATA_MAX_TAG_COUNT - 1), GetTableTag(table, DATA_MAX_TAG_COUNT - 1));
    for (uint32_t i = 0; i < DATA_MAX_METADATA_COUNT; ++i)
    {
        DataStoreFieldMeta expected = GetFieldMeta(store->m_Tables[0], i);
        DataStoreFieldMeta actual = GetFieldMeta(table, i);
        ASSERT_EQ(expected.m_Field, actual.m_Field);
        ASSERT_EQ(expected.m_Name, actual.m_Name);
        ASSERT_EQ(expected.m_Offset, actual.m_Offset);
        ASSERT_EQ(expected.m_ChildIndex, actual.m_ChildIndex);
        ASSERT_EQ(expected.m_ChildCount, actual.m_ChildCount);
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(loaded));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    delete[] bytes;
    delete[] tags;
}

// Compact metadata preserves 32-bit row strides and field offsets above 64 KiB.
TEST(Data, CompactMetadataPreservesWideByteOffsets)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreFieldDesc field = { 10, DATASTORE_TYPE_VECTOR3, 65536 };
    DataStoreTableDesc table = { 1, 0, 0, &field, 1, 65548 };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    DataStoreValueType type = DATASTORE_TYPE_VECTOR3;
    DataStoreValueData value = {};
    value.m_Vector3[0] = 3;
    value.m_Vector3[1] = 5;
    value.m_Vector3[2] = 7;
    DataStoreId id;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowAdd(store, 1, 0, &type, &value, 1, &id, 0));
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, bytes, size, &size));
    HDataStoreContext      loaded = DataStoreCreate(0);
    HDataStoreBlobInstance instance;
    ASSERT_EQ(DATASTORE_RESULT_OK, Load(loaded, bytes, size, &instance, 0));
    DataStoreQueryItem requested = { 10, DATASTORE_TYPE_VECTOR3, 0, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryDesc desc = { { requested }, 0, 0, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(loaded, &desc, &query));
    DataStoreFieldId handle = DataStoreQueryFindField(query, &requested);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    DataStoreIterator it = DataStoreQueryIter(query);
    ASSERT_TRUE(DataStoreIterNext(&it));
    DataStoreRowIterator rows = DataStoreIterRows(&it);
    ASSERT_TRUE(DataStoreRowIterNext(&rows));
    const DataStoreVector3* actual = DataStoreRowIterGetVector3(&rows, handle);
    ASSERT_EQ(0, memcmp(value.m_Vector3, actual, sizeof(DataStoreVector3)));
    ASSERT_EQ(65548u, it.m_RowStride);
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(loaded));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    delete[] bytes;
}

// Interleaved items preserve nested names, NULL fields, group zero and copied setup inputs.
TEST(Data, InlineQueryFieldsAndTags)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreId        group = 0;
    char               name[] = "light.color";
    DataStoreQueryDesc desc = {
        { DataStoreTag("light"),
          DataStoreField(name, DATASTORE_TYPE_VECTOR3, DATASTORE_FIELD_MUTABLE),
          DataStoreTag("visible"),
          DataStoreField("unused", DATASTORE_TYPE_NULL, 0) },
        &group,
        1,
        0
    };
    name[0] = 'X';
    HDataStoreQuery query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    ASSERT_EQ(2u, query->m_Fields.Size());
    ASSERT_EQ(2u, query->m_Tags.Size());
    DataStoreFieldId color_field = DataStoreQueryFindField(query, &desc.m_Items[1]);
    ASSERT_EQ(0u, color_field);
    ASSERT_EQ(UINT32_MAX, DataStoreQueryFindField(query, &desc.m_Items[0]));
    ASSERT_EQ(UINT32_MAX, DataStoreQueryFindField(query, &desc.m_Items[2]));
    memset(&desc, 0, sizeof(desc));

    DataStoreFieldDesc  color = { dmHashString64("color"), DATASTORE_TYPE_VECTOR3, 0, 0, "color" };
    DataStoreStructDesc light = { &color, 1, sizeof(DataStoreVector3) };
    DataStoreFieldDesc  fields[] = {
        { dmHashString64("light"), DATASTORE_TYPE_STRUCT, 0, &light, "light" },
        { dmHashString64("unused"), DATASTORE_TYPE_NULL, sizeof(DataStoreVector3) }
    };
    uint64_t           tags[] = { dmHashString64("light"), dmHashString64("visible") };
    DataStoreTableDesc table = { 1, tags, 2, fields, 2, sizeof(DataStoreVector3) };
    DataStoreVector3   value = { { 1, 2, 3 } };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, 0, 1, &value, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, NextTestGroupId(), 1, &value, 0));
    table.m_Type = 2;
    table.m_TagCount = 1;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 2, 0, 1, &value, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    ASSERT_EQ(1u, DataStoreQueryGetRowCount(query));
    DataStoreIterator it = DataStoreQueryIterRange(query, 0, DataStoreQueryGetRowCount(query));
    ASSERT_TRUE(DataStoreIterNext(&it));
    DataStoreRowIterator rows = DataStoreIterRows(&it);
    ASSERT_TRUE(DataStoreRowIterNext(&rows));
    DataStoreRowIterGetVector3Mut(&rows, color_field)->m_Values[0] = 7;
    ASSERT_EQ(7.0f, DataStoreRowIterGetVector3(&rows, color_field)->m_Values[0]);
    ASSERT_FALSE(DataStoreRowIterNext(&rows));
    ASSERT_FALSE(DataStoreIterNext(&it));
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Full mixed item arrays need no terminator; invalid kinds/types are rejected before copying.
TEST(Data, InlineQueryCapacity)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreQueryDesc desc = {};
    for (uint32_t i = 0; i < DATASTORE_QUERY_MAX_ITEMS; i += 2)
    {
        desc.m_Items[i] = { i + 1, DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_FIELD };
        desc.m_Items[i + 1] = { i + 1, DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_TAG };
    }
    HDataStoreQuery query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    ASSERT_EQ((uint32_t)DATASTORE_QUERY_MAX_ITEMS / 2, query->m_Fields.Size());
    ASSERT_EQ((uint32_t)DATASTORE_QUERY_MAX_ITEMS / 2, query->m_Tags.Size());
    ASSERT_NE(UINT32_MAX, DataStoreQueryFindField(query, &desc.m_Items[DATASTORE_QUERY_MAX_ITEMS - 2]));
    ASSERT_EQ(UINT32_MAX, DataStoreQueryFindField(query, &desc.m_Items[DATASTORE_QUERY_MAX_ITEMS - 1]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    query = 0;
    desc.m_Items[DATASTORE_QUERY_MAX_ITEMS - 1].m_Kind = (DataStoreQueryItemKind)99;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreQueryCreate(store, &desc, &query));
    ASSERT_EQ((HDataStoreQuery)0, query);
    desc.m_Items[DATASTORE_QUERY_MAX_ITEMS - 1].m_Kind = DATASTORE_QUERY_ITEM_TAG;
    desc.m_Items[DATASTORE_QUERY_MAX_ITEMS - 2].m_Type = (DataStoreValueType)256;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreQueryCreate(store, &desc, &query));
    desc = { { { 0, DATASTORE_TYPE_NULL, 0, DATASTORE_QUERY_ITEM_FIELD } }, 0, 0, 0 };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    ASSERT_EQ(1u, query->m_Fields.Size());
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    // Entries after the first unused item are outside the descriptor.
    desc.m_Items[2] = { 0, DATASTORE_TYPE_NUMBER, 0, (DataStoreQueryItemKind)99 };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    ASSERT_EQ(1u, query->m_Fields.Size());
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Item kinds distinguish a zero hash from an unused entry, for both tags and fields.
TEST(Data, QueryItemsIncludeZeroHashes)
{
    HDataStoreContext  store = DataStoreCreate(0);
    uint64_t           tag   = 0;
    DataStoreFieldDesc field = { 0, DATASTORE_TYPE_NUMBER };
    DataStoreTableDesc table = { 1, &tag, 1, &field, 1, sizeof(double) };
    double             value = 7;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, 0, 1, &value, 0));
    DataStoreQueryDesc desc = { { { 0, DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_FIELD }, { 0, DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_TAG } }, 0, 0, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    ASSERT_EQ(1u, query->m_Fields.Size());
    ASSERT_EQ(1u, query->m_Tags.Size());
    DataStoreFieldId handle = DataStoreQueryFindField(query, &desc.m_Items[0]);
    ASSERT_NE(UINT32_MAX, handle);
    ASSERT_EQ(UINT32_MAX, DataStoreQueryFindField(query, &desc.m_Items[1]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    DataStoreIterator it = DataStoreQueryIterRange(query, 0, DataStoreQueryGetRowCount(query));
    ASSERT_TRUE(DataStoreIterNext(&it));
    DataStoreRowIterator rows = DataStoreIterRows(&it);
    ASSERT_TRUE(DataStoreRowIterNext(&rows));
    ASSERT_EQ(value, *DataStoreRowIterGetNumber(&rows, handle));
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Zero and DATASTORE_FIELD_MUTABLE are valid access flags; reject invalid kinds/flags before narrowing.
TEST(Data, CompactQueryFieldsValidateBeforeNarrowing)
{
    HDataStoreContext   store = DataStoreCreate(0);
    DataStoreQueryDesc  desc = { { { 1, DATASTORE_TYPE_VECTOR3, 0, DATASTORE_QUERY_ITEM_FIELD } }, 0, 0, 0 };
    DataStoreQueryItem& field = desc.m_Items[0];
    HDataStoreQuery     query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    ASSERT_EQ(0u, DataStoreQueryFindField(query, &field));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    field.m_Access = DATASTORE_FIELD_MUTABLE;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    ASSERT_EQ(0u, DataStoreQueryFindField(query, &field));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    field.m_Type = (DataStoreValueType)256;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreQueryCreate(store, &desc, &query));
    field.m_Type = DATASTORE_TYPE_VECTOR3;
    field.m_Access = 2;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreQueryCreate(store, &desc, &query));
    field.m_Access = 256;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreQueryCreate(store, &desc, &query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Retired generations are excluded from free-slot counts used by later bulk appends.
TEST(Data, FreeSlotCountExcludesRetiredGenerations)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreTableDesc table = { 1 };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    DataStoreRowDesc rows[6] = {};
    DataStoreId      ids[6];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowAddBatch(store, 1, rows, 4, ids));
    ASSERT_EQ(0u, store->m_FreeSlotCount);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[0]));
    ASSERT_EQ(1u, store->m_FreeSlotCount);
    // An exhausted generation is retired permanently rather than reusable.
    store->m_Slots[(uint32_t)ids[1]].m_Generation = UINT32_MAX;
    DataStoreId retired = ((uint64_t)UINT32_MAX << 32) | (uint32_t)ids[1];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, retired));
    ASSERT_EQ(1u, store->m_FreeSlotCount);
    DataStoreId added[2];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowAddBatch(store, 1, rows, 2, added));
    ASSERT_EQ(0u, store->m_FreeSlotCount);
    ASSERT_EQ(5u, store->m_Slots.Size());
    ASSERT_NE(ids[0], added[0]);
    ASSERT_EQ((uint32_t)ids[0], (uint32_t)added[0]);
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreRowRemove(store, ids[0]));
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreRowRemove(store, retired));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableRemove(store, 1));
    ASSERT_EQ(4u, store->m_FreeSlotCount);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowAddBatch(store, 1, rows, 6, ids));
    ASSERT_EQ(0u, store->m_FreeSlotCount);
    ASSERT_EQ(7u, store->m_Slots.Size());
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Vector3 batch reads match scalar results for native, packed, stale and invalid IDs.
TEST(Data, BatchVector3MatchesScalarGetter)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreFieldDesc vector = { 42, DATASTORE_TYPE_VECTOR3, 0 };
    DataStoreFieldDesc number = { 42, DATASTORE_TYPE_NUMBER, 0 };
    DataStoreFieldDesc missing = { 43, DATASTORE_TYPE_VECTOR3, 0 };
    DataStoreFieldDesc collision[] = {
        { 58, DATASTORE_TYPE_VECTOR3, 0 },
        { 42, DATASTORE_TYPE_VECTOR3, 12 }
    };
    DataStoreTableDesc tables[] = {
        { 1, 0, 0, &vector, 1, 12 },
        { 2, 0, 0, &number, 1, 8 },
        { 3, 0, 0, &missing, 1, 12 },
        { 4, 0, 0, collision, 2, 24 }
    };
    DataStoreValueType vector_types[] = { DATASTORE_TYPE_VECTOR3, DATASTORE_TYPE_VECTOR3 };
    DataStoreValueType number_type = DATASTORE_TYPE_NUMBER;
    DataStoreValueData vectors[2] = {};
    vectors[0].m_Vector3[0] = 1;
    vectors[0].m_Vector3[1] = 2;
    vectors[0].m_Vector3[2] = 3;
    vectors[1].m_Vector3[0] = 4;
    vectors[1].m_Vector3[1] = 5;
    vectors[1].m_Vector3[2] = 6;
    DataStoreValueData scalar = { 1 };
    DataStoreId        valid[5];
    for (uint32_t i = 0; i < 4; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &tables[i]));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowAdd(store, tables[i].m_Type, 0, i == 1 ? &number_type : vector_types, i == 1 ? &scalar : vectors, tables[i].m_FieldCount, &valid[i], 0));
    }
    DataStoreId stale = valid[0];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, stale));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowAdd(store, 1, 0, vector_types, vectors, 1, &valid[0], 0));
    // Include a packed row alongside independent rows in the same store.
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, bytes, size, &size));
    HDataStoreBlob         blob;
    HDataStoreBlobInstance instance;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(store, blob, 0, &instance));
    valid[4] = MakeId(store, GetInstanceSlots(instance)[0]);
    DataStoreId      ids[83];
    DataStoreVector3 actual[83];
    DataStoreId      successful[] = { valid[0], valid[3], valid[4] };
    for (uint32_t i = 0; i < 83; ++i)
    {
        ids[i] = successful[i % 3];
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3Batch(store, 83, ids, 42, actual));
    for (uint32_t i = 0; i < 83; ++i)
    {
        DataStoreVector3 expected;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3(store, ids[i], 42, &expected));
        ASSERT_EQ(0, memcmp(&actual[i], &expected, sizeof(expected)));
    }

    // Failures before/after an internal batch boundary return the scalar error.
    // Output on error is deliberately not inspected: partial writes are allowed.
    DataStoreId bad[] = { valid[1], valid[2], stale, UINT64_MAX, 0 };
    uint32_t    positions[] = { 0, 1, 63, 64, 82 };
    for (uint32_t b = 0; b < sizeof(bad) / sizeof(bad[0]); ++b)
    {
        DataStoreVector3 ignored;
        DataStoreResult  expected = DataStoreFieldGetVector3(store, bad[b], 42, &ignored);
        ASSERT_NE(DATASTORE_RESULT_OK, expected);
        for (uint32_t p = 0; p < sizeof(positions) / sizeof(positions[0]); ++p)
        {
            uint32_t    index = positions[p];
            DataStoreId saved = ids[index];
            ids[index] = bad[b];
            ASSERT_EQ(expected, DataStoreFieldGetVector3Batch(store, 83, ids, 42, actual));
            ids[index] = saved;
        }
    }
    // The first error follows input order even when later errors differ.
    ids[3] = valid[2];
    ids[80] = valid[1];
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGetVector3Batch(store, 83, ids, 42, actual));
    ids[3] = valid[1];
    ids[80] = valid[2];
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFieldGetVector3Batch(store, 83, ids, 42, actual));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3Batch(0, 0, 0, 42, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobRemove(instance));
    DataStoreBlobDestroy(blob);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    delete[] bytes;
}

// Groups share native tables while filtered queries stay correct across row changes.
TEST(Data, NativeRowsShareGroupsWithoutSplittingTables)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreFieldDesc field = { 10, DATASTORE_TYPE_NUMBER };
    DataStoreTableDesc table = { 1, 0, 0, &field, 1, sizeof(double), DATASTORE_RESET_RETAIN };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    DataStoreId        group = NextTestGroupId();
    DataStoreQueryDesc desc = { { 0 }, &group, 1, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    double      values[] = { 1, 2, 3 };
    DataStoreId first[3], other[3], last[3];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, group, 3, values, first));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, 0, 3, values, other));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, group, 3, values, last));
    ASSERT_EQ(1u, store->m_Tables.Size());
    // Removing the first row swaps the last row into its slot; group filters survive.
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, first[0]));
    for (uint32_t pass = 0; pass < 2; ++pass)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
        ASSERT_EQ(5u, DataStoreQueryGetRowCount(query));
        DataStoreIterator it      = DataStoreQueryIterRange(query, 0, 5);
        uint32_t          visited = 0;
        while (DataStoreIterNext(&it))
        {
            DataStoreRowIterator rows = DataStoreIterRows(&it);
            while (DataStoreRowIterNext(&rows))
            {
                ASSERT_EQ(group, DataStoreRowIterGetGroupId(&rows));
                DataStoreId id = DataStoreRowIterGetId(&rows);
                ASSERT_NE(first[0], id);
                for (uint32_t i = 0; i < 3; ++i)
                {
                    ASSERT_NE(other[i], id);
                }
                ++visited;
            }
        }
        ASSERT_EQ(5u, visited);
        DataStoreQueryEnd(query);
        // A reset changes only values, leaving the shared group intact.
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetNumber(store, last[2], 10, 100));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, last[2]));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    desc.m_GroupIds = 0;
    desc.m_GroupIdCount = 0;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    DataStoreIterator it = DataStoreQueryIter(query);
    ASSERT_TRUE(DataStoreIterNext(&it));
    DataStoreRowIterator rows       = DataStoreIterRows(&it);
    uint32_t             zero_group = 0;
    while (DataStoreRowIterNext(&rows))
    {
        zero_group += DataStoreRowIterGetGroupId(&rows) == 0;
    }
    ASSERT_EQ(3u, zero_group);
    ASSERT_FALSE(DataStoreIterNext(&it));
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Native, SoA and decoded creation allow omitted output IDs without losing row identity.
TEST(Data, CreateRowsWithoutReturningIds)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreId        group = NextTestGroupId();
    DataStoreFieldDesc field = { 10, DATASTORE_TYPE_NUMBER };
    DataStoreTableDesc table = { 1, 0, 0, &field, 1, sizeof(double) };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    DataStoreQueryItem query_field = { 10, DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryDesc desc = { { query_field }, 0, 0, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    DataStoreFieldId    field_index = DataStoreQueryFindField(query, &query_field);
    double              values[] = { 1, 2, 3, 4 };
    DataStoreFieldArray column = { 10, values + 2 };
    DataStoreValueType  type = DATASTORE_TYPE_NUMBER;
    DataStoreValueData  decoded = { 5 };
    DataStoreRowDesc    row = { group, &type, &decoded, 1 };
    DataStoreId         ids[6] = {};
    for (uint32_t pass = 0; pass < 2; ++pass)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, group, 2, values, 0));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreateSoA(store, 1, group, 2, 1, &column, 0));
        decoded.m_Number = 5;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowAddBatch(store, 1, &row, 1, 0));
        decoded.m_Number = 6;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowAdd(store, 1, group, &type, &decoded, 1, 0, 0));

        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
        ASSERT_EQ(6u, DataStoreQueryGetRowCount(query));
        DataStoreIterator it   = DataStoreQueryIterRange(query, 0, 6);
        uint32_t          seen = 0;
        while (DataStoreIterNext(&it))
        {
            DataStoreRowIterator rows = DataStoreIterRows(&it);
            while (DataStoreRowIterNext(&rows))
            {
                double value = *DataStoreRowIterGetNumber(&rows, field_index);
                ASSERT_TRUE(value >= 1 && value <= 6);
                uint32_t index = (uint32_t)value - 1;
                ASSERT_EQ(0u, seen & (1u << index));
                seen |= 1u << index;
                ASSERT_EQ(group, DataStoreRowIterGetGroupId(&rows));
                DataStoreId id = DataStoreRowIterGetId(&rows);
                ASSERT_NE(ids[index], id);
                ids[index] = id;
            }
        }
        ASSERT_EQ(63u, seen);
        DataStoreQueryEnd(query);
        // Query-discovered IDs support lookup and removal, including reused slots.
        for (uint32_t i = 0; i < 6; ++i)
        {
            double value;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, ids[i], 10, &value));
            ASSERT_EQ((double)(i + 1), value);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[i]));
            ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGetNumber(store, ids[i], 10, &value));
        }
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Native batches validate every row before writing across row-storage block boundaries.
TEST(Data, NativeRowsValidateBeforeWritingAcrossBlocks)
{
    const uint32_t count = 1031;
    struct NativeRow
    {
        uint8_t          m_Enabled;
        DataStoreVector3 m_Position;
    };
    NativeRow   input[count] = {};
    DataStoreId ids[count];
    for (uint32_t i = 0; i < count; ++i)
    {
        input[i].m_Enabled = i & 1;
        input[i].m_Position.m_Values[0] = (float)i;
        input[i].m_Position.m_Values[1] = 2;
        input[i].m_Position.m_Values[2] = 3;
        ids[i] = 99;
    }
    DataStoreFieldDesc fields[] = {
        { 1, DATASTORE_TYPE_BOOLEAN, offsetof(NativeRow, m_Enabled) },
        { 2, DATASTORE_TYPE_VECTOR3, offsetof(NativeRow, m_Position) }
    };
    DataStoreTableDesc table = { 1, 0, 0, fields, 2, sizeof(NativeRow), DATASTORE_RESET_RETAIN };
    HDataStoreContext  store = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    input[count - 1].m_Enabled = 2;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowCreate(store, 1, 0, count, input, ids));
    ASSERT_TRUE(FindTable(store, 1)->m_Rows.Empty());
    for (uint32_t i = 0; i < count; ++i)
    {
        ASSERT_EQ(99u, ids[i]);
    }

    input[count - 1].m_Enabled = 0;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, 0, count, input, ids));
    for (uint32_t i = 0; i < count; ++i)
    {
        uint8_t          enabled;
        DataStoreVector3 position;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetBoolean(store, ids[i], 1, &enabled));
        ASSERT_EQ(input[i].m_Enabled, enabled);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3(store, ids[i], 2, &position));
        ASSERT_EQ(0, memcmp(&input[i].m_Position, &position, sizeof(position)));
    }
    DataStoreVector3 changed = { { -1, -2, -3 } };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetVector3(store, ids[count - 1], 2, &changed));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[count - 1]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3(store, ids[count - 1], 2, &changed));
    ASSERT_EQ((float)(count - 1), changed.m_Values[0]);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// String batch reads cover native and packed rows while preserving typed error results.
TEST(Data, BatchStringsAcrossOwnedAndPackedRows)
{
    HDataStoreContext store = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, Register(store, 1, 0, 0));
    DataStoreId ids[131];
    char text[32];
    for (uint32_t i = 0; i < 131; ++i)
    {
        snprintf(text, sizeof(text), "value-%u", i);
        ASSERT_EQ(DATASTORE_RESULT_OK, Add(store, 1, 0, i, text, &ids[i]));
    }
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, bytes, size, &size));
    HDataStoreBlob         blob;
    HDataStoreBlobInstance instance;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(store, blob, 0, &instance));
    for (uint32_t i = 0; i < 131; i += 2)
    {
        ids[i] = MakeId(store, GetInstanceSlots(instance)[i]);
    }
    const char* strings[131];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetStringBatch(store, 131, ids, 20, strings));
    for (uint32_t i = 0; i < 131; ++i)
    {
        snprintf(text, sizeof(text), "value-%u", i);
        ASSERT_STREQ(text, strings[i]);
    }
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFieldGetStringBatch(store, 131, ids, 10, strings));
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGetStringBatch(store, 131, ids, 999, strings));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetStringBatch(0, 0, 0, 20, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobRemove(instance));
    DataStoreBlobDestroy(blob);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    delete[] bytes;
}

// C callers create, query, update and remove native AoS rows through the public API.
TEST(Data, NativeRowLifecycleFromC)
{
    ASSERT_EQ(0, TestDataNativeRowsFromC());
}

// C callers create and query rows from field arrays through the public SoA input API.
TEST(Data, SoARowLifecycleFromC)
{
    ASSERT_EQ(0, TestDataSoARowsFromC());
}

// Large SoA batches validate every field before publishing and preserve rows after slot reuse.
TEST(Data, SoARowsWithManyFieldsValidateBeforePublishing)
{
    const uint32_t field_count = 40;
    const uint32_t count = 1031;
    struct Row
    {
        double  m_Values[field_count];
        uint8_t m_Enabled;
    };
    double              values[field_count][count];
    uint8_t             enabled[count];
    DataStoreFieldDesc  layout[field_count + 1] = {};
    DataStoreFieldArray fields[field_count + 1] = {};
    DataStoreId         ids[count];
    for (uint32_t f = 0; f < field_count; ++f)
    {
        layout[f] = { 100 + f, DATASTORE_TYPE_NUMBER, (uint32_t)(f * sizeof(double)) };
        fields[field_count - f] = { 100 + f, values[f] };
        for (uint32_t r = 0; r < count; ++r)
        {
            values[f][r] = f * count + r;
        }
    }
    layout[field_count] = { 200, DATASTORE_TYPE_BOOLEAN, offsetof(Row, m_Enabled) };
    fields[0] = { 200, enabled };
    for (uint32_t r = 0; r < count; ++r)
    {
        enabled[r] = r & 1;
        ids[r] = 99;
    }
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreTableDesc desc = { 1, 0, 0, layout, field_count + 1, sizeof(Row), DATASTORE_RESET_RETAIN };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &desc));
    DataStoreTable* table = FindTable(store, 1);
    uint64_t revision = store->m_Revision;
    enabled[count - 1] = 2;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowCreateSoA(store, 1, 0, count, field_count + 1, fields, ids));
    ASSERT_EQ(revision, store->m_Revision);
    ASSERT_TRUE(table->m_Rows.Empty());
    ASSERT_TRUE(table->m_Chunks.Empty());
    for (uint32_t r = 0; r < count; ++r)
    {
        ASSERT_EQ(99u, ids[r]);
    }

    enabled[count - 1] = 0;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreateSoA(store, 1, 0, count, field_count + 1, fields, ids));
    for (uint32_t f = 0; f < field_count; ++f)
    {
        for (uint32_t r = 0; r < count; ++r)
        {
            double value;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, ids[r], 100 + f, &value));
            ASSERT_EQ(values[f][r], value);
        }
    }
    // Append after swap removal and verify earlier rows/defaults remain intact.
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[0]));
    DataStoreId added;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreateSoA(store, 1, 0, 1, field_count + 1, fields, &added));
    ASSERT_NE(ids[0], added);
    values[3][count - 1] = -1;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetNumber(store, ids[count - 1], 103, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[count - 1]));
    double value;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, ids[count - 1], 103, &value));
    ASSERT_EQ((double)(4 * count - 1), value);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, added, 103, &value));
    ASSERT_EQ((double)(3 * count), value);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

static void CheckNativeReferences(HDataStoreContext store, const DataStoreId ids[2])
{
    for (uint32_t i = 0; i < 2; ++i)
    {
        DataStoreValue object;
        DataStoreValue child;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[i], 11, &object));
        ASSERT_EQ(DATASTORE_TYPE_STRUCT, object.m_Type);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&object.m_Value.m_Struct, 1, &child));
        ASSERT_STREQ(i ? "bandits" : "guards", child.m_Value.m_String);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&object.m_Value.m_Struct, 2, &child));
        ASSERT_EQ(i ? 25.0 : 15.0, child.m_Value.m_Number);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[i], 12, &object));
        ASSERT_EQ(DATASTORE_TYPE_LIST, object.m_Type);
        ASSERT_EQ(2u, object.m_Value.m_List.m_Count);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(&object.m_Value.m_List, 0, &child));
        if (i)
        {
            ASSERT_EQ(DATASTORE_TYPE_STRUCT, child.m_Type);
            DataStoreValue faction;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&child.m_Value.m_Struct, 1, &faction));
            ASSERT_STREQ("guards", faction.m_Value.m_String);
        }
        else
        {
            ASSERT_EQ(DATASTORE_TYPE_STRING, child.m_Type);
            ASSERT_STREQ("coin", child.m_Value.m_String);
        }
    }
}

// Native C reference inputs become owned payloads and survive serialization and reset.
TEST(Data, NativeReferencesFromC)
{
    for (uint32_t soa = 0; soa < 2; ++soa)
    {
        HDataStoreContext store = DataStoreCreate(0);
        DataStoreId       ids[2];
        ASSERT_EQ(0, CreateReferenceRowsFromC(store, soa, ids));
        CheckNativeReferences(store, ids);
        DataStoreTable* table = FindTable(store, 93);
        ASSERT_TRUE(table->m_Owned->m_BaseValues != 0);
        ASSERT_EQ((DataStorePayloadBlock*)0, table->m_Owned->m_BaseValues->m_Next); // One payload allocation for the batch.

        uint8_t DM_ALIGNED(8) bytes[4096];
        uint32_t size;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, bytes, sizeof(bytes), &size));
        HDataStoreBlob blob;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
        HDataStoreContext      loaded = DataStoreCreate(0);
        HDataStoreBlobInstance instance;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(loaded, blob, 0, &instance));
        DataStoreBlobDestroy(blob);
        DataStoreId loaded_ids[] = { MakeId(loaded, 0), MakeId(loaded, 1) };
        CheckNativeReferences(loaded, loaded_ids);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(loaded));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[0]));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[1]));
        const char* name;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetString(store, ids[1], 10, &name));
        ASSERT_STREQ("Bandit", name);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    }
}

// Invalid native reference batches publish nothing; empty lists still need no values array.
TEST(Data, NativeReferenceValidation)
{
    for (uint32_t soa = 0; soa < 2; ++soa)
    {
        HDataStoreContext  store = DataStoreCreate(0);
        DataStoreFieldDesc field = { 1, DATASTORE_TYPE_LIST };
        DataStoreTableDesc table = { 1, 0, 0, &field, 1, sizeof(DataStoreReference) };
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
        double             numbers[] = { 1, 2 };
        DataStoreListInput lists[] = {
            { DATASTORE_TYPE_NUMBER, 2, numbers },
            { DATASTORE_TYPE_NUMBER, 1 }
        };
        DataStoreReference rows[2] = {};
        rows[0].m_List = &lists[0];
        rows[1].m_List = &lists[1];
        DataStoreFieldArray column = { 1, rows };
        DataStoreId         ids[] = { 123, 456 };
        for (uint32_t invalid = 0; invalid < 5; ++invalid)
        {
            switch (invalid)
            {
                case 1:
                {
                    lists[1] = { (DataStoreValueType)UINT32_MAX };
                    break;
                }
                case 2:
                {
                    lists[1] = { DATASTORE_TYPE_LIST, 1, &rows[1] }; // Cycle exceeds depth limit.
                    break;
                }
                case 3:
                {
                    rows[1].m_List = 0;
                    break;
                }
                case 4:
                {
                    lists[1] = { DATASTORE_TYPE_STRING, 1, &rows[1] }; // NULL string.
                    rows[0].m_List = &lists[1];
                    break;
                }
            }
            DataStoreResult result = soa ? DataStoreRowCreateSoA(store, 1, 0, 2, 1, &column, ids) : DataStoreRowCreate(store, 1, 0, 2, rows, ids);
            ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, result);
            ASSERT_EQ((DataStoreId)123, ids[0]);
            ASSERT_EQ((DataStoreId)456, ids[1]);
            DataStoreTable* stored = FindTable(store, 1);
            ASSERT_TRUE(stored->m_Rows.Empty());
            ASSERT_TRUE(stored->m_Chunks.Empty());
            ASSERT_EQ((DataStorePayloadBlock*)0, stored->m_Owned->m_BaseValues);
        }
        lists[1] = { DATASTORE_TYPE_NUMBER }; // Empty lists need no values array.
        rows[0].m_List = &lists[0];
        rows[1].m_List = &lists[1];
        DataStoreResult result = soa ? DataStoreRowCreateSoA(store, 1, 0, 2, 1, &column, 0) : DataStoreRowCreate(store, 1, 0, 2, rows, 0);
        ASSERT_EQ(DATASTORE_RESULT_OK, result);
        ASSERT_EQ(2u, FindTable(store, 1)->m_Rows.Size());
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    }
}

// Native dynamic structs validate their layouts and support empty structs and nested lists.
TEST(Data, NativeDynamicStructAndNestedListInput)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreFieldDesc field = { 1, DATASTORE_TYPE_STRUCT };
    DataStoreTableDesc table = { 1, 0, 0, &field, 1, sizeof(DataStoreReference) };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    uint64_t           bytes[] = { 2, 0 };
    DataStoreFieldDesc members[] = {
        { 1, DATASTORE_TYPE_BOOLEAN },
        { 2, DATASTORE_TYPE_NULL, 8 }
    };
    DataStoreStructDesc  layout = { members, 2, sizeof(bytes) };
    DataStoreStructInput object = { &layout, bytes };
    DataStoreReference   row = {};
    row.m_Struct = &object;
    DataStoreId id = 123;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowCreate(store, 1, 0, 1, &row, &id)); // Invalid Boolean.
    bytes[0] = 1;
    members[1].m_Field = 1;
    ASSERT_EQ(DATASTORE_RESULT_ALREADY_EXISTS, DataStoreRowCreate(store, 1, 0, 1, &row, &id));
    members[1].m_Field = 2;
    members[1].m_Offset = 17;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowCreate(store, 1, 0, 1, &row, &id));
    object.m_Layout = 0;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowCreate(store, 1, 0, 1, &row, &id));
    ASSERT_EQ((DataStoreId)123, id);
    object.m_Layout = &layout;
    layout = {};
    object.m_Values = 0;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, 0, 1, &row, &id)); // Empty dynamic struct.
    DataStoreValue value;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, id, 1, &value));
    ASSERT_EQ(0u, value.m_Value.m_Struct.m_Count);

    field.m_Type = DATASTORE_TYPE_LIST;
    table.m_Type = 2;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    DataStoreVector3   vector = { { 1, 2, 3 } };
    DataStoreListInput child = { DATASTORE_TYPE_VECTOR3, 1, &vector };
    DataStoreReference element = {};
    element.m_List = &child;
    DataStoreListInput parent = { DATASTORE_TYPE_LIST, 1, &element };
    row.m_List = &parent;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 2, 0, 1, &row, &id));
    vector.m_Values[2] = 99;
    DataStoreValue nested;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, id, 1, &value));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(&value.m_Value.m_List, 0, &nested));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(&nested.m_Value.m_List, 0, &value));
    ASSERT_EQ(DATASTORE_TYPE_VECTOR3, value.m_Type);
    ASSERT_EQ(3.0f, value.m_Value.m_Vector3[2]);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

static void CheckMixedList(const DataStoreList* list)
{
    ASSERT_EQ(9u, list->m_Count);
    const DataStoreValueType types[] = {
        DATASTORE_TYPE_NUMBER, DATASTORE_TYPE_STRING, DATASTORE_TYPE_BOOLEAN, DATASTORE_TYPE_STRUCT, DATASTORE_TYPE_LIST, DATASTORE_TYPE_NULL, DATASTORE_TYPE_VECTOR3, DATASTORE_TYPE_VECTOR4, DATASTORE_TYPE_MATRIX4
    };
    for (uint32_t i = 0; i < list->m_Count; ++i)
    {
        DataStoreValue value;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(list, i, &value));
        ASSERT_EQ(types[i], value.m_Type);
    }
    DataStoreValue value;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(list, 0, &value));
    ASSERT_EQ(100.0, value.m_Value.m_Number);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(list, 1, &value));
    ASSERT_STREQ("coin", value.m_Value.m_String);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(list, 2, &value));
    ASSERT_EQ(1, value.m_Value.m_Boolean);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(list, 3, &value));
    DataStoreValue child;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&value.m_Value.m_Struct, 42, &child));
    ASSERT_EQ(100.0, child.m_Value.m_Number);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(list, 4, &value));
    ASSERT_EQ(2u, value.m_Value.m_List.m_Count);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(&value.m_Value.m_List, 0, &child));
    ASSERT_EQ(1.0, child.m_Value.m_Number);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(list, 6, &value));
    ASSERT_EQ(3.0f, value.m_Value.m_Vector3[2]);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(list, 7, &value));
    ASSERT_EQ(7.0f, value.m_Value.m_Vector4[3]);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(list, 8, &value));
    ASSERT_EQ(1.0f, value.m_Value.m_Matrix4[15]);
}

// Mixed lists created from C retain their values across ownership, reset and blob loading.
TEST(Data, MixedListCreationFromC)
{
    for (uint32_t soa = 0; soa < 2; ++soa)
    {
        HDataStoreContext store = DataStoreCreate(0);
        DataStoreId       ids[2];
        ASSERT_EQ(0, CreateMixedListRowsFromC(store, soa, ids));
        DataStoreValue value;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[0], 1, &value));
        CheckMixedList(&value.m_Value.m_List);
        DataStoreValue parent;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[1], 1, &parent));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(&parent.m_Value.m_List, 0, &value));
        CheckMixedList(&value.m_Value.m_List);
        DataStoreTable* table = FindTable(store, 94);
        ASSERT_EQ((DataStorePayloadBlock*)0, table->m_Owned->m_BaseValues->m_Next);

        DataStoreValue empty = { DATASTORE_TYPE_LIST };
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, ids[0], 1, &empty));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[0]));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[0], 1, &value));
        CheckMixedList(&value.m_Value.m_List);

        uint8_t DM_ALIGNED(8) bytes[4096];
        uint32_t size;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, bytes, sizeof(bytes), &size));
        HDataStoreBlob blob;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
        HDataStoreContext      loaded = DataStoreCreate(0);
        HDataStoreBlobInstance instance;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(loaded, blob, 0, &instance));
        DataStoreBlobDestroy(blob);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(loaded, MakeId(loaded, 0), 1, &value));
        CheckMixedList(&value.m_Value.m_List);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(loaded, MakeId(loaded, 1), 1, &parent));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(&parent.m_Value.m_List, 0, &value));
        CheckMixedList(&value.m_Value.m_List);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(loaded));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    }
}

// Invalid mixed-list inputs publish no rows or payloads; empty mixed lists remain valid.
TEST(Data, MixedListValidationBeforePublishing)
{
    for (uint32_t soa = 0; soa < 2; ++soa)
    {
        HDataStoreContext  store = DataStoreCreate(0);
        DataStoreFieldDesc field = { 1, DATASTORE_TYPE_LIST };
        DataStoreTableDesc table = { 1, 0, 0, &field, 1, sizeof(DataStoreReference) };
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
        DataStoreValueType type = DATASTORE_TYPE_NUMBER;
        const void*        payload = 0;
        DataStoreListInput mixed = { DATASTORE_TYPE_NUMBER, 1, &payload, &type };
        DataStoreListInput empty = { DATASTORE_TYPE_NULL };
        DataStoreReference rows[2] = {};
        rows[0].m_List = &empty;
        rows[1].m_List = &mixed;
        DataStoreFieldArray column = { 1, rows };
        DataStoreId         ids[] = { 123, 456 };
        uint8_t             boolean = 2;
        for (uint32_t invalid = 0; invalid < 5; ++invalid)
        {
            switch (invalid)
            {
                case 1:
                {
                    type = (DataStoreValueType)UINT32_MAX;
                    break;
                }
                case 2:
                {
                    type = DATASTORE_TYPE_BOOLEAN;
                    payload = &boolean;
                    break;
                }
                case 3:
                {
                    type = DATASTORE_TYPE_LIST;
                    payload = &mixed; // Mixed-list cycle reaches the existing depth limit.
                    break;
                }
                case 4:
                {
                    mixed.m_Values = 0;
                    break;
                }
            }
            DataStoreResult result = soa ? DataStoreRowCreateSoA(store, 1, 0, 2, 1, &column, ids) : DataStoreRowCreate(store, 1, 0, 2, rows, ids);
            ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, result);
            ASSERT_EQ((DataStoreId)123, ids[0]);
            ASSERT_EQ((DataStoreId)456, ids[1]);
            DataStoreTable* stored = FindTable(store, 1);
            ASSERT_TRUE(stored->m_Rows.Empty());
            ASSERT_EQ((DataStorePayloadBlock*)0, stored->m_Owned->m_BaseValues);
        }
        mixed.m_Count = 0; // Empty mixed lists need no payload pointer array.
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, 0, 1, rows + 1, 0));
        mixed.m_Count = 1;
        mixed.m_Values = &payload;
        type = DATASTORE_TYPE_NULL;
        payload = 0;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreateSoA(store, 1, 0, 2, 1, &column, 0));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    }
}

// Root aliases must not hide inline members during registration or blob loading.
TEST(Data, DuplicateFullFieldNamesAreRejected)
{
    HDataStoreContext   store = DataStoreCreate(0);
    uint64_t            color = dmHashString64("light.color");
    DataStoreFieldDesc  member = { dmHashString64("color"), DATASTORE_TYPE_NUMBER, 0, 0, "color" };
    DataStoreStructDesc light = { &member, 1, sizeof(double) };
    DataStoreFieldDesc  fields[] = {
        { dmHashString64("light"), DATASTORE_TYPE_STRUCT, 0, &light, "light" },
        { color, DATASTORE_TYPE_NUMBER, sizeof(double) }
    };
    DataStoreTableDesc desc = { 1, 0, 0, fields, 2, 2 * sizeof(double) };
    DataStoreQueryItem field = { color, DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryDesc query_desc = { { field }, 0, 0, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &query_desc, &query));
    uint64_t revision = store->m_Revision;
    ASSERT_EQ(DATASTORE_RESULT_ALREADY_EXISTS, DataStoreTableAdd(store, &desc));
    ASSERT_EQ(revision, store->m_Revision);
    ASSERT_TRUE(store->m_Tables.Empty());
    ASSERT_TRUE(query->m_Tables.Empty());

    fields[1].m_Field = dmHashString64("intensity");
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &desc));
    double      values[] = { 3, 7 };
    DataStoreId id;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, 0, 1, values, &id));
    double value;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, id, color, &value));
    ASSERT_EQ(3.0, value);

    uint8_t DM_ALIGNED(8) bytes[512];
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, bytes, sizeof(bytes), &size));
    HDataStoreBlob blob;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
    DataStoreBlobDestroy(blob);
    uint32_t table_offset = (uint32_t)ReadDataInteger(bytes + DATA_TABLE_OFFSETS_OFFSET, 4);
    uint8_t* root = bytes + table_offset + DATA_TABLE_HEADER_SIZE + DATA_FIELD_META_SIZE;
    WriteDataInteger(root + offsetof(DataStoreFileFieldMeta, m_NameHash), color, 8);
    ASSERT_EQ(DATASTORE_RESULT_INVALID_FORMAT, DataStoreBlobLoad(bytes, size, &blob));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Native AoS/SoA creation and nested setters must share the blob reader's depth budget.
TEST(Data, InlineAndDynamicNestingShareOneLimit)
{
    DataStoreListInput lists[DATA_MAX_NESTING];
    DataStoreReference references[DATA_MAX_NESTING] = {};
    double             number = 7;
    for (uint32_t i = 0; i < DATA_MAX_NESTING; ++i)
    {
        lists[i] = { i ? DATASTORE_TYPE_LIST : DATASTORE_TYPE_NUMBER, 1, i ? (const void*)&references[i - 1] : &number };
        references[i].m_List = &lists[i];
    }
    DataStoreFieldDesc  member = { dmHashString64("items"), DATASTORE_TYPE_LIST, 0, 0, "items" };
    DataStoreStructDesc layout = { &member, 1, sizeof(DataStoreReference) };
    DataStoreFieldDesc  root = { dmHashString64("state"), DATASTORE_TYPE_STRUCT, 0, &layout, "state" };
    DataStoreTableDesc  desc = { 1, 0, 0, &root, 1, sizeof(DataStoreReference) };
    for (uint32_t soa = 0; soa < 2; ++soa)
    {
        HDataStoreContext store = DataStoreCreate(0);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &desc));
        DataStoreReference  rows[] = { references[DATA_MAX_NESTING - 2], references[DATA_MAX_NESTING - 1] };
        DataStoreFieldArray column = { root.m_Field, rows };
        DataStoreId         ids[] = { 123, 456 };
        DataStoreResult     result = soa ? DataStoreRowCreateSoA(store, 1, 0, 2, 1, &column, ids) : DataStoreRowCreate(store, 1, 0, 2, rows, ids);
        ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, result);
        ASSERT_EQ((DataStoreId)123, ids[0]);
        ASSERT_EQ((DataStoreId)456, ids[1]);
        ASSERT_TRUE(FindTable(store, 1)->m_Rows.Empty());
        result = soa ? DataStoreRowCreateSoA(store, 1, 0, 1, 1, &column, ids) : DataStoreRowCreate(store, 1, 0, 1, rows, ids);
        ASSERT_EQ(DATASTORE_RESULT_OK, result);

        DataStoreValue nested[DATA_MAX_NESTING + 1];
        nested[0] = Number(number);
        for (uint32_t i = 1; i <= DATA_MAX_NESTING; ++i)
        {
            nested[i] = List(&nested[i - 1], 1);
        }
        uint64_t field = dmHashString64("state.items");
        ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFieldSet(store, ids[0], field, &nested[DATA_MAX_NESTING]));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, ids[0], field, &nested[DATA_MAX_NESTING - 1]));
        uint8_t DM_ALIGNED(8) bytes[4096];
        uint32_t size;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, bytes, sizeof(bytes), &size));
        HDataStoreBlob blob;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
        DataStoreBlobDestroy(blob);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    }
}

// Repeated waves reuse default storage, and swap removal preserves surviving reset values.
TEST(Data, NativeDefaultsRemainDenseAcrossWaves)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreFieldDesc field = { 1, DATASTORE_TYPE_NUMBER };
    DataStoreTableDesc desc = { 1, 0, 0, &field, 1, sizeof(double), DATASTORE_RESET_RETAIN };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &desc));
    DataStoreTable* table = FindTable(store, 1);
    double          permanent_value = 99;
    DataStoreId     permanent;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, 0, 1, &permanent_value, &permanent));
    double      values[128];
    DataStoreId ids[128];
    uint32_t    capacity = 0;
    for (uint32_t wave = 0; wave < 32; ++wave)
    {
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(values); ++i)
        {
            values[i] = wave * 128 + i;
        }
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, 0, 128, values, ids));
        if (!wave)
        {
            capacity = table->m_Chunks.Size();
        }
        ASSERT_EQ(capacity, table->m_Chunks.Size());
        for (uint32_t i = 0; i < 128; ++i)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetNumber(store, ids[i], 1, -1));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[i]));
            double value;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, ids[i], 1, &value));
            ASSERT_EQ(values[i], value);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[i]));
            ASSERT_EQ((table->m_Rows.Size() + table->m_ChunkMask) >> table->m_ChunkShift, table->m_Chunks.Size());
            for (uint32_t row = 0; row < table->m_Rows.Size(); ++row)
            {
                ASSERT_EQ(row, table->m_Rows[row].m_BaseIndex);
            }
        }
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, permanent));
        double value;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, permanent, 1, &value));
        ASSERT_EQ(permanent_value, value);
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, permanent));
    ASSERT_TRUE(table->m_Chunks.Empty());
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Emptying a native table releases string/list arenas while keeping child slots reusable.
TEST(Data, EmptyNativeTableReleasesPayloadArenas)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreFieldDesc root = { 1, DATASTORE_TYPE_STRUCT };
    DataStoreTableDesc table_desc = { 1, 0, 0, &root, 1, sizeof(DataStoreReference), DATASTORE_RESET_RETAIN };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table_desc));
    DataStoreFieldDesc fields[] = {
        { 10, DATASTORE_TYPE_STRING },
        { 20, DATASTORE_TYPE_LIST, sizeof(DataStoreReference) }
    };
    DataStoreStructDesc layout = { fields, 2, 2 * sizeof(DataStoreReference) };
    double              numbers[] = { 3, 7 };
    DataStoreListInput  list = { DATASTORE_TYPE_NUMBER, 2, numbers };
    DataStoreReference  members[2] = {};
    members[0].m_String = "original";
    members[1].m_List = &list;
    DataStoreStructInput input = { &layout, members };
    DataStoreReference   rows[2] = {};
    rows[0].m_Struct = &input;
    rows[1].m_Struct = &input;
    DataStoreTable* table = FindTable(store, 1);
    for (uint32_t wave = 0; wave < 4; ++wave)
    {
        DataStoreId ids[2];
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, 0, 2, rows, ids));
        DataStoreValue value;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[1], 1, &value));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSet(store, ids[1], 1, &value));
        ASSERT_TRUE(table->m_Payloads != 0);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[0]));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[1]));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, ids[1], 1, &value));
        DataStoreValue child;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&value.m_Value.m_Struct, 10, &child));
        ASSERT_STREQ("original", child.m_Value.m_String);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[1]));
        ASSERT_TRUE(table->m_Chunks.Empty());
        ASSERT_EQ((DataStorePayloadBlock*)0, table->m_Owned->m_BaseValues);
        ASSERT_EQ((DataStorePayloadBlock*)0, table->m_Payloads);
        ASSERT_EQ(0u, table->m_Structs->m_Tables[0]->m_RowCount);
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}
