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

extern "C" int TestDataFromC(HDSContext store);
extern "C" int TestDataStoreLifecycleFromC(void);
extern "C" int TestDataQuerySetupFromC(void);
extern "C" int TestDataNativeRowsFromC(void);
extern "C" int TestDataSoARowsFromC(void);
extern "C" int CreateReferenceRowsFromC(HDSContext store, int soa, DSId ids[2]);
extern "C" int CreateMixedListRowsFromC(HDSContext store, int soa, DSId ids[2]);

// Helpers for internal binding/value tests. Public C traversal is covered in test_data_c.c.
static DSResult GetTestFieldNumber(const DSIterator* batch, uint32_t row, uint32_t field, double* value)
{
    DSValue  field_value;
    DSResult result = DSIterGetField(batch, row, field, &field_value);
    if (result == DS_RESULT_OK)
        *value = field_value.m_Value.m_Number;
    return result;
}

static DSResult SetTestFieldNumber(const DSIterator* batch, uint32_t row, uint32_t field, double value)
{
    DSValue field_value = { .m_Type = DS_TYPE_NUMBER, .m_Value = { .m_Number = value } };
    return DSIterSetField(batch, row, field, &field_value);
}

static DSResult SetTestFieldBoolean(const DSIterator* batch, uint32_t row, uint32_t field, uint8_t value)
{
    DSValue field_value = { .m_Type = DS_TYPE_BOOLEAN, .m_Value = { .m_Boolean = value } };
    return DSIterSetField(batch, row, field, &field_value);
}

static DSResult GetTestFieldString(const DSIterator* batch, uint32_t row, uint32_t field, const char** value)
{
    DSValue  field_value;
    DSResult result = DSIterGetField(batch, row, field, &field_value);
    if (result == DS_RESULT_OK)
        *value = field_value.m_Value.m_String;
    return result;
}

static DSResult SetTestFieldString(const DSIterator* batch, uint32_t row, uint32_t field, const char* value)
{
    DSValue field_value = { .m_Type = DS_TYPE_STRING, .m_Value = { .m_String = value } };
    return DSIterSetField(batch, row, field, &field_value);
}

static DSResult GetTestFieldVector3(const DSIterator* batch, uint32_t row, uint32_t field, DSVector3* value)
{
    DSValue  field_value;
    DSResult result = DSIterGetField(batch, row, field, &field_value);
    if (result == DS_RESULT_OK)
        memcpy(value->m_Values, field_value.m_Value.m_Vector3, sizeof(value->m_Values));
    return result;
}

static DSResult SetTestFieldVector3(const DSIterator* batch, uint32_t row, uint32_t field, const DSVector3* value)
{
    DSValue field_value = { .m_Type = DS_TYPE_VECTOR3 };
    memcpy(field_value.m_Value.m_Vector3, value->m_Values, sizeof(value->m_Values));
    return DSIterSetField(batch, row, field, &field_value);
}

static DSValue Number(double v)
{
    DSValue x = {};
    x.m_Type = DS_TYPE_NUMBER;
    x.m_Value.m_Number = v;
    return x;
}

static DSValue String(const char* v)
{
    DSValue x = {};
    x.m_Type = DS_TYPE_STRING;
    x.m_Value.m_String = v;
    return x;
}

static DSValue Boolean(uint8_t v)
{
    DSValue x = {};
    x.m_Type = DS_TYPE_BOOLEAN;
    x.m_Value.m_Boolean = v;
    return x;
}

static DSValue Null()
{
    DSValue x = {};
    x.m_Type = DS_TYPE_NULL;
    return x;
}

static DSValue List(const DSValue* values, uint32_t count)
{
    DSValue x = {};
    x.m_Type = DS_TYPE_LIST;
    DSList list = { values, count, 0, 0 };
    x.m_Value.m_List = list;
    return x;
}

static DSValue Struct(const uint64_t* names, const DSValueType* types, const DSValueData* values, uint32_t count)
{
    DSValue x = {};
    x.m_Type = DS_TYPE_STRUCT;
    DSStruct object = { .m_Count = count, .m_Source = DS_STRUCT_ARRAY, .m_Array = { .m_Names = names, .m_Types = types, .m_Values = values } };
    x.m_Value.m_Struct = object;
    return x;
}

static DSValue Vector3(float x, float y, float z)
{
    DSValue v = {};
    v.m_Type = DS_TYPE_VECTOR3;
    v.m_Value.m_Vector3[0] = x;
    v.m_Value.m_Vector3[1] = y;
    v.m_Value.m_Vector3[2] = z;
    return v;
}

static DSValue Matrix()
{
    DSValue v = {};
    v.m_Type = DS_TYPE_MATRIX4;
    for (uint32_t i = 0; i < 16; ++i)
        v.m_Value.m_Matrix4[i] = (float)i + 0.5f;
    return v;
}

// Keep single-value fixtures concise; bulk-input tests use the separate arrays directly.
static DSResult AddTestRow(HDSContext store, uint64_t type, DSGroupId group, const DSValue* values, uint32_t count, DSId* out_id, uint64_t component_id = 0)
{
    dmArray<DSValueType> types;
    dmArray<DSValueData> payloads;
    types.SetCapacity(count);
    types.SetSize(count);
    payloads.SetCapacity(count);
    payloads.SetSize(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        types[i] = values[i].m_Type;
        payloads[i] = values[i].m_Value;
    }
    return DSAddRow(store, type, group, types.Begin(), payloads.Begin(), count, out_id, component_id);
}

static DSResult Register(HDSContext store, uint64_t type, const uint64_t* tags = 0, uint32_t tag_count = 0)
{
    DSFieldDesc meta[] = { { 10, DS_TYPE_NUMBER, 0 }, { 20, DS_TYPE_STRING, 8 } };
    DSTableDesc desc = { type, tags, tag_count, meta, 2, 16 };
    return DSRegisterTable(store, &desc);
}

static DSResult Add(HDSContext store, uint64_t type, DSGroupId group, double number, const char* string, DSId* id)
{
    DSValue values[] = { Number(number), String(string) };
    return AddTestRow(store, type, group, values, 2, id);
}

static DSResult Load(HDSContext store, const uint8_t* bytes, uint32_t size, HDSBlobInstance* instance, DSGroupId group = 0)
{
    HDSBlob  blob;
    DSResult result = DSLoadBlob(bytes, size, &blob);
    if (result != DS_RESULT_OK)
        return result;
    result = DSAddBlob(store, blob, group, instance);
    DSDestroyBlob(blob);
    return result;
}

static DSId FirstId(HDSContext store)
{
    DSQueryDesc desc = {};
    HDSQuery    query;
    if (DSCreateQuery(store, &desc, &query) != DS_RESULT_OK)
        return 0;
    if (DSQueryTryBegin(query) != DS_RESULT_OK)
    {
        DSDestroyQuery(query);
        return 0;
    }
    DSIterator it = DSQueryIter(query);
    DSId       id = DSIterNext(&it) ? DSIterGetId(&it, 0) : 0;
    DSQueryEnd(query);
    DSDestroyQuery(query);
    return id;
}

TEST(Data, StringOwnershipAndSelfAssignment)
{
    HDSContext store = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, Register(store, 1));
    char text[] = "copied";
    DSId id;
    ASSERT_EQ(DS_RESULT_OK, Add(store, 1, 1, 2.5, text, &id));
    text[0] = 'X';
    DSValue out;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, 20, &out));
    ASSERT_STREQ("copied", out.m_Value.m_String);
    ASSERT_EQ(DS_RESULT_OK, DSSetField(store, id, 20, &out));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, 20, &out));
    ASSERT_STREQ("copied", out.m_Value.m_String);
    DSValue replacement = String(text);
    ASSERT_EQ(DS_RESULT_OK, DSSetField(store, id, 20, &replacement));
    text[1] = 'Y';
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, 20, &out));
    ASSERT_STREQ("Xopied", out.m_Value.m_String);
    replacement = String(0);
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSetField(store, id, 20, &replacement));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, GrowthSwapRemovalAndStaleIds)
{
    HDSContext store = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, Register(store, 1));
    DSId ids[257];
    for (uint32_t i = 0; i < 257; ++i)
        ASSERT_EQ(DS_RESULT_OK, Add(store, 1, i, i, "value", &ids[i]));
    DSValue out;
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGet(store, 0, 10, &out));
    for (uint32_t i = 0; i < 257; i += 2)
    {
        ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[i]));
        ASSERT_EQ(DS_RESULT_NOT_FOUND, DSRemoveRow(store, ids[i]));
    }
    for (uint32_t i = 1; i < 257; i += 2)
    {
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[i], 10, &out));
        ASSERT_EQ((double)i, out.m_Value.m_Number);
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[i], 20, &out));
        ASSERT_STREQ("value", out.m_Value.m_String);
    }
    for (uint32_t i = 0; i < 257; i += 2)
    {
        DSId replacement;
        ASSERT_EQ(DS_RESULT_OK, Add(store, 1, i, -1, "new", &replacement));
        ASSERT_NE(ids[i], replacement);
        ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGet(store, ids[i], 10, &out));
    }
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterTable(store, 1));
    ASSERT_EQ(DS_RESULT_OK, Register(store, 1));
    DSId replacement;
    ASSERT_EQ(DS_RESULT_OK, Add(store, 1, 0, 42, "reregistered", &replacement));
    for (uint32_t i = 0; i < 257; ++i)
        ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGet(store, ids[i], 10, &out));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, LiveQueriesTagsGroupsAndCopiedDescriptors)
{
    HDSContext  store = DSCreateStore(0);
    uint64_t    tags[] = { 100, 200 };
    DSGroupId   groups[] = { 7, 7, 8 };
    DSQueryDesc desc = {
        .m_GroupIds = groups,
        .m_GroupIdCount = 3,
        .m_Items = { { .m_Hash = tags[0], .m_Kind = DS_QUERY_ITEM_TAG },
                     { .m_Hash = tags[1], .m_Kind = DS_QUERY_ITEM_TAG } }
    };
    HDSQuery query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSIterator it = DSQueryIter(query);
    ASSERT_FALSE(DSIterNext(&it));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, Register(store, 1, tags, 2));
    ASSERT_EQ(DS_RESULT_OK, Register(store, 2, tags, 1));
    ASSERT_EQ(DS_RESULT_OK, Register(store, 3, tags, 2));
    tags[0] = 999;
    groups[0] = groups[1] = groups[2] = 999;
    DSId id;
    for (uint64_t type = 1; type <= 3; ++type)
    {
        const DSGroupId row_groups[] = { 7, 7, 0, 8, 0, 7 };
        for (uint32_t row = 0; row < 6; ++row)
            ASSERT_EQ(DS_RESULT_OK, Add(store, type, row_groups[row], row, "test", &id));
    }
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    it = DSQueryIter(query);
    uint32_t total = 0;
    while (DSIterNext(&it))
    {
        ASSERT_NE((uint64_t)2, DSIterGetType(&it));
        for (uint32_t row = 0; row < DSIterGetCount(&it); ++row)
        {
            DSGroupId group = DSIterGetGroupId(&it, row);
            ASSERT_TRUE(group == 7 || group == 8);
            ++total;
        }
    }
    ASSERT_FALSE(DSIterNext(&it));
    ASSERT_EQ(8u, total);
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterTable(store, 1));
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterTable(store, 3));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    it = DSQueryIter(query);
    ASSERT_FALSE(DSIterNext(&it));
    uint64_t new_tags[] = { 100, 200 };
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, Register(store, 1, new_tags, 2));
    ASSERT_EQ(DS_RESULT_OK, Add(store, 1, 7, 1, "new", &id));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    it = DSQueryIter(query);
    ASSERT_TRUE(DSIterNext(&it));
    ASSERT_EQ(1u, DSIterGetCount(&it));
    ASSERT_EQ(id, DSIterGetId(&it, 0));
    DSQueryEnd(query);
    DSDestroyQuery(query);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Serial iteration reserves declared writes and blocks structural changes until End.
TEST(Data, QueryReservationBlocksMutations)
{
    HDSContext store = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, Register(store, 1));
    DSId id;
    ASSERT_EQ(DS_RESULT_OK, Add(store, 1, 3, 1, "one", &id));
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, bytes, size, &size));
    HDSBlob blob;
    ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
    HDSBlobInstance instance;
    ASSERT_EQ(DS_RESULT_OK, DSAddBlob(store, blob, 4, &instance));
    DSQueryItem field = { .m_Hash = 10, .m_Type = DS_TYPE_NUMBER, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryDesc desc = { .m_Items = { field } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    uint32_t handle = DSQueryFindField(query, &field);
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(query));
    DSIterator batch = DSQueryIter(query);
    ASSERT_TRUE(DSIterNext(&batch));
    DSRowIterator rows = DSIterRows(&batch);
    ASSERT_TRUE(DSRowIterNext(&rows));
    DSId            unchanged = 123;
    HDSBlobInstance unchanged_instance = instance;
    ASSERT_EQ(DS_RESULT_LOCKED, Register(store, 2));
    ASSERT_EQ(DS_RESULT_LOCKED, Add(store, 1, 3, 2, "two", &unchanged));
    DSValue     values[] = { Number(2), String("two") };
    DSValueType types[] = { DS_TYPE_NUMBER, DS_TYPE_STRING };
    DSValueData payloads[] = { values[0].m_Value, values[1].m_Value };
    DSRowDesc   add = { .m_Group = 3, .m_Types = types, .m_Values = payloads, .m_ValueCount = 2 };
    ASSERT_EQ(DS_RESULT_LOCKED, DSAddRows(store, 1, &add, 1, &unchanged));
    ASSERT_EQ((DSId)123, unchanged);
    ASSERT_EQ(DS_RESULT_LOCKED, DSRemoveRow(store, id));
    ASSERT_EQ(DS_RESULT_LOCKED, DSUnregisterTable(store, 1));
    ASSERT_EQ(DS_RESULT_LOCKED, DSAddBlob(store, blob, 5, &unchanged_instance));
    ASSERT_EQ((uintptr_t)instance, (uintptr_t)unchanged_instance);
    ASSERT_EQ(DS_RESULT_LOCKED, DSRemoveBlob(instance));
    ASSERT_EQ(DS_RESULT_LOCKED, DSDestroyStore(store));
    *DSRowIterGetNumberMut(&rows, handle) = 9;
    ASSERT_EQ(9.0, *DSRowIterGetNumber(&rows, handle));
    ASSERT_EQ(DS_RESULT_LOCKED, DSResetRow(store, id));
    ASSERT_EQ(9.0, *DSRowIterGetNumber(&rows, handle));
    // An early traversal exit must release the reservation before mutation.
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, id));
    double number;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, id, 10, &number));
    ASSERT_EQ(1.0, number);
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, id));
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterTable(store, 1));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveBlob(instance));
    ASSERT_EQ(DS_RESULT_OK, Register(store, 2));
    ASSERT_EQ(DS_RESULT_OK, Add(store, 2, 3, 2, "two", &id));
    ASSERT_EQ(DS_RESULT_OK, DSAddBlob(store, blob, 5, &instance));
    DSDestroyQuery(query);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    DSDestroyBlob(blob);
    delete[] bytes;
}

TEST(Data, NestedQueriesAndWritesPreserveBatches)
{
    HDSContext store = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, Register(store, 1));
    DSFieldDesc meta[] = { { 10, DS_TYPE_NUMBER, 8 }, { 20, DS_TYPE_STRING, 24 } };
    DSTableDesc table = { 2, 0, 0, meta, 2, 32 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    DSId ids[2][4];
    for (uint32_t t = 0; t < 2; ++t)
        for (uint32_t r = 0; r < 4; ++r)
            ASSERT_EQ(DS_RESULT_OK, Add(store, t + 1, r == 1 ? 8 : 7, t * 10 + r, "base", &ids[t][r]));

    DSGroupId   group = 7;
    DSQueryItem fields[] = {
        { .m_Hash = 10, .m_Type = DS_TYPE_NUMBER, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD },
        { .m_Hash = 20, .m_Type = DS_TYPE_STRING, .m_Kind = DS_QUERY_ITEM_FIELD }
    };
    DSQueryDesc desc = { .m_GroupIds = &group, .m_GroupIdCount = 1, .m_Items = { fields[0], fields[1] } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    // A second query and iterator must not replace the first iterator's batch state.
    DSQueryDesc other_desc = { .m_GroupIds = &group, .m_GroupIdCount = 1, .m_Items = { fields[1] } };
    HDSQuery    other_query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &other_desc, &other_query));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(other_query));
    DSIterator it = DSQueryIter(query);
    uint32_t   visited = 0, batches = 0;
    while (DSIterNext(&it))
    {
        ++batches;
        DSIterator other = DSQueryIter(other_query);
        while (DSIterNext(&other))
        {
            const char* string;
            ASSERT_EQ(DS_RESULT_OK, GetTestFieldString(&other, 0, 0, &string));
            ASSERT_STREQ("base", string);
        }
        uint32_t t = (uint32_t)DSIterGetType(&it) - 1;
        for (uint32_t row = 0; row < DSIterGetCount(&it); ++row)
        {
            DSId     id = DSIterGetId(&it, row);
            uint32_t r = 0;
            while (r < 4 && ids[t][r] != id)
                ++r;
            ASSERT_TRUE(r < 4 && r != 1);
            ASSERT_EQ(group, DSIterGetGroupId(&it, row));
            double number = -1;
            ASSERT_EQ(DS_RESULT_OK, GetTestFieldNumber(&it, row, 0, &number));
            ASSERT_EQ((double)(t * 10 + r), number);
            ASSERT_EQ(DS_RESULT_OK, SetTestFieldNumber(&it, row, 0, 99));
            ASSERT_EQ(DS_RESULT_OK, GetTestFieldNumber(&it, row, 0, &number));
            ASSERT_EQ(99.0, number);
            ++visited;
        }
    }
    ASSERT_EQ(6u, visited);
    ASSERT_EQ(4u, batches);
    double number = -1;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, GetTestFieldNumber(&it, 0, 0, &number));
    ASSERT_EQ(-1.0, number);

    DSQueryEnd(other_query);
    DSQueryEnd(query);
    DSDestroyQuery(other_query);
    DSDestroyQuery(query);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, BoundFieldQueries)
{
    HDSContext  store = DSCreateStore(0);
    uint64_t    tag = 8;
    DSGroupId   group = 7;
    DSQueryItem fields[] = {
        { .m_Hash = 20, .m_Type = DS_TYPE_STRING, .m_Kind = DS_QUERY_ITEM_FIELD },
        { .m_Hash = 10, .m_Type = DS_TYPE_NUMBER, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD }
    };
    DSQueryDesc desc = { .m_GroupIds = &group, .m_GroupIdCount = 1, .m_Items = { fields[0], fields[1], { .m_Hash = tag, .m_Kind = DS_QUERY_ITEM_TAG } } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    fields[0].m_Hash = 999; // Query owns the original filter/binding order.
    ASSERT_EQ(DS_RESULT_OK, Register(store, 1, &tag, 1));
    DSFieldDesc swapped[] = { { 20, DS_TYPE_STRING, 0 }, { 10, DS_TYPE_NUMBER, 8 } };
    DSTableDesc table = { 2, &tag, 1, swapped, 2, 16 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    DSFieldDesc wrong[] = { { 10, DS_TYPE_STRING, 0 }, { 20, DS_TYPE_STRING, 8 } };
    table.m_Type = 3;
    table.m_Fields = wrong;
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    table.m_Type = 4;
    table.m_Fields = swapped;
    table.m_FieldCount = 1;
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table)); // Missing number.
    ASSERT_EQ(DS_RESULT_OK, Register(store, 5));             // Missing tag.
    DSId ids[6];
    ASSERT_EQ(DS_RESULT_OK, Add(store, 1, 7, 1, "one", &ids[0]));
    ASSERT_EQ(DS_RESULT_OK, Add(store, 1, 8, 99, "other group", &ids[1]));
    DSValue values[] = { String("two"), Number(2) };
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(store, 2, 7, values, 2, &ids[2]));
    values[1] = String("wrong kind");
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(store, 3, 7, values, 2, &ids[3]));
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(store, 4, 7, values, 1, &ids[4]));
    ASSERT_EQ(DS_RESULT_OK, Add(store, 5, 7, 3, "no tag", &ids[5]));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSIterator it = DSQueryIter(query);
    DSValue    out = Number(123);
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSIterGetField(&it, 0, 0, &out));
    uint32_t total = 0;
    while (DSIterNext(&it))
    {
        ASSERT_EQ(1u, DSIterGetCount(&it));
        ASSERT_EQ(DS_RESULT_OK, DSIterGetField(&it, 0, 1, &out));
        ASSERT_EQ((double)DSIterGetType(&it), out.m_Value.m_Number);
        DSValue changed = Number(44);
        ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSIterSetField(&it, 0, 0, &changed));
        ASSERT_EQ(DS_RESULT_OK, DSIterSetField(&it, 0, 1, &changed));
        ASSERT_EQ(DS_RESULT_OK, DSIterGetFieldByHash(&it, 0, 10, &out));
        ASSERT_EQ(44.0, out.m_Value.m_Number);
        ASSERT_EQ(DS_RESULT_LOCKED, DSResetField(store, DSIterGetId(&it, 0), 10));
        ASSERT_EQ(DS_RESULT_OK, DSIterGetField(&it, 0, 0, &out));
        ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSIterSetField(&it, 0, 0, &out));
        ASSERT_STREQ(DSIterGetType(&it) == 1 ? "one" : "two", out.m_Value.m_String);
        ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSIterGetField(&it, 1, 0, &out));
        ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSIterGetField(&it, 0, 2, &out));
        ++total;
    }
    ASSERT_EQ(2u, total);
    it = DSQueryIter(query);
    ASSERT_TRUE(DSIterNext(&it));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSResetTable(store, 2));
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterTable(store, 1));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    it = DSQueryIter(query);
    ASSERT_TRUE(DSIterNext(&it));
    ASSERT_EQ((uint64_t)2, DSIterGetType(&it));
    ASSERT_EQ(DS_RESULT_OK, DSIterGetField(&it, 0, 1, &out));
    ASSERT_EQ(2.0, out.m_Value.m_Number);
    DSQueryEnd(query);
    DSDestroyQuery(query);
    desc.m_Items[0].m_Type = (DSValueType)99;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSCreateQuery(store, &desc, &query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, BoundPackedFields)
{
    HDSContext source = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, Register(source, 1));
    DSId id;
    ASSERT_EQ(DS_RESULT_OK, Add(source, 1, 0, 5, "packed", &id));
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, bytes, size, &size));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(source));
    HDSContext  store = DSCreateStore(0);
    DSQueryItem fields[] = {
        { .m_Hash = 10, .m_Type = DS_TYPE_NUMBER, .m_Kind = DS_QUERY_ITEM_FIELD },
        { .m_Hash = 20, .m_Type = DS_TYPE_STRING, .m_Kind = DS_QUERY_ITEM_FIELD }
    };
    DSQueryDesc desc = { .m_Items = { fields[0], fields[1] } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    HDSBlobInstance instance;
    ASSERT_EQ(DS_RESULT_OK, Load(store, bytes, size, &instance));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSIterator it = DSQueryIter(query);
    ASSERT_TRUE(DSIterNext(&it));
    DSValue value;
    ASSERT_EQ(DS_RESULT_OK, DSIterGetField(&it, 0, 1, &value));
    ASSERT_STREQ("packed", value.m_Value.m_String);
    id = DSIterGetId(&it, 0);
    DSQueryEnd(query);
    value = String("override");
    ASSERT_EQ(DS_RESULT_OK, DSSetField(store, id, 20, &value));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    it = DSQueryIter(query);
    ASSERT_TRUE(DSIterNext(&it));
    ASSERT_EQ(DS_RESULT_OK, DSIterGetField(&it, 0, 1, &value));
    ASSERT_STREQ("override", value.m_Value.m_String);
    ASSERT_EQ(DS_RESULT_OK, DSIterGetField(&it, 0, 0, &value));
    ASSERT_EQ(5.0, value.m_Value.m_Number);
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSResetBlob(instance));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    it = DSQueryIter(query);
    ASSERT_TRUE(DSIterNext(&it));
    ASSERT_EQ(DS_RESULT_OK, DSIterGetField(&it, 0, 1, &value));
    ASSERT_STREQ("packed", value.m_Value.m_String);
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSRemoveBlob(instance));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    it = DSQueryIter(query);
    ASSERT_FALSE(DSIterNext(&it));
    DSQueryEnd(query);
    DSDestroyQuery(query);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    delete[] bytes;
}

