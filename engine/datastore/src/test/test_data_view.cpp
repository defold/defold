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
    DSVector3 m_Position;
    float     m_Health;
};

static DSResult RegisterView(HDSContext store, uint64_t type, ViewRow* values, uint32_t capacity, uint32_t count, const uint32_t* indices, DSId* ids)
{
    DSFieldDesc fields[] = {
        { 1, DS_TYPE_VECTOR3, offsetof(ViewRow, m_Position) },
        { 2, DS_TYPE_FLOAT, offsetof(ViewRow, m_Health) }
    };
    DSTableDesc desc = { type, &type, 1, fields, 2, sizeof(ViewRow) };
    return DSTableAddView(store, &desc, values, capacity, count, indices, ids);
}

static DSResult CreateViewQuery(HDSContext store, uint64_t tag, uint64_t field, uint32_t access, HDSQuery* query, DSFieldId* binding, uint32_t flags = 0)
{
    DSQueryItem required = { field, field == 1 ? DS_TYPE_VECTOR3 : DS_TYPE_FLOAT, access, DS_QUERY_ITEM_FIELD };
    DSQueryDesc desc = { 0, 0, flags, { required, { tag, DS_TYPE_NUMBER, 0, DS_QUERY_ITEM_TAG } } };
    DSResult    result = DSQueryCreate(store, &desc, query);
    if (result == DS_RESULT_OK)
        *binding = DSQueryFindField(*query, &required);
    return result;
}

// Sparse source rows and a range starting inside a contiguous run must retain
// their logical IDs while returning pointers into the caller's original array.
TEST(DataView, SparseRangesAndBorrowedPointers)
{
    HDSContext store = DSStoreCreate(0);
    ViewRow    values[8] = {};
    uint32_t   indices[] = { 2, 3, 4, 0, 7 };
    DSId       ids[5];
    ASSERT_EQ(DS_RESULT_OK, RegisterView(store, 100, values, 8, 5, indices, ids));
    HDSQuery query;
    DSFieldId field;
    ASSERT_EQ(DS_RESULT_OK, CreateViewQuery(store, 100, 1, DS_FIELD_MUTABLE, &query, &field, DS_ROW_MUTABLE));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    ASSERT_EQ(5u, DSQueryGetRowCount(query));
    DSIterator it = DSQueryIterRange(query, 1, 3);
    uint32_t   index = 1;
    while (DSIterNext(&it))
    {
        DSRowIterator rows = DSIterRows(&it);
        while (DSRowIterNext(&rows))
        {
            ASSERT_LT(index, 4u);
            ASSERT_EQ(ids[index], DSRowIterGetId(&rows));
            DSVector3* position = DSRowIterGetVector3Mut(&rows, field);
            ASSERT_EQ(&values[indices[index]].m_Position, position);
            ViewRow* row = (ViewRow*)DSRowIterGetDataMut(&rows);
            ASSERT_EQ(&values[indices[index]], row);
            row->m_Position.m_Values[0] = (float)index;
            ++index;
        }
    }
    ASSERT_EQ(4u, index);
    ASSERT_FALSE(DSIterNext(&it));
    ASSERT_EQ(0u, DSIterGetType(&it));
    ASSERT_EQ(DS_RESULT_LOCKED, DSTableRemove(store, 100));
    DSQueryEnd(query);
    ASSERT_EQ(1.0f, values[3].m_Position.m_Values[0]);
    ASSERT_EQ(2.0f, values[4].m_Position.m_Values[0]);
    ASSERT_EQ(3.0f, values[0].m_Position.m_Values[0]);
    ASSERT_EQ(0.0f, values[2].m_Position.m_Values[0]);
    DSVector3 value;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3(store, ids[2], 1, &value));
    ASSERT_EQ(2.0f, value.m_Values[0]);

    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    it = DSQueryIter(query);
    index = 0;
    while (DSIterNext(&it))
    {
        DSRowIterator rows = DSIterRows(&it);
        while (DSRowIterNext(&rows))
        {
            ASSERT_EQ(ids[index], DSRowIterGetId(&rows));
            ASSERT_EQ((const DSVector3*)&values[indices[index]].m_Position, DSRowIterGetVector3(&rows, field));
            ASSERT_EQ((const void*)&values[indices[index]], DSRowIterGetData(&rows));
            ++index;
        }
    }
    ASSERT_EQ(5u, index);
    ASSERT_FALSE(DSIterNext(&it));
    ASSERT_EQ(0u, DSIterGetType(&it));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRowReset(store, ids[0]));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRowRemove(store, ids[0]));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRowCreate(store, 100, 0, 1, values, 0));
    ASSERT_EQ(DS_RESULT_OK, DSTableRemove(store, 100));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGetVector3(store, ids[0], 1, &value));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    ASSERT_EQ(0u, DSQueryGetRowCount(query));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
    ASSERT_EQ(2.0f, values[4].m_Position.m_Values[0]);
}

