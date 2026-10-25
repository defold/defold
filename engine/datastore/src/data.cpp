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
#include <dlib/log.h>
#include "data.h"

// Resolved once per input column, never per row. Values are copied directly into
// the table's reset storage, then published through the same path as native rows.
struct DSColumnBinding
{
    const uint8_t* m_Values; // Borrowed contiguous native field values.
    uint32_t       m_Offset; // Destination byte offset within each row.
    uint32_t       m_Size;   // Source stride and copy size; UINT32_MAX until bound.
};

static const uint32_t INVALID_SLOT = UINT32_MAX;

static void           RemoveRow(HDSContext store, DSSlot* slot);

void                  ReserveData(DSBlock** blocks, size_t size, size_t minimum_capacity)
{
    const size_t header_size = (sizeof(DSBlock) + 7) & ~(size_t)7;
    DSBlock*     block = *blocks;
    size_t       offset = block ? (block->m_Size + 7) & ~(size_t)7 : 0;
    if (!block || offset > block->m_Capacity || size > block->m_Capacity - offset)
    {
        size_t capacity = size > minimum_capacity ? size : minimum_capacity;
        assert(capacity <= SIZE_MAX - header_size);
        block = (DSBlock*)new uint8_t[header_size + capacity];
        block->m_Next = *blocks;
        block->m_Size = 0;
        block->m_Capacity = capacity;
        *blocks = block;
    }
}

void* AllocateData(DSBlock** blocks, size_t size, size_t minimum_capacity)
{
    ReserveData(blocks, size, minimum_capacity);
    const size_t header_size = (sizeof(DSBlock) + 7) & ~(size_t)7;
    DSBlock*     block = *blocks;
    size_t       offset = (block->m_Size + 7) & ~(size_t)7;
    void*        result = (uint8_t*)block + header_size + offset;
    block->m_Size = offset + size;
    return result;
}

void DeleteBlocks(DSBlock** blocks)
{
    while (*blocks)
    {
        DSBlock* block = *blocks;
        *blocks = block->m_Next;
        delete[] (uint8_t*)block;
    }
}

static bool IsPoolStorage(const DSBlobPool* pool, const void* bytes)
{
    return (uintptr_t)bytes >= (uintptr_t)pool && (uintptr_t)bytes < (uintptr_t)pool + pool->m_AllocationSize;
}

// Finds an independently registered table; blob instances are managed by their handle.
DSTable* FindTable(HDSContext store, uint64_t type)
{
    for (uint32_t i = 0; i < store->m_Tables.Size(); ++i)
    {
        if (!store->m_Tables[i]->m_Pool && store->m_Tables[i]->m_Type == type)
            return store->m_Tables[i];
    }
    return 0;
}

static DSGroup* GetOrCreateGroup(HDSContext store, DSGroupId id)
{
    if (!id)
        return 0;
    DSGroup* group = store->m_Groups.Get(id);
    if (group)
        return group;
    if (store->m_Groups.Full())
    {
        uint32_t capacity = store->m_Groups.Capacity();
        store->m_Groups.SetCapacity(capacity ? capacity * 2 : 16);
    }
    DSGroup entry = { .m_FirstSlot = INVALID_SLOT };
    store->m_Groups.Put(id, entry);
    return store->m_Groups.Get(id);
}

static void AddGroupRow(HDSContext store, DSGroup* group, uint32_t index)
{
    if (!group)
        return;
    DSSlot*  slots = store->m_Slots.Begin();
    slots[index].m_GroupPrevious = INVALID_SLOT;
    slots[index].m_GroupNext = group->m_FirstSlot;
    if (group->m_FirstSlot != INVALID_SLOT)
        slots[group->m_FirstSlot].m_GroupPrevious = index;
    group->m_FirstSlot = index;
}

