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

struct GroupRow
{
    double             m_Value;
    DataStoreReference m_Name;
};

static void RegisterGroupTable(HDataStoreContext store, uint64_t type)
{
    DataStoreFieldDesc fields[] = {
        { 1, DATASTORE_TYPE_NUMBER, offsetof(GroupRow, m_Value) },
        { 2, DATASTORE_TYPE_STRING, offsetof(GroupRow, m_Name) }
    };
    DataStoreTableDesc table = { type, 0, 0, fields, 2, sizeof(GroupRow) };
    EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
}

// Removing an owner visits its scattered rows across tables without invalidating survivors.
TEST(DataGroups, ScatteredRowsAcrossTables)
{
    HDataStoreContext store = DataStoreCreate(0);
    DataStoreId       groups[] = { UINT64_C(0x100000001), UINT64_C(0x200000001), UINT64_MAX };
    DataStoreId       ids[3][128];
    for (uint32_t t = 0; t < 3; ++t)
    {
        RegisterGroupTable(store, t + 1);
        for (uint32_t r = 0; r < 128; ++r)
        {
            GroupRow row = { (double)r, { "shared input" } };
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, t + 1, groups[r % 3], 1, &row, &ids[t][r]));
        }
    }
    // Exercise both ends and the middle of an owner's membership chain.
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[2][127]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[1][64]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[0][1]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreGroupRemove(store, groups[1]));
    for (uint32_t t = 0; t < 3; ++t)
    {
        for (uint32_t r = 0; r < 128; ++r)
        {
            double value;
            ASSERT_EQ(r % 3 == 1 ? DATASTORE_RESULT_NOT_FOUND : DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, ids[t][r], 1, &value));
            if (r % 3 != 1)
            {
                ASSERT_EQ((double)r, value);
            }
        }
    }
    // Unregistering a table must unlink its remaining members before later owner removal.
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableRemove(store, 2));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreGroupRemove(store, groups[0]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreGroupRemove(store, groups[2]));
    DataStoreMemoryStats stats;
    GetDataMemoryStats(store, &stats);
    ASSERT_EQ(0u, stats.m_Rows);
    ASSERT_EQ(0u, stats.m_BaseUsed + stats.m_PayloadUsed);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Queries may precede ownership, and caller IDs survive unchanged through removal and reuse.
