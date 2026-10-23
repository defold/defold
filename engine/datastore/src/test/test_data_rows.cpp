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
        { .m_Field = 4, .m_Type = DS_TYPE_UINT32, .m_Offset = offsetof(StorageRow, m_Color) },
        { .m_Field = 1, .m_Type = DS_TYPE_BOOLEAN, .m_Offset = offsetof(StorageRow, m_Enabled) },
        { .m_Field = 2, .m_Type = DS_TYPE_VECTOR3, .m_Offset = offsetof(StorageRow, m_Position) },
        { .m_Field = 3, .m_Type = DS_TYPE_FLOAT, .m_Offset = offsetof(StorageRow, m_Health) }
    };
    DSTableDesc table = { .m_Type = 100, .m_Fields = fields, .m_FieldCount = 4, .m_RowStride = sizeof(StorageRow), .m_ResetPolicy = DS_RESET_RETAIN };
    return DSRegisterTable(store, &table);
}

// Row growth preserves query bindings and group ranges; swap removal and reset
// must keep every field and its defaults associated with the same row ID.
TEST(DataRows, GrowthRangesRemovalAndReset)
{
    HDSContext store = DSCreateStore(0);
    DSGroupId  group = CreateTestGroup(store);
    ASSERT_EQ(DS_RESULT_OK, RegisterStorageRows(store));
    DSQueryItem fields[] = {
        { .m_Hash = 3, .m_Type = DS_TYPE_FLOAT, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD },
        { .m_Hash = 2, .m_Type = DS_TYPE_VECTOR3, .m_Kind = DS_QUERY_ITEM_FIELD },
        { .m_Hash = 4, .m_Type = DS_TYPE_UINT32, .m_Kind = DS_QUERY_ITEM_FIELD }
    };
    DSQueryDesc desc = { .m_GroupIds = &group, .m_GroupIdCount = 1, .m_Items = { fields[0], fields[1], fields[2] } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    uint32_t   health_field = DSQueryFindField(query, &fields[0]);
    uint32_t   position_field = DSQueryFindField(query, &fields[1]);
    uint32_t   color_field = DSQueryFindField(query, &fields[2]);
    StorageRow input[36];
    DSId       ids[36];
    DSVector3  positions[33];
    uint32_t   colors[33];
    float      health[33];
    uint8_t    enabled[33];
    for (uint32_t i = 0; i < 36; ++i)
    {
        input[i] = { .m_Enabled = (uint8_t)(i & 1), .m_Position = { { (float)i, 2, 3 } }, .m_Health = 100.0f + i, .m_Color = UINT32_MAX - i };
        if (i >= 3)
        {
            positions[i - 3] = input[i].m_Position;
            colors[i - 3] = input[i].m_Color;
            health[i - 3] = input[i].m_Health;
            enabled[i - 3] = input[i].m_Enabled;
        }
    }
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 100, 0, 3, input, ids));
    DSFieldArray columns[] = { { 3, health }, { 2, positions }, { 1, enabled }, { 4, colors } };
    ASSERT_EQ(DS_RESULT_OK, DSCreateRowsSoA(store, 100, group, 33, 4, columns, ids + 3));
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
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[1]));
    float value;
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGetFloat(store, ids[1], 3, &value));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetFloat(store, ids[35], 3, &value));
    ASSERT_EQ(input[35].m_Health, value);
    ASSERT_EQ(DS_RESULT_OK, DSSetFieldFloat(store, ids[35], 3, -0.0f));
    ASSERT_EQ(DS_RESULT_OK, DSSetFieldUInt32(store, ids[35], 4, 0));
    ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, ids[35]));
    uint32_t color;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetUInt32(store, ids[35], 4, &color));
    ASSERT_EQ(input[35].m_Color, color);
    ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, ids[8]));
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
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Native row pointers must select the same bytes as field pointers for a clipped
// group range, preserve untouched fields and leave retained reset values intact.
TEST(DataRows, NativeRowPointers)
{
    HDSContext store = DSCreateStore(0);
    DSGroupId  group = CreateTestGroup(store);
    ASSERT_EQ(DS_RESULT_OK, RegisterStorageRows(store));
    StorageRow input[] = {
        { .m_Position = { { 1, 2, 3 } }, .m_Health = 10, .m_Color = 11 },
        { .m_Position = { { 4, 5, 6 } }, .m_Health = 20, .m_Color = 22 },
        { .m_Position = { { 7, 8, 9 } }, .m_Health = 30, .m_Color = 33 }
    };
    DSId ids[3];
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 100, 0, 1, input, ids));
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 100, group, 2, input + 1, ids + 1));
    DSQueryDesc desc = {
        .m_GroupIds = &group,
        .m_GroupIdCount = 1,
        .m_Flags = DS_ROW_MUTABLE,
        .m_Items = {
        { .m_Hash = 2, .m_Type = DS_TYPE_VECTOR3, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD },
        { .m_Hash = 3, .m_Type = DS_TYPE_FLOAT, .m_Kind = DS_QUERY_ITEM_FIELD } }
    };
    HDSQuery query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    uint32_t position_field = DSQueryFindField(query, &desc.m_Items[0]);
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
    ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, ids[2]));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3(store, ids[2], 2, &position));
    ASSERT_EQ(input[2].m_Position.m_Values[0], position.m_Values[0]);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Field-only queries must not expose native rows, even with writable fields.
