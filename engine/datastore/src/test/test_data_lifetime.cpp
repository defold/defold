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

#include <jc_test/jc_test.h>
#include "test_data_memory.h"

struct LifetimeRow
{
    double        m_Health;
    DataReference m_Name;
    DataReference m_Attributes;
    DataReference m_Loot;
};

struct LifetimeAttributes
{
    DataReference m_Faction;
    double        m_AlertRadius;
};

static DataResult RegisterLifetimeTable(HDataStore store, DataResetPolicy reset)
{
    DataFieldDesc fields[] = {
        { .m_Field = 1, .m_Type = DATA_TYPE_NUMBER, .m_Offset = offsetof(LifetimeRow, m_Health) },
        { .m_Field = 2, .m_Type = DATA_TYPE_STRING, .m_Offset = offsetof(LifetimeRow, m_Name) },
        { .m_Field = 3, .m_Type = DATA_TYPE_STRUCT, .m_Offset = offsetof(LifetimeRow, m_Attributes) },
        { .m_Field = 4, .m_Type = DATA_TYPE_LIST, .m_Offset = offsetof(LifetimeRow, m_Loot) }
    };
    DataTableDesc table = {
        .m_Type = 1, .m_Fields = fields, .m_FieldCount = 4, .m_RowStride = sizeof(LifetimeRow), .m_ResetPolicy = reset
    };
    return DataRegisterTable(store, &table);
}

static DataResult CreateLifetimeRows(HDataStore store, DataGroupId group, DataId* ids, bool columns)
{
    DataFieldDesc members[] = {
        { .m_Field = 5, .m_Type = DATA_TYPE_STRING, .m_Offset = offsetof(LifetimeAttributes, m_Faction) },
        { .m_Field = 6, .m_Type = DATA_TYPE_NUMBER, .m_Offset = offsetof(LifetimeAttributes, m_AlertRadius) }
    };
    DataStructDesc     layout = { .m_Fields = members, .m_FieldCount = 2, .m_Size = sizeof(LifetimeAttributes) };
    LifetimeAttributes attributes = { .m_Faction = { .m_String = "guards" }, .m_AlertRadius = 25.0 };
    DataStructInput    object = { .m_Layout = &layout, .m_Values = &attributes };
    double             amount = 15;
    const void*        values[] = { "health_pickup", &amount, &object, 0 };
    DataValueType      types[] = { DATA_TYPE_STRING, DATA_TYPE_NUMBER, DATA_TYPE_STRUCT, DATA_TYPE_NULL };
    DataListInput      list = { .m_Count = 4, .m_Values = values, .m_Types = types };
    LifetimeRow        row = {
               .m_Health = 100, .m_Name = { .m_String = "guard" }, .m_Attributes = { .m_Struct = &object }, .m_Loot = { .m_List = &list }
    };
    if (!columns)
        return DataCreateRows(store, 1, group, 1, &row, ids);
    DataFieldArray fields[] = {
        { .m_Field = 4, .m_Values = &row.m_Loot }, { .m_Field = 3, .m_Values = &row.m_Attributes }, { .m_Field = 1, .m_Values = &row.m_Health }, { .m_Field = 2, .m_Values = &row.m_Name }
    };
    return DataCreateRowsSoA(store, 1, group, 1, 4, fields, ids);
}

