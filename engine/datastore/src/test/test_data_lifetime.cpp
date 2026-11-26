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
    double             m_Health;
    DataStoreReference m_Name;
    DataStoreReference m_Attributes;
    DataStoreReference m_Loot;
};

struct LifetimeAttributes
{
    DataStoreReference m_Faction;
    double             m_AlertRadius;
};

static DataStoreResult RegisterLifetimeTable(HDataStoreContext store, DataStoreResetPolicy reset)
{
    DataStoreFieldDesc fields[] = {
        { 1, DATASTORE_TYPE_NUMBER, offsetof(LifetimeRow, m_Health) },
        { 2, DATASTORE_TYPE_STRING, offsetof(LifetimeRow, m_Name) },
        { 3, DATASTORE_TYPE_STRUCT, offsetof(LifetimeRow, m_Attributes) },
        { 4, DATASTORE_TYPE_LIST, offsetof(LifetimeRow, m_Loot) }
    };
    DataStoreTableDesc table = {
        1,
        0,
        0,
        fields,
        4,
        sizeof(LifetimeRow)
    };
    if (reset)
    {
        table.m_ResetPolicy = reset;
    }
    return DataStoreTableAdd(store, &table);
}

static DataStoreResult CreateLifetimeRows(HDataStoreContext store, DataStoreId group, DataStoreId* ids, bool columns)
{
    DataStoreFieldDesc members[] = {
        { 5, DATASTORE_TYPE_STRING, offsetof(LifetimeAttributes, m_Faction) },
        { 6, DATASTORE_TYPE_NUMBER, offsetof(LifetimeAttributes, m_AlertRadius) }
    };
    DataStoreStructDesc  layout = { members, 2, sizeof(LifetimeAttributes) };
    LifetimeAttributes   attributes = { { "guards" }, 25.0 };
    DataStoreStructInput object = { &layout, &attributes };
    double               amount = 15;
    const void*          values[] = { "health_pickup", &amount, &object, 0 };
    DataStoreValueType   types[] = { DATASTORE_TYPE_STRING, DATASTORE_TYPE_NUMBER, DATASTORE_TYPE_STRUCT, DATASTORE_TYPE_NULL };
    DataStoreListInput   list = { DATASTORE_TYPE_NUMBER, 4, values, types };
    LifetimeRow          row = {};
    row.m_Health = 100;
    row.m_Name.m_String = "guard";
    row.m_Attributes.m_Struct = &object;
    row.m_Loot.m_List = &list;
    if (!columns)
    {
        return DataStoreRowCreate(store, 1, group, 1, &row, ids);
    }
    DataStoreFieldArray fields[] = {
        { 4, &row.m_Loot },
        { 3, &row.m_Attributes },
        { 1, &row.m_Health },
        { 2, &row.m_Name }
    };
    return DataStoreRowCreateSoA(store, 1, group, 1, 4, fields, ids);
}

static void CheckLifetimeViews(HDataStoreContext store, DataStoreId id)
{
    DataStoreValueView attributes, field, list, element;
    EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetView(store, id, 3, &attributes));
    EXPECT_EQ(DATASTORE_TYPE_STRUCT, DataStoreViewGetType(&attributes));
    uint32_t count = 0;
    EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetCount(&attributes, &count));
    EXPECT_EQ(2u, count);
    EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetField(&attributes, 6, &field));
    double radius = 0;
    EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetNumber(&field, &radius));
    EXPECT_EQ(25.0, radius);
    EXPECT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreViewGetField(&attributes, 999, &field));
    EXPECT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreViewGetElement(&attributes, 0, &field));
    EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetView(store, id, 4, &list));
    EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetCount(&list, &count));
    EXPECT_EQ(4u, count);
    EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetElement(&list, 0, &element));
    const char* string = 0;
    EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetString(&element, &string));
    EXPECT_STREQ("health_pickup", string);
    EXPECT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreViewGetNumber(&element, &radius));
    EXPECT_EQ(25.0, radius);
    EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetElement(&list, 1, &element));
    EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetNumber(&element, &radius));
    EXPECT_EQ(15.0, radius);
    EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetElement(&list, 2, &element));
    EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetField(&element, 5, &element));
    EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetString(&element, &string));
    EXPECT_STREQ("guards", string);
    EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetElement(&list, 3, &element));
    EXPECT_EQ(DATASTORE_TYPE_NULL, DataStoreViewGetType(&element));
    EXPECT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreViewGetElement(&list, 4, &element));
    EXPECT_EQ(DATASTORE_TYPE_NULL, DataStoreViewGetType(&element));

    DataStoreValueType    expected[] = { DATASTORE_TYPE_STRING, DATASTORE_TYPE_NUMBER, DATASTORE_TYPE_STRUCT, DATASTORE_TYPE_NULL };
    DataStoreViewIterator items = DataStoreViewIter(&list);
    uint32_t              seen = 0;
    while (DataStoreViewIterNext(&items, &element))
    {
        ASSERT_LT(seen, 4u);
        EXPECT_EQ(expected[seen++], DataStoreViewGetType(&element));
        if (DataStoreViewGetType(&element) == DATASTORE_TYPE_STRING)
        {
            EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetString(&element, &string));
            EXPECT_STREQ("health_pickup", string);
        }
        else if (DataStoreViewGetType(&element) == DATASTORE_TYPE_NUMBER)
        {
            EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetNumber(&element, &radius));
            EXPECT_EQ(15.0, radius);
        }
        else if (DataStoreViewGetType(&element) == DATASTORE_TYPE_STRUCT)
        {
            EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetField(&element, 5, &field));
            EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetString(&field, &string));
            EXPECT_STREQ("guards", string);
        }
    }
    EXPECT_EQ(4u, seen);
    EXPECT_FALSE(DataStoreViewIterNext(&items, &element));
    EXPECT_EQ(DATASTORE_TYPE_NULL, DataStoreViewGetType(&element));

    DataStoreViewIterator members = DataStoreViewIter(&attributes);
    uint32_t              kinds   = 0;
    seen = 0;
    while (DataStoreViewIterNext(&members, &field))
    {
        kinds |= 1u << DataStoreViewGetType(&field);
        ++seen;
    }
    EXPECT_EQ(2u, seen);
    EXPECT_EQ((1u << DATASTORE_TYPE_STRING) | (1u << DATASTORE_TYPE_NUMBER), kinds);
}