// Both iterator entry points require the cached whole-row mutable flag.
TEST(DataRows, NativeRowPointerFlags)
{
    HDSContext store = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, RegisterStorageRows(store));
    StorageRow input[] = { { .m_Health = 10 }, { .m_Health = 20 } };
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 100, 0, 2, input, 0));
    DSQueryItem health = { .m_Hash = 3, .m_Type = DS_TYPE_FLOAT, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryItem health_mut = { .m_Hash = 3, .m_Type = DS_TYPE_FLOAT, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryDesc descriptions[] = {
        {},
        { .m_Items = { health } },
        { .m_Items = { health_mut } },
        { .m_Flags = DS_ROW_MUTABLE }
    };
    for (uint32_t i = 0; i < 4; ++i)
    {
        HDSQuery query;
        ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &descriptions[i], &query));
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
        ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    }
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Whole-row access must cover fields outside the filter in either admission
// order, while field-only access and read/read reservations remain independent.
TEST(DataRows, WholeRowReservations)
{
    HDSContext store = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, RegisterStorageRows(store));
    StorageRow value = {};
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 100, 0, 1, &value, 0));
    DSQueryItem position = { .m_Hash = 2, .m_Type = DS_TYPE_VECTOR3, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryItem position_mut = { .m_Hash = 2, .m_Type = DS_TYPE_VECTOR3, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryItem health = { .m_Hash = 3, .m_Type = DS_TYPE_FLOAT, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryItem health_mut = { .m_Hash = 3, .m_Type = DS_TYPE_FLOAT, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryDesc descriptions[] = {
        { .m_Items = { position } },
        { .m_Items = { position_mut } },
        { .m_Items = { health } },
        { .m_Items = { health_mut } },
        { .m_Flags = DS_ROW_MUTABLE, .m_Items = { position } },
        {}
    };
    // Independent read/write sets: position=1, health=2, other row bytes=4.
    const uint32_t reads[] = { 1, 1, 2, 2, 7, 0 };
    const uint32_t writes[] = { 0, 1, 0, 2, 7, 0 };
    HDSQuery       queries[6];
    for (uint32_t i = 0; i < 6; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &descriptions[i], &queries[i]));
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
        ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(queries[i]));
    DSQueryDesc invalid = { .m_Flags = 1 };
    HDSQuery    query = 0;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSCreateQuery(store, &invalid, &query));
    invalid.m_Flags = UINT32_MAX;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSCreateQuery(store, &invalid, &query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Row writers still honor group filters. Cached whole-row conflicts must also
// refresh when a matching table is registered or removed after query creation.
TEST(DataRows, WholeRowGroupsAndTableChanges)
{
    HDSContext store = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_OK, RegisterStorageRows(store));
    StorageRow value = {};
    DSGroupId  groups[] = { CreateTestGroup(store), CreateTestGroup(store) };
    HDSQuery   queries[2];
    for (uint32_t i = 0; i < 2; ++i)
    {
        ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 100, groups[i], 1, &value, 0));
        DSQueryDesc desc = { .m_GroupIds = &groups[i], .m_GroupIdCount = 1, .m_Flags = DS_ROW_MUTABLE };
        ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &queries[i]));
    }
    DSQueryDesc missing = { .m_Flags = DS_ROW_MUTABLE, .m_Items = { DSTag("other") } };
    HDSQuery    other;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &missing, &other));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(queries[0]));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(queries[1]));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(other));
    ASSERT_EQ(0u, DSQueryGetRowCount(other));
    DSQueryEnd(other);
    DSQueryEnd(queries[1]);
    DSQueryEnd(queries[0]);

    uint64_t    tag = dmHashString64("other");
    DSFieldDesc field = { .m_Field = 3, .m_Type = DS_TYPE_FLOAT, .m_Offset = offsetof(StorageRow, m_Health) };
    DSTableDesc table = { .m_Type = 200, .m_Tags = &tag, .m_TagCount = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(StorageRow) };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 200, groups[0], 1, &value, 0));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(queries[0]));
    ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(other));
    DSQueryEnd(queries[0]);
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(other));
    ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(queries[0]));
    DSQueryEnd(other);
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterTable(store, 200));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(other));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(queries[0]));
    DSQueryEnd(queries[0]);
    DSQueryEnd(other);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(other));
    for (uint32_t i = 0; i < 2; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(queries[i]));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Inline members and owned strings/containers must retain their values and
