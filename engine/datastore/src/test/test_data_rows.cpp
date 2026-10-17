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
#include "../data.h"

struct StorageRow
{
    uint8_t     m_Enabled;
    DataVector3 m_Position;
    float       m_Health;
    uint32_t    m_Color;
};

struct StorageOtherRow
{
    float       m_Health;
    DataVector3 m_Position;
};

struct StorageLight
{
    uint32_t    m_Color;
    DataVector3 m_Direction;
    float       m_Health;
};

struct StorageReferences
{
    uint8_t       m_Enabled;
    StorageLight  m_Light;
    DataReference m_Name;
    DataReference m_Dynamic;
    DataReference m_List;
};

static DataResult RegisterStorageRows(HDataStore store)
{
    // Declaration order differs from native offsets and query order.
    DataFieldDesc fields[] = {
        { .m_Field = 4, .m_Type = DATA_TYPE_UINT32, .m_Offset = offsetof(StorageRow, m_Color) },
        { .m_Field = 1, .m_Type = DATA_TYPE_BOOLEAN, .m_Offset = offsetof(StorageRow, m_Enabled) },
        { .m_Field = 2, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = offsetof(StorageRow, m_Position) },
        { .m_Field = 3, .m_Type = DATA_TYPE_FLOAT, .m_Offset = offsetof(StorageRow, m_Health) }
    };
    DataTableDesc table = { .m_Type = 100, .m_Fields = fields, .m_FieldCount = 4, .m_RowStride = sizeof(StorageRow) };
    return DataRegisterTable(store, &table);
}

// Row growth preserves query bindings and group ranges; swap removal and reset
// must keep every field and its defaults associated with the same row ID.
TEST(DataRows, GrowthRangesRemovalAndReset)
{
    HDataStore store = DataCreateStore();
    ASSERT_EQ(DATA_RESULT_OK, RegisterStorageRows(store));
    DataGroupId    group = 20;
    DataQueryField fields[] = {
        { .m_Field = 3, .m_Type = DATA_TYPE_FLOAT, .m_Access = DATA_ACCESS_READ_WRITE },
        { .m_Field = 2, .m_Type = DATA_TYPE_VECTOR3 },
        { .m_Field = 4, .m_Type = DATA_TYPE_UINT32 }
    };
    DataQueryDesc desc = { .m_GroupIds = &group, .m_GroupIdCount = 1, .m_Fields = fields, .m_FieldCount = 3 };
    HDataQuery    query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    uint32_t    health_field = DataQueryFindField(query, &fields[0]);
    uint32_t    position_field = DataQueryFindField(query, &fields[1]);
    uint32_t    color_field = DataQueryFindField(query, &fields[2]);
    StorageRow  input[36];
    DataId      ids[36];
    DataVector3 positions[33];
    uint32_t    colors[33];
    float       health[33];
    uint8_t     enabled[33];
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
    ASSERT_EQ(DATA_RESULT_OK, DataCreateRows(store, 100, 10, 3, input, ids));
    DataFieldArray columns[] = { { 3, health }, { 2, positions }, { 1, enabled }, { 4, colors } };
    ASSERT_EQ(DATA_RESULT_OK, DataCreateRowsSoA(store, 100, group, 33, 4, columns, ids + 3));
    ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(query));
    ASSERT_EQ(33u, DataQueryGetRowCount(query));
    DataIterator it = DataQueryIterRange(query, 5, 11);
    uint32_t     visited = 0;
    while (DataIterNext(&it))
    {
        DataRowIterator rows = DataIterRows(&it);
        while (DataRowIterNext(&rows))
        {
            uint32_t i = 8 + visited++;
            ASSERT_EQ(ids[i], DataRowIterGetId(&rows));
            ASSERT_EQ(group, DataRowIterGetGroupId(&rows));
            ASSERT_EQ((float)i, DataRowIterGetVector3(&rows, position_field)->m_Values[0]);
            ASSERT_EQ(input[i].m_Color, *DataRowIterGetUInt32(&rows, color_field));
            *DataRowIterGetFloatMut(&rows, health_field) += 7;
        }
    }
    ASSERT_EQ(11u, visited);
    DataQueryEnd(query);
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[1]));
    float value;
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGetFloat(store, ids[1], 3, &value));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetFloat(store, ids[35], 3, &value));
    ASSERT_EQ(input[35].m_Health, value);
    ASSERT_EQ(DATA_RESULT_OK, DataSetFieldFloat(store, ids[35], 3, -0.0f));
    ASSERT_EQ(DATA_RESULT_OK, DataSetFieldUInt32(store, ids[35], 4, 0));
    ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, ids[35]));
    uint32_t color;
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetUInt32(store, ids[35], 4, &color));
    ASSERT_EQ(input[35].m_Color, color);
    ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, ids[8]));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetFloat(store, ids[8], 3, &value));
    ASSERT_EQ(input[8].m_Health, value);
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetFloat(store, ids[9], 3, &value));
    ASSERT_EQ(input[9].m_Health + 7, value);
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetUInt32Batch(store, 33, ids + 3, 4, colors));
    for (uint32_t i = 0; i < 33; ++i)
        ASSERT_EQ(input[i + 3].m_Color, colors[i]);
    ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(query));
    ASSERT_EQ(33u, DataQueryGetRowCount(query));
    DataQueryEnd(query);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(query));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