TEST(DataGroups, CallerIdsAndQueryLifetime)
{
    HDataStoreContext store = DataStoreCreate(0);
    RegisterGroupTable(store, 1);
    DataStoreId        group = UINT64_C(0x1234567800000007);
    DataStoreQueryDesc desc = { { 0 }, &group, 1, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    ASSERT_EQ(0u, store->m_Groups.Size());
    GroupRow    row = { 10, { "name" } };
    DataStoreId id;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, group, 1, &row, &id));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    ASSERT_EQ(1u, DataStoreQueryGetRowCount(query));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreGroupRemove(store, group));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreRowCreate(store, 1, group + 1, 1, &row, 0));
    ASSERT_EQ(1u, store->m_Groups.Size());
    DataStoreIterator it = DataStoreQueryIter(query);
    ASSERT_TRUE(DataStoreIterNext(&it));
    DataStoreRowIterator rows = DataStoreIterRows(&it);
    ASSERT_TRUE(DataStoreRowIterNext(&rows));
    ASSERT_EQ(group, DataStoreRowIterGetGroupId(&rows));
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreGroupRemove(store, group));
    ASSERT_EQ(0u, store->m_Groups.Size());
    DataStoreId         next = group + UINT64_C(0x100000000);
    DataStoreId         replacement;
    DataStoreFieldArray fields[] = { { 1, &row.m_Value }, { 2, &row.m_Name } };
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreGroupRemove(store, group));
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreGroupRemove(store, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreateSoA(store, 1, next, 1, 2, fields, &replacement));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    ASSERT_EQ(0u, DataStoreQueryGetRowCount(query));
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, group, 1, &row, &replacement));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    ASSERT_EQ(1u, DataStoreQueryGetRowCount(query));
    DataStoreQueryEnd(query);
    double value;
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGetNumber(store, id, 1, &value));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreGroupRemove(store, group));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreGroupRemove(store, next));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Growing and reusing the owner registry must preserve full IDs and scattered membership.
TEST(DataGroups, RegistryGrowthAndReuse)
{
    HDataStoreContext store = DataStoreCreate(0);
    RegisterGroupTable(store, 1);
    GroupRow    row = { 25, { "name" } };
    DataStoreId ids[256];
    for (uint32_t i = 0; i < 256; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, UINT64_C(0x100000000) + i, 1, &row, &ids[i]));
    }
    uint32_t capacity = store->m_Groups.Capacity();
    for (uint32_t i = 0; i < 256; i += 2)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreGroupRemove(store, UINT64_C(0x100000000) + i));
    }
    for (uint32_t i = 0; i < 128; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, UINT64_C(0x200000000) + i, 1, &row, 0));
    }
    ASSERT_EQ(capacity, store->m_Groups.Capacity());
    for (uint32_t i = 0; i < 256; ++i)
    {
        double value;
        ASSERT_EQ(i & 1 ? DATASTORE_RESULT_OK : DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGetNumber(store, ids[i], 1, &value));
        if (i & 1)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreGroupRemove(store, UINT64_C(0x100000000) + i));
        }
    }
    for (uint32_t i = 0; i < 128; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreGroupRemove(store, UINT64_C(0x200000000) + i));
    }
    ASSERT_EQ(0u, store->m_Groups.Size());
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Group removal owns blob registrations even after their last row is removed explicitly.
TEST(DataGroups, BlobRegistrationsAndEmptyInstances)
{
    HDataStoreContext source = DataStoreCreate(0);
    RegisterGroupTable(source, 1);
    GroupRow row = { 10, { "resource" } };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(source, 1, 0, 1, &row, 0));
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, size, &size));
    HDataStoreBlob blob;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
    HDataStoreContext      store = DataStoreCreate(0);
    DataStoreId            group = NextTestGroupId(), other = NextTestGroupId();
    HDataStoreBlobInstance instances[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(store, blob, i == 2 ? other : group, &instances[i]));
    }
    DataStoreId survivor = MakeId(store, GetInstanceSlots(instances[2])[0]);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetString(store, MakeId(store, GetInstanceSlots(instances[0])[0]), 2, "replacement"));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, MakeId(store, GetInstanceSlots(instances[0])[0])));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobRemove(instances[1]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreGroupRemove(store, group));
    DataStoreMemoryStats stats;
    GetDataMemoryStats(store, &stats);
    ASSERT_EQ(1u, stats.m_Instances);
    ASSERT_EQ(1u, stats.m_Rows);
    double value;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, survivor, 1, &value));
    ASSERT_EQ(10.0, value);
    HDataStoreBlobInstance recreated;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(store, blob, group, &recreated));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreGroupRemove(store, group));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, survivor, 1, &value));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreGroupRemove(store, other));
    GetDataMemoryStats(store, &stats);
    ASSERT_EQ(0u, stats.m_Instances);
    ASSERT_EQ(0u, stats.m_Tables);
    DataStoreBlobDestroy(blob);
    delete[] bytes;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(source));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Ungrouped rows remain caller-owned and individual deletion does not destroy their group.
TEST(DataGroups, EmptyGroupAndUngroupedRows)
{
    HDataStoreContext store = DataStoreCreate(0);
    RegisterGroupTable(store, 1);
    DataStoreId group = NextTestGroupId();
    GroupRow    row = { 1, { "name" } };
    DataStoreId ungrouped, owned;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, 0, 1, &row, &ungrouped));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, group, 1, &row, &owned));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, owned));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, group, 1, &row, &owned));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreGroupRemove(store, group));
    double value;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, ungrouped, 1, &value));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ungrouped));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, group, 1, &row, &owned));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, owned));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreGroupRemove(store, group));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Even a blob created without rows retains a registration that group removal must release.
