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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <new>
#include <dlib/hash.h>
#include "data.h"

// Resolved once per input column, never per row. Values are copied directly into
// the table's reset storage, then published through the same path as native rows.
struct DataColumnBinding
{
    const uint8_t* m_Values; // Borrowed contiguous native field values.
    uint32_t       m_Offset; // Destination byte offset within each row.
    uint32_t       m_Size;   // Source stride and copy size; UINT32_MAX until bound.
};

static const uint32_t INVALID_SLOT = UINT32_MAX;

void                  ReserveData(DataBlock** blocks, size_t size, size_t minimum_capacity)
{
    const size_t header_size = (sizeof(DataBlock) + 7) & ~(size_t)7;
    DataBlock*   block = *blocks;
    size_t       offset = block ? (block->m_Size + 7) & ~(size_t)7 : 0;
    if (!block || offset > block->m_Capacity || size > block->m_Capacity - offset)
    {
        size_t capacity = size > minimum_capacity ? size : minimum_capacity;
        assert(capacity <= SIZE_MAX - header_size);
        block = (DataBlock*)new uint8_t[header_size + capacity];
        block->m_Next = *blocks;
        block->m_Size = 0;
        block->m_Capacity = capacity;
        *blocks = block;
    }
}

void* AllocateData(DataBlock** blocks, size_t size, size_t minimum_capacity)
{
    ReserveData(blocks, size, minimum_capacity);
    const size_t header_size = (sizeof(DataBlock) + 7) & ~(size_t)7;
    DataBlock*   block = *blocks;
    size_t       offset = (block->m_Size + 7) & ~(size_t)7;
    void*        result = (uint8_t*)block + header_size + offset;
    block->m_Size = offset + size;
    return result;
}

void DeleteBlocks(DataBlock** blocks)
{
    while (*blocks)
    {
        DataBlock* block = *blocks;
        *blocks = block->m_Next;
        delete[] (uint8_t*)block;
    }
}

static bool IsPoolStorage(const DataBlobPool* pool, const void* bytes)
{
    return (uintptr_t)bytes >= (uintptr_t)pool && (uintptr_t)bytes < (uintptr_t)pool + pool->m_AllocationSize;
}

static void GetPayloadMemory(const DataBlock* blocks, DataMemoryStats* stats)
{
    for (const DataBlock* block = blocks; block; block = block->m_Next)
    {
        ++stats->m_PayloadBlocks;
        stats->m_PayloadUsed += block->m_Size;
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
        (uint64_t)query->m_Tags.Capacity() * sizeof(uint64_t);
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
            for (const DataBlock* block = owned->m_BaseValues; block; block = block->m_Next)
            {
                ++out_stats->m_BaseBlocks;
                out_stats->m_BaseUsed += block->m_Size;
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
        out_stats->m_ValueBytes += table->m_Values.Capacity();
        GetPayloadMemory(table->m_Payloads, out_stats);
    }
    out_stats->m_TotalBytes = out_stats->m_StoreBytes + out_stats->m_TableBytes + out_stats->m_RowMetadataBytes +
    out_stats->m_SlotBytes + out_stats->m_BaseRowBytes + out_stats->m_ValueBytes + out_stats->m_InstanceBytes +
    out_stats->m_QueryBytes + out_stats->m_BaseCapacity + out_stats->m_PayloadCapacity +
    (out_stats->m_BaseBlocks + out_stats->m_PayloadBlocks) * sizeof(DataBlock);
}

// Finds an independently registered table; blob instances are managed by their handle.
DataTable* FindTable(HDataStore store, uint64_t type)
{
    for (uint32_t i = 0; i < store->m_Tables.Size(); ++i)
    {
        if (!store->m_Tables[i]->m_Pool && store->m_Tables[i]->m_Type == type)
            return store->m_Tables[i];
    }
    return 0;
}

static void ReleaseSlot(HDataStore store, uint32_t index)
{
    DataSlot* slot = &store->m_Slots[index];
    slot->m_Table = 0;
    // Retire exhausted slots rather than ever resurrecting an old ID.
    if (slot->m_Generation != UINT32_MAX)
    {
        ++slot->m_Generation;
        slot->m_NextFree = store->m_FreeSlot;
        store->m_FreeSlot = index;
        ++store->m_FreeSlotCount;
    }
}

static void ReserveSlots(HDataStore store, uint32_t count)
{
    uint32_t new_slots = count > store->m_FreeSlotCount ? count - store->m_FreeSlotCount : 0;

    Reserve(store->m_Slots, store->m_Slots.Size() + new_slots);
}

// Detach a growing packed array from the initial shared allocation. Subsequent
// growth uses dmArray ownership; registration handles and row IDs remain stable.
template <typename T>
static void ReserveTableArray(DataTable* table, dmArray<T>& array, uint32_t count)
{
    if (count <= array.Capacity())
        return;
    if (table->m_Pool && IsPoolStorage(table->m_Pool, array.Begin()))
    {
        dmArray<T> grown;
        Reserve(grown, count);
        grown.PushArray(array.Begin(), array.Size());
        array.Swap(grown);
    }
    else
        Reserve(array, count);
}

static void ReserveRows(HDataStore store, DataTable* table, uint32_t count)
{
    ReserveTableArray(table, table->m_Rows, table->m_Rows.Size() + count);
    ReserveTableArray(table, table->m_Values, (table->m_Rows.Size() + count) * table->m_RowStride);
    ReserveSlots(store, count);
}

static uint8_t* ReserveRowValues(HDataStore store, DataTable* table, uint32_t count, size_t payload_size)
{
    if (payload_size)
        ReserveData(&table->m_Owned->m_BaseValues, payload_size);
    ReserveRows(store, table, count);
    uint32_t first_base = table->m_Rows.Size();
    uint32_t size = (first_base + count) * table->m_RowStride;
    Reserve(table->m_Owned->m_BaseRows, size);
    table->m_Owned->m_BaseRows.SetSize(size);
    table->m_Values.SetSize((table->m_Rows.Size() + count) * table->m_RowStride);
    return table->m_RowStride ? table->m_Owned->m_BaseRows.Begin() + (size_t)first_base * table->m_RowStride : 0;
}

static void ReserveStore(HDataStore store, uint32_t table_count, uint32_t row_count)
{
    Reserve(store->m_Tables, store->m_Tables.Size() + table_count);
    ReserveSlots(store, row_count);
}

static DataId AddSlot(HDataStore store, DataTable* table, DataRow row)
{
    uint32_t index = store->m_FreeSlot;
    if (index == INVALID_SLOT)
    {
        index = store->m_Slots.Size();
        DataSlot slot = { .m_Generation = 1 };
        store->m_Slots.Push(slot);
    }
    else
    {
        store->m_FreeSlot = store->m_Slots[index].m_NextFree;
        --store->m_FreeSlotCount;
    }

    DataSlot* slot = &store->m_Slots[index];
    slot->m_Table = table;
    slot->m_Row = table->m_Rows.Size();
    row.m_Slot = index;
    table->m_Rows.Push(row);
    return MakeId(store, index);
}

// Capacity is reserved for the whole batch. Publish array sizes once after
// filling row identities and generation-bearing slots, including reused slots.
static void AddRowSlots(HDataStore store, DataTable* table, const DataRowDesc* rows, DataGroupId group, uint32_t count, DataId* out_ids)
{
    uint32_t  first_row = table->m_Rows.Size();
    uint32_t  slot_count = store->m_Slots.Size();
    uint32_t  free_slot = store->m_FreeSlot;
    DataSlot* slots = store->m_Slots.Begin();
    DataRow*  output = table->m_Rows.Begin() + first_row;

    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t index = free_slot;
        if (index == INVALID_SLOT)
        {
            index = slot_count++;
            slots[index].m_Generation = 1;
        }
        else
            free_slot = slots[index].m_NextFree;
        DataSlot* slot = &slots[index];
        slot->m_Table = table;
        slot->m_Row = first_row + i;
        DataRow row = {
            .m_Group = rows ? rows[i].m_Group : group,
            .m_ComponentId = rows ? rows[i].m_ComponentId : 0,
            .m_Slot = index,
            .m_BaseIndex = first_row + i
        };
        output[i] = row;
        if (out_ids)
            out_ids[i] = ((uint64_t)slot->m_Generation << 32) | index;
    }

    store->m_FreeSlotCount -= count - (slot_count - store->m_Slots.Size());
    store->m_FreeSlot = free_slot;
    store->m_Slots.SetSize(slot_count);
    table->m_Rows.SetSize(first_row + count);
}