// Different tables sharing physical rows must conflict on writes, while disjoint
// rows, disjoint fields and shared readers remain concurrently reservable.
TEST(DataView, AliasedReservations)
{
    HDSContext     store = DSStoreCreate(0);
    ViewRow        values[4] = {};
    const uint32_t a[] = { 0, 1 }, b[] = { 2, 3 }, c[] = { 1, 1 };
    ASSERT_EQ(DS_RESULT_OK, RegisterView(store, 100, values, 4, 2, a, 0));
    ASSERT_EQ(DS_RESULT_OK, RegisterView(store, 200, values, 4, 2, b, 0));
    ASSERT_EQ(DS_RESULT_OK, RegisterView(store, 300, values, 4, 2, c, 0));
    HDSQuery writer, disjoint, reader, reader2, health;
    DSFieldId field;
    ASSERT_EQ(DS_RESULT_OK, CreateViewQuery(store, 100, 1, DS_FIELD_MUTABLE, &writer, &field));
    ASSERT_EQ(DS_RESULT_OK, CreateViewQuery(store, 200, 1, DS_FIELD_MUTABLE, &disjoint, &field));
    ASSERT_EQ(DS_RESULT_OK, CreateViewQuery(store, 300, 1, 0, &reader, &field));
    ASSERT_EQ(DS_RESULT_OK, CreateViewQuery(store, 100, 1, 0, &reader2, &field));
    ASSERT_EQ(DS_RESULT_OK, CreateViewQuery(store, 300, 2, DS_FIELD_MUTABLE, &health, &field));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(writer));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(disjoint));
    ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(reader));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(health));
    DSQueryEnd(health);
    DSQueryEnd(disjoint);
    DSQueryEnd(writer);
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(reader));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(reader2));
    ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(writer));
    DSQueryEnd(reader2);
    DSQueryEnd(reader);
    ASSERT_EQ(DS_RESULT_OK, DSTableRemove(store, 100));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(reader));
    DSQueryEnd(reader);
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(writer));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(disjoint));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(reader));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(reader2));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(health));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Whole-row reservations must cover all fields of aliased views, while views
// selecting disjoint source rows may still reserve whole-row writes together.
TEST(DataView, WholeRowAliasReservations)
{
    HDSContext store = DSStoreCreate(0);
    ViewRow    values[4] = {};
    uint32_t   a[] = { 0, 1 }, b[] = { 2, 3 }, c[] = { 1, 2 };
    ASSERT_EQ(DS_RESULT_OK, RegisterView(store, 100, values, 4, 2, a, 0));
    ASSERT_EQ(DS_RESULT_OK, RegisterView(store, 200, values, 4, 2, b, 0));
    ASSERT_EQ(DS_RESULT_OK, RegisterView(store, 300, values, 4, 2, c, 0));
    HDSQuery writer, disjoint, reader, health, position;
    DSFieldId field;
    ASSERT_EQ(DS_RESULT_OK, CreateViewQuery(store, 100, 1, 0, &writer, &field, DS_ROW_MUTABLE));
    ASSERT_EQ(DS_RESULT_OK, CreateViewQuery(store, 200, 1, 0, &disjoint, &field, DS_ROW_MUTABLE));
    ASSERT_EQ(DS_RESULT_OK, CreateViewQuery(store, 300, 1, 0, &reader, &field));
    ASSERT_EQ(DS_RESULT_OK, CreateViewQuery(store, 300, 2, DS_FIELD_MUTABLE, &health, &field));
    ASSERT_EQ(DS_RESULT_OK, CreateViewQuery(store, 100, 1, 0, &position, &field));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(writer));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(disjoint));
    ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(reader));
    ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(health));
    DSQueryEnd(disjoint);
    DSQueryEnd(writer);
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(reader));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(position));
    ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(writer));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(health));
    DSQueryEnd(health);
    DSQueryEnd(position);
    DSQueryEnd(reader);
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(writer));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(disjoint));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(reader));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(health));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(position));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Empty views are valid, out-of-range mappings publish no table, and live queries
// must rematch a view after its owner changes population and re-registers it.
TEST(DataView, EmptyAndReboundViews)
{
    HDSContext store = DSStoreCreate(0);
    ViewRow    values[2] = {};
    uint32_t   invalid[] = { 2 };
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, RegisterView(store, 100, values, 2, 1, invalid, 0));
    ASSERT_EQ(DS_RESULT_OK, RegisterView(store, 100, values, 2, 0, 0, 0));
    HDSQuery query;
    DSFieldId field;
    ASSERT_EQ(DS_RESULT_OK, CreateViewQuery(store, 100, 1, 0, &query, &field));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    ASSERT_EQ(0u, DSQueryGetRowCount(query));
    DSIterator it = DSQueryIterRange(query, 0, 0);
    ASSERT_FALSE(DSIterNext(&it));
    ASSERT_FALSE(DSIterNext(&it));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSTableRemove(store, 100));
    ASSERT_EQ(DS_RESULT_OK, RegisterView(store, 100, values, 2, 2, 0, 0));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    ASSERT_EQ(2u, DSQueryGetRowCount(query));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}