static void CheckLifetimeViews(HDataStore store, DataId id)
{
    DataValueView attributes, field, list, element;
    EXPECT_EQ(DATA_RESULT_OK, DataFieldGetView(store, id, 3, &attributes));
    EXPECT_EQ(DATA_TYPE_STRUCT, DataViewGetType(&attributes));
    uint32_t count = 0;
    EXPECT_EQ(DATA_RESULT_OK, DataViewGetCount(&attributes, &count));
    EXPECT_EQ(2u, count);
    EXPECT_EQ(DATA_RESULT_OK, DataViewGetField(&attributes, 6, &field));
    double radius = 0;
    EXPECT_EQ(DATA_RESULT_OK, DataViewGetNumber(&field, &radius));
    EXPECT_EQ(25.0, radius);
    EXPECT_EQ(DATA_RESULT_NOT_FOUND, DataViewGetField(&attributes, 999, &field));
    EXPECT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataViewGetElement(&attributes, 0, &field));
    EXPECT_EQ(DATA_RESULT_OK, DataFieldGetView(store, id, 4, &list));
    EXPECT_EQ(DATA_RESULT_OK, DataViewGetCount(&list, &count));
    EXPECT_EQ(4u, count);
    EXPECT_EQ(DATA_RESULT_OK, DataViewGetElement(&list, 0, &element));
    const char* string = 0;
    EXPECT_EQ(DATA_RESULT_OK, DataViewGetString(&element, &string));
    EXPECT_STREQ("health_pickup", string);
    EXPECT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataViewGetNumber(&element, &radius));
    EXPECT_EQ(25.0, radius);
    EXPECT_EQ(DATA_RESULT_OK, DataViewGetElement(&list, 1, &element));
    EXPECT_EQ(DATA_RESULT_OK, DataViewGetNumber(&element, &radius));
    EXPECT_EQ(15.0, radius);
    EXPECT_EQ(DATA_RESULT_OK, DataViewGetElement(&list, 2, &element));
    EXPECT_EQ(DATA_RESULT_OK, DataViewGetField(&element, 5, &element));
    EXPECT_EQ(DATA_RESULT_OK, DataViewGetString(&element, &string));
    EXPECT_STREQ("guards", string);
    EXPECT_EQ(DATA_RESULT_OK, DataViewGetElement(&list, 3, &element));
    EXPECT_EQ(DATA_TYPE_NULL, DataViewGetType(&element));
    EXPECT_EQ(DATA_RESULT_NOT_FOUND, DataViewGetElement(&list, 4, &element));
    EXPECT_EQ(DATA_TYPE_NULL, DataViewGetType(&element));
}