TEST(Data, SharedMetadataAndDenseRows)
{
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc meta[] = { { 10, DS_TYPE_VECTOR3, 0 }, { 20, DS_TYPE_NUMBER, 16 }, { 30, DS_TYPE_NUMBER, 24 } };
    DSTableDesc desc = { 1, 0, 0, meta, 3, 32 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &desc));
    meta[0].m_Field = 999; // The construction descriptor is copied once.
    DSValue values[] = { Vector3(1, 2, 3), Number(4), Number(10) };
    DSId    ids[3];
    for (uint32_t i = 0; i < 3; ++i)
        ASSERT_EQ(DS_RESULT_OK, AddTestRow(store, 1, i, values, 3, &ids[i]));
    DSTable* table = store->m_Tables[0];
    ASSERT_EQ(3u, table->m_Owned->m_Fields.Size());
    ASSERT_EQ(32u, table->m_RowStride); // No per-row names or kind tags in value storage.
    ASSERT_EQ(0, memcmp(GetNativeDefaultRow(table, 0), GetNativeDefaultRow(table, 1), 32));
    ASSERT_EQ((uintptr_t)0, (uintptr_t)table->m_Owned->m_BaseValues);
    ASSERT_EQ((uintptr_t)0, (uintptr_t)table->m_Payloads);
    DSValue out;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[1], 10, &out));
    ASSERT_EQ(2.0f, out.m_Value.m_Vector3[1]);
    DSValue changed = Vector3(0.25f, 0.5f, 0.75f);
    ASSERT_EQ(DS_RESULT_OK, DSSetField(store, ids[1], 10, &changed));
    ASSERT_EQ(3u, table->m_Rows.Size());
    ASSERT_EQ(0, memcmp(GetRowBytes(table, 0), GetNativeDefaultRow(table, 0), 32));
    ASSERT_NE(0, memcmp(GetRowBytes(table, 1), GetNativeDefaultRow(table, 1), 32));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[1], 20, &out));
    ASSERT_EQ(4.0, out.m_Value.m_Number); // Updating color preserves the other mutable fields.
    changed = Number(3);
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSetField(store, ids[1], 10, &changed));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[1], 10, &out));
    ASSERT_EQ(0.5f, out.m_Value.m_Vector3[1]);
    ASSERT_EQ(DS_RESULT_OK, DSResetField(store, ids[1], 10));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[1], 10, &out));
    ASSERT_EQ(2.0f, out.m_Value.m_Vector3[1]);
    ASSERT_EQ(DS_RESULT_OK, DSResetTable(store, 1));
    ASSERT_EQ((uintptr_t)0, (uintptr_t)table->m_Payloads);
    for (uint32_t r = 0; r < 3; ++r)
        ASSERT_EQ(0, memcmp(GetRowBytes(table, r), GetNativeDefaultRow(table, r), 32));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, MetadataAndBulkValidation)
{
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc meta[] = { { 1, DS_TYPE_NUMBER, 0 }, { 2, DS_TYPE_BOOLEAN, 8 } };
    DSTableDesc desc = { 1, 0, 0, meta, 2, 16 };
    meta[1].m_Offset = 7;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterTable(store, &desc));
    meta[1].m_Offset = 16;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterTable(store, &desc));
    meta[1].m_Offset = 8;
    meta[1].m_Field = 1;
    ASSERT_EQ(DS_RESULT_ALREADY_EXISTS, DSRegisterTable(store, &desc));
    meta[1].m_Field = 2;
    meta[1].m_Type = (DSValueType)99;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterTable(store, &desc));
    meta[1].m_Type = DS_TYPE_BOOLEAN;
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &desc));
    ASSERT_EQ(DS_RESULT_ALREADY_EXISTS, DSRegisterTable(store, &desc));
    DSValueType types[] = { DS_TYPE_NUMBER, DS_TYPE_BOOLEAN };
    DSValueData values[] = { { .m_Number = 4 }, { .m_Boolean = 1 }, { .m_Number = 5 }, { .m_Boolean = 2 } };
    DSRowDesc   rows[] = {
        { .m_Group = 1, .m_Types = types, .m_Values = values, .m_ValueCount = 2, .m_ComponentId = 7 },
        { .m_Group = 2, .m_Types = types, .m_Values = values + 2, .m_ValueCount = 2, .m_ComponentId = 8 }
    };
    DSId ids[] = { 123, 456 };
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSAddRows(store, 1, rows, 2, ids));
    ASSERT_EQ((DSId)123, ids[0]);
    ASSERT_EQ((DSId)456, ids[1]);
    ASSERT_EQ(0u, store->m_Tables[0]->m_Rows.Size());
    values[3].m_Boolean = 0;
    rows[1].m_ValueCount = 1;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSAddRows(store, 1, rows, 2, ids));
    rows[1].m_ValueCount = 2;
    types[0] = DS_TYPE_STRING;
    values[2].m_String = "wrong kind";
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSAddRows(store, 1, rows, 2, ids));
    types[0] = DS_TYPE_NUMBER;
    values[2].m_Number = 5;
    rows[1].m_Types = 0;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSAddRows(store, 1, rows, 2, ids));
    rows[1].m_Types = types;
    rows[1].m_Values = 0;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSAddRows(store, 1, rows, 2, ids));
    rows[1].m_Values = values + 2;
    ASSERT_EQ(DS_RESULT_OK, DSAddRows(store, 1, rows, 2, ids));
    ASSERT_EQ(2u, store->m_Tables[0]->m_Rows.Size());
    values[0].m_Number = 99;
    types[0] = DS_TYPE_STRING; // Insertion no longer depends on either input array.
    DSValue out = Number(123);
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[0], 1, &out));
    ASSERT_EQ(4.0, out.m_Value.m_Number);
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGet(store, ids[0], 999, &out));
    ASSERT_EQ(4.0, out.m_Value.m_Number);
    ASSERT_EQ(DS_RESULT_OK, DSAddRows(store, 1, 0, 0, 0));
    ASSERT_EQ(DS_RESULT_OK, DSAddRows(0, 999, 0, 0, 0));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSAddRows(store, 1, 0, 1, ids));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, BulkAppendPreservesDefaultsAndReusesSlots)
{
    DSFieldDesc fields[] = {
        { .m_Field = 10, .m_Type = DS_TYPE_NUMBER, .m_Offset = 0 },
        { .m_Field = 20, .m_Type = DS_TYPE_BOOLEAN, .m_Offset = 8 },
        { .m_Field = 30, .m_Type = DS_TYPE_VECTOR3, .m_Offset = 12 },
        { .m_Field = 40, .m_Type = DS_TYPE_VECTOR4, .m_Offset = 24 },
        { .m_Field = 50, .m_Type = DS_TYPE_MATRIX4, .m_Offset = 40 }
    };
    const DSValueType types[] = { DS_TYPE_NUMBER, DS_TYPE_BOOLEAN, DS_TYPE_VECTOR3, DS_TYPE_VECTOR4, DS_TYPE_MATRIX4 };
    DSTableDesc       table = { .m_Type = 1, .m_Fields = fields, .m_FieldCount = 5, .m_RowStride = 104 };
    HDSContext        store = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    DSValueData values[3][5] = {};
    DSRowDesc   rows[3] = {};
    for (uint32_t r = 0; r < 3; ++r)
    {
        values[r][0].m_Number = 100 + r;
        values[r][1].m_Boolean = r & 1;
        for (uint32_t f = 2; f < 5; ++f)
            for (uint32_t v = 0; v < DSTypeSize(types[f]) / sizeof(float); ++v)
                values[r][f].m_Matrix4[v] = (float)(100 * r + 10 * f + v);
        rows[r] = { .m_Group = 20 + r, .m_Types = types, .m_Values = values[r], .m_ValueCount = 5, .m_ComponentId = 30 + r };
    }
    DSId old_ids[2], ids[] = { 123, 456, 789 };
    ASSERT_EQ(DS_RESULT_OK, DSAddRows(store, 1, rows, 2, old_ids));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, old_ids[0]));
    values[2][1].m_Boolean = 2;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSAddRows(store, 1, rows, 3, ids));
    ASSERT_EQ((DSId)123, ids[0]);
    ASSERT_EQ((DSId)789, ids[2]);
    ASSERT_EQ(1u, store->m_Tables[0]->m_Rows.Size());
    values[2][1].m_Boolean = 0;
    ASSERT_EQ(DS_RESULT_OK, DSAddRows(store, 1, rows, 3, ids));
    ASSERT_EQ((uint32_t)old_ids[0], (uint32_t)ids[0]); // Reused slot with a new generation, followed by fresh slots.
    ASSERT_NE(old_ids[0], ids[0]);
    ASSERT_EQ(4u, store->m_Slots.Size());
    double number;
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGetNumber(store, old_ids[0], 10, &number));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, old_ids[1], 10, &number));
    ASSERT_EQ(101.0, number);
    for (uint32_t r = 0; r < 3; ++r)
    {
        const DSSlot* slot = FindSlot(store, ids[r]);
        ASSERT_EQ((DSGroupId)(20 + r), slot->m_Table->m_Rows[slot->m_Row].m_Group);
        ASSERT_EQ((uint64_t)(30 + r), DSGetComponentId(store, ids[r]));
        ASSERT_EQ(DS_RESULT_OK, DSSetFieldNumber(store, ids[r], 10, -1));
        ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, ids[r]));
        for (uint32_t f = 0; f < 5; ++f)
        {
            DSValue value;
            ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[r], fields[f].m_Field, &value));
            ASSERT_EQ(0, memcmp(&values[r][f], &value.m_Value, DSTypeSize(types[f])));
        }
        const uint8_t* bytes = GetFieldBytes(slot->m_Table, &slot->m_Table->m_Rows[slot->m_Row], 0);
        ASSERT_EQ(0, bytes[9] | bytes[10] | bytes[11]); // Padding is initialized for every appended row.
    }
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, InlineBulkValidationIsAtomicAcrossMemberOrders)
{
    DSFieldDesc members[] = {
        { .m_Field = 10, .m_Type = DS_TYPE_VECTOR3, .m_Offset = 0, .m_Name = "10" },
        { .m_Field = 20, .m_Type = DS_TYPE_BOOLEAN, .m_Offset = 12, .m_Name = "20" }
    };
    DSStructDesc layout = { .m_Fields = members, .m_FieldCount = 2, .m_Size = 16 };
    DSFieldDesc  field = { .m_Field = 1, .m_Type = DS_TYPE_STRUCT, .m_Struct = &layout, .m_Name = "1" };
    DSTableDesc  table = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = 16 };
    HDSContext   store = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    uint64_t    names[] = { 10, 20, 20, 10 };
    DSValueType types[] = { DS_TYPE_VECTOR3, DS_TYPE_BOOLEAN, DS_TYPE_BOOLEAN, DS_TYPE_VECTOR3 };
    DSValueData children[] = { { .m_Vector3 = { 1, 2, 3 } }, { .m_Boolean = 1 }, { .m_Boolean = 2 }, { .m_Vector3 = { 4, 5, 6 } } };
    DSValueData values[] = {
        { .m_Struct = { .m_Count = 2, .m_Source = DS_STRUCT_ARRAY, .m_Array = { .m_Names = names, .m_Types = types, .m_Values = children } } },
        { .m_Struct = { .m_Count = 2, .m_Source = DS_STRUCT_ARRAY, .m_Array = { .m_Names = names + 2, .m_Types = types + 2, .m_Values = children + 2 } } }
    };
    const DSValueType kind = DS_TYPE_STRUCT;
    DSRowDesc         rows[] = {
        { .m_Types = &kind, .m_Values = values, .m_ValueCount = 1 },
        { .m_Types = &kind, .m_Values = values + 1, .m_ValueCount = 1 }
    };
    DSId ids[] = { 123, 456 };
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSAddRows(store, 1, rows, 2, ids));
    ASSERT_EQ((DSId)123, ids[0]);
    ASSERT_EQ((DSId)456, ids[1]);
    ASSERT_EQ(0u, store->m_Tables[0]->m_Rows.Size());
    ASSERT_EQ(0u, store->m_Tables[0]->m_Rows.Size());
    children[2].m_Boolean = 0;
    types[3] = DS_TYPE_NUMBER;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSAddRows(store, 1, rows, 2, ids));
    types[3] = DS_TYPE_VECTOR3;
    names[3] = 99;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSAddRows(store, 1, rows, 2, ids));
    names[3] = 10;
    ASSERT_EQ((DSId)123, ids[0]);
    ASSERT_EQ((DSId)456, ids[1]);
    ASSERT_EQ(0u, store->m_Tables[0]->m_Rows.Size());
    ASSERT_EQ(0u, store->m_Tables[0]->m_Rows.Size());
    ASSERT_EQ(DS_RESULT_OK, DSAddRows(store, 1, rows, 2, ids));
    for (uint32_t i = 0; i < 2; ++i)
    {
        DSValue object, color, enabled;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[i], 1, &object));
        ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&object.m_Value.m_Struct, 10, &color));
        ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&object.m_Value.m_Struct, 20, &enabled));
        ASSERT_EQ(i ? 6.0f : 3.0f, color.m_Value.m_Vector3[2]);
        ASSERT_EQ(i ? 0 : 1, enabled.m_Value.m_Boolean);
    }
    // Borrowed inline views keep working through the general input path.
    HDSContext copy = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(copy, &table));
    for (uint32_t i = 0; i < 2; ++i)
    {
        DSValue view;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[i], 1, &view));
        values[i] = view.m_Value;
    }
    DSId copied[2];
    ASSERT_EQ(DS_RESULT_OK, DSAddRows(copy, 1, rows, 2, copied));
    for (uint32_t i = 0; i < 2; ++i)
    {
        DSValue object, color, enabled;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(copy, copied[i], 1, &object));
        ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&object.m_Value.m_Struct, 10, &color));
        ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&object.m_Value.m_Struct, 20, &enabled));
        ASSERT_EQ(i ? 6.0f : 3.0f, color.m_Value.m_Vector3[2]);
        ASSERT_EQ(i ? 0 : 1, enabled.m_Value.m_Boolean);
    }
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(copy));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, CApiStoreLifecycle)
{
    ASSERT_EQ(0, TestDataStoreLifecycleFromC());
}

// The concise descriptor and field helper work from C without C++ initializer features.
TEST(Data, CApiQuerySetup)
{
    ASSERT_EQ(0, TestDataQuerySetupFromC());
}

TEST(Data, CApiMathAndNestedViews)
{
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc meta[] = { { 42, DS_TYPE_NUMBER, 0 }, { 43, DS_TYPE_VECTOR3, 8 }, { 44, DS_TYPE_VECTOR4, 20 }, { 45, DS_TYPE_MATRIX4, 36 }, { 46, DS_TYPE_STRUCT, 104 }, { 47, DS_TYPE_BOOLEAN, 112 }, { 48, DS_TYPE_STRING, 120 } };
    uint64_t    tag = 7;
    DSTableDesc desc = { 1, &tag, 1, meta, 7, 128 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &desc));
    DSValue vec4 = {};
    vec4.m_Type = DS_TYPE_VECTOR4;
    uint64_t          color_name = 10;
    const DSValueType color_type = DS_TYPE_VECTOR3;
    DSValueData       color = { .m_Vector3 = { 1.0f, 0.5f, 0.25f } };
    DSValue           values[] = { Number(17), Vector3(1, 1, 1), vec4, Matrix(), Struct(&color_name, &color_type, &color, 1), Boolean(0), String("initial") };
    DSId              id;
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(store, 1, 123, values, 7, &id));
    ASSERT_EQ(0, TestDataFromC(store));
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, bytes, size, &size));
    HDSContext      loaded = DSCreateStore(0);
    HDSBlobInstance instance;
    ASSERT_EQ(DS_RESULT_OK, Load(loaded, bytes, size, &instance, 123));
    DSMatrix4 loaded_matrix;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetMatrix4(loaded, FirstId(loaded), 45, &loaded_matrix));
    ASSERT_EQ(44.0f, loaded_matrix.m_Values[3 * 4 + 2]);
    DSValue out;
    for (uint32_t i = 1; i <= 3; ++i)
    {
        DSValue original;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, meta[i].m_Field, &original));
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(loaded, FirstId(loaded), meta[i].m_Field, &out));
        ASSERT_EQ(original.m_Type, out.m_Type);
        ASSERT_EQ(0, memcmp(&original.m_Value, &out.m_Value, DSTypeSize(original.m_Type)));
    }
    ASSERT_EQ(DS_RESULT_OK, DSResetTable(store, 1));
    for (uint32_t i = 1; i <= 3; ++i)
    {
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, meta[i].m_Field, &out));
        ASSERT_EQ(values[i].m_Type, out.m_Type);
        ASSERT_EQ(0, memcmp(&values[i].m_Value, &out.m_Value, DSTypeSize(out.m_Type)));
    }
    ASSERT_EQ(0, TestDataFromC(store));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(loaded));
    delete[] bytes;
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, AlignedMathReadsPreserveFloatBits)
{
    // Zero, negative zero, subnormal, infinities and a NaN with a payload.
    const uint32_t bits[] = { 0, 0x80000000, 1, 0x7f800000, 0xff800000, 0x7fc01234, 0x3f800000, 0xbf800000, 0, 0x80000000, 1, 0x7f800000, 0xff800000, 0x7fc01234, 0x3f800000, 0xbf800000 };
    DSFieldDesc    meta[] = { { 10, DS_TYPE_VECTOR3, 0 }, { 20, DS_TYPE_VECTOR4, 12 }, { 30, DS_TYPE_MATRIX4, 28 } };
    DSTableDesc    desc = { 1, 0, 0, meta, 3, 92 };
    DSValue        values[3] = {};
    for (uint32_t i = 0; i < 3; ++i)
    {
        values[i].m_Type = meta[i].m_Type;
        memcpy(&values[i].m_Value, bits, DSTypeSize(meta[i].m_Type));
    }
    HDSContext source = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(source, &desc));
    DSId id;
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, 1, 0, values, 3, &id));
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, bytes, size, &size));
    HDSContext      loaded = DSCreateStore(0);
    HDSBlobInstance instance;
    ASSERT_EQ(DS_RESULT_OK, Load(loaded, bytes, size, &instance));
    HDSContext stores[] = { source, loaded };
    for (uint32_t mode = 0; mode < 2; ++mode)
    {
        HDSContext store = stores[mode];
        DSId       row = FirstId(store);
        // Read defaults, then the same bit patterns through override storage.
        for (uint32_t overridden = 0; overridden < 2; ++overridden)
        {
            for (uint32_t i = 0; i < 3; ++i)
            {
                if (overridden)
                    ASSERT_EQ(DS_RESULT_OK, DSSetField(store, row, meta[i].m_Field, &values[i]));
                DSValue value;
                ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, row, meta[i].m_Field, &value));
                ASSERT_EQ(meta[i].m_Type, value.m_Type);
                ASSERT_EQ(0, memcmp(bits, &value.m_Value, DSTypeSize(value.m_Type)));
            }
            DSVector3 vector3;
            DSVector4 vector4;
            DSMatrix4 matrix;
            ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3(store, row, 10, &vector3));
            ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector4(store, row, 20, &vector4));
            ASSERT_EQ(DS_RESULT_OK, DSFieldGetMatrix4(store, row, 30, &matrix));
            ASSERT_EQ(0, memcmp(bits, vector3.m_Values, sizeof(vector3.m_Values)));
            ASSERT_EQ(0, memcmp(bits, vector4.m_Values, sizeof(vector4.m_Values)));
            ASSERT_EQ(0, memcmp(bits, matrix.m_Values, sizeof(matrix.m_Values)));
            DSQueryItem fields[] = {
                { .m_Hash = 30, .m_Type = DS_TYPE_MATRIX4, .m_Kind = DS_QUERY_ITEM_FIELD },
                { .m_Hash = 10, .m_Type = DS_TYPE_VECTOR3, .m_Kind = DS_QUERY_ITEM_FIELD },
                { .m_Hash = 20, .m_Type = DS_TYPE_VECTOR4, .m_Kind = DS_QUERY_ITEM_FIELD }
            };
            DSQueryDesc query_desc = { .m_Items = { fields[0], fields[1], fields[2] } };
            HDSQuery    query;
            ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &query_desc, &query));
            uint32_t matrix_field = DSQueryFindField(query, &fields[0]);
            uint32_t vector3_field = DSQueryFindField(query, &fields[1]);
            uint32_t vector4_field = DSQueryFindField(query, &fields[2]);
            ASSERT_NE(UINT32_MAX, matrix_field);
            ASSERT_NE(UINT32_MAX, vector3_field);
            ASSERT_NE(UINT32_MAX, vector4_field);
            ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
            DSIterator it = DSQueryIter(query);
            ASSERT_TRUE(DSIterNext(&it));
            DSRowIterator rows = DSIterRows(&it);
            ASSERT_TRUE(DSRowIterNext(&rows));
            ASSERT_EQ(0, memcmp(bits, DSRowIterGetMatrix4(&rows, matrix_field), sizeof(DSMatrix4)));
            ASSERT_EQ(0, memcmp(bits, DSRowIterGetVector3(&rows, vector3_field), sizeof(DSVector3)));
            ASSERT_EQ(0, memcmp(bits, DSRowIterGetVector4(&rows, vector4_field), sizeof(DSVector4)));
            DSQueryEnd(query);
            DSDestroyQuery(query);
        }
    }
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(loaded));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(source));
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
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc meta = { 10, DS_TYPE_NUMBER, 0 };
    DSTableDesc desc = { 1, 0, 0, &meta, 1, 8 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &desc));
    const DSValueType types[] = { DS_TYPE_NUMBER };
    DSValueData       values[] = { { .m_Number = 1.5 }, { .m_Number = -2 } };
    DSId              ids[2];
    DSRowDesc         rows[] = {
        { .m_Group = 100, .m_Types = types, .m_Values = values, .m_ValueCount = 1, .m_ComponentId = 7 },
        { .m_Group = 200, .m_Types = types, .m_Values = values + 1, .m_ValueCount = 1, .m_ComponentId = 8 }
    };
    ASSERT_EQ(DS_RESULT_OK, DSAddRows(store, 1, rows, 2, ids));
    uint8_t  DM_ALIGNED(8) bytes[sizeof(expected)];
    uint32_t size = 0;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, 0, 0, &size));
    ASSERT_EQ(sizeof(expected), (size_t)size);
    memset(bytes, 0xff, sizeof(bytes));
    ASSERT_EQ(DS_RESULT_BUFFER_TOO_SMALL, DSWriteBlob(store, bytes, size - 1, &size));
    for (uint32_t i = 0; i < sizeof(bytes); ++i)
        ASSERT_EQ(0xff, bytes[i]);
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, bytes, size, &size));
    ASSERT_EQ(0, memcmp(expected, bytes, size));
    HDSContext      loaded = DSCreateStore(0);
    HDSBlobInstance instance;
    ASSERT_EQ(DS_RESULT_OK, Load(loaded, expected, sizeof(expected), &instance, 300));
    ASSERT_EQ((uintptr_t)(expected + 144), (uintptr_t)(GetInstanceTable(instance, 0)->m_Blob + GetInstanceTable(instance, 0)->m_Offsets.m_Rows));
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(loaded, bytes, size, &size));
    ASSERT_EQ(0, memcmp(expected, bytes, size));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(loaded));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, BlobMutableRowsSharedMetadataAndGroupLifetime)
{
    HDSContext source = DSCreateStore(0);
    uint64_t   tag = dmHashString64("light");
    ASSERT_EQ(DS_RESULT_OK, Register(source, 1, &tag, 1));
    ASSERT_EQ(DS_RESULT_OK, Register(source, 2, &tag, 1));
    DSValue values[] = { Number(1), String("shared") };
    DSId    id;
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, 1, 999, values, 2, &id, 11));
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, 1, 999, values, 2, &id, 12));
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, 2, 999, values, 2, &id, 13));
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, 0, 0, &size));
    uint8_t* memory = new uint8_t[size];
    uint8_t* bytes = memory;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, bytes, size, &size));
    uint8_t* unchanged = new uint8_t[size];
    memcpy(unchanged, bytes, size);
    HDSBlob blob;
    ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
    ASSERT_EQ((uintptr_t)bytes, (uintptr_t)blob->m_Data);
    HDSContext  store = DSCreateStore(0);
    DSQueryDesc query_desc = { .m_Items = { { .m_Hash = tag, .m_Kind = DS_QUERY_ITEM_TAG }, { .m_Hash = 20, .m_Type = DS_TYPE_STRING, .m_Kind = DS_QUERY_ITEM_FIELD } } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &query_desc, &query));
    HDSBlobInstance first, second;
    ASSERT_EQ(DS_RESULT_OK, DSAddBlob(store, blob, 100, &first));
    ASSERT_EQ(DS_RESULT_OK, DSAddBlob(store, blob, 200, &second));
    ASSERT_EQ(2u, store->m_Tables.Size());
    ASSERT_EQ((uintptr_t)(GetInstanceTable(first, 0)->m_Blob + GetInstanceTable(first, 0)->m_Offsets.m_Table), (uintptr_t)(GetInstanceTable(second, 0)->m_Blob + GetInstanceTable(second, 0)->m_Offsets.m_Table));
    ASSERT_EQ((uintptr_t)(GetInstanceTable(first, 0)->m_Blob + GetInstanceTable(first, 0)->m_Offsets.m_Rows), (uintptr_t)(GetInstanceTable(second, 0)->m_Blob + GetInstanceTable(second, 0)->m_Offsets.m_Rows));
    ASSERT_EQ((uintptr_t)GetInstanceTable(first, 0), (uintptr_t)GetInstanceTable(second, 0));
    ASSERT_EQ(4u, GetInstanceTable(first, 0)->m_Rows.Size());
    for (uint32_t t = 0; t < store->m_Tables.Size(); ++t)
    {
        DSTable* table = store->m_Tables[t];
        ASSERT_TRUE(table->m_Pool != 0);
        ASSERT_EQ((uintptr_t)bytes, (uintptr_t)table->m_Blob);
        ASSERT_EQ((uintptr_t)0, (uintptr_t)table->m_Payloads);
        uint32_t     source_count = table->m_Rows.Size() / 2;
        const DSRow* records = (const DSRow*)(bytes + table->m_Offsets.m_Rows) - source_count;
        for (uint32_t r = 0; r < table->m_Rows.Size(); ++r)
        {
            const DSRow& row = table->m_Rows[r];
            ASSERT_EQ((DSGroupId)(r < source_count ? 100 : 200), row.m_Group);
            ASSERT_EQ(records[r % source_count].m_ComponentId, row.m_ComponentId);
            ASSERT_EQ(r % source_count, row.m_BaseIndex);
            ASSERT_EQ(r < source_count ? first->m_Index : second->m_Index, row.m_InstanceIndex);
            ASSERT_EQ(r, store->m_Slots[row.m_Slot].m_Row);
            ASSERT_EQ((uintptr_t)table, (uintptr_t)store->m_Slots[row.m_Slot].m_Table);
        }
    }
    DSId ids[2][3] = {};
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSIterator it = DSQueryIter(query);
    uint32_t   total = 0;
    uintptr_t  string = (uintptr_t)(bytes + ReadDataInteger(bytes + 12, 4));
    while (DSIterNext(&it))
    {
        for (uint32_t r = 0; r < DSIterGetCount(&it); ++r)
        {
            DSId     row = DSIterGetId(&it, r);
            uint64_t component = DSGetComponentId(store, row);
            uint32_t group = DSIterGetGroupId(&it, r) == 100 ? 0 : 1;
            ASSERT_TRUE(component >= 11 && component <= 13);
            ids[group][component - 11] = row;
            DSValue out;
            ASSERT_EQ(DS_RESULT_OK, DSIterGetFieldByHash(&it, r, 20, &out));
            ASSERT_EQ(string, (uintptr_t)out.m_Value.m_String);
            ++total;
        }
    }
    DSQueryEnd(query);
    ASSERT_EQ(6u, total);
    ASSERT_NE(ids[0][0], ids[1][0]);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(source));
    DSDestroyBlob(blob);
    DSValue change = String("override");
    ASSERT_EQ(DS_RESULT_OK, DSSetField(store, ids[0][0], 20, &change));
    ASSERT_EQ(DS_RESULT_OK, DSSetField(store, ids[0][1], 20, &change));
    ASSERT_EQ(DS_RESULT_OK, DSSetField(store, ids[0][2], 20, &change));
    ASSERT_EQ(DS_RESULT_OK, DSSetField(store, ids[1][0], 20, &change));
    DSValue out;
    ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, ids[0][0]));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[0][0], 20, &out));
    ASSERT_EQ(string, (uintptr_t)out.m_Value.m_String);
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[0][1], 20, &out));
    ASSERT_STREQ("override", out.m_Value.m_String);
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[0][2], 20, &out));
    ASSERT_STREQ("override", out.m_Value.m_String);
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[1][0], 20, &out));
    ASSERT_STREQ("override", out.m_Value.m_String);
    ASSERT_EQ(DS_RESULT_OK, DSSetField(store, ids[0][0], 20, &change));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[0][1]));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSResetRow(store, ids[0][1]));
    ASSERT_EQ(DS_RESULT_OK, DSResetBlob(first));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGet(store, ids[0][1], 20, &out));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[0][0], 20, &out));
    ASSERT_EQ(string, (uintptr_t)out.m_Value.m_String);
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[0][2], 20, &out));
    ASSERT_EQ(string, (uintptr_t)out.m_Value.m_String);
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[1][0], 20, &out));
    ASSERT_STREQ("override", out.m_Value.m_String);
    ASSERT_EQ(DS_RESULT_OK, DSRemoveBlob(first));
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGet(store, ids[0][i], 20, &out));
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[1][i], 20, &out));
    }
    ASSERT_EQ(DS_RESULT_OK, DSSetField(store, ids[1][0], 20, &out));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[1][0], 20, &out));
    ASSERT_NE(string, (uintptr_t)out.m_Value.m_String);
    ASSERT_EQ(DS_RESULT_OK, DSResetField(store, ids[1][0], 20));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[1][0], 20, &out));
    ASSERT_EQ(string, (uintptr_t)out.m_Value.m_String);
    ASSERT_EQ(DS_RESULT_OK, DSRemoveBlob(second));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    it = DSQueryIter(query);
    ASSERT_FALSE(DSIterNext(&it));
    DSQueryEnd(query);
    DSDestroyQuery(query);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    ASSERT_EQ(0, memcmp(unchanged, bytes, size));
    delete[] unchanged;
    delete[] memory;
}