static void ReleaseSlot(HDSContext store, uint32_t index)
{
    DSSlot*   slot = &store->m_Slots[index];
    DSGroupId group = slot->m_Table->m_Rows[slot->m_Row].m_Group;
    if (group)
    {
        if (slot->m_GroupPrevious == INVALID_SLOT)
            store->m_Groups.Get(group)->m_FirstSlot = slot->m_GroupNext;
        else
            store->m_Slots[slot->m_GroupPrevious].m_GroupNext = slot->m_GroupNext;
        if (slot->m_GroupNext != INVALID_SLOT)
            store->m_Slots[slot->m_GroupNext].m_GroupPrevious = slot->m_GroupPrevious;
    }
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

static void ReserveSlots(HDSContext store, uint32_t count)
{
    uint32_t new_slots = count > store->m_FreeSlotCount ? count - store->m_FreeSlotCount : 0;

    Reserve(store->m_Slots, store->m_Slots.Size() + new_slots);
}

// Detach a growing packed array from the initial shared allocation. Subsequent
// growth uses dmArray ownership; registration handles and row IDs remain stable.
template <typename T>
static void ReserveTableArray(DSTable* table, dmArray<T>& array, uint32_t count)
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

static void ReserveRows(HDSContext store, DSTable* table, uint32_t count)
{
    ReserveTableArray(table, table->m_Rows, table->m_Rows.Size() + count);
    ReserveRowStorage(store, table, table->m_Rows.Size() + count);
    ReserveSlots(store, count);
}

static void ReserveRowValues(HDSContext store, DSTable* table, uint32_t count, size_t payload_size)
{
    if (payload_size)
        ReservePayloads(GetNativePayloads(table), payload_size);
    ReserveRows(store, table, count);
}

static uint8_t* GetNewRowValues(DSTable* table, uint32_t row)
{
    if (!table->m_RowStride)
        return 0;
    return table->m_RetainDefaults ? GetNativeDefaultRow(table, row) : GetRowBytes(table, row);
}

static void ReserveStore(HDSContext store, uint32_t table_count, uint32_t row_count)
{
    Reserve(store->m_Tables, store->m_Tables.Size() + table_count);
    ReserveSlots(store, row_count);
}

static uint32_t AllocateSlot(HDSContext store, DSTable* table, uint32_t row)
{
    uint32_t index = store->m_FreeSlot;
    if (index == INVALID_SLOT)
    {
        index = store->m_Slots.Size();
        DSSlot slot = { .m_Generation = 1 };
        store->m_Slots.Push(slot);
    }
    else
    {
        store->m_FreeSlot = store->m_Slots[index].m_NextFree;
        --store->m_FreeSlotCount;
    }

    DSSlot* slot = &store->m_Slots[index];
    slot->m_Table = table;
    slot->m_Row = row;
    return index;
}

// Capacity is reserved for the whole batch. Publish array sizes once after
// filling row identities and generation-bearing slots, including reused slots.
static void AddRowSlots(HDSContext store, DSTable* table, const DSRowDesc* rows, DSGroupId group, uint32_t count, DSId* out_ids)
{
    uint32_t first_row = table->m_Rows.Size();
    uint32_t slot_count = store->m_Slots.Size();
    uint32_t free_slot = store->m_FreeSlot;
    DSSlot*  slots = store->m_Slots.Begin();
    DSRow*   output = table->m_Rows.Begin() + first_row;
    DSGroup* owner = GetOrCreateGroup(store, group);

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
        DSSlot* slot = &slots[index];
        slot->m_Table = table;
        slot->m_Row = first_row + i;
        DSRow row = {
            .m_Group = rows ? rows[i].m_Group : group,
            .m_ComponentId = rows ? rows[i].m_ComponentId : 0,
            .m_Slot = index,
            .m_BaseIndex = first_row + i
        };
        output[i] = row;
        AddGroupRow(store, rows ? GetOrCreateGroup(store, row.m_Group) : owner, index);
        if (out_ids)
            out_ids[i] = ((uint64_t)slot->m_Generation << 32) | index;
    }

    store->m_FreeSlotCount -= count - (slot_count - store->m_Slots.Size());
    store->m_FreeSlot = free_slot;
    store->m_Slots.SetSize(slot_count);
    table->m_Rows.SetSize(first_row + count);
}

static void DeleteTable(HDSContext store, DSTable* table)
{
    for (uint32_t i = 0; i < table->m_Rows.Size(); ++i)
    {
        ReleaseSlot(store, table->m_Rows[i].m_Slot);
    }

    TrimRowStorage(store, table, 0);
    DeleteStructStorage(table);
    DeletePayloads(&table->m_Payloads);
    bool packed = table->m_Pool != 0;
    if (!packed)
    {
        DeletePayloads(&table->m_Owned->m_BaseValues);
        table->m_Owned->~DSOwnedTable();
    }
    table->~DSTable();
    if (!packed)
        delete[] (uint8_t*)table; // Packed table storage belongs to its resource pool.
}

static void DeleteBlobPool(DSBlobPool* pool);

HDSContext  DSCreateStore(const DSCreateParams* params)
{
    uint32_t page_size = params && params->m_PageSize ? params->m_PageSize : 524288;
    uint32_t chunk_size = params && params->m_RowChunkSize ? params->m_RowChunkSize : 262144;
    if (chunk_size < 64 || (chunk_size & (chunk_size - 1)) ||
        page_size < chunk_size || (page_size & (page_size - 1)))
        return 0;
    dmLogOnceWarning("The datastore API is experimental and may change without notice.");
    HDSContext store = new DSStore;
    store->m_PageSize = page_size;
    store->m_RowChunkSize = chunk_size;
    store->m_FreePage = UINT32_MAX;
    store->m_FreePageSlot = UINT32_MAX;
    store->m_SparePageCount = 0;
    store->m_MaxSparePages = params ? params->m_MaxSparePages : 16;
    store->m_FreeSlotCount = 0;
    store->m_FreeSlot = INVALID_SLOT;
    store->m_Pools = 0;
    store->m_Mutex = dmMutex::New();
    store->m_Revision = 1;
    store->m_NextTableSerial = 0;
    store->m_ScheduleDirty = true;
    store->m_ScheduleActive = false;
    store->m_ScheduleJobs = 0;
    return store;
}

DSResult DSDestroyStore(HDSContext store)
{
    dmMutex::Lock(store->m_Mutex);
    if (store->m_ScheduleActive || store->m_ActiveQueries.Size())
    {
        dmMutex::Unlock(store->m_Mutex);
        return DS_RESULT_LOCKED;
    }
    assert(store->m_Queries.Empty());
    while (store->m_Pools)
        DeleteBlobPool(store->m_Pools);
    for (uint32_t i = 0; i < store->m_Tables.Size(); ++i)
        DeleteTable(store, store->m_Tables[i]);
    dmMutex::Unlock(store->m_Mutex);
    dmMutex::Delete(store->m_Mutex);
    DeleteRowPages(store);
    delete store;
    return DS_RESULT_OK;
}

static uint32_t FieldSize(const DSFieldDesc& field)
{
    return field.m_Struct ? field.m_Struct->m_Size : DSTypeSize(field.m_Type);
}