// ownership across reset and a version-1 blob roundtrip.
TEST(DataRows, ReferencesInlineFieldsAndBlobRoundtrip)
{
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc light_fields[] = {
        { .m_Field = dmHashString64("color"), .m_Type = DS_TYPE_UINT32, .m_Offset = offsetof(StorageLight, m_Color), .m_Name = "color" },
        { .m_Field = dmHashString64("direction"), .m_Type = DS_TYPE_VECTOR3, .m_Offset = offsetof(StorageLight, m_Direction), .m_Name = "direction" },
        { .m_Field = dmHashString64("health"), .m_Type = DS_TYPE_FLOAT, .m_Offset = offsetof(StorageLight, m_Health), .m_Name = "health" }
    };
    DSStructDesc light = { .m_Fields = light_fields, .m_FieldCount = 3, .m_Size = sizeof(StorageLight) };
    DSFieldDesc  fields[] = {
        { .m_Field = 1, .m_Type = DS_TYPE_BOOLEAN, .m_Offset = offsetof(StorageReferences, m_Enabled) },
        { .m_Field = dmHashString64("light"), .m_Type = DS_TYPE_STRUCT, .m_Offset = offsetof(StorageReferences, m_Light), .m_Struct = &light, .m_Name = "light" },
        { .m_Field = 3, .m_Type = DS_TYPE_STRING, .m_Offset = offsetof(StorageReferences, m_Name) },
        { .m_Field = 4, .m_Type = DS_TYPE_STRUCT, .m_Offset = offsetof(StorageReferences, m_Dynamic) },
        { .m_Field = 5, .m_Type = DS_TYPE_LIST, .m_Offset = offsetof(StorageReferences, m_List) }
    };
    DSTableDesc table = { .m_Type = 100, .m_Fields = fields, .m_FieldCount = 5, .m_RowStride = sizeof(StorageReferences), .m_ResetPolicy = DS_RESET_RETAIN };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    StorageLight      child = { .m_Color = UINT32_MAX, .m_Direction = { { 1, 2, 3 } }, .m_Health = -0.0f };
    DSStructInput     dynamic = { .m_Layout = &light, .m_Values = &child };
    DSValueType       kinds[] = { DS_TYPE_FLOAT, DS_TYPE_UINT32, DS_TYPE_STRUCT };
    const void*       payloads[] = { &child.m_Health, &child.m_Color, &dynamic };
    DSListInput       list = { .m_Count = 3, .m_Values = payloads, .m_Types = kinds };
    char              name[] = "school";
    StorageReferences input = { .m_Enabled = 1, .m_Light = child, .m_Name = { .m_String = name }, .m_Dynamic = { .m_Struct = &dynamic }, .m_List = { .m_List = &list } };
    DSId              id;
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 100, 0, 1, &input, &id));
    name[0] = 'X';
    const char* stored_name;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetString(store, id, 3, &stored_name));
    ASSERT_STREQ("school", stored_name);
    uint64_t health = dmHashString64("light.health");
    ASSERT_EQ(DS_RESULT_OK, DSSetFieldFloat(store, id, health, 12.5f));
    ASSERT_EQ(DS_RESULT_OK, DSSetFieldString(store, id, 3, "replacement"));
    ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, id));
    float value;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetFloat(store, id, health, &value));
    ASSERT_EQ(0, memcmp(&value, &child.m_Health, sizeof(value)));
    DSValue object;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, dmHashString64("light"), &object));
    DSValue member;
    ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&object.m_Value.m_Struct, dmHashString64("direction"), &member));
    ASSERT_EQ(0, memcmp(member.m_Value.m_Vector3, child.m_Direction.m_Values, sizeof(DSVector3)));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, 4, &object));
    ASSERT_EQ(DS_RESULT_OK, DSGetStructField(&object.m_Value.m_Struct, dmHashString64("color"), &member));
    ASSERT_EQ(UINT32_MAX, member.m_Value.m_UInt32);
    ASSERT_EQ(DS_RESULT_OK, DSFieldGet(store, id, 5, &object));
    ASSERT_EQ(DS_RESULT_OK, DSGetListValue(&object.m_Value.m_List, 1, &member));
    ASSERT_EQ(UINT32_MAX, member.m_Value.m_UInt32);
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, 0, 0, &size));
    dmArray<uint8_t> bytes;
    bytes.SetCapacity(size);
    bytes.SetSize(size);
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(store, bytes.Begin(), size, &size));
    HDSBlob blob;
    ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes.Begin(), size, &blob));
    HDSContext      loaded = DSCreateStore(0);
    HDSBlobInstance instance;
    ASSERT_EQ(DS_RESULT_OK, DSAddBlob(loaded, blob, 0, &instance));
    DSQueryItem query_field = { .m_Hash = dmHashString64("light.color"), .m_Type = DS_TYPE_UINT32, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryDesc desc = { .m_Items = { query_field } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(loaded, &desc, &query));
    uint32_t field = DSQueryFindField(query, &query_field);
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSIterator it = DSQueryIter(query);
    ASSERT_TRUE(DSIterNext(&it));
    DSRowIterator rows = DSIterRows(&it);
    ASSERT_TRUE(DSRowIterNext(&rows));
    ASSERT_EQ(UINT32_MAX, *DSRowIterGetUInt32(&rows, field));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveBlob(instance));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(loaded));
    DSDestroyBlob(blob);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Tables without fields must support row creation, reset and removal.