static void DeleteTable(HDataStore store, DataTable* table)
{
    for (uint32_t i = 0; i < table->m_Rows.Size(); ++i)
    {
        ReleaseSlot(store, table->m_Rows[i].m_Slot);
    }

    DeleteStructStorage(table);
    DeleteBlocks(&table->m_Payloads);
    bool packed = table->m_Pool != 0;
    if (!packed)
    {
        DeleteBlocks(&table->m_Owned->m_BaseValues);
        table->m_Owned->~DataOwnedTable();
    }
    table->~DataTable();
    if (!packed)
        delete[] (uint8_t*)table; // Packed table storage belongs to its resource pool.
}

static void DeleteBlobPool(DataBlobPool* pool);

HDataStore  DataCreateStore(void)
{
    HDataStore store = new DataStore;
    store->m_FreeSlotCount = 0;
    store->m_FreeSlot = INVALID_SLOT;
    store->m_LockCount = 0;
    store->m_Pools = 0;
    store->m_Mutex = dmMutex::New();
    store->m_Revision = 1;
    return store;
}

DataResult DataDestroyStore(HDataStore store)
{
    dmMutex::Lock(store->m_Mutex);
    if (store->m_LockCount || store->m_ActiveQueries.Size())
    {
        dmMutex::Unlock(store->m_Mutex);
        return DATA_RESULT_LOCKED;
    }
    assert(store->m_Queries.Empty());
    while (store->m_Pools)
        DeleteBlobPool(store->m_Pools);
    for (uint32_t i = 0; i < store->m_Tables.Size(); ++i)
        DeleteTable(store, store->m_Tables[i]);
    dmMutex::Unlock(store->m_Mutex);
    dmMutex::Delete(store->m_Mutex);
    delete store;
    return DATA_RESULT_OK;
}

void DataStoreLock(HDataStore store)
{
    dmMutex::Lock(store->m_Mutex);
    assert(!store->m_ActiveQueries.Size());
    assert(store->m_LockCount != UINT32_MAX);
    ++store->m_LockCount;
}

void DataStoreUnlock(HDataStore store)
{
    assert(store->m_LockCount);
    --store->m_LockCount;
    dmMutex::Unlock(store->m_Mutex);
}