TEST(Data, NestedViewsAllKindsAndReset)
{
    DSValue vec4 = {};
    vec4.m_Type = DS_TYPE_VECTOR4;
    vec4.m_Value.m_Vector4[3] = 8.0f;
    DSValue           elements[] = { Null(), Number(1.5), Boolean(1), String("shared"), Vector3(1, 2, 3), vec4, Matrix(), List(0, 0), Struct(0, 0, 0, 0) };
    const DSValueType types[] = { DS_TYPE_LIST, DS_TYPE_STRING };
    DSValueData       fields[] = { List(elements, 9).m_Value, String("shared").m_Value };
    uint64_t          names[] = { 10, 20 };
    DSValue           value = Struct(names, types, fields, 2);
    DSFieldDesc       meta = { 1, DS_TYPE_STRUCT, 0 };
    DSTableDesc       desc = { 1, 0, 0, &meta, 1, 8 };
    HDSContext        source = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(source, &desc));
    DSId id;
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, 1, 0, &value, 1, &id));
    elements[1] = Number(99);
    names[0] = 99;
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    uint8_t* roundtrip = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, bytes, size, &size));
    HDSContext      store = DSCreateStore(0);
    HDSBlobInstance instance;
    ASSERT_EQ(DS_RESULT_OK, Load(store, bytes, size, &instance));
    id = FirstId(store);
    DSValue object, list, child, shared;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, 1, &object));
    ASSERT_EQ((uintptr_t)(bytes), (uintptr_t)object.m_Value.m_Struct.m_View.m_Buffer);
    ASSERT_EQ(DS_STRUCT_PACKED, object.m_Value.m_Struct.m_Source);
    ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&object.m_Value.m_Struct, 10, &list));
    ASSERT_EQ(9u, list.m_Value.m_List.m_Count);
    for (uint32_t i = 0; i < 9; ++i)
    {
        ASSERT_EQ(DS_RESULT_OK, DSGetListValue(&list.m_Value.m_List, i, &child));
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
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSGetListValue(&list.m_Value.m_List, 9, &child));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSGetStructField(&object.m_Value.m_Struct, 99, &child));
    ASSERT_EQ(DS_RESULT_OK, DSGetListValue(&list.m_Value.m_List, 3, &child));
    ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&object.m_Value.m_Struct, 20, &shared));
    ASSERT_EQ((uintptr_t)child.m_Value.m_String, (uintptr_t)shared.m_Value.m_String);
    ASSERT_TRUE((const uint8_t*)shared.m_Value.m_String >= bytes && (const uint8_t*)shared.m_Value.m_String < bytes + size);
    ASSERT_EQ((uintptr_t)(bytes), (uintptr_t)GetInstanceTable(instance, 0)->m_Blob); // Values remain borrowed.
    ASSERT_EQ((uintptr_t)0, (uintptr_t)instance->m_Payloads);
    ASSERT_EQ(DS_RESULT_OK, DSSetField(store, id, 1, &object)); // Copy only when explicitly setting.
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, 1, &child));
    ASSERT_TRUE(child.m_Value.m_Struct.m_View.m_Buffer != object.m_Value.m_Struct.m_View.m_Buffer);
    ASSERT_EQ(DS_STRUCT_ROW, child.m_Value.m_Struct.m_Source);
    ASSERT_EQ(DS_RESULT_OK, DSSetField(store, id, 1, &child)); // Owned view self-assignment.
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, 1, &child));
    ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&child.m_Value.m_Struct, 20, &shared));
    ASSERT_STREQ("shared", shared.m_Value.m_String);
    ASSERT_EQ(DS_RESULT_OK, DSResetField(store, id, 1));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, 1, &child));
    ASSERT_EQ(object.m_Value.m_Struct.m_View.m_Offset, child.m_Value.m_Struct.m_View.m_Offset);
    DSResetBlob(instance);
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, roundtrip, size, &size));
    ASSERT_EQ(0, memcmp(bytes, roundtrip, size));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(source));
    delete[] bytes;
    delete[] roundtrip;
}

TEST(Data, CompactNestedBulkStorageAndBorrowedInsertion)
{
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc meta = { 1, DS_TYPE_STRUCT, 0 };
    DSTableDesc desc = { 1, 0, 0, &meta, 1, 8 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &desc));
    uint64_t          names[] = { 10, 20 };
    const DSValueType types[] = { DS_TYPE_VECTOR3, DS_TYPE_NUMBER };
    DSValueData       fields[] = { { .m_Vector3 = { 1, 2, 3 } }, { .m_Number = 4 } };
    DSValue           value = Struct(names, types, fields, 2);
    DSRowDesc         rows[128];
    DSId              ids[128];
    for (uint32_t i = 0; i < 128; ++i)
    {
        DSRowDesc row = { .m_Group = i, .m_Types = &value.m_Type, .m_Values = &value.m_Value, .m_ValueCount = 1 };
        rows[i] = row;
    }
    ASSERT_EQ(DS_RESULT_OK, DSAddRows(store, 1, rows, 128, ids));
    DSMemoryStats memory;
    GetDataMemoryStats(store, &memory);
    ASSERT_EQ((uint64_t)0, memory.m_BaseBlocks);
    ASSERT_EQ((uint64_t)128, memory.m_StructRows);
    ASSERT_EQ(1u, FindTable(store, 1)->m_Structs->m_Tables.Size());
    ASSERT_EQ(24u, FindTable(store, 1)->m_Structs->m_Tables[0]->m_RowStride);
    ASSERT_TRUE(memory.m_StructRowBytes < (uint64_t)(128 * 64));

    DSValue borrowed;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[0], 1, &borrowed));
    ASSERT_TRUE(borrowed.m_Value.m_Struct.m_View.m_StructTable != 0);
    ASSERT_EQ(DS_STRUCT_ROW, borrowed.m_Value.m_Struct.m_Source);
    for (uint32_t i = 0; i < 128; ++i)
        rows[i].m_Values = &borrowed.m_Value;
    // Growing row/arena storage must keep the source view valid until copying ends.
    ASSERT_EQ(DS_RESULT_OK, DSAddRows(store, 1, rows, 128, ids));
    fields[1].m_Number = 99;
    ASSERT_EQ(DS_RESULT_OK, DSSetField(store, ids[0], 1, &value));
    ASSERT_EQ(DS_RESULT_OK, DSResetTable(store, 1));
    for (uint32_t i = 0; i < 128; ++i)
    {
        DSValue object, child;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[i], 1, &object));
        ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&object.m_Value.m_Struct, 10, &child));
        ASSERT_EQ(3.0f, child.m_Value.m_Vector3[2]);
        ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&object.m_Value.m_Struct, 20, &child));
        ASSERT_EQ(4.0, child.m_Value.m_Number);
    }
    GetDataMemoryStats(store, &memory);
    ASSERT_EQ((uint64_t)0, memory.m_BaseBlocks);
    ASSERT_EQ((uint64_t)256, memory.m_StructRows);
    ASSERT_EQ((uint64_t)0, memory.m_PayloadBlocks);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Child slots are reused without moving survivors; empty buffers are freed and
// subsequent insertions reuse the layout without following stale free slots.
TEST(Data, OwnedStructRowsShareLayoutsButNotValues)
{
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc field = { .m_Field = 1, .m_Type = DS_TYPE_STRUCT };
    DSTableDesc desc = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = 8 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &desc));
    uint64_t    names[] = { 2, 3 };
    DSValueType types[] = { DS_TYPE_NUMBER, DS_TYPE_VECTOR3 };
    DSValueData values[] = { { .m_Number = 10 }, { .m_Vector3 = { 1, 2, 3 } } };
    DSValue     object = Struct(names, types, values, 2);
    DSId        ids[3];
    for (uint32_t i = 0; i < 3; ++i)
        ASSERT_EQ(DS_RESULT_OK, AddTestRow(store, 1, 0, &object, 1, &ids[i]));
    DSTable* table = FindTable(store, 1);
    ASSERT_EQ(1u, table->m_Structs->m_Tables.Size());
    DSStructTable* children = table->m_Structs->m_Tables[0];
    ASSERT_EQ(3u, children->m_RowCount);
    DSValue first, second, child;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[0], 1, &first));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[1], 1, &second));
    ASSERT_NE(first.m_Value.m_Struct.m_View.m_Offset, second.m_Value.m_Struct.m_View.m_Offset);

    // Input member order is not part of the shared layout's identity.
    uint64_t    reordered_names[] = { 3, 2 };
    DSValueType reordered_types[] = { DS_TYPE_VECTOR3, DS_TYPE_NUMBER };
    DSValueData reordered_values[] = { { .m_Vector3 = { 4, 5, 6 } }, { .m_Number = 20 } };
    DSValue     replacement = Struct(reordered_names, reordered_types, reordered_values, 2);
    ASSERT_EQ(DS_RESULT_OK, DSSetField(store, ids[0], 1, &replacement));
    ASSERT_EQ(1u, table->m_Structs->m_Tables.Size());
    ASSERT_EQ(4u, children->m_RowCount); // Three defaults and one replacement.
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[0], 1, &first));
    ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&first.m_Value.m_Struct, 2, &child));
    ASSERT_EQ(20.0, child.m_Value.m_Number);
    ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&second.m_Value.m_Struct, 2, &child));
    ASSERT_EQ(10.0, child.m_Value.m_Number);
    ASSERT_EQ(DS_RESULT_OK, DSResetField(store, ids[0], 1));
    ASSERT_EQ(3u, children->m_RowCount);

    uint32_t capacity = children->m_Values.Capacity();
    uint32_t slots = children->m_Values.Size();
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[1])); // Swaps the parent row only.
    ASSERT_EQ(2u, children->m_RowCount);
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[2], 1, &second));
    ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&second.m_Value.m_Struct, 2, &child));
    ASSERT_EQ(10.0, child.m_Value.m_Number);
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(store, 1, 0, &object, 1, &ids[1]));
    ASSERT_EQ(slots, children->m_Values.Size());
    ASSERT_EQ(capacity, children->m_Values.Capacity());
    for (uint32_t i = 0; i < 3; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[i]));
    ASSERT_EQ(0u, children->m_RowCount);
    ASSERT_EQ(0u, children->m_Values.Capacity());
    for (uint32_t i = 0; i < 3; ++i)
        ASSERT_EQ(DS_RESULT_OK, AddTestRow(store, 1, 0, &object, 1, &ids[i]));
    ASSERT_EQ(1u, table->m_Structs->m_Tables.Size());
    ASSERT_EQ(3u, children->m_RowCount);
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[i], 1, &first));
        ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&first.m_Value.m_Struct, 2, &child));
        ASSERT_EQ(10.0, child.m_Value.m_Number);
        ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[i]));
    }
    ASSERT_EQ(0u, children->m_Values.Capacity());
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Recursive children can share a layout with their parents. Its buffer must
// remain live until every descendant and retained default has been released.
TEST(Data, OwnedStructRowsReleaseRecursiveTrees)
{
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc field = { .m_Field = 1, .m_Type = DS_TYPE_STRUCT };
    DSTableDesc desc = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = 8 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &desc));
    uint64_t    name = 2;
    DSValueType number_type = DS_TYPE_NUMBER;
    DSValueType struct_type = DS_TYPE_STRUCT;
    DSValueData number = { .m_Number = 42 };
    DSValue     leaf = Struct(&name, &number_type, &number, 1);
    DSValue     middle = Struct(&name, &struct_type, &leaf.m_Value, 1);
    DSValue     root = Struct(&name, &struct_type, &middle.m_Value, 1);
    DSId        ids[40];
    for (uint32_t i = 0; i < 40; ++i)
        ASSERT_EQ(DS_RESULT_OK, AddTestRow(store, 1, 0, &root, 1, &ids[i]));
    DSTable* table = FindTable(store, 1);
    ASSERT_EQ(2u, table->m_Structs->m_Tables.Size());
    ASSERT_EQ(80u, table->m_Structs->m_Tables[0]->m_RowCount);
    ASSERT_EQ(40u, table->m_Structs->m_Tables[1]->m_RowCount);
    DSValue view, child, grandchild;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[0], 1, &view));
    ASSERT_EQ(DS_RESULT_OK, DSSetField(store, ids[0], 1, &view)); // Same-layout growth during a borrowed copy.
    ASSERT_EQ(82u, table->m_Structs->m_Tables[0]->m_RowCount);
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[0], 1, &view));
    ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&view.m_Value.m_Struct, name, &child));
    ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&child.m_Value.m_Struct, name, &grandchild));
    ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&grandchild.m_Value.m_Struct, name, &view));
    ASSERT_EQ(42.0, view.m_Value.m_Number);
    ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, ids[0]));
    ASSERT_EQ(80u, table->m_Structs->m_Tables[0]->m_RowCount);
    for (uint32_t i = 0; i < 40; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[i]));
    for (uint32_t t = 0; t < table->m_Structs->m_Tables.Size(); ++t)
    {
        ASSERT_EQ(0u, table->m_Structs->m_Tables[t]->m_RowCount);
        ASSERT_EQ(0u, table->m_Structs->m_Tables[t]->m_Values.Capacity());
    }
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, OwnedStructRowsKeepBlobDefaultsBorrowed)
{
    HDSContext  source = DSCreateStore(0);
    DSFieldDesc field = { .m_Field = 1, .m_Type = DS_TYPE_STRUCT };
    DSTableDesc desc = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = 8 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(source, &desc));
    uint64_t    name = 2;
    DSValueType type = DS_TYPE_NUMBER;
    DSValueData number = { .m_Number = 10 };
    DSValue     object = Struct(&name, &type, &number, 1);
    DSId        id;
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, 1, 0, &object, 1, &id));
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    uint8_t* copy = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, bytes, size, &size));
    memcpy(copy, bytes, size);
    HDSBlob blob;
    ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
    HDSContext      store = DSCreateStore(0);
    HDSBlobInstance instances[2];
    for (uint32_t i = 0; i < 2; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSAddBlob(store, blob, i, &instances[i]));
    DSTable* table = GetInstanceTable(instances[0], 0);
    ASSERT_TRUE(table->m_Structs == 0);
    DSId ids[] = { MakeId(store, GetInstanceSlots(instances[0])[0]), MakeId(store, GetInstanceSlots(instances[1])[0]) };
    number.m_Number = 20;
    for (uint32_t i = 0; i < 2; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSSetField(store, ids[i], 1, &object));
    ASSERT_EQ(2u, table->m_Structs->m_Tables[0]->m_RowCount);
    ASSERT_EQ(DS_RESULT_OK, DSResetBlob(instances[0]));
    ASSERT_EQ(1u, table->m_Structs->m_Tables[0]->m_RowCount);
    DSValue value, child;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[0], 1, &value));
    ASSERT_EQ((uintptr_t)bytes, (uintptr_t)value.m_Value.m_Struct.m_View.m_Buffer);
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[1], 1, &value));
    ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&value.m_Value.m_Struct, name, &child));
    ASSERT_EQ(20.0, child.m_Value.m_Number);
    ASSERT_EQ(DS_RESULT_OK, DSRemoveBlob(instances[1]));
    ASSERT_EQ(0u, table->m_Structs->m_Tables[0]->m_RowCount);
    ASSERT_EQ(0, memcmp(bytes, copy, size));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveBlob(instances[0]));
    DSDestroyBlob(blob);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(source));
    delete[] copy;
    delete[] bytes;
}

TEST(Data, NestedValidationAndDepth)
{
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc meta = { 1, DS_TYPE_LIST, 0 };
    DSTableDesc desc = { 1, 0, 0, &meta, 1, 8 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &desc));
    DSValue base = List(0, 0);
    DSId    id;
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(store, 1, 0, &base, 1, &id));
    DSValue invalid = List(0, 1);
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSetField(store, id, 1, &invalid));
    invalid = List(&invalid, 1);
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSetField(store, id, 1, &invalid));
    DSValue bad = Boolean(2);
    invalid = List(&bad, 1);
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSetField(store, id, 1, &invalid));
    uint64_t    names[] = { 10, 10 };
    DSValueType types[] = { DS_TYPE_NULL, DS_TYPE_NULL };
    DSValueData fields[2] = {};
    DSValue     object = Struct(names, types, fields, 2);
    invalid = List(&object, 1);
    ASSERT_EQ(DS_RESULT_ALREADY_EXISTS, DSSetField(store, id, 1, &invalid));
    names[1] = 20;
    object.m_Value.m_Struct.m_Array.m_Types = 0;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSetField(store, id, 1, &invalid));
    object.m_Value.m_Struct.m_Array.m_Types = types;
    types[1] = (DSValueType)99;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSetField(store, id, 1, &invalid));
    types[1] = DS_TYPE_NULL;
    DSValue values[DATA_MAX_NESTING + 2];
    values[0] = Number(1);
    for (uint32_t i = 1; i < DATA_MAX_NESTING + 2; ++i)
        values[i] = List(&values[i - 1], 1);
    ASSERT_EQ(DS_RESULT_OK, DSSetField(store, id, 1, &values[DATA_MAX_NESTING]));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSetField(store, id, 1, &values[DATA_MAX_NESTING + 1]));
    uint8_t  DM_ALIGNED(8) bytes[4096];
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, bytes, sizeof(bytes), &size));
    HDSBlob blob;
    ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
    DSDestroyBlob(blob);
    // Last number becomes an empty list, which would add a 65th container.
    uint32_t container = (uint32_t)ReadDataInteger(bytes + 24 + DATA_TABLE_HEADER_SIZE + DATA_FIELD_META_SIZE + sizeof(DSRow), 8);
    for (uint32_t i = 1; i < DATA_MAX_NESTING; ++i)
        container = (uint32_t)ReadDataInteger(bytes + container + 12, 4);
    WriteDataInteger(bytes + container + 8, DS_TYPE_LIST, 4);
    memset(bytes + size - 8, 0, 8);
    bytes[size - 8] = 8;
    ASSERT_EQ(DS_RESULT_INVALID_FORMAT, DSLoadBlob(bytes, size, &blob));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, MalformedBlobDirectoryLayoutAndValues)
{
    HDSContext  source = DSCreateStore(0);
    DSFieldDesc meta[] = { { 1, DS_TYPE_BOOLEAN, 0 }, { 2, DS_TYPE_STRING, 8 }, { 3, DS_TYPE_LIST, 16 } };
    DSTableDesc desc = { 1, 0, 0, meta, 3, 24 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(source, &desc));
    desc.m_Type = 2;
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(source, &desc));
    DSValue item = Number(2);
    DSValue values[] = { Boolean(1), String("text"), List(&item, 1) };
    DSId    id;
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, 1, 0, values, 3, &id));
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, 2, 0, values, 3, &id));
    uint8_t  DM_ALIGNED(8) bytes[1024], saved[1024];
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, bytes, sizeof(bytes), &size));
    memcpy(saved, bytes, size);
    HDSBlob sentinel;
    ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(saved, size, &sentinel));
    for (uint32_t length = 0; length < size; ++length)
    {
        HDSBlob output = sentinel;
        ASSERT_EQ(DS_RESULT_INVALID_FORMAT, DSLoadBlob(bytes, length, &output));
        ASSERT_EQ((uintptr_t)sentinel, (uintptr_t)output);
    }
    uint32_t second = (uint32_t)ReadDataInteger(bytes + 20, 4);
    // First table starts at 24, metadata at 48, row record at 144, values at 176.
    // The record's group, slot, base/instance indices and reserved bytes must be zero.
    const uint32_t corrupt[] = { 0, 4, 8, 12, 16, 20, 32, 34, 36, 40, 44, 46, 56, 60, 64, 66, 68, 88, 92, 96, 98, 100, 120, 124, 128, 130, 132, 144, 160, 164, 168, 172, 176, 184, 192 };
    for (uint32_t i = 0; i < sizeof(corrupt) / sizeof(corrupt[0]); ++i)
    {
        bytes[corrupt[i]] = 0xff;
        HDSBlob output = sentinel;
        ASSERT_EQ(DS_RESULT_INVALID_FORMAT, DSLoadBlob(bytes, size, &output));
        ASSERT_EQ((uintptr_t)sentinel, (uintptr_t)output);
        memcpy(bytes, saved, size);
    }
    bytes[second + offsetof(DSTableHeader, m_RowStride)] = 0;
    HDSBlob output = sentinel;
    ASSERT_EQ(DS_RESULT_INVALID_FORMAT, DSLoadBlob(bytes, size, &output));
    memcpy(bytes, saved, size);
    memcpy(bytes + 80, bytes + 48, 8); // Repeated field name in shared metadata.
    ASSERT_EQ(DS_RESULT_INVALID_FORMAT, DSLoadBlob(bytes, size, &output));
    memcpy(bytes, saved, size);
    WriteDataInteger(bytes + 92, 0, 4); // String overlaps boolean in every row.
    ASSERT_EQ(DS_RESULT_INVALID_FORMAT, DSLoadBlob(bytes, size, &output));
    memcpy(bytes, saved, size);
    uint32_t nested = (uint32_t)ReadDataInteger(bytes + 192, 8);
    bytes[nested + 8] = 99;
    ASSERT_EQ(DS_RESULT_INVALID_FORMAT, DSLoadBlob(bytes, size, &output));
    memcpy(bytes, saved, size);
    bytes[size - 1] = 'X';
    ASSERT_EQ(DS_RESULT_INVALID_FORMAT, DSLoadBlob(bytes, size, &output));
    memcpy(bytes, saved, size);
    bytes[size] = 0;
    ASSERT_EQ(DS_RESULT_INVALID_FORMAT, DSLoadBlob(bytes, size + 1, &output));
    ASSERT_EQ(DS_RESULT_INVALID_FORMAT, DSLoadBlob(0, 0, &output));
    ASSERT_EQ((uintptr_t)sentinel, (uintptr_t)output);
    DSDestroyBlob(sentinel);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(source));
}

