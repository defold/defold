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

#include <string.h>
#include <dlib/hash.h>
#include <jc_test/jc_test.h>
#include "test_data_memory.h"

struct StorageRow
{
    uint8_t          m_Enabled;
    DataStoreVector3 m_Position;
    float            m_Health;
    uint32_t         m_Color;
};

struct StorageOtherRow
{
    float            m_Health;
    DataStoreVector3 m_Position;
};

struct StorageLight
{
    uint32_t         m_Color;
    DataStoreVector3 m_Direction;
    float            m_Health;
};

struct StorageReferences
{
    uint8_t             m_Enabled;
    StorageLight        m_Light;
    DataStoreReference  m_Name;
    DataStoreReference  m_Dynamic;
    DataStoreReference  m_List;
};

static DataStoreResult RegisterStorageRows(HDataStoreContext store)
{
    // Declaration order differs from native offsets and query order.
    DataStoreFieldDesc fields[] = {
        { 4, DATASTORE_TYPE_UINT32, offsetof(StorageRow, m_Color) },
        { 1, DATASTORE_TYPE_BOOLEAN, offsetof(StorageRow, m_Enabled) },
        { 2, DATASTORE_TYPE_VECTOR3, offsetof(StorageRow, m_Position) },
        { 3, DATASTORE_TYPE_FLOAT, offsetof(StorageRow, m_Health) }
    };
    DataStoreTableDesc table = { 100, 0, 0, fields, 4, sizeof(StorageRow), DATASTORE_RESET_RETAIN };
    return DataStoreTableAdd(store, &table);
}

