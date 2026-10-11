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

#include <dmsdk/datastore/data.h>
#include <jc_test/jc_test.h>

struct ViewRow
{
    DataVector3 m_Position;
    float       m_Health;
};

static DataResult RegisterView(HDataStore store, uint64_t type, ViewRow* values, uint32_t capacity, uint32_t count, const uint32_t* indices, DataId* ids)
{
    DataFieldDesc fields[] = {
        { .m_Field = 1, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = offsetof(ViewRow, m_Position) },
        { .m_Field = 2, .m_Type = DATA_TYPE_FLOAT, .m_Offset = offsetof(ViewRow, m_Health) }
    };
    DataTableDesc desc = { .m_Type = type, .m_Tags = &type, .m_TagCount = 1, .m_Fields = fields, .m_FieldCount = 2, .m_RowStride = sizeof(ViewRow) };
    return DataRegisterTableView(store, &desc, values, capacity, count, indices, ids);
}

static DataResult CreateViewQuery(HDataStore store, uint64_t tag, uint64_t field, DataAccess access, HDataQuery* query, uint32_t* binding)
{
    DataQueryField required = { .m_Field = field, .m_Type = field == 1 ? DATA_TYPE_VECTOR3 : DATA_TYPE_FLOAT, .m_Access = access };
    DataQueryDesc  desc = { .m_AllTags = &tag, .m_AllTagCount = 1, .m_Fields = &required, .m_FieldCount = 1 };
    DataResult     result = DataCreateQuery(store, &desc, query);
    if (result == DATA_RESULT_OK)
        *binding = DataQueryFindField(*query, &required);
    return result;
}

// Sparse source rows and a range starting inside a contiguous run must retain
// their logical IDs while returning pointers into the caller's original array.
TEST(DataView, SparseRangesAndBorrowedPointers)
{
    HDataStore store = DataCreateStore();
    ViewRow    values[8] = {};
    uint32_t   indices[] = { 2, 3, 4, 0, 7 };
    DataId     ids[5];
    ASSERT_EQ(DATA_RESULT_OK, RegisterView(store, 100, values, 8, 5, indices, ids));
    HDataQuery query;
    uint32_t   field;
    ASSERT_EQ(DATA_RESULT_OK, CreateViewQuery(store, 100, 1, DATA_ACCESS_READ_WRITE, &query, &field));
    ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(query));
    ASSERT_EQ(5u, DataQueryGetRowCount(query));
    DataIterator it = DataQueryIterRange(query, 1, 3);
    uint32_t     index = 1;
    while (DataIterNext(&it))
    {
        DataRowIterator rows = DataIterRows(&it);
        while (DataRowIterNext(&rows))
        {
            ASSERT_LT(index, 4u);
            ASSERT_EQ(ids[index], DataRowIterGetId(&rows));
            DataVector3* position = DataRowIterGetVector3Mut(&rows, field);
            ASSERT_EQ(&values[indices[index]].m_Position, position);
            position->m_Values[0] = (float)index;
            ++index;
        }
    }
    ASSERT_EQ(4u, index);
    ASSERT_FALSE(DataIterNext(&it));
    ASSERT_EQ(0u, DataIterGetType(&it));
    ASSERT_EQ(DATA_RESULT_LOCKED, DataUnregisterTable(store, 100));
    DataQueryEnd(query);
    ASSERT_EQ(1.0f, values[3].m_Position.m_Values[0]);
    ASSERT_EQ(2.0f, values[4].m_Position.m_Values[0]);
    ASSERT_EQ(3.0f, values[0].m_Position.m_Values[0]);
    ASSERT_EQ(0.0f, values[2].m_Position.m_Values[0]);
    DataVector3 value;
    ASSERT_EQ(DATA_RESULT_OK, DataFieldGetVector3(store, ids[2], 1, &value));
    ASSERT_EQ(2.0f, value.m_Values[0]);

    DataStoreLock(store);
    it = DataQueryIter(query);
    index = 0;
    while (DataIterNext(&it))
    {
        DataRowIterator rows = DataIterRows(&it);
        while (DataRowIterNext(&rows))
        {
            ASSERT_EQ(ids[index], DataRowIterGetId(&rows));
            ASSERT_EQ((const DataVector3*)&values[indices[index]].m_Position, DataRowIterGetVector3(&rows, field));
            ++index;
        }
    }
    ASSERT_EQ(5u, index);
    ASSERT_FALSE(DataIterNext(&it));
    ASSERT_EQ(0u, DataIterGetType(&it));
    DataStoreUnlock(store);
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataResetRow(store, ids[0]));
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataRemoveRow(store, ids[0]));
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataCreateRows(store, 100, 0, 1, values, 0));
    ASSERT_EQ(DATA_RESULT_OK, DataUnregisterTable(store, 100));
    ASSERT_EQ(DATA_RESULT_NOT_FOUND, DataFieldGetVector3(store, ids[0], 1, &value));
    ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(query));
    ASSERT_EQ(0u, DataQueryGetRowCount(query));
    DataQueryEnd(query);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(query));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
    ASSERT_EQ(2.0f, values[4].m_Position.m_Values[0]);
}