TEST(Data, EmptyNullAndBorrowedResourceReuse)
{
    HDSContext source = DSCreateStore(0);
    uint8_t    DM_ALIGNED(8) bytes[256];
    uint32_t   size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, bytes, sizeof(bytes), &size));
    ASSERT_EQ(16u, size);
    HDSBlob blob;
    ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
    HDSBlobInstance instance;
    HDSContext      store = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, DSAddBlob(store, blob, 1, &instance));
    DSResetBlob(instance);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    ASSERT_EQ(1u, blob->m_RefCount);
    DSDestroyBlob(blob);
    DSFieldDesc meta = { 1, DS_TYPE_NULL, 0 };
    DSTableDesc desc = { 1, 0, 0, &meta, 1, 0 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(source, &desc));
    DSValue value = Null();
    DSId    id;
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, 1, 0, &value, 1, &id));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(source, id, 1, &value));
    ASSERT_EQ(DS_RESULT_OK, DSSetField(source, id, 1, &value));
    DSTableDesc empty = { 2, 0, 0, 0, 0, 0 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(source, &empty));
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, 2, 0, 0, 0, &id));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGet(source, id, 1, &value));
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, bytes, sizeof(bytes), &size));
    ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
    ASSERT_EQ(2u, blob->m_TableCount);
    ASSERT_EQ(2u, blob->m_RowCount);
    store = DSCreateStore(0);
    HDSContext      another = DSCreateStore(0);
    HDSBlobInstance second;
    ASSERT_EQ(DS_RESULT_OK, DSAddBlob(store, blob, 1, &instance));
    ASSERT_EQ(DS_RESULT_OK, DSAddBlob(another, blob, 2, &second));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    ASSERT_EQ(2u, blob->m_RefCount);
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(another, FirstId(another), 1, &value));
    ASSERT_EQ(DS_TYPE_NULL, value.m_Type);
    DSDestroyBlob(blob);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(another));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(source));
}

TEST(Data, ResetAddedValuesAllocationBlocksAndDefaults)
{
    HDSContext store = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, Register(store, 1));
    DSId first, second;
    ASSERT_EQ(DS_RESULT_OK, Add(store, 1, 1, 17, "original", &first));
    DSValue value = String("new baseline");
    ASSERT_EQ(DS_RESULT_OK, DSSetField(store, first, 20, &value));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, first, 20, &value));
    ASSERT_EQ(DS_RESULT_OK, Add(store, 1, 2, 23, value.m_Value.m_String, &second));
    char large[8193];
    memset(large, 'L', sizeof(large) - 1);
    large[sizeof(large) - 1] = 0;
    for (uint32_t cycle = 0; cycle < 3; ++cycle)
    {
        value = String(large);
        ASSERT_EQ(DS_RESULT_OK, DSSetField(store, first, 20, &value));
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, first, 20, &value));
        ASSERT_STREQ(large, value.m_Value.m_String);
        ASSERT_EQ(DS_RESULT_OK, DSSetField(store, first, 20, &value));
        for (uint32_t i = 0; i < 300; ++i)
        {
            value = String("replacement");
            ASSERT_EQ(DS_RESULT_OK, DSSetField(store, first, 20, &value));
        }
        ASSERT_EQ(DS_RESULT_OK, DSResetTable(store, 1));
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, first, 20, &value));
        ASSERT_STREQ("original", value.m_Value.m_String);
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, second, 20, &value));
        ASSERT_STREQ("new baseline", value.m_Value.m_String);
    }
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSResetField(store, 0, 20));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSResetField(store, first, 999));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSResetTable(store, 999));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}

TEST(Data, TypedLookupHashCollisionsAndWideIndexes)
{
    const uint32_t count = 258;
    DSFieldDesc    fields[count];
    DSValue        values[count];
    for (uint32_t i = 0; i < count; ++i)
    {
        fields[i] = { .m_Field = 1 + 16 * i, .m_Type = DS_TYPE_NUMBER, .m_Offset = i * 16 };
        values[i] = Number(i);
    }
    // Colliding names have different kinds; two direct candidates exceed 255.
    fields[1].m_Type = DS_TYPE_VECTOR3;
    values[1] = Vector3(1, 2, 3);
    fields[256].m_Field = 2;
    fields[257].m_Field = 0;
    DSTableDesc desc = { .m_Type = 1, .m_Fields = fields, .m_FieldCount = count, .m_RowStride = count * 16 };
    HDSContext  source = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(source, &desc));
    DSId original;
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, 1, 0, values, count, &original));
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, bytes, size, &size));
    HDSContext      loaded = DSCreateStore(0);
    HDSBlobInstance instance;
    ASSERT_EQ(DS_RESULT_OK, Load(loaded, bytes, size, &instance));
    HDSContext stores[] = { source, loaded };
    DSId       ids[] = { original, FirstId(loaded) };
    for (uint32_t mode = 0; mode < 2; ++mode)
    {
        HDSContext store = stores[mode];
        DSId       id = ids[mode];
        double     number;
        DSVector3  vector;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, id, 1, &number));
        ASSERT_EQ(0.0, number);
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3(store, id, 17, &vector));
        ASSERT_EQ(3.0f, vector.m_Values[2]);
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, id, fields[255].m_Field, &number));
        ASSERT_EQ(255.0, number);
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, id, 2, &number));
        ASSERT_EQ(256.0, number);
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, id, 0, &number));
        ASSERT_EQ(257.0, number);
        number = 123;
        ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSFieldGetNumber(store, id, 17, &number));
        ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGetNumber(store, id, 1 + 16 * count, &number));
        ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGetNumber(store, id, 3, &number));
        ASSERT_EQ(123.0, number);
        ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, id));
        ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGetNumber(store, id, 2, &number));
        ASSERT_EQ(123.0, number);
        ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    }
    delete[] bytes;
}

TEST(Data, TypedAccessErrorsPackedValuesAndReset)
{
    HDSContext source = DSCreateStore(0);
    // Field order differs from query binding order.
    DSFieldDesc meta[] = { { 10, DS_TYPE_VECTOR3, 0 }, { 20, DS_TYPE_STRING, 16 }, { 30, DS_TYPE_BOOLEAN, 24 } };
    DSTableDesc table = { 1, 0, 0, meta, 3, 32 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(source, &table));
    DSValue values[] = { Vector3(1, 2, 3), String("packed"), Boolean(1) };
    DSId    original;
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, 1, 0, values, 3, &original));
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, bytes, size, &size));
    uint8_t* saved = new uint8_t[size];
    memcpy(saved, bytes, size);

    HDSContext      loaded = DSCreateStore(0);
    HDSBlobInstance instance;
    ASSERT_EQ(DS_RESULT_OK, Load(loaded, bytes, size, &instance));
    HDSContext stores[] = { source, loaded };
    for (uint32_t mode = 0; mode < 2; ++mode)
    {
        HDSContext  store = stores[mode];
        DSQueryItem fields[] = {
            { .m_Hash = 30, .m_Type = DS_TYPE_BOOLEAN, .m_Kind = DS_QUERY_ITEM_FIELD },
            { .m_Hash = 10, .m_Type = DS_TYPE_VECTOR3, .m_Kind = DS_QUERY_ITEM_FIELD },
            { .m_Hash = 20, .m_Type = DS_TYPE_STRING, .m_Kind = DS_QUERY_ITEM_FIELD }
        };
        DSQueryDesc desc = { .m_Items = { fields[0], fields[1], fields[2] } };
        HDSQuery    query;
        ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
        DSIterator it = DSQueryIter(query);
        DSVector3  vector = { { 7, 8, 9 } };
        ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, GetTestFieldVector3(&it, 0, 1, &vector));
        ASSERT_EQ(7.0f, vector.m_Values[0]);
        ASSERT_TRUE(DSIterNext(&it));
        DSId id = DSIterGetId(&it, 0);
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3(store, id, 10, &vector));
        ASSERT_EQ(1.0f, vector.m_Values[0]);
        ASSERT_EQ(3.0f, vector.m_Values[2]);
        const char* string;
        ASSERT_EQ(DS_RESULT_OK, GetTestFieldString(&it, 0, 2, &string));
        ASSERT_STREQ("packed", string);
        if (mode)
            ASSERT_TRUE(string >= (const char*)bytes && string < (const char*)bytes + size);

        double number = 123;
        ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, GetTestFieldNumber(&it, 1, 1, &number));
        ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, GetTestFieldNumber(&it, 0, 3, &number));
        ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, SetTestFieldNumber(&it, 0, 1, 4));
        ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, SetTestFieldVector3(&it, 1, 1, &vector));
        ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, SetTestFieldBoolean(&it, 0, 0, 2));
        ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, SetTestFieldString(&it, 0, 2, 0));
        DSQueryEnd(query);
        ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSFieldGetNumber(store, id, 10, &number));
        ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGetNumber(store, 0, 10, &number));
        ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGetNumber(store, id, 99, &number));
        ASSERT_EQ(123.0, number);
        ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSetFieldNumber(store, id, 10, 4));
        ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSetFieldBoolean(store, id, 30, 2));
        ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSetFieldString(store, id, 20, 0));
        ASSERT_EQ(DS_RESULT_NOT_FOUND, DSSetFieldVector3(store, 0, 10, &vector));
        ASSERT_EQ(DS_RESULT_NOT_FOUND, DSSetFieldVector3(store, id, 99, &vector));
        DSMemoryStats memory;
        GetDataMemoryStats(store, &memory);
        ASSERT_EQ((uint64_t)0, memory.m_PayloadBlocks);

        vector.m_Values[1] = 8;
        ASSERT_EQ(DS_RESULT_OK, DSSetFieldVector3(store, id, 10, &vector));
        vector.m_Values[1] = 99; // Set copied the caller's math value.
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3(store, id, 10, &vector));
        ASSERT_EQ(8.0f, vector.m_Values[1]);
        ASSERT_EQ(DS_RESULT_OK, DSSetFieldBoolean(store, id, 30, 0));
        uint8_t boolean = 1;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetBoolean(store, id, 30, &boolean));
        ASSERT_EQ(0, boolean);
        char replacement[] = "changed";
        ASSERT_EQ(DS_RESULT_OK, DSSetFieldString(store, id, 20, replacement));
        replacement[0] = 'X';
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetString(store, id, 20, &string));
        ASSERT_STREQ("changed", string);
        ASSERT_EQ(DS_RESULT_OK, DSSetFieldString(store, id, 20, string));
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetString(store, id, 20, &string));
        ASSERT_STREQ("changed", string);
        ASSERT_EQ(DS_RESULT_OK, DSResetField(store, id, 10));
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3(store, id, 10, &vector));
        ASSERT_EQ(2.0f, vector.m_Values[1]);
        if (mode)
            DSResetBlob(instance);
        else
            ASSERT_EQ(DS_RESULT_OK, DSResetTable(store, 1));
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetBoolean(store, id, 30, &boolean));
        ASSERT_EQ(1, boolean);
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetString(store, id, 20, &string));
        ASSERT_STREQ("packed", string);
        GetDataMemoryStats(store, &memory);
        ASSERT_EQ((uint64_t)0, memory.m_PayloadBlocks);
        ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, id));
        ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGetVector3(store, id, 10, &vector));
        ASSERT_EQ(DS_RESULT_NOT_FOUND, DSSetFieldVector3(store, id, 10, &vector));
        ASSERT_EQ(2.0f, vector.m_Values[1]);
        DSDestroyQuery(query);
    }
    ASSERT_EQ(0, memcmp(saved, bytes, size));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(source));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(loaded));
    delete[] saved;
    delete[] bytes;
}

TEST(Data, BoundQueryRegistrationChurn)
{
    HDSContext source = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, Register(source, 1));
    DSId id;
    ASSERT_EQ(DS_RESULT_OK, Add(source, 1, 0, 5, "packed", &id));
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, bytes, size, &size));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(source));
    HDSBlob blob;
    ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
    HDSContext  store = DSCreateStore(0);
    DSQueryItem fields[] = {
        { .m_Hash = 10, .m_Type = DS_TYPE_NUMBER, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD },
        { .m_Hash = 20, .m_Type = DS_TYPE_STRING, .m_Kind = DS_QUERY_ITEM_FIELD }
    };
    DSQueryDesc desc = { .m_Items = { fields[0], fields[1] } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    // Cross registration pages and grow dense storage, then reuse slots with live queries.
    const uint32_t  count = 600;
    HDSBlobInstance instances[count];
    for (uint32_t cycle = 0; cycle < 3; ++cycle)
    {
        for (uint32_t i = 0; i < count; ++i)
            ASSERT_EQ(DS_RESULT_OK, DSAddBlob(store, blob, i + 1, &instances[i]));
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
        DSIterator it = DSQueryIter(query);
        uint32_t   visited = 0;
        while (DSIterNext(&it))
        {
            ASSERT_EQ(count, DSIterGetCount(&it));
            for (uint32_t r = 0; r < DSIterGetCount(&it); ++r)
            {
                ASSERT_EQ(DS_RESULT_OK, SetTestFieldNumber(&it, r, 0, (double)DSIterGetGroupId(&it, r)));
                ++visited;
            }
        }
        ASSERT_EQ(count, visited);
        DSQueryEnd(query);
        DSMemoryStats before, after;
        GetDataMemoryStats(store, &before);
        ASSERT_EQ((uint64_t)1, before.m_Tables);
        ASSERT_EQ((uint64_t)count, before.m_Instances);
        for (uint32_t i = 0; i < count; i += 2)
            ASSERT_EQ(DS_RESULT_OK, DSRemoveBlob(instances[i]));
        for (uint32_t i = 0; i < count; i += 2)
            ASSERT_EQ(DS_RESULT_OK, DSAddBlob(store, blob, 1000 + i, &instances[i]));
        GetDataMemoryStats(store, &after);
        ASSERT_EQ(before.m_TotalBytes, after.m_TotalBytes);
        ASSERT_EQ(before.m_QueryBytes, after.m_QueryBytes);
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
        it = DSQueryIter(query);
        visited = 0;
        while (DSIterNext(&it))
        {
            for (uint32_t r = 0; r < DSIterGetCount(&it); ++r)
            {
                double      number;
                const char* string;
                DSGroupId   group = DSIterGetGroupId(&it, r);
                ASSERT_EQ(DS_RESULT_OK, GetTestFieldNumber(&it, r, 0, &number));
                ASSERT_EQ(group >= 1000 ? 5.0 : (double)group, number);
                ASSERT_EQ(DS_RESULT_OK, GetTestFieldString(&it, r, 1, &string));
                ASSERT_STREQ("packed", string);
                ++visited;
            }
        }
        ASSERT_EQ(count, visited);
        DSQueryEnd(query);
        for (uint32_t i = 0; i < count; ++i)
            ASSERT_EQ(DS_RESULT_OK, DSRemoveBlob(instances[i]));
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
        it = DSQueryIter(query);
        ASSERT_FALSE(DSIterNext(&it));
        DSQueryEnd(query);
    }
    DSDestroyQuery(query);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    DSDestroyBlob(blob);
    delete[] bytes;
}

extern "C" int TestInlineDataFromC(HDSContext store);

TEST(Data, InlineCompositionQueriesAndRoundtrip)
{
    // Nested fixed layouts preserve C alignment and include variable data.
    DSFieldDesc  light_fields[] = { { .m_Field = 10, .m_Type = DS_TYPE_VECTOR3, .m_Offset = 0, .m_Name = "10" }, { .m_Field = 20, .m_Type = DS_TYPE_STRING, .m_Offset = 16, .m_Name = "20" }, { .m_Field = 30, .m_Type = DS_TYPE_LIST, .m_Offset = 24, .m_Name = "30" } };
    DSStructDesc light = { light_fields, 3, 32 };
    DSFieldDesc  wrapper_fields[] = { { .m_Field = 40, .m_Type = DS_TYPE_STRUCT, .m_Offset = 8, .m_Struct = &light, .m_Name = "40" } };
    DSStructDesc wrapper = { wrapper_fields, 1, 40 };
    DSFieldDesc  fields[] = { { .m_Field = 1, .m_Type = DS_TYPE_NUMBER, .m_Offset = 0, .m_Name = "1" }, { .m_Field = 2, .m_Type = DS_TYPE_STRUCT, .m_Offset = 8, .m_Struct = &wrapper, .m_Name = "2" }, { .m_Field = 3, .m_Type = DS_TYPE_BOOLEAN, .m_Offset = 48, .m_Name = "3" } };
    DSTableDesc  table = { 1, 0, 0, fields, 3, 56 };
    HDSContext   source = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(source, &table));
    // Changing a declaration after registration cannot alter the compiled layout.
    light_fields[0].m_Offset = 999;
    uint64_t          names[] = { 30, 10, 20 };
    DSValue           list_items[] = { Number(7), String("shared") };
    const DSValueType child_types[] = { DS_TYPE_LIST, DS_TYPE_VECTOR3, DS_TYPE_STRING };
    DSValueData       children[] = { List(list_items, 2).m_Value, Vector3(1, 2, 3).m_Value, String("shared").m_Value };
    DSValue           light_value = Struct(names, child_types, children, 3);
    uint64_t          light_name = 40;
    DSValue           values[] = { Number(5), Struct(&light_name, &light_value.m_Type, &light_value.m_Value, 1), Boolean(1) };
    DSId              original;
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, 1, 1, values, 3, &original));
    ASSERT_EQ(7u, source->m_Tables[0]->m_MetadataCount);
    ASSERT_EQ(32u, GetFieldMeta(source->m_Tables[0], 3).m_Size);

    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, 0, 0, &size));
    uint8_t* allocation = new uint8_t[size];
    uint8_t* bytes = allocation;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, bytes, size, &size));
    uint8_t* saved = new uint8_t[size];
    memcpy(saved, bytes, size);
    for (uint32_t packed = 0; packed < 2; ++packed)
    {
        HDSContext      store = packed ? DSCreateStore(0) : source;
        HDSBlobInstance instance = 0;
        if (packed)
            ASSERT_EQ(DS_RESULT_OK, Load(store, bytes, size, &instance));
        DSQueryItem fields[] = {
            { .m_Hash = dmHashString64("2.40.10"), .m_Type = DS_TYPE_VECTOR3, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD },
            { .m_Hash = dmHashString64("2.40.20"), .m_Type = DS_TYPE_STRING, .m_Kind = DS_QUERY_ITEM_FIELD },
            { .m_Hash = dmHashString64("2.40.30"), .m_Type = DS_TYPE_LIST, .m_Kind = DS_QUERY_ITEM_FIELD },
            { .m_Hash = 1, .m_Type = DS_TYPE_NUMBER, .m_Kind = DS_QUERY_ITEM_FIELD },
            { .m_Hash = 2, .m_Type = DS_TYPE_STRUCT, .m_Kind = DS_QUERY_ITEM_FIELD }
        };
        DSQueryDesc desc = { .m_Items = { fields[0], fields[1], fields[2], fields[3], fields[4] } };
        HDSQuery    query;
        ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
        DSIterator it = DSQueryIter(query);
        ASSERT_TRUE(DSIterNext(&it));
        DSId      id = DSIterGetId(&it, 0);
        DSVector3 color;
        ASSERT_EQ(DS_RESULT_OK, GetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(3.0f, color.m_Values[2]);
        const char* string;
        ASSERT_EQ(DS_RESULT_OK, GetTestFieldString(&it, 0, 1, &string));
        ASSERT_STREQ("shared", string);
        if (packed)
            ASSERT_TRUE((const uint8_t*)string >= bytes && (const uint8_t*)string < bytes + size);
        color.m_Values[0] = 42;
        ASSERT_EQ(DS_RESULT_OK, SetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(DS_RESULT_OK, GetTestFieldString(&it, 0, 1, &string));
        ASSERT_STREQ("shared", string); // Sibling references survive the first leaf override.
        DSValue list, child;
        ASSERT_EQ(DS_RESULT_OK, DSIterGetField(&it, 0, 2, &list));
        ASSERT_EQ(DS_RESULT_OK, DSGetListValue(&list.m_Value.m_List, 0, &child));
        ASSERT_EQ(7.0, child.m_Value.m_Number);
        DSQueryEnd(query);
        ASSERT_EQ(DS_RESULT_OK, DSSetFieldString(store, id, dmHashString64("2.40.20"), "changed"));
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3(store, id, dmHashString64("2.40.10"), &color));
        ASSERT_EQ(42.0f, color.m_Values[0]);
        // Persist a leaf override with untouched sibling references.
        uint8_t  DM_ALIGNED(8) changed_blob[1024];
        uint32_t changed_size;
        ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, changed_blob, sizeof(changed_blob), &changed_size));
        HDSContext      changed_store = DSCreateStore(0);
        HDSBlobInstance changed_instance;
        ASSERT_EQ(DS_RESULT_OK, Load(changed_store, changed_blob, changed_size, &changed_instance));
        DSQueryItem changed_fields[] = {
            { .m_Hash = dmHashString64("2.40.10"), .m_Type = DS_TYPE_VECTOR3, .m_Kind = DS_QUERY_ITEM_FIELD },
            { .m_Hash = dmHashString64("2.40.20"), .m_Type = DS_TYPE_STRING, .m_Kind = DS_QUERY_ITEM_FIELD }
        };
        DSQueryDesc changed_desc = { .m_Items = { changed_fields[0], changed_fields[1] } };
        HDSQuery    changed_query;
        ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(changed_store, &changed_desc, &changed_query));
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(changed_query));
        DSIterator changed_it = DSQueryIter(changed_query);
        ASSERT_TRUE(DSIterNext(&changed_it));
        ASSERT_EQ(DS_RESULT_OK, GetTestFieldVector3(&changed_it, 0, 0, &color));
        ASSERT_EQ(42.0f, color.m_Values[0]);
        ASSERT_EQ(DS_RESULT_OK, GetTestFieldString(&changed_it, 0, 1, &string));
        ASSERT_STREQ("changed", string);
        DSQueryEnd(changed_query);
        DSDestroyQuery(changed_query);
        ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(changed_store));
        ASSERT_EQ(DS_RESULT_OK, DSResetField(store, id, 2));
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3(store, id, dmHashString64("2.40.10"), &color));
        ASSERT_EQ(1.0f, color.m_Values[0]);
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetString(store, id, dmHashString64("2.40.20"), &string));
        ASSERT_STREQ("shared", string);
        // A write to another root must not change the reference mode of this struct.
        ASSERT_EQ(DS_RESULT_OK, DSSetFieldNumber(store, id, 1, 9));
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetString(store, id, dmHashString64("2.40.20"), &string));
        ASSERT_STREQ("shared", string);
        color.m_Values[0] = 42;
        ASSERT_EQ(DS_RESULT_OK, DSSetFieldVector3(store, id, dmHashString64("2.40.10"), &color));
        ASSERT_EQ(DS_RESULT_OK, DSSetFieldString(store, id, dmHashString64("2.40.20"), "changed"));
        ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, id));
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3(store, id, dmHashString64("2.40.10"), &color));
        ASSERT_EQ(1.0f, color.m_Values[0]);
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetString(store, id, dmHashString64("2.40.20"), &string));
        ASSERT_STREQ("shared", string);
        double number;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, id, 1, &number));
        ASSERT_EQ(5.0, number);
        ASSERT_EQ(DS_RESULT_OK, DSSetFieldVector3(store, id, dmHashString64("2.40.10"), &color));
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetString(store, id, dmHashString64("2.40.20"), &string));
        ASSERT_STREQ("shared", string); // Reusing the reset slot restores packed references correctly.
        if (packed)
            DSResetBlob(instance);
        else
            ASSERT_EQ(DS_RESULT_OK, DSResetTable(store, 1));
        ASSERT_EQ(0, TestInlineDataFromC(store));
        DSValue view, light_view;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, 2, &view));
        ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&view.m_Value.m_Struct, 40, &light_view));
        ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&light_view.m_Value.m_Struct, 10, &child));
        ASSERT_EQ(3.0f, child.m_Value.m_Vector3[2]);
        ASSERT_EQ(DS_RESULT_OK, DSSetField(store, id, 2, &view)); // Borrowed self-assignment.
        uint8_t* roundtrip = new uint8_t[size];
        uint32_t written;
        ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, roundtrip, size, &written));
        ASSERT_EQ(size, written);
        ASSERT_EQ(0, memcmp(saved, roundtrip, size));
        HDSBlob checked;
        ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(roundtrip, size, &checked));
        DSDestroyBlob(checked);
        delete[] roundtrip;
        ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, id));
        DSDestroyQuery(query);
        ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    }
    ASSERT_EQ(0, memcmp(saved, bytes, size));
    delete[] saved;
    delete[] allocation;
}