// Row growth preserves query bindings and group ranges; swap removal and reset
// must keep every field and its defaults associated with the same row ID.
TEST(DataRows, GrowthRangesRemovalAndReset)
{
    HDataStoreContext store = DataStoreCreate(0);
    DataStoreId       group = NextTestGroupId();
    ASSERT_EQ(DATASTORE_RESULT_OK, RegisterStorageRows(store));
    DataStoreQueryItem fields[] = {
        { 3, DATASTORE_TYPE_FLOAT, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD },
        { 2, DATASTORE_TYPE_VECTOR3, 0, DATASTORE_QUERY_ITEM_FIELD },
        { 4, DATASTORE_TYPE_UINT32, 0, DATASTORE_QUERY_ITEM_FIELD }
    };
    DataStoreQueryDesc desc = { { fields[0], fields[1], fields[2] }, &group, 1, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    DataStoreFieldId health_field = DataStoreQueryFindField(query, &fields[0]);
    DataStoreFieldId position_field = DataStoreQueryFindField(query, &fields[1]);
    DataStoreFieldId color_field = DataStoreQueryFindField(query, &fields[2]);
    StorageRow       input[36];
    DataStoreId      ids[36];
    DataStoreVector3 positions[33];
    uint32_t         colors[33];
    float            health[33];
    uint8_t          enabled[33];
    for (uint32_t i = 0; i < 36; ++i)
    {
        input[i] = { (uint8_t)(i & 1), { { (float)i, 2, 3 } }, 100.0f + i, UINT32_MAX - i };
        if (i >= 3)
        {
            positions[i - 3] = input[i].m_Position;
            colors[i - 3] = input[i].m_Color;
            health[i - 3] = input[i].m_Health;
            enabled[i - 3] = input[i].m_Enabled;
        }
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 100, 0, 3, input, ids));
    DataStoreFieldArray columns[] = { { 3, health }, { 2, positions }, { 1, enabled }, { 4, colors } };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreateSoA(store, 100, group, 33, 4, columns, ids + 3));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    ASSERT_EQ(33u, DataStoreQueryGetRowCount(query));
    DataStoreIterator it      = DataStoreQueryIterRange(query, 5, 11);
    uint32_t          visited = 0;
    while (DataStoreIterNext(&it))
    {
        DataStoreRowIterator rows = DataStoreIterRows(&it);
        while (DataStoreRowIterNext(&rows))
        {
            uint32_t i = 8 + visited++;
            ASSERT_EQ(ids[i], DataStoreRowIterGetId(&rows));
            ASSERT_EQ(group, DataStoreRowIterGetGroupId(&rows));
            ASSERT_EQ((float)i, DataStoreRowIterGetVector3(&rows, position_field)->m_Values[0]);
            ASSERT_EQ(input[i].m_Color, *DataStoreRowIterGetUInt32(&rows, color_field));
            *DataStoreRowIterGetFloatMut(&rows, health_field) += 7;
        }
    }
    ASSERT_EQ(11u, visited);
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[1]));
    float value;
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGetFloat(store, ids[1], 3, &value));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetFloat(store, ids[35], 3, &value));
    ASSERT_EQ(input[35].m_Health, value);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetFloat(store, ids[35], 3, -0.0f));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetUInt32(store, ids[35], 4, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[35]));
    uint32_t color;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetUInt32(store, ids[35], 4, &color));
    ASSERT_EQ(input[35].m_Color, color);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[8]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetFloat(store, ids[8], 3, &value));
    ASSERT_EQ(input[8].m_Health, value);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetFloat(store, ids[9], 3, &value));
    ASSERT_EQ(input[9].m_Health + 7, value);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetUInt32Batch(store, 33, ids + 3, 4, colors));
    for (uint32_t i = 0; i < 33; ++i)
    {
        ASSERT_EQ(input[i + 3].m_Color, colors[i]);
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    ASSERT_EQ(33u, DataStoreQueryGetRowCount(query));
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Native row pointers must select the same bytes as field pointers for a clipped
// group range, preserve untouched fields and leave retained reset values intact.
TEST(DataRows, NativeRowPointers)
{
    HDataStoreContext store = DataStoreCreate(0);
    DataStoreId       group = NextTestGroupId();
    ASSERT_EQ(DATASTORE_RESULT_OK, RegisterStorageRows(store));
    StorageRow input[] = {
        { 0, { { 1, 2, 3 } }, 10, 11 },
        { 0, { { 4, 5, 6 } }, 20, 22 },
        { 0, { { 7, 8, 9 } }, 30, 33 }
    };
    DataStoreId ids[3];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 100, 0, 1, input, ids));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 100, group, 2, input + 1, ids + 1));
    DataStoreQueryDesc desc = {
        { { 2, DATASTORE_TYPE_VECTOR3, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD },
            { 3, DATASTORE_TYPE_FLOAT, 0, DATASTORE_QUERY_ITEM_FIELD } },
        &group,
        1,
        DATASTORE_ROW_MUTABLE
    };
    HDataStoreQuery query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    DataStoreFieldId position_field = DataStoreQueryFindField(query, &desc.m_Items[0]);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    DataStoreIterator it = DataStoreQueryIterRange(query, 1, 1);
    ASSERT_TRUE(DataStoreIterNext(&it));
    ASSERT_EQ(100u, DataStoreIterGetType(&it));
    DataStoreRowIterator rows = DataStoreIterRows(&it);
    ASSERT_TRUE(DataStoreRowIterNext(&rows));
    const StorageRow* read = (const StorageRow*)DataStoreRowIterGetData(&rows);
    ASSERT_EQ(input[2].m_Health, read->m_Health);
    ASSERT_EQ((const void*)&read->m_Position, (const void*)DataStoreRowIterGetVector3(&rows, position_field));
    StorageRow* write = (StorageRow*)DataStoreRowIterGetDataMut(&rows);
    ASSERT_EQ((const void*)read, (const void*)write);
    write->m_Position.m_Values[0] = 42;
    ASSERT_FALSE(DataStoreRowIterNext(&rows));
    ASSERT_FALSE(DataStoreIterNext(&it));
    DataStoreQueryEnd(query);
    DataStoreVector3 position;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3(store, ids[2], 2, &position));
    ASSERT_EQ(42.0f, position.m_Values[0]);
    uint32_t color;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetUInt32(store, ids[2], 4, &color));
    ASSERT_EQ(input[2].m_Color, color);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[2]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3(store, ids[2], 2, &position));
    ASSERT_EQ(input[2].m_Position.m_Values[0], position.m_Values[0]);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Field-only queries must not expose native rows, even with writable fields.
