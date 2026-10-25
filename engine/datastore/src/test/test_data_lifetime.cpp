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
    double      m_Health;
    DSReference m_Name;
    DSReference m_Attributes;
    DSReference m_Loot;
};

struct LifetimeAttributes
{
    DSReference m_Faction;
    double      m_AlertRadius;
};

static DSResult RegisterLifetimeTable(HDSContext store, DSResetPolicy reset)
{
    DSFieldDesc fields[] = {
        { .m_Field = 1, .m_Type = DS_TYPE_NUMBER, .m_Offset = offsetof(LifetimeRow, m_Health) },
        { .m_Field = 2, .m_Type = DS_TYPE_STRING, .m_Offset = offsetof(LifetimeRow, m_Name) },
        { .m_Field = 3, .m_Type = DS_TYPE_STRUCT, .m_Offset = offsetof(LifetimeRow, m_Attributes) },
        { .m_Field = 4, .m_Type = DS_TYPE_LIST, .m_Offset = offsetof(LifetimeRow, m_Loot) }
    };
    DSTableDesc table = {
        .m_Type = 1, .m_Fields = fields, .m_FieldCount = 4, .m_RowStride = sizeof(LifetimeRow)
    };
    if (reset)
        table.m_ResetPolicy = reset;
    return DSRegisterTable(store, &table);
}

static DSResult CreateLifetimeRows(HDSContext store, DSGroupId group, DSId* ids, bool columns)
{
    DSFieldDesc members[] = {
        { .m_Field = 5, .m_Type = DS_TYPE_STRING, .m_Offset = offsetof(LifetimeAttributes, m_Faction) },
        { .m_Field = 6, .m_Type = DS_TYPE_NUMBER, .m_Offset = offsetof(LifetimeAttributes, m_AlertRadius) }
    };
    DSStructDesc       layout = { .m_Fields = members, .m_FieldCount = 2, .m_Size = sizeof(LifetimeAttributes) };
    LifetimeAttributes attributes = { .m_Faction = { .m_String = "guards" }, .m_AlertRadius = 25.0 };
    DSStructInput      object = { .m_Layout = &layout, .m_Values = &attributes };
    double             amount = 15;
    const void*        values[] = { "health_pickup", &amount, &object, 0 };
    DSValueType        types[] = { DS_TYPE_STRING, DS_TYPE_NUMBER, DS_TYPE_STRUCT, DS_TYPE_NULL };
    DSListInput        list = { .m_Count = 4, .m_Values = values, .m_Types = types };
    LifetimeRow        row = {
               .m_Health = 100, .m_Name = { .m_String = "guard" }, .m_Attributes = { .m_Struct = &object }, .m_Loot = { .m_List = &list }
    };
    if (!columns)
        return DSCreateRows(store, 1, group, 1, &row, ids);
    DSFieldArray fields[] = {
        { .m_Field = 4, .m_Values = &row.m_Loot }, { .m_Field = 3, .m_Values = &row.m_Attributes }, { .m_Field = 1, .m_Values = &row.m_Health }, { .m_Field = 2, .m_Values = &row.m_Name }
    };
    return DSCreateRowsSoA(store, 1, group, 1, 4, fields, ids);
}