TEST(DataRows, EmptyRows)
{
    HDSContext  store = DSCreateStore(0);
    DSTableDesc table = { .m_Type = 1, .m_ResetPolicy = DS_RESET_RETAIN };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    DSId ids[2];
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, 0, 2, 0, ids));
    ASSERT_EQ(DS_RESULT_OK, DSResetRow(store, ids[0]));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[0]));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[1]));
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterTable(store, 1));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// One query can cross tables with different row strides and field offsets,
// retaining field handles after growth and when a range starts inside a table.
TEST(DataRows, MixedTableQuery)
{
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc field = { .m_Field = 1, .m_Type = DS_TYPE_FLOAT, .m_Offset = offsetof(StorageRow, m_Health) };
    DSTableDesc table = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(StorageRow) };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    table.m_Type = 2;
    field.m_Offset = offsetof(StorageOtherRow, m_Health);
    table.m_RowStride = sizeof(StorageOtherRow);
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    DSQueryItem requested = { .m_Hash = 1, .m_Type = DS_TYPE_FLOAT, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryDesc desc = { .m_Items = { requested } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    uint32_t        handle = DSQueryFindField(query, &requested);
    StorageRow      input[19] = {};
    StorageOtherRow other[19] = {};
    for (uint32_t i = 0; i < 19; ++i)
    {
        input[i].m_Health = (float)i;
        other[i].m_Health = (float)i;
    }
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, 0, 3, input, 0));
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 2, 0, 3, other + 3, 0));
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 2, 0, 16, other + 3, 0));
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
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}
