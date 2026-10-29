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
    uint8_t   m_Enabled;
    DSVector3 m_Position;
    float     m_Health;
    uint32_t  m_Color;
};

struct StorageOtherRow
{
    float     m_Health;
    DSVector3 m_Position;
};

struct StorageLight
{
    uint32_t  m_Color;
    DSVector3 m_Direction;
    float     m_Health;
};

struct StorageReferences
{
    uint8_t      m_Enabled;
    StorageLight m_Light;
    DSReference  m_Name;
    DSReference  m_Dynamic;
    DSReference  m_List;
};

static DSResult RegisterStorageRows(HDSContext store)
{
    // Declaration order differs from native offsets and query order.
    DSFieldDesc fields[] = {
        { 4, DS_TYPE_UINT32, offsetof(StorageRow, m_Color) },
        { 1, DS_TYPE_BOOLEAN, offsetof(StorageRow, m_Enabled) },
        { 2, DS_TYPE_VECTOR3, offsetof(StorageRow, m_Position) },
        { 3, DS_TYPE_FLOAT, offsetof(StorageRow, m_Health) }
    };
    DSTableDesc table = { 100, 0, 0, fields, 4, sizeof(StorageRow), DS_RESET_RETAIN };
    return DSTableAdd(store, &table);
}

// Row growth preserves query bindings and group ranges; swap removal and reset
// must keep every field and its defaults associated with the same row ID.
TEST(DataRows, GrowthRangesRemovalAndReset)
{
    HDSContext store = DSStoreCreate(0);
    DSId       group = NextTestGroupId();
    ASSERT_EQ(DS_RESULT_OK, RegisterStorageRows(store));
    DSQueryItem fields[] = {
        { 3, DS_TYPE_FLOAT, DS_FIELD_MUTABLE, DS_QUERY_ITEM_FIELD },
        { 2, DS_TYPE_VECTOR3, 0, DS_QUERY_ITEM_FIELD },
        { 4, DS_TYPE_UINT32, 0, DS_QUERY_ITEM_FIELD }
    };
    DSQueryDesc desc = { &group, 1, 0, { fields[0], fields[1], fields[2] } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(store, &desc, &query));
    DSFieldId  health_field = DSQueryFindField(query, &fields[0]);
    DSFieldId  position_field = DSQueryFindField(query, &fields[1]);
    DSFieldId  color_field = DSQueryFindField(query, &fields[2]);
    StorageRow input[36];
    DSId       ids[36];
    DSVector3  positions[33];
    uint32_t   colors[33];
    float      health[33];
    uint8_t    enabled[33];
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
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 100, 0, 3, input, ids));
    DSFieldArray columns[] = { { 3, health }, { 2, positions }, { 1, enabled }, { 4, colors } };
    ASSERT_EQ(DS_RESULT_OK, DSRowCreateSoA(store, 100, group, 33, 4, columns, ids + 3));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    ASSERT_EQ(33u, DSQueryGetRowCount(query));
    DSIterator it = DSQueryIterRange(query, 5, 11);
    uint32_t   visited = 0;
    while (DSIterNext(&it))
    {
        DSRowIterator rows = DSIterRows(&it);
        while (DSRowIterNext(&rows))
        {
            uint32_t i = 8 + visited++;
            ASSERT_EQ(ids[i], DSRowIterGetId(&rows));
            ASSERT_EQ(group, DSRowIterGetGroupId(&rows));
            ASSERT_EQ((float)i, DSRowIterGetVector3(&rows, position_field)->m_Values[0]);
            ASSERT_EQ(input[i].m_Color, *DSRowIterGetUInt32(&rows, color_field));
            *DSRowIterGetFloatMut(&rows, health_field) += 7;
        }
    }
    ASSERT_EQ(11u, visited);
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSRowRemove(store, ids[1]));
    float value;
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGetFloat(store, ids[1], 3, &value));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetFloat(store, ids[35], 3, &value));
    ASSERT_EQ(input[35].m_Health, value);
    ASSERT_EQ(DS_RESULT_OK, DSFieldSetFloat(store, ids[35], 3, -0.0f));
    ASSERT_EQ(DS_RESULT_OK, DSFieldSetUInt32(store, ids[35], 4, 0));
    ASSERT_EQ(DS_RESULT_OK, DSRowReset(store, ids[35]));
    uint32_t color;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetUInt32(store, ids[35], 4, &color));
    ASSERT_EQ(input[35].m_Color, color);
    ASSERT_EQ(DS_RESULT_OK, DSRowReset(store, ids[8]));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetFloat(store, ids[8], 3, &value));
    ASSERT_EQ(input[8].m_Health, value);
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetFloat(store, ids[9], 3, &value));
    ASSERT_EQ(input[9].m_Health + 7, value);
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetUInt32Batch(store, 33, ids + 3, 4, colors));
    for (uint32_t i = 0; i < 33; ++i)
        ASSERT_EQ(input[i + 3].m_Color, colors[i]);
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    ASSERT_EQ(33u, DSQueryGetRowCount(query));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Native row pointers must select the same bytes as field pointers for a clipped
// group range, preserve untouched fields and leave retained reset values intact.
TEST(DataRows, NativeRowPointers)
{
    HDSContext store = DSStoreCreate(0);
    DSId       group = NextTestGroupId();
    ASSERT_EQ(DS_RESULT_OK, RegisterStorageRows(store));
    StorageRow input[] = {
        { 0, { { 1, 2, 3 } }, 10, 11 },
        { 0, { { 4, 5, 6 } }, 20, 22 },
        { 0, { { 7, 8, 9 } }, 30, 33 }
    };
    DSId ids[3];
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 100, 0, 1, input, ids));
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 100, group, 2, input + 1, ids + 1));
    DSQueryDesc desc = {
        &group,
        1,
        DS_ROW_MUTABLE,
        { { 2, DS_TYPE_VECTOR3, DS_FIELD_MUTABLE, DS_QUERY_ITEM_FIELD },
          { 3, DS_TYPE_FLOAT, 0, DS_QUERY_ITEM_FIELD } }
    };
    HDSQuery query;
    ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(store, &desc, &query));
    DSFieldId position_field = DSQueryFindField(query, &desc.m_Items[0]);
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSIterator it = DSQueryIterRange(query, 1, 1);
    ASSERT_TRUE(DSIterNext(&it));
    ASSERT_EQ(100u, DSIterGetType(&it));
    DSRowIterator rows = DSIterRows(&it);
    ASSERT_TRUE(DSRowIterNext(&rows));
    const StorageRow* read = (const StorageRow*)DSRowIterGetData(&rows);
    ASSERT_EQ(input[2].m_Health, read->m_Health);
    ASSERT_EQ((const void*)&read->m_Position, (const void*)DSRowIterGetVector3(&rows, position_field));
    StorageRow* write = (StorageRow*)DSRowIterGetDataMut(&rows);
    ASSERT_EQ((const void*)read, (const void*)write);
    write->m_Position.m_Values[0] = 42;
    ASSERT_FALSE(DSRowIterNext(&rows));
    ASSERT_FALSE(DSIterNext(&it));
    DSQueryEnd(query);
    DSVector3 position;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3(store, ids[2], 2, &position));
    ASSERT_EQ(42.0f, position.m_Values[0]);
    uint32_t color;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetUInt32(store, ids[2], 4, &color));
    ASSERT_EQ(input[2].m_Color, color);
    ASSERT_EQ(DS_RESULT_OK, DSRowReset(store, ids[2]));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3(store, ids[2], 2, &position));
    ASSERT_EQ(input[2].m_Position.m_Values[0], position.m_Values[0]);
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Field-only queries must not expose native rows, even with writable fields.
// Both iterator entry points require the cached whole-row mutable flag.
TEST(DataRows, NativeRowPointerFlags)
{
    HDSContext store = DSStoreCreate(0);
    ASSERT_EQ(DS_RESULT_OK, RegisterStorageRows(store));
    StorageRow input[] = { { 0, {}, 10 }, { 0, {}, 20 } };
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 100, 0, 2, input, 0));
    DSQueryItem health = { 3, DS_TYPE_FLOAT, 0, DS_QUERY_ITEM_FIELD };
    DSQueryItem health_mut = { 3, DS_TYPE_FLOAT, DS_FIELD_MUTABLE, DS_QUERY_ITEM_FIELD };
    DSQueryDesc descriptions[] = {
        {},
        { 0, 0, 0, { health } },
        { 0, 0, 0, { health_mut } },
        { 0, 0, DS_ROW_MUTABLE }
    };
    for (uint32_t i = 0; i < 4; ++i)
    {
        HDSQuery query;
        ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(store, &descriptions[i], &query));
        for (uint32_t mode = 0; mode < 2; ++mode)
        {
            ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
            DSIterator it = mode == 0 ? DSQueryIterRange(query, 1, 1) : DSQueryIter(query);
            ASSERT_TRUE(DSIterNext(&it));
            DSRowIterator rows = DSIterRows(&it);
            ASSERT_TRUE(DSRowIterNext(&rows));
            const StorageRow* read = (const StorageRow*)DSRowIterGetData(&rows);
            StorageRow*       write = (StorageRow*)DSRowIterGetDataMut(&rows);
            ASSERT_EQ(i >= 3, read != 0);
            ASSERT_EQ(i >= 3, write != 0);
            if (read)
                ASSERT_EQ(input[mode == 0 ? 1 : 0].m_Health, read->m_Health);
            if (write)
                ASSERT_EQ((const void*)read, (const void*)write);
            DSQueryEnd(query);
        }
        ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    }
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Whole-row access must cover fields outside the filter in either admission
// order, while field-only access and read/read reservations remain independent.
TEST(DataRows, WholeRowReservations)
{
    HDSContext store = DSStoreCreate(0);
    ASSERT_EQ(DS_RESULT_OK, RegisterStorageRows(store));
    StorageRow value = {};
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 100, 0, 1, &value, 0));
    DSQueryItem position = { 2, DS_TYPE_VECTOR3, 0, DS_QUERY_ITEM_FIELD };
    DSQueryItem position_mut = { 2, DS_TYPE_VECTOR3, DS_FIELD_MUTABLE, DS_QUERY_ITEM_FIELD };
    DSQueryItem health = { 3, DS_TYPE_FLOAT, 0, DS_QUERY_ITEM_FIELD };
    DSQueryItem health_mut = { 3, DS_TYPE_FLOAT, DS_FIELD_MUTABLE, DS_QUERY_ITEM_FIELD };
    DSQueryDesc descriptions[] = {
        { 0, 0, 0, { position } },
        { 0, 0, 0, { position_mut } },
        { 0, 0, 0, { health } },
        { 0, 0, 0, { health_mut } },
        { 0, 0, DS_ROW_MUTABLE, { position } },
        {}
    };
    // Independent read/write sets: position=1, health=2, other row bytes=4.
    const uint32_t reads[] = { 1, 1, 2, 2, 7, 0 };
    const uint32_t writes[] = { 0, 1, 0, 2, 7, 0 };
    HDSQuery       queries[6];
    for (uint32_t i = 0; i < 6; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(store, &descriptions[i], &queries[i]));
    // Row access does not change the access declared for a typed field binding.
    ASSERT_EQ(UINT32_MAX, DSQueryFindField(queries[4], &position_mut));
    for (uint32_t pass = 0; pass < 2; ++pass)
    {
        for (uint32_t a = 0; a < 6; ++a)
        {
            ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(queries[a]));
            for (uint32_t b = 0; b < 6; ++b)
            {
                bool conflict = a == b || (writes[a] & reads[b]) || (writes[b] & reads[a]);
                ASSERT_EQ(conflict ? DS_RESULT_BUSY : DS_RESULT_OK, DSQueryTryBegin(queries[b]));
                if (!conflict)
                    DSQueryEnd(queries[b]);
            }
            DSQueryEnd(queries[a]);
        }
    }
    for (uint32_t i = 0; i < 6; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(queries[i]));
    DSQueryDesc invalid = { 0, 0, 1 };
    HDSQuery    query = 0;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSQueryCreate(store, &invalid, &query));
    invalid.m_Flags = UINT32_MAX;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSQueryCreate(store, &invalid, &query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Row writers still honor group filters. Cached whole-row conflicts must also
// refresh when a matching table is registered or removed after query creation.
TEST(DataRows, WholeRowGroupsAndTableChanges)
{
    HDSContext store = DSStoreCreate(0);
    ASSERT_EQ(DS_RESULT_OK, RegisterStorageRows(store));
    StorageRow value = {};
    DSId       groups[] = { NextTestGroupId(), NextTestGroupId() };
    HDSQuery   queries[2];
    for (uint32_t i = 0; i < 2; ++i)
    {
        ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 100, groups[i], 1, &value, 0));
        DSQueryDesc desc = { &groups[i], 1, DS_ROW_MUTABLE };
        ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(store, &desc, &queries[i]));
    }
    DSQueryDesc missing = { 0, 0, DS_ROW_MUTABLE, { DSTag("other") } };
    HDSQuery    other;
    ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(store, &missing, &other));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(queries[0]));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(queries[1]));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(other));
    ASSERT_EQ(0u, DSQueryGetRowCount(other));
    DSQueryEnd(other);
    DSQueryEnd(queries[1]);
    DSQueryEnd(queries[0]);

    uint64_t    tag = dmHashString64("other");
    DSFieldDesc field = { 3, DS_TYPE_FLOAT, offsetof(StorageRow, m_Health) };
    DSTableDesc table = { 200, &tag, 1, &field, 1, sizeof(StorageRow) };
    ASSERT_EQ(DS_RESULT_OK, DSTableAdd(store, &table));
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 200, groups[0], 1, &value, 0));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(queries[0]));
    ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(other));
    DSQueryEnd(queries[0]);
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(other));
    ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(queries[0]));
    DSQueryEnd(other);
    ASSERT_EQ(DS_RESULT_OK, DSTableRemove(store, 200));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(other));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(queries[0]));
    DSQueryEnd(queries[0]);
    DSQueryEnd(other);
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(other));
    for (uint32_t i = 0; i < 2; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(queries[i]));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Inline members and owned strings/containers must retain their values and