// Native row and field-array inputs copy stack references; reset history is optional.
// Views read nested members and mixed lists without exposing internals.
TEST(DataLifetime, ResetPolicyAndContainerViews)
{
    for (uint32_t history = 0; history < 2; ++history)
        for (uint32_t columns = 0; columns < 2; ++columns)
        {
            HDataStore store = DataCreateStore();
            ASSERT_EQ(DATA_RESULT_OK, RegisterLifetimeTable(store, (DataResetPolicy)history));
            DataId ids[2];
            ASSERT_EQ(DATA_RESULT_OK, CreateLifetimeRows(store, 7, ids, columns != 0));
            ASSERT_EQ(DATA_RESULT_OK, CreateLifetimeRows(store, 8, ids + 1, columns != 0));
            CheckLifetimeViews(store, ids[0]);
            ASSERT_EQ(DATA_RESULT_OK, DataSetFieldNumber(store, ids[0], 1, 42));
            ASSERT_EQ(DATA_RESULT_OK, DataSetFieldString(store, ids[0], 2, "captain"));
            ASSERT_EQ(history ? DATA_RESULT_INVALID_ARGUMENT : DATA_RESULT_OK, DataResetRow(store, ids[0]));
            ASSERT_EQ(history ? DATA_RESULT_INVALID_ARGUMENT : DATA_RESULT_OK, DataResetField(store, ids[0], 1));
            ASSERT_EQ(history ? DATA_RESULT_INVALID_ARGUMENT : DATA_RESULT_OK, DataResetTable(store, 1));
            double health;
            ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(store, ids[0], 1, &health));
            ASSERT_EQ(history ? 42.0 : 100.0, health);
            DataMemoryStats stats;
            GetDataMemoryStats(store, &stats);
            ASSERT_EQ(history ? 0u : 2 * sizeof(LifetimeRow), FindTable(store, 1)->m_Owned->m_BaseRows.Size());
            if (history)
                ASSERT_EQ(0u, stats.m_BaseRowBytes + stats.m_BaseCapacity);

            DataQueryField requested = { .m_Field = 4, .m_Type = DATA_TYPE_LIST };
            DataQueryDesc  desc = { .m_Fields = &requested, .m_FieldCount = 1 };
            HDataQuery     query;
            ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
            uint32_t loot = DataQueryFindField(query, &requested);
            ASSERT_NE(UINT32_MAX, loot);
            ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(query));
            DataIterator it = DataQueryIterRange(query, 0, DataQueryGetRowCount(query));
            uint32_t     seen = 0;
            while (DataIterNext(&it))
            {
                DataRowIterator rows = DataIterRows(&it);
                while (DataRowIterNext(&rows))
                {
                    DataValueView view;
                    DataRowIterGetView(&rows, loot, &view);
                    ASSERT_EQ(DATA_RESULT_OK, DataViewGetElement(&view, 2, &view));
                    ASSERT_EQ(DATA_RESULT_OK, DataViewGetField(&view, 6, &view));
                    ASSERT_EQ(DATA_RESULT_OK, DataViewGetNumber(&view, &health));
                    ASSERT_EQ(25.0, health);
                    ++seen;
                }
            }
            ASSERT_EQ(2u, seen);
            DataQueryEnd(query);
            ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(query));
            uint32_t size;
            ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, 0, 0, &size));
            uint8_t* bytes = new uint8_t[size];
            ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, bytes, size, &size));
            HDataBlob blob;
            ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes, size, &blob));
            HDataStore        loaded = DataCreateStore();
            HDataBlobInstance instance;
            ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(loaded, blob, 0, &instance));
            ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(loaded, &desc, &query));
            ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(query));
            it = DataQueryIterRange(query, 0, DataQueryGetRowCount(query));
            DataId loaded_id;
            ASSERT_TRUE(DataIterNext(&it));
            DataRowIterator loaded_rows = DataIterRows(&it);
            ASSERT_TRUE(DataRowIterNext(&loaded_rows));
            loaded_id = DataRowIterGetId(&loaded_rows);
            DataValueView packed;
            DataRowIterGetView(&loaded_rows, DataQueryFindField(query, &requested), &packed);
            ASSERT_EQ(DATA_RESULT_OK, DataViewGetElement(&packed, 2, &packed));
            ASSERT_EQ(DATA_RESULT_OK, DataViewGetField(&packed, 6, &packed));
            ASSERT_EQ(DATA_RESULT_OK, DataViewGetNumber(&packed, &health));
            ASSERT_EQ(25.0, health);
            DataQueryEnd(query);
            ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(query));
            CheckLifetimeViews(loaded, loaded_id);
            ASSERT_EQ(DATA_RESULT_OK, DataSetFieldNumber(loaded, loaded_id, 1, -1));
            ASSERT_EQ(DATA_RESULT_OK, DataResetRow(loaded, loaded_id));
            ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(loaded, loaded_id, 1, &health));
            ASSERT_EQ(history ? 42.0 : 100.0, health);
            ASSERT_EQ(DATA_RESULT_OK, DataRemoveBlob(instance));
            ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(loaded));
            DataDestroyBlob(blob);
            delete[] bytes;
            ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[0]));
            CheckLifetimeViews(store, ids[1]); // Swap removal preserves the surviving payloads.
            ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[1]));
            GetDataMemoryStats(store, &stats);
            ASSERT_EQ(0u, stats.m_BaseUsed + stats.m_PayloadUsed + stats.m_StructRows);
            ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
        }
}

