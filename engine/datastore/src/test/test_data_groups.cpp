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
    double      m_Value;
    DSReference m_Name;
};

static void RegisterGroupTable(HDSContext store, uint64_t type)
{
    DSFieldDesc fields[] = {
        { .m_Field = 1, .m_Type = DS_TYPE_NUMBER, .m_Offset = offsetof(GroupRow, m_Value) },
        { .m_Field = 2, .m_Type = DS_TYPE_STRING, .m_Offset = offsetof(GroupRow, m_Name) }
    };
    DSTableDesc table = { .m_Type = type, .m_Fields = fields, .m_FieldCount = 2, .m_RowStride = sizeof(GroupRow) };
    EXPECT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
}

// Removing an owner visits its scattered rows across tables without invalidating survivors.
TEST(DataGroups, ScatteredRowsAcrossTables)
{
    HDSContext store = DSCreateStore(0);
    DSGroupId  groups[] = { CreateTestGroup(store), CreateTestGroup(store), CreateTestGroup(store) };
    DSId       ids[3][128];
    for (uint32_t t = 0; t < 3; ++t)
    {
        RegisterGroupTable(store, t + 1);
        for (uint32_t r = 0; r < 128; ++r)
        {
            GroupRow row = { .m_Value = (double)r, .m_Name = { .m_String = "shared input" } };
            ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, t + 1, groups[r % 3], 1, &row, &ids[t][r]));
        }
    }
    // Exercise both ends and the middle of an owner's membership chain.
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[2][127]));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[1][64]));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ids[0][1]));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveGroup(store, groups[1]));
    for (uint32_t t = 0; t < 3; ++t)
        for (uint32_t r = 0; r < 128; ++r)
        {
            double value;
            ASSERT_EQ(r % 3 == 1 ? DS_RESULT_NOT_FOUND : DS_RESULT_OK, DSFieldGetNumber(store, ids[t][r], 1, &value));
            if (r % 3 != 1)
                ASSERT_EQ((double)r, value);
        }
    // Unregistering a table must unlink its remaining members before later owner removal.
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterTable(store, 2));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveGroup(store, groups[0]));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveGroup(store, groups[2]));
    DSMemoryStats stats;
    GetDataMemoryStats(store, &stats);
    ASSERT_EQ(0u, stats.m_Rows);
    ASSERT_EQ(0u, stats.m_BaseUsed + stats.m_PayloadUsed);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Stale handles cannot create or delete rows belonging to a reused group slot.
TEST(DataGroups, GenerationAndQueryLifetime)
{
    HDSContext store = DSCreateStore(0);
    RegisterGroupTable(store, 1);
    DSGroupId   group = CreateTestGroup(store);
    DSQueryDesc desc = { .m_GroupIds = &group, .m_GroupIdCount = 1 };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    GroupRow row = { .m_Value = 10, .m_Name = { .m_String = "name" } };
    DSId     id;
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, group, 1, &row, &id));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    ASSERT_EQ(1u, DSQueryGetRowCount(query));
    ASSERT_EQ(DS_RESULT_LOCKED, DSRemoveGroup(store, group));
    // Empty group creation can grow the registry while iterators retain their rows.
    for (uint32_t i = 0; i < 100; ++i)
        CreateTestGroup(store);
    DSIterator it = DSQueryIter(query);
    ASSERT_TRUE(DSIterNext(&it));
    DSRowIterator rows = DSIterRows(&it);
    ASSERT_TRUE(DSRowIterNext(&rows));
    ASSERT_EQ(group, DSRowIterGetGroupId(&rows));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSRemoveGroup(store, group));
    DSGroupId next = CreateTestGroup(store);
    ASSERT_NE(group, next);
    ASSERT_EQ((uint32_t)group, (uint32_t)next);
    DSId untouched = 123;
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSCreateRows(store, 1, group, 1, &row, &untouched));
    ASSERT_EQ((DSId)123, untouched);
    DSFieldArray fields[] = { { .m_Field = 1, .m_Values = &row.m_Value }, { .m_Field = 2, .m_Values = &row.m_Name } };
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSCreateRowsSoA(store, 1, group, 1, 2, fields, &untouched));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSCreateRows(store, 1, 7, 1, &row, &untouched));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSRemoveGroup(store, group));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSRemoveGroup(store, 0));
    ASSERT_EQ(DS_RESULT_OK, DSCreateRowsSoA(store, 1, next, 1, 2, fields, &untouched));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    ASSERT_EQ(0u, DSQueryGetRowCount(query));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSRemoveGroup(store, next));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Exhausting a generation retires the slot instead of making an old group live again.