static uint32_t FieldSize(const DataFieldDesc& field)
{
    return field.m_Struct ? field.m_Struct->m_Size : DataTypeSize(field.m_Type);
}

DataResult ValidateLayout(const DataFieldDesc* fields, uint32_t count, uint32_t size, uint32_t depth, uint64_t* metadata_count, uint32_t* out_alignment)
{
    if ((count && !fields) || count > DATA_MAX_METADATA_COUNT || depth > DATA_MAX_NESTING)
        return DATA_RESULT_INVALID_ARGUMENT;
    *metadata_count += count;
    if (*metadata_count > DATA_MAX_METADATA_COUNT)
        return DATA_RESULT_INVALID_ARGUMENT;
    uint32_t alignment = 1;
    for (uint32_t i = 0; i < count; ++i)
    {
        const DataFieldDesc& field = fields[i];
        uint32_t             field_size = FieldSize(field);
        if (((depth || field.m_Struct) && (!field.m_Name || !field.m_Name[0] || strchr(field.m_Name, '.'))) ||
            field_size == UINT32_MAX || field.m_Offset > size || field_size > size - field.m_Offset ||
            (field.m_Struct && field.m_Type != DATA_TYPE_STRUCT))
            return DATA_RESULT_INVALID_ARGUMENT;
        for (uint32_t j = 0; j < i; ++j)
        {
            if (fields[j].m_Field == field.m_Field)
                return DATA_RESULT_ALREADY_EXISTS;
            uint32_t other_size = FieldSize(fields[j]);
            if (field_size && other_size && field.m_Offset < fields[j].m_Offset + other_size && fields[j].m_Offset < field.m_Offset + field_size)
                return DATA_RESULT_INVALID_ARGUMENT;
        }
        uint32_t field_alignment = DataTypeAlignment(field.m_Type);
        if (field.m_Struct)
        {
            DataResult result = ValidateLayout(field.m_Struct->m_Fields, field.m_Struct->m_FieldCount, field_size, depth + 1, metadata_count, &field_alignment);
            if (result != DATA_RESULT_OK)
                return result;
        }
        if (field.m_Offset % field_alignment)
            return DATA_RESULT_INVALID_ARGUMENT;
        if (field_alignment > alignment)
            alignment = field_alignment;
    }
    if (size % alignment)
        return DATA_RESULT_INVALID_ARGUMENT;
    *out_alignment = alignment;
    return DATA_RESULT_OK;
}

static void CompileLayout(dmArray<DataFieldMeta>& metadata, const DataFieldDesc* fields, uint32_t count, uint32_t first, uint32_t base_offset, const HashState64* prefix)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        const DataFieldDesc& field = fields[i];
        HashState64          name;
        if (prefix)
        {
            dmHashClone64(&name, prefix, false);
            dmHashUpdateBuffer64(&name, ".", 1);
        }
        else
            dmHashInit64(&name, false);
        if (field.m_Name)
            dmHashUpdateBuffer64(&name, field.m_Name, (uint32_t)strlen(field.m_Name));
        HashState64 full_name;
        dmHashClone64(&full_name, &name, false);
        uint64_t      full_hash = dmHashFinal64(&full_name);
        DataFieldMeta meta = {
            .m_Field = prefix ? full_hash : field.m_Field,
            .m_Type = field.m_Type,
            .m_Offset = base_offset + field.m_Offset,
            .m_Size = FieldSize(field),
            .m_Name = field.m_Field
        };
        if (field.m_Struct)
        {
            meta.m_ChildIndex = (uint16_t)metadata.Size();
            meta.m_ChildCount = (uint16_t)field.m_Struct->m_FieldCount;
            metadata.SetSize(metadata.Size() + meta.m_ChildCount);
            CompileLayout(metadata, field.m_Struct->m_Fields, meta.m_ChildCount, meta.m_ChildIndex, meta.m_Offset, &name);
        }
        metadata[first + i] = meta;
    }
}

static int CompareFieldNames(const void* a, const void* b)
{
    uint64_t first = *(const uint64_t*)a;
    uint64_t second = *(const uint64_t*)b;
    return (first > second) - (first < second);
}

// Full names must be unique across roots and all inline members. Keep the
// usual small layouts on the stack; blob validation reuses larger scratch arrays.
bool ValidateFieldNames(const uint8_t* metadata, uint32_t count, dmArray<uint64_t>& scratch)
{
    uint64_t  local[64];
    uint64_t* names = local;
    if (count > DM_ARRAY_SIZE(local))
    {
        Reserve(scratch, count);
        names = scratch.Begin();
    }
    for (uint32_t i = 0; i < count; ++i)
        names[i] = ReadDataInteger(metadata + (size_t)i * DATA_FIELD_META_SIZE + offsetof(DataFileFieldMeta, m_NameHash), 8);
    qsort(names, count, sizeof(uint64_t), CompareFieldNames);
    for (uint32_t i = 1; i < count; ++i)
        if (names[i] == names[i - 1])
            return false;
    return true;
}

static DataTable* NewTable(HDataStore store, const DataTableDesc* desc, DataBlobPool* pool, const uint8_t* packed_table = 0, DataTable* storage = 0, dmArray<DataFieldMeta>* metadata = 0);

