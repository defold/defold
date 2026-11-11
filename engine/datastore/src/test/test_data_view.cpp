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

#include <dmsdk/datastore/datastore.h>
#include <jc_test/jc_test.h>

struct ViewRow
{
    DataStoreVector3 m_Position;
    float            m_Health;
};

static DataStoreResult RegisterView(HDataStoreContext store, uint64_t type, ViewRow* values, uint32_t capacity, uint32_t count, const uint32_t* indices, DataStoreId* ids)
{
    DataStoreFieldDesc fields[] = {
        { 1, DATASTORE_TYPE_VECTOR3, offsetof(ViewRow, m_Position) },
        { 2, DATASTORE_TYPE_FLOAT, offsetof(ViewRow, m_Health) }
    };
    DataStoreTableDesc desc = { type, &type, 1, fields, 2, sizeof(ViewRow) };
    return DataStoreTableAddView(store, &desc, values, capacity, count, indices, ids);
}

static DataStoreResult CreateViewQuery(HDataStoreContext store, uint64_t tag, uint64_t field, uint32_t access, HDataStoreQuery* query, DataStoreFieldId* binding, uint32_t flags)
{
    DataStoreQueryItem required = { field, field == 1 ? DATASTORE_TYPE_VECTOR3 : DATASTORE_TYPE_FLOAT, access, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryDesc desc = { { required, { tag, DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_TAG } }, 0, 0, flags };
    DataStoreResult    result = DataStoreQueryCreate(store, &desc, query);
    if (result == DATASTORE_RESULT_OK)
    {
        *binding = DataStoreQueryFindField(*query, &required);
    }
    return result;
}

// Sparse source rows and a range starting inside a contiguous run must retain
// their logical IDs while returning pointers into the caller's original array.
TEST(DataView, SparseRangesAndBorrowedPointers)
{
    HDataStoreContext store = DataStoreCreate(0);
    ViewRow           values[8] = {};
    uint32_t          indices[] = { 2, 3, 4, 0, 7 };
    DataStoreId       ids[5];
    ASSERT_EQ(DATASTORE_RESULT_OK, RegisterView(store, 100, values, 8, 5, indices, ids));
    HDataStoreQuery query;
    DataStoreFieldId field;
    ASSERT_EQ(DATASTORE_RESULT_OK, CreateViewQuery(store, 100, 1, DATASTORE_FIELD_MUTABLE, &query, &field, DATASTORE_ROW_MUTABLE));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    ASSERT_EQ(5u, DataStoreQueryGetRowCount(query));
    DataStoreIterator it    = DataStoreQueryIterRange(query, 1, 3);
    uint32_t          index = 1;
    while (DataStoreIterNext(&it))
    {
        DataStoreRowIterator rows = DataStoreIterRows(&it);
        while (DataStoreRowIterNext(&rows))
        {
            ASSERT_LT(index, 4u);
            ASSERT_EQ(ids[index], DataStoreRowIterGetId(&rows));
            DataStoreVector3* position = DataStoreRowIterGetVector3Mut(&rows, field);
            ASSERT_EQ(&values[indices[index]].m_Position, position);
            ViewRow* row = (ViewRow*)DataStoreRowIterGetDataMut(&rows);
            ASSERT_EQ(&values[indices[index]], row);
            row->m_Position.m_Values[0] = (float)index;
            ++index;
        }
    }
    ASSERT_EQ(4u, index);
    ASSERT_FALSE(DataStoreIterNext(&it));
    ASSERT_EQ(0u, DataStoreIterGetType(&it));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreTableRemove(store, 100));
    DataStoreQueryEnd(query);
    ASSERT_EQ(1.0f, values[3].m_Position.m_Values[0]);
    ASSERT_EQ(2.0f, values[4].m_Position.m_Values[0]);
    ASSERT_EQ(3.0f, values[0].m_Position.m_Values[0]);
    ASSERT_EQ(0.0f, values[2].m_Position.m_Values[0]);
    DataStoreVector3 value;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3(store, ids[2], 1, &value));
    ASSERT_EQ(2.0f, value.m_Values[0]);

    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    it = DataStoreQueryIter(query);
    index = 0;
    while (DataStoreIterNext(&it))
    {
        DataStoreRowIterator rows = DataStoreIterRows(&it);
        while (DataStoreRowIterNext(&rows))
        {
            ASSERT_EQ(ids[index], DataStoreRowIterGetId(&rows));
            ASSERT_EQ((const DataStoreVector3*)&values[indices[index]].m_Position, DataStoreRowIterGetVector3(&rows, field));
            ASSERT_EQ((const void*)&values[indices[index]], DataStoreRowIterGetData(&rows));
            ++index;
        }
    }
    ASSERT_EQ(5u, index);
    ASSERT_FALSE(DataStoreIterNext(&it));
    ASSERT_EQ(0u, DataStoreIterGetType(&it));
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowReset(store, ids[0]));
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowRemove(store, ids[0]));
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowCreate(store, 100, 0, 1, values, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableRemove(store, 100));
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGetVector3(store, ids[0], 1, &value));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    ASSERT_EQ(0u, DataStoreQueryGetRowCount(query));
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    ASSERT_EQ(2.0f, values[4].m_Position.m_Values[0]);
}