DSResult ValidateLayout(const DSFieldDesc* fields, uint32_t count, uint32_t size, uint32_t depth, uint64_t* metadata_count, uint32_t* out_alignment)
{
    if ((count && !fields) || count > DATA_MAX_METADATA_COUNT || depth > DATA_MAX_NESTING)
        return DS_RESULT_INVALID_ARGUMENT;
    *metadata_count += count;
    if (*metadata_count > DATA_MAX_METADATA_COUNT)
        return DS_RESULT_INVALID_ARGUMENT;
    uint32_t alignment = 1;
    for (uint32_t i = 0; i < count; ++i)
    {
        const DSFieldDesc& field = fields[i];
        uint32_t           field_size = FieldSize(field);
        if (((depth || field.m_Struct) && (!field.m_Name || !field.m_Name[0] || strchr(field.m_Name, '.'))) ||
            field_size == UINT32_MAX || field.m_Offset > size || field_size > size - field.m_Offset ||
            (field.m_Struct && field.m_Type != DS_TYPE_STRUCT))
            return DS_RESULT_INVALID_ARGUMENT;
        for (uint32_t j = 0; j < i; ++j)
        {
            if (fields[j].m_Field == field.m_Field)
                return DS_RESULT_ALREADY_EXISTS;
            uint32_t other_size = FieldSize(fields[j]);
            if (field_size && other_size && field.m_Offset < fields[j].m_Offset + other_size && fields[j].m_Offset < field.m_Offset + field_size)
                return DS_RESULT_INVALID_ARGUMENT;
        }
        uint32_t field_alignment = DSTypeAlignment(field.m_Type);
        if (field.m_Struct)
        {
            DSResult result = ValidateLayout(field.m_Struct->m_Fields, field.m_Struct->m_FieldCount, field_size, depth + 1, metadata_count, &field_alignment);
            if (result != DS_RESULT_OK)
                return result;
        }
        if (field.m_Offset % field_alignment)
            return DS_RESULT_INVALID_ARGUMENT;
        if (field_alignment > alignment)
            alignment = field_alignment;
    }
    if (size % alignment)
        return DS_RESULT_INVALID_ARGUMENT;
    *out_alignment = alignment;
    return DS_RESULT_OK;
}

static void CompileLayout(dmArray<DSFieldMeta>& metadata, const DSFieldDesc* fields, uint32_t count, uint32_t first, uint32_t base_offset, const HashState64* prefix)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        const DSFieldDesc& field = fields[i];
        HashState64        name;
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
        uint64_t    full_hash = dmHashFinal64(&full_name);
        DSFieldMeta meta = {
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
        names[i] = ReadDataInteger(metadata + (size_t)i * DATA_FIELD_META_SIZE + offsetof(DSFileFieldMeta, m_NameHash), 8);
    qsort(names, count, sizeof(uint64_t), CompareFieldNames);
    for (uint32_t i = 1; i < count; ++i)
        if (names[i] == names[i - 1])
            return false;
    return true;
}

static DSTable* NewTable(HDSContext store, const DSTableDesc* desc, DSBlobPool* pool, const uint8_t* packed_table = 0, DSTable* storage = 0, dmArray<DSFieldMeta>* metadata = 0);

static DSResult RegisterTable(HDSContext store, const DSTableDesc* desc)
{
    if (store->m_ActiveQueries.Size())
        return DS_RESULT_LOCKED;
    if ((uint32_t)desc->m_ResetPolicy > DS_RESET_RETAIN || (desc->m_TagCount && !desc->m_Tags) || desc->m_TagCount > DATA_MAX_TAG_COUNT)
        return DS_RESULT_INVALID_ARGUMENT;

    uint64_t metadata_count = 0;
    uint32_t alignment;
    DSResult result = ValidateLayout(desc->m_Fields, desc->m_FieldCount, desc->m_RowStride, 0, &metadata_count, &alignment);
    if (result != DS_RESULT_OK)
        return result;
    if (desc->m_ResetPolicy == DS_RESET_RETAIN && (uint64_t)desc->m_RowStride * 2 > SIZE_MAX)
        return DS_RESULT_INVALID_ARGUMENT;
    if (FindTable(store, desc->m_Type))
        return DS_RESULT_ALREADY_EXISTS;

    dmArray<DSFieldMeta> metadata;
    metadata.SetCapacity((uint32_t)metadata_count);
    metadata.SetSize(desc->m_FieldCount);
    CompileLayout(metadata, desc->m_Fields, desc->m_FieldCount, 0, 0, 0);
    dmArray<uint64_t> scratch;
    if (metadata_count > desc->m_FieldCount && !ValidateFieldNames((const uint8_t*)metadata.Begin(), metadata.Size(), scratch))
        return DS_RESULT_ALREADY_EXISTS;

    ++store->m_Revision;
    NewTable(store, desc, 0, 0, 0, &metadata);
    return DS_RESULT_OK;
}

DSResult DSRegisterTable(HDSContext store, const DSTableDesc* desc)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    return RegisterTable(store, desc);
}

static int CompareViewIndices(const void* a, const void* b)
{
    uint32_t x = *(const uint32_t*)a;
    uint32_t y = *(const uint32_t*)b;
    return (x > y) - (x < y);
}

