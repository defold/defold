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

#include "test_data_memory.h"

static bool IsPoolStorage(const DataBlobPool* pool, const void* bytes)
{
    return (uintptr_t)bytes >= (uintptr_t)pool && (uintptr_t)bytes < (uintptr_t)pool + pool->m_AllocationSize;
}

static void GetPayloadMemory(const DataPayloadBlock* blocks, DataMemoryStats* stats)
{
    for (const DataPayloadBlock* block = blocks; block; block = block->m_Next)
    {
        ++stats->m_PayloadBlocks;
        stats->m_PayloadUsed += block->m_LiveSize;
        stats->m_PayloadCapacity += block->m_Capacity;
    }
}

void GetDataMemoryStats(HDataStore store, DataMemoryStats* out_stats)
{
    memset(out_stats, 0, sizeof(*out_stats));
    out_stats->m_Tables = store->m_Tables.Size();
    out_stats->m_StoreBytes = sizeof(DataStore) + (uint64_t)store->m_Tables.Capacity() * sizeof(DataTable*) +
    (uint64_t)store->m_Queries.Capacity() * sizeof(HDataQuery);
    // Object pool storage: query pointers, logical entries and the reverse index.
    out_stats->m_StoreBytes += (uint64_t)store->m_ActiveQueries.Capacity() * (sizeof(HDataQuery) + 3 * sizeof(uint32_t));
    out_stats->m_SlotBytes = (uint64_t)store->m_Slots.Capacity() * sizeof(DataSlot);
    for (DataBlobPool* pool = store->m_Pools; pool; pool = pool->m_Next)
    {
        out_stats->m_Instances += pool->m_Live;
        // The embedded table/row arrays are accounted below. Retain unused initial
        // row capacity here when an array has grown out of the shared allocation.
        out_stats->m_InstanceBytes += pool->m_AllocationSize - (uint64_t)pool->m_Blob->m_TableCount * sizeof(DataTable) +
        (uint64_t)pool->m_Pages.Capacity() * sizeof(uint8_t*) +
        (uint64_t)pool->m_Pages.Size() * (1u << pool->m_PageShift) * pool->m_InstanceStride;
        for (uint32_t i = 0; i < pool->m_Issued; ++i)
        {
            HDataBlobInstance instance = GetPoolInstance(pool, i);
            if (instance->m_Pool)
                GetPayloadMemory(instance->m_Payloads, out_stats);
        }
    }
    for (uint32_t i = 0; i < store->m_Queries.Size(); ++i)
    {
        HDataQuery query = store->m_Queries[i];
        out_stats->m_QueryBytes += sizeof(DataQuery) + (uint64_t)query->m_Tables.Capacity() * sizeof(DataQueryTable) +
        (uint64_t)query->m_Fields.Capacity() * sizeof(DataQueryFieldInfo) +
        (uint64_t)query->m_Groups.Capacity() * sizeof(DataGroupId) +
        (uint64_t)query->m_Ranges.Capacity() * sizeof(DataQueryRange) +
        (uint64_t)query->m_Tags.Capacity() * sizeof(uint64_t) +
        (uint64_t)query->m_Conflicts.Capacity() * sizeof(DataQueryConflict);
        for (const DataBlock* block = query->m_Bindings; block; block = block->m_Next)
            out_stats->m_QueryBytes += sizeof(DataBlock) + block->m_Capacity;
    }
    for (uint32_t i = 0; i < store->m_Tables.Size(); ++i)
    {
        const DataTable* table = store->m_Tables[i];
        out_stats->m_Rows += table->m_Rows.Size();
        out_stats->m_TableBytes += sizeof(DataTable);
        out_stats->m_RowMetadataBytes += (uint64_t)table->m_Rows.Capacity() * sizeof(DataRow);
        if (!table->m_Pool)
        {
            const DataOwnedTable* owned = table->m_Owned;
            out_stats->m_TableBytes += sizeof(DataOwnedTable) + (uint64_t)owned->m_Tags.Capacity() * sizeof(uint64_t) +
            (uint64_t)owned->m_Fields.Capacity() * sizeof(DataFieldMeta);
            out_stats->m_BaseRowBytes += owned->m_BaseRows.Capacity();
            for (const DataPayloadBlock* block = owned->m_BaseValues; block; block = block->m_Next)
            {
                ++out_stats->m_BaseBlocks;
                out_stats->m_BaseUsed += block->m_LiveSize;
                out_stats->m_BaseCapacity += block->m_Capacity;
            }
        }
        else
        {
            if (IsPoolStorage(table->m_Pool, table->m_Rows.Begin()))
                out_stats->m_InstanceBytes -= (uint64_t)table->m_Rows.Capacity() * sizeof(DataRow);
            if (IsPoolStorage(table->m_Pool, table->m_Values.Begin()))
                out_stats->m_InstanceBytes -= table->m_Values.Capacity();
        }
        if (table->m_Structs)
        {
            out_stats->m_TableBytes += sizeof(DataStructStorage) + (uint64_t)table->m_Structs->m_Tables.Capacity() * sizeof(DataStructTable*);
            for (uint32_t j = 0; j < table->m_Structs->m_Tables.Size(); ++j)
            {
                const DataStructTable* child = table->m_Structs->m_Tables[j];
                out_stats->m_TableBytes += sizeof(DataStructTable) + (uint64_t)child->m_Fields.Capacity() * sizeof(DataStructField);
                out_stats->m_StructRows += child->m_RowCount;
                out_stats->m_StructRowBytes += child->m_Values.Capacity();
                out_stats->m_ValueBytes += child->m_Values.Capacity();
            }
        }
        if (!table->m_IsView)
            out_stats->m_ValueBytes += table->m_Values.Capacity();
        out_stats->m_TableBytes += (uint64_t)table->m_ViewAliases.Capacity() * sizeof(DataTable*);
        GetPayloadMemory(table->m_Payloads, out_stats);
    }
    out_stats->m_TotalBytes = out_stats->m_StoreBytes + out_stats->m_TableBytes + out_stats->m_RowMetadataBytes +
    out_stats->m_SlotBytes + out_stats->m_BaseRowBytes + out_stats->m_ValueBytes + out_stats->m_InstanceBytes +
    out_stats->m_QueryBytes + out_stats->m_BaseCapacity + out_stats->m_PayloadCapacity +
    (out_stats->m_BaseBlocks + out_stats->m_PayloadBlocks) * sizeof(DataPayloadBlock);
}