// Native row and field-array inputs copy stack references without reset history by default.
// Views read nested members and mixed lists without exposing internals.
TEST(DataLifetime, ResetPolicyAndContainerViews)
{
    for (uint32_t history = 0; history < 2; ++history)
    {
        for (uint32_t columns = 0; columns < 2; ++columns)
        {
            HDataStoreContext store = DataStoreCreate(0);
            ASSERT_EQ(DATASTORE_RESULT_OK, RegisterLifetimeTable(store, (DataStoreResetPolicy)history));
            DataStoreId ids[2];
            ASSERT_EQ(DATASTORE_RESULT_OK, CreateLifetimeRows(store, 0, ids, columns != 0));
            ASSERT_EQ(DATASTORE_RESULT_OK, CreateLifetimeRows(store, 0, ids + 1, columns != 0));
            CheckLifetimeViews(store, ids[0]);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetNumber(store, ids[0], 1, 42));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetString(store, ids[0], 2, "captain"));
            ASSERT_EQ(history ? DATASTORE_RESULT_OK : DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowReset(store, ids[0]));
            ASSERT_EQ(history ? DATASTORE_RESULT_OK : DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFieldReset(store, ids[0], 1));
            ASSERT_EQ(history ? DATASTORE_RESULT_OK : DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreTableReset(store, 1));
            double health;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, ids[0], 1, &health));
            ASSERT_EQ(history ? 100.0 : 42.0, health);
            DataStoreMemoryStats stats;
            GetDataMemoryStats(store, &stats);
            ASSERT_EQ(history != 0, FindTable(store, 1)->m_DefaultOffset != 0);
            if (!history)
            {
                ASSERT_EQ(0u, stats.m_BaseRowBytes + stats.m_BaseCapacity);
            }

            DataStoreQueryItem requested = { 4, DATASTORE_TYPE_LIST, 0, DATASTORE_QUERY_ITEM_FIELD };
            DataStoreQueryDesc desc = { { requested }, 0, 0, 0 };
            HDataStoreQuery    query;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
            DataStoreFieldId loot = DataStoreQueryFindField(query, &requested);
            ASSERT_NE(UINT32_MAX, loot);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
            DataStoreIterator it   = DataStoreQueryIterRange(query, 0, DataStoreQueryGetRowCount(query));
            uint32_t          seen = 0;
            while (DataStoreIterNext(&it))
            {
                DataStoreRowIterator rows = DataStoreIterRows(&it);
                while (DataStoreRowIterNext(&rows))
                {
                    DataStoreValueView view;
                    DataStoreRowIterGetView(&rows, loot, &view);
                    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetElement(&view, 2, &view));
                    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetField(&view, 6, &view));
                    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetNumber(&view, &health));
                    ASSERT_EQ(25.0, health);
                    ++seen;
                }
            }
            ASSERT_EQ(2u, seen);
            DataStoreQueryEnd(query);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
            uint32_t size;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, 0, 0, &size));
            uint8_t* bytes = new uint8_t[size];
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, bytes, size, &size));
            HDataStoreBlob blob;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
            HDataStoreContext      loaded = DataStoreCreate(0);
            HDataStoreBlobInstance instance;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(loaded, blob, 0, &instance));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(loaded, &desc, &query));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
            it = DataStoreQueryIterRange(query, 0, DataStoreQueryGetRowCount(query));
            DataStoreId loaded_id;
            ASSERT_TRUE(DataStoreIterNext(&it));
            DataStoreRowIterator loaded_rows = DataStoreIterRows(&it);
            ASSERT_TRUE(DataStoreRowIterNext(&loaded_rows));
            loaded_id = DataStoreRowIterGetId(&loaded_rows);
            DataStoreValueView packed;
            DataStoreRowIterGetView(&loaded_rows, DataStoreQueryFindField(query, &requested), &packed);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetElement(&packed, 2, &packed));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetField(&packed, 6, &packed));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetNumber(&packed, &health));
            ASSERT_EQ(25.0, health);
            DataStoreQueryEnd(query);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
            CheckLifetimeViews(loaded, loaded_id);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetNumber(loaded, loaded_id, 1, -1));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(loaded, loaded_id));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(loaded, loaded_id, 1, &health));
            ASSERT_EQ(history ? 100.0 : 42.0, health);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobRemove(instance));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(loaded));
            DataStoreBlobDestroy(blob);
            delete[] bytes;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[0]));
            CheckLifetimeViews(store, ids[1]); // Swap removal preserves the surviving payloads.
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[1]));
            GetDataMemoryStats(store, &stats);
            ASSERT_EQ(0u, stats.m_BaseUsed + stats.m_PayloadUsed + stats.m_StructRows);
            ASSERT_EQ(0u, stats.m_StructRowBytes);
            ASSERT_EQ(DATASTORE_RESULT_OK, CreateLifetimeRows(store, 0, ids, columns != 0));
            CheckLifetimeViews(store, ids[0]);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[0]));
            GetDataMemoryStats(store, &stats);
            ASSERT_EQ(0u, stats.m_StructRowBytes);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
        }
    }
}

