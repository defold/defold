// Copyright 2026 The Defold Foundation
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
#include <new>
#include <string.h>
#include <dlib/endian.h>
#include <dlib/static_assert.h>
#include "data.h"

static const uint32_t INVALID_SLOT = UINT32_MAX;

// The format's alignment must suffice for every native pointer view.
template <typename T>
struct DataNativeAlignment
{
    uint8_t m_Padding;
    T       m_Value;
};
DM_STATIC_ASSERT(sizeof(double) == 8 && offsetof(DataNativeAlignment<double>, m_Value) <= 8, Invalid_number_layout);
DM_STATIC_ASSERT(sizeof(float) == 4 && offsetof(DataNativeAlignment<float>, m_Value) <= 4, Invalid_float_layout);
DM_STATIC_ASSERT(sizeof(DataVector3) == 12 && offsetof(DataNativeAlignment<DataVector3>, m_Value) <= 4, Invalid_vector3_layout);
DM_STATIC_ASSERT(sizeof(DataVector4) == 16 && offsetof(DataNativeAlignment<DataVector4>, m_Value) <= 4, Invalid_vector4_layout);
DM_STATIC_ASSERT(sizeof(DataMatrix4) == 64 && offsetof(DataNativeAlignment<DataMatrix4>, m_Value) <= 4, Invalid_matrix4_layout);

// Row storage contains payload bytes, not DataValue objects. DataPropertyMeta
// supplies each property's fixed type and byte offset once per table. Numeric
// payloads use little-endian bytes with C-compatible member/row alignment.
// DataValue is a temporary tagged input/result for internal generic operations. Its tag selects
// the union member and validates writes; typed getters do not construct one.
// Declared fixed structs are inline bytes with shared child metadata. Dynamic
// structs/lists retain their own child type/offset arrays and struct name hashes.

// One allocation in a registration, table or query arena. Payload follows the
// header at an eight-byte boundary. Used bytes include padding; capacity excludes
// the header. Blocks never move. Registration/table reset frees replacement
// payloads; independently added default values remain until table destruction.
struct DataBlock
{
    DataBlock* m_Next;
    size_t     m_Size;
    size_t     m_Capacity;
};

static void ReserveData(DataBlock** blocks, size_t size, size_t minimum_capacity = 4096)
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

static void* AllocateData(DataBlock** blocks, size_t size, size_t minimum_capacity = 4096)
{
    ReserveData(blocks, size, minimum_capacity);
    const size_t header_size = (sizeof(DataBlock) + 7) & ~(size_t)7;
    DataBlock*   block = *blocks;
    size_t       offset = (block->m_Size + 7) & ~(size_t)7;
    void*        result = (uint8_t*)block + header_size + offset;
    block->m_Size = offset + size;
    return result;
}

uint64_t ReadDataInteger(const uint8_t* bytes, uint32_t size)
{
    uint64_t value = 0;
    for (uint32_t i = 0; i < size; ++i)
        value |= (uint64_t)bytes[i] << (i * 8);
    return value;
}

void WriteDataInteger(uint8_t* bytes, uint64_t value, uint32_t size)
{
    for (uint32_t i = 0; i < size; ++i)
        bytes[i] = (uint8_t)(value >> (i * 8));
}

// Size of a property's slot in a byte row. Reference slots hold a blob offset
// for borrowed defaults or an arena pointer for owned values, both in 8 bytes.
uint32_t DataTypeSize(DataValueType type)
{
    switch (type)
    {
        case DATA_VALUE_TYPE_NULL:
            return 0;
        case DATA_VALUE_TYPE_BOOLEAN:
            return 1;
        case DATA_VALUE_TYPE_NUMBER:
        case DATA_VALUE_TYPE_STRING:
        case DATA_VALUE_TYPE_STRUCT:
        case DATA_VALUE_TYPE_LIST:
            return 8;
        case DATA_VALUE_TYPE_VECTOR3:
            return 12;
        case DATA_VALUE_TYPE_VECTOR4:
            return 16;
        case DATA_VALUE_TYPE_MATRIX4:
            return 64;
        default:
            return UINT32_MAX;
    }
}

uint32_t DataTypeAlignment(DataValueType type)
{
    switch (type)
    {
        case DATA_VALUE_TYPE_NUMBER:
        case DATA_VALUE_TYPE_STRING:
        case DATA_VALUE_TYPE_STRUCT:
        case DATA_VALUE_TYPE_LIST:
            return 8;
        case DATA_VALUE_TYPE_VECTOR3:
        case DATA_VALUE_TYPE_VECTOR4:
        case DATA_VALUE_TYPE_MATRIX4:
            return 4;
        default:
            return 1;
    }
}

// Fixed-size copies preserve float bits and support unaligned source bytes.
// Only big-endian hosts need to convert the little-endian payload per float.
static void ReadDataFloats(void* out_value, const uint8_t* bytes, uint32_t count)
{
#if DM_ENDIAN == DM_ENDIAN_LITTLE
    memcpy(out_value, bytes, count * sizeof(float));
#else
    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t bits = (uint32_t)ReadDataInteger(bytes + i * 4, 4);
        memcpy((uint8_t*)out_value + i * 4, &bits, 4);
    }
#endif
}

// A packed container is a view. Reading it never allocates or expands its children.
// The type argument comes from table metadata or a container's child metadata;
// there is no type tag alongside the scalar payload at buffer + offset.
static DataValue ReadPackedValue(const uint8_t* buffer, DataValueType type, uint32_t offset)
{
    DataValue value = {};
    value.m_Type = type;
    const uint8_t* data = buffer + offset;
    switch (type)
    {
        case DATA_VALUE_TYPE_NUMBER:
        {
            uint64_t bits = ReadDataInteger(data, 8);
            memcpy(&value.m_Value.m_Number, &bits, 8);
            break;
        }
        case DATA_VALUE_TYPE_BOOLEAN:
            value.m_Value.m_Boolean = *data;
            break;
        case DATA_VALUE_TYPE_STRING:
            value.m_Value.m_String = (const char*)data;
            break;
        case DATA_VALUE_TYPE_STRUCT:
            value.m_Value.m_Struct.m_Buffer = buffer;
            value.m_Value.m_Struct.m_Offset = offset;
            value.m_Value.m_Struct.m_Count = (uint32_t)ReadDataInteger(data + 4, 4);
            break;
        case DATA_VALUE_TYPE_LIST:
            value.m_Value.m_List.m_Buffer = buffer;
            value.m_Value.m_List.m_Offset = offset;
            value.m_Value.m_List.m_Count = (uint32_t)ReadDataInteger(data + 4, 4);
            break;
        case DATA_VALUE_TYPE_VECTOR3:
            ReadDataFloats(value.m_Value.m_Vector3, data, 3);
            break;
        case DATA_VALUE_TYPE_VECTOR4:
            ReadDataFloats(value.m_Value.m_Vector4, data, 4);
            break;
        case DATA_VALUE_TYPE_MATRIX4:
            ReadDataFloats(value.m_Value.m_Matrix4, data, 16);
            break;
        default:
            break;
    }
    return value;
}

// File references are blob-relative offsets. In mutable rows, a low-bit tag
// distinguishes a shared blob offset (offset << 1 | 1) from an aligned arena
// pointer (low bit zero). Only reference slots use this encoding; numeric access
// is a direct load. The tagged offset fits in 64 bits on 32-bit hosts too.
// Owned payloads are eight-byte aligned; dynamic children retain root-relative
// offsets within their original blob or owned payload, without pointer fixups.
static DataValue ReadStoredValue(const DataTable* table, const DataPropertyMeta& meta, const uint8_t* bytes)
{
    if (meta.m_ChildIndex)
    {
        DataValue value = {};
        value.m_Type = DATA_VALUE_TYPE_STRUCT;
        DataStruct object = { 0, 0, meta.m_ChildCount, bytes, meta.m_ChildIndex, table };
        value.m_Value.m_Struct = object;
        return value;
    }
    if (meta.m_Type == DATA_VALUE_TYPE_NULL)
    {
        DataValue value = {};
        value.m_Type = DATA_VALUE_TYPE_NULL;
        return value;
    }
    if (meta.m_Type != DATA_VALUE_TYPE_STRING && meta.m_Type != DATA_VALUE_TYPE_STRUCT && meta.m_Type != DATA_VALUE_TYPE_LIST)
        return ReadPackedValue(bytes, meta.m_Type, 0);
    uint64_t reference = ReadDataInteger(bytes, 8);
    return (reference & 1) ? ReadPackedValue(table->m_Blob, meta.m_Type, (uint32_t)(reference >> 1)) :
                             ReadPackedValue((const uint8_t*)(uintptr_t)reference, meta.m_Type, 0);
}

uint64_t GetChildName(const DataStruct* object, uint32_t index)
{
    if (object->m_Table)
        return GetPropertyMeta(object->m_Table, object->m_Offset + index).m_Property;
    return object->m_Buffer ? ReadDataInteger(object->m_Buffer + object->m_Offset + 8 + index * 8, 8) : object->m_Names[index];
}