DataResult        DataRegisterTable(HDataStore store, const DataTableDesc* desc)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_LockCount || store->m_ActiveQueries.Size())
        return DATA_RESULT_LOCKED;
    if ((desc->m_TagCount && !desc->m_Tags) || desc->m_TagCount > DATA_MAX_TAG_COUNT)
        return DATA_RESULT_INVALID_ARGUMENT;

    uint64_t   metadata_count = 0;
    uint32_t   alignment;
    DataResult result = ValidateLayout(desc->m_Fields, desc->m_FieldCount, desc->m_RowStride, 0, &metadata_count, &alignment);
    if (result != DATA_RESULT_OK)
        return result;
    if (FindTable(store, desc->m_Type))
        return DATA_RESULT_ALREADY_EXISTS;

    dmArray<DataFieldMeta> metadata;
    metadata.SetCapacity((uint32_t)metadata_count);
    metadata.SetSize(desc->m_FieldCount);
    CompileLayout(metadata, desc->m_Fields, desc->m_FieldCount, 0, 0, 0);
    dmArray<uint64_t> scratch;
    if (metadata_count > desc->m_FieldCount && !ValidateFieldNames((const uint8_t*)metadata.Begin(), metadata.Size(), scratch))
        return DATA_RESULT_ALREADY_EXISTS;

    ++store->m_Revision;
    NewTable(store, desc, 0, 0, 0, &metadata);
    return DATA_RESULT_OK;
}

static DataTable* NewTable(HDataStore store, const DataTableDesc* desc, DataBlobPool* pool, const uint8_t* packed_table, DataTable* storage, dmArray<DataFieldMeta>* metadata)
{
    DataTable* table = storage;
    if (!table)
    {
        uint8_t* allocation = new uint8_t[sizeof(DataTable) + sizeof(DataOwnedTable)];
        table = new (allocation) DataTable;
        table->m_Owned = new (table + 1) DataOwnedTable;
        table->m_Owned->m_BaseValues = 0;
    }

    table->m_Type = desc->m_Type;
    table->m_Blob = pool ? pool->m_Blob->m_Data : 0;
    if (pool)
    {
        table->m_Offsets.m_Table = (uint32_t)(packed_table - table->m_Blob);
        table->m_Offsets.m_Rows = 0;
    }
    table->m_TagCount = (uint16_t)desc->m_TagCount;
    table->m_FieldCount = (uint16_t)desc->m_FieldCount;
    table->m_MetadataCount = packed_table ? (uint16_t)ReadDataInteger(packed_table + offsetof(DataTableHeader, m_MetadataCount), 2) : (uint16_t)desc->m_FieldCount;
    table->m_RowStride = desc->m_RowStride;
    table->m_Pool = pool;
    table->m_Payloads = 0;
    table->m_Structs = 0;
    if (!packed_table && desc->m_TagCount)
    {
        table->m_Owned->m_Tags.SetCapacity(desc->m_TagCount);
        table->m_Owned->m_Tags.PushArray(desc->m_Tags, desc->m_TagCount);
    }
    if (!packed_table)
    {
        table->m_MetadataCount = (uint16_t)metadata->Size();
        table->m_Owned->m_Fields.Swap(*metadata);
    }

    table->m_HasReferences = false;
    for (uint32_t i = 0; i < table->m_MetadataCount; ++i)
    {
        DataFieldMeta meta = GetFieldMeta(table, i);
        table->m_HasReferences |= !meta.m_ChildIndex && (meta.m_Type == DATA_TYPE_STRING || meta.m_Type == DATA_TYPE_STRUCT || meta.m_Type == DATA_TYPE_LIST);
    }

    // Immutable candidate indexes accelerate typed hash access without copying
    // names or values. Collisions retain the normal metadata search.
    table->m_FieldMetadata = pool ? packed_table + DATA_TABLE_HEADER_SIZE + table->m_TagCount * 8 :
                                    (const uint8_t*)table->m_Owned->m_Fields.Begin();
    memset(table->m_FieldLookup, 0xff, sizeof(table->m_FieldLookup));
    for (uint32_t i = 0; i < table->m_MetadataCount; ++i)
    {
        uint32_t bucket = (uint32_t)GetFieldMeta(table, i).m_Field & (DATA_FIELD_LOOKUP_SIZE - 1);
        if (table->m_FieldLookup[bucket] == UINT16_MAX)
            table->m_FieldLookup[bucket] = (uint16_t)i;
    }

    Reserve(store->m_Tables, store->m_Tables.Size() + 1);
    table->m_StoreIndex = store->m_Tables.Size();
    store->m_Tables.Push(table);
    for (uint32_t i = 0; i < store->m_Queries.Size(); ++i)
    {
        MatchTable(store->m_Queries[i], table);
    }
    return table;
}

static void RemoveTable(HDataStore store, DataTable* table)
{
    for (uint32_t i = 0; i < store->m_Queries.Size(); ++i)
    {
        UnmatchTable(store->m_Queries[i], table);
    }
    store->m_Tables.Back()->m_StoreIndex = table->m_StoreIndex;
    store->m_Tables.EraseSwap(table->m_StoreIndex);
    DeleteTable(store, table);
}

DataResult DataUnregisterTable(HDataStore store, uint64_t type)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_LockCount || store->m_ActiveQueries.Size())
        return DATA_RESULT_LOCKED;
    ++store->m_Revision;
    DataTable* table = FindTable(store, type);
    if (!table)
        return DATA_RESULT_NOT_FOUND;
    RemoveTable(store, table);
    return DATA_RESULT_OK;
}

static uint64_t AlignBlobStorage(uint64_t size)
{
    return (size + 7) & ~(uint64_t)7;
}