static bool ViewRowsOverlap(const DSTable* a, const DSTable* b)
{
    if (a->m_Rows.Empty() || b->m_Rows.Empty())
        return false;
    uintptr_t av = (uintptr_t)a->m_Values.Begin();
    uintptr_t bv = (uintptr_t)b->m_Values.Begin();
    if (av + a->m_Values.Size() <= bv || bv + b->m_Values.Size() <= av)
        return false;
    if (av != bv || a->m_RowStride != b->m_RowStride)
        return true;
    dmArray<uint32_t> indices[2];
    const DSTable*    tables[] = { a, b };
    for (uint32_t t = 0; t < 2; ++t)
    {
        indices[t].SetCapacity(tables[t]->m_Rows.Size());
        for (uint32_t r = 0; r < tables[t]->m_Rows.Size(); ++r)
            indices[t].Push(tables[t]->m_Rows[r].m_BaseIndex);
        qsort(indices[t].Begin(), indices[t].Size(), sizeof(uint32_t), CompareViewIndices);
    }
    uint32_t i = 0, j = 0;
    while (i < indices[0].Size() && j < indices[1].Size())
    {
        if (indices[0][i] == indices[1][j])
            return true;
        if (indices[0][i] < indices[1][j])
            ++i;
        else
            ++j;
    }
    return false;
}

DSResult DSRegisterTableView(HDSContext store, const DSTableDesc* desc, void* values, uint32_t value_count, uint32_t row_count, const uint32_t* indices, DSId* out_ids)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_ActiveQueries.Size())
        return DS_RESULT_LOCKED;
    if (!desc->m_RowStride ||
        (desc->m_FieldCount && !desc->m_Fields) ||
        (uint64_t)value_count * desc->m_RowStride > UINT32_MAX ||
        row_count > UINT32_MAX / sizeof(DSRow) ||
        row_count > UINT32_MAX / sizeof(DSSlot) - store->m_Slots.Size() ||
        (!indices && row_count > value_count))
        return DS_RESULT_INVALID_ARGUMENT;
    for (uint32_t i = 0; i < desc->m_FieldCount; ++i)
        if (desc->m_Fields[i].m_Type == DS_TYPE_STRING || desc->m_Fields[i].m_Type == DS_TYPE_STRUCT || desc->m_Fields[i].m_Type == DS_TYPE_LIST)
            return DS_RESULT_INVALID_ARGUMENT;
    for (uint32_t i = 0; indices && i < row_count; ++i)
        if (indices[i] >= value_count)
            return DS_RESULT_INVALID_ARGUMENT;
    DSResult result = RegisterTable(store, desc);
    if (result != DS_RESULT_OK)
        return result;
    DSTable* table = FindTable(store, desc->m_Type);
    table->m_IsView = true;
    table->m_RetainDefaults = false;
    table->m_DefaultOffset = 0;
    uint32_t bytes = value_count * desc->m_RowStride;
    table->m_Values.Set((uint8_t*)values, bytes, bytes, true);
    table->m_Rows.SetCapacity(row_count);
    table->m_Rows.SetSize(row_count);
    ReserveSlots(store, row_count);
    for (uint32_t i = 0; i < row_count; ++i)
    {
        DSRow row = { .m_Slot = AllocateSlot(store, table, i), .m_BaseIndex = indices ? indices[i] : i };
        table->m_Rows[i] = row;
        if (out_ids)
            out_ids[i] = MakeId(store, row.m_Slot);
    }
    for (uint32_t i = 0; i < store->m_Tables.Size(); ++i)
    {
        DSTable* other = store->m_Tables[i];
        if (other == table || !other->m_IsView || !ViewRowsOverlap(table, other))
            continue;
        Reserve(table->m_ViewAliases, table->m_ViewAliases.Size() + 1);
        Reserve(other->m_ViewAliases, other->m_ViewAliases.Size() + 1);
        table->m_ViewAliases.Push(other);
        other->m_ViewAliases.Push(table);
    }
    return DS_RESULT_OK;
}

static DSTable* NewTable(HDSContext store, const DSTableDesc* desc, DSBlobPool* pool, const uint8_t* packed_table, DSTable* storage, dmArray<DSFieldMeta>* metadata)
{
    DSTable* table = storage;
    if (!table)
    {
        uint8_t* allocation = new uint8_t[sizeof(DSTable) + sizeof(DSOwnedTable)];
        table = new (allocation) DSTable;
        table->m_Owned = new (table + 1) DSOwnedTable;
        table->m_Owned->m_BaseValues = 0;
    }

    table->m_Type = desc->m_Type;
    table->m_Serial = store->m_NextTableSerial++;
    table->m_Blob = pool ? pool->m_Blob->m_Data : 0;
    if (pool)
    {
        table->m_Offsets.m_Table = (uint32_t)(packed_table - table->m_Blob);
        table->m_Offsets.m_Rows = 0;
    }
    table->m_TagCount = (uint16_t)desc->m_TagCount;
    table->m_FieldCount = (uint16_t)desc->m_FieldCount;
    table->m_MetadataCount = packed_table ? (uint16_t)ReadDataInteger(packed_table + offsetof(DSTableHeader, m_MetadataCount), 2) : (uint16_t)desc->m_FieldCount;
    table->m_RowStride = desc->m_RowStride;
    table->m_IsView = false;
    table->m_RetainDefaults = pool || desc->m_ResetPolicy == DS_RESET_RETAIN;
    table->m_Pool = pool;
    InitializeRowStorage(store, table);
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
        DSFieldMeta meta = GetFieldMeta(table, i);
        table->m_HasReferences |= !meta.m_ChildIndex && (meta.m_Type == DS_TYPE_STRING || meta.m_Type == DS_TYPE_STRUCT || meta.m_Type == DS_TYPE_LIST);
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

static void RemoveTable(HDSContext store, DSTable* table)
{
    for (uint32_t i = 0; i < table->m_ViewAliases.Size(); ++i)
    {
        dmArray<DSTable*>& aliases = table->m_ViewAliases[i]->m_ViewAliases;
        for (uint32_t j = 0; j < aliases.Size(); ++j)
            if (aliases[j] == table)
            {
                aliases.EraseSwap(j);
                break;
            }
    }
    for (uint32_t i = 0; i < store->m_Queries.Size(); ++i)
    {
        UnmatchTable(store->m_Queries[i], table);
    }
    store->m_Tables.Back()->m_StoreIndex = table->m_StoreIndex;
    store->m_Tables.EraseSwap(table->m_StoreIndex);
    DeleteTable(store, table);
}

DSResult DSUnregisterTable(HDSContext store, uint64_t type)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_ActiveQueries.Size())
        return DS_RESULT_LOCKED;
    ++store->m_Revision;
    DSTable* table = FindTable(store, type);
    if (!table)
        return DS_RESULT_NOT_FOUND;
    RemoveTable(store, table);
    return DS_RESULT_OK;
}