DataValue GetChildValue(const DataValue* value, uint32_t index)
{
    bool              named = value->m_Type == DATA_VALUE_TYPE_STRUCT;
    const DataStruct* object = &value->m_Value.m_Struct;
    if (named && object->m_Table)
    {
        DataPropertyMeta meta = GetPropertyMeta(object->m_Table, object->m_Offset + index);
        const uint8_t*   bytes = meta.m_Size ? object->m_Buffer + meta.m_Offset : 0;
        return ReadStoredValue(object->m_Table, meta, bytes);
    }
    const uint8_t* buffer = named ? value->m_Value.m_Struct.m_Buffer : value->m_Value.m_List.m_Buffer;
    if (!buffer)
        return named ? value->m_Value.m_Struct.m_Values[index] : value->m_Value.m_List.m_Values[index];
    uint32_t       offset = named ? value->m_Value.m_Struct.m_Offset : value->m_Value.m_List.m_Offset;
    uint32_t       count = named ? value->m_Value.m_Struct.m_Count : value->m_Value.m_List.m_Count;
    const uint8_t* types = buffer + offset + 8 + (named ? count * 8 : 0);
    DataValueType  type = (DataValueType)ReadDataInteger(types + index * 4, 4);
    uint32_t       child = (uint32_t)ReadDataInteger(types + count * 4 + index * 4, 4);
    return ReadPackedValue(buffer, type, child);
}

DataResult DataGetStructProperty(const DataStruct* object, uint64_t property, DataValue* out_value)
{
    for (uint32_t i = 0; i < object->m_Count; ++i)
    {
        if (GetChildName(object, i) == property)
        {
            DataValue value = {};
            value.m_Type = DATA_VALUE_TYPE_STRUCT;
            value.m_Value.m_Struct = *object;
            *out_value = GetChildValue(&value, i);
            return DATA_RESULT_OK;
        }
    }
    return DATA_RESULT_NOT_FOUND;
}

DataResult DataGetListValue(const DataList* list, uint32_t index, DataValue* out_value)
{
    if (index >= list->m_Count)
        return DATA_RESULT_NOT_FOUND;
    DataValue value = {};
    value.m_Type = DATA_VALUE_TYPE_LIST;
    value.m_Value.m_List = *list;
    *out_value = GetChildValue(&value, index);
    return DATA_RESULT_OK;
}

static bool IsReference(DataValueType type)
{
    return type == DATA_VALUE_TYPE_STRING || type == DATA_VALUE_TYPE_STRUCT || type == DATA_VALUE_TYPE_LIST;
}

static void StoreScalar(uint8_t* bytes, const DataValue* value)
{
    switch (value->m_Type)
    {
        case DATA_VALUE_TYPE_NUMBER:
        {
            uint64_t bits;
            memcpy(&bits, &value->m_Value.m_Number, 8);
            WriteDataInteger(bytes, bits, 8);
            break;
        }
        case DATA_VALUE_TYPE_BOOLEAN:
            *bytes = value->m_Value.m_Boolean;
            break;
        case DATA_VALUE_TYPE_VECTOR3:
        case DATA_VALUE_TYPE_VECTOR4:
        case DATA_VALUE_TYPE_MATRIX4:
        {
            const float* values = value->m_Type == DATA_VALUE_TYPE_VECTOR3 ? value->m_Value.m_Vector3 :
            value->m_Type == DATA_VALUE_TYPE_VECTOR4                       ? value->m_Value.m_Vector4 :
                                                                             value->m_Value.m_Matrix4;
            for (uint32_t i = 0; i < DataTypeSize(value->m_Type) / 4; ++i)
            {
                uint32_t bits;
                memcpy(&bits, &values[i], 4);
                WriteDataInteger(bytes + i * 4, bits, 4);
            }
            break;
        }
        default:
            break;
    }
}

// Owned containers share the packed view layout. Child references are 32-bit
// offsets from the root, so no DataValue arrays or child pointers are retained.
static size_t StoreOwnedValue(uint8_t* buffer, size_t offset, const DataValue* value)
{
    uint8_t* bytes = buffer + offset;
    if (value->m_Type == DATA_VALUE_TYPE_STRUCT || value->m_Type == DATA_VALUE_TYPE_LIST)
    {
        bool     named = value->m_Type == DATA_VALUE_TYPE_STRUCT;
        uint32_t count = named ? value->m_Value.m_Struct.m_Count : value->m_Value.m_List.m_Count;
        uint8_t* types = bytes + 8 + (named ? count * 8 : 0);
        uint8_t* offsets = types + count * 4;
        WriteDataInteger(bytes, 0, 4);
        WriteDataInteger(bytes + 4, count, 4);
        offset += 8 + (size_t)count * (named ? 16 : 8);
        for (uint32_t i = 0; i < count; ++i)
        {
            DataValue child = GetChildValue(value, i);
            if (named)
                WriteDataInteger(bytes + 8 + i * 8, GetChildName(&value->m_Value.m_Struct, i), 8);
            WriteDataInteger(types + i * 4, child.m_Type, 4);
            WriteDataInteger(offsets + i * 4, offset, 4);
            offset = StoreOwnedValue(buffer, offset, &child);
        }
        return offset;
    }
    size_t size = DataTypeSize(value->m_Type);
    if (value->m_Type == DATA_VALUE_TYPE_STRING)
    {
        size = strlen(value->m_Value.m_String) + 1;
        memcpy(bytes, value->m_Value.m_String, size);
    }
    else
    {
        StoreScalar(bytes, value);
    }
    size_t aligned_size = (size + 7) & ~(size_t)7;
    memset(bytes + size, 0, aligned_size - size);
    return offset + aligned_size;
}

// Callers validate and reserve the complete payload before writing. Blocks never
// move, so values borrowed from this same table remain valid during insertion/set.
static void StoreValue(DataBlock** blocks, uint8_t* bytes, const DataValue* value)
{
    if (!IsReference(value->m_Type))
    {
        StoreScalar(bytes, value);
        return;
    }
    DataBlock* block = *blocks;
    size_t     offset = (block->m_Size + 7) & ~(size_t)7;
    uint8_t*   buffer = (uint8_t*)block + ((sizeof(DataBlock) + 7) & ~(size_t)7) + offset;
    block->m_Size = offset + StoreOwnedValue(buffer, 0, value);
    assert(block->m_Size <= block->m_Capacity);
    WriteDataInteger(bytes, (uintptr_t)buffer, 8);
}

DataPropertyMeta GetPropertyMeta(const DataTable* table, uint32_t index)
{
    if (!table->m_Pool)
        return table->m_Owned->m_Properties[index];
    const uint8_t*   meta = table->m_Blob + table->m_Offsets.m_Table + DATA_TABLE_HEADER_SIZE + table->m_TagCount * 8 + index * DATA_PROPERTY_META_SIZE;
    DataPropertyMeta result = {
        ReadDataInteger(meta, 8), (DataValueType)ReadDataInteger(meta + 8, 4), (uint32_t)ReadDataInteger(meta + 12, 4), (uint32_t)ReadDataInteger(meta + 16, 4), (uint32_t)ReadDataInteger(meta + 20, 4), (uint32_t)ReadDataInteger(meta + 24, 4)
    };
    return result;
}

uint64_t GetTableTag(const DataTable* table, uint32_t index)
{
    return table->m_Pool ? ReadDataInteger(table->m_Blob + table->m_Offsets.m_Table + DATA_TABLE_HEADER_SIZE + index * 8, 8) : table->m_Owned->m_Tags[index];
}

static const uint8_t* GetPropertyBytes(const DataTable* table, const DataRow* row, uint32_t offset)
{
    return table->m_Values.Begin() + (size_t)(row - table->m_Rows.Begin()) * table->m_RowStride + offset;
}

static uint8_t* GetPropertyBytes(DataTable* table, const DataRow* row, uint32_t offset)
{
    return table->m_Values.Begin() + (size_t)(row - table->m_Rows.Begin()) * table->m_RowStride + offset;
}

static DataValue ReadBoundValue(const DataTable* table, const DataRow* row, const DataPropertyMeta& meta)
{
    const uint8_t* bytes = meta.m_Size ? GetPropertyBytes(table, row, meta.m_Offset) : 0;
    return ReadStoredValue(table, meta, bytes);
}

DataValue ReadRowValue(const DataTable* table, const DataRow* row, uint32_t property_index)
{
    return ReadBoundValue(table, row, GetPropertyMeta(table, property_index));
}

// Tag only the mutable copy. Fixed structs recurse through shared metadata;
// strings and dynamic containers continue to borrow their original blob payloads.
static void TagBlobReferences(const DataTable* table, const DataPropertyMeta& meta, uint8_t* bytes, uint32_t rows)
{
    if (meta.m_ChildIndex)
    {
        for (uint32_t i = 0; i < meta.m_ChildCount; ++i)
        {
            DataPropertyMeta child = GetPropertyMeta(table, meta.m_ChildIndex + i);
            TagBlobReferences(table, child, bytes + child.m_Offset, rows);
        }
    }
    else if (IsReference(meta.m_Type))
    {
        for (uint32_t r = 0; r < rows; ++r)
        {
            uint8_t* value = bytes + (size_t)r * table->m_RowStride;
            WriteDataInteger(value, (ReadDataInteger(value, 8) << 1) | 1, 8);
        }
    }
}

static void InitializeBlobValues(DataTable* table, uint32_t start, uint32_t count)
{
    if (!table->m_RowStride || !count)
        return;
    uint8_t* bytes = table->m_Values.Begin() + (size_t)start * table->m_RowStride;
    memcpy(bytes, table->m_Blob + table->m_Offsets.m_Rows, (size_t)count * table->m_RowStride);
    for (uint32_t i = 0; i < table->m_PropertyCount; ++i)
    {
        DataPropertyMeta meta = GetPropertyMeta(table, i);
        TagBlobReferences(table, meta, bytes + meta.m_Offset, count);
    }
}

