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

static bool IsPoolStorage(const DSBlobPool* pool, const void* bytes)
{
    return (uintptr_t)bytes >= (uintptr_t)pool && (uintptr_t)bytes < (uintptr_t)pool + pool->m_AllocationSize;
}

static void GetPayloadMemory(const DSPayloadBlock* blocks, DSMemoryStats* stats)
{
    for (const DSPayloadBlock* block = blocks; block; block = block->m_Next)
    {
        ++stats->m_PayloadBlocks;
        stats->m_PayloadUsed += block->m_LiveSize;
        stats->m_PayloadCapacity += block->m_Capacity;
    }
}

void GetDataMemoryStats(HDSContext store, DSMemoryStats* out_stats)
{
    memset(out_stats, 0, sizeof(*out_stats));
    out_stats->m_Tables = store->m_Tables.Size();
    out_stats->m_StoreBytes = sizeof(DSStore) + (uint64_t)store->m_Tables.Capacity() * sizeof(DSTable*) +
    (uint64_t)store->m_Queries.Capacity() * sizeof(HDSQuery);
    // Object pool storage: query pointers, logical entries and the reverse index.
    out_stats->m_StoreBytes += (uint64_t)store->m_ActiveQueries.Capacity() * (sizeof(HDSQuery) + 3 * sizeof(uint32_t));
    out_stats->m_StoreBytes += (uint64_t)store->m_Pages.Capacity() * sizeof(DSRowPage);
    for (uint32_t p = 0; p < store->m_Pages.Size(); ++p)
        if (store->m_Pages[p].m_Data)
            out_stats->m_PageSlackBytes += store->m_PageSize;
    out_stats->m_SlotBytes = (uint64_t)store->m_Slots.Capacity() * sizeof(DSSlot);
    for (DSBlobPool* pool = store->m_Pools; pool; pool = pool->m_Next)
    {
        out_stats->m_Instances += pool->m_Live;
        // The embedded table/row arrays are accounted below. Retain unused initial
        // row capacity here when an array has grown out of the shared allocation.
        out_stats->m_InstanceBytes += pool->m_AllocationSize - (uint64_t)pool->m_Blob->m_TableCount * sizeof(DSTable) +
        (uint64_t)pool->m_Pages.Capacity() * sizeof(uint8_t*) +
        (uint64_t)pool->m_Pages.Size() * (1u << pool->m_PageShift) * pool->m_InstanceStride;
        for (uint32_t i = 0; i < pool->m_Issued; ++i)
        {
            HDSBlobInstance instance = GetPoolInstance(pool, i);
            if (instance->m_Pool)
                GetPayloadMemory(instance->m_Payloads, out_stats);
        }
    }
    for (uint32_t i = 0; i < store->m_Queries.Size(); ++i)
    {
        HDSQuery query = store->m_Queries[i];
        out_stats->m_QueryBytes += sizeof(DSQuery) + (uint64_t)query->m_Tables.Capacity() * sizeof(DSQueryTable) +
        (uint64_t)query->m_Fields.Capacity() * sizeof(DSQueryFieldInfo) +
        (uint64_t)query->m_Groups.Capacity() * sizeof(DSGroupId) +
        (uint64_t)query->m_Ranges.Capacity() * sizeof(DSQueryRange) +
        (uint64_t)query->m_Tags.Capacity() * sizeof(uint64_t) +
        (uint64_t)query->m_Conflicts.Capacity() * sizeof(DSQueryConflict);
        for (const DSBlock* block = query->m_Bindings; block; block = block->m_Next)
            out_stats->m_QueryBytes += sizeof(DSBlock) + block->m_Capacity;
    }
    for (uint32_t i = 0; i < store->m_Tables.Size(); ++i)
    {
        const DSTable* table = store->m_Tables[i];
        out_stats->m_Rows += table->m_Rows.Size();
        out_stats->m_TableBytes += sizeof(DSTable);
        out_stats->m_RowMetadataBytes += (uint64_t)table->m_Rows.Capacity() * sizeof(DSRow);
        if (!table->m_Pool)
        {
            const DSOwnedTable* owned = table->m_Owned;
            out_stats->m_TableBytes += sizeof(DSOwnedTable) + (uint64_t)owned->m_Tags.Capacity() * sizeof(uint64_t) +
            (uint64_t)owned->m_Fields.Capacity() * sizeof(DSFieldMeta);
            for (const DSPayloadBlock* block = owned->m_BaseValues; block; block = block->m_Next)
            {
                ++out_stats->m_BaseBlocks;
                out_stats->m_BaseUsed += block->m_LiveSize;
                out_stats->m_BaseCapacity += block->m_Capacity;
            }
        }
        else
        {
            if (IsPoolStorage(table->m_Pool, table->m_Rows.Begin()))
                out_stats->m_InstanceBytes -= (uint64_t)table->m_Rows.Capacity() * sizeof(DSRow);
        }
        if (table->m_Structs)
        {
            out_stats->m_TableBytes += sizeof(DSStructStorage) + (uint64_t)table->m_Structs->m_Tables.Capacity() * sizeof(DSStructTable*);
            for (uint32_t j = 0; j < table->m_Structs->m_Tables.Size(); ++j)
            {
                const DSStructTable* child = table->m_Structs->m_Tables[j];
                out_stats->m_TableBytes += sizeof(DSStructTable) + (uint64_t)child->m_Fields.Capacity() * sizeof(DSStructField);
                out_stats->m_StructRows += child->m_RowCount;
                out_stats->m_StructRowBytes += child->m_Values.Capacity();
                out_stats->m_ValueBytes += child->m_Values.Capacity();
            }
        }
        out_stats->m_TableBytes += (uint64_t)table->m_Chunks.Capacity() * sizeof(DSRowChunk);
        for (uint32_t c = 0; c < table->m_Chunks.Size(); ++c)
        {
            uint64_t values = (uint64_t)table->m_RowStride << table->m_ChunkShift;
            out_stats->m_ValueBytes += values;
            out_stats->m_BaseRowBytes += table->m_DefaultOffset;
            if (table->m_Chunks[c].m_Page != UINT32_MAX)
                out_stats->m_PageSlackBytes -= values + table->m_DefaultOffset;
        }
        out_stats->m_TableBytes += (uint64_t)table->m_ViewAliases.Capacity() * sizeof(DSTable*);
        GetPayloadMemory(table->m_Payloads, out_stats);
    }
    out_stats->m_TotalBytes = out_stats->m_StoreBytes + out_stats->m_TableBytes + out_stats->m_RowMetadataBytes + out_stats->m_PageSlackBytes +
    out_stats->m_SlotBytes + out_stats->m_BaseRowBytes + out_stats->m_ValueBytes + out_stats->m_InstanceBytes +
    out_stats->m_QueryBytes + out_stats->m_BaseCapacity + out_stats->m_PayloadCapacity +
    (out_stats->m_BaseBlocks + out_stats->m_PayloadBlocks) * sizeof(DSPayloadBlock);
}