static uint64_t AlignBlobStorage(uint64_t size)
{
    return (size + 7) & ~(uint64_t)7;
}

static DSBlobPool* NewBlobPool(HDSContext store, HDSBlob blob)
{
    uint64_t       rows_offset = AlignBlobStorage(sizeof(DSBlobPool) + (uint64_t)blob->m_TableCount * sizeof(DSTable));
    uint64_t       page_offset = rows_offset + (uint64_t)blob->m_RowCount * sizeof(DSRow);
    const uint8_t* file = blob->m_Data;

    uint64_t       stride = AlignBlobStorage(sizeof(DSBlobInstance) + (uint64_t)blob->m_RowCount * sizeof(uint32_t));
    uint32_t       shift = 8; // Up to 256 registrations per page, bounded to 64 KiB unless one record is larger.
    while (shift && (stride << shift) > 65536)
        --shift;
    uint64_t size = page_offset + (stride << shift);
    if (size > SIZE_MAX || stride > UINT32_MAX)
        return 0;
    uint8_t*    storage = new uint8_t[(size_t)size];
    DSBlobPool* pool = new (storage) DSBlobPool;
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
    dmAtomicIncrement32(&blob->m_RefCount);

    ReserveStore(store, blob->m_TableCount, blob->m_RowCount);
    DSRow*   rows = (DSRow*)(storage + (size_t)rows_offset);
    uint32_t first_slot = 0;
    for (uint32_t t = 0; t < blob->m_TableCount; ++t)
    {
        const uint8_t* data = file + ReadDataInteger(file + DATA_TABLE_OFFSETS_OFFSET + t * 4, 4);
        DSTableDesc    desc = {
               .m_Type = ReadDataInteger(data + offsetof(DSTableHeader, m_TypeHash), 8),
               .m_TagCount = (uint32_t)ReadDataInteger(data + offsetof(DSTableHeader, m_TagCount), 2),
               .m_FieldCount = (uint32_t)ReadDataInteger(data + offsetof(DSTableHeader, m_FieldCount), 2),
               .m_RowStride = (uint32_t)ReadDataInteger(data + offsetof(DSTableHeader, m_RowStride), 4)
        };
        uint32_t       count = (uint32_t)ReadDataInteger(data + offsetof(DSTableHeader, m_RowCount), 4);
        DSTable*       table = NewTable(store, &desc, pool, data, new (GetPoolTable(pool, t)) DSTable);
        const uint8_t* records = data + DATA_TABLE_HEADER_SIZE + desc.m_TagCount * 8 + table->m_MetadataCount * DATA_FIELD_META_SIZE;
        table->m_Offsets.m_Rows = (uint32_t)(records + (size_t)count * sizeof(DSRow) - file);
        table->m_Offsets.m_FirstSlot = first_slot;
        first_slot += count;
        table->m_Rows.Set(rows, 0, count, true);
        rows += count;
    }
    return pool;
}

static bool CanAppendBlob(DSBlobPool* pool)
{
    for (uint32_t t = 0; t < pool->m_Blob->m_TableCount; ++t)
    {
        const DSTable* table = GetPoolTable(pool, t);
        uint64_t       rows = table->m_Rows.Size() + ReadDataInteger(table->m_Blob + table->m_Offsets.m_Table + offsetof(DSTableHeader, m_RowCount), 4);
        if (rows > UINT32_MAX / sizeof(DSRow) || rows * table->m_RowStride > UINT32_MAX)
            return false;
    }
    return pool->m_FreeInstance != INVALID_SLOT || pool->m_Issued != UINT32_MAX;
}