static DataBlobPool* NewBlobPool(HDataStore store, HDataBlob blob)
{
    uint64_t       rows_offset = AlignBlobStorage(sizeof(DataBlobPool) + (uint64_t)blob->m_TableCount * sizeof(DataTable));
    uint64_t       values_offset = rows_offset + (uint64_t)blob->m_RowCount * sizeof(DataRow);
    uint64_t       page_offset = values_offset;
    const uint8_t* file = blob->m_Data;
    for (uint32_t t = 0; t < blob->m_TableCount; ++t)
    {
        const uint8_t* data = file + ReadDataInteger(file + DATA_TABLE_OFFSETS_OFFSET + t * 4, 4);
        page_offset += AlignBlobStorage(ReadDataInteger(data + offsetof(DataTableHeader, m_RowCount), 4) * ReadDataInteger(data + offsetof(DataTableHeader, m_RowStride), 4));
    }

    uint64_t stride = AlignBlobStorage(sizeof(DataBlobInstance) + (uint64_t)blob->m_RowCount * sizeof(uint32_t));
    uint32_t shift = 8; // Up to 256 registrations per page, bounded to 64 KiB unless one record is larger.
    while (shift && (stride << shift) > 65536)
        --shift;
    uint64_t size = page_offset + (stride << shift);
    if (size > SIZE_MAX || stride > UINT32_MAX)
        return 0;
    uint8_t*      storage = new uint8_t[(size_t)size];
    DataBlobPool* pool = new (storage) DataBlobPool;
    pool->m_Store = store;
    pool->m_Blob = blob;
    pool->m_Next = store->m_Pools;
    pool->m_FirstPage = storage + (size_t)page_offset;
    pool->m_AllocationSize = size;
    pool->m_InstanceStride = (uint32_t)stride;
    pool->m_PageShift = shift;
    pool->m_Issued = 0;
    pool->m_Live = 0;
    pool->m_FreeInstance = INVALID_SLOT;
    store->m_Pools = pool;
    ++blob->m_RefCount;

    ReserveStore(store, blob->m_TableCount, blob->m_RowCount);
    DataRow* rows = (DataRow*)(storage + (size_t)rows_offset);
    uint8_t* values = storage + (size_t)values_offset;
    uint32_t first_slot = 0;
    for (uint32_t t = 0; t < blob->m_TableCount; ++t)
    {
        const uint8_t* data = file + ReadDataInteger(file + DATA_TABLE_OFFSETS_OFFSET + t * 4, 4);
        DataTableDesc  desc = {
             .m_Type = ReadDataInteger(data + offsetof(DataTableHeader, m_TypeHash), 8),
             .m_TagCount = (uint32_t)ReadDataInteger(data + offsetof(DataTableHeader, m_TagCount), 2),
             .m_FieldCount = (uint32_t)ReadDataInteger(data + offsetof(DataTableHeader, m_FieldCount), 2),
             .m_RowStride = (uint32_t)ReadDataInteger(data + offsetof(DataTableHeader, m_RowStride), 4)
        };
        uint32_t       count = (uint32_t)ReadDataInteger(data + offsetof(DataTableHeader, m_RowCount), 4);
        DataTable*     table = NewTable(store, &desc, pool, data, new (GetPoolTable(pool, t)) DataTable);
        const uint8_t* components = data + DATA_TABLE_HEADER_SIZE + desc.m_TagCount * 8 + table->m_MetadataCount * DATA_FIELD_META_SIZE;
        table->m_Offsets.m_Rows = (uint32_t)(components + (size_t)count * 8 - file);
        table->m_Offsets.m_FirstSlot = first_slot;
        first_slot += count;
        table->m_Rows.Set(rows, 0, count, true);
        rows += count;
        uint32_t capacity = (uint32_t)AlignBlobStorage((uint64_t)count * desc.m_RowStride);
        table->m_Values.Set(values, 0, capacity, true);
        values += capacity;
    }
    return pool;
}

static bool CanAppendBlob(DataBlobPool* pool)
{
    for (uint32_t t = 0; t < pool->m_Blob->m_TableCount; ++t)
    {
        const DataTable* table = GetPoolTable(pool, t);
        uint64_t         rows = table->m_Rows.Size() + ReadDataInteger(table->m_Blob + table->m_Offsets.m_Table + offsetof(DataTableHeader, m_RowCount), 4);
        if (rows > UINT32_MAX / sizeof(DataRow) || rows * table->m_RowStride > UINT32_MAX)
            return false;
    }
    return pool->m_FreeInstance != INVALID_SLOT || pool->m_Issued != UINT32_MAX;
}

static HDataBlobInstance NewBlobInstance(DataBlobPool* pool)
{
    HDataBlobInstance instance;
    if (pool->m_FreeInstance != INVALID_SLOT)
    {
        instance = GetPoolInstance(pool, pool->m_FreeInstance);
        pool->m_FreeInstance = instance->m_NextFree;
    }
    else
    {
        uint32_t index = pool->m_Issued++;
        uint32_t page = index >> pool->m_PageShift;
        if (page > pool->m_Pages.Size())
        {
            Reserve(pool->m_Pages, page);
            pool->m_Pages.Push(new uint8_t[(size_t)pool->m_InstanceStride << pool->m_PageShift]);
        }
        instance = GetPoolInstance(pool, index);
        instance->m_Index = index;
    }
    instance->m_Pool = pool;
    instance->m_Payloads = 0;
    ++pool->m_Live;
    return instance;
}