TEST(DataGroups, EmptyBlobs)
{
    for (uint32_t tables = 0; tables < 2; ++tables)
    {
        HDataStoreContext source = DataStoreCreate(0);
        if (tables)
        {
            RegisterGroupTable(source, 1);
        }
        uint32_t size;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, 0, 0, &size));
        uint8_t* bytes = new uint8_t[size];
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, size, &size));
        HDataStoreBlob blob;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
        HDataStoreContext      store = DataStoreCreate(0);
        DataStoreId            group = NextTestGroupId();
        HDataStoreBlobInstance instance;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(store, blob, group, &instance));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreGroupRemove(store, group));
        DataStoreMemoryStats stats;
        GetDataMemoryStats(store, &stats);
        ASSERT_EQ(0u, stats.m_Instances);
        ASSERT_EQ(0u, stats.m_Tables);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
        DataStoreBlobDestroy(blob);
        delete[] bytes;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(source));
    }
}

// Validation rejects a whole decoded batch before creating any owner records.
TEST(DataGroups, InvalidBatchDoesNotCreateGroups)
{
    HDataStoreContext store = DataStoreCreate(0);
    RegisterGroupTable(store, 1);
    DataStoreId        group = NextTestGroupId(), other = NextTestGroupId();
    DataStoreValueType types[] = { DATASTORE_TYPE_NUMBER, DATASTORE_TYPE_STRING };
    DataStoreValueData values[2] = {};
    values[0].m_Number = 1;
    values[1].m_String = "name";
    DataStoreRowDesc rows[] = {
        { group, types, values, 2 },
        { other, types, values, 1 }
    };
    DataStoreId ids[] = { 42, 43 };
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowAddBatch(store, 1, rows, 2, ids));
    ASSERT_EQ((DataStoreId)42, ids[0]);
    ASSERT_EQ((DataStoreId)43, ids[1]);
    ASSERT_EQ(0u, FindTable(store, 1)->m_Rows.Size());
    ASSERT_EQ(0u, store->m_Groups.Size());
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreGroupRemove(store, group));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Public AoS and SoA validation and zero-row calls must not register empty owners.
TEST(DataGroups, EmptyAndInvalidInputDoesNotCreateGroups)
{
    HDataStoreContext store = DataStoreCreate(0);
    DataStoreId       group = UINT64_MAX;
    RegisterGroupTable(store, 1);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, group, 0, 0, 0));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreateSoA(store, 1, group, 0, 0, 0, 0));
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowCreate(store, 1, group, 1, 0, 0));
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreRowCreateSoA(store, 1, group, 1, 0, 0, 0));
    ASSERT_EQ(0u, store->m_Groups.Size());
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreGroupRemove(store, group));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Internal construction can publish different caller groups in one validated batch.
TEST(DataGroups, MixedGroupsInOneBatch)
{
    HDataStoreContext store = DataStoreCreate(0);
    RegisterGroupTable(store, 1);
    DataStoreValueType types[] = { DATASTORE_TYPE_NUMBER, DATASTORE_TYPE_STRING };
    DataStoreValueData values[2] = {};
    values[0].m_Number = 1;
    values[1].m_String = "name";
    DataStoreRowDesc rows[] = {
        { UINT64_C(0x100000001), types, values, 2 },
        { UINT64_C(0x200000001), types, values, 2 },
        { 0, types, values, 2 }
    };
    DataStoreId ids[3];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowAddBatch(store, 1, rows, 3, ids));
    ASSERT_EQ(2u, store->m_Groups.Size());
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreGroupRemove(store, rows[0].m_Group));
    double value;
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGetNumber(store, ids[0], 1, &value));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, ids[1], 1, &value));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreGroupRemove(store, rows[1].m_Group));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, ids[2], 1, &value));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}