// ownership across reset and a version-1 blob roundtrip.
TEST(DataRows, ReferencesInlineFieldsAndBlobRoundtrip)
{
    HDSContext  store = DSStoreCreate(0);
    DSFieldDesc light_fields[] = {
        { dmHashString64("color"), DS_TYPE_UINT32, offsetof(StorageLight, m_Color), 0, "color" },
        { dmHashString64("direction"), DS_TYPE_VECTOR3, offsetof(StorageLight, m_Direction), 0, "direction" },
        { dmHashString64("health"), DS_TYPE_FLOAT, offsetof(StorageLight, m_Health), 0, "health" }
    };
    DSStructDesc light = { light_fields, 3, sizeof(StorageLight) };
    DSFieldDesc  fields[] = {
        { 1, DS_TYPE_BOOLEAN, offsetof(StorageReferences, m_Enabled) },
        { dmHashString64("light"), DS_TYPE_STRUCT, offsetof(StorageReferences, m_Light), &light, "light" },
        { 3, DS_TYPE_STRING, offsetof(StorageReferences, m_Name) },
        { 4, DS_TYPE_STRUCT, offsetof(StorageReferences, m_Dynamic) },
        { 5, DS_TYPE_LIST, offsetof(StorageReferences, m_List) }
    };
    DSTableDesc table = { 100, 0, 0, fields, 5, sizeof(StorageReferences), DS_RESET_RETAIN };
    ASSERT_EQ(DS_RESULT_OK, DSTableAdd(store, &table));
    StorageLight      child = { UINT32_MAX, { { 1, 2, 3 } }, -0.0f };
    DSStructInput     dynamic = { &light, &child };
    DSValueType       kinds[] = { DS_TYPE_FLOAT, DS_TYPE_UINT32, DS_TYPE_STRUCT };
    const void*       payloads[] = { &child.m_Health, &child.m_Color, &dynamic };
    DSListInput       list = { DS_TYPE_NUMBER, 3, payloads, kinds };
    char              name[] = "school";
    StorageReferences input = {};
    input.m_Enabled = 1;
    input.m_Light = child;
    input.m_Name.m_String = name;
    input.m_Dynamic.m_Struct = &dynamic;
    input.m_List.m_List = &list;
    DSId              id;
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 100, 0, 1, &input, &id));
    name[0] = 'X';
    const char* stored_name;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetString(store, id, 3, &stored_name));
    ASSERT_STREQ("school", stored_name);
    uint64_t health = dmHashString64("light.health");
    ASSERT_EQ(DS_RESULT_OK, DSFieldSetFloat(store, id, health, 12.5f));
    ASSERT_EQ(DS_RESULT_OK, DSFieldSetString(store, id, 3, "replacement"));
    ASSERT_EQ(DS_RESULT_OK, DSRowReset(store, id));
    float value;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetFloat(store, id, health, &value));
    ASSERT_EQ(0, memcmp(&value, &child.m_Health, sizeof(value)));
    DSValue object;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, dmHashString64("light"), &object));
    DSValue member;
    ASSERT_EQ(DS_RESULT_OK, DSStructGetField(&object.m_Value.m_Struct, dmHashString64("direction"), &member));
    ASSERT_EQ(0, memcmp(member.m_Value.m_Vector3, child.m_Direction.m_Values, sizeof(DSVector3)));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, 4, &object));
    ASSERT_EQ(DS_RESULT_OK, DSStructGetField(&object.m_Value.m_Struct, dmHashString64("color"), &member));
    ASSERT_EQ(UINT32_MAX, member.m_Value.m_UInt32);
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, 5, &object));
    ASSERT_EQ(DS_RESULT_OK, DSListGetValue(&object.m_Value.m_List, 1, &member));
    ASSERT_EQ(UINT32_MAX, member.m_Value.m_UInt32);
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSBlobWrite(store, 0, 0, &size));
    dmArray<uint8_t> bytes;
    bytes.SetCapacity(size);
    bytes.SetSize(size);
    ASSERT_EQ(DS_RESULT_OK, DSBlobWrite(store, bytes.Begin(), size, &size));
    HDSBlob blob;
    ASSERT_EQ(DS_RESULT_OK, DSBlobLoad(bytes.Begin(), size, &blob));
    HDSContext      loaded = DSStoreCreate(0);
    HDSBlobInstance instance;
    ASSERT_EQ(DS_RESULT_OK, DSBlobAdd(loaded, blob, 0, &instance));
    DSQueryItem query_field = { dmHashString64("light.color"), DS_TYPE_UINT32, 0, DS_QUERY_ITEM_FIELD };
    DSQueryDesc desc = { 0, 0, 0, { query_field } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(loaded, &desc, &query));
    DSFieldId field = DSQueryFindField(query, &query_field);
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSIterator it = DSQueryIter(query);
    ASSERT_TRUE(DSIterNext(&it));
    DSRowIterator rows = DSIterRows(&it);
    ASSERT_TRUE(DSRowIterNext(&rows));
    ASSERT_EQ(UINT32_MAX, *DSRowIterGetUInt32(&rows, field));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSBlobRemove(instance));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(loaded));
    DSBlobDestroy(blob);
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Tables without fields must support row creation, reset and removal.
TEST(DataRows, EmptyRows)
{
    HDSContext  store = DSStoreCreate(0);
    DSTableDesc table = { 1, 0, 0, 0, 0, 0, DS_RESET_RETAIN };
    ASSERT_EQ(DS_RESULT_OK, DSTableAdd(store, &table));
    DSId ids[2];
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 1, 0, 2, 0, ids));
    ASSERT_EQ(DS_RESULT_OK, DSRowReset(store, ids[0]));
    ASSERT_EQ(DS_RESULT_OK, DSRowRemove(store, ids[0]));
    ASSERT_EQ(DS_RESULT_OK, DSRowRemove(store, ids[1]));
    ASSERT_EQ(DS_RESULT_OK, DSTableRemove(store, 1));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// One query can cross tables with different row strides and field offsets,