DataResult DataAddBlob(HDataStore store, HDataBlob blob, DataGroupId group, HDataBlobInstance* out_instance)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_LockCount || store->m_ActiveQueries.Size())
        return DATA_RESULT_LOCKED;
    ++store->m_Revision;
    if (blob->m_RowCount > UINT32_MAX / sizeof(DataSlot) - store->m_Slots.Size() ||
        blob->m_TableCount > UINT32_MAX / sizeof(DataTable*) - store->m_Tables.Size())
        return DATA_RESULT_INVALID_ARGUMENT;

    DataBlobPool* pool = store->m_Pools;
    while (pool && (pool->m_Blob != blob || !CanAppendBlob(pool)))
        pool = pool->m_Next;
    if (!pool)
    {
        pool = NewBlobPool(store, blob);
        if (!pool)
            return DATA_RESULT_INVALID_ARGUMENT;
    }

    ReserveSlots(store, blob->m_RowCount);
    HDataBlobInstance instance = NewBlobInstance(pool);
    uint32_t*         slots = GetInstanceSlots(instance);
    for (uint32_t t = 0; t < blob->m_TableCount; ++t)
    {
        DataTable* table = GetPoolTable(pool, t);
        uint32_t   count = (uint32_t)ReadDataInteger(table->m_Blob + table->m_Offsets.m_Table + offsetof(DataTableHeader, m_RowCount), 4);
        uint32_t   start = table->m_Rows.Size();
        ReserveTableArray(table, table->m_Rows, start + count);
        ReserveTableArray(table, table->m_Values, (start + count) * table->m_RowStride);
        table->m_Values.SetSize((start + count) * table->m_RowStride);
        InitializeBlobValues(table, start, count);
        for (uint32_t r = 0; r < count; ++r)
        {
            DataRow row = {
                .m_Group = group,
                .m_InstanceIndex = instance->m_Index,
                .m_BaseIndex = r
            };
            slots[table->m_Offsets.m_FirstSlot + r] = (uint32_t)AddSlot(store, table, row);
        }
    }

    *out_instance = instance;
    return DATA_RESULT_OK;
}

static void DeleteBlobPool(DataBlobPool* pool)
{
    HDataStore store = pool->m_Store;
    for (uint32_t i = 0; i < pool->m_Issued; ++i)
    {
        HDataBlobInstance instance = GetPoolInstance(pool, i);
        if (instance->m_Pool)
            DeleteBlocks(&instance->m_Payloads);
    }
    for (uint32_t t = 0; t < pool->m_Blob->m_TableCount; ++t)
        RemoveTable(store, GetPoolTable(pool, t));
    DataBlobPool** link = &store->m_Pools;
    while (*link != pool)
        link = &(*link)->m_Next;
    *link = pool->m_Next;
    DataDestroyBlob(pool->m_Blob);
    for (uint32_t i = 0; i < pool->m_Pages.Size(); ++i)
        delete[] pool->m_Pages[i];
    pool->~DataBlobPool();
    delete[] (uint8_t*)pool;
}

DataResult DataRemoveBlob(HDataBlobInstance instance)
{
    DataBlobPool* pool = instance->m_Pool;
    HDataStore    store = pool->m_Store;
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_LockCount || store->m_ActiveQueries.Size())
        return DATA_RESULT_LOCKED;
    ++store->m_Revision;
    const uint32_t* slots = GetInstanceSlots(instance);
    for (uint32_t i = 0; i < pool->m_Blob->m_RowCount; ++i)
        if (slots[i] != INVALID_SLOT)
            DataRemoveRow(store, MakeId(store, slots[i]));
    DeleteBlocks(&instance->m_Payloads);
    instance->m_Pool = 0;
    instance->m_NextFree = pool->m_FreeInstance;
    pool->m_FreeInstance = instance->m_Index;
    if (!--pool->m_Live)
        DeleteBlobPool(pool);
    return DATA_RESULT_OK;
}

DataResult DataAddRow(HDataStore store, uint64_t type, DataGroupId group, const DataValueType* types, const DataValueData* values, uint32_t value_count, DataId* out_id, uint64_t component_id)
{
    DataRowDesc row = {
        .m_Group = group,
        .m_Types = types,
        .m_Values = values,
        .m_ValueCount = value_count,
        .m_ComponentId = component_id
    };
    return DataAddRows(store, type, &row, 1, out_id);
}

static void PublishRows(HDataStore store, DataTable* table, const DataRowDesc* rows, DataGroupId group, uint32_t count, DataId* out_ids, const uint8_t* data)
{
    if (table->m_RowStride)
        memcpy(table->m_Values.Begin() + (size_t)table->m_Rows.Size() * table->m_RowStride, data, (size_t)count * table->m_RowStride);
    AddRowSlots(store, table, rows, group, count, out_ids);
}