TEST(Data, InlineLayoutAndNameValidation)
{
    HDSContext   store = DSCreateStore(0);
    DSFieldDesc  members[] = { { .m_Field = 10, .m_Type = DS_TYPE_VECTOR3, .m_Offset = 0, .m_Name = "10" }, { .m_Field = 20, .m_Type = DS_TYPE_NUMBER, .m_Offset = 16, .m_Name = "20" } };
    DSStructDesc light = { members, 2, 24 };
    DSFieldDesc  fields[] = { { .m_Field = 1, .m_Type = DS_TYPE_STRUCT, .m_Offset = 0, .m_Struct = &light, .m_Name = "1" } };
    DSTableDesc  table = { 1, 0, 0, fields, 1, 24 };
    fields[0].m_Name = 0;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterTable(store, &table));
    fields[0].m_Name = "1";
    members[0].m_Name = "bad.name";
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterTable(store, &table));
    members[0].m_Name = "";
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterTable(store, &table));
    members[0].m_Name = "10";
    light.m_Size = 8;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterTable(store, &table));
    light.m_Size = 24;
    members[1].m_Offset = 4;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterTable(store, &table));
    members[1].m_Offset = 16;
    members[1].m_Field = 10;
    ASSERT_EQ(DS_RESULT_ALREADY_EXISTS, DSRegisterTable(store, &table));
    members[1].m_Field = 20;
    fields[0].m_Type = DS_TYPE_VECTOR3;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterTable(store, &table));
    fields[0].m_Type = DS_TYPE_STRUCT;
    DSStructDesc cycle = { fields, 1, 24 };
    fields[0].m_Struct = &cycle;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterTable(store, &table));
    fields[0].m_Struct = &light;
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    uint64_t    names[] = { 10, 20 };
    DSValueType child_types[] = { DS_TYPE_VECTOR3, DS_TYPE_NUMBER };
    DSValueData children[] = { { .m_Vector3 = { 1, 2, 3 } }, { .m_Number = 4 } };
    DSValue     value = Struct(names, child_types, children, 2);
    DSId        id = 0;
    child_types[1] = DS_TYPE_BOOLEAN;
    children[1].m_Boolean = 1;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, AddTestRow(store, 1, 0, &value, 1, &id));
    child_types[1] = DS_TYPE_NUMBER;
    children[1].m_Number = 4;
    names[1] = 99;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, AddTestRow(store, 1, 0, &value, 1, &id));
    names[1] = 20;
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(store, 1, 0, &value, 1, &id));
    child_types[1] = DS_TYPE_BOOLEAN;
    children[1].m_Boolean = 1;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSetField(store, id, 1, &value));
    DSQueryDesc  desc = { .m_Items = { { .m_Hash = dmHashString64("1.10"), .m_Type = DS_TYPE_VECTOR3, .m_Kind = DS_QUERY_ITEM_FIELD } } };
    DSQueryItem& field = desc.m_Items[0];
    HDSQuery     query;
    field.m_Type = (DSValueType)99;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSCreateQuery(store, &desc, &query));
    field.m_Type = DS_TYPE_VECTOR3;
    field.m_Hash = dmHashString64("1.99");
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSIterator it = DSQueryIter(query);
    ASSERT_FALSE(DSIterNext(&it));
    DSQueryEnd(query);
    DSDestroyQuery(query);
    field.m_Hash = dmHashString64("1.10");
    field.m_Type = DS_TYPE_NUMBER;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    it = DSQueryIter(query);
    ASSERT_FALSE(DSIterNext(&it));
    DSQueryEnd(query);
    DSDestroyQuery(query);

    uint8_t  DM_ALIGNED(8) bytes[1024], saved[1024];
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, bytes, sizeof(bytes), &size));
    memcpy(saved, bytes, size);
    HDSBlob        blob;
    const uint32_t metadata = 24 + DATA_TABLE_HEADER_SIZE;
    const uint32_t corrupt[] = { metadata + offsetof(DSFileFieldMeta, m_ChildIndex), metadata + offsetof(DSFileFieldMeta, m_ChildCount), metadata + offsetof(DSFileFieldMeta, m_ByteSize), metadata + DATA_FIELD_META_SIZE + offsetof(DSFileFieldMeta, m_ChildIndex), metadata + 2 * DATA_FIELD_META_SIZE + offsetof(DSFileFieldMeta, m_ByteOffset) };
    const uint32_t widths[] = { 2, 2, 4, 2, 4 };
    const uint32_t values[] = { 0, UINT32_MAX, 25, 1, 4 };
    for (uint32_t i = 0; i < 5; ++i)
    {
        WriteDataInteger(bytes + corrupt[i], values[i], widths[i]);
        ASSERT_EQ(DS_RESULT_INVALID_FORMAT, DSLoadBlob(bytes, size, &blob));
        memcpy(bytes, saved, size);
    }
    ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
    DSDestroyBlob(blob);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, InlineNamesBindDifferentTablesAndTrackRegistration)
{
    HDSContext  store = DSCreateStore(0);
    DSQueryItem field = { .m_Hash = dmHashString64("1.10"), .m_Type = DS_TYPE_VECTOR3, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryDesc query_desc = { .m_Items = { field } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &query_desc, &query));
    DSFieldDesc  members[] = { { .m_Field = 10, .m_Type = DS_TYPE_VECTOR3, .m_Offset = 0, .m_Name = "10" }, { .m_Field = 20, .m_Type = DS_TYPE_NUMBER, .m_Offset = 16, .m_Name = "20" } };
    DSStructDesc light = { members, 2, 24 };
    uint64_t     names[] = { 10, 20 };
    DSValueType  child_types[] = { DS_TYPE_VECTOR3, DS_TYPE_NUMBER };
    DSValueData  children[] = { { .m_Vector3 = { 1, 2, 3 } }, { .m_Number = 4 } };
    DSValue      value = Struct(names, child_types, children, 2);
    DSId         ids[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        DSFieldDesc root = { .m_Field = 1, .m_Type = DS_TYPE_STRUCT, .m_Offset = i * 8, .m_Struct = i < 2 ? &light : 0, .m_Name = "1" };
        DSTableDesc table = { i + 1, 0, 0, &root, 1, i * 8 + (i < 2 ? 24u : 8u) };
        ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
        ASSERT_EQ(DS_RESULT_OK, AddTestRow(store, i + 1, i, &value, 1, &ids[i]));
    }
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSIterator it = DSQueryIter(query);
    uint32_t   visited = 0;
    while (DSIterNext(&it))
    {
        ASSERT_TRUE(DSIterGetType(&it) < 3); // Dynamic structs cannot promise a member layout.
        DSVector3 color;
        ASSERT_EQ(DS_RESULT_OK, GetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(1.0f, color.m_Values[0]);
        color.m_Values[0] = 8;
        ASSERT_EQ(DS_RESULT_OK, SetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(DS_RESULT_OK, GetTestFieldVector3(&it, 0, 0, &color));
        ASSERT_EQ(8.0f, color.m_Values[0]);
        ++visited;
    }
    ASSERT_EQ(2u, visited);
    DSQueryEnd(query);
    DSValue object, intensity;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[0], 1, &object));
    ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&object.m_Value.m_Struct, 20, &intensity));
    ASSERT_EQ(4.0, intensity.m_Value.m_Number);
    ASSERT_EQ(DS_RESULT_OK, DSResetTable(store, 1));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    it = DSQueryIter(query);
    ASSERT_TRUE(DSIterNext(&it));
    DSVector3 color;
    ASSERT_EQ(DS_RESULT_OK, GetTestFieldVector3(&it, 0, 0, &color));
    ASSERT_EQ(1.0f, color.m_Values[0]);
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterTable(store, 1));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    it = DSQueryIter(query);
    ASSERT_TRUE(DSIterNext(&it));
    ASSERT_EQ((uint64_t)2, DSIterGetType(&it));
    ASSERT_FALSE(DSIterNext(&it));
    DSQueryEnd(query);
    DSDestroyQuery(query);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, ResetOneComponentPreservesOtherRowsAndReusesStorage)
{
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc fields[10] = {};
    DSValue     values[10];
    for (uint32_t p = 0; p < 10; ++p)
    {
        fields[p].m_Field = p;
        fields[p].m_Type = DS_TYPE_NUMBER;
        fields[p].m_Offset = p * 8;
        values[p] = Number(p);
    }
    DSTableDesc table = { 1, 0, 0, fields, 10, 80 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    DSId ids[3];
    for (uint32_t r = 0; r < 3; ++r)
        ASSERT_EQ(DS_RESULT_OK, AddTestRow(store, 1, r == 2 ? 8 : 7, values, 10, &ids[r], r + 11));
    DSMemoryStats before, after;
    GetDataMemoryStats(store, &before);
    ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, ids[0]));
    GetDataMemoryStats(store, &after);
    ASSERT_EQ(before.m_TotalBytes, after.m_TotalBytes);

    for (uint32_t r = 0; r < 3; ++r)
        for (uint32_t p = 0; p < 10; ++p)
            ASSERT_EQ(DS_RESULT_OK, DSSetFieldNumber(store, ids[r], p, 100 * (r + 1) + p));
    DSGroupId   group = 7;
    DSQueryItem field = { .m_Hash = 9, .m_Type = DS_TYPE_NUMBER, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryDesc desc = { .m_GroupIds = &group, .m_GroupIdCount = 1, .m_Items = { field } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, ids[0]));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSIterator it = DSQueryIter(query);
    ASSERT_TRUE(DSIterNext(&it));
    ASSERT_EQ(2u, DSIterGetCount(&it));
    double number;
    ASSERT_EQ(DS_RESULT_OK, GetTestFieldNumber(&it, 0, 0, &number));
    ASSERT_EQ(9.0, number);
    ASSERT_EQ(DS_RESULT_OK, GetTestFieldNumber(&it, 1, 0, &number));
    ASSERT_EQ(209.0, number);

    ASSERT_FALSE(DSIterNext(&it));
    DSQueryEnd(query);
    for (uint32_t r = 0; r < 3; ++r)
    {
        ASSERT_EQ((uint64_t)(r + 11), DSGetComponentId(store, ids[r]));
        for (uint32_t p = 0; p < 10; ++p)
        {
            ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, ids[r], p, &number));
            ASSERT_EQ((double)(r ? 100 * (r + 1) + p : p), number);
        }
    }
    GetDataMemoryStats(store, &before);
    for (uint32_t cycle = 0; cycle < 16; ++cycle)
    {
        ASSERT_EQ(DS_RESULT_OK, DSSetFieldNumber(store, ids[0], 9, 500));
        ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, ids[0]));
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, ids[0], 9, &number));
        ASSERT_EQ(9.0, number);
    }
    GetDataMemoryStats(store, &after);
    ASSERT_EQ(before.m_TotalBytes, after.m_TotalBytes);
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSResetRow(store, 0));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[0]));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSResetRow(store, ids[0]));
    ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, ids[2])); // Swap removal moved this row.
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, ids[2], 9, &number));
    ASSERT_EQ(9.0, number);
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, ids[1], 9, &number));
    ASSERT_EQ(209.0, number);
    DSDestroyQuery(query);

    DSTableDesc empty = { 2, 0, 0, 0, 0, 0 };
    DSId        empty_id;
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &empty));
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(store, 2, 7, 0, 0, &empty_id));
    ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, empty_id));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSResetRow(store, ids[0]));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, RowIterationWithoutFieldFilters)
{
    HDSContext  source = DSCreateStore(0);
    DSFieldDesc meta[] = { { 10, DS_TYPE_NUMBER, 0 }, { 20, DS_TYPE_STRING, 8 }, { 30, DS_TYPE_VECTOR3, 16 }, { 40, DS_TYPE_NULL, 0 } };
    DSTableDesc table = { 1, 0, 0, meta, 4, 32 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(source, &table));
    DSValue values[] = { Number(10), String("original"), Vector3(1, 2, 3), Null() };
    DSId    id;
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, 1, 7, values, 4, &id));
    values[0] = Number(20);
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, 1, 8, values, 4, &id));
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, bytes, size, &size));
    HDSContext      loaded = DSCreateStore(0);
    HDSBlobInstance instance;
    ASSERT_EQ(DS_RESULT_OK, Load(loaded, bytes, size, &instance, 9));
    HDSContext stores[] = { source, loaded };
    for (uint32_t mode = 0; mode < 2; ++mode)
    {
        HDSContext  store = stores[mode];
        DSQueryDesc desc = {};
        HDSQuery    query;
        ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
        DSIterator    batch = DSQueryIter(query);
        DSRowIterator empty_rows = DSIterRows(&batch);
        ASSERT_FALSE(DSRowIterNext(&empty_rows));
        ASSERT_EQ((DSId)0, DSRowIterGetId(&empty_rows));
        ASSERT_TRUE(DSIterNext(&batch));
        DSRowIterator rows = DSIterRows(&batch);
        uint32_t      visited = 0;
        while (DSRowIterNext(&rows))
        {
            id = DSRowIterGetId(&rows);
            ASSERT_NE((DSId)0, id);
            ASSERT_EQ((DSGroupId)(mode ? 9 : 7 + visited), DSRowIterGetGroupId(&rows));
            double number;
            ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, id, 10, &number));
            ASSERT_EQ(10.0 + 10.0 * visited, number);
            ++visited;
        }
        ASSERT_EQ(2u, visited);
        ASSERT_FALSE(DSRowIterNext(&rows));
        ASSERT_EQ((DSId)0, DSRowIterGetId(&rows));
        ASSERT_FALSE(DSIterNext(&batch));
        DSQueryEnd(query);
        DSDestroyQuery(query);
    }
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(loaded));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(source));
    delete[] bytes;
}

TEST(Data, RowAccessUsesBatchRelativeRowsAfterRemoval)
{
    HDSContext store = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, Register(store, 1));
    DSId ids[6];
    for (uint32_t row = 0; row < 6; ++row)
        ASSERT_EQ(DS_RESULT_OK, Add(store, 1, row % 2, row + 10, "base", &ids[row]));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[1])); // Last row moves; its base bytes do not.
    DSGroupId   group = 1;
    DSQueryItem field = { .m_Hash = 10, .m_Type = DS_TYPE_NUMBER, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD };
    for (uint32_t requested = 0; requested < 2; ++requested)
    {
        DSQueryDesc desc = { .m_GroupIds = &group, .m_GroupIdCount = 1 };
        if (requested)
            desc.m_Items[0] = field;
        HDSQuery query;
        ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
        uint32_t handle = DSQueryFindField(query, &field);
        ASSERT_EQ(requested != 0, handle != UINT32_MAX);
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
        DSIterator batch = DSQueryIter(query);
        uint32_t   visited = 0;
        while (DSIterNext(&batch))
        {
            DSRowIterator rows = DSIterRows(&batch);
            while (DSRowIterNext(&rows))
            {
                DSId id = DSRowIterGetId(&rows);
                ASSERT_TRUE(id == ids[3] || id == ids[5]);
                ASSERT_EQ(group, DSRowIterGetGroupId(&rows));
                if (requested)
                {
                    ASSERT_EQ(id == ids[3] ? 13.0 : 15.0, *DSRowIterGetNumber(&rows, handle));
                    *DSRowIterGetNumberMut(&rows, handle) = 99;
                    ASSERT_EQ(99.0, *DSRowIterGetNumber(&rows, handle));
                }
                ++visited;
            }
        }
        ASSERT_EQ(2u, visited);
        DSQueryEnd(query);
        ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, ids[3]));
        ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, ids[5]));
        DSDestroyQuery(query);
    }
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, DeferredRemovalUsesStableIds)
{
    HDSContext store = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, Register(store, 1));
    ASSERT_EQ(DS_RESULT_OK, Register(store, 2));
    DSQueryItem field = { .m_Hash = 10, .m_Type = DS_TYPE_NUMBER, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryDesc desc = { .m_Items = { field } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    uint32_t      handle = DSQueryFindField(query, &field);
    dmArray<DSId> removals;
    removals.SetCapacity(64);
    DSId ids[64];
    // Reuse both the caller's deletion buffer and released store ID slots.
    for (uint32_t cycle = 0; cycle < 2; ++cycle)
    {
        for (uint32_t r = 0; r < 64; ++r)
            ASSERT_EQ(DS_RESULT_OK, Add(store, 1 + r / 32, r, r, "base", &ids[r]));
        removals.SetSize(0);
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
        DSIterator batch = DSQueryIter(query);
        uint32_t   visited = 0;
        while (DSIterNext(&batch))
        {
            DSRowIterator rows = DSIterRows(&batch);
            while (DSRowIterNext(&rows))
            {
                DSGroupId group = DSRowIterGetGroupId(&rows);
                ASSERT_EQ(ids[group], DSRowIterGetId(&rows));
                ASSERT_EQ((double)group, *DSRowIterGetNumber(&rows, handle));
                if (group % 2 == 0 || group == 63)
                    removals.Push(DSRowIterGetId(&rows));
                ++visited;
            }
        }
        ASSERT_EQ(64u, visited);
        ASSERT_EQ(33u, removals.Size());
        // Queued IDs are still readable before the caller applies its buffer.
        double value;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, removals[0], 10, &value));
        DSQueryEnd(query);
        for (uint32_t r = 0; r < removals.Size(); ++r)
            ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, removals[r]));
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
        batch = DSQueryIter(query);
        visited = 0;
        while (DSIterNext(&batch))
        {
            DSRowIterator rows = DSIterRows(&batch);
            while (DSRowIterNext(&rows))
            {
                DSGroupId group = DSRowIterGetGroupId(&rows);
                ASSERT_TRUE(group % 2 == 1 && group != 63);
                ASSERT_EQ(ids[group], DSRowIterGetId(&rows));
                ++visited;
            }
        }
        ASSERT_EQ(31u, visited);
        DSQueryEnd(query);
        for (uint32_t r = 0; r < 64; ++r)
        {
            if (r % 2 == 0 || r == 63)
                ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGetNumber(store, ids[r], 10, &value));
            else
                ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[r]));
        }
        ASSERT_EQ(64u, removals.Capacity());
    }
    DSDestroyQuery(query);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, EmptyRowHasNoFields)
{
    HDSContext  store = DSCreateStore(0);
    DSTableDesc table = { 1, 0, 0, 0, 0, 0 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    DSId id;
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(store, 1, 7, 0, 0, &id));
    DSQueryDesc desc = {};
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSIterator batch = DSQueryIter(query);
    ASSERT_TRUE(DSIterNext(&batch));
    DSRowIterator rows = DSIterRows(&batch);
    ASSERT_TRUE(DSRowIterNext(&rows));
    ASSERT_EQ(id, DSRowIterGetId(&rows));
    ASSERT_FALSE(DSRowIterNext(&rows));
    ASSERT_FALSE(DSIterNext(&batch));
    DSQueryEnd(query);
    DSDestroyQuery(query);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, AlignedLayoutValidation)
{
    HDSContext   store = DSCreateStore(0);
    DSFieldDesc  members[] = { { .m_Field = 1, .m_Type = DS_TYPE_NUMBER, .m_Offset = 0, .m_Name = "1" }, { .m_Field = 2, .m_Type = DS_TYPE_BOOLEAN, .m_Offset = 8, .m_Name = "2" } };
    DSStructDesc inner = { members, 2, 16 };
    DSFieldDesc  root = { .m_Field = 3, .m_Type = DS_TYPE_STRUCT, .m_Offset = 0, .m_Struct = &inner, .m_Name = "3" };
    DSTableDesc  table = { 1, 0, 0, &root, 1, 24 };
    members[0].m_Offset = 1;
    members[1].m_Offset = 9;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterTable(store, &table));
    members[0].m_Offset = 0;
    members[1].m_Offset = 8;
    inner.m_Size = 9;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterTable(store, &table));
    inner.m_Size = 16;
    root.m_Offset = 4;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterTable(store, &table));
    root.m_Offset = 8;
    table.m_RowStride = 25;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterTable(store, &table));
    table.m_RowStride = 24;
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    uint64_t          names[] = { 1, 2 };
    const DSValueType child_types[] = { DS_TYPE_NUMBER, DS_TYPE_BOOLEAN };
    DSValueData       children[] = { { .m_Number = 17 }, { .m_Boolean = 1 } };
    DSValue           value = Struct(names, child_types, children, 2);
    DSId              id;
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(store, 1, 0, &value, 1, &id));
    uint8_t  DM_ALIGNED(8) bytes[512], saved[512];
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, bytes, sizeof(bytes), &size));
    memcpy(saved, bytes, size);
    HDSBlob blob;
    ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
    // Reject unaligned input once, before any direct pointer can be obtained.
    uint8_t* unaligned = new uint8_t[size + 1];
    memcpy(unaligned + 1, bytes, size);
    HDSBlob output = blob;
    ASSERT_EQ(DS_RESULT_INVALID_FORMAT, DSLoadBlob(unaligned + 1, size, &output));
    ASSERT_EQ((uintptr_t)blob, (uintptr_t)output);
    delete[] unaligned;
    uint32_t       table_offset = (uint32_t)ReadDataInteger(bytes + 16, 4);
    uint32_t       metadata = table_offset + DATA_TABLE_HEADER_SIZE;
    const size_t   offsets[] = { 4, table_offset + offsetof(DSTableHeader, m_RowStride), metadata + offsetof(DSFileFieldMeta, m_ByteOffset), metadata + offsetof(DSFileFieldMeta, m_ByteSize), metadata + DATA_FIELD_META_SIZE + offsetof(DSFileFieldMeta, m_ByteOffset) };
    const uint32_t values[] = { 7, 25, 4, 9, 1 };
    for (uint32_t i = 0; i < 5; ++i)
    {
        WriteDataInteger(bytes + offsets[i], values[i], 4);
        ASSERT_EQ(DS_RESULT_INVALID_FORMAT, DSLoadBlob(bytes, size, &output));
        ASSERT_EQ((uintptr_t)blob, (uintptr_t)output);
        memcpy(bytes, saved, size);
    }
    DSDestroyBlob(blob);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

extern "C" int TestDataPointersFromC(HDSContext store, int write);