static void CheckLifetimeViews(HDSContext store, DSId id)
{
    DSValueView attributes, field, list, element;
    EXPECT_EQ(DS_RESULT_OK, DSFieldGetView(store, id, 3, &attributes));
    EXPECT_EQ(DS_TYPE_STRUCT, DSViewGetType(&attributes));
    uint32_t count = 0;
    EXPECT_EQ(DS_RESULT_OK, DSViewGetCount(&attributes, &count));
    EXPECT_EQ(2u, count);
    EXPECT_EQ(DS_RESULT_OK, DSViewGetField(&attributes, 6, &field));
    double radius = 0;
    EXPECT_EQ(DS_RESULT_OK, DSViewGetNumber(&field, &radius));
    EXPECT_EQ(25.0, radius);
    EXPECT_EQ(DS_RESULT_NOT_FOUND, DSViewGetField(&attributes, 999, &field));
    EXPECT_EQ(DS_RESULT_INVALID_ARGUMENT, DSViewGetElement(&attributes, 0, &field));
    EXPECT_EQ(DS_RESULT_OK, DSFieldGetView(store, id, 4, &list));
    EXPECT_EQ(DS_RESULT_OK, DSViewGetCount(&list, &count));
    EXPECT_EQ(4u, count);
    EXPECT_EQ(DS_RESULT_OK, DSViewGetElement(&list, 0, &element));
    const char* string = 0;
    EXPECT_EQ(DS_RESULT_OK, DSViewGetString(&element, &string));
    EXPECT_STREQ("health_pickup", string);
    EXPECT_EQ(DS_RESULT_INVALID_ARGUMENT, DSViewGetNumber(&element, &radius));
    EXPECT_EQ(25.0, radius);
    EXPECT_EQ(DS_RESULT_OK, DSViewGetElement(&list, 1, &element));
    EXPECT_EQ(DS_RESULT_OK, DSViewGetNumber(&element, &radius));
    EXPECT_EQ(15.0, radius);
    EXPECT_EQ(DS_RESULT_OK, DSViewGetElement(&list, 2, &element));
    EXPECT_EQ(DS_RESULT_OK, DSViewGetField(&element, 5, &element));
    EXPECT_EQ(DS_RESULT_OK, DSViewGetString(&element, &string));
    EXPECT_STREQ("guards", string);
    EXPECT_EQ(DS_RESULT_OK, DSViewGetElement(&list, 3, &element));
    EXPECT_EQ(DS_TYPE_NULL, DSViewGetType(&element));
    EXPECT_EQ(DS_RESULT_NOT_FOUND, DSViewGetElement(&list, 4, &element));
    EXPECT_EQ(DS_TYPE_NULL, DSViewGetType(&element));

    DSValueType    expected[] = { DS_TYPE_STRING, DS_TYPE_NUMBER, DS_TYPE_STRUCT, DS_TYPE_NULL };
    DSViewIterator items = DSViewIter(&list);
    uint32_t       seen = 0;
    while (DSViewIterNext(&items, &element))
    {
        ASSERT_LT(seen, 4u);
        EXPECT_EQ(expected[seen++], DSViewGetType(&element));
        if (DSViewGetType(&element) == DS_TYPE_STRING)
        {
            EXPECT_EQ(DS_RESULT_OK, DSViewGetString(&element, &string));
            EXPECT_STREQ("health_pickup", string);
        }
        else if (DSViewGetType(&element) == DS_TYPE_NUMBER)
        {
            EXPECT_EQ(DS_RESULT_OK, DSViewGetNumber(&element, &radius));
            EXPECT_EQ(15.0, radius);
        }
        else if (DSViewGetType(&element) == DS_TYPE_STRUCT)
        {
            EXPECT_EQ(DS_RESULT_OK, DSViewGetField(&element, 5, &field));
            EXPECT_EQ(DS_RESULT_OK, DSViewGetString(&field, &string));
            EXPECT_STREQ("guards", string);
        }
    }
    EXPECT_EQ(4u, seen);
    EXPECT_FALSE(DSViewIterNext(&items, &element));
    EXPECT_EQ(DS_TYPE_NULL, DSViewGetType(&element));

    DSViewIterator members = DSViewIter(&attributes);
    uint32_t       kinds = 0;
    seen = 0;
    while (DSViewIterNext(&members, &field))
    {
        kinds |= 1u << DSViewGetType(&field);
        ++seen;
    }
    EXPECT_EQ(2u, seen);
    EXPECT_EQ((1u << DS_TYPE_STRING) | (1u << DS_TYPE_NUMBER), kinds);
}