// Fixed fields use native bytes. Validate booleans and measure reference
// payloads before allocating or publishing the batch.
static DataResult ValidateNativeRows(const DataTable* table, uint32_t num_rows, const void* rows, size_t* payload_size)
{
    if (table->m_RowStride && !rows)
        return DATA_RESULT_INVALID_ARGUMENT;
    for (uint32_t f = 0; f < table->m_MetadataCount; ++f)
    {
        DataFieldMeta meta = GetFieldMeta(table, f);
        if (meta.m_Type != DATA_TYPE_BOOLEAN)
            continue;
        const uint8_t* values = (const uint8_t*)rows + meta.m_Offset;
        for (uint32_t r = 0; r < num_rows; ++r)
            if (values[(size_t)r * table->m_RowStride] > 1)
                return DATA_RESULT_INVALID_ARGUMENT;
    }
    if (table->m_HasReferences)
    {
        for (uint32_t f = 0; f < table->m_FieldCount; ++f)
        {
            DataFieldMeta meta = GetFieldMeta(table, f);
            DataResult    result = ValidateNativeReferences(table, meta, (const uint8_t*)rows + meta.m_Offset, table->m_RowStride, num_rows, 0, payload_size);
            if (result != DATA_RESULT_OK)
                return result;
        }
    }
    return DATA_RESULT_OK;
}

static bool CanCreateRows(HDataStore store, const DataTable* table, uint32_t count)
{
    return count <= UINT32_MAX / sizeof(DataSlot) - store->m_Slots.Size() &&
    count <= UINT32_MAX / sizeof(DataRow) - table->m_Rows.Size() &&
    (uint64_t)(table->m_Rows.Size() + count) * table->m_RowStride <= UINT32_MAX;
}

DataResult DataCreateRows(HDataStore store, uint64_t type, DataGroupId group, uint32_t count, const void* rows, DataId* out_ids)
{
    if (!count)
        return DATA_RESULT_OK;
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_LockCount || store->m_ActiveQueries.Size())
        return DATA_RESULT_LOCKED;
    DataTable* table = FindTable(store, type);
    if (!table)
        return DATA_RESULT_NOT_FOUND;
    if (!CanCreateRows(store, table, count))
        return DATA_RESULT_INVALID_ARGUMENT;
    size_t     payload_size = 0;
    DataResult result = ValidateNativeRows(table, count, rows, &payload_size);
    if (result != DATA_RESULT_OK)
        return result;

    uint8_t* data = ReserveRowValues(store, table, count, payload_size);
    size_t   bytes = (size_t)count * table->m_RowStride;
    if (bytes)
        memcpy(data, rows, bytes);
    if (table->m_HasReferences)
        StoreNativeReferences(table, count, data);
    ++store->m_Revision;
    PublishRows(store, table, 0, group, count, out_ids, data);
    return DATA_RESULT_OK;
}

static bool ValidateColumnBooleans(const DataTable* table, const DataFieldMeta& meta, const uint8_t* values, uint32_t stride, uint32_t count)
{
    if (!meta.m_Size)
        return true;
    if (meta.m_Type == DATA_TYPE_BOOLEAN)
    {
        for (uint32_t r = 0; r < count; ++r)
            if (values[(size_t)r * stride] > 1)
                return false;
    }
    for (uint32_t i = 0; i < meta.m_ChildCount; ++i)
    {
        DataFieldMeta child = GetFieldMeta(table, meta.m_ChildIndex + i);
        if (!ValidateColumnBooleans(table, child, values + (child.m_Offset - meta.m_Offset), stride, count))
            return false;
    }
    return true;
}

template <uint32_t Size>
static void CopyColumn(uint8_t* dst, const uint8_t* src, uint32_t stride, uint32_t count)
{
    for (uint32_t r = 0; r < count; ++r)
        memcpy(dst + (size_t)r * stride, src + (size_t)r * Size, Size);
}

DataResult DataCreateRowsSoA(HDataStore store, uint64_t type, DataGroupId group, uint32_t count, uint32_t field_count, const DataFieldArray* fields, DataId* out_ids)
{
    if (!count)
        return DATA_RESULT_OK;
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_LockCount || store->m_ActiveQueries.Size())
        return DATA_RESULT_LOCKED;
    DataTable* table = FindTable(store, type);
    if (!table)
        return DATA_RESULT_NOT_FOUND;
    if (!CanCreateRows(store, table, count) ||
        field_count != table->m_FieldCount || (field_count && !fields))
        return DATA_RESULT_INVALID_ARGUMENT;

    DataColumnBinding          local[32];
    dmArray<DataColumnBinding> columns;
    if (field_count <= DM_ARRAY_SIZE(local))
        columns.Set(local, field_count, DM_ARRAY_SIZE(local), true);
    else
    {
        columns.SetCapacity(field_count);
        columns.SetSize(field_count);
    }
    for (uint32_t i = 0; i < field_count; ++i)
        columns[i].m_Size = UINT32_MAX;
    uint32_t field_bytes = 0;
    size_t   payload_size = 0;
    for (uint32_t i = 0; i < field_count; ++i)
    {
        uint32_t index = FindField(table, fields[i].m_Field);
        if (index >= field_count || columns[index].m_Size != UINT32_MAX)
            return DATA_RESULT_INVALID_ARGUMENT;
        DataFieldMeta  meta = GetFieldMeta(table, index);
        const uint8_t* values = (const uint8_t*)fields[i].m_Values;
        if ((meta.m_Size && !values) || !ValidateColumnBooleans(table, meta, values, meta.m_Size, count))
            return DATA_RESULT_INVALID_ARGUMENT;
        if (table->m_HasReferences)
        {
            DataResult result = ValidateNativeReferences(table, meta, values, meta.m_Size, count, 0, &payload_size);
            if (result != DATA_RESULT_OK)
                return result;
        }
        columns[index] = { .m_Values = values, .m_Offset = meta.m_Offset, .m_Size = meta.m_Size };
        field_bytes += meta.m_Size;
    }

    uint8_t* data = ReserveRowValues(store, table, count, payload_size);
    uint32_t stride = table->m_RowStride;
    // Registered fields do not overlap; clear padding only when the row has gaps.
    if (field_bytes < stride)
        memset(data, 0, (size_t)count * stride);
    for (uint32_t f = 0; f < field_count; ++f)
    {
        const DataColumnBinding& column = columns[f];
        if (!column.m_Size)
            continue;
        uint8_t*       dst = data + column.m_Offset;
        const uint8_t* src = column.m_Values;
        // Dispatch common sizes once so each row uses native loads/stores.
        switch (column.m_Size)
        {
            case 1:
                CopyColumn<1>(dst, src, stride, count);
                break;
            case 8:
                CopyColumn<8>(dst, src, stride, count);
                break;
            case 12:
                CopyColumn<12>(dst, src, stride, count);
                break;
            case 16:
                CopyColumn<16>(dst, src, stride, count);
                break;
            case 64:
                CopyColumn<64>(dst, src, stride, count);
                break;
            default:
                for (uint32_t r = 0; r < count; ++r)
                    memcpy(dst + (size_t)r * stride, src + (size_t)r * column.m_Size, column.m_Size);
        }
    }
    if (table->m_HasReferences)
        StoreNativeReferences(table, count, data);
    ++store->m_Revision;
    PublishRows(store, table, 0, group, count, out_ids, data);
    return DATA_RESULT_OK;
}