// retaining field handles after growth and when a range starts inside a table.
TEST(DataRows, MixedTableQuery)
{
    HDSContext  store = DSStoreCreate(0);
    DSFieldDesc field = { 1, DS_TYPE_FLOAT, offsetof(StorageRow, m_Health) };
    DSTableDesc table = { 1, 0, 0, &field, 1, sizeof(StorageRow) };
    ASSERT_EQ(DS_RESULT_OK, DSTableAdd(store, &table));
    table.m_Type = 2;
    field.m_Offset = offsetof(StorageOtherRow, m_Health);
    table.m_RowStride = sizeof(StorageOtherRow);
    ASSERT_EQ(DS_RESULT_OK, DSTableAdd(store, &table));
    DSQueryItem requested = { 1, DS_TYPE_FLOAT, DS_FIELD_MUTABLE, DS_QUERY_ITEM_FIELD };
    DSQueryDesc desc = { 0, 0, 0, { requested } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(store, &desc, &query));
    DSFieldId       handle = DSQueryFindField(query, &requested);
    StorageRow      input[19] = {};
    StorageOtherRow other[19] = {};
    for (uint32_t i = 0; i < 19; ++i)
    {
        input[i].m_Health = (float)i;
        other[i].m_Health = (float)i;
    }
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 1, 0, 3, input, 0));
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 2, 0, 3, other + 3, 0));
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 2, 0, 16, other + 3, 0));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSIterator it = DSQueryIterRange(query, 1, 8);
    uint32_t   visited = 0;
    while (DSIterNext(&it))
    {
        DSRowIterator rows = DSIterRows(&it);
        while (DSRowIterNext(&rows))
        {
            float expected = (float)(visited < 5 ? visited + 1 : visited - 2);
            ASSERT_EQ(expected, *DSRowIterGetFloat(&rows, handle));
            *DSRowIterGetFloatMut(&rows, handle) = expected + 100;
            ++visited;
        }
    }
    ASSERT_EQ(8u, visited);
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    it = DSQueryIterRange(query, 4, 1);
    ASSERT_TRUE(DSIterNext(&it));
    DSRowIterator rows = DSIterRows(&it);
    ASSERT_TRUE(DSRowIterNext(&rows));
    ASSERT_EQ(104.0f, *DSRowIterGetFloat(&rows, handle));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}