// Native row and field-array inputs copy stack references without reset history by default.
// Views read nested members and mixed lists without exposing internals.
TEST(DataLifetime, ResetPolicyAndContainerViews)
{
    for (uint32_t history = 0; history < 2; ++history)
        for (uint32_t columns = 0; columns < 2; ++columns)
        {
            HDSContext store = DSCreateStore(0);
            ASSERT_EQ(DS_RESULT_OK, RegisterLifetimeTable(store, (DSResetPolicy)history));
            DSId ids[2];
            ASSERT_EQ(DS_RESULT_OK, CreateLifetimeRows(store, 0, ids, columns != 0));
            ASSERT_EQ(DS_RESULT_OK, CreateLifetimeRows(store, 0, ids + 1, columns != 0));
            CheckLifetimeViews(store, ids[0]);
            ASSERT_EQ(DS_RESULT_OK, DSSetFieldNumber(store, ids[0], 1, 42));
            ASSERT_EQ(DS_RESULT_OK, DSSetFieldString(store, ids[0], 2, "captain"));
            ASSERT_EQ(history ? DS_RESULT_OK : DS_RESULT_INVALID_ARGUMENT, DSResetRow(store, ids[0]));
            ASSERT_EQ(history ? DS_RESULT_OK : DS_RESULT_INVALID_ARGUMENT, DSResetField(store, ids[0], 1));
            ASSERT_EQ(history ? DS_RESULT_OK : DS_RESULT_INVALID_ARGUMENT, DSResetTable(store, 1));
            double health;
            ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, ids[0], 1, &health));
            ASSERT_EQ(history ? 100.0 : 42.0, health);
            DSMemoryStats stats;
            GetDataMemoryStats(store, &stats);
            ASSERT_EQ(history != 0, FindTable(store, 1)->m_DefaultOffset != 0);
            if (!history)
                ASSERT_EQ(0u, stats.m_BaseRowBytes + stats.m_BaseCapacity);

            DSQueryItem requested = { .m_Hash = 4, .m_Type = DS_TYPE_LIST, .m_Kind = DS_QUERY_ITEM_FIELD };
            DSQueryDesc desc = { .m_Items = { requested } };
            HDSQuery    query;
            ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
            uint32_t loot = DSQueryFindField(query, &requested);
            ASSERT_NE(UINT32_MAX, loot);
            ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
            DSIterator it = DSQueryIterRange(query, 0, DSQueryGetRowCount(query));
            uint32_t   seen = 0;
            while (DSIterNext(&it))
            {
                DSRowIterator rows = DSIterRows(&it);
                while (DSRowIterNext(&rows))
                {
                    DSValueView view;
                    DSRowIterGetView(&rows, loot, &view);
                    ASSERT_EQ(DS_RESULT_OK, DSViewGetElement(&view, 2, &view));
                    ASSERT_EQ(DS_RESULT_OK, DSViewGetField(&view, 6, &view));
                    ASSERT_EQ(DS_RESULT_OK, DSViewGetNumber(&view, &health));
                    ASSERT_EQ(25.0, health);
                    ++seen;
                }
            }
            ASSERT_EQ(2u, seen);
            DSQueryEnd(query);
            ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
            uint32_t size;
            ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, 0, 0, &size));
            uint8_t* bytes = new uint8_t[size];
            ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, bytes, size, &size));
            HDSBlob blob;
            ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
            HDSContext      loaded = DSCreateStore(0);
            HDSBlobInstance instance;
            ASSERT_EQ(DS_RESULT_OK, DSAddBlob(loaded, blob, 0, &instance));
            ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(loaded, &desc, &query));
            ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
            it = DSQueryIterRange(query, 0, DSQueryGetRowCount(query));
            DSId loaded_id;
            ASSERT_TRUE(DSIterNext(&it));
            DSRowIterator loaded_rows = DSIterRows(&it);
            ASSERT_TRUE(DSRowIterNext(&loaded_rows));
            loaded_id = DSRowIterGetId(&loaded_rows);
            DSValueView packed;
            DSRowIterGetView(&loaded_rows, DSQueryFindField(query, &requested), &packed);
            ASSERT_EQ(DS_RESULT_OK, DSViewGetElement(&packed, 2, &packed));
            ASSERT_EQ(DS_RESULT_OK, DSViewGetField(&packed, 6, &packed));
            ASSERT_EQ(DS_RESULT_OK, DSViewGetNumber(&packed, &health));
            ASSERT_EQ(25.0, health);
            DSQueryEnd(query);
            ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
            CheckLifetimeViews(loaded, loaded_id);
            ASSERT_EQ(DS_RESULT_OK, DSSetFieldNumber(loaded, loaded_id, 1, -1));
            ASSERT_EQ(DS_RESULT_OK, DSResetRow(loaded, loaded_id));
            ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(loaded, loaded_id, 1, &health));
            ASSERT_EQ(history ? 100.0 : 42.0, health);
            ASSERT_EQ(DS_RESULT_OK, DSRemoveBlob(instance));
            ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(loaded));
            DSDestroyBlob(blob);
            delete[] bytes;
            ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[0]));
            CheckLifetimeViews(store, ids[1]); // Swap removal preserves the surviving payloads.
            ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[1]));
            GetDataMemoryStats(store, &stats);
            ASSERT_EQ(0u, stats.m_BaseUsed + stats.m_PayloadUsed + stats.m_StructRows);
            ASSERT_EQ(0u, stats.m_StructRowBytes);
            ASSERT_EQ(DS_RESULT_OK, CreateLifetimeRows(store, 0, ids, columns != 0));
            CheckLifetimeViews(store, ids[0]);
            ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[0]));
            GetDataMemoryStats(store, &stats);
            ASSERT_EQ(0u, stats.m_StructRowBytes);
            ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
        }
}