static const uint8_t* GetDefaultRow(const DataTable* table, const DataRow* row)
{
    const uint8_t* base = table->m_Pool ? table->m_Blob + table->m_Offsets.m_Rows : table->m_Owned->m_BaseRows.Begin();
    return base + (size_t)row->m_BaseIndex * table->m_RowStride;
}

static void ResetRow(DataTable* table, DataRow* row)
{
    if (!table->m_RowStride)
        return;
    uint8_t* bytes = GetPropertyBytes(table, row, 0);
    memcpy(bytes, GetDefaultRow(table, row), table->m_RowStride);
    if (table->m_Pool)
    {
        for (uint32_t i = 0; i < table->m_PropertyCount; ++i)
        {
            DataPropertyMeta meta = GetPropertyMeta(table, i);
            TagBlobReferences(table, meta, bytes + meta.m_Offset, 1);
        }
    }
}

static void DeleteBlocks(DataBlock** blocks)
{
    while (*blocks)
    {
        DataBlock* block = *blocks;
        *blocks = block->m_Next;
        delete[] (uint8_t*)block;
    }
}

// One requested property resolved for one matching table. Cached metadata
// supplies the field offset and kind without a name lookup per row.
struct DataQueryBinding
{
    DataPropertyMeta m_Meta;
};

// A borrowed table and its query-owned bindings, in query property order.
// m_Fields is NULL for tag-only queries. Table removal releases these bindings
// to the query's free list before the table storage is destroyed.
struct DataQueryTable
{
    DataTable*        m_Table;
    DataQueryBinding* m_Fields;
};

// Live query owned by its caller and registered with m_Store. Owns copies of
// filters, the matching-table array and the arena holding property bindings;
// it borrows the store/tables and must be destroyed before the store.
// Table changes update matches; owner filtering happens during row iteration.
struct DataQuery
{
    HDataStore                 m_Store;
    dmArray<DataQueryTable>    m_Tables;
    dmArray<DataQueryProperty> m_Properties;
    dmArray<uint64_t>          m_Paths;
    dmArray<DataOwnerId>       m_Owners;
    dmArray<uint64_t>          m_Tags;
    DataBlock*                 m_Bindings;
    DataQueryBinding*          m_FreeFields; // Recycled binding arrays; first bytes hold the next pointer.
};

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
        (uint64_t)query->m_Properties.Capacity() * sizeof(DataQueryProperty) + query->m_Paths.Capacity() * sizeof(uint64_t) +
        (uint64_t)query->m_Owners.Capacity() * sizeof(DataOwnerId) +
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
            (uint64_t)owned->m_Properties.Capacity() * sizeof(DataPropertyMeta);
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
        out_stats->m_ValueBytes += table->m_Values.Capacity();
        GetPayloadMemory(table->m_Payloads, out_stats);
    }
    out_stats->m_TotalBytes = out_stats->m_StoreBytes + out_stats->m_TableBytes + out_stats->m_RowMetadataBytes +
    out_stats->m_SlotBytes + out_stats->m_BaseRowBytes + out_stats->m_ValueBytes + out_stats->m_InstanceBytes +
    out_stats->m_QueryBytes + out_stats->m_BaseCapacity + out_stats->m_PayloadCapacity +
    (out_stats->m_BaseBlocks + out_stats->m_PayloadBlocks) * sizeof(DataBlock);
}

template <typename T>
static void Reserve(dmArray<T>& array, uint32_t count)
{
    if (count > array.Capacity())
    {
        const uint32_t max_capacity = UINT32_MAX / sizeof(T);
        assert(count <= max_capacity);
        uint32_t capacity = array.Capacity() < 16 ? 16 : array.Capacity();
        while (capacity < count)
        {
            capacity = capacity > max_capacity / 2 ? count : capacity * 2;
        }
        array.SetCapacity(capacity);
    }
}

template <typename T>
static bool Contains(const dmArray<T>& array, T value)
{
    for (uint32_t i = 0; i < array.Size(); ++i)
    {
        if (array[i] == value)
            return true;
    }
    return false;
}

// Finds an independently registered table; blob instances are managed by their handle.
static DataTable* FindTable(HDataStore store, uint64_t type)
{
    for (uint32_t i = 0; i < store->m_Tables.Size(); ++i)
    {
        if (!store->m_Tables[i]->m_Pool && store->m_Tables[i]->m_Type == type)
            return store->m_Tables[i];
    }
    return 0;
}

static uint32_t FindProperty(const DataTable* table, uint64_t property)
{
    for (uint32_t i = 0; i < table->m_PropertyCount; ++i)
    {
        if (GetPropertyMeta(table, i).m_Property == property)
            return i;
    }
    return UINT32_MAX;
}

static uint32_t FindMember(const DataTable* table, uint32_t first, uint32_t count, uint64_t property)
{
    for (uint32_t i = 0; i < count; ++i)
        if (GetPropertyMeta(table, first + i).m_Property == property)
            return first + i;
    return UINT32_MAX;
}

// Resolve names during table matching and retain the field metadata/byte offset.
// Dynamic containers cannot bind paths.
static bool ResolveQueryProperty(const DataTable* table, const DataQueryProperty& property, DataQueryBinding* binding)
{
    uint32_t index = FindProperty(table, property.m_Property);
    if (index == UINT32_MAX)
        return false;
    DataPropertyMeta meta = GetPropertyMeta(table, index);
    uint32_t         offset = meta.m_Offset;
    for (uint32_t i = 0; i < property.m_PathCount; ++i)
    {
        if (!meta.m_ChildIndex)
            return false;
        uint32_t child = FindMember(table, meta.m_ChildIndex, meta.m_ChildCount, property.m_Path[i]);
        if (child == UINT32_MAX)
            return false;
        meta = GetPropertyMeta(table, child);
        offset += meta.m_Offset;
    }
    if (meta.m_Type != property.m_Type)
        return false;
    meta.m_Offset = offset;
    binding->m_Meta = meta;
    return true;
}

static DataQueryBinding* AllocateQueryFields(HDataQuery query)
{
    if (query->m_FreeFields)
    {
        DataQueryBinding* fields = query->m_FreeFields;
        memcpy(&query->m_FreeFields, (const void*)fields, sizeof(query->m_FreeFields));
        return fields;
    }
    size_t size = query->m_Properties.Size() * sizeof(DataQueryBinding);
    return (DataQueryBinding*)AllocateData(&query->m_Bindings, size, query->m_Bindings ? 4096 : 256);
}

static void ReleaseQueryFields(HDataQuery query, DataQueryBinding* fields)
{
    if (!fields)
        return;
    // Reuse fixed-size binding sets after table removal. The query owns all blocks.
    memcpy((void*)fields, &query->m_FreeFields, sizeof(query->m_FreeFields));
    query->m_FreeFields = fields;
}

static void MatchTable(HDataQuery query, DataTable* table)
{
    for (uint32_t i = 0; i < query->m_Tags.Size(); ++i)
    {
        bool found = false;
        for (uint32_t j = 0; j < table->m_TagCount && !found; ++j)
            found = GetTableTag(table, j) == query->m_Tags[i];
        if (!found)
            return;
    }
    // Reject before allocating bindings. Layouts are immutable for a table's lifetime.
    for (uint32_t i = 0; i < query->m_Properties.Size(); ++i)
    {
        DataQueryBinding binding;
        if (!ResolveQueryProperty(table, query->m_Properties[i], &binding))
            return;
    }
    DataQueryTable match = { table, 0 };
    if (query->m_Properties.Size())
        match.m_Fields = AllocateQueryFields(query);
    for (uint32_t i = 0; i < query->m_Properties.Size(); ++i)
    {
        ResolveQueryProperty(table, query->m_Properties[i], &match.m_Fields[i]);
    }
    Reserve(query->m_Tables, query->m_Tables.Size() + 1);
    query->m_Tables.Push(match);
}

static bool MatchesOwner(HDataQuery query, DataOwnerId owner)
{
    return query->m_Owners.Empty() || Contains(query->m_Owners, owner);
}

static DataId MakeId(HDataStore store, uint32_t index)
{
    return ((uint64_t)store->m_Slots[index].m_Generation << 32) | index;
}

static DataSlot* FindSlot(HDataStore store, DataId id)
{
    uint32_t index = (uint32_t)id;
    if (index >= store->m_Slots.Size())
        return 0;
    DataSlot* slot = &store->m_Slots[index];
    return slot->m_Table && slot->m_Generation == (uint32_t)(id >> 32) ? slot : 0;
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
    }
}

static void ReserveSlots(HDataStore store, uint32_t count)
{
    uint32_t new_slots = count;
    for (uint32_t slot = store->m_FreeSlot; slot != INVALID_SLOT && new_slots; slot = store->m_Slots[slot].m_NextFree)
        --new_slots;
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
        DataSlot slot = {};
        slot.m_Generation = 1;
        store->m_Slots.Push(slot);
    }
    else
    {
        store->m_FreeSlot = store->m_Slots[index].m_NextFree;
    }
    DataSlot* slot = &store->m_Slots[index];
    slot->m_Table = table;
    slot->m_Row = table->m_Rows.Size();
    row.m_Slot = index;
    table->m_Rows.Push(row);
    return MakeId(store, index);
}