DataResult DataAddRows(HDataStore store, uint64_t type, const DataRowDesc* rows, uint32_t count, DataId* out_ids)
{
    if (!count)
        return DATA_RESULT_OK;
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_LockCount || store->m_ActiveQueries.Size())
        return DATA_RESULT_LOCKED;
    ++store->m_Revision;
    DataTable* table = FindTable(store, type);
    if (!table)
        return DATA_RESULT_NOT_FOUND;
    if (!rows || !CanCreateRows(store, table, count))
        return DATA_RESULT_INVALID_ARGUMENT;

    size_t     payload_size = 0;
    DataResult result = ValidateRowValues(table, rows, count, &payload_size);
    if (result != DATA_RESULT_OK)
        return result;

    uint8_t* data = ReserveRowValues(store, table, count, payload_size);
    if (table->m_RowStride)
    {
        memset(data, 0, (size_t)count * table->m_RowStride);
        StoreRowValues(table, rows, count, data);
    }
    PublishRows(store, table, rows, 0, count, out_ids, data);
    return DATA_RESULT_OK;
}

DataResult DataRemoveRow(HDataStore store, DataId id)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_LockCount || store->m_ActiveQueries.Size())
        return DATA_RESULT_LOCKED;
    ++store->m_Revision;
    DataSlot* slot = FindSlot(store, id);
    if (!slot)
        return DATA_RESULT_NOT_FOUND;
    DataTable* table = slot->m_Table;
    uint32_t   row = slot->m_Row;
    if (table->m_Structs)
        ReleaseRowStructs(table, row, true);
    if (table->m_Pool)
    {
        const DataRow&    removed = table->m_Rows[row];
        HDataBlobInstance instance = GetPoolInstance(table->m_Pool, removed.m_InstanceIndex);
        GetInstanceSlots(instance)[table->m_Offsets.m_FirstSlot + removed.m_BaseIndex] = INVALID_SLOT;
    }
    store->m_Slots[table->m_Rows.Back().m_Slot].m_Row = row;
    uint32_t last = table->m_Rows.Size() - 1;
    if (row != last && table->m_RowStride)
        memcpy(table->m_Values.Begin() + (size_t)row * table->m_RowStride,
               table->m_Values.Begin() + (size_t)last * table->m_RowStride,
               table->m_RowStride);
    table->m_Values.SetSize(last * table->m_RowStride);
    if (!table->m_Pool)
    {
        dmArray<uint8_t>& defaults = table->m_Owned->m_BaseRows;
        if (row != last && table->m_RowStride)
            memcpy(defaults.Begin() + (size_t)row * table->m_RowStride,
                   defaults.Begin() + (size_t)last * table->m_RowStride,
                   table->m_RowStride);
        defaults.SetSize(last * table->m_RowStride);
        table->m_Rows.Back().m_BaseIndex = row;
        if (!last)
        {
            DeleteBlocks(&table->m_Owned->m_BaseValues);
            DeleteBlocks(&table->m_Payloads);
        }
    }
    table->m_Rows.EraseSwap(row);
    ReleaseSlot(store, (uint32_t)id);
    return DATA_RESULT_OK;
}

uint64_t GetRowComponentId(const DataTable* table, const DataRow* row)
{
    if (!table->m_Pool)
        return row->m_ComponentId;
    const uint8_t* data = table->m_Blob + table->m_Offsets.m_Table;
    const uint8_t* components = data + DATA_TABLE_HEADER_SIZE + (size_t)table->m_TagCount * 8 + (size_t)table->m_MetadataCount * DATA_FIELD_META_SIZE;
    return ReadDataInteger(components + (size_t)row->m_BaseIndex * 8, 8);
}

uint64_t DataGetComponentId(HDataStore store, DataId id)
{
    DataSlot* slot = FindSlot(store, id);
    return slot ? GetRowComponentId(slot->m_Table, &slot->m_Table->m_Rows[slot->m_Row]) : 0;
}