// Empty containers terminate without changing the output; nested and independent
// iterators keep their own positions and visit null values as actual elements.
TEST(DataLifetime, ContainerIteratorEmptyAndNested)
{
    HDSContext store = DSCreateStore(0);
    DSFieldDesc field = { .m_Field = 1, .m_Type = DS_TYPE_LIST };
    DSTableDesc table = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(DSReference) };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    DSListInput   empty = {};
    DSStructDesc  layout = {};
    DSStructInput object = { .m_Layout = &layout };
    DSValueType   types[] = { DS_TYPE_LIST, DS_TYPE_STRUCT, DS_TYPE_NULL };
    const void*   values[] = { &empty, &object, 0 };
    DSListInput   nested = { .m_Count = 3, .m_Values = values, .m_Types = types };
    DSReference   references[] = { { .m_List = &nested }, { .m_List = &empty } };
    DSListInput   list = { .m_Type = DS_TYPE_LIST, .m_Count = 2, .m_Values = references };
    DSReference   row = { .m_List = &list };
    DSId          id;
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, 0, 1, &row, &id));
    DSValueView parent, child, element;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetView(store, id, 1, &parent));
    DSViewIterator outer = DSViewIter(&parent);
    DSViewIterator independent = DSViewIter(&parent);
    ASSERT_TRUE(DSViewIterNext(&outer, &child));
    DSViewIterator inner = DSViewIter(&child);
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_TRUE(DSViewIterNext(&inner, &element));
        ASSERT_EQ(types[i], DSViewGetType(&element));
        DSViewIterator empty_iterator = DSViewIter(&element);
        DSValueView    untouched = parent;
        ASSERT_FALSE(DSViewIterNext(&empty_iterator, &untouched));
        ASSERT_EQ(DS_TYPE_LIST, DSViewGetType(&untouched));
        uint32_t count;
        ASSERT_EQ(DS_RESULT_OK, DSViewGetCount(&untouched, &count));
        ASSERT_EQ(2u, count);
    }
    ASSERT_FALSE(DSViewIterNext(&inner, &element));
    ASSERT_EQ(DS_TYPE_NULL, DSViewGetType(&element));
    ASSERT_TRUE(DSViewIterNext(&outer, &child));
    inner = DSViewIter(&child);
    ASSERT_FALSE(DSViewIterNext(&inner, &element));
    ASSERT_FALSE(DSViewIterNext(&outer, &child));
    ASSERT_TRUE(DSViewIterNext(&independent, &child));
    uint32_t count;
    ASSERT_EQ(DS_RESULT_OK, DSViewGetCount(&child, &count));
    ASSERT_EQ(3u, count);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Repeated row churn and self-assignment must recycle payloads while another row