// Different tables sharing physical rows must conflict on writes, while disjoint
// rows, disjoint fields and shared readers remain concurrently reservable.
TEST(DataView, AliasedReservations)
{
    HDataStoreContext store = DataStoreCreate(0);
    ViewRow           values[4] = {};
    const uint32_t    a[] = { 0, 1 }, b[] = { 2, 3 }, c[] = { 1, 1 };
    ASSERT_EQ(DATASTORE_RESULT_OK, RegisterView(store, 100, values, 4, 2, a, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, RegisterView(store, 200, values, 4, 2, b, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, RegisterView(store, 300, values, 4, 2, c, 0));
    HDataStoreQuery writer, disjoint, reader, reader2, health;
    DataStoreFieldId field;
    ASSERT_EQ(DATASTORE_RESULT_OK, CreateViewQuery(store, 100, 1, DATASTORE_FIELD_MUTABLE, &writer, &field, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, CreateViewQuery(store, 200, 1, DATASTORE_FIELD_MUTABLE, &disjoint, &field, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, CreateViewQuery(store, 300, 1, 0, &reader, &field, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, CreateViewQuery(store, 100, 1, 0, &reader2, &field, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, CreateViewQuery(store, 300, 2, DATASTORE_FIELD_MUTABLE, &health, &field, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(writer));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(disjoint));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreQueryTryBegin(reader));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(health));
    DataStoreQueryEnd(health);
    DataStoreQueryEnd(disjoint);
    DataStoreQueryEnd(writer);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(reader));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(reader2));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreQueryTryBegin(writer));
    DataStoreQueryEnd(reader2);
    DataStoreQueryEnd(reader);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableRemove(store, 100));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(reader));
    DataStoreQueryEnd(reader);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(writer));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(disjoint));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(reader));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(reader2));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(health));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Whole-row reservations must cover all fields of aliased views, while views
// selecting disjoint source rows may still reserve whole-row writes together.
TEST(DataView, WholeRowAliasReservations)
{
    HDataStoreContext store = DataStoreCreate(0);
    ViewRow           values[4] = {};
    uint32_t          a[] = { 0, 1 }, b[] = { 2, 3 }, c[] = { 1, 2 };
    ASSERT_EQ(DATASTORE_RESULT_OK, RegisterView(store, 100, values, 4, 2, a, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, RegisterView(store, 200, values, 4, 2, b, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, RegisterView(store, 300, values, 4, 2, c, 0));
    HDataStoreQuery writer, disjoint, reader, health, position;
    DataStoreFieldId field;
    ASSERT_EQ(DATASTORE_RESULT_OK, CreateViewQuery(store, 100, 1, 0, &writer, &field, DATASTORE_ROW_MUTABLE));
    ASSERT_EQ(DATASTORE_RESULT_OK, CreateViewQuery(store, 200, 1, 0, &disjoint, &field, DATASTORE_ROW_MUTABLE));
    ASSERT_EQ(DATASTORE_RESULT_OK, CreateViewQuery(store, 300, 1, 0, &reader, &field, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, CreateViewQuery(store, 300, 2, DATASTORE_FIELD_MUTABLE, &health, &field, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, CreateViewQuery(store, 100, 1, 0, &position, &field, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(writer));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(disjoint));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreQueryTryBegin(reader));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreQueryTryBegin(health));
    DataStoreQueryEnd(disjoint);
    DataStoreQueryEnd(writer);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(reader));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(position));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreQueryTryBegin(writer));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(health));
    DataStoreQueryEnd(health);
    DataStoreQueryEnd(position);
    DataStoreQueryEnd(reader);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(writer));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(disjoint));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(reader));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(health));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(position));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Missing storage for selected rows fails before any table or IDs are published.
// A valid empty view can still be registered under the same type afterward.
TEST(DataView, NullBackingPublishesNoView)
{
    HDataStoreContext store = DataStoreCreate(0);
    ViewRow           values[1] = {};
    DataStoreId       id = 123;
    uint32_t          index = 0;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, RegisterView(store, 100, 0, 1, 1, 0, &id));
    ASSERT_EQ((DataStoreId)123, id);
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, RegisterView(store, 100, 0, 1, 1, &index, &id));
    ASSERT_EQ((DataStoreId)123, id);
    ASSERT_EQ(DATASTORE_RESULT_OK, RegisterView(store, 100, values, 1, 0, 0, 0));
    HDataStoreQuery  query;
    DataStoreFieldId field;
    ASSERT_EQ(DATASTORE_RESULT_OK, CreateViewQuery(store, 100, 1, 0, &query, &field, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    ASSERT_EQ(0u, DataStoreQueryGetRowCount(query));
    DataStoreIterator it = DataStoreQueryIter(query);
    ASSERT_FALSE(DataStoreIterNext(&it));
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Empty views are valid, out-of-range mappings publish no table, and live queries
// must rematch a view after its owner changes population and re-registers it.
TEST(DataView, EmptyAndReboundViews)
{
    HDataStoreContext store = DataStoreCreate(0);
    ViewRow           values[2] = {};
    uint32_t          invalid[] = { 2 };
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, RegisterView(store, 100, values, 2, 1, invalid, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, RegisterView(store, 100, values, 2, 0, 0, 0));
    HDataStoreQuery query;
    DataStoreFieldId field;
    ASSERT_EQ(DATASTORE_RESULT_OK, CreateViewQuery(store, 100, 1, 0, &query, &field, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    ASSERT_EQ(0u, DataStoreQueryGetRowCount(query));
    DataStoreIterator it = DataStoreQueryIterRange(query, 0, 0);
    ASSERT_FALSE(DataStoreIterNext(&it));
    ASSERT_FALSE(DataStoreIterNext(&it));
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableRemove(store, 100));
    ASSERT_EQ(DATASTORE_RESULT_OK, RegisterView(store, 100, values, 2, 2, 0, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    ASSERT_EQ(2u, DataStoreQueryGetRowCount(query));
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}