TEST(Data, NativeFieldPointersAndReset)
{
    struct NativeRow
    {
        double    m_Number;
        uint8_t   m_Boolean;
        DSVector3 m_Vector3;
        DSVector4 m_Vector4;
        DSMatrix4 m_Matrix4;
    };
    DSFieldDesc fields[] = {
        { 1, DS_TYPE_NUMBER, offsetof(NativeRow, m_Number) },
        { 2, DS_TYPE_BOOLEAN, offsetof(NativeRow, m_Boolean) },
        { 3, DS_TYPE_VECTOR3, offsetof(NativeRow, m_Vector3) },
        { 4, DS_TYPE_VECTOR4, offsetof(NativeRow, m_Vector4) },
        { 5, DS_TYPE_MATRIX4, offsetof(NativeRow, m_Matrix4) }
    };
    DSTableDesc table = { 1, 0, 0, fields, 5, sizeof(NativeRow) };
    HDSContext  source = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(source, &table));
    DSValue values[] = { Number(17), Boolean(0), Vector3(1, 2, 3), {}, Matrix() };
    values[3].m_Type = DS_TYPE_VECTOR4;
    values[3].m_Value.m_Vector4[3] = 4;
    for (uint32_t i = 0; i < 16; ++i)
        values[4].m_Value.m_Matrix4[i] = (float)i;
    DSId ids[3];
    for (uint32_t i = 0; i < 3; ++i)
        ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, 1, i, values, 5, &ids[i]));
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    uint8_t* saved = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, bytes, size, &size));
    memcpy(saved, bytes, size);
    HDSContext      loaded = DSCreateStore(0);
    HDSBlobInstance instance;
    ASSERT_EQ(DS_RESULT_OK, Load(loaded, bytes, size, &instance));
    HDSContext stores[] = { source, loaded };
    for (uint32_t packed = 0; packed < 2; ++packed)
    {
        HDSContext store = stores[packed];
        ASSERT_EQ(0, TestDataPointersFromC(store, 0));
        DSMemoryStats before, after;
        GetDataMemoryStats(store, &before);
        ASSERT_GT(before.m_ValueBytes, (uint64_t)0);
        ASSERT_EQ((uint64_t)0, before.m_PayloadBlocks);
        ASSERT_EQ(0, TestDataPointersFromC(store, 1));
        GetDataMemoryStats(store, &after);
        ASSERT_EQ(before.m_ValueBytes, after.m_ValueBytes);
        ASSERT_EQ(before.m_TotalBytes, after.m_TotalBytes);
        ASSERT_EQ((uint64_t)0, after.m_PayloadBlocks);
        DSId id = FirstId(store);
        ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, id));
        double number;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, id, 1, &number));
        ASSERT_EQ(17.0, number);
        if (packed)
            DSResetBlob(instance);
        else
            ASSERT_EQ(DS_RESULT_OK, DSResetTable(store, 1));
        ASSERT_EQ(0, TestDataPointersFromC(store, 0));
        ASSERT_EQ(0, memcmp(saved, bytes, size));
    }
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(loaded));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(source));
    delete[] saved;
    delete[] bytes;
}

TEST(Data, QueryFieldHandlesSurviveTableChanges)
{
    HDSContext  store = DSCreateStore(0);
    DSQueryItem fields[] = {
        { .m_Hash = 30, .m_Type = DS_TYPE_VECTOR3, .m_Kind = DS_QUERY_ITEM_FIELD },
        { .m_Hash = 10, .m_Type = DS_TYPE_NUMBER, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD }
    };
    DSQueryDesc desc = { .m_Items = { fields[0], fields[1] } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    uint32_t vector_field = DSQueryFindField(query, &fields[0]);
    uint32_t number_field = DSQueryFindField(query, &fields[1]);
    ASSERT_NE(UINT32_MAX, vector_field);
    ASSERT_NE(UINT32_MAX, number_field);
    ASSERT_NE(vector_field, number_field);
    DSQueryItem invalid = { .m_Hash = 99, .m_Type = DS_TYPE_VECTOR3, .m_Kind = DS_QUERY_ITEM_FIELD };
    ASSERT_EQ(UINT32_MAX, DSQueryFindField(query, &invalid));
    invalid.m_Hash = 30;
    invalid.m_Type = DS_TYPE_NUMBER;
    ASSERT_EQ(UINT32_MAX, DSQueryFindField(query, &invalid));

    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSIterator empty = DSQueryIter(query);
    ASSERT_FALSE(DSIterNext(&empty));
    DSQueryEnd(query);

    // Reuse the handles after END and across registration/removal with new offsets.
    for (uint32_t pass = 0; pass < 3; ++pass)
    {
        DSFieldDesc meta[] = { { 10, DS_TYPE_NUMBER, pass * 8 }, { 30, DS_TYPE_VECTOR3, pass * 8 + 8 } };
        DSTableDesc table = { 1, 0, 0, meta, 2, pass * 8 + 24 };
        ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
        DSValue values[] = { Number(10 + pass), Vector3(1, 2, 3) };
        DSId    id;
        ASSERT_EQ(DS_RESULT_OK, AddTestRow(store, 1, 0, values, 2, &id));
        ASSERT_EQ(vector_field, DSQueryFindField(query, &fields[0]));
        ASSERT_EQ(number_field, DSQueryFindField(query, &fields[1]));
        for (uint32_t traversal = 0; traversal < 2; ++traversal)
        {
            ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
            DSIterator it = DSQueryIter(query);
            ASSERT_TRUE(DSIterNext(&it));
            DSRowIterator rows = DSIterRows(&it);
            ASSERT_TRUE(DSRowIterNext(&rows));
            ASSERT_EQ(id, DSRowIterGetId(&rows));
            ASSERT_EQ(10.0 + pass, *DSRowIterGetNumber(&rows, number_field));
            ASSERT_EQ(3.0f, DSRowIterGetVector3(&rows, vector_field)->m_Values[2]);
            *DSRowIterGetNumberMut(&rows, number_field) = 99;
            ASSERT_EQ(99.0, *DSRowIterGetNumber(&rows, number_field));
            ASSERT_FALSE(DSRowIterNext(&rows));
            ASSERT_FALSE(DSIterNext(&it));
            DSQueryEnd(query);
            ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, id));
        }
        ASSERT_EQ(DS_RESULT_OK, DSUnregisterTable(store, 1));
    }
    DSDestroyQuery(query);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, NestedFieldPointersSharePayloadsAndPreserveSiblings)
{
    DSFieldDesc       members[] = { { .m_Field = 10, .m_Type = DS_TYPE_VECTOR3, .m_Offset = 0, .m_Name = "10" }, { .m_Field = 20, .m_Type = DS_TYPE_NUMBER, .m_Offset = 16, .m_Name = "20" }, { .m_Field = 30, .m_Type = DS_TYPE_STRING, .m_Offset = 24, .m_Name = "30" } };
    DSStructDesc      light = { members, 3, 32 };
    uint64_t          names[] = { 10, 20, 30 };
    const DSValueType types[] = { DS_TYPE_VECTOR3, DS_TYPE_NUMBER, DS_TYPE_STRING };
    DSValueData       first[] = { { .m_Vector3 = { 1, 2, 3 } }, { .m_Number = 7 }, { .m_String = "shared" } };
    DSValueData       second[] = { { .m_Vector3 = { 9, 8, 7 } }, { .m_Number = 6 }, { .m_String = "other" } };
    DSValue           values[] = { Struct(names, types, first, 3), Struct(names, types, second, 3) };
    HDSContext        source = DSCreateStore(0);
    for (uint32_t i = 0; i < 2; ++i)
    {
        DSFieldDesc roots[] = { { .m_Field = 1, .m_Type = DS_TYPE_STRUCT, .m_Offset = i * 8, .m_Struct = &light, .m_Name = "1" }, { .m_Field = 2, .m_Type = DS_TYPE_STRUCT, .m_Offset = i * 8 + 32, .m_Struct = &light, .m_Name = "2" } };
        DSTableDesc table = { i + 1, 0, 0, roots, 2, i * 8 + 64 };
        ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(source, &table));
        DSId id;
        ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, i + 1, i, values, 2, &id));
    }
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    uint8_t* saved = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, bytes, size, &size));
    memcpy(saved, bytes, size);
    HDSContext      loaded = DSCreateStore(0);
    HDSBlobInstance instance;
    ASSERT_EQ(DS_RESULT_OK, Load(loaded, bytes, size, &instance));
    HDSContext stores[] = { source, loaded };
    for (uint32_t packed = 0; packed < 2; ++packed)
    {
        HDSContext  store = stores[packed];
        DSQueryItem fields[] = {
            { .m_Hash = dmHashString64("2.10"), .m_Type = DS_TYPE_VECTOR3, .m_Kind = DS_QUERY_ITEM_FIELD },
            { .m_Hash = dmHashString64("1.10"), .m_Type = DS_TYPE_VECTOR3, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD }
        };
        DSQueryDesc desc = { .m_Items = { fields[0], fields[1] } };
        HDSQuery    query;
        ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
        // Lookup uses full-name hashes even when the descriptors change order.
        DSQueryItem swap = fields[0];
        fields[0] = fields[1];
        fields[1] = swap;
        uint32_t field = DSQueryFindField(query, &fields[0]);
        uint32_t other = DSQueryFindField(query, &fields[1]);
        ASSERT_NE(UINT32_MAX, field);
        ASSERT_NE(UINT32_MAX, other);
        ASSERT_NE(field, other);
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
        DSIterator it = DSQueryIter(query);
        uint32_t   batches = 0;
        DSId       ids[2];
        while (DSIterNext(&it))
        {
            DSRowIterator rows = DSIterRows(&it);
            ASSERT_TRUE(DSRowIterNext(&rows));
            DSId             id = DSRowIterGetId(&rows);
            const DSVector3* original = DSRowIterGetVector3(&rows, field);
            ASSERT_EQ(1.0f, original->m_Values[0]);
            ASSERT_EQ(9.0f, DSRowIterGetVector3(&rows, other)->m_Values[0]);
            if (packed)
            {
                uint32_t       table_index = (uint32_t)DSIterGetType(&it) - 1;
                const DSTable* table = GetInstanceTable(instance, table_index);
                ASSERT_EQ((uintptr_t)(GetRowBytes(table, 0) + table_index * 8), (uintptr_t)original);
            }
            ids[batches] = id;
            DSRowIterGetVector3Mut(&rows, field)->m_Values[0] = 42;
            ASSERT_EQ(42.0f, DSRowIterGetVector3(&rows, field)->m_Values[0]);
            ASSERT_EQ(9.0f, DSRowIterGetVector3(&rows, other)->m_Values[0]);
            DSMemoryStats before, after;
            GetDataMemoryStats(store, &before);
            DSRowIterGetVector3Mut(&rows, field)->m_Values[1] = 43;
            GetDataMemoryStats(store, &after);
            ASSERT_EQ(before.m_PayloadUsed, after.m_PayloadUsed);
            ASSERT_EQ(before.m_PayloadBlocks, after.m_PayloadBlocks);
            DSRowIterGetVector3Mut(&rows, field)->m_Values[2] = 44;
            ++batches;
        }
        ASSERT_EQ(2u, batches);
        DSQueryEnd(query);
        for (uint32_t r = 0; r < 2; ++r)
        {
            DSId    id = ids[r];
            DSValue root, child;
            ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, 1, &root));
            ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&root.m_Value.m_Struct, 20, &child));
            ASSERT_EQ(7.0, child.m_Value.m_Number);
            ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&root.m_Value.m_Struct, 30, &child));
            ASSERT_STREQ("shared", child.m_Value.m_String);
            ASSERT_EQ(DS_RESULT_OK, DSResetField(store, id, 1));
            // The same full-name hash works through ID access, including packed metadata.
            DSVector3 copy;
            uint64_t  color_hash = dmHashString64("1.10");
            ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3(store, id, color_hash, &copy));
            ASSERT_EQ(1.0f, copy.m_Values[0]);
            ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3Batch(store, 1, &id, color_hash, &copy));
            copy.m_Values[0] = 17;
            ASSERT_EQ(DS_RESULT_OK, DSSetFieldVector3(store, id, color_hash, &copy));
            ASSERT_EQ(DS_RESULT_OK, DSResetField(store, id, color_hash));
            ASSERT_EQ(DS_RESULT_OK, DSSetFieldString(store, id, dmHashString64("1.30"), "changed"));
            ASSERT_EQ(DS_RESULT_OK, DSResetField(store, id, dmHashString64("1.30")));
            ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, id));
            ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3(store, id, color_hash, &copy));
            ASSERT_EQ(2.0f, copy.m_Values[1]);
            ASSERT_EQ(3.0f, copy.m_Values[2]);
        }
        DSDestroyQuery(query);
    }
    ASSERT_EQ(0, memcmp(saved, bytes, size));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(loaded));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(source));
    delete[] saved;
    delete[] bytes;
}

TEST(Data, MutableInlineReferencesAndInstanceResetAfterRemoval)
{
    DSFieldDesc  members[] = { { .m_Field = 10, .m_Type = DS_TYPE_NUMBER, .m_Offset = 0, .m_Name = "10" }, { .m_Field = 20, .m_Type = DS_TYPE_STRING, .m_Offset = 8, .m_Name = "20" }, { .m_Field = 30, .m_Type = DS_TYPE_LIST, .m_Offset = 16, .m_Name = "30" } };
    DSStructDesc layout = { members, 3, 24 };
    DSFieldDesc  root = { .m_Field = 1, .m_Type = DS_TYPE_STRUCT, .m_Offset = 0, .m_Struct = &layout, .m_Name = "1" };
    DSTableDesc  table = { 1, 0, 0, &root, 1, 24 };
    HDSContext   source = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(source, &table));
    uint64_t          names[] = { 10, 20, 30 };
    DSValue           list_values[] = { String("nested"), Number(7) };
    const DSValueType types[] = { DS_TYPE_NUMBER, DS_TYPE_STRING, DS_TYPE_LIST };
    DSValueData       values[] = { Number(0).m_Value, String("shared").m_Value, List(list_values, 2).m_Value };
    DSValue           object = Struct(names, types, values, 3);
    for (uint32_t r = 0; r < 3; ++r)
    {
        values[0].m_Number = 10 + r;
        DSId id;
        ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, 1, 0, &object, 1, &id, r));
    }
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    uint8_t* saved = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, bytes, size, &size));
    memcpy(saved, bytes, size);
    HDSBlob blob;
    ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
    HDSContext      store = DSCreateStore(0);
    HDSBlobInstance first, second;
    ASSERT_EQ(DS_RESULT_OK, DSAddBlob(store, blob, 1, &first));
    ASSERT_EQ(DS_RESULT_OK, DSAddBlob(store, blob, 2, &second));
    DSDestroyBlob(blob);
    DSQueryItem fields[] = {
        { .m_Hash = dmHashString64("1.10"), .m_Type = DS_TYPE_NUMBER, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD },
        { .m_Hash = dmHashString64("1.20"), .m_Type = DS_TYPE_STRING, .m_Kind = DS_QUERY_ITEM_FIELD }
    };
    DSQueryDesc desc = { .m_Items = { fields[0], fields[1] } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    uint32_t number_field = DSQueryFindField(query, &fields[0]);
    ASSERT_NE(UINT32_MAX, number_field);
    DSId        ids[2][3] = {};
    const char* shared_list_string = 0;
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSIterator it = DSQueryIter(query);
    while (DSIterNext(&it))
    {
        DSRowIterator rows = DSIterRows(&it);
        while (DSRowIterNext(&rows))
        {
            DSId     id = DSRowIterGetId(&rows);
            uint32_t group = (uint32_t)DSRowIterGetGroupId(&rows) - 1;
            uint32_t component = (uint32_t)DSGetComponentId(store, id);
            ids[group][component] = id;
            ASSERT_EQ(10.0 + component, *DSRowIterGetNumber(&rows, number_field));
            ASSERT_EQ((uintptr_t)DSRowIterGetNumber(&rows, number_field), (uintptr_t)DSRowIterGetNumberMut(&rows, number_field));
            if (!group)
                *DSRowIterGetNumberMut(&rows, number_field) += 100;
            DSValue current, child, element;
            ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, 1, &current));
            ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&current.m_Value.m_Struct, 20, &child));
            ASSERT_STREQ("shared", child.m_Value.m_String);
            ASSERT_TRUE((uintptr_t)child.m_Value.m_String >= (uintptr_t)bytes && (uintptr_t)child.m_Value.m_String < (uintptr_t)(bytes + size));
            ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, 1, &current));
            ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&current.m_Value.m_Struct, 30, &child));
            ASSERT_EQ(DS_RESULT_OK, DSGetListValue(&child.m_Value.m_List, 0, &element));
            ASSERT_STREQ("nested", element.m_Value.m_String);
            if (!shared_list_string)
                shared_list_string = element.m_Value.m_String;
            ASSERT_EQ((uintptr_t)shared_list_string, (uintptr_t)element.m_Value.m_String);
        }
    }
    DSQueryEnd(query);
    for (uint32_t component = 0; component < 3; ++component)
        ASSERT_EQ(DS_RESULT_OK, DSSetFieldString(store, ids[0][component], dmHashString64("1.20"), "replacement"));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[0][0]));
    DSResetBlob(first);
    ASSERT_EQ((uintptr_t)0, (uintptr_t)first->m_Payloads);
    for (uint32_t group = 0; group < 2; ++group)
        for (uint32_t component = group ? 0 : 1; component < 3; ++component)
        {
            DSValue current, child;
            ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[group][component], 1, &current));
            ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&current.m_Value.m_Struct, 10, &child));
            ASSERT_EQ(10.0 + component, child.m_Value.m_Number);
            ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&current.m_Value.m_Struct, 20, &child));
            ASSERT_STREQ("shared", child.m_Value.m_String);
        }
    ASSERT_EQ(0, memcmp(saved, bytes, size));
    DSDestroyQuery(query);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(source));
    delete[] saved;
    delete[] bytes;
}

TEST(Data, PooledRegistrationResetAndRemovalAfterSlotReuse)
{
    HDSContext source = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, Register(source, 1));
    DSTableDesc empty = { 2, 0, 0, 0, 0, 0 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(source, &empty));
    DSValue values[] = { Number(10), String("shared") };
    DSId    id;
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, 1, 0, values, 2, &id, 11));
    values[0] = Number(20);
    ASSERT_EQ(DS_RESULT_OK, AddTestRow(source, 1, 0, values, 2, &id, 12));
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, bytes, size, &size));
    HDSBlob blob;
    ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
    HDSContext      store = DSCreateStore(0);
    HDSBlobInstance first, second;
    ASSERT_EQ(DS_RESULT_OK, DSAddBlob(store, blob, 77, &first));
    ASSERT_EQ(DS_RESULT_OK, DSAddBlob(store, blob, 77, &second));
    DSDestroyBlob(blob);
    ASSERT_EQ((uintptr_t)first->m_Pool, (uintptr_t)second->m_Pool);
    ASSERT_EQ(2u, store->m_Tables.Size());
    ASSERT_EQ(0u, GetInstanceTable(first, 1)->m_Rows.Size());
    DSId            ids[2][2];
    HDSBlobInstance instances[] = { first, second };
    for (uint32_t i = 0; i < 2; ++i)
        for (uint32_t r = 0; r < 2; ++r)
        {
            uint32_t slot = GetInstanceSlots(instances[i])[r];
            ids[i][r] = ((uint64_t)store->m_Slots[slot].m_Generation << 32) | slot;
            ASSERT_EQ((uint64_t)(11 + r), DSGetComponentId(store, ids[i][r]));
            ASSERT_EQ(DS_RESULT_OK, DSSetFieldNumber(store, ids[i][r], 10, 100 + i));
            ASSERT_EQ(DS_RESULT_OK, DSSetFieldString(store, ids[i][r], 20, i ? "second" : "first"));
        }
    ASSERT_TRUE(first->m_Payloads != 0 && second->m_Payloads != 0);
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[0][0]));
    ASSERT_EQ(UINT32_MAX, GetInstanceSlots(first)[0]);
    ASSERT_EQ(DS_RESULT_OK, Register(store, 3));
    DSId reused;
    ASSERT_EQ(DS_RESULT_OK, Add(store, 3, 77, 500, "independent", &reused));
    ASSERT_EQ((uint32_t)ids[0][0], (uint32_t)reused);
    ASSERT_NE(ids[0][0], reused);
    DSResetBlob(first);
    ASSERT_EQ((uintptr_t)0, (uintptr_t)first->m_Payloads);
    ASSERT_TRUE(second->m_Payloads != 0);
    double      number;
    const char* string;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, ids[0][1], 10, &number));
    ASSERT_EQ(20.0, number);
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetString(store, ids[0][1], 20, &string));
    ASSERT_STREQ("shared", string);
    ASSERT_EQ(DS_RESULT_OK, DSRemoveBlob(first));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGetNumber(store, ids[0][1], 10, &number));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, reused, 10, &number));
    ASSERT_EQ(500.0, number);
    for (uint32_t r = 0; r < 2; ++r)
    {
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, ids[1][r], 10, &number));
        ASSERT_EQ(101.0, number);
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetString(store, ids[1][r], 20, &string));
        ASSERT_STREQ("second", string);
    }
    DSResetBlob(second);
    for (uint32_t r = 0; r < 2; ++r)
    {
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, ids[1][r], 10, &number));
        ASSERT_EQ(10.0 + r * 10, number);
        ASSERT_EQ((uint64_t)(11 + r), DSGetComponentId(store, ids[1][r]));
    }
    ASSERT_EQ(DS_RESULT_OK, DSRemoveBlob(second));
    ASSERT_EQ((uintptr_t)0, (uintptr_t)store->m_Pools);
    ASSERT_EQ(1u, store->m_Tables.Size());
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetString(store, reused, 20, &string));
    ASSERT_STREQ("independent", string);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(source));
    delete[] bytes;
}

TEST(Data, CompactMetadataLimitsRoundTrip)
{
    // A reusable binary tree reaches 65,535 entries with only two siblings per
    // level, keeping validation linear instead of constructing one huge flat type.
    DSStructDesc layouts[15] = {};
    DSFieldDesc  members[15][2] = {};
    for (uint32_t depth = 0; depth < 15; ++depth)
    {
        for (uint32_t i = 0; i < 2; ++i)
        {
            DSFieldDesc field = {
                .m_Field = i + 1,
                .m_Type = depth == 14 ? DS_TYPE_NULL : DS_TYPE_STRUCT,
                .m_Struct = depth == 14 ? 0 : &layouts[depth + 1],
                .m_Name = i ? "right" : "left"
            };
            members[depth][i] = field;
        }
        layouts[depth].m_Fields = members[depth];
        layouts[depth].m_FieldCount = 2;
    }
    DSFieldDesc roots[] = {
        { .m_Field = 1, .m_Type = DS_TYPE_STRUCT, .m_Struct = layouts, .m_Name = "1" },
        { .m_Field = 2, .m_Type = DS_TYPE_NULL, .m_Name = "2" }
    };
    uint64_t* tags = new uint64_t[DATA_MAX_TAG_COUNT];
    for (uint32_t i = 0; i < DATA_MAX_TAG_COUNT; ++i)
        tags[i] = i;
    HDSContext  store = DSCreateStore(0);
    DSTableDesc desc = { .m_Type = 1, .m_Tags = tags, .m_TagCount = DATA_MAX_TAG_COUNT + 1, .m_Fields = roots, .m_FieldCount = 1 };
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterTable(store, &desc));
    desc.m_TagCount = DATA_MAX_TAG_COUNT;
    desc.m_FieldCount = DATA_MAX_METADATA_COUNT + 1;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterTable(store, &desc));
    desc.m_FieldCount = 2;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterTable(store, &desc)); // Nested total is 65,536.
    ASSERT_EQ(0u, store->m_Tables.Size());
    desc.m_FieldCount = 1;
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &desc));
    ASSERT_EQ(DATA_MAX_METADATA_COUNT, (uint32_t)store->m_Tables[0]->m_MetadataCount);
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, bytes, size, &size));
    HDSContext      loaded = DSCreateStore(0);
    HDSBlobInstance instance;
    ASSERT_EQ(DS_RESULT_OK, Load(loaded, bytes, size, &instance));
    DSTable* table = GetInstanceTable(instance, 0);
    ASSERT_EQ(DATA_MAX_METADATA_COUNT, (uint32_t)table->m_MetadataCount);
    ASSERT_EQ(DATA_MAX_TAG_COUNT, (uint32_t)table->m_TagCount);
    ASSERT_EQ((uint64_t)(DATA_MAX_TAG_COUNT - 1), GetTableTag(table, DATA_MAX_TAG_COUNT - 1));
    for (uint32_t i = 0; i < DATA_MAX_METADATA_COUNT; ++i)
    {
        DSFieldMeta expected = GetFieldMeta(store->m_Tables[0], i);
        DSFieldMeta actual = GetFieldMeta(table, i);
        ASSERT_EQ(expected.m_Field, actual.m_Field);
        ASSERT_EQ(expected.m_Name, actual.m_Name);
        ASSERT_EQ(expected.m_Offset, actual.m_Offset);
        ASSERT_EQ(expected.m_ChildIndex, actual.m_ChildIndex);
        ASSERT_EQ(expected.m_ChildCount, actual.m_ChildCount);
    }
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(loaded));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    delete[] bytes;
    delete[] tags;
}

