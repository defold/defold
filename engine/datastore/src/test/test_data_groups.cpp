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
    EXPECT_EQ(DS_RESULT_OK, DSTableAdd(store, &table));
}

// Removing an owner visits its scattered rows across tables without invalidating survivors.
TEST(DataGroups, ScatteredRowsAcrossTables)
{
    HDSContext store = DSStoreCreate(0);
    DSId       groups[] = { UINT64_C(0x100000001), UINT64_C(0x200000001), UINT64_MAX };
    DSId       ids[3][128];
    for (uint32_t t = 0; t < 3; ++t)
    {
        RegisterGroupTable(store, t + 1);
        for (uint32_t r = 0; r < 128; ++r)
        {
            GroupRow row = { .m_Value = (double)r, .m_Name = { .m_String = "shared input" } };
            ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, t + 1, groups[r % 3], 1, &row, &ids[t][r]));
        }
    }
    // Exercise both ends and the middle of an owner's membership chain.
    ASSERT_EQ(DS_RESULT_OK, DSRowRemove(store, ids[2][127]));
    ASSERT_EQ(DS_RESULT_OK, DSRowRemove(store, ids[1][64]));
    ASSERT_EQ(DS_RESULT_OK, DSRowRemove(store, ids[0][1]));
    ASSERT_EQ(DS_RESULT_OK, DSGroupRemove(store, groups[1]));
    for (uint32_t t = 0; t < 3; ++t)
        for (uint32_t r = 0; r < 128; ++r)
        {
            double value;
            ASSERT_EQ(r % 3 == 1 ? DS_RESULT_NOT_FOUND : DS_RESULT_OK, DSFieldGetNumber(store, ids[t][r], 1, &value));
            if (r % 3 != 1)
                ASSERT_EQ((double)r, value);
        }
    // Unregistering a table must unlink its remaining members before later owner removal.
    ASSERT_EQ(DS_RESULT_OK, DSTableRemove(store, 2));
    ASSERT_EQ(DS_RESULT_OK, DSGroupRemove(store, groups[0]));
    ASSERT_EQ(DS_RESULT_OK, DSGroupRemove(store, groups[2]));
    DSMemoryStats stats;
    GetDataMemoryStats(store, &stats);
    ASSERT_EQ(0u, stats.m_Rows);
    ASSERT_EQ(0u, stats.m_BaseUsed + stats.m_PayloadUsed);
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Queries may precede ownership, and caller IDs survive unchanged through removal and reuse.
TEST(DataGroups, CallerIdsAndQueryLifetime)
{
    HDSContext store = DSStoreCreate(0);
    RegisterGroupTable(store, 1);
    DSId        group = UINT64_C(0x1234567800000007);
    DSQueryDesc desc = { .m_GroupIds = &group, .m_GroupIdCount = 1 };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(store, &desc, &query));
    ASSERT_EQ(0u, store->m_Groups.Size());
    GroupRow row = { .m_Value = 10, .m_Name = { .m_String = "name" } };
    DSId     id;
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 1, group, 1, &row, &id));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    ASSERT_EQ(1u, DSQueryGetRowCount(query));
    ASSERT_EQ(DS_RESULT_LOCKED, DSGroupRemove(store, group));
    ASSERT_EQ(DS_RESULT_LOCKED, DSRowCreate(store, 1, group + 1, 1, &row, 0));
    ASSERT_EQ(1u, store->m_Groups.Size());
    DSIterator it = DSQueryIter(query);
    ASSERT_TRUE(DSIterNext(&it));
    DSRowIterator rows = DSIterRows(&it);
    ASSERT_TRUE(DSRowIterNext(&rows));
    ASSERT_EQ(group, DSRowIterGetGroupId(&rows));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSGroupRemove(store, group));
    ASSERT_EQ(0u, store->m_Groups.Size());
    DSId      next = group + UINT64_C(0x100000000);
    DSId      replacement;
    DSFieldArray fields[] = { { .m_Field = 1, .m_Values = &row.m_Value }, { .m_Field = 2, .m_Values = &row.m_Name } };
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSGroupRemove(store, group));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSGroupRemove(store, 0));
    ASSERT_EQ(DS_RESULT_OK, DSRowCreateSoA(store, 1, next, 1, 2, fields, &replacement));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    ASSERT_EQ(0u, DSQueryGetRowCount(query));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 1, group, 1, &row, &replacement));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    ASSERT_EQ(1u, DSQueryGetRowCount(query));
    DSQueryEnd(query);
    double value;
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGetNumber(store, id, 1, &value));
    ASSERT_EQ(DS_RESULT_OK, DSGroupRemove(store, group));
    ASSERT_EQ(DS_RESULT_OK, DSGroupRemove(store, next));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Growing and reusing the owner registry must preserve full IDs and scattered membership.