// Both iterator entry points require the cached whole-row mutable flag.
TEST(DataRows, NativeRowPointerFlags)
{
    HDataStoreContext store = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, RegisterStorageRows(store));
    StorageRow input[] = { { 0, {}, 10 }, { 0, {}, 20 } };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 100, 0, 2, input, 0));
    DataStoreQueryItem health = { 3, DATASTORE_TYPE_FLOAT, 0, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryItem health_mut = { 3, DATASTORE_TYPE_FLOAT, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryDesc descriptions[] = {
        {},
        { { health }, 0, 0, 0 },
        { { health_mut }, 0, 0, 0 },
        { { 0 }, 0, 0, DATASTORE_ROW_MUTABLE }
    };
    for (uint32_t i = 0; i < 4; ++i)
    {
        HDataStoreQuery query;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &descriptions[i], &query));
        for (uint32_t mode = 0; mode < 2; ++mode)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
            DataStoreIterator it = mode == 0 ? DataStoreQueryIterRange(query, 1, 1) : DataStoreQueryIter(query);
            ASSERT_TRUE(DataStoreIterNext(&it));
            DataStoreRowIterator rows = DataStoreIterRows(&it);
            ASSERT_TRUE(DataStoreRowIterNext(&rows));
            const StorageRow* read = (const StorageRow*)DataStoreRowIterGetData(&rows);
            StorageRow*       write = (StorageRow*)DataStoreRowIterGetDataMut(&rows);
            ASSERT_EQ(i >= 3, read != 0);
            ASSERT_EQ(i >= 3, write != 0);
            if (read)
            {
                ASSERT_EQ(input[mode == 0 ? 1 : 0].m_Health, read->m_Health);
            }
            if (write)
            {
                ASSERT_EQ((const void*)read, (const void*)write);
            }
            DataStoreQueryEnd(query);
        }
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Whole-row access must cover fields outside the filter in either admission
// order, while field-only access and read/read reservations remain independent.
TEST(DataRows, WholeRowReservations)
{
    HDataStoreContext store = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, RegisterStorageRows(store));
    StorageRow value = {};
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 100, 0, 1, &value, 0));
    DataStoreQueryItem position = { 2, DATASTORE_TYPE_VECTOR3, 0, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryItem position_mut = { 2, DATASTORE_TYPE_VECTOR3, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryItem health = { 3, DATASTORE_TYPE_FLOAT, 0, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryItem health_mut = { 3, DATASTORE_TYPE_FLOAT, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryDesc descriptions[] = {
        { { position }, 0, 0, 0 },
        { { position_mut }, 0, 0, 0 },
        { { health }, 0, 0, 0 },
        { { health_mut }, 0, 0, 0 },
        { { position }, 0, 0, DATASTORE_ROW_MUTABLE },
        {}
    };
    // Independent read/write sets: position=1, health=2, other row bytes=4.
    const uint32_t  reads[] = { 1, 1, 2, 2, 7, 0 };
    const uint32_t  writes[] = { 0, 1, 0, 2, 7, 0 };
    HDataStoreQuery queries[6];
    for (uint32_t i = 0; i < 6; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &descriptions[i], &queries[i]));
    }
    // Row access does not change the access declared for a typed field binding.
    ASSERT_EQ(UINT32_MAX, DataStoreQueryFindField(queries[4], &position_mut));
    for (uint32_t pass = 0; pass < 2; ++pass)
    {
        for (uint32_t a = 0; a < 6; ++a)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(queries[a]));
            for (uint32_t b = 0; b < 6; ++b)
            {
                bool conflict = a == b || (writes[a] & reads[b]) || (writes[b] & reads[a]);
                ASSERT_EQ(conflict ? DATASTORE_RESULT_BUSY : DATASTORE_RESULT_OK, DataStoreQueryTryBegin(queries[b]));
                if (!conflict)
                {
                    DataStoreQueryEnd(queries[b]);
                }
            }
            DataStoreQueryEnd(queries[a]);
        }
    }
    for (uint32_t i = 0; i < 6; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(queries[i]));
    }
    DataStoreQueryDesc invalid = { { 0 }, 0, 0, 1 };
    HDataStoreQuery    query = 0;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreQueryCreate(store, &invalid, &query));
    invalid.m_Flags = UINT32_MAX;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreQueryCreate(store, &invalid, &query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Row writers still honor group filters. Cached whole-row conflicts must also
// refresh when a matching table is registered or removed after query creation.
TEST(DataRows, WholeRowGroupsAndTableChanges)
{
    HDataStoreContext store = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_OK, RegisterStorageRows(store));
    StorageRow      value = {};
    DataStoreId     groups[] = { NextTestGroupId(), NextTestGroupId() };
    HDataStoreQuery queries[2];
    for (uint32_t i = 0; i < 2; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 100, groups[i], 1, &value, 0));
        DataStoreQueryDesc desc = { { 0 }, &groups[i], 1, DATASTORE_ROW_MUTABLE };
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &queries[i]));
    }
    DataStoreQueryDesc missing = { { DataStoreTag("other") }, 0, 0, DATASTORE_ROW_MUTABLE };
    HDataStoreQuery    other;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &missing, &other));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(queries[0]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(queries[1]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(other));
    ASSERT_EQ(0u, DataStoreQueryGetRowCount(other));
    DataStoreQueryEnd(other);
    DataStoreQueryEnd(queries[1]);
    DataStoreQueryEnd(queries[0]);

    uint64_t           tag   = dmHashString64("other");
    DataStoreFieldDesc field = { 3, DATASTORE_TYPE_FLOAT, offsetof(StorageRow, m_Health) };
    DataStoreTableDesc table = { 200, &tag, 1, &field, 1, sizeof(StorageRow) };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 200, groups[0], 1, &value, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(queries[0]));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreQueryTryBegin(other));
    DataStoreQueryEnd(queries[0]);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(other));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreQueryTryBegin(queries[0]));
    DataStoreQueryEnd(other);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableRemove(store, 200));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(other));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(queries[0]));
    DataStoreQueryEnd(queries[0]);
    DataStoreQueryEnd(other);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(other));
    for (uint32_t i = 0; i < 2; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(queries[i]));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Inline members and owned strings/containers must retain their values and