TEST(Data, CompactMetadataPreservesWideByteOffsets)
{
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc field = { .m_Field = 10, .m_Type = DS_TYPE_VECTOR3, .m_Offset = 65536 };
    DSTableDesc table = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = 65548 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    DSValueType type = DS_TYPE_VECTOR3;
    DSValueData value = { .m_Vector3 = { 3, 5, 7 } };
    DSId        id;
    ASSERT_EQ(DS_RESULT_OK, DSAddRow(store, 1, 42, &type, &value, 1, &id));
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, bytes, size, &size));
    HDSContext      loaded = DSCreateStore(0);
    HDSBlobInstance instance;
    ASSERT_EQ(DS_RESULT_OK, Load(loaded, bytes, size, &instance));
    DSQueryItem requested = { .m_Hash = 10, .m_Type = DS_TYPE_VECTOR3, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryDesc desc = { .m_Items = { requested } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(loaded, &desc, &query));
    uint32_t handle = DSQueryFindField(query, &requested);
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSIterator it = DSQueryIter(query);
    ASSERT_TRUE(DSIterNext(&it));
    DSRowIterator rows = DSIterRows(&it);
    ASSERT_TRUE(DSRowIterNext(&rows));
    const DSVector3* actual = DSRowIterGetVector3(&rows, handle);
    ASSERT_EQ(0, memcmp(value.m_Vector3, actual, sizeof(DSVector3)));
    ASSERT_EQ(65548u, it.m_RowStride);
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(loaded));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    delete[] bytes;
}

// Interleaved items preserve nested names, NULL fields, group zero and copied setup inputs.
TEST(Data, InlineQueryFieldsAndTags)
{
    HDSContext  store = DSCreateStore(0);
    DSGroupId   group = 0;
    char        name[] = "light.color";
    DSQueryDesc desc = {
        .m_GroupIds = &group,
        .m_GroupIdCount = 1,
        .m_Items = { DSTag("light"),
                     DSField(name, DS_TYPE_VECTOR3, DS_FIELD_MUTABLE),
                     DSTag("visible"),
                     DSField("unused", DS_TYPE_NULL, 0) }
    };
    name[0] = 'X';
    HDSQuery query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    ASSERT_EQ(2u, query->m_Fields.Size());
    ASSERT_EQ(2u, query->m_Tags.Size());
    uint32_t color_field = DSQueryFindField(query, &desc.m_Items[1]);
    ASSERT_EQ(0u, color_field);
    ASSERT_EQ(UINT32_MAX, DSQueryFindField(query, &desc.m_Items[0]));
    ASSERT_EQ(UINT32_MAX, DSQueryFindField(query, &desc.m_Items[2]));
    memset(&desc, 0, sizeof(desc));

    DSFieldDesc  color = { .m_Field = dmHashString64("color"), .m_Type = DS_TYPE_VECTOR3, .m_Name = "color" };
    DSStructDesc light = { .m_Fields = &color, .m_FieldCount = 1, .m_Size = sizeof(DSVector3) };
    DSFieldDesc  fields[] = {
        { .m_Field = dmHashString64("light"), .m_Type = DS_TYPE_STRUCT, .m_Struct = &light, .m_Name = "light" },
        { .m_Field = dmHashString64("unused"), .m_Type = DS_TYPE_NULL, .m_Offset = sizeof(DSVector3) }
    };
    uint64_t    tags[] = { dmHashString64("light"), dmHashString64("visible") };
    DSTableDesc table = { .m_Type = 1, .m_Tags = tags, .m_TagCount = 2, .m_Fields = fields, .m_FieldCount = 2, .m_RowStride = sizeof(DSVector3) };
    DSVector3   value = { .m_Values = { 1, 2, 3 } };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, 0, 1, &value, 0));
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, 1, 1, &value, 0));
    table.m_Type = 2;
    table.m_TagCount = 1;
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 2, 0, 1, &value, 0));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    ASSERT_EQ(1u, DSQueryGetRowCount(query));
    DSIterator it = DSQueryIterRange(query, 0, DSQueryGetRowCount(query));
    ASSERT_TRUE(DSIterNext(&it));
    DSRowIterator rows = DSIterRows(&it);
    ASSERT_TRUE(DSRowIterNext(&rows));
    DSRowIterGetVector3Mut(&rows, color_field)->m_Values[0] = 7;
    ASSERT_EQ(7.0f, DSRowIterGetVector3(&rows, color_field)->m_Values[0]);
    ASSERT_FALSE(DSRowIterNext(&rows));
    ASSERT_FALSE(DSIterNext(&it));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Full mixed item arrays need no terminator; invalid kinds/types are rejected before copying.
TEST(Data, InlineQueryCapacity)
{
    HDSContext  store = DSCreateStore(0);
    DSQueryDesc desc = {};
    for (uint32_t i = 0; i < DS_QUERY_MAX_ITEMS; i += 2)
    {
        desc.m_Items[i] = { .m_Hash = i + 1, .m_Type = DS_TYPE_NUMBER, .m_Kind = DS_QUERY_ITEM_FIELD };
        desc.m_Items[i + 1] = { .m_Hash = i + 1, .m_Kind = DS_QUERY_ITEM_TAG };
    }
    HDSQuery query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    ASSERT_EQ((uint32_t)DS_QUERY_MAX_ITEMS / 2, query->m_Fields.Size());
    ASSERT_EQ((uint32_t)DS_QUERY_MAX_ITEMS / 2, query->m_Tags.Size());
    ASSERT_NE(UINT32_MAX, DSQueryFindField(query, &desc.m_Items[DS_QUERY_MAX_ITEMS - 2]));
    ASSERT_EQ(UINT32_MAX, DSQueryFindField(query, &desc.m_Items[DS_QUERY_MAX_ITEMS - 1]));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    query = 0;
    desc.m_Items[DS_QUERY_MAX_ITEMS - 1].m_Kind = (DSQueryItemKind)99;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSCreateQuery(store, &desc, &query));
    ASSERT_EQ((HDSQuery)0, query);
    desc.m_Items[DS_QUERY_MAX_ITEMS - 1].m_Kind = DS_QUERY_ITEM_TAG;
    desc.m_Items[DS_QUERY_MAX_ITEMS - 2].m_Type = (DSValueType)256;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSCreateQuery(store, &desc, &query));
    desc = { .m_Items = { { .m_Type = DS_TYPE_NULL, .m_Kind = DS_QUERY_ITEM_FIELD } } };
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    ASSERT_EQ(1u, query->m_Fields.Size());
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    // Entries after the first unused item are outside the descriptor.
    desc.m_Items[2] = { .m_Kind = (DSQueryItemKind)99 };
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    ASSERT_EQ(1u, query->m_Fields.Size());
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Item kinds distinguish a zero hash from an unused entry, for both tags and fields.
TEST(Data, QueryItemsIncludeZeroHashes)
{
    HDSContext  store = DSCreateStore(0);
    uint64_t    tag = 0;
    DSFieldDesc field = { .m_Type = DS_TYPE_NUMBER };
    DSTableDesc table = { .m_Type = 1, .m_Tags = &tag, .m_TagCount = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(double) };
    double      value = 7;
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, 0, 1, &value, 0));
    DSQueryDesc desc = { .m_Items = { { .m_Kind = DS_QUERY_ITEM_FIELD }, { .m_Hash = 0, .m_Kind = DS_QUERY_ITEM_TAG } } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    ASSERT_EQ(1u, query->m_Fields.Size());
    ASSERT_EQ(1u, query->m_Tags.Size());
    uint32_t handle = DSQueryFindField(query, &desc.m_Items[0]);
    ASSERT_NE(UINT32_MAX, handle);
    ASSERT_EQ(UINT32_MAX, DSQueryFindField(query, &desc.m_Items[1]));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSIterator it = DSQueryIterRange(query, 0, DSQueryGetRowCount(query));
    ASSERT_TRUE(DSIterNext(&it));
    DSRowIterator rows = DSIterRows(&it);
    ASSERT_TRUE(DSRowIterNext(&rows));
    ASSERT_EQ(value, *DSRowIterGetNumber(&rows, handle));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Zero and DS_FIELD_MUTABLE are valid access flags; reject invalid kinds/flags before narrowing.
TEST(Data, CompactQueryFieldsValidateBeforeNarrowing)
{
    HDSContext   store = DSCreateStore(0);
    DSQueryDesc  desc = { .m_Items = { { .m_Hash = 1, .m_Type = DS_TYPE_VECTOR3, .m_Access = 0, .m_Kind = DS_QUERY_ITEM_FIELD } } };
    DSQueryItem& field = desc.m_Items[0];
    HDSQuery     query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    ASSERT_EQ(0u, DSQueryFindField(query, &field));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    field.m_Access = DS_FIELD_MUTABLE;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    ASSERT_EQ(0u, DSQueryFindField(query, &field));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    field.m_Type = (DSValueType)256;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSCreateQuery(store, &desc, &query));
    field.m_Type = DS_TYPE_VECTOR3;
    field.m_Access = 2;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSCreateQuery(store, &desc, &query));
    field.m_Access = 256;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSCreateQuery(store, &desc, &query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, FreeSlotCountExcludesRetiredGenerations)
{
    HDSContext  store = DSCreateStore(0);
    DSTableDesc table = { .m_Type = 1 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    DSRowDesc rows[6] = {};
    DSId      ids[6];
    ASSERT_EQ(DS_RESULT_OK, DSAddRows(store, 1, rows, 4, ids));
    ASSERT_EQ(0u, store->m_FreeSlotCount);
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[0]));
    ASSERT_EQ(1u, store->m_FreeSlotCount);
    // An exhausted generation is retired permanently rather than reusable.
    store->m_Slots[(uint32_t)ids[1]].m_Generation = UINT32_MAX;
    DSId retired = ((uint64_t)UINT32_MAX << 32) | (uint32_t)ids[1];
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, retired));
    ASSERT_EQ(1u, store->m_FreeSlotCount);
    DSId added[2];
    ASSERT_EQ(DS_RESULT_OK, DSAddRows(store, 1, rows, 2, added));
    ASSERT_EQ(0u, store->m_FreeSlotCount);
    ASSERT_EQ(5u, store->m_Slots.Size());
    ASSERT_NE(ids[0], added[0]);
    ASSERT_EQ((uint32_t)ids[0], (uint32_t)added[0]);
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSRemoveRow(store, ids[0]));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSRemoveRow(store, retired));
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterTable(store, 1));
    ASSERT_EQ(4u, store->m_FreeSlotCount);
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    ASSERT_EQ(DS_RESULT_OK, DSAddRows(store, 1, rows, 6, ids));
    ASSERT_EQ(0u, store->m_FreeSlotCount);
    ASSERT_EQ(7u, store->m_Slots.Size());
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, BatchVector3MatchesScalarGetter)
{
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc vector = { .m_Field = 42, .m_Type = DS_TYPE_VECTOR3, .m_Offset = 0 };
    DSFieldDesc number = { .m_Field = 42, .m_Type = DS_TYPE_NUMBER, .m_Offset = 0 };
    DSFieldDesc missing = { .m_Field = 43, .m_Type = DS_TYPE_VECTOR3, .m_Offset = 0 };
    DSFieldDesc collision[] = {
        { .m_Field = 58, .m_Type = DS_TYPE_VECTOR3, .m_Offset = 0 },
        { .m_Field = 42, .m_Type = DS_TYPE_VECTOR3, .m_Offset = 12 }
    };
    DSTableDesc tables[] = {
        { .m_Type = 1, .m_Fields = &vector, .m_FieldCount = 1, .m_RowStride = 12 },
        { .m_Type = 2, .m_Fields = &number, .m_FieldCount = 1, .m_RowStride = 8 },
        { .m_Type = 3, .m_Fields = &missing, .m_FieldCount = 1, .m_RowStride = 12 },
        { .m_Type = 4, .m_Fields = collision, .m_FieldCount = 2, .m_RowStride = 24 }
    };
    DSValueType vector_types[] = { DS_TYPE_VECTOR3, DS_TYPE_VECTOR3 };
    DSValueType number_type = DS_TYPE_NUMBER;
    DSValueData vectors[] = { { .m_Vector3 = { 1, 2, 3 } }, { .m_Vector3 = { 4, 5, 6 } } };
    DSValueData scalar = { .m_Number = 1 };
    DSId        valid[5];
    for (uint32_t i = 0; i < 4; ++i)
    {
        ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &tables[i]));
        ASSERT_EQ(DS_RESULT_OK, DSAddRow(store, tables[i].m_Type, i, i == 1 ? &number_type : vector_types, i == 1 ? &scalar : vectors, tables[i].m_FieldCount, &valid[i]));
    }
    DSId stale = valid[0];
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, stale));
    ASSERT_EQ(DS_RESULT_OK, DSAddRow(store, 1, 77, vector_types, vectors, 1, &valid[0]));
    // Include a packed row alongside independent rows in the same store.
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, bytes, size, &size));
    HDSBlob         blob;
    HDSBlobInstance instance;
    ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
    ASSERT_EQ(DS_RESULT_OK, DSAddBlob(store, blob, 88, &instance));
    valid[4] = MakeId(store, GetInstanceSlots(instance)[0]);
    DSId      ids[83];
    DSVector3 actual[83];
    DSId      successful[] = { valid[0], valid[3], valid[4] };
    for (uint32_t i = 0; i < 83; ++i)
        ids[i] = successful[i % 3];
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3Batch(store, 83, ids, 42, actual));
    for (uint32_t i = 0; i < 83; ++i)
    {
        DSVector3 expected;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3(store, ids[i], 42, &expected));
        ASSERT_EQ(0, memcmp(&actual[i], &expected, sizeof(expected)));
    }

    // Failures before/after an internal batch boundary return the scalar error.
    // Output on error is deliberately not inspected: partial writes are allowed.
    DSId     bad[] = { valid[1], valid[2], stale, UINT64_MAX, 0 };
    uint32_t positions[] = { 0, 1, 63, 64, 82 };
    for (uint32_t b = 0; b < sizeof(bad) / sizeof(bad[0]); ++b)
    {
        DSVector3 ignored;
        DSResult  expected = DSFieldGetVector3(store, bad[b], 42, &ignored);
        ASSERT_NE(DS_RESULT_OK, expected);
        for (uint32_t p = 0; p < sizeof(positions) / sizeof(positions[0]); ++p)
        {
            uint32_t index = positions[p];
            DSId     saved = ids[index];
            ids[index] = bad[b];
            ASSERT_EQ(expected, DSFieldGetVector3Batch(store, 83, ids, 42, actual));
            ids[index] = saved;
        }
    }
    // The first error follows input order even when later errors differ.
    ids[3] = valid[2];
    ids[80] = valid[1];
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGetVector3Batch(store, 83, ids, 42, actual));
    ids[3] = valid[1];
    ids[80] = valid[2];
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSFieldGetVector3Batch(store, 83, ids, 42, actual));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3Batch(0, 0, 0, 42, 0));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveBlob(instance));
    DSDestroyBlob(blob);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    delete[] bytes;
}

TEST(Data, NativeRowsShareGroupsWithoutSplittingTables)
{
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc field = { .m_Field = 10, .m_Type = DS_TYPE_NUMBER };
    DSTableDesc table = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(double) };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    DSGroupId   group = 77;
    DSQueryDesc desc = { .m_GroupIds = &group, .m_GroupIdCount = 1 };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    double values[] = { 1, 2, 3 };
    DSId   first[3], other[3], last[3];
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, group, 3, values, first));
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, 0, 3, values, other));
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, group, 3, values, last));
    ASSERT_EQ(1u, store->m_Tables.Size());
    // Removing the first row swaps the last row into its slot; group filters survive.
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, first[0]));
    for (uint32_t pass = 0; pass < 2; ++pass)
    {
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
        ASSERT_EQ(5u, DSQueryGetRowCount(query));
        DSIterator it = DSQueryIterRange(query, 0, 5);
        uint32_t   visited = 0;
        while (DSIterNext(&it))
        {
            DSRowIterator rows = DSIterRows(&it);
            while (DSRowIterNext(&rows))
            {
                ASSERT_EQ(group, DSRowIterGetGroupId(&rows));
                DSId id = DSRowIterGetId(&rows);
                ASSERT_NE(first[0], id);
                for (uint32_t i = 0; i < 3; ++i)
                    ASSERT_NE(other[i], id);
                ++visited;
            }
        }
        ASSERT_EQ(5u, visited);
        DSQueryEnd(query);
        // A reset changes only values, leaving the shared group intact.
        ASSERT_EQ(DS_RESULT_OK, DSSetFieldNumber(store, last[2], 10, 100));
        ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, last[2]));
    }
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    desc.m_GroupIds = 0;
    desc.m_GroupIdCount = 0;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSIterator it = DSQueryIter(query);
    ASSERT_TRUE(DSIterNext(&it));
    DSRowIterator rows = DSIterRows(&it);
    uint32_t      zero_group = 0;
    while (DSRowIterNext(&rows))
        zero_group += DSRowIterGetGroupId(&rows) == 0;
    ASSERT_EQ(3u, zero_group);
    ASSERT_FALSE(DSIterNext(&it));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, CreateRowsWithoutReturningIds)
{
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc field = { .m_Field = 10, .m_Type = DS_TYPE_NUMBER };
    DSTableDesc table = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(double) };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    DSQueryItem query_field = { .m_Hash = 10, .m_Type = DS_TYPE_NUMBER, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryDesc desc = { .m_Items = { query_field } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    uint32_t     field_index = DSQueryFindField(query, &query_field);
    double       values[] = { 1, 2, 3, 4 };
    DSFieldArray column = { .m_Field = 10, .m_Values = values + 2 };
    DSValueType  type = DS_TYPE_NUMBER;
    DSValueData  decoded = { .m_Number = 5 };
    DSRowDesc    row = { .m_Group = 42, .m_Types = &type, .m_Values = &decoded, .m_ValueCount = 1 };
    DSId         ids[6] = {};
    for (uint32_t pass = 0; pass < 2; ++pass)
    {
        ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, 42, 2, values, 0));
        ASSERT_EQ(DS_RESULT_OK, DSCreateRowsSoA(store, 1, 42, 2, 1, &column, 0));
        decoded.m_Number = 5;
        ASSERT_EQ(DS_RESULT_OK, DSAddRows(store, 1, &row, 1, 0));
        decoded.m_Number = 6;
        ASSERT_EQ(DS_RESULT_OK, DSAddRow(store, 1, 42, &type, &decoded, 1, 0));

        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
        ASSERT_EQ(6u, DSQueryGetRowCount(query));
        DSIterator it = DSQueryIterRange(query, 0, 6);
        uint32_t   seen = 0;
        while (DSIterNext(&it))
        {
            DSRowIterator rows = DSIterRows(&it);
            while (DSRowIterNext(&rows))
            {
                double value = *DSRowIterGetNumber(&rows, field_index);
                ASSERT_TRUE(value >= 1 && value <= 6);
                uint32_t index = (uint32_t)value - 1;
                ASSERT_EQ(0u, seen & (1u << index));
                seen |= 1u << index;
                ASSERT_EQ(42u, DSRowIterGetGroupId(&rows));
                DSId id = DSRowIterGetId(&rows);
                ASSERT_NE(ids[index], id);
                ids[index] = id;
            }
        }
        ASSERT_EQ(63u, seen);
        DSQueryEnd(query);
        // Query-discovered IDs support lookup and removal, including reused slots.
        for (uint32_t i = 0; i < 6; ++i)
        {
            double value;
            ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, ids[i], 10, &value));
            ASSERT_EQ((double)(i + 1), value);
            ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[i]));
            ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGetNumber(store, ids[i], 10, &value));
        }
    }
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, NativeRowsValidateBeforeWritingAcrossBlocks)
{
    const uint32_t count = 1031;
    struct NativeRow
    {
        uint8_t   m_Enabled;
        DSVector3 m_Position;
    };
    NativeRow input[count] = {};
    DSId      ids[count];
    for (uint32_t i = 0; i < count; ++i)
    {
        input[i].m_Enabled = i & 1;
        input[i].m_Position.m_Values[0] = (float)i;
        input[i].m_Position.m_Values[1] = 2;
        input[i].m_Position.m_Values[2] = 3;
        ids[i] = 99;
    }
    DSFieldDesc fields[] = {
        { .m_Field = 1, .m_Type = DS_TYPE_BOOLEAN, .m_Offset = offsetof(NativeRow, m_Enabled) },
        { .m_Field = 2, .m_Type = DS_TYPE_VECTOR3, .m_Offset = offsetof(NativeRow, m_Position) }
    };
    DSTableDesc table = { .m_Type = 1, .m_Fields = fields, .m_FieldCount = 2, .m_RowStride = sizeof(NativeRow) };
    HDSContext  store = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    input[count - 1].m_Enabled = 2;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSCreateRows(store, 1, 42, count, input, ids));
    ASSERT_TRUE(FindTable(store, 1)->m_Rows.Empty());
    for (uint32_t i = 0; i < count; ++i)
        ASSERT_EQ(99u, ids[i]);

    input[count - 1].m_Enabled = 0;
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, 42, count, input, ids));
    for (uint32_t i = 0; i < count; ++i)
    {
        uint8_t   enabled;
        DSVector3 position;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetBoolean(store, ids[i], 1, &enabled));
        ASSERT_EQ(input[i].m_Enabled, enabled);
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3(store, ids[i], 2, &position));
        ASSERT_EQ(0, memcmp(&input[i].m_Position, &position, sizeof(position)));
    }
    DSVector3 changed = { .m_Values = { -1, -2, -3 } };
    ASSERT_EQ(DS_RESULT_OK, DSSetFieldVector3(store, ids[count - 1], 2, &changed));
    ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, ids[count - 1]));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3(store, ids[count - 1], 2, &changed));
    ASSERT_EQ((float)(count - 1), changed.m_Values[0]);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

TEST(Data, BatchStringsAcrossOwnedAndPackedRows)
{
    HDSContext store = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, Register(store, 1));
    DSId ids[131];
    char text[32];
    for (uint32_t i = 0; i < 131; ++i)
    {
        snprintf(text, sizeof(text), "value-%u", i);
        ASSERT_EQ(DS_RESULT_OK, Add(store, 1, i, i, text, &ids[i]));
    }
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, bytes, size, &size));
    HDSBlob         blob;
    HDSBlobInstance instance;
    ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
    ASSERT_EQ(DS_RESULT_OK, DSAddBlob(store, blob, 999, &instance));
    for (uint32_t i = 0; i < 131; i += 2)
        ids[i] = MakeId(store, GetInstanceSlots(instance)[i]);
    const char* strings[131];
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetStringBatch(store, 131, ids, 20, strings));
    for (uint32_t i = 0; i < 131; ++i)
    {
        snprintf(text, sizeof(text), "value-%u", i);
        ASSERT_STREQ(text, strings[i]);
    }
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSFieldGetStringBatch(store, 131, ids, 10, strings));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGetStringBatch(store, 131, ids, 999, strings));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetStringBatch(0, 0, 0, 20, 0));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveBlob(instance));
    DSDestroyBlob(blob);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
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
    double       values[field_count][count];
    uint8_t      enabled[count];
    DSFieldDesc  layout[field_count + 1] = {};
    DSFieldArray fields[field_count + 1] = {};
    DSId         ids[count];
    for (uint32_t f = 0; f < field_count; ++f)
    {
        layout[f] = { .m_Field = 100 + f, .m_Type = DS_TYPE_NUMBER, .m_Offset = (uint32_t)(f * sizeof(double)) };
        fields[field_count - f] = { .m_Field = 100 + f, .m_Values = values[f] };
        for (uint32_t r = 0; r < count; ++r)
            values[f][r] = f * count + r;
    }
    layout[field_count] = { .m_Field = 200, .m_Type = DS_TYPE_BOOLEAN, .m_Offset = offsetof(Row, m_Enabled) };
    fields[0] = { .m_Field = 200, .m_Values = enabled };
    for (uint32_t r = 0; r < count; ++r)
    {
        enabled[r] = r & 1;
        ids[r] = 99;
    }
    HDSContext  store = DSCreateStore(0);
    DSTableDesc desc = { .m_Type = 1, .m_Fields = layout, .m_FieldCount = field_count + 1, .m_RowStride = sizeof(Row) };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &desc));
    DSTable* table = FindTable(store, 1);
    uint64_t revision = store->m_Revision;
    enabled[count - 1] = 2;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSCreateRowsSoA(store, 1, 42, count, field_count + 1, fields, ids));
    ASSERT_EQ(revision, store->m_Revision);
    ASSERT_TRUE(table->m_Rows.Empty());
    ASSERT_TRUE(table->m_Chunks.Empty());
    for (uint32_t r = 0; r < count; ++r)
        ASSERT_EQ(99u, ids[r]);

    enabled[count - 1] = 0;
    ASSERT_EQ(DS_RESULT_OK, DSCreateRowsSoA(store, 1, 42, count, field_count + 1, fields, ids));
    for (uint32_t f = 0; f < field_count; ++f)
    {
        for (uint32_t r = 0; r < count; ++r)
        {
            double value;
            ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, ids[r], 100 + f, &value));
            ASSERT_EQ(values[f][r], value);
        }
    }
    // Append after swap removal and verify earlier rows/defaults remain intact.
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[0]));
    DSId added;
    ASSERT_EQ(DS_RESULT_OK, DSCreateRowsSoA(store, 1, 0, 1, field_count + 1, fields, &added));
    ASSERT_NE(ids[0], added);
    values[3][count - 1] = -1;
    ASSERT_EQ(DS_RESULT_OK, DSSetFieldNumber(store, ids[count - 1], 103, 0));
    ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, ids[count - 1]));
    double value;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, ids[count - 1], 103, &value));
    ASSERT_EQ((double)(4 * count - 1), value);
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, added, 103, &value));
    ASSERT_EQ((double)(3 * count), value);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