TEST(DataGroups, RegistryGrowthAndReuse)
{
    HDSContext store = DSStoreCreate(0);
    RegisterGroupTable(store, 1);
    GroupRow row = { .m_Value = 25, .m_Name = { .m_String = "name" } };
    DSId     ids[256];
    for (uint32_t i = 0; i < 256; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 1, UINT64_C(0x100000000) + i, 1, &row, &ids[i]));
    uint32_t capacity = store->m_Groups.Capacity();
    for (uint32_t i = 0; i < 256; i += 2)
        ASSERT_EQ(DS_RESULT_OK, DSGroupRemove(store, UINT64_C(0x100000000) + i));
    for (uint32_t i = 0; i < 128; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 1, UINT64_C(0x200000000) + i, 1, &row, 0));
    ASSERT_EQ(capacity, store->m_Groups.Capacity());
    for (uint32_t i = 0; i < 256; ++i)
    {
        double value;
        ASSERT_EQ(i & 1 ? DS_RESULT_OK : DS_RESULT_NOT_FOUND, DSFieldGetNumber(store, ids[i], 1, &value));
        if (i & 1)
            ASSERT_EQ(DS_RESULT_OK, DSGroupRemove(store, UINT64_C(0x100000000) + i));
    }
    for (uint32_t i = 0; i < 128; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSGroupRemove(store, UINT64_C(0x200000000) + i));
    ASSERT_EQ(0u, store->m_Groups.Size());
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Group removal owns blob registrations even after their last row is removed explicitly.
TEST(DataGroups, BlobRegistrationsAndEmptyInstances)
{
    HDSContext source = DSStoreCreate(0);
    RegisterGroupTable(source, 1);
    GroupRow row = { .m_Value = 10, .m_Name = { .m_String = "resource" } };
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(source, 1, 0, 1, &row, 0));
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSBlobWrite(source, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSBlobWrite(source, bytes, size, &size));
    HDSBlob blob;
    ASSERT_EQ(DS_RESULT_OK, DSBlobLoad(bytes, size, &blob));
    HDSContext      store = DSStoreCreate(0);
    DSId      group = NextTestGroupId(), other = NextTestGroupId();
    HDSBlobInstance instances[4];
    for (uint32_t i = 0; i < 4; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSBlobAdd(store, blob, i == 2 ? other : group, &instances[i]));
    DSId survivor = MakeId(store, GetInstanceSlots(instances[2])[0]);
    ASSERT_EQ(DS_RESULT_OK, DSFieldSetString(store, MakeId(store, GetInstanceSlots(instances[0])[0]), 2, "replacement"));
    ASSERT_EQ(DS_RESULT_OK, DSRowRemove(store, MakeId(store, GetInstanceSlots(instances[0])[0])));
    ASSERT_EQ(DS_RESULT_OK, DSBlobRemove(instances[1]));
    ASSERT_EQ(DS_RESULT_OK, DSGroupRemove(store, group));
    DSMemoryStats stats;
    GetDataMemoryStats(store, &stats);
    ASSERT_EQ(1u, stats.m_Instances);
    ASSERT_EQ(1u, stats.m_Rows);
    double value;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, survivor, 1, &value));
    ASSERT_EQ(10.0, value);
    HDSBlobInstance recreated;
    ASSERT_EQ(DS_RESULT_OK, DSBlobAdd(store, blob, group, &recreated));
    ASSERT_EQ(DS_RESULT_OK, DSGroupRemove(store, group));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, survivor, 1, &value));
    ASSERT_EQ(DS_RESULT_OK, DSGroupRemove(store, other));
    GetDataMemoryStats(store, &stats);
    ASSERT_EQ(0u, stats.m_Instances);
    ASSERT_EQ(0u, stats.m_Tables);
    DSBlobDestroy(blob);
    delete[] bytes;
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(source));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Ungrouped rows remain caller-owned and individual deletion does not destroy their group.
TEST(DataGroups, EmptyGroupAndUngroupedRows)
{
    HDSContext store = DSStoreCreate(0);
    RegisterGroupTable(store, 1);
    DSId                group = NextTestGroupId();
    GroupRow  row = { .m_Value = 1, .m_Name = { .m_String = "name" } };
    DSId      ungrouped, owned;
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 1, 0, 1, &row, &ungrouped));
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 1, group, 1, &row, &owned));
    ASSERT_EQ(DS_RESULT_OK, DSRowRemove(store, owned));
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 1, group, 1, &row, &owned));
    ASSERT_EQ(DS_RESULT_OK, DSGroupRemove(store, group));
    double value;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, ungrouped, 1, &value));
    ASSERT_EQ(DS_RESULT_OK, DSRowRemove(store, ungrouped));
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 1, group, 1, &row, &owned));
    ASSERT_EQ(DS_RESULT_OK, DSRowRemove(store, owned));
    ASSERT_EQ(DS_RESULT_OK, DSGroupRemove(store, group));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Even a blob created without rows retains a registration that group removal must release.