// keeps the table alive. Reset preserves originals and releases replacements.
TEST(DataLifetime, PayloadChurn)
{
    HDSContext store = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, RegisterLifetimeTable(store, DS_RESET_RETAIN));
    DSId sentinel;
    ASSERT_EQ(DS_RESULT_OK, CreateLifetimeRows(store, 0, &sentinel, false));
    char name[1024];
    memset(name, 'a', sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    DSMemoryStats warm;
    for (uint32_t i = 0; i < 1000; ++i)
    {
        DSId id;
        ASSERT_EQ(DS_RESULT_OK, CreateLifetimeRows(store, 0, &id, false));
        ASSERT_EQ(DS_RESULT_OK, DSSetFieldString(store, id, 2, name));
        const char* borrowed;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetString(store, id, 2, &borrowed));
        ASSERT_EQ(DS_RESULT_OK, DSSetFieldString(store, id, 2, borrowed));
        ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, id));
        ASSERT_EQ(DS_RESULT_OK, DSSetFieldString(store, sentinel, 2, name));
        ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, sentinel));
        if (i == 9)
            GetDataMemoryStats(store, &warm);
    }
    CheckLifetimeViews(store, sentinel);
    DSMemoryStats current;
    GetDataMemoryStats(store, &current);
    ASSERT_EQ(warm.m_BaseUsed, current.m_BaseUsed);
    ASSERT_EQ(0u, current.m_PayloadUsed);
    ASSERT_LE(current.m_BaseBlocks, 2u);
    ASSERT_LE(current.m_PayloadBlocks, 1u);
    ASSERT_LE(current.m_BaseCapacity + current.m_PayloadCapacity, 3u * 4096u);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Compatibility caches survive row churn, but adding/removing a matching table
// changes the answer. Disjoint group filters may write the same table field.
TEST(DataLifetime, QueryCompatibilityCache)
{
    HDSContext  store = DSCreateStore(0);
    uint64_t    tags[] = { 1, 2 };
    DSFieldDesc field = { .m_Field = 1, .m_Type = DS_TYPE_NUMBER };
    DSTableDesc table = { .m_Type = 1, .m_Tags = tags, .m_TagCount = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = 8 };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    DSQueryItem requested = { .m_Hash = 1, .m_Type = DS_TYPE_NUMBER, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryDesc desc = { .m_Items = { requested, { .m_Hash = tags[0], .m_Kind = DS_QUERY_ITEM_TAG } } };
    HDSQuery    a, b;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &a));
    desc.m_Items[1].m_Hash = tags[1];
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &b));
    for (uint32_t cycle = 0; cycle < 3; ++cycle)
    {
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(a));
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(b));
        DSQueryEnd(b);
        DSQueryEnd(a);
        table.m_Type = 2;
        table.m_TagCount = 2;
        ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(a));
        ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(b));
        DSQueryEnd(a);
        ASSERT_EQ(DS_RESULT_OK, DSUnregisterTable(store, 2));
    }
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(a));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(b));
    DSGroupId groups[] = { NextTestGroupId(), NextTestGroupId(), NextTestGroupId(), NextTestGroupId(), 0 };
    groups[4] = groups[1];
    desc = { .m_GroupIds = groups, .m_GroupIdCount = 2, .m_Items = { requested } };
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &a));
    desc.m_GroupIds = groups + 2;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &b));
    HDSQuery overlap;
    desc.m_GroupIds = groups + 3;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &overlap));
    double health = 50;
    DSId   id;
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, groups[0], 1, &health, &id));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(a));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(b));
    ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(overlap));
    ASSERT_EQ(DS_RESULT_LOCKED, DSRemoveRow(store, id));
    DSQueryEnd(b);
    DSQueryEnd(a);
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, id));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(a));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(b));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(overlap));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Owned dynamic structs, mixed lists and reset defaults are reclaimed with their group.
TEST(DataLifetime, GroupRemovalReleasesNestedPayloads)
{
    for (uint32_t history = 0; history < 2; ++history)
        for (uint32_t columns = 0; columns < 2; ++columns)
        {
            HDSContext store = DSCreateStore(0);
            ASSERT_EQ(DS_RESULT_OK, RegisterLifetimeTable(store, (DSResetPolicy)history));
            DSGroupId group = NextTestGroupId();
            DSId      id;
            ASSERT_EQ(DS_RESULT_OK, CreateLifetimeRows(store, group, &id, columns != 0));
            CheckLifetimeViews(store, id);
            ASSERT_EQ(DS_RESULT_OK, DSSetFieldString(store, id, 2, "changed"));
            ASSERT_EQ(DS_RESULT_OK, DSRemoveGroup(store, group));
            DSMemoryStats stats;
            GetDataMemoryStats(store, &stats);
            ASSERT_EQ(0u, stats.m_Rows);
            ASSERT_EQ(0u, stats.m_StructRows);
            ASSERT_EQ(0u, stats.m_BaseUsed + stats.m_PayloadUsed);
            ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
        }
}