TEST(DataGroups, ExhaustedGeneration)
{
    HDSContext store = DSCreateStore(0);
    DSGroupId  original = CreateTestGroup(store);
    store->m_Groups[(uint32_t)original].m_Generation = UINT32_MAX;
    DSGroupId last = ((uint64_t)UINT32_MAX << 32) | (uint32_t)original;
    ASSERT_EQ(DS_RESULT_OK, DSRemoveGroup(store, last));
    DSGroupId next = CreateTestGroup(store);
    ASSERT_NE((uint32_t)original, (uint32_t)next);
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSRemoveGroup(store, original));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSRemoveGroup(store, last));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Group removal owns blob registrations even after their last row is removed explicitly.
TEST(DataGroups, BlobRegistrationsAndEmptyInstances)
{
    HDSContext source = DSCreateStore(0);
    RegisterGroupTable(source, 1);
    GroupRow row = { .m_Value = 10, .m_Name = { .m_String = "resource" } };
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(source, 1, 0, 1, &row, 0));
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, bytes, size, &size));
    HDSBlob blob;
    ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
    HDSContext      store = DSCreateStore(0);
    DSGroupId       group = CreateTestGroup(store), other = CreateTestGroup(store);
    HDSBlobInstance instances[4];
    for (uint32_t i = 0; i < 4; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSAddBlob(store, blob, i == 2 ? other : group, &instances[i]));
    DSId survivor = MakeId(store, GetInstanceSlots(instances[2])[0]);
    ASSERT_EQ(DS_RESULT_OK, DSSetFieldString(store, MakeId(store, GetInstanceSlots(instances[0])[0]), 2, "replacement"));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, MakeId(store, GetInstanceSlots(instances[0])[0])));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveBlob(instances[1]));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveGroup(store, group));
    DSMemoryStats stats;
    GetDataMemoryStats(store, &stats);
    ASSERT_EQ(1u, stats.m_Instances);
    ASSERT_EQ(1u, stats.m_Rows);
    double value;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, survivor, 1, &value));
    ASSERT_EQ(10.0, value);
    HDSBlobInstance untouched = instances[2];
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSAddBlob(store, blob, group, &untouched));
    ASSERT_EQ(instances[2], untouched);
    ASSERT_EQ(DS_RESULT_OK, DSRemoveGroup(store, other));
    GetDataMemoryStats(store, &stats);
    ASSERT_EQ(0u, stats.m_Instances);
    ASSERT_EQ(0u, stats.m_Tables);
    DSDestroyBlob(blob);
    delete[] bytes;
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(source));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Ungrouped rows remain caller-owned and individual deletion does not destroy their group.
TEST(DataGroups, EmptyGroupAndUngroupedRows)
{
    HDSContext store = DSCreateStore(0);
    RegisterGroupTable(store, 1);
    DSGroupId group = CreateTestGroup(store);
    GroupRow  row = { .m_Value = 1, .m_Name = { .m_String = "name" } };
    DSId      ungrouped, owned;
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, 0, 1, &row, &ungrouped));
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, group, 1, &row, &owned));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, owned));
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, group, 1, &row, &owned));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveGroup(store, group));
    double value;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, ungrouped, 1, &value));
    ASSERT_EQ(DS_RESULT_OK, DSRemoveRow(store, ungrouped));
    group = CreateTestGroup(store);
    ASSERT_EQ(DS_RESULT_OK, DSRemoveGroup(store, group));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Even a blob created without rows retains a registration that group removal must release.
TEST(DataGroups, EmptyBlobs)
{
    for (uint32_t tables = 0; tables < 2; ++tables)
    {
        HDSContext source = DSCreateStore(0);
        if (tables)
            RegisterGroupTable(source, 1);
        uint32_t size;
        ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, 0, 0, &size));
        uint8_t* bytes = new uint8_t[size];
        ASSERT_EQ(DS_RESULT_OK, DSWriteBlob(source, bytes, size, &size));
        HDSBlob blob;
        ASSERT_EQ(DS_RESULT_OK, DSLoadBlob(bytes, size, &blob));
        HDSContext      store = DSCreateStore(0);
        DSGroupId       group = CreateTestGroup(store);
        HDSBlobInstance instance;
        ASSERT_EQ(DS_RESULT_OK, DSAddBlob(store, blob, group, &instance));
        ASSERT_EQ(DS_RESULT_OK, DSRemoveGroup(store, group));
        DSMemoryStats stats;
        GetDataMemoryStats(store, &stats);
        ASSERT_EQ(0u, stats.m_Instances);
        ASSERT_EQ(0u, stats.m_Tables);
        ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
        DSDestroyBlob(blob);
        delete[] bytes;
        ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(source));
    }
}

// A bad group anywhere in a decoded batch rejects all rows before modifying the store.
TEST(DataGroups, InvalidBatchIsAtomic)
{
    HDSContext store = DSCreateStore(0);
    RegisterGroupTable(store, 1);
    DSGroupId group = CreateTestGroup(store), removed = CreateTestGroup(store);
    ASSERT_EQ(DS_RESULT_OK, DSRemoveGroup(store, removed));
    DSValueType types[] = { DS_TYPE_NUMBER, DS_TYPE_STRING };
    DSValueData values[] = { { .m_Number = 1 }, { .m_String = "name" } };
    DSRowDesc   rows[] = {
        { .m_Group = group, .m_Types = types, .m_Values = values, .m_ValueCount = 2 },
        { .m_Group = removed, .m_Types = types, .m_Values = values, .m_ValueCount = 2 }
    };
    DSId ids[] = { 42, 43 };
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSAddRows(store, 1, rows, 2, ids));
    ASSERT_EQ((DSId)42, ids[0]);
    ASSERT_EQ((DSId)43, ids[1]);
    ASSERT_EQ(0u, FindTable(store, 1)->m_Rows.Size());
    ASSERT_EQ(DS_RESULT_OK, DSRemoveGroup(store, group));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}