// ownership across reset and a version-1 blob roundtrip.
TEST(DataRows, ReferencesInlineFieldsAndBlobRoundtrip)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreFieldDesc light_fields[] = {
        { dmHashString64("color"), DATASTORE_TYPE_UINT32, offsetof(StorageLight, m_Color), 0, "color" },
        { dmHashString64("direction"), DATASTORE_TYPE_VECTOR3, offsetof(StorageLight, m_Direction), 0, "direction" },
        { dmHashString64("health"), DATASTORE_TYPE_FLOAT, offsetof(StorageLight, m_Health), 0, "health" }
    };
    DataStoreStructDesc light = { light_fields, 3, sizeof(StorageLight) };
    DataStoreFieldDesc  fields[] = {
        { 1, DATASTORE_TYPE_BOOLEAN, offsetof(StorageReferences, m_Enabled) },
        { dmHashString64("light"), DATASTORE_TYPE_STRUCT, offsetof(StorageReferences, m_Light), &light, "light" },
        { 3, DATASTORE_TYPE_STRING, offsetof(StorageReferences, m_Name) },
        { 4, DATASTORE_TYPE_STRUCT, offsetof(StorageReferences, m_Dynamic) },
        { 5, DATASTORE_TYPE_LIST, offsetof(StorageReferences, m_List) }
    };
    DataStoreTableDesc table = { 100, 0, 0, fields, 5, sizeof(StorageReferences), DATASTORE_RESET_RETAIN };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    StorageLight         child = { UINT32_MAX, { { 1, 2, 3 } }, -0.0f };
    DataStoreStructInput dynamic = { &light, &child };
    DataStoreValueType   kinds[] = { DATASTORE_TYPE_FLOAT, DATASTORE_TYPE_UINT32, DATASTORE_TYPE_STRUCT };
    const void*          payloads[] = { &child.m_Health, &child.m_Color, &dynamic };
    DataStoreListInput   list = { DATASTORE_TYPE_NUMBER, 3, payloads, kinds };
    char                 name[] = "school";
    StorageReferences    input = {};
    input.m_Enabled = 1;
    input.m_Light = child;
    input.m_Name.m_String = name;
    input.m_Dynamic.m_Struct = &dynamic;
    input.m_List.m_List = &list;
    DataStoreId id;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 100, 0, 1, &input, &id));
    name[0] = 'X';
    const char* stored_name;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetString(store, id, 3, &stored_name));
    ASSERT_STREQ("school", stored_name);
    uint64_t health = dmHashString64("light.health");
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetFloat(store, id, health, 12.5f));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetString(store, id, 3, "replacement"));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, id));
    float value;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetFloat(store, id, health, &value));
    ASSERT_EQ(0, memcmp(&value, &child.m_Health, sizeof(value)));
    DataStoreValue object;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, id, dmHashString64("light"), &object));
    DataStoreValue member;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&object.m_Value.m_Struct, dmHashString64("direction"), &member));
    ASSERT_EQ(0, memcmp(member.m_Value.m_Vector3, child.m_Direction.m_Values, sizeof(DataStoreVector3)));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, id, 4, &object));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreStructGetField(&object.m_Value.m_Struct, dmHashString64("color"), &member));
    ASSERT_EQ(UINT32_MAX, member.m_Value.m_UInt32);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGet(store, id, 5, &object));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreListGetValue(&object.m_Value.m_List, 1, &member));
    ASSERT_EQ(UINT32_MAX, member.m_Value.m_UInt32);
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, 0, 0, &size));
    dmArray<uint8_t> bytes;
    bytes.SetCapacity(size);
    bytes.SetSize(size);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(store, bytes.Begin(), size, &size));
    HDataStoreBlob blob;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes.Begin(), size, &blob));
    HDataStoreContext      loaded = DataStoreCreate(0);
    HDataStoreBlobInstance instance;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(loaded, blob, 0, &instance));
    DataStoreQueryItem query_field = { dmHashString64("light.color"), DATASTORE_TYPE_UINT32, 0, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryDesc desc = { { query_field }, 0, 0, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(loaded, &desc, &query));
    DataStoreFieldId field = DataStoreQueryFindField(query, &query_field);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    DataStoreIterator it = DataStoreQueryIter(query);
    ASSERT_TRUE(DataStoreIterNext(&it));
    DataStoreRowIterator rows = DataStoreIterRows(&it);
    ASSERT_TRUE(DataStoreRowIterNext(&rows));
    ASSERT_EQ(UINT32_MAX, *DataStoreRowIterGetUInt32(&rows, field));
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobRemove(instance));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(loaded));
    DataStoreBlobDestroy(blob);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Tables without fields must support row creation, reset and removal.