static HDSBlobInstance NewBlobInstance(DSBlobPool* pool)
{
    HDSBlobInstance instance;
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

DSResult DSAddBlob(HDSContext store, HDSBlob blob, DSGroupId group, HDSBlobInstance* out_instance)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_ActiveQueries.Size())
        return DS_RESULT_LOCKED;
    ++store->m_Revision;
    if (blob->m_RowCount > UINT32_MAX / sizeof(DSSlot) - store->m_Slots.Size() ||
        blob->m_TableCount > UINT32_MAX / sizeof(DSTable*) - store->m_Tables.Size())
        return DS_RESULT_INVALID_ARGUMENT;

    DSBlobPool* pool = store->m_Pools;
    while (pool && (pool->m_Blob != blob || !CanAppendBlob(pool)))
        pool = pool->m_Next;
    if (!pool)
    {
        pool = NewBlobPool(store, blob);
        if (!pool)
            return DS_RESULT_INVALID_ARGUMENT;
    }

    ReserveSlots(store, blob->m_RowCount);
    DSGroup* owner = GetOrCreateGroup(store, group);
    HDSBlobInstance instance = NewBlobInstance(pool);
    instance->m_Group = group;
    instance->m_GroupPrevious = 0;
    instance->m_GroupNext = 0;
    if (group)
    {
        instance->m_GroupNext = owner->m_FirstInstance;
        if (owner->m_FirstInstance)
            owner->m_FirstInstance->m_GroupPrevious = instance;
        owner->m_FirstInstance = instance;
    }
    uint32_t*       slots = GetInstanceSlots(instance);
    for (uint32_t t = 0; t < blob->m_TableCount; ++t)
    {
        DSTable* table = GetPoolTable(pool, t);
        uint32_t count = (uint32_t)ReadDataInteger(table->m_Blob + table->m_Offsets.m_Table + offsetof(DSTableHeader, m_RowCount), 4);
        if (!count)
            continue;
        uint32_t start = table->m_Rows.Size();
        ReserveTableArray(table, table->m_Rows, start + count);
        ReserveRowStorage(store, table, start + count);
        table->m_Rows.SetSize(start + count);
        size_t         record_bytes = (size_t)count * sizeof(DSRow);
        const uint8_t* records = table->m_Blob + table->m_Offsets.m_Rows - record_bytes;
        DSRow*         rows = table->m_Rows.Begin() + start;
        memcpy(rows, records, record_bytes);
        InitializeBlobValues(table, start, count);
        for (uint32_t r = 0; r < count; ++r)
        {
            rows[r].m_Group = group;
            rows[r].m_Slot = AllocateSlot(store, table, start + r);
            AddGroupRow(store, owner, rows[r].m_Slot);
            rows[r].m_BaseIndex = r;
            rows[r].m_InstanceIndex = instance->m_Index;
            slots[table->m_Offsets.m_FirstSlot + r] = rows[r].m_Slot;
        }
    }

    *out_instance = instance;
    return DS_RESULT_OK;
}

static void DeleteBlobPool(DSBlobPool* pool)
{
    HDSContext store = pool->m_Store;
    for (uint32_t i = 0; i < pool->m_Issued; ++i)
    {
        HDSBlobInstance instance = GetPoolInstance(pool, i);
        if (instance->m_Pool)
            DeletePayloads(&instance->m_Payloads);
    }
    for (uint32_t t = 0; t < pool->m_Blob->m_TableCount; ++t)
        RemoveTable(store, GetPoolTable(pool, t));
    DSBlobPool** link = &store->m_Pools;
    while (*link != pool)
        link = &(*link)->m_Next;
    *link = pool->m_Next;
    DSDestroyBlob(pool->m_Blob);
    for (uint32_t i = 0; i < pool->m_Pages.Size(); ++i)
        delete[] pool->m_Pages[i];
    pool->~DSBlobPool();
    delete[] (uint8_t*)pool;
}

static void RemoveBlob(HDSBlobInstance instance)
{
    DSBlobPool* pool = instance->m_Pool;
    HDSContext  store = pool->m_Store;
    if (instance->m_Group)
    {
        if (instance->m_GroupPrevious)
            instance->m_GroupPrevious->m_GroupNext = instance->m_GroupNext;
        else
            store->m_Groups.Get(instance->m_Group)->m_FirstInstance = instance->m_GroupNext;
        if (instance->m_GroupNext)
            instance->m_GroupNext->m_GroupPrevious = instance->m_GroupPrevious;
    }
    const uint32_t* slots = GetInstanceSlots(instance);
    for (uint32_t i = 0; i < pool->m_Blob->m_RowCount; ++i)
        if (slots[i] != INVALID_SLOT)
            RemoveRow(store, &store->m_Slots[slots[i]]);
    DeletePayloads(&instance->m_Payloads);
    instance->m_Pool = 0;
    instance->m_NextFree = pool->m_FreeInstance;
    pool->m_FreeInstance = instance->m_Index;
    if (!--pool->m_Live)
        DeleteBlobPool(pool);
}

DSResult DSRemoveBlob(HDSBlobInstance instance)
{
    HDSContext store = instance->m_Pool->m_Store;
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_ActiveQueries.Size())
        return DS_RESULT_LOCKED;
    ++store->m_Revision;
    RemoveBlob(instance);
    return DS_RESULT_OK;
}

DSResult DSAddRow(HDSContext store, uint64_t type, DSGroupId group, const DSValueType* types, const DSValueData* values, uint32_t value_count, DSId* out_id, uint64_t component_id)
{
    DSRowDesc row = {
        .m_Group = group,
        .m_Types = types,
        .m_Values = values,
        .m_ValueCount = value_count,
        .m_ComponentId = component_id
    };
    return DSAddRows(store, type, &row, 1, out_id);
}

static void PublishRows(HDSContext store, DSTable* table, const DSRowDesc* rows, DSGroupId group, uint32_t count, DSId* out_ids)
{
    if (table->m_RetainDefaults && table->m_RowStride)
    {
        uint32_t row = table->m_Rows.Size();
        for (uint32_t copied = 0; copied < count;)
        {
            uint32_t span = GetRowSpan(table, row + copied, count - copied);
            memcpy(GetRowBytes(table, row + copied), GetNativeDefaultRow(table, row + copied), (size_t)span * table->m_RowStride);
            copied += span;
        }
    }
    AddRowSlots(store, table, rows, group, count, out_ids);
}