TEST(DataGroups, EmptyBlobs)
{
    for (uint32_t tables = 0; tables < 2; ++tables)
    {
        HDSContext source = DSStoreCreate(0);
        if (tables)
            RegisterGroupTable(source, 1);
        uint32_t size;
        ASSERT_EQ(DS_RESULT_OK, DSBlobWrite(source, 0, 0, &size));
        uint8_t* bytes = new uint8_t[size];
        ASSERT_EQ(DS_RESULT_OK, DSBlobWrite(source, bytes, size, &size));
        HDSBlob blob;
        ASSERT_EQ(DS_RESULT_OK, DSBlobLoad(bytes, size, &blob));
        HDSContext      store = DSStoreCreate(0);
        DSId            group = NextTestGroupId();
        HDSBlobInstance instance;
        ASSERT_EQ(DS_RESULT_OK, DSBlobAdd(store, blob, group, &instance));
        ASSERT_EQ(DS_RESULT_OK, DSGroupRemove(store, group));
        DSMemoryStats stats;
        GetDataMemoryStats(store, &stats);
        ASSERT_EQ(0u, stats.m_Instances);
        ASSERT_EQ(0u, stats.m_Tables);
        ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
        DSBlobDestroy(blob);
        delete[] bytes;
        ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(source));
    }
}

// Validation rejects a whole decoded batch before creating any owner records.
TEST(DataGroups, InvalidBatchDoesNotCreateGroups)
{
    HDSContext store = DSStoreCreate(0);
    RegisterGroupTable(store, 1);
    DSId      group = NextTestGroupId(), other = NextTestGroupId();
    DSValueType types[] = { DS_TYPE_NUMBER, DS_TYPE_STRING };
    DSValueData values[] = { { .m_Number = 1 }, { .m_String = "name" } };
    DSRowDesc   rows[] = {
        { .m_Group = group, .m_Types = types, .m_Values = values, .m_ValueCount = 2 },
        { .m_Group = other, .m_Types = types, .m_Values = values, .m_ValueCount = 1 }
    };
    DSId ids[] = { 42, 43 };
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRowAddBatch(store, 1, rows, 2, ids));
    ASSERT_EQ((DSId)42, ids[0]);
    ASSERT_EQ((DSId)43, ids[1]);
    ASSERT_EQ(0u, FindTable(store, 1)->m_Rows.Size());
    ASSERT_EQ(0u, store->m_Groups.Size());
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSGroupRemove(store, group));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Public AoS and SoA validation and zero-row calls must not register empty owners.
TEST(DataGroups, EmptyAndInvalidInputDoesNotCreateGroups)
{
    HDSContext store = DSStoreCreate(0);
    DSId       group = UINT64_MAX;
    RegisterGroupTable(store, 1);
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 1, group, 0, 0, 0));
    ASSERT_EQ(DS_RESULT_OK, DSRowCreateSoA(store, 1, group, 0, 0, 0, 0));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRowCreate(store, 1, group, 1, 0, 0));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRowCreateSoA(store, 1, group, 1, 0, 0, 0));
    ASSERT_EQ(0u, store->m_Groups.Size());
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSGroupRemove(store, group));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Internal construction can publish different caller groups in one validated batch.
TEST(DataGroups, MixedGroupsInOneBatch)
{
    HDSContext store = DSStoreCreate(0);
    RegisterGroupTable(store, 1);
    DSValueType types[] = { DS_TYPE_NUMBER, DS_TYPE_STRING };
    DSValueData values[] = { { .m_Number = 1 }, { .m_String = "name" } };
    DSRowDesc   rows[] = {
        { .m_Group = UINT64_C(0x100000001), .m_Types = types, .m_Values = values, .m_ValueCount = 2 },
        { .m_Group = UINT64_C(0x200000001), .m_Types = types, .m_Values = values, .m_ValueCount = 2 },
        { .m_Group = 0, .m_Types = types, .m_Values = values, .m_ValueCount = 2 }
    };
    DSId ids[3];
    ASSERT_EQ(DS_RESULT_OK, DSRowAddBatch(store, 1, rows, 3, ids));
    ASSERT_EQ(2u, store->m_Groups.Size());
    ASSERT_EQ(DS_RESULT_OK, DSGroupRemove(store, rows[0].m_Group));
    double value;
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGetNumber(store, ids[0], 1, &value));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, ids[1], 1, &value));
    ASSERT_EQ(DS_RESULT_OK, DSGroupRemove(store, rows[1].m_Group));
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, ids[2], 1, &value));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}