static void DeleteTable(HDataStore store, DataTable* table)
{
    for (uint32_t i = 0; i < table->m_Rows.Size(); ++i)
    {
        ReleaseSlot(store, table->m_Rows[i].m_Slot);
    }
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

// Measure the compact payload during the existing validation pass. A complete
// owned tree must fit 32-bit child offsets; batches may contain multiple trees.
static DataResult ValidateValue(const DataValue* value, uint32_t depth, size_t* out_size)
{
    uint32_t size = DataTypeSize(value->m_Type);
    if (size == UINT32_MAX)
        return DATA_RESULT_INVALID_ARGUMENT;
    *out_size = ((size_t)size + 7) & ~(size_t)7;
    if (value->m_Type == DATA_VALUE_TYPE_BOOLEAN)
        return value->m_Value.m_Boolean <= 1 ? DATA_RESULT_OK : DATA_RESULT_INVALID_ARGUMENT;
    if (value->m_Type == DATA_VALUE_TYPE_STRING)
    {
        if (!value->m_Value.m_String)
            return DATA_RESULT_INVALID_ARGUMENT;
        size_t length = strlen(value->m_Value.m_String);
        if (length > UINT32_MAX - 8)
            return DATA_RESULT_INVALID_ARGUMENT;
        *out_size = (length + 8) & ~(size_t)7;
        return DATA_RESULT_OK;
    }
    if (value->m_Type != DATA_VALUE_TYPE_STRUCT && value->m_Type != DATA_VALUE_TYPE_LIST)
        return DATA_RESULT_OK;
    bool             named = value->m_Type == DATA_VALUE_TYPE_STRUCT;
    uint32_t         count = named ? value->m_Value.m_Struct.m_Count : value->m_Value.m_List.m_Count;
    const uint8_t*   buffer = named ? value->m_Value.m_Struct.m_Buffer : value->m_Value.m_List.m_Buffer;
    const DataValue* values = named ? value->m_Value.m_Struct.m_Values : value->m_Value.m_List.m_Values;
    if (depth == DATA_MAX_NESTING || count > UINT32_MAX / sizeof(DataValue) ||
        (!buffer && count && (!values || (named && !value->m_Value.m_Struct.m_Names))))
        return DATA_RESULT_INVALID_ARGUMENT;
    uint64_t total = 8 + (uint64_t)count * (named ? 16 : 8);
    for (uint32_t i = 0; i < count; ++i)
    {
        DataValue  child = GetChildValue(value, i);
        size_t     child_size;
        DataResult result = ValidateValue(&child, depth + 1, &child_size);
        if (result != DATA_RESULT_OK)
            return result;
        total += child_size;
        if (total > UINT32_MAX)
            return DATA_RESULT_INVALID_ARGUMENT;
        if (named)
        {
            uint64_t name = GetChildName(&value->m_Value.m_Struct, i);
            for (uint32_t j = 0; j < i; ++j)
                if (GetChildName(&value->m_Value.m_Struct, j) == name)
                    return DATA_RESULT_ALREADY_EXISTS;
        }
    }
    *out_size = (size_t)total;
    return DATA_RESULT_OK;
}

static void DeleteBlobPool(DataBlobPool* pool);

HDataStore  DataCreateStore(void)
{
    HDataStore store = new DataStore;
    store->m_FreeSlot = INVALID_SLOT;
    store->m_LockCount = 0;
    store->m_Pools = 0;
    return store;
}

DataResult DataDestroyStore(HDataStore store)
{
    if (store->m_LockCount)
        return DATA_RESULT_LOCKED;
    assert(store->m_Queries.Empty());
    while (store->m_Pools)
        DeleteBlobPool(store->m_Pools);
    for (uint32_t i = 0; i < store->m_Tables.Size(); ++i)
        DeleteTable(store, store->m_Tables[i]);
    delete store;
    return DATA_RESULT_OK;
}

void DataStoreLock(HDataStore store)
{
    assert(store->m_LockCount != UINT32_MAX);
    ++store->m_LockCount;
}

void DataStoreUnlock(HDataStore store)
{
    assert(store->m_LockCount);
    --store->m_LockCount;
}

static uint32_t FieldSize(const DataPropertyDesc& field)
{
    return field.m_Struct ? field.m_Struct->m_Size : DataTypeSize(field.m_Type);
}

static DataResult ValidateLayout(const DataPropertyDesc* fields, uint32_t count, uint32_t size, uint32_t depth, uint64_t* metadata_count, uint32_t* out_alignment)
{
    if ((count && !fields) || count > UINT32_MAX / sizeof(DataPropertyMeta) || depth > DATA_MAX_NESTING)
        return DATA_RESULT_INVALID_ARGUMENT;
    *metadata_count += count;
    if (*metadata_count > UINT32_MAX / DATA_PROPERTY_META_SIZE)
        return DATA_RESULT_INVALID_ARGUMENT;
    uint32_t alignment = 1;
    for (uint32_t i = 0; i < count; ++i)
    {
        const DataPropertyDesc& field = fields[i];
        uint32_t                field_size = FieldSize(field);
        if (field_size == UINT32_MAX || field.m_Offset > size || field_size > size - field.m_Offset ||
            (field.m_Struct && field.m_Type != DATA_VALUE_TYPE_STRUCT))
            return DATA_RESULT_INVALID_ARGUMENT;
        for (uint32_t j = 0; j < i; ++j)
        {
            if (fields[j].m_Property == field.m_Property)
                return DATA_RESULT_ALREADY_EXISTS;
            uint32_t other_size = FieldSize(fields[j]);
            if (field_size && other_size && field.m_Offset < fields[j].m_Offset + other_size && fields[j].m_Offset < field.m_Offset + field_size)
                return DATA_RESULT_INVALID_ARGUMENT;
        }
        uint32_t field_alignment = DataTypeAlignment(field.m_Type);
        if (field.m_Struct)
        {
            DataResult result = ValidateLayout(field.m_Struct->m_Properties, field.m_Struct->m_PropertyCount, field_size, depth + 1, metadata_count, &field_alignment);
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

static void CompileLayout(dmArray<DataPropertyMeta>& metadata, const DataPropertyDesc* fields, uint32_t count, uint32_t first)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        const DataPropertyDesc& field = fields[i];
        DataPropertyMeta        meta = { field.m_Property, field.m_Type, field.m_Offset, 0, 0, FieldSize(field) };
        if (field.m_Struct)
        {
            meta.m_ChildIndex = metadata.Size();
            meta.m_ChildCount = field.m_Struct->m_PropertyCount;
            metadata.SetSize(metadata.Size() + meta.m_ChildCount);
            CompileLayout(metadata, field.m_Struct->m_Properties, meta.m_ChildCount, meta.m_ChildIndex);
        }
        metadata[first + i] = meta;
    }
}

static DataTable* NewTable(HDataStore store, const DataTableDesc* desc, DataBlobPool* pool, const uint8_t* packed_table = 0, DataTable* storage = 0);

DataResult        DataRegisterTable(HDataStore store, const DataTableDesc* desc)
{
    if (store->m_LockCount)
        return DATA_RESULT_LOCKED;
    if ((desc->m_TagCount && !desc->m_Tags) || desc->m_TagCount > UINT32_MAX / sizeof(uint64_t))
        return DATA_RESULT_INVALID_ARGUMENT;
    uint64_t   metadata_count = 0;
    uint32_t   alignment;
    DataResult result = ValidateLayout(desc->m_Properties, desc->m_PropertyCount, desc->m_RowStride, 0, &metadata_count, &alignment);
    if (result != DATA_RESULT_OK)
        return result;
    if (FindTable(store, desc->m_Type))
        return DATA_RESULT_ALREADY_EXISTS;
    NewTable(store, desc, 0);
    return DATA_RESULT_OK;
}

static DataTable* NewTable(HDataStore store, const DataTableDesc* desc, DataBlobPool* pool, const uint8_t* packed_table, DataTable* storage)
{
    DataTable* table = storage;
    if (!table)
    {
        uint8_t* allocation = new uint8_t[sizeof(DataTable) + sizeof(DataOwnedTable)];
        table = new (allocation) DataTable;
        table->m_Owned = new (table + 1) DataOwnedTable;
        table->m_Owned->m_BaseCount = 0;
        table->m_Owned->m_BaseValues = 0;
    }
    table->m_Type = desc->m_Type;
    table->m_Blob = pool ? pool->m_Blob->m_Data : 0;
    if (pool)
    {
        table->m_Offsets.m_Table = (uint32_t)(packed_table - table->m_Blob);
        table->m_Offsets.m_Rows = 0;
    }
    table->m_TagCount = desc->m_TagCount;
    table->m_PropertyCount = desc->m_PropertyCount;
    table->m_MetadataCount = packed_table ? (uint32_t)ReadDataInteger(packed_table + 24, 4) : desc->m_PropertyCount;
    table->m_RowStride = desc->m_RowStride;
    table->m_Pool = pool;
    table->m_Payloads = 0;
    if (!packed_table && desc->m_TagCount)
    {
        table->m_Owned->m_Tags.SetCapacity(desc->m_TagCount);
        table->m_Owned->m_Tags.PushArray(desc->m_Tags, desc->m_TagCount);
    }
    if (!packed_table && desc->m_PropertyCount)
    {
        uint64_t count = 0;
        uint32_t alignment;
        ValidateLayout(desc->m_Properties, desc->m_PropertyCount, desc->m_RowStride, 0, &count, &alignment);
        table->m_MetadataCount = (uint32_t)count;
        table->m_Owned->m_Properties.SetCapacity(table->m_MetadataCount);
        table->m_Owned->m_Properties.SetSize(desc->m_PropertyCount);
        CompileLayout(table->m_Owned->m_Properties, desc->m_Properties, desc->m_PropertyCount, 0);
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
        dmArray<DataQueryTable>& tables = store->m_Queries[i]->m_Tables;
        for (uint32_t j = 0; j < tables.Size(); ++j)
        {
            if (tables[j].m_Table == table)
            {
                ReleaseQueryFields(store->m_Queries[i], tables[j].m_Fields);
                tables.EraseSwap(j);
                break;
            }
        }
    }
    store->m_Tables.Back()->m_StoreIndex = table->m_StoreIndex;
    store->m_Tables.EraseSwap(table->m_StoreIndex);
    DeleteTable(store, table);
}

DataResult DataUnregisterTable(HDataStore store, uint64_t type)
{
    if (store->m_LockCount)
        return DATA_RESULT_LOCKED;
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
        const uint8_t* data = file + ReadDataInteger(file + 16 + t * 4, 4);
        page_offset += AlignBlobStorage(ReadDataInteger(data + 16, 4) * ReadDataInteger(data + 20, 4));
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
        const uint8_t* data = file + ReadDataInteger(file + 16 + t * 4, 4);
        DataTableDesc  desc = {};
        desc.m_Type = ReadDataInteger(data, 8);
        desc.m_TagCount = (uint32_t)ReadDataInteger(data + 8, 4);
        desc.m_PropertyCount = (uint32_t)ReadDataInteger(data + 12, 4);
        desc.m_RowStride = (uint32_t)ReadDataInteger(data + 20, 4);
        uint32_t       count = (uint32_t)ReadDataInteger(data + 16, 4);
        DataTable*     table = NewTable(store, &desc, pool, data, new (GetPoolTable(pool, t)) DataTable);
        const uint8_t* components = data + DATA_TABLE_HEADER_SIZE + desc.m_TagCount * 8 + table->m_MetadataCount * DATA_PROPERTY_META_SIZE;
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
        uint64_t         rows = table->m_Rows.Size() + ReadDataInteger(table->m_Blob + table->m_Offsets.m_Table + 16, 4);
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

DataResult DataAddBlob(HDataStore store, HDataBlob blob, DataOwnerId owner, HDataBlobInstance* out_instance)
{
    if (store->m_LockCount)
        return DATA_RESULT_LOCKED;
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
        uint32_t   count = (uint32_t)ReadDataInteger(table->m_Blob + table->m_Offsets.m_Table + 16, 4);
        uint32_t   start = table->m_Rows.Size();
        ReserveTableArray(table, table->m_Rows, start + count);
        ReserveTableArray(table, table->m_Values, (start + count) * table->m_RowStride);
        table->m_Values.SetSize((start + count) * table->m_RowStride);
        InitializeBlobValues(table, start, count);
        for (uint32_t r = 0; r < count; ++r)
        {
            DataRow row = {};
            row.m_Owner = owner;
            row.m_InstanceIndex = instance->m_Index;
            row.m_BaseIndex = r;
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
    if (store->m_LockCount)
        return DATA_RESULT_LOCKED;
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

static DataResult ValidateStoredValue(const DataTable* table, const DataPropertyMeta& meta, const DataValue* value, uint32_t depth, size_t* payload_size)
{
    if (value->m_Type != meta.m_Type || depth > DATA_MAX_NESTING)
        return DATA_RESULT_INVALID_ARGUMENT;
    if (!meta.m_ChildIndex)
    {
        size_t     size;
        DataResult result = ValidateValue(value, depth, &size);
        if (result != DATA_RESULT_OK)
            return result;
        if (IsReference(value->m_Type))
        {
            if (size > SIZE_MAX - sizeof(DataBlock) - *payload_size)
                return DATA_RESULT_INVALID_ARGUMENT;
            *payload_size += size;
        }
        return DATA_RESULT_OK;
    }
    const DataStruct* object = &value->m_Value.m_Struct;
    if (object->m_Count != meta.m_ChildCount || (!object->m_Buffer && object->m_Count && (!object->m_Names || !object->m_Values)))
        return DATA_RESULT_INVALID_ARGUMENT;
    for (uint32_t i = 0; i < meta.m_ChildCount; ++i)
    {
        DataPropertyMeta field = GetPropertyMeta(table, meta.m_ChildIndex + i);
        DataValue        child;
        if (DataGetStructProperty(object, field.m_Property, &child) != DATA_RESULT_OK)
            return DATA_RESULT_INVALID_ARGUMENT;
        DataResult result = ValidateStoredValue(table, field, &child, depth + 1, payload_size);
        if (result != DATA_RESULT_OK)
            return result;
    }
    return DATA_RESULT_OK;
}

static void StoreStoredValue(const DataTable* table, const DataPropertyMeta& meta, DataBlock** blocks, uint8_t* bytes, const DataValue* value)
{
    if (!meta.m_ChildIndex)
    {
        StoreValue(blocks, bytes, value);
        return;
    }
    for (uint32_t i = 0; i < meta.m_ChildCount; ++i)
    {
        DataPropertyMeta field = GetPropertyMeta(table, meta.m_ChildIndex + i);
        DataValue        child;
        DataGetStructProperty(&value->m_Value.m_Struct, field.m_Property, &child);
        StoreStoredValue(table, field, blocks, field.m_Size ? bytes + field.m_Offset : 0, &child);
    }
}

DataResult DataAddRow(HDataStore store, uint64_t type, DataOwnerId owner, const DataValue* values, uint32_t value_count, DataId* out_id, uint64_t component_id)
{
    DataRowDesc row = { owner, values, value_count, component_id };
    return DataAddRows(store, type, &row, 1, out_id);
}

DataResult DataAddRows(HDataStore store, uint64_t type, const DataRowDesc* rows, uint32_t count, DataId* out_ids)
{
    if (store->m_LockCount)
        return DATA_RESULT_LOCKED;
    DataTable* table = FindTable(store, type);
    if (!table)
        return DATA_RESULT_NOT_FOUND;
    if (!count)
        return DATA_RESULT_OK;
    if (!rows || !out_ids || count > UINT32_MAX / sizeof(DataSlot) - store->m_Slots.Size() ||
        count > UINT32_MAX / sizeof(DataRow) - table->m_Rows.Size() || count > UINT32_MAX - table->m_Owned->m_BaseCount ||
        (table->m_RowStride && (uint64_t)(table->m_Owned->m_BaseCount + count) * table->m_RowStride > UINT32_MAX))
        return DATA_RESULT_INVALID_ARGUMENT;
    size_t payload_size = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (rows[i].m_ValueCount != table->m_PropertyCount || (rows[i].m_ValueCount && !rows[i].m_Values))
            return DATA_RESULT_INVALID_ARGUMENT;
        for (uint32_t j = 0; j < table->m_PropertyCount; ++j)
        {
            const DataValue* value = &rows[i].m_Values[j];
            DataResult       result = ValidateStoredValue(table, table->m_Owned->m_Properties[j], value, 0, &payload_size);
            if (result != DATA_RESULT_OK)
                return result;
        }
    }
    if (payload_size)
        ReserveData(&table->m_Owned->m_BaseValues, payload_size);
    ReserveRows(store, table, count);
    uint32_t size = (table->m_Owned->m_BaseCount + count) * table->m_RowStride;
    Reserve(table->m_Owned->m_BaseRows, size);
    table->m_Owned->m_BaseRows.SetSize(size);
    table->m_Values.SetSize((table->m_Rows.Size() + count) * table->m_RowStride);
    for (uint32_t i = 0; i < count; ++i)
    {
        DataRow row = {};
        row.m_Owner = rows[i].m_Owner;
        row.m_ComponentId = rows[i].m_ComponentId;
        row.m_BaseIndex = table->m_Owned->m_BaseCount++;
        if (table->m_RowStride)
        {
            uint8_t* data = table->m_Owned->m_BaseRows.Begin() + (size_t)row.m_BaseIndex * table->m_RowStride;
            memset(data, 0, table->m_RowStride);
            for (uint32_t j = 0; j < table->m_PropertyCount; ++j)
            {
                const DataPropertyMeta& meta = table->m_Owned->m_Properties[j];
                StoreStoredValue(table, meta, &table->m_Owned->m_BaseValues, data + meta.m_Offset, &rows[i].m_Values[j]);
            }
            memcpy(table->m_Values.Begin() + (size_t)table->m_Rows.Size() * table->m_RowStride, data, table->m_RowStride);
        }
        out_ids[i] = AddSlot(store, table, row);
    }
    return DATA_RESULT_OK;
}

DataResult DataRemoveRow(HDataStore store, DataId id)
{
    if (store->m_LockCount)
        return DATA_RESULT_LOCKED;
    DataSlot* slot = FindSlot(store, id);
    if (!slot)
        return DATA_RESULT_NOT_FOUND;
    DataTable* table = slot->m_Table;
    uint32_t   row = slot->m_Row;
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
    table->m_Rows.EraseSwap(row);
    ReleaseSlot(store, (uint32_t)id);
    return DATA_RESULT_OK;
}

static DataResult ReadProperty(DataTable* table, uint32_t row, uint64_t property, DataValue* out_value)
{
    uint32_t index = FindProperty(table, property);
    if (index == UINT32_MAX)
        return DATA_RESULT_NOT_FOUND;
    *out_value = ReadRowValue(table, &table->m_Rows[row], index);
    return DATA_RESULT_OK;
}

DataResult DataGetProperty(HDataStore store, DataId id, uint64_t property, DataValue* out_value)
{
    DataSlot* slot = FindSlot(store, id);
    return slot ? ReadProperty(slot->m_Table, slot->m_Row, property, out_value) : DATA_RESULT_NOT_FOUND;
}

static DataResult SetRowProperty(DataTable* table, DataRow* row, const DataPropertyMeta& meta, const DataValue* value)
{
    size_t     payload_size = 0;
    DataResult result = ValidateStoredValue(table, meta, value, 0, &payload_size);
    if (result != DATA_RESULT_OK)
        return result;
    DataBlock** payloads = &table->m_Payloads;
    if (payload_size)
    {
        if (table->m_Pool)
            payloads = &GetPoolInstance(table->m_Pool, row->m_InstanceIndex)->m_Payloads;
        ReserveData(payloads, payload_size);
    }
    uint8_t* bytes = meta.m_Size ? GetPropertyBytes(table, row, meta.m_Offset) : 0;
    StoreStoredValue(table, meta, payloads, bytes, value);
    return DATA_RESULT_OK;
}

DataResult DataSetProperty(HDataStore store, DataId id, uint64_t property, const DataValue* value)
{
    DataSlot* slot = FindSlot(store, id);
    if (!slot)
        return DATA_RESULT_NOT_FOUND;
    DataTable* table = slot->m_Table;
    uint32_t   index = FindProperty(table, property);
    if (index == UINT32_MAX)
        return DATA_RESULT_NOT_FOUND;
    return SetRowProperty(table, &table->m_Rows[slot->m_Row], GetPropertyMeta(table, index), value);
}

DataResult DataResetProperty(HDataStore store, DataId id, uint64_t property)
{
    DataSlot* slot = FindSlot(store, id);
    if (!slot)
        return DATA_RESULT_NOT_FOUND;
    DataTable* table = slot->m_Table;
    uint32_t   index = FindProperty(table, property);
    if (index == UINT32_MAX)
        return DATA_RESULT_NOT_FOUND;
    DataRow*         row = &table->m_Rows[slot->m_Row];
    DataPropertyMeta meta = GetPropertyMeta(table, index);
    if (meta.m_Size)
    {
        uint8_t* bytes = GetPropertyBytes(table, row, meta.m_Offset);
        memcpy(bytes, GetDefaultRow(table, row) + meta.m_Offset, meta.m_Size);
        if (table->m_Pool)
            TagBlobReferences(table, meta, bytes, 1);
    }
    return DATA_RESULT_OK;
}

DataResult DataResetRow(HDataStore store, DataId id)
{
    DataSlot* slot = FindSlot(store, id);
    if (!slot)
        return DATA_RESULT_NOT_FOUND;
    DataTable* table = slot->m_Table;
    DataRow*   row = &table->m_Rows[slot->m_Row];
    ResetRow(table, row);
    return DATA_RESULT_OK;
}

static void ResetTable(DataTable* table)
{
    for (uint32_t i = 0; i < table->m_Rows.Size(); ++i)
        ResetRow(table, &table->m_Rows[i]);
    DeleteBlocks(&table->m_Payloads);
}

DataResult DataResetTable(HDataStore store, uint64_t type)
{
    DataTable* table = FindTable(store, type);
    if (!table)
        return DATA_RESULT_NOT_FOUND;
    ResetTable(table);
    return DATA_RESULT_OK;
}

void DataResetBlob(HDataBlobInstance instance)
{
    DataBlobPool*   pool = instance->m_Pool;
    const uint32_t* slots = GetInstanceSlots(instance);
    for (uint32_t i = 0; i < pool->m_Blob->m_RowCount; ++i)
    {
        if (slots[i] == INVALID_SLOT)
            continue;
        const DataSlot& slot = pool->m_Store->m_Slots[slots[i]];
        ResetRow(slot.m_Table, &slot.m_Table->m_Rows[slot.m_Row]);
    }
    DeleteBlocks(&instance->m_Payloads);
}

uint64_t GetRowComponentId(const DataTable* table, const DataRow* row)
{
    if (!table->m_Pool)
        return row->m_ComponentId;
    const uint8_t* data = table->m_Blob + table->m_Offsets.m_Table;
    const uint8_t* components = data + DATA_TABLE_HEADER_SIZE + (size_t)table->m_TagCount * 8 + (size_t)table->m_MetadataCount * DATA_PROPERTY_META_SIZE;
    return ReadDataInteger(components + (size_t)row->m_BaseIndex * 8, 8);
}

uint64_t DataGetComponentId(HDataStore store, DataId id)
{
    DataSlot* slot = FindSlot(store, id);
    return slot ? GetRowComponentId(slot->m_Table, &slot->m_Table->m_Rows[slot->m_Row]) : 0;
}

DataResult DataCreateQuery(HDataStore store, const DataQueryDesc* desc, HDataQuery* out_query)
{
    if ((desc->m_OwnerIdCount && !desc->m_OwnerIds) || (desc->m_AllTagCount && !desc->m_AllTags) ||
        desc->m_OwnerIdCount > UINT32_MAX / sizeof(DataOwnerId) || desc->m_AllTagCount > UINT32_MAX / sizeof(uint64_t) ||
        (desc->m_PropertyCount && !desc->m_Properties) || desc->m_PropertyCount > UINT32_MAX / sizeof(DataQueryBinding))
        return DATA_RESULT_INVALID_ARGUMENT;
    uint64_t path_count = 0;
    for (uint32_t i = 0; i < desc->m_PropertyCount; ++i)
    {
        const DataQueryProperty& property = desc->m_Properties[i];
        if ((uint32_t)property.m_Type > DATA_VALUE_TYPE_MATRIX4 || property.m_PathCount > DATA_MAX_NESTING ||
            (property.m_PathCount && !property.m_Path))
            return DATA_RESULT_INVALID_ARGUMENT;
        path_count += property.m_PathCount;
        if (path_count > UINT32_MAX / sizeof(uint64_t))
            return DATA_RESULT_INVALID_ARGUMENT;
    }
    HDataQuery query = new DataQuery;
    query->m_Store = store;
    query->m_Bindings = 0;
    query->m_FreeFields = 0;
    if (desc->m_OwnerIdCount)
    {
        query->m_Owners.SetCapacity(desc->m_OwnerIdCount);
        query->m_Owners.PushArray(desc->m_OwnerIds, desc->m_OwnerIdCount);
    }
    if (desc->m_AllTagCount)
    {
        query->m_Tags.SetCapacity(desc->m_AllTagCount);
        query->m_Tags.PushArray(desc->m_AllTags, desc->m_AllTagCount);
    }
    if (desc->m_PropertyCount)
    {
        query->m_Properties.SetCapacity(desc->m_PropertyCount);
        query->m_Properties.PushArray(desc->m_Properties, desc->m_PropertyCount);
        query->m_Paths.SetCapacity((uint32_t)path_count);
        for (uint32_t i = 0; i < desc->m_PropertyCount; ++i)
        {
            DataQueryProperty& property = query->m_Properties[i];
            if (property.m_PathCount)
            {
                uint32_t first = query->m_Paths.Size();
                query->m_Paths.PushArray(property.m_Path, property.m_PathCount);
                property.m_Path = query->m_Paths.Begin() + first;
            }
            else
                property.m_Path = 0;
        }
    }
    query->m_Tables.SetCapacity(store->m_Tables.Size());
    for (uint32_t i = 0; i < store->m_Tables.Size(); ++i)
    {
        MatchTable(query, store->m_Tables[i]);
    }
    Reserve(store->m_Queries, store->m_Queries.Size() + 1);
    store->m_Queries.Push(query);
    *out_query = query;
    return DATA_RESULT_OK;
}

void DataDestroyQuery(HDataQuery query)
{
    dmArray<HDataQuery>& queries = query->m_Store->m_Queries;
    for (uint32_t i = 0; i < queries.Size(); ++i)
    {
        if (queries[i] == query)
        {
            queries.EraseSwap(i);
            break;
        }
    }
    DeleteBlocks(&query->m_Bindings);
    delete query;
}

DataIterator DataQueryIter(HDataQuery query)
{
    assert(query->m_Store->m_LockCount);
    DataIterator iterator = {};
    iterator.m_Query = query;
    return iterator;
}

DataResult DataIterNext(DataIterator* iterator)
{
    HDataQuery query = iterator->m_Query;
    iterator->m_Count = 0;
    while (iterator->m_TableIndex < query->m_Tables.Size())
    {
        DataTable* table = query->m_Tables[iterator->m_TableIndex].m_Table;
        uint32_t   count = table->m_Rows.Size();
        uint32_t   row = iterator->m_NextRow;
        if (query->m_Owners.Empty())
        {
            iterator->m_StartRow = row;
            iterator->m_Count = count - row;
            iterator->m_NextRow = count;
        }
        else
        {
            while (row < count && !MatchesOwner(query, table->m_Rows[row].m_Owner))
                ++row;
            iterator->m_StartRow = row;
            while (row < count && MatchesOwner(query, table->m_Rows[row].m_Owner))
                ++row;
            iterator->m_Count = row - iterator->m_StartRow;
            iterator->m_NextRow = row;
        }
        if (iterator->m_Count)
        {
            // The caller holds a structural lock while these storage pointers are borrowed.
            // Writes and resets leave row identities, bindings and mutable row storage in place.
            iterator->m_Table = table;
            iterator->m_Rows = table->m_Rows.Begin() + iterator->m_StartRow;
            iterator->m_Fields = query->m_Tables[iterator->m_TableIndex].m_Fields;
            iterator->m_Values = table->m_RowStride ? table->m_Values.Begin() + (size_t)iterator->m_StartRow * table->m_RowStride : 0;
            iterator->m_RowStride = table->m_RowStride;
            iterator->m_FieldCount = iterator->m_Fields ? query->m_Properties.Size() : table->m_PropertyCount;
            return DATA_RESULT_OK;
        }
        ++iterator->m_TableIndex;
        iterator->m_NextRow = 0;
    }
    return DATA_RESULT_END;
}

uint32_t DataIterGetCount(const DataIterator* iterator)
{
    return iterator->m_Count;
}

uint64_t DataIterGetType(const DataIterator* iterator)
{
    return DataIterGetCount(iterator) ? ((const DataTable*)iterator->m_Table)->m_Type : 0;
}

DataId DataIterGetId(const DataIterator* iterator, uint32_t row)
{
    if (row >= DataIterGetCount(iterator))
        return 0;
    return MakeId(iterator->m_Query->m_Store, ((const DataRow*)iterator->m_Rows)[row].m_Slot);
}

DataOwnerId DataIterGetOwnerId(const DataIterator* iterator, uint32_t row)
{
    if (row >= DataIterGetCount(iterator))
        return 0;
    return ((const DataRow*)iterator->m_Rows)[row].m_Owner;
}

DataResult DataIterGetProperty(const DataIterator* iterator, uint32_t row, uint64_t property, DataValue* out_value)
{
    if (row >= iterator->m_Count)
        return DATA_RESULT_INVALID_ARGUMENT;
    return ReadProperty((DataTable*)iterator->m_Table, iterator->m_StartRow + row, property, out_value);
}

DataResult DataIterGetField(const DataIterator* iterator, uint32_t row, uint32_t field, DataValue* out_value)
{
    if (row >= iterator->m_Count || !iterator->m_Fields || field >= iterator->m_FieldCount)
        return DATA_RESULT_INVALID_ARGUMENT;
    const DataQueryBinding& binding = ((const DataQueryBinding*)iterator->m_Fields)[field];
    *out_value = ReadBoundValue((const DataTable*)iterator->m_Table, &((const DataRow*)iterator->m_Rows)[row], binding.m_Meta);
    return DATA_RESULT_OK;
}

DataResult DataIterSetField(const DataIterator* iterator, uint32_t row, uint32_t field, const DataValue* value)
{
    if (row >= iterator->m_Count || !iterator->m_Fields || field >= iterator->m_FieldCount)
        return DATA_RESULT_INVALID_ARGUMENT;
    const DataQueryBinding& binding = ((const DataQueryBinding*)iterator->m_Fields)[field];
    return SetRowProperty((DataTable*)iterator->m_Table, &((DataRow*)iterator->m_Rows)[row], binding.m_Meta, value);
}

// Specialize the payload copy at compile time; public typed reads have no runtime type dispatch.
template <DataValueType TYPE>
static DataResult CopyTypedValue(const DataTable* table, const uint8_t* bytes, void* out_value)
{
    switch (TYPE)
    {
        case DATA_VALUE_TYPE_NUMBER:
        {
            uint64_t bits = ReadDataInteger(bytes, 8);
            memcpy(out_value, &bits, 8);
            break;
        }
        case DATA_VALUE_TYPE_BOOLEAN:
            *(uint8_t*)out_value = *bytes;
            break;
        case DATA_VALUE_TYPE_STRING:
        {
            uint64_t    reference = ReadDataInteger(bytes, 8);
            const char* string = (reference & 1) ? (const char*)table->m_Blob + (reference >> 1) : (const char*)(uintptr_t)reference;
            *(const char**)out_value = string;
            break;
        }
        case DATA_VALUE_TYPE_VECTOR3:
            ReadDataFloats(out_value, bytes, 3);
            break;
        case DATA_VALUE_TYPE_VECTOR4:
            ReadDataFloats(out_value, bytes, 4);
            break;
        case DATA_VALUE_TYPE_MATRIX4:
            ReadDataFloats(out_value, bytes, 16);
            break;
        default:
            break;
    }
    return DATA_RESULT_OK;
}

template <DataValueType TYPE>
static DataResult ReadTypedValue(const DataTable* table, const DataRow* row, const DataPropertyMeta& meta, void* out_value)
{
    if (meta.m_Type != TYPE)
        return DATA_RESULT_INVALID_ARGUMENT;
    const uint8_t* bytes = GetPropertyBytes(table, row, meta.m_Offset);
    return CopyTypedValue<TYPE>(table, bytes, out_value);
}

template <DataValueType TYPE>
static DataResult GetTypedProperty(HDataStore store, DataId id, uint64_t property, void* out_value)
{
    DataSlot* slot = FindSlot(store, id);
    if (!slot)
        return DATA_RESULT_NOT_FOUND;
    DataTable* table = slot->m_Table;
    uint32_t   index = FindProperty(table, property);
    if (index == UINT32_MAX)
        return DATA_RESULT_NOT_FOUND;
    return ReadTypedValue<TYPE>(table, &table->m_Rows[slot->m_Row], GetPropertyMeta(table, index), out_value);
}

DataId DataRowIterGetId(const DataRowIterator* iterator)
{
    return DataIterGetId(iterator->m_Parent, iterator->m_Index);
}

DataOwnerId DataRowIterGetOwnerId(const DataRowIterator* iterator)
{
    return DataIterGetOwnerId(iterator->m_Parent, iterator->m_Index);
}

uint32_t DataQueryFindField(HDataQuery query, const DataQueryProperty* property)
{
#if DM_ENDIAN != DM_ENDIAN_LITTLE
    return UINT32_MAX;
#else
    if (property->m_PathCount > DATA_MAX_NESTING ||
        (property->m_PathCount && !property->m_Path))
        return UINT32_MAX;
    switch (property->m_Type)
    {
        case DATA_VALUE_TYPE_NUMBER:
        case DATA_VALUE_TYPE_BOOLEAN:
        case DATA_VALUE_TYPE_VECTOR3:
        case DATA_VALUE_TYPE_VECTOR4:
        case DATA_VALUE_TYPE_MATRIX4:
            break;
        default:
            return UINT32_MAX;
    }
    const dmArray<DataQueryProperty>& properties = query->m_Properties;
    for (uint32_t i = 0; i < properties.Size(); ++i)
    {
        const DataQueryProperty& candidate = properties[i];
        if (candidate.m_Property == property->m_Property && candidate.m_Type == property->m_Type &&
            candidate.m_PathCount == property->m_PathCount &&
            (!property->m_PathCount || !memcmp(candidate.m_Path, property->m_Path, property->m_PathCount * sizeof(uint64_t))))
            return i;
    }
    return UINT32_MAX;
#endif
}

// Field handles already identify a matched path/kind in an aligned batch.
// Getters trust that binding and the caller's locked iterator lifetime.
const void* DataGetFieldPointerInternal(const DataIterator* batch, uint32_t row_index, uint32_t field)
{
    const DataQueryBinding& binding = ((const DataQueryBinding*)batch->m_Fields)[field];
    return batch->m_Values + (size_t)row_index * batch->m_RowStride + binding.m_Meta.m_Offset;
}

void* DataGetFieldPointerMutInternal(const DataIterator* batch, uint32_t row_index, uint32_t field)
{
    const DataQueryBinding& binding = ((const DataQueryBinding*)batch->m_Fields)[field];
    return batch->m_Values + (size_t)row_index * batch->m_RowStride + binding.m_Meta.m_Offset;
}

// Cursors contain indices only. Resolve shared metadata when a caller accesses
// a value or explicitly asks for its kind/name, never when advancing a cursor.
static DataQueryBinding GetFieldBinding(const DataFieldIterator* iterator)
{
    const DataIterator* batch = iterator->m_Batch;
    if (batch->m_Fields)
        return ((const DataQueryBinding*)batch->m_Fields)[iterator->m_Index];
    DataQueryBinding binding = { GetPropertyMeta((const DataTable*)batch->m_Table, iterator->m_Index) };
    return binding;
}

DataValueType DataFieldIterGetType(const DataFieldIterator* iterator)
{
    return iterator->m_Index == UINT32_MAX ? DATA_VALUE_TYPE_NULL : GetFieldBinding(iterator).m_Meta.m_Type;
}

uint64_t DataFieldIterGetNameHash(const DataFieldIterator* iterator)
{
    return iterator->m_Index == UINT32_MAX ? 0 : GetFieldBinding(iterator).m_Meta.m_Property;
}

// Keep the metadata-reading fallback out of the bound getter's stack frame.
// Requested fields already have bindings and only need direct scalar loads.
template <DataValueType TYPE>
#if defined(_MSC_VER)
__declspec(noinline)
#else
__attribute__((noinline))
#endif
static DataResult
GetUnboundTypedField(const DataIterator* batch, const DataRow* row, uint32_t field, void* out_value)
{
    const DataTable* table = (const DataTable*)batch->m_Table;
    return ReadTypedValue<TYPE>(table, row, GetPropertyMeta(table, field), out_value);
}

template <DataValueType TYPE>
static DataResult GetTypedField(const DataIterator* batch, uint32_t row_index, uint32_t field_index, void* out_value)
{
    if (field_index == UINT32_MAX)
        return DATA_RESULT_INVALID_ARGUMENT;
    const DataRow* row = &((const DataRow*)batch->m_Rows)[row_index];
    if (!batch->m_Fields)
        return GetUnboundTypedField<TYPE>(batch, row, field_index, out_value);
    const DataQueryBinding& binding = ((const DataQueryBinding*)batch->m_Fields)[field_index];
    if (binding.m_Meta.m_Type != TYPE)
        return DATA_RESULT_INVALID_ARGUMENT;
    const uint8_t* bytes = batch->m_Values + (size_t)row_index * batch->m_RowStride + binding.m_Meta.m_Offset;
    return CopyTypedValue<TYPE>((const DataTable*)batch->m_Table, bytes, out_value);
}

static DataResult SetField(const DataFieldIterator* iterator, const DataValue* value)
{
    if (iterator->m_Index == UINT32_MAX)
        return DATA_RESULT_INVALID_ARGUMENT;
    const DataIterator* batch = iterator->m_Batch;
    DataQueryBinding    binding = GetFieldBinding(iterator);
    DataRow*            row = &((DataRow*)batch->m_Rows)[iterator->m_RowIndex];
    return SetRowProperty((DataTable*)batch->m_Table, row, binding.m_Meta, value);
}

DataResult DataFieldIterGetStructPropertyVector3(const DataFieldIterator* iterator, uint64_t property, DataVector3* out_value)
{
    if (iterator->m_Index == UINT32_MAX)
        return DATA_RESULT_INVALID_ARGUMENT;
    DataQueryBinding binding = GetFieldBinding(iterator);
    DataPropertyMeta field = binding.m_Meta;
    if (field.m_Type != DATA_VALUE_TYPE_STRUCT)
        return DATA_RESULT_INVALID_ARGUMENT;
    const DataIterator* batch = iterator->m_Batch;
    const DataTable*    table = (const DataTable*)batch->m_Table;
    const DataRow*      data_row = &((const DataRow*)batch->m_Rows)[iterator->m_RowIndex];
    if (field.m_ChildIndex)
    {
        uint32_t child = FindMember(table, field.m_ChildIndex, field.m_ChildCount, property);
        if (child == UINT32_MAX)
            return DATA_RESULT_NOT_FOUND;
        DataPropertyMeta meta = GetPropertyMeta(table, child);
        meta.m_Offset += field.m_Offset;
        return ReadTypedValue<DATA_VALUE_TYPE_VECTOR3>(table, data_row, meta, out_value);
    }
    const uint8_t* bytes = GetPropertyBytes(table, data_row, field.m_Offset);
    uint64_t       reference = ReadDataInteger(bytes, 8);
    bool           packed = (reference & 1) != 0;
    const uint8_t* buffer = packed ? table->m_Blob : (const uint8_t*)(uintptr_t)reference;
    const uint8_t* object = packed ? buffer + (reference >> 1) : buffer;
    uint32_t       count = (uint32_t)ReadDataInteger(object + 4, 4);
    const uint8_t* names = object + 8;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (ReadDataInteger(names + i * 8, 8) != property)
            continue;
        const uint8_t* types = names + count * 8;
        if (ReadDataInteger(types + i * 4, 4) != DATA_VALUE_TYPE_VECTOR3)
            return DATA_RESULT_INVALID_ARGUMENT;
        uint32_t offset = (uint32_t)ReadDataInteger(types + count * 4 + i * 4, 4);
        ReadDataFloats(out_value->m_Values, buffer + offset, 3);
        return DATA_RESULT_OK;
    }
    return DATA_RESULT_NOT_FOUND;
}

DataResult DataGetPropertyNumber(HDataStore store, DataId id, uint64_t property, double* out_value)
{
    return GetTypedProperty<DATA_VALUE_TYPE_NUMBER>(store, id, property, out_value);
}

DataResult DataSetPropertyNumber(HDataStore store, DataId id, uint64_t property, double value)
{
    DataValue input = {};
    input.m_Type = DATA_VALUE_TYPE_NUMBER;
    input.m_Value.m_Number = value;
    return DataSetProperty(store, id, property, &input);
}

DataResult DataGetPropertyBoolean(HDataStore store, DataId id, uint64_t property, uint8_t* out_value)
{
    return GetTypedProperty<DATA_VALUE_TYPE_BOOLEAN>(store, id, property, out_value);
}

DataResult DataSetPropertyBoolean(HDataStore store, DataId id, uint64_t property, uint8_t value)
{
    DataValue input = {};
    input.m_Type = DATA_VALUE_TYPE_BOOLEAN;
    input.m_Value.m_Boolean = value;
    return DataSetProperty(store, id, property, &input);
}

DataResult DataGetPropertyString(HDataStore store, DataId id, uint64_t property, const char** out_value)
{
    return GetTypedProperty<DATA_VALUE_TYPE_STRING>(store, id, property, out_value);
}

DataResult DataSetPropertyString(HDataStore store, DataId id, uint64_t property, const char* value)
{
    DataValue input = {};
    input.m_Type = DATA_VALUE_TYPE_STRING;
    input.m_Value.m_String = value;
    return DataSetProperty(store, id, property, &input);
}

DataResult DataGetPropertyVector3(HDataStore store, DataId id, uint64_t property, DataVector3* out_value)
{
    return GetTypedProperty<DATA_VALUE_TYPE_VECTOR3>(store, id, property, out_value->m_Values);
}

DataResult DataSetPropertyVector3(HDataStore store, DataId id, uint64_t property, const DataVector3* value)
{
    DataValue input = {};
    input.m_Type = DATA_VALUE_TYPE_VECTOR3;
    memcpy(input.m_Value.m_Vector3, value->m_Values, sizeof(value->m_Values));
    return DataSetProperty(store, id, property, &input);
}

DataResult DataGetPropertyVector4(HDataStore store, DataId id, uint64_t property, DataVector4* out_value)
{
    return GetTypedProperty<DATA_VALUE_TYPE_VECTOR4>(store, id, property, out_value->m_Values);
}

DataResult DataSetPropertyVector4(HDataStore store, DataId id, uint64_t property, const DataVector4* value)
{
    DataValue input = {};
    input.m_Type = DATA_VALUE_TYPE_VECTOR4;
    memcpy(input.m_Value.m_Vector4, value->m_Values, sizeof(value->m_Values));
    return DataSetProperty(store, id, property, &input);
}

DataResult DataGetPropertyMatrix4(HDataStore store, DataId id, uint64_t property, DataMatrix4* out_value)
{
    return GetTypedProperty<DATA_VALUE_TYPE_MATRIX4>(store, id, property, out_value->m_Values);
}

DataResult DataSetPropertyMatrix4(HDataStore store, DataId id, uint64_t property, const DataMatrix4* value)
{
    DataValue input = {};
    input.m_Type = DATA_VALUE_TYPE_MATRIX4;
    memcpy(input.m_Value.m_Matrix4, value->m_Values, sizeof(value->m_Values));
    return DataSetProperty(store, id, property, &input);
}

DataResult DataGetFieldNumberInternal(const DataIterator* batch, uint32_t row, uint32_t field, double* out_value)
{
    return GetTypedField<DATA_VALUE_TYPE_NUMBER>(batch, row, field, out_value);
}

DataResult DataFieldIterSetNumber(const DataFieldIterator* iterator, double value)
{
    DataValue input = {};
    input.m_Type = DATA_VALUE_TYPE_NUMBER;
    input.m_Value.m_Number = value;
    return SetField(iterator, &input);
}

DataResult DataGetFieldBooleanInternal(const DataIterator* batch, uint32_t row, uint32_t field, uint8_t* out_value)
{
    return GetTypedField<DATA_VALUE_TYPE_BOOLEAN>(batch, row, field, out_value);
}

DataResult DataFieldIterSetBoolean(const DataFieldIterator* iterator, uint8_t value)
{
    DataValue input = {};
    input.m_Type = DATA_VALUE_TYPE_BOOLEAN;
    input.m_Value.m_Boolean = value;
    return SetField(iterator, &input);
}

DataResult DataGetFieldStringInternal(const DataIterator* batch, uint32_t row, uint32_t field, const char** out_value)
{
    return GetTypedField<DATA_VALUE_TYPE_STRING>(batch, row, field, out_value);
}

DataResult DataFieldIterSetString(const DataFieldIterator* iterator, const char* value)
{
    DataValue input = {};
    input.m_Type = DATA_VALUE_TYPE_STRING;
    input.m_Value.m_String = value;
    return SetField(iterator, &input);
}

DataResult DataGetFieldVector3Internal(const DataIterator* batch, uint32_t row, uint32_t field, DataVector3* out_value)
{
    return GetTypedField<DATA_VALUE_TYPE_VECTOR3>(batch, row, field, out_value->m_Values);
}

DataResult DataFieldIterSetVector3(const DataFieldIterator* iterator, const DataVector3* value)
{
    DataValue input = {};
    input.m_Type = DATA_VALUE_TYPE_VECTOR3;
    memcpy(input.m_Value.m_Vector3, value->m_Values, sizeof(value->m_Values));
    return SetField(iterator, &input);
}

DataResult DataGetFieldVector4Internal(const DataIterator* batch, uint32_t row, uint32_t field, DataVector4* out_value)
{
    return GetTypedField<DATA_VALUE_TYPE_VECTOR4>(batch, row, field, out_value->m_Values);
}

DataResult DataFieldIterSetVector4(const DataFieldIterator* iterator, const DataVector4* value)
{
    DataValue input = {};
    input.m_Type = DATA_VALUE_TYPE_VECTOR4;
    memcpy(input.m_Value.m_Vector4, value->m_Values, sizeof(value->m_Values));
    return SetField(iterator, &input);
}

DataResult DataGetFieldMatrix4Internal(const DataIterator* batch, uint32_t row, uint32_t field, DataMatrix4* out_value)
{
    return GetTypedField<DATA_VALUE_TYPE_MATRIX4>(batch, row, field, out_value->m_Values);
}

DataResult DataFieldIterSetMatrix4(const DataFieldIterator* iterator, const DataMatrix4* value)
{
    DataValue input = {};
    input.m_Type = DATA_VALUE_TYPE_MATRIX4;
    memcpy(input.m_Value.m_Matrix4, value->m_Values, sizeof(value->m_Values));
    return SetField(iterator, &input);
}