TEST(DataRows, EmptyRows)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreTableDesc table = { 1, 0, 0, 0, 0, 0, DATASTORE_RESET_RETAIN };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    DataStoreId ids[2];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, 0, 2, 0, ids));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[0]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[0]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[1]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableRemove(store, 1));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// One query can cross tables with different row strides and field offsets,
// retaining field handles after growth and when a range starts inside a table.
TEST(DataRows, MixedTableQuery)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreFieldDesc field = { 1, DATASTORE_TYPE_FLOAT, offsetof(StorageRow, m_Health) };
    DataStoreTableDesc table = { 1, 0, 0, &field, 1, sizeof(StorageRow) };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    table.m_Type = 2;
    field.m_Offset = offsetof(StorageOtherRow, m_Health);
    table.m_RowStride = sizeof(StorageOtherRow);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    DataStoreQueryItem requested = { 1, DATASTORE_TYPE_FLOAT, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryDesc desc = { { requested }, 0, 0, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    DataStoreFieldId handle = DataStoreQueryFindField(query, &requested);
    StorageRow       input[19] = {};
    StorageOtherRow  other[19] = {};
    for (uint32_t i = 0; i < 19; ++i)
    {
        input[i].m_Health = (float)i;
        other[i].m_Health = (float)i;
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, 0, 3, input, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 2, 0, 3, other + 3, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 2, 0, 16, other + 3, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    DataStoreIterator it      = DataStoreQueryIterRange(query, 1, 8);
    uint32_t          visited = 0;
    while (DataStoreIterNext(&it))
    {
        DataStoreRowIterator rows = DataStoreIterRows(&it);
        while (DataStoreRowIterNext(&rows))
        {
            float expected = (float)(visited < 5 ? visited + 1 : visited - 2);
            ASSERT_EQ(expected, *DataStoreRowIterGetFloat(&rows, handle));
            *DataStoreRowIterGetFloatMut(&rows, handle) = expected + 100;
            ++visited;
        }
    }
    ASSERT_EQ(8u, visited);
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    it = DataStoreQueryIterRange(query, 4, 1);
    ASSERT_TRUE(DataStoreIterNext(&it));
    DataStoreRowIterator rows = DataStoreIterRows(&it);
    ASSERT_TRUE(DataStoreRowIterNext(&rows));
    ASSERT_EQ(104.0f, *DataStoreRowIterGetFloat(&rows, handle));
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}