// Inline members and owned strings/containers must retain their values and
// ownership across reset and a version-1 blob roundtrip.
TEST(DataRows, ReferencesInlineFieldsAndBlobRoundtrip)
{
    HDataStore    store = DataCreateStore();
    DataFieldDesc light_fields[] = {
        { .m_Field = dmHashString64("color"), .m_Type = DATA_TYPE_UINT32, .m_Offset = offsetof(StorageLight, m_Color), .m_Name = "color" },
        { .m_Field = dmHashString64("direction"), .m_Type = DATA_TYPE_VECTOR3, .m_Offset = offsetof(StorageLight, m_Direction), .m_Name = "direction" },
        { .m_Field = dmHashString64("health"), .m_Type = DATA_TYPE_FLOAT, .m_Offset = offsetof(StorageLight, m_Health), .m_Name = "health" }
    };
    DataStructDesc light = { .m_Fields = light_fields, .m_FieldCount = 3, .m_Size = sizeof(StorageLight) };
    DataFieldDesc  fields[] = {
        { .m_Field = 1, .m_Type = DATA_TYPE_BOOLEAN, .m_Offset = offsetof(StorageReferences, m_Enabled) },
        { .m_Field = dmHashString64("light"), .m_Type = DATA_TYPE_STRUCT, .m_Offset = offsetof(StorageReferences, m_Light), .m_Struct = &light, .m_Name = "light" },
        { .m_Field = 3, .m_Type = DATA_TYPE_STRING, .m_Offset = offsetof(StorageReferences, m_Name) },
        { .m_Field = 4, .m_Type = DATA_TYPE_STRUCT, .m_Offset = offsetof(StorageReferences, m_Dynamic) },
        { .m_Field = 5, .m_Type = DATA_TYPE_LIST, .m_Offset = offsetof(StorageReferences, m_List) }
    };
    DataTableDesc table = { .m_Type = 100, .m_Fields = fields, .m_FieldCount = 5, .m_RowStride = sizeof(StorageReferences) };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    StorageLight      child = { .m_Color = UINT32_MAX, .m_Direction = { { 1, 2, 3 } }, .m_Health = -0.0f };
    DataStructInput   dynamic = { .m_Layout = &light, .m_Values = &child };
    DataValueType     kinds[] = { DATA_TYPE_FLOAT, DATA_TYPE_UINT32, DATA_TYPE_STRUCT };
    const void*       payloads[] = { &child.m_Health, &child.m_Color, &dynamic };
    DataListInput     list = { .m_Count = 3, .m_Values = payloads, .m_Types = kinds };
    char              name[] = "school";
    StorageReferences input = { .m_Enabled = 1, .m_Light = child, .m_Name = { .m_String = name }, .m_Dynamic = { .m_Struct = &dynamic }, .m_List = { .m_List = &list } };
    DataId            id;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateRows(store, 100, 7, 1, &input, &id));
    name[0] = 'X';
    const char* stored_name;
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetString(store, id, 3, &stored_name));
    ASSERT_STREQ("school", stored_name);
    uint64_t health = dmHashString64("light.health");
    ASSERT_EQ(DATA_RESULT_OK, DataSetFieldFloat(store, id, health, 12.5f));
    ASSERT_EQ(DATA_RESULT_OK, DataSetFieldString(store, id, 3, "replacement"));
    ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, id));
    float value;
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetFloat(store, id, health, &value));
    ASSERT_EQ(0, memcmp(&value, &child.m_Health, sizeof(value)));
    DataValue object;
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, id, dmHashString64("light"), &object));
    DataValue member;
    ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&object.m_Value.m_Struct, dmHashString64("direction"), &member));
    ASSERT_EQ(0, memcmp(member.m_Value.m_Vector3, child.m_Direction.m_Values, sizeof(DataVector3)));
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, id, 4, &object));
    ASSERT_EQ(DATA_RESULT_OK, DataGetStructField(&object.m_Value.m_Struct, dmHashString64("color"), &member));
    ASSERT_EQ(UINT32_MAX, member.m_Value.m_UInt32);
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGet(store, id, 5, &object));
    ASSERT_EQ(DATA_RESULT_OK, DataGetListValue(&object.m_Value.m_List, 1, &member));
    ASSERT_EQ(UINT32_MAX, member.m_Value.m_UInt32);
    uint32_t size;
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, 0, 0, &size));
    dmArray<uint8_t> bytes;
    bytes.SetCapacity(size);
    bytes.SetSize(size);
    ASSERT_EQ(DATA_RESULT_OK, DataWriteBlob(store, bytes.Begin(), size, &size));
    HDataBlob blob;
    ASSERT_EQ(DATA_RESULT_OK, DataLoadBlob(bytes.Begin(), size, &blob));
    HDataStore        loaded = DataCreateStore();
    HDataBlobInstance instance;
    ASSERT_EQ(DATA_RESULT_OK, DataAddBlob(loaded, blob, 7, &instance));
    DataQueryField query_field = { .m_Field = dmHashString64("light.color"), .m_Type = DATA_TYPE_UINT32 };
    DataQueryDesc  desc = { .m_Fields = &query_field, .m_FieldCount = 1 };
    HDataQuery     query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(loaded, &desc, &query));
    uint32_t field = DataQueryFindField(query, &query_field);
    DataStoreLock(loaded);
    DataIterator it = DataQueryIter(query);
    ASSERT_TRUE(DataIterNext(&it));
    DataRowIterator rows = DataIterRows(&it);
    ASSERT_TRUE(DataRowIterNext(&rows));
    ASSERT_EQ(UINT32_MAX, *DataRowIterGetUInt32(&rows, field));
    DataStoreUnlock(loaded);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(query));
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveBlob(instance));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(loaded));
    DataDestroyBlob(blob);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