// Empty containers terminate without changing the output; nested and independent
// iterators keep their own positions and visit null values as actual elements.
TEST(DataLifetime, ContainerIteratorEmptyAndNested)
{
    HDataStoreContext store = DataStoreCreate(0);
    DataStoreFieldDesc field = { 1, DATASTORE_TYPE_LIST };
    DataStoreTableDesc table = { 1, 0, 0, &field, 1, sizeof(DataStoreReference) };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    DataStoreListInput   empty = {};
    DataStoreStructDesc  layout = {};
    DataStoreStructInput object = { &layout };
    DataStoreValueType   types[] = { DATASTORE_TYPE_LIST, DATASTORE_TYPE_STRUCT, DATASTORE_TYPE_NULL };
    const void*          values[] = { &empty, &object, 0 };
    DataStoreListInput   nested = { DATASTORE_TYPE_NUMBER, 3, values, types };
    DataStoreReference   references[2] = {};
    references[0].m_List = &nested;
    references[1].m_List = &empty;
    DataStoreListInput   list = { DATASTORE_TYPE_LIST, 2, references };
    DataStoreReference   row = {};
    row.m_List = &list;
    DataStoreId id;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, 0, 1, &row, &id));
    DataStoreValueView parent, child, element;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetView(store, id, 1, &parent));
    DataStoreViewIterator outer = DataStoreViewIter(&parent);
    DataStoreViewIterator independent = DataStoreViewIter(&parent);
    ASSERT_TRUE(DataStoreViewIterNext(&outer, &child));
    DataStoreViewIterator inner = DataStoreViewIter(&child);
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_TRUE(DataStoreViewIterNext(&inner, &element));
        ASSERT_EQ(types[i], DataStoreViewGetType(&element));
        DataStoreViewIterator empty_iterator = DataStoreViewIter(&element);
        DataStoreValueView    untouched = parent;
        ASSERT_FALSE(DataStoreViewIterNext(&empty_iterator, &untouched));
        ASSERT_EQ(DATASTORE_TYPE_LIST, DataStoreViewGetType(&untouched));
        uint32_t count;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetCount(&untouched, &count));
        ASSERT_EQ(2u, count);
    }
    ASSERT_FALSE(DataStoreViewIterNext(&inner, &element));
    ASSERT_EQ(DATASTORE_TYPE_NULL, DataStoreViewGetType(&element));
    ASSERT_TRUE(DataStoreViewIterNext(&outer, &child));
    inner = DataStoreViewIter(&child);
    ASSERT_FALSE(DataStoreViewIterNext(&inner, &element));
    ASSERT_FALSE(DataStoreViewIterNext(&outer, &child));
    ASSERT_TRUE(DataStoreViewIterNext(&independent, &child));
    uint32_t count;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreViewGetCount(&child, &count));
    ASSERT_EQ(3u, count);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Repeated row churn and self-assignment must recycle payloads while another row