// Repeated row churn and self-assignment must recycle payloads while another row
// keeps the table alive. Reset preserves originals and releases replacements.
TEST(DataLifetime, PayloadChurn)
{
    HDataStore store = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, RegisterLifetimeTable(store, DATA_RESET_RETAIN));
    DataId sentinel;
    ASSERT_EQ(DATA_RESULT_OK, CreateLifetimeRows(store, 1, &sentinel, false));
    char name[1024];
    memset(name, 'a', sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    DataMemoryStats warm;
    for (uint32_t i = 0; i < 1000; ++i)
    {
        DataId id;
        ASSERT_EQ(DATA_RESULT_OK, CreateLifetimeRows(store, 2, &id, false));
        ASSERT_EQ(DATA_RESULT_OK, DataSetFieldString(store, id, 2, name));
        const char* borrowed;
        ASSERT_EQ(DATA_RESULT_OK, DataFieldGetString(store, id, 2, &borrowed));
        ASSERT_EQ(DATA_RESULT_OK, DataSetFieldString(store, id, 2, borrowed));
        ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, id));
        ASSERT_EQ(DATA_RESULT_OK, DataSetFieldString(store, sentinel, 2, name));
        ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, sentinel));
        if (i == 9)
            GetDataMemoryStats(store, &warm);
    }
    CheckLifetimeViews(store, sentinel);
    DataMemoryStats current;
    GetDataMemoryStats(store, &current);
    ASSERT_EQ(warm.m_BaseUsed, current.m_BaseUsed);
    ASSERT_EQ(0u, current.m_PayloadUsed);
    ASSERT_LE(current.m_BaseBlocks, 2u);
    ASSERT_LE(current.m_PayloadBlocks, 1u);
    ASSERT_LE(current.m_BaseCapacity + current.m_PayloadCapacity, 3u * 4096u);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

// Compatibility caches survive row churn, but adding/removing a matching table
// changes the answer. Disjoint group filters may write the same table field.
TEST(DataLifetime, QueryCompatibilityCache)
{
    HDataStore    store = DataCreateStore();
    uint64_t      tags[] = { 1, 2 };
    DataFieldDesc field = { .m_Field = 1, .m_Type = DATA_TYPE_NUMBER };
    DataTableDesc table = { .m_Type = 1, .m_Tags = tags, .m_TagCount = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = 8 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    DataQueryField requested = { .m_Field = 1, .m_Type = DATA_TYPE_NUMBER, .m_Access = DATA_ACCESS_READ_WRITE };
    DataQueryDesc  desc = { .m_AllTags = tags, .m_AllTagCount = 1, .m_Fields = &requested, .m_FieldCount = 1 };
    HDataQuery     a, b;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &a));
    desc.m_AllTags = tags + 1;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &b));
    for (uint32_t cycle = 0; cycle < 3; ++cycle)
    {
        ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(a));
        ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(b));
        DataQueryEnd(b);
        DataQueryEnd(a);
        table.m_Type = 2;
        table.m_TagCount = 2;
        ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
        ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(a));
        ASSERT_EQ(DATA_RESULT_BUSY, DataQueryTryBegin(b));
        DataQueryEnd(a);
        ASSERT_EQ(DATA_RESULT_OK, DataUnregisterTable(store, 2));
    }
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(a));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(b));
    DataGroupId groups[] = { 7, 1, 8, 9, 1 };
    desc = { .m_GroupIds = groups, .m_GroupIdCount = 2, .m_Fields = &requested, .m_FieldCount = 1 };
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &a));
    desc.m_GroupIds = groups + 2;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &b));
    HDataQuery overlap;
    desc.m_GroupIds = groups + 3;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &overlap));
    double health = 50;
    DataId id;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateRows(store, 1, 7, 1, &health, &id));
    ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(a));
    ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(b));
    ASSERT_EQ(DATA_RESULT_BUSY, DataQueryTryBegin(overlap));
    ASSERT_EQ(DATA_RESULT_LOCKED, DataRemoveRow(store, id));
    DataQueryEnd(b);
    DataQueryEnd(a);
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, id));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(a));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(b));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(overlap));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}