// Tables without fields must support row creation, reset and removal.
TEST(DataRows, EmptyRows)
{
    HDataStore    store = DataCreateStore();
    DataTableDesc table = { .m_Type = 1 };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    DataId ids[2];
    ASSERT_EQ(DATA_RESULT_OK, DataCreateRows(store, 1, 0, 2, 0, ids));
    ASSERT_EQ(DATA_RESULT_OK, DataResetRow(store, ids[0]));
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[0]));
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(store, ids[1]));
    ASSERT_EQ(DATA_RESULT_OK, DataUnregisterTable(store, 1));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

// One query can cross tables with different row strides and field offsets,
// retaining field handles after growth and when a range starts inside a table.
TEST(DataRows, MixedTableQuery)
{
    HDataStore    store = DataCreateStore();
    DataFieldDesc field = { .m_Field = 1, .m_Type = DATA_TYPE_FLOAT, .m_Offset = offsetof(StorageRow, m_Health) };
    DataTableDesc table = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(StorageRow) };
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    table.m_Type = 2;
    field.m_Offset = offsetof(StorageOtherRow, m_Health);
    table.m_RowStride = sizeof(StorageOtherRow);
    ASSERT_EQ(DATA_RESULT_OK, DataRegisterTable(store, &table));
    DataQueryField requested = { .m_Field = 1, .m_Type = DATA_TYPE_FLOAT, .m_Access = DATA_ACCESS_READ_WRITE };
    DataQueryDesc  desc = { .m_Fields = &requested, .m_FieldCount = 1 };
    HDataQuery     query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    uint32_t        handle = DataQueryFindField(query, &requested);
    StorageRow      input[19] = {};
    StorageOtherRow other[19] = {};
    for (uint32_t i = 0; i < 19; ++i)
    {
        input[i].m_Health = (float)i;
        other[i].m_Health = (float)i;
    }
    ASSERT_EQ(DATA_RESULT_OK, DataCreateRows(store, 1, 0, 3, input, 0));
    ASSERT_EQ(DATA_RESULT_OK, DataCreateRows(store, 2, 0, 3, other + 3, 0));
    ASSERT_EQ(DATA_RESULT_OK, DataCreateRows(store, 2, 0, 16, other + 3, 0));
    ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(query));
    DataIterator it = DataQueryIterRange(query, 1, 8);
    uint32_t     visited = 0;
    while (DataIterNext(&it))
    {
        DataRowIterator rows = DataIterRows(&it);
        while (DataRowIterNext(&rows))
        {
            float expected = (float)(visited < 5 ? visited + 1 : visited - 2);
            ASSERT_EQ(expected, *DataRowIterGetFloat(&rows, handle));
            *DataRowIterGetFloatMut(&rows, handle) = expected + 100;
            ++visited;
        }
    }
    ASSERT_EQ(8u, visited);
    DataQueryEnd(query);
    ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(query));
    it = DataQueryIterRange(query, 4, 1);
    ASSERT_TRUE(DataIterNext(&it));
    DataRowIterator rows = DataIterRows(&it);
    ASSERT_TRUE(DataRowIterNext(&rows));
    ASSERT_EQ(104.0f, *DataRowIterGetFloat(&rows, handle));
    DataQueryEnd(query);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(query));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}