static void CheckNativeReferences(HDSContext store, const DSId ids[2])
{
    for (uint32_t i = 0; i < 2; ++i)
    {
        DSValue object;
        DSValue child;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[i], 11, &object));
        ASSERT_EQ(DS_TYPE_STRUCT, object.m_Type);
        ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&object.m_Value.m_Struct, 1, &child));
        ASSERT_STREQ(i ? "bandits" : "guards", child.m_Value.m_String);
        ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&object.m_Value.m_Struct, 2, &child));
        ASSERT_EQ(i ? 25.0 : 15.0, child.m_Value.m_Number);
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[i], 12, &object));
        ASSERT_EQ(DS_TYPE_LIST, object.m_Type);
        ASSERT_EQ(2u, object.m_Value.m_List.m_Count);
        ASSERT_EQ(DS_RESULT_OK, DSGetListValue(&object.m_Value.m_List, 0, &child));
        if (i)
        {
            ASSERT_EQ(DS_TYPE_STRUCT, child.m_Type);
            DSValue faction;
            ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&child.m_Value.m_Struct, 1, &faction));
            ASSERT_STREQ("guards", faction.m_Value.m_String);
        }
        else
        {
            ASSERT_EQ(DS_TYPE_STRING, child.m_Type);
            ASSERT_STREQ("coin", child.m_Value.m_String);
        }
    }
}

TEST(Data, NativeReferencesFromC)
{
    for (uint32_t soa = 0; soa < 2; ++soa)
    {
        HDSContext store = DSCreateStore(0);
        DSId       ids[2];
        ASSERT_EQ(0, CreateReferenceRowsFromC(store, soa, ids));
        CheckNativeReferences(store, ids);
        DSTable* table = FindTable(store, 93);
        ASSERT_TRUE(table->m_Owned->m_BaseValues != 0);
        ASSERT_EQ((DSPayloadBlock*)0, table->m_Owned->m_BaseValues->m_Next); // One payload allocation for the batch.

        uint8_t  DM_ALIGNED(8) bytes[4096];
        uint32_t size;
        ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, bytes, sizeof(bytes), &size));
        HDSBlob blob;
        ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
        HDSContext      loaded = DSCreateStore(0);
        HDSBlobInstance instance;
        ASSERT_EQ(DS_RESULT_OK, DSAddBlob(loaded, blob, 42, &instance));
        DSDestroyBlob(blob);
        DSId loaded_ids[] = { MakeId(loaded, 0), MakeId(loaded, 1) };
        CheckNativeReferences(loaded, loaded_ids);
        ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(loaded));
        ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[0]));
        ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, ids[1]));
        const char* name;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetString(store, ids[1], 10, &name));
        ASSERT_STREQ("Bandit", name);
        ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    }
}

TEST(Data, NativeReferenceValidation)
{
    for (uint32_t soa = 0; soa < 2; ++soa)
    {
        HDSContext  store = DSCreateStore(0);
        DSFieldDesc field = { .m_Field = 1, .m_Type = DS_TYPE_LIST };
        DSTableDesc table = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(DSReference) };
        ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
        double      numbers[] = { 1, 2 };
        DSListInput lists[] = {
            { .m_Type = DS_TYPE_NUMBER, .m_Count = 2, .m_Values = numbers },
            { .m_Type = DS_TYPE_NUMBER, .m_Count = 1 }
        };
        DSReference  rows[] = { { .m_List = &lists[0] }, { .m_List = &lists[1] } };
        DSFieldArray column = { .m_Field = 1, .m_Values = rows };
        DSId         ids[] = { 123, 456 };
        for (uint32_t invalid = 0; invalid < 5; ++invalid)
        {
            switch (invalid)
            {
                case 1:
                    lists[1] = { .m_Type = (DSValueType)UINT32_MAX };
                    break;
                case 2:
                    lists[1] = { .m_Type = DS_TYPE_LIST, .m_Count = 1, .m_Values = &rows[1] }; // Cycle exceeds depth limit.
                    break;
                case 3:
                    rows[1].m_List = 0;
                    break;
                case 4:
                    lists[1] = { .m_Type = DS_TYPE_STRING, .m_Count = 1, .m_Values = &rows[1] }; // NULL string.
                    rows[0].m_List = &lists[1];
                    break;
            }
            DSResult result = soa ? DSCreateRowsSoA(store, 1, 0, 2, 1, &column, ids) : DSCreateRows(store, 1, 0, 2, rows, ids);
            ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, result);
            ASSERT_EQ((DSId)123, ids[0]);
            ASSERT_EQ((DSId)456, ids[1]);
            DSTable* stored = FindTable(store, 1);
            ASSERT_TRUE(stored->m_Rows.Empty());
            ASSERT_TRUE(stored->m_Chunks.Empty());
            ASSERT_EQ((DSPayloadBlock*)0, stored->m_Owned->m_BaseValues);
        }
        lists[1] = { .m_Type = DS_TYPE_NUMBER }; // Empty lists need no values array.
        rows[0].m_List = &lists[0];
        rows[1].m_List = &lists[1];
        DSResult result = soa ? DSCreateRowsSoA(store, 1, 0, 2, 1, &column, 0) : DSCreateRows(store, 1, 0, 2, rows, 0);
        ASSERT_EQ(DS_RESULT_OK, result);
        ASSERT_EQ(2u, FindTable(store, 1)->m_Rows.Size());
        ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    }
}

TEST(Data, NativeDynamicStructAndNestedListInput)
{
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc field = { .m_Field = 1, .m_Type = DS_TYPE_STRUCT };
    DSTableDesc table = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(DSReference) };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    uint64_t    bytes[] = { 2, 0 };
    DSFieldDesc members[] = {
        { .m_Field = 1, .m_Type = DS_TYPE_BOOLEAN },
        { .m_Field = 2, .m_Type = DS_TYPE_NULL, .m_Offset = 8 }
    };
    DSStructDesc  layout = { .m_Fields = members, .m_FieldCount = 2, .m_Size = sizeof(bytes) };
    DSStructInput object = { .m_Layout = &layout, .m_Values = bytes };
    DSReference   row = { .m_Struct = &object };
    DSId          id = 123;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSCreateRows(store, 1, 0, 1, &row, &id)); // Invalid Boolean.
    bytes[0] = 1;
    members[1].m_Field = 1;
    ASSERT_EQ(DS_RESULT_ALREADY_EXISTS, DSCreateRows(store, 1, 0, 1, &row, &id));
    members[1].m_Field = 2;
    members[1].m_Offset = 17;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSCreateRows(store, 1, 0, 1, &row, &id));
    object.m_Layout = 0;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSCreateRows(store, 1, 0, 1, &row, &id));
    ASSERT_EQ((DSId)123, id);
    object.m_Layout = &layout;
    layout = {};
    object.m_Values = 0;
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, 0, 1, &row, &id)); // Empty dynamic struct.
    DSValue value;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, 1, &value));
    ASSERT_EQ(0u, value.m_Value.m_Struct.m_Count);

    field.m_Type = DS_TYPE_LIST;
    table.m_Type = 2;
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    DSVector3   vector = { .m_Values = { 1, 2, 3 } };
    DSListInput child = { .m_Type = DS_TYPE_VECTOR3, .m_Count = 1, .m_Values = &vector };
    DSReference element = { .m_List = &child };
    DSListInput parent = { .m_Type = DS_TYPE_LIST, .m_Count = 1, .m_Values = &element };
    row.m_List = &parent;
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 2, 0, 1, &row, &id));
    vector.m_Values[2] = 99;
    DSValue nested;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, 1, &value));
    ASSERT_EQ(DS_RESULT_OK, DSGetListValue(&value.m_Value.m_List, 0, &nested));
    ASSERT_EQ(DS_RESULT_OK, DSGetListValue(&nested.m_Value.m_List, 0, &value));
    ASSERT_EQ(DS_TYPE_VECTOR3, value.m_Type);
    ASSERT_EQ(3.0f, value.m_Value.m_Vector3[2]);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

static void CheckMixedList(const DSList* list)
{
    ASSERT_EQ(9u, list->m_Count);
    const DSValueType types[] = {
        DS_TYPE_NUMBER, DS_TYPE_STRING, DS_TYPE_BOOLEAN, DS_TYPE_STRUCT, DS_TYPE_LIST, DS_TYPE_NULL, DS_TYPE_VECTOR3, DS_TYPE_VECTOR4, DS_TYPE_MATRIX4
    };
    for (uint32_t i = 0; i < list->m_Count; ++i)
    {
        DSValue value;
        ASSERT_EQ(DS_RESULT_OK, DSGetListValue(list, i, &value));
        ASSERT_EQ(types[i], value.m_Type);
    }
    DSValue value;
    ASSERT_EQ(DS_RESULT_OK, DSGetListValue(list, 0, &value));
    ASSERT_EQ(100.0, value.m_Value.m_Number);
    ASSERT_EQ(DS_RESULT_OK, DSGetListValue(list, 1, &value));
    ASSERT_STREQ("coin", value.m_Value.m_String);
    ASSERT_EQ(DS_RESULT_OK, DSGetListValue(list, 2, &value));
    ASSERT_EQ(1, value.m_Value.m_Boolean);
    ASSERT_EQ(DS_RESULT_OK, DSGetListValue(list, 3, &value));
    DSValue child;
    ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&value.m_Value.m_Struct, 42, &child));
    ASSERT_EQ(100.0, child.m_Value.m_Number);
    ASSERT_EQ(DS_RESULT_OK, DSGetListValue(list, 4, &value));
    ASSERT_EQ(2u, value.m_Value.m_List.m_Count);
    ASSERT_EQ(DS_RESULT_OK, DSGetListValue(&value.m_Value.m_List, 0, &child));
    ASSERT_EQ(1.0, child.m_Value.m_Number);
    ASSERT_EQ(DS_RESULT_OK, DSGetListValue(list, 6, &value));
    ASSERT_EQ(3.0f, value.m_Value.m_Vector3[2]);
    ASSERT_EQ(DS_RESULT_OK, DSGetListValue(list, 7, &value));
    ASSERT_EQ(7.0f, value.m_Value.m_Vector4[3]);
    ASSERT_EQ(DS_RESULT_OK, DSGetListValue(list, 8, &value));
    ASSERT_EQ(1.0f, value.m_Value.m_Matrix4[15]);
}

TEST(Data, MixedListCreationFromC)
{
    for (uint32_t soa = 0; soa < 2; ++soa)
    {
        HDSContext store = DSCreateStore(0);
        DSId       ids[2];
        ASSERT_EQ(0, CreateMixedListRowsFromC(store, soa, ids));
        DSValue value;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[0], 1, &value));
        CheckMixedList(&value.m_Value.m_List);
        DSValue parent;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[1], 1, &parent));
        ASSERT_EQ(DS_RESULT_OK, DSGetListValue(&parent.m_Value.m_List, 0, &value));
        CheckMixedList(&value.m_Value.m_List);
        DSTable* table = FindTable(store, 94);
        ASSERT_EQ((DSPayloadBlock*)0, table->m_Owned->m_BaseValues->m_Next);

        DSValue empty = { .m_Type = DS_TYPE_LIST };
        ASSERT_EQ(DS_RESULT_OK, DSSetField(store, ids[0], 1, &empty));
        ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, ids[0]));
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[0], 1, &value));
        CheckMixedList(&value.m_Value.m_List);

        uint8_t  DM_ALIGNED(8) bytes[4096];
        uint32_t size;
        ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, bytes, sizeof(bytes), &size));
        HDSBlob blob;
        ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
        HDSContext      loaded = DSCreateStore(0);
        HDSBlobInstance instance;
        ASSERT_EQ(DS_RESULT_OK, DSAddBlob(loaded, blob, 42, &instance));
        DSDestroyBlob(blob);
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(loaded, MakeId(loaded, 0), 1, &value));
        CheckMixedList(&value.m_Value.m_List);
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(loaded, MakeId(loaded, 1), 1, &parent));
        ASSERT_EQ(DS_RESULT_OK, DSGetListValue(&parent.m_Value.m_List, 0, &value));
        CheckMixedList(&value.m_Value.m_List);
        ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(loaded));
        ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    }
}

TEST(Data, MixedListValidationBeforePublishing)
{
    for (uint32_t soa = 0; soa < 2; ++soa)
    {
        HDSContext  store = DSCreateStore(0);
        DSFieldDesc field = { .m_Field = 1, .m_Type = DS_TYPE_LIST };
        DSTableDesc table = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(DSReference) };
        ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
        DSValueType  type = DS_TYPE_NUMBER;
        const void*  payload = 0;
        DSListInput  mixed = { .m_Count = 1, .m_Values = &payload, .m_Types = &type };
        DSListInput  empty = { .m_Type = DS_TYPE_NULL };
        DSReference  rows[] = { { .m_List = &empty }, { .m_List = &mixed } };
        DSFieldArray column = { .m_Field = 1, .m_Values = rows };
        DSId         ids[] = { 123, 456 };
        uint8_t      boolean = 2;
        for (uint32_t invalid = 0; invalid < 5; ++invalid)
        {
            switch (invalid)
            {
                case 1:
                    type = (DSValueType)UINT32_MAX;
                    break;
                case 2:
                    type = DS_TYPE_BOOLEAN;
                    payload = &boolean;
                    break;
                case 3:
                    type = DS_TYPE_LIST;
                    payload = &mixed; // Mixed-list cycle reaches the existing depth limit.
                    break;
                case 4:
                    mixed.m_Values = 0;
                    break;
            }
            DSResult result = soa ? DSCreateRowsSoA(store, 1, 0, 2, 1, &column, ids) : DSCreateRows(store, 1, 0, 2, rows, ids);
            ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, result);
            ASSERT_EQ((DSId)123, ids[0]);
            ASSERT_EQ((DSId)456, ids[1]);
            DSTable* stored = FindTable(store, 1);
            ASSERT_TRUE(stored->m_Rows.Empty());
            ASSERT_EQ((DSPayloadBlock*)0, stored->m_Owned->m_BaseValues);
        }
        mixed.m_Count = 0; // Empty mixed lists need no payload pointer array.
        ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, 0, 1, rows + 1, 0));
        mixed.m_Count = 1;
        mixed.m_Values = &payload;
        type = DS_TYPE_NULL;
        payload = 0;
        ASSERT_EQ(DS_RESULT_OK, DSCreateRowsSoA(store, 1, 0, 2, 1, &column, 0));
        ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    }
}

// Root aliases must not hide inline members during registration or blob loading.
TEST(Data, DuplicateFullFieldNamesAreRejected)
{
    HDSContext   store = DSCreateStore(0);
    uint64_t     color = dmHashString64("light.color");
    DSFieldDesc  member = { .m_Field = dmHashString64("color"), .m_Type = DS_TYPE_NUMBER, .m_Name = "color" };
    DSStructDesc light = { .m_Fields = &member, .m_FieldCount = 1, .m_Size = sizeof(double) };
    DSFieldDesc  fields[] = {
        { .m_Field = dmHashString64("light"), .m_Type = DS_TYPE_STRUCT, .m_Struct = &light, .m_Name = "light" },
        { .m_Field = color, .m_Type = DS_TYPE_NUMBER, .m_Offset = sizeof(double) }
    };
    DSTableDesc desc = { .m_Type = 1, .m_Fields = fields, .m_FieldCount = 2, .m_RowStride = 2 * sizeof(double) };
    DSQueryItem field = { .m_Hash = color, .m_Type = DS_TYPE_NUMBER, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryDesc query_desc = { .m_Items = { field } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &query_desc, &query));
    uint64_t revision = store->m_Revision;
    ASSERT_EQ(DS_RESULT_ALREADY_EXISTS, DSRegisterTable(store, &desc));
    ASSERT_EQ(revision, store->m_Revision);
    ASSERT_TRUE(store->m_Tables.Empty());
    ASSERT_TRUE(query->m_Tables.Empty());

    fields[1].m_Field = dmHashString64("intensity");
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &desc));
    double values[] = { 3, 7 };
    DSId   id;
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, 0, 1, values, &id));
    double value;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, id, color, &value));
    ASSERT_EQ(3.0, value);

    uint8_t  DM_ALIGNED(8) bytes[512];
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, bytes, sizeof(bytes), &size));
    HDSBlob blob;
    ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
    DSDestroyBlob(blob);
    uint32_t table_offset = (uint32_t)ReadDataInteger(bytes + DATA_TABLE_OFFSETS_OFFSET, 4);
    uint8_t* root = bytes + table_offset + DATA_TABLE_HEADER_SIZE + DATA_FIELD_META_SIZE;
    WriteDataInteger(root + offsetof(DSFileFieldMeta, m_NameHash), color, 8);
    ASSERT_EQ(DS_RESULT_INVALID_FORMAT, DSLoadBlob(bytes, size, &blob));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Native AoS/SoA creation and nested setters must share the blob reader's depth budget.
TEST(Data, InlineAndDynamicNestingShareOneLimit)
{
    DSListInput lists[DATA_MAX_NESTING];
    DSReference references[DATA_MAX_NESTING] = {};
    double      number = 7;
    for (uint32_t i = 0; i < DATA_MAX_NESTING; ++i)
    {
        lists[i] = { .m_Type = i ? DS_TYPE_LIST : DS_TYPE_NUMBER, .m_Count = 1, .m_Values = i ? (const void*)&references[i - 1] : &number };
        references[i].m_List = &lists[i];
    }
    DSFieldDesc  member = { .m_Field = dmHashString64("items"), .m_Type = DS_TYPE_LIST, .m_Name = "items" };
    DSStructDesc layout = { .m_Fields = &member, .m_FieldCount = 1, .m_Size = sizeof(DSReference) };
    DSFieldDesc  root = { .m_Field = dmHashString64("state"), .m_Type = DS_TYPE_STRUCT, .m_Struct = &layout, .m_Name = "state" };
    DSTableDesc  desc = { .m_Type = 1, .m_Fields = &root, .m_FieldCount = 1, .m_RowStride = sizeof(DSReference) };
    for (uint32_t soa = 0; soa < 2; ++soa)
    {
        HDSContext store = DSCreateStore(0);
        ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &desc));
        DSReference  rows[] = { references[DATA_MAX_NESTING - 2], references[DATA_MAX_NESTING - 1] };
        DSFieldArray column = { .m_Field = root.m_Field, .m_Values = rows };
        DSId         ids[] = { 123, 456 };
        DSResult     result = soa ? DSCreateRowsSoA(store, 1, 0, 2, 1, &column, ids) : DSCreateRows(store, 1, 0, 2, rows, ids);
        ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, result);
        ASSERT_EQ((DSId)123, ids[0]);
        ASSERT_EQ((DSId)456, ids[1]);
        ASSERT_TRUE(FindTable(store, 1)->m_Rows.Empty());
        result = soa ? DSCreateRowsSoA(store, 1, 0, 1, 1, &column, ids) : DSCreateRows(store, 1, 0, 1, rows, ids);
        ASSERT_EQ(DS_RESULT_OK, result);

        DSValue nested[DATA_MAX_NESTING + 1];
        nested[0] = Number(number);
        for (uint32_t i = 1; i <= DATA_MAX_NESTING; ++i)
            nested[i] = List(&nested[i - 1], 1);
        uint64_t field = dmHashString64("state.items");
        ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSetField(store, ids[0], field, &nested[DATA_MAX_NESTING]));
        ASSERT_EQ(DS_RESULT_OK, DSSetField(store, ids[0], field, &nested[DATA_MAX_NESTING - 1]));
        uint8_t  DM_ALIGNED(8) bytes[4096];
        uint32_t size;
        ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, bytes, sizeof(bytes), &size));
        HDSBlob blob;
        ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
        DSDestroyBlob(blob);
        ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    }
}

// Repeated waves reuse default storage, and swap removal preserves surviving reset values.
TEST(Data, NativeDefaultsRemainDenseAcrossWaves)
{
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc field = { .m_Field = 1, .m_Type = DS_TYPE_NUMBER };
    DSTableDesc desc = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(double) };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &desc));
    DSTable* table = FindTable(store, 1);
    double   permanent_value = 99;
    DSId     permanent;
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, 0, 1, &permanent_value, &permanent));
    double   values[128];
    DSId     ids[128];
    uint32_t capacity = 0;
    for (uint32_t wave = 0; wave < 32; ++wave)
    {
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(values); ++i)
            values[i] = wave * 128 + i;
        ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, wave, 128, values, ids));
        if (!wave)
            capacity = table->m_Chunks.Size();
        ASSERT_EQ(capacity, table->m_Chunks.Size());
        for (uint32_t i = 0; i < 128; ++i)
        {
            ASSERT_EQ(DS_RESULT_OK, DSSetFieldNumber(store, ids[i], 1, -1));
            ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, ids[i]));
            double value;
            ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, ids[i], 1, &value));
            ASSERT_EQ(values[i], value);
            ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[i]));
            ASSERT_EQ((table->m_Rows.Size() + table->m_ChunkMask) >> table->m_ChunkShift, table->m_Chunks.Size());
            for (uint32_t row = 0; row < table->m_Rows.Size(); ++row)
                ASSERT_EQ(row, table->m_Rows[row].m_BaseIndex);
        }
        ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, permanent));
        double value;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, permanent, 1, &value));
        ASSERT_EQ(permanent_value, value);
    }
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, permanent));
    ASSERT_TRUE(table->m_Chunks.Empty());
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Emptying a native table releases string/list arenas while keeping child slots reusable.
TEST(Data, EmptyNativeTableReleasesPayloadArenas)
{
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc root = { .m_Field = 1, .m_Type = DS_TYPE_STRUCT };
    DSTableDesc table_desc = { .m_Type = 1, .m_Fields = &root, .m_FieldCount = 1, .m_RowStride = sizeof(DSReference) };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table_desc));
    DSFieldDesc fields[] = {
        { .m_Field = 10, .m_Type = DS_TYPE_STRING },
        { .m_Field = 20, .m_Type = DS_TYPE_LIST, .m_Offset = sizeof(DSReference) }
    };
    DSStructDesc  layout = { .m_Fields = fields, .m_FieldCount = 2, .m_Size = 2 * sizeof(DSReference) };
    double        numbers[] = { 3, 7 };
    DSListInput   list = { .m_Type = DS_TYPE_NUMBER, .m_Count = 2, .m_Values = numbers };
    DSReference   members[] = { { .m_String = "original" }, { .m_List = &list } };
    DSStructInput input = { .m_Layout = &layout, .m_Values = members };
    DSReference   rows[] = { { .m_Struct = &input }, { .m_Struct = &input } };
    DSTable*      table = FindTable(store, 1);
    for (uint32_t wave = 0; wave < 4; ++wave)
    {
        DSId ids[2];
        ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, 0, 2, rows, ids));
        DSValue value;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[1], 1, &value));
        ASSERT_EQ(DS_RESULT_OK, DSSetField(store, ids[1], 1, &value));
        ASSERT_TRUE(table->m_Payloads != 0);
        ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[0]));
        ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, ids[1]));
        ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, ids[1], 1, &value));
        DSValue child;
        ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&value.m_Value.m_Struct, 10, &child));
        ASSERT_STREQ("original", child.m_Value.m_String);
        ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[1]));
        ASSERT_TRUE(table->m_Chunks.Empty());
        ASSERT_EQ((DSPayloadBlock*)0, table->m_Owned->m_BaseValues);
        ASSERT_EQ((DSPayloadBlock*)0, table->m_Payloads);
        ASSERT_EQ(0u, table->m_Structs->m_Tables[0]->m_RowCount);
    }
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}