// Fixed fields use native bytes. Validate booleans and measure reference
// payloads before allocating or publishing the batch.
static DSResult ValidateNativeRows(const DSTable* table, uint32_t num_rows, const void* rows, size_t* payload_size)
{
    if (table->m_RowStride && !rows)
        return DS_RESULT_INVALID_ARGUMENT;
    for (uint32_t f = 0; f < table->m_MetadataCount; ++f)
    {
        DSFieldMeta meta = GetFieldMeta(table, f);
        if (meta.m_Type != DS_TYPE_BOOLEAN)
            continue;
        const uint8_t* values = (const uint8_t*)rows + meta.m_Offset;
        for (uint32_t r = 0; r < num_rows; ++r)
            if (values[(size_t)r * table->m_RowStride] > 1)
                return DS_RESULT_INVALID_ARGUMENT;
    }
    if (table->m_HasReferences)
    {
        for (uint32_t f = 0; f < table->m_FieldCount; ++f)
        {
            DSFieldMeta meta = GetFieldMeta(table, f);
            DSResult    result = ValidateNativeReferences(table, meta, (const uint8_t*)rows + meta.m_Offset, table->m_RowStride, num_rows, 0, payload_size);
            if (result != DS_RESULT_OK)
                return result;
        }
    }
    return DS_RESULT_OK;
}

static bool CanCreateRows(HDSContext store, const DSTable* table, uint32_t count)
{
    return !table->m_IsView && count <= UINT32_MAX / sizeof(DSSlot) - store->m_Slots.Size() &&
    count <= UINT32_MAX / sizeof(DSRow) - table->m_Rows.Size() &&
    (uint64_t)(table->m_Rows.Size() + count) * table->m_RowStride <= UINT32_MAX;
}

DSResult DSCreateRows(HDSContext store, uint64_t type, DSGroupId group, uint32_t count, const void* rows, DSId* out_ids)
{
    if (!count)
        return DS_RESULT_OK;
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_ActiveQueries.Size())
        return DS_RESULT_LOCKED;
    DSTable* table = FindTable(store, type);
    if (!table)
        return DS_RESULT_NOT_FOUND;
    if (!CanCreateRows(store, table, count))
        return DS_RESULT_INVALID_ARGUMENT;
    size_t   payload_size = 0;
    DSResult result = ValidateNativeRows(table, count, rows, &payload_size);
    if (result != DS_RESULT_OK)
        return result;

    ReserveRowValues(store, table, count, payload_size);
    for (uint32_t first = 0; first < count;)
    {
        uint32_t row = table->m_Rows.Size() + first;
        uint32_t span = GetRowSpan(table, row, count - first);
        uint8_t* data = GetNewRowValues(table, row);
        if (table->m_RowStride)
            memcpy(data, (const uint8_t*)rows + (size_t)first * table->m_RowStride, (size_t)span * table->m_RowStride);
        if (table->m_HasReferences)
            StoreNativeReferences(table, span, data);
        first += span;
    }
    ++store->m_Revision;
    PublishRows(store, table, 0, group, count, out_ids);
    return DS_RESULT_OK;
}

static bool ValidateColumnBooleans(const DSTable* table, const DSFieldMeta& meta, const uint8_t* values, uint32_t stride, uint32_t count)
{
    if (!meta.m_Size)
        return true;
    if (meta.m_Type == DS_TYPE_BOOLEAN)
    {
        for (uint32_t r = 0; r < count; ++r)
            if (values[(size_t)r * stride] > 1)
                return false;
    }
    for (uint32_t i = 0; i < meta.m_ChildCount; ++i)
    {
        DSFieldMeta child = GetFieldMeta(table, meta.m_ChildIndex + i);
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

DSResult DSCreateRowsSoA(HDSContext store, uint64_t type, DSGroupId group, uint32_t count, uint32_t field_count, const DSFieldArray* fields, DSId* out_ids)
{
    if (!count)
        return DS_RESULT_OK;
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_ActiveQueries.Size())
        return DS_RESULT_LOCKED;
    DSTable* table = FindTable(store, type);
    if (!table)
        return DS_RESULT_NOT_FOUND;
    if (!CanCreateRows(store, table, count) ||
        field_count != table->m_FieldCount || (field_count && !fields))
        return DS_RESULT_INVALID_ARGUMENT;

    DSColumnBinding          local[32];
    dmArray<DSColumnBinding> columns;
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
            return DS_RESULT_INVALID_ARGUMENT;
        DSFieldMeta    meta = GetFieldMeta(table, index);
        const uint8_t* values = (const uint8_t*)fields[i].m_Values;
        if ((meta.m_Size && !values) || !ValidateColumnBooleans(table, meta, values, meta.m_Size, count))
            return DS_RESULT_INVALID_ARGUMENT;
        if (table->m_HasReferences)
        {
            DSResult result = ValidateNativeReferences(table, meta, values, meta.m_Size, count, 0, &payload_size);
            if (result != DS_RESULT_OK)
                return result;
        }
        columns[index] = { .m_Values = values, .m_Offset = meta.m_Offset, .m_Size = meta.m_Size };
        field_bytes += meta.m_Size;
    }

    ReserveRowValues(store, table, count, payload_size);
    uint32_t stride = table->m_RowStride;
    for (uint32_t first = 0; first < count;)
    {
        uint32_t row = table->m_Rows.Size() + first;
        uint32_t span = GetRowSpan(table, row, count - first);
        uint8_t* data = GetNewRowValues(table, row);
        // Registered fields do not overlap; clear padding only when the row has gaps.
        if (field_bytes < stride)
            memset(data, 0, (size_t)span * stride);
        for (uint32_t f = 0; f < field_count; ++f)
        {
            const DSColumnBinding& column = columns[f];
            if (!column.m_Size)
                continue;
            uint8_t*       dst = data + column.m_Offset;
            const uint8_t* src = column.m_Values + (size_t)first * column.m_Size;
            // Dispatch common sizes once so each row uses native loads/stores.
            switch (column.m_Size)
            {
                case 1:
                    CopyColumn<1>(dst, src, stride, span);
                    break;
                case 4:
                    CopyColumn<4>(dst, src, stride, span);
                    break;
                case 8:
                    CopyColumn<8>(dst, src, stride, span);
                    break;
                case 12:
                    CopyColumn<12>(dst, src, stride, span);
                    break;
                case 16:
                    CopyColumn<16>(dst, src, stride, span);
                    break;
                case 64:
                    CopyColumn<64>(dst, src, stride, span);
                    break;
                default:
                    for (uint32_t r = 0; r < span; ++r)
                        memcpy(dst + (size_t)r * stride, src + (size_t)r * column.m_Size, column.m_Size);
            }
        }
        if (table->m_HasReferences)
            StoreNativeReferences(table, span, data);
        first += span;
    }
    ++store->m_Revision;
    PublishRows(store, table, 0, group, count, out_ids);
    return DS_RESULT_OK;
}