// keeps the table alive. Reset preserves originals and releases replacements.
TEST(DataLifetime, PayloadChurn)
{
    HDataStoreContext store = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, RegisterLifetimeTable(store, DATASTORE_RESET_RETAIN));
    DataStoreId sentinel;
    ASSERT_EQ(DATASTORE_RESULT_OK, CreateLifetimeRows(store, 0, &sentinel, false));
    char name[1024];
    memset(name, 'a', sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    DataStoreMemoryStats warm;
    for (uint32_t i = 0; i < 1000; ++i)
    {
        DataStoreId id;
        ASSERT_EQ(DATASTORE_RESULT_OK, CreateLifetimeRows(store, 0, &id, false));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetString(store, id, 2, name));
        const char* borrowed;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetString(store, id, 2, &borrowed));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetString(store, id, 2, borrowed));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, id));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetString(store, sentinel, 2, name));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, sentinel));
        if (i == 9)
        {
            GetDataMemoryStats(store, &warm);
        }
    }
    CheckLifetimeViews(store, sentinel);
    DataStoreMemoryStats current;
    GetDataMemoryStats(store, &current);
    ASSERT_EQ(warm.m_BaseUsed, current.m_BaseUsed);
    ASSERT_EQ(0u, current.m_PayloadUsed);
    ASSERT_LE(current.m_BaseBlocks, 2u);
    ASSERT_LE(current.m_PayloadBlocks, 1u);
    ASSERT_LE(current.m_BaseCapacity + current.m_PayloadCapacity, 3u * 4096u);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Compatibility caches survive row churn, but adding/removing a matching table
// changes the answer. Disjoint group filters may write the same table field.
TEST(DataLifetime, QueryCompatibilityCache)
{
    HDataStoreContext  store = DataStoreCreate(0);
    uint64_t           tags[] = { 1, 2 };
    DataStoreFieldDesc field = { 1, DATASTORE_TYPE_NUMBER };
    DataStoreTableDesc table = { 1, tags, 1, &field, 1, 8 };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    DataStoreQueryItem requested = { 1, DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryDesc desc = { { requested, { tags[0], DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_TAG } }, 0, 0, 0 };
    HDataStoreQuery    a, b;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &a));
    desc.m_Items[1].m_Hash = tags[1];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &b));
    for (uint32_t cycle = 0; cycle < 3; ++cycle)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(a));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(b));
        DataStoreQueryEnd(b);
        DataStoreQueryEnd(a);
        table.m_Type = 2;
        table.m_TagCount = 2;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(a));
        ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreQueryTryBegin(b));
        DataStoreQueryEnd(a);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableRemove(store, 2));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(a));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(b));
    DataStoreId groups[] = { NextTestGroupId(), NextTestGroupId(), NextTestGroupId(), NextTestGroupId(), 0 };
    groups[4] = groups[1];
    desc = { { requested }, groups, 2, 0 };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &a));
    desc.m_GroupIds = groups + 2;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &b));
    HDataStoreQuery overlap;
    desc.m_GroupIds = groups + 3;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &overlap));
    double      health = 50;
    DataStoreId id;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, groups[0], 1, &health, &id));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(a));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(b));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreQueryTryBegin(overlap));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreRowRemove(store, id));
    DataStoreQueryEnd(b);
    DataStoreQueryEnd(a);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, id));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(a));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(b));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(overlap));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Owned dynamic structs, mixed lists and reset defaults are reclaimed with their group.
TEST(DataLifetime, GroupRemovalReleasesNestedPayloads)
{
    for (uint32_t history = 0; history < 2; ++history)
    {
        for (uint32_t columns = 0; columns < 2; ++columns)
        {
            HDataStoreContext store = DataStoreCreate(0);
            ASSERT_EQ(DATASTORE_RESULT_OK, RegisterLifetimeTable(store, (DataStoreResetPolicy)history));
            DataStoreId      group = NextTestGroupId();
            DataStoreId      id;
            ASSERT_EQ(DATASTORE_RESULT_OK, CreateLifetimeRows(store, group, &id, columns != 0));
            CheckLifetimeViews(store, id);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetString(store, id, 2, "changed"));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreGroupRemove(store, group));
            DataStoreMemoryStats stats;
            GetDataMemoryStats(store, &stats);
            ASSERT_EQ(0u, stats.m_Rows);
            ASSERT_EQ(0u, stats.m_StructRows);
            ASSERT_EQ(0u, stats.m_BaseUsed + stats.m_PayloadUsed);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
        }
    }
}