// Different tables sharing physical rows must conflict on writes, while disjoint
// rows, disjoint fields and shared readers remain concurrently reservable.
TEST(DataView, AliasedReservations)
{
    HDataStore     store = DataCreateStore();
    ViewRow        values[4] = {};
    const uint32_t a[] = { 0, 1 }, b[] = { 2, 3 }, c[] = { 1, 1 };
    ASSERT_EQ(DATA_RESULT_OK, RegisterView(store, 100, values, 4, 2, a, 0));
    ASSERT_EQ(DATA_RESULT_OK, RegisterView(store, 200, values, 4, 2, b, 0));
    ASSERT_EQ(DATA_RESULT_OK, RegisterView(store, 300, values, 4, 2, c, 0));
    HDataQuery writer, disjoint, reader, reader2, health;
    uint32_t   field;
    ASSERT_EQ(DATA_RESULT_OK, CreateViewQuery(store, 100, 1, DATA_ACCESS_READ_WRITE, &writer, &field));
    ASSERT_EQ(DATA_RESULT_OK, CreateViewQuery(store, 200, 1, DATA_ACCESS_READ_WRITE, &disjoint, &field));
    ASSERT_EQ(DATA_RESULT_OK, CreateViewQuery(store, 300, 1, DATA_ACCESS_READ, &reader, &field));
    ASSERT_EQ(DATA_RESULT_OK, CreateViewQuery(store, 100, 1, DATA_ACCESS_READ, &reader2, &field));
    ASSERT_EQ(DATA_RESULT_OK, CreateViewQuery(store, 300, 2, DATA_ACCESS_READ_WRITE, &health, &field));
    ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(writer));
    ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(disjoint));
    ASSERT_EQ(DATA_RESULT_BUSY, DataQueryTryBegin(reader));
    ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(health));
    DataQueryEnd(health);
    DataQueryEnd(disjoint);
    DataQueryEnd(writer);
    ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(reader));
    ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(reader2));
    ASSERT_EQ(DATA_RESULT_BUSY, DataQueryTryBegin(writer));
    DataQueryEnd(reader2);
    DataQueryEnd(reader);
    ASSERT_EQ(DATA_RESULT_OK, DataUnregisterTable(store, 100));
    ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(reader));
    DataQueryEnd(reader);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(writer));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(disjoint));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(reader));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(reader2));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(health));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}

// Empty views are valid, out-of-range mappings publish no table, and live queries
// must rematch a view after its owner changes population and re-registers it.
TEST(DataView, EmptyAndReboundViews)
{
    HDataStore store = DataCreateStore();
    ViewRow    values[2] = {};
    uint32_t   invalid[] = { 2 };
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, RegisterView(store, 100, values, 2, 1, invalid, 0));
    ASSERT_EQ(DATA_RESULT_OK, RegisterView(store, 100, values, 2, 0, 0, 0));
    HDataQuery query;
    uint32_t   field;
    ASSERT_EQ(DATA_RESULT_OK, CreateViewQuery(store, 100, 1, DATA_ACCESS_READ, &query, &field));
    ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(query));
    ASSERT_EQ(0u, DataQueryGetRowCount(query));
    DataIterator it = DataQueryIterRange(query, 0, 0);
    ASSERT_FALSE(DataIterNext(&it));
    ASSERT_FALSE(DataIterNext(&it));
    DataQueryEnd(query);
    ASSERT_EQ(DATA_RESULT_OK, DataUnregisterTable(store, 100));
    ASSERT_EQ(DATA_RESULT_OK, RegisterView(store, 100, values, 2, 2, 0, 0));
    ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(query));
    ASSERT_EQ(2u, DataQueryGetRowCount(query));
    DataQueryEnd(query);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(query));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(store));
}