DSResult DSAddRows(HDSContext store, uint64_t type, const DSRowDesc* rows, uint32_t count, DSId* out_ids)
{
    if (!count)
        return DS_RESULT_OK;
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_ActiveQueries.Size())
        return DS_RESULT_LOCKED;
    ++store->m_Revision;
    DSTable* table = FindTable(store, type);
    if (!table)
        return DS_RESULT_NOT_FOUND;
    if (!rows || !CanCreateRows(store, table, count))
        return DS_RESULT_INVALID_ARGUMENT;

    size_t   payload_size = 0;
    DSResult result = ValidateRowValues(table, rows, count, &payload_size);
    if (result != DS_RESULT_OK)
        return result;

    ReserveRowValues(store, table, count, payload_size);
    if (table->m_RowStride)
    {
        for (uint32_t first = 0; first < count;)
        {
            uint32_t row = table->m_Rows.Size() + first;
            uint32_t span = GetRowSpan(table, row, count - first);
            uint8_t* data = GetNewRowValues(table, row);
            memset(data, 0, (size_t)span * table->m_RowStride);
            StoreRowValues(table, rows + first, span, data);
            first += span;
        }
    }
    PublishRows(store, table, rows, 0, count, out_ids);
    return DS_RESULT_OK;
}

static void RemoveRow(HDSContext store, DSSlot* slot)
{
    DSTable* table = slot->m_Table;
    uint32_t row = slot->m_Row;
    if (table->m_HasReferences)
        ReleaseRowValues(table, row, true);
    if (table->m_Pool)
    {
        const DSRow&    removed = table->m_Rows[row];
        HDSBlobInstance instance = GetPoolInstance(table->m_Pool, removed.m_InstanceIndex);
        GetInstanceSlots(instance)[table->m_Offsets.m_FirstSlot + removed.m_BaseIndex] = INVALID_SLOT;
    }
    ReleaseSlot(store, table->m_Rows[row].m_Slot);
    uint32_t last = table->m_Rows.Size() - 1;
    DSRow*   last_record = table->m_Rows.Begin() + last;
    if (row != last)
        store->m_Slots.Begin()[last_record->m_Slot].m_Row = row;
    if (row != last && table->m_RowStride)
    {
        const DSRowChunk* chunks = table->m_Chunks.Begin();
        uint32_t          shift = table->m_ChunkShift, mask = table->m_ChunkMask;
        uint32_t          stride = table->m_RowStride;
        uint8_t*          target = chunks[row >> shift].m_Data + (size_t)(row & mask) * stride;
        uint8_t*          source = chunks[last >> shift].m_Data + (size_t)(last & mask) * stride;
        memcpy(target, source, table->m_RowStride);
        if (table->m_DefaultOffset)
            memcpy(target + table->m_DefaultOffset, source + table->m_DefaultOffset, table->m_RowStride);
    }
    if (!table->m_Pool)
    {
        last_record->m_BaseIndex = row;
        if (!last)
        {
            DeletePayloads(&table->m_Owned->m_BaseValues);
            DeletePayloads(&table->m_Payloads);
        }
    }
    table->m_Rows.Begin()[row] = *last_record;
    table->m_Rows.SetSize(last);
    if (!(last & table->m_ChunkMask))
        TrimRowStorage(store, table, last);
}

DSResult DSRemoveRow(HDSContext store, DSId id)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_ActiveQueries.Size())
        return DS_RESULT_LOCKED;
    DSSlot* slot = FindSlot(store, id);
    if (!slot)
        return DS_RESULT_NOT_FOUND;
    if (slot->m_Table->m_IsView)
        return DS_RESULT_INVALID_ARGUMENT;
    ++store->m_Revision;
    RemoveRow(store, slot);
    return DS_RESULT_OK;
}

DSResult DSRemoveGroup(HDSContext store, DSGroupId id)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_ActiveQueries.Size())
        return DS_RESULT_LOCKED;
    DSGroup* group = store->m_Groups.Get(id);
    if (!group)
        return DS_RESULT_NOT_FOUND;
    ++store->m_Revision;
    while (group->m_FirstInstance)
        RemoveBlob(group->m_FirstInstance);
    while (group->m_FirstSlot != INVALID_SLOT)
        RemoveRow(store, &store->m_Slots[group->m_FirstSlot]);
    store->m_Groups.Erase(id);
    return DS_RESULT_OK;
}

uint64_t DSGetComponentId(HDSContext store, DSId id)
{
    DSSlot* slot = FindSlot(store, id);
    return slot ? slot->m_Table->m_Rows[slot->m_Row].m_ComponentId : 0;
}
