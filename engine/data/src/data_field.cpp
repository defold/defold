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
#include <stddef.h>
#include <string.h>
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
DM_STATIC_ASSERT(sizeof(DataReference) == 8, Invalid_reference_input_layout);

// Row storage contains payload bytes, not DataValue objects. DataFieldMeta
// supplies each field's fixed type and byte offset once per table. Numeric
// payloads use little-endian bytes with C-compatible member/row alignment.
// DataValue is a temporary tagged input/result for internal generic operations. Its tag selects
// the union member and validates writes; typed getters do not construct one.
// Declared fixed structs are inline bytes with shared child metadata. Dynamic
// structs share child-table metadata; lists retain packed child kind/offset arrays.

// Size of a field's slot in a byte row. Eight-byte reference slots hold a blob
// offset, an owned string/list pointer, or owned struct table/row indices.
uint32_t DataTypeSize(DataValueType type)
{
    switch (type)
    {
        case DATA_TYPE_NULL:
            return 0;
        case DATA_TYPE_BOOLEAN:
            return 1;
        case DATA_TYPE_NUMBER:
        case DATA_TYPE_STRING:
        case DATA_TYPE_STRUCT:
        case DATA_TYPE_LIST:
            return 8;
        case DATA_TYPE_VECTOR3:
            return 12;
        case DATA_TYPE_VECTOR4:
            return 16;
        case DATA_TYPE_MATRIX4:
            return 64;
        default:
            return UINT32_MAX;
    }
}

uint32_t DataTypeAlignment(DataValueType type)
{
    switch (type)
    {
        case DATA_TYPE_NUMBER:
        case DATA_TYPE_STRING:
        case DATA_TYPE_STRUCT:
        case DATA_TYPE_LIST:
            return 8;
        case DATA_TYPE_VECTOR3:
        case DATA_TYPE_VECTOR4:
        case DATA_TYPE_MATRIX4:
            return 4;
        default:
            return 1;
    }
}

// A packed container is a view. Reading it never allocates or expands its children.
// The type argument comes from table metadata or a container's child metadata;
// there is no type tag alongside the scalar payload at buffer + offset.
static DataValue ReadPackedValue(const uint8_t* buffer, DataValueType type, uint32_t offset)
{
    DataValue      value = { .m_Type = type };
    const uint8_t* data = buffer + offset;
    switch (type)
    {
        case DATA_TYPE_NUMBER:
        {
            uint64_t bits = ReadDataInteger(data, 8);
            memcpy(&value.m_Value.m_Number, &bits, 8);
            break;
        }
        case DATA_TYPE_BOOLEAN:
            value.m_Value.m_Boolean = *data;
            break;
        case DATA_TYPE_STRING:
            value.m_Value.m_String = (const char*)data;
            break;
        case DATA_TYPE_STRUCT:
            value.m_Value.m_Struct.m_Source = DATA_STRUCT_PACKED;
            value.m_Value.m_Struct.m_View.m_Buffer = buffer;
            value.m_Value.m_Struct.m_View.m_Offset = offset;
            value.m_Value.m_Struct.m_Count = (uint32_t)ReadDataInteger(data + offsetof(DataFileContainerHeader, m_Count), 4);
            break;
        case DATA_TYPE_LIST:
            value.m_Value.m_List.m_Buffer = buffer;
            value.m_Value.m_List.m_Offset = offset;
            value.m_Value.m_List.m_Count = (uint32_t)ReadDataInteger(data + offsetof(DataFileContainerHeader, m_Count), 4);
            break;
        case DATA_TYPE_VECTOR3:
            ReadDataFloats(value.m_Value.m_Vector3, data, 3);
            break;
        case DATA_TYPE_VECTOR4:
            ReadDataFloats(value.m_Value.m_Vector4, data, 4);
            break;
        case DATA_TYPE_MATRIX4:
            ReadDataFloats(value.m_Value.m_Matrix4, data, 16);
            break;
        default:
            break;
    }
    return value;
}

// File references are blob-relative offsets. In mutable rows, a low-bit tag
// distinguishes a shared blob offset (offset << 1 | 1) from an aligned arena
// pointer (low two bits zero). Owned struct rows use tag 2 and table/row indices.
// Only reference slots use this encoding; numeric access
// is a direct load. The tagged offset fits in 64 bits on 32-bit hosts too.
// Owned string/list payloads are eight-byte aligned. Packed containers retain
// root-relative offsets within their blob or owned payload, without pointer fixups.
static DataValue ReadStoredValue(const DataTable* table, const DataFieldMeta& meta, const uint8_t* bytes)
{
    if (meta.m_ChildIndex)
    {
        DataStruct object = {
            .m_Count = meta.m_ChildCount,
            .m_Source = DATA_STRUCT_INLINE,
            .m_View = { .m_Buffer = bytes ? bytes - meta.m_Offset : 0, .m_Offset = meta.m_ChildIndex, .m_Table = table }
        };
        DataValue value = {
            .m_Type = DATA_TYPE_STRUCT,
            .m_Value = { .m_Struct = object }
        };
        return value;
    }
    if (meta.m_Type == DATA_TYPE_NULL)
    {
        DataValue value = { .m_Type = DATA_TYPE_NULL };
        return value;
    }
    if (meta.m_Type != DATA_TYPE_STRING && meta.m_Type != DATA_TYPE_STRUCT && meta.m_Type != DATA_TYPE_LIST)
        return ReadPackedValue(bytes, meta.m_Type, 0);
    uint64_t reference = ReadDataInteger(bytes, 8);
    if (meta.m_Type == DATA_TYPE_STRUCT && IsStructRow(reference))
    {
        DataValue value = { .m_Type = DATA_TYPE_STRUCT, .m_Value = { .m_Struct = ReadStructRow(table, reference) } };
        return value;
    }
    return (reference & 1) ? ReadPackedValue(table->m_Blob, meta.m_Type, (uint32_t)(reference >> 1)) :
                             ReadPackedValue((const uint8_t*)(uintptr_t)reference, meta.m_Type, 0);
}

static bool IsReference(DataValueType type)
{
    return type == DATA_TYPE_STRING || type == DATA_TYPE_STRUCT || type == DATA_TYPE_LIST;
}

// Interpret caller-owned native input without allocating temporary value trees.
static DataValue ReadNativeInput(DataValueType type, const DataStructDesc* layout, const uint8_t* bytes)
{
    DataValue value = { .m_Type = type };
    if (layout)
    {
        value.m_Value.m_Struct.m_Source = DATA_STRUCT_NATIVE;
        value.m_Value.m_Struct.m_Input.m_Layout = layout;
        value.m_Value.m_Struct.m_Input.m_Data = bytes;
        value.m_Value.m_Struct.m_Count = layout->m_FieldCount;
    }
    else if (IsReference(type))
    {
        DataReference reference;
        memcpy(&reference, bytes, sizeof(reference));
        switch (type)
        {
            case DATA_TYPE_STRING:
                value.m_Value.m_String = reference.m_String;
                break;
            case DATA_TYPE_STRUCT:
                if (!reference.m_Struct || !reference.m_Struct->m_Layout)
                    value.m_Type = (DataValueType)UINT32_MAX;
                else
                    return ReadNativeInput(type, reference.m_Struct->m_Layout, (const uint8_t*)reference.m_Struct->m_Values);
                break;
            case DATA_TYPE_LIST:
                if (!reference.m_List)
                    value.m_Type = (DataValueType)UINT32_MAX;
                else
                {
                    value.m_Value.m_List.m_Input = reference.m_List;
                    value.m_Value.m_List.m_Count = reference.m_List->m_Count;
                }
                break;
            default:
                break;
        }
    }
    else if (type != DATA_TYPE_NULL)
        memcpy(&value.m_Value, bytes, DataTypeSize(type));
    return value;
}

uint64_t GetChildName(const DataStruct* object, uint32_t index)
{
    if (object->m_Source == DATA_STRUCT_NATIVE)
        return object->m_Input.m_Layout->m_Fields[index].m_Field;
    if (object->m_Source == DATA_STRUCT_ROW)
        return object->m_View.m_StructTable->m_Fields[index].m_Name;
    if (object->m_Source == DATA_STRUCT_INLINE)
        return GetFieldMeta(object->m_View.m_Table, object->m_View.m_Offset + index).m_Name;
    return object->m_Source == DATA_STRUCT_PACKED ? ReadDataInteger(object->m_View.m_Buffer + object->m_View.m_Offset + sizeof(DataFileContainerHeader) + index * 8, 8) : object->m_Array.m_Names[index];
}

DataValueType GetStructFieldType(const DataStruct* object, uint32_t index)
{
    if (object->m_Source == DATA_STRUCT_NATIVE)
        return object->m_Input.m_Layout->m_Fields[index].m_Type;
    if (object->m_Source == DATA_STRUCT_ROW)
        return object->m_View.m_StructTable->m_Fields[index].m_Type;
    if (object->m_Source == DATA_STRUCT_INLINE)
        return GetFieldMeta(object->m_View.m_Table, object->m_View.m_Offset + index).m_Type;
    if (object->m_Source == DATA_STRUCT_PACKED)
        return (DataValueType)ReadDataInteger(object->m_View.m_Buffer + object->m_View.m_Offset + sizeof(DataFileContainerHeader) + object->m_Count * 8 + index * 4, 4);
    return object->m_Array.m_Types[index];
}

DataValue GetChildValue(DataValueType type, const DataValueData* value, uint32_t index)
{
    bool              named = type == DATA_TYPE_STRUCT;
    const DataStruct* object = &value->m_Struct;
    if (named && object->m_Source == DATA_STRUCT_NATIVE)
    {
        const DataFieldDesc& field = object->m_Input.m_Layout->m_Fields[index];
        const uint8_t*       bytes = object->m_Input.m_Data ? object->m_Input.m_Data + field.m_Offset : 0;
        return ReadNativeInput(field.m_Type, field.m_Struct, bytes);
    }
    if (!named && value->m_List.m_Input)
    {
        const DataListInput* input = value->m_List.m_Input;
        if (input->m_Types)
        {
            DataValueType type = input->m_Types[index];
            const void*   payload = ((const void* const*)input->m_Values)[index];
            uint32_t      size = DataTypeSize(type);
            if (size == UINT32_MAX || (size && !payload))
            {
                DataValue invalid = { .m_Type = (DataValueType)UINT32_MAX };
                return invalid;
            }
            DataReference reference = {};
            switch (type)
            {
                case DATA_TYPE_STRING:
                    reference.m_String = (const char*)payload;
                    break;
                case DATA_TYPE_STRUCT:
                    reference.m_Struct = (const DataStructInput*)payload;
                    break;
                case DATA_TYPE_LIST:
                    reference.m_List = (const DataListInput*)payload;
                    break;
                default:
                    return ReadNativeInput(type, 0, (const uint8_t*)payload);
            }
            return ReadNativeInput(type, 0, (const uint8_t*)&reference);
        }
        uint32_t       stride = DataTypeSize(input->m_Type);
        const uint8_t* bytes = stride ? (const uint8_t*)input->m_Values + (size_t)index * stride : 0;
        return ReadNativeInput(input->m_Type, 0, bytes);
    }
    if (named && object->m_Source == DATA_STRUCT_ROW)
    {
        const DataStructTable* child_table = object->m_View.m_StructTable;
        const DataStructField& field = child_table->m_Fields[index];
        DataFieldMeta          meta = { .m_Type = field.m_Type, .m_Size = DataTypeSize(field.m_Type) };
        const uint8_t*         bytes = child_table->m_Values.Begin() + (size_t)object->m_View.m_Offset * child_table->m_RowStride + field.m_Offset;
        return ReadStoredValue(object->m_View.m_Table, meta, bytes);
    }
    if (named && object->m_Source == DATA_STRUCT_INLINE)
    {
        DataFieldMeta  meta = GetFieldMeta(object->m_View.m_Table, object->m_View.m_Offset + index);
        const uint8_t* bytes = meta.m_Size ? object->m_View.m_Buffer + meta.m_Offset : 0;
        return ReadStoredValue(object->m_View.m_Table, meta, bytes);
    }
    if (named && object->m_Source == DATA_STRUCT_ARRAY)
    {
        DataValue child = { .m_Type = object->m_Array.m_Types[index], .m_Value = object->m_Array.m_Values[index] };
        return child;
    }
    if (!named && !value->m_List.m_Buffer)
        return value->m_List.m_Values[index];
    const uint8_t* buffer = named ? value->m_Struct.m_View.m_Buffer : value->m_List.m_Buffer;
    uint32_t       offset = named ? value->m_Struct.m_View.m_Offset : value->m_List.m_Offset;
    uint32_t       count = named ? value->m_Struct.m_Count : value->m_List.m_Count;
    const uint8_t* types = buffer + offset + sizeof(DataFileContainerHeader) + (named ? count * 8 : 0);
    DataValueType  child_type = (DataValueType)ReadDataInteger(types + index * 4, 4);
    uint32_t       child = (uint32_t)ReadDataInteger(types + count * 4 + index * 4, 4);
    return ReadPackedValue(buffer, child_type, child);
}

DataResult DataGetStructField(const DataStruct* object, uint64_t field, DataValue* out_value)
{
    for (uint32_t i = 0; i < object->m_Count; ++i)
    {
        if (GetChildName(object, i) == field)
        {
            DataValue value = {
                .m_Type = DATA_TYPE_STRUCT,
                .m_Value = { .m_Struct = *object }
            };
            *out_value = GetChildValue(value.m_Type, &value.m_Value, i);
            return DATA_RESULT_OK;
        }
    }
    return DATA_RESULT_NOT_FOUND;
}

DataResult DataGetListValue(const DataList* list, uint32_t index, DataValue* out_value)
{
    if (index >= list->m_Count)
        return DATA_RESULT_NOT_FOUND;
    DataValue value = {
        .m_Type = DATA_TYPE_LIST,
        .m_Value = { .m_List = *list }
    };
    *out_value = GetChildValue(value.m_Type, &value.m_Value, index);
    return DATA_RESULT_OK;
}

static void StoreScalar(uint8_t* bytes, DataValueType type, const DataValueData* value)
{
    switch (type)
    {
        case DATA_TYPE_NUMBER:
        {
            uint64_t bits;
            memcpy(&bits, &value->m_Number, 8);
            WriteDataInteger(bytes, bits, 8);
            break;
        }
        case DATA_TYPE_BOOLEAN:
            *bytes = value->m_Boolean;
            break;
        case DATA_TYPE_VECTOR3:
            memcpy(bytes, value->m_Vector3, sizeof(value->m_Vector3));
            break;
        case DATA_TYPE_VECTOR4:
            memcpy(bytes, value->m_Vector4, sizeof(value->m_Vector4));
            break;
        case DATA_TYPE_MATRIX4:
            memcpy(bytes, value->m_Matrix4, sizeof(value->m_Matrix4));
            break;
        default:
            break;
    }
}

// Owned containers share the packed view layout. Child references are 32-bit
// offsets from the root, so no DataValue arrays or child pointers are retained.
static size_t StoreOwnedValue(uint8_t* buffer, size_t offset, DataValueType type, const DataValueData* value)
{
    uint8_t* bytes = buffer + offset;
    if (type == DATA_TYPE_STRUCT || type == DATA_TYPE_LIST)
    {
        bool     named = type == DATA_TYPE_STRUCT;
        uint32_t count = named ? value->m_Struct.m_Count : value->m_List.m_Count;
        uint8_t* types = bytes + sizeof(DataFileContainerHeader) + (named ? count * 8 : 0);
        uint8_t* offsets = types + count * 4;
        WriteDataInteger(bytes + offsetof(DataFileContainerHeader, m_ByteSize), 0, 4);
        WriteDataInteger(bytes + offsetof(DataFileContainerHeader, m_Count), count, 4);
        offset += sizeof(DataFileContainerHeader) + (size_t)count * (named ? 16 : 8);
        for (uint32_t i = 0; i < count; ++i)
        {
            DataValue child = GetChildValue(type, value, i);
            if (named)
                WriteDataInteger(bytes + sizeof(DataFileContainerHeader) + i * 8, GetChildName(&value->m_Struct, i), 8);
            WriteDataInteger(types + i * 4, child.m_Type, 4);
            WriteDataInteger(offsets + i * 4, offset, 4);
            offset = StoreOwnedValue(buffer, offset, child.m_Type, &child.m_Value);
        }
        return offset;
    }
    size_t size = DataTypeSize(type);
    if (type == DATA_TYPE_STRING)
    {
        size = strlen(value->m_String) + 1;
        memcpy(bytes, value->m_String, size);
    }
    else
    {
        StoreScalar(bytes, type, value);
    }
    size_t aligned_size = (size + 7) & ~(size_t)7;
    memset(bytes + size, 0, aligned_size - size);
    return offset + aligned_size;
}

// Callers validate and reserve the complete payload before writing. Blocks never
// move, so values borrowed from this same table remain valid during insertion/set.
void StoreValue(DataBlock** blocks, uint8_t* bytes, DataValueType type, const DataValueData* value)
{
    if (!IsReference(type))
    {
        StoreScalar(bytes, type, value);
        return;
    }

    DataBlock* block = *blocks;
    size_t     offset = (block->m_Size + 7) & ~(size_t)7;
    uint8_t*   buffer = (uint8_t*)block + ((sizeof(DataBlock) + 7) & ~(size_t)7) + offset;
    block->m_Size = offset + StoreOwnedValue(buffer, 0, type, value);
    assert(block->m_Size <= block->m_Capacity);
    WriteDataInteger(bytes, (uintptr_t)buffer, 8);
}

DataValue ReadBoundValue(const DataTable* table, const DataRow* row, const DataFieldMeta& meta)
{
    const uint8_t* bytes = meta.m_Size ? GetFieldBytes(table, row, meta.m_Offset) : 0;
    return ReadStoredValue(table, meta, bytes);
}

DataValue ReadRowValue(const DataTable* table, const DataRow* row, uint32_t field_index)
{
    return ReadBoundValue(table, row, GetFieldMeta(table, field_index));
}

// Tag only the mutable copy. Fixed structs recurse through shared metadata;
// strings and dynamic containers continue to borrow their original blob payloads.
static void TagBlobReferences(const DataTable* table, const DataFieldMeta& meta, uint8_t* bytes, uint32_t rows)
{
    if (meta.m_ChildIndex)
    {
        for (uint32_t i = 0; i < meta.m_ChildCount; ++i)
        {
            DataFieldMeta child = GetFieldMeta(table, meta.m_ChildIndex + i);
            TagBlobReferences(table, child, bytes + (child.m_Offset - meta.m_Offset), rows);
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

void InitializeBlobValues(DataTable* table, uint32_t start, uint32_t count)
{
    if (!table->m_RowStride || !count)
        return;

    uint8_t* bytes = table->m_Values.Begin() + (size_t)start * table->m_RowStride;
    memcpy(bytes, table->m_Blob + table->m_Offsets.m_Rows, (size_t)count * table->m_RowStride);

    if (table->m_HasReferences)
    {
        for (uint32_t i = 0; i < table->m_FieldCount; ++i)
        {
            DataFieldMeta meta = GetFieldMeta(table, i);
            TagBlobReferences(table, meta, bytes + meta.m_Offset, count);
        }
    }
}

static const uint8_t* GetDefaultRow(const DataTable* table, const DataRow* row)
{
    const uint8_t* base = table->m_Pool ? table->m_Blob + table->m_Offsets.m_Rows : table->m_Owned->m_BaseRows.Begin();
    return base + (size_t)row->m_BaseIndex * table->m_RowStride;
}

// The current and default references may name the same exclusively owned tree.
// Only release replacements on reset; removal also releases the original tree.
static void ReleaseFieldStructs(DataTable* table, DataRow* row, const DataFieldMeta& meta, bool defaults)
{
    if (meta.m_Type != DATA_TYPE_STRUCT)
        return;
    if (meta.m_ChildIndex)
    {
        for (uint32_t i = 0; i < meta.m_ChildCount; ++i)
            ReleaseFieldStructs(table, row, GetFieldMeta(table, meta.m_ChildIndex + i), defaults);
        return;
    }
    uint8_t* bytes = GetFieldBytes(table, row, meta.m_Offset);
    uint64_t current = ReadDataInteger(bytes, 8);
    uint64_t base = ReadDataInteger(GetDefaultRow(table, row) + meta.m_Offset, 8);
    if (IsStructRow(current) && (table->m_Pool || current != base))
        ReleaseStructRow(table, current);
    if (defaults && !table->m_Pool && IsStructRow(base))
        ReleaseStructRow(table, base);
}

void ReleaseRowStructs(DataTable* table, uint32_t row, bool defaults)
{
    for (uint32_t i = 0; i < table->m_FieldCount; ++i)
        ReleaseFieldStructs(table, &table->m_Rows[row], GetFieldMeta(table, i), defaults);
}

static void ResetRow(DataTable* table, DataRow* row)
{
    if (!table->m_RowStride)
        return;
    if (table->m_Structs)
        ReleaseRowStructs(table, (uint32_t)(row - table->m_Rows.Begin()), false);
    uint8_t* bytes = GetFieldBytes(table, row, 0);
    memcpy(bytes, GetDefaultRow(table, row), table->m_RowStride);
    if (table->m_Pool && table->m_HasReferences)
    {
        for (uint32_t i = 0; i < table->m_FieldCount; ++i)
        {
            DataFieldMeta meta = GetFieldMeta(table, i);
            TagBlobReferences(table, meta, bytes + meta.m_Offset, 1);
        }
    }
}

// Measure arena payloads during validation. Struct fields use separate child
// rows; lists retain packed trees with 32-bit offsets.
static DataResult ValidateValue(DataValueType type, const DataValueData* value, uint32_t depth, size_t* out_size, bool packed = false)
{
    uint32_t size = DataTypeSize(type);
    if (size == UINT32_MAX)
        return DATA_RESULT_INVALID_ARGUMENT;
    *out_size = ((size_t)size + 7) & ~(size_t)7;
    if (type == DATA_TYPE_BOOLEAN)
        return value->m_Boolean <= 1 ? DATA_RESULT_OK : DATA_RESULT_INVALID_ARGUMENT;
    if (type == DATA_TYPE_STRING)
    {
        if (!value->m_String)
            return DATA_RESULT_INVALID_ARGUMENT;
        size_t length = strlen(value->m_String);
        if (length > UINT32_MAX - 8)
            return DATA_RESULT_INVALID_ARGUMENT;
        *out_size = (length + 8) & ~(size_t)7;
        return DATA_RESULT_OK;
    }
    if (type != DATA_TYPE_STRUCT && type != DATA_TYPE_LIST)
        return DATA_RESULT_OK;
    bool           named = type == DATA_TYPE_STRUCT;
    uint32_t       count = named ? value->m_Struct.m_Count : value->m_List.m_Count;
    bool           native = named ? value->m_Struct.m_Source == DATA_STRUCT_NATIVE : value->m_List.m_Input != 0;
    bool           arrays = named ? value->m_Struct.m_Source == DATA_STRUCT_ARRAY : !native && !value->m_List.m_Buffer;
    bool           missing_input = arrays && (named ? (!value->m_Struct.m_Array.m_Names || !value->m_Struct.m_Array.m_Types || !value->m_Struct.m_Array.m_Values) : !value->m_List.m_Values);
    if (depth == DATA_MAX_NESTING || count > UINT32_MAX / (named ? sizeof(DataValueData) : sizeof(DataValue)) ||
        (count && missing_input))
        return DATA_RESULT_INVALID_ARGUMENT;
    if (named && count > DATA_MAX_METADATA_COUNT)
        return DATA_RESULT_INVALID_ARGUMENT;
    if (native && named)
    {
        const DataStructDesc* layout = value->m_Struct.m_Input.m_Layout;
        if (layout->m_Size && !value->m_Struct.m_Input.m_Data)
            return DATA_RESULT_INVALID_ARGUMENT;
        uint64_t   metadata_count = 0;
        uint32_t   alignment;
        DataResult result = ValidateLayout(layout->m_Fields, layout->m_FieldCount, layout->m_Size, 0, &metadata_count, &alignment);
        if (result != DATA_RESULT_OK)
            return result;
    }
    else if (native)
    {
        const DataListInput* input = value->m_List.m_Input;
        uint32_t             stride = input->m_Types ? sizeof(void*) : DataTypeSize(input->m_Type);
        if (stride == UINT32_MAX || (stride && count && !input->m_Values) || (uint64_t)stride * count > SIZE_MAX)
            return DATA_RESULT_INVALID_ARGUMENT;
    }
    uint64_t total = named && !packed ? 0 : sizeof(DataFileContainerHeader) + (uint64_t)count * (named ? 16 : 8);
    for (uint32_t i = 0; i < count; ++i)
    {
        DataValue  child = GetChildValue(type, value, i);
        size_t     child_size;
        DataResult result = ValidateValue(child.m_Type, &child.m_Value, depth + 1, &child_size, packed || !named);
        if (result != DATA_RESULT_OK)
            return result;
        if (packed || !named || IsReference(child.m_Type))
            total += child_size;
        if (total > UINT32_MAX)
            return DATA_RESULT_INVALID_ARGUMENT;
        if (named)
        {
            uint64_t name = GetChildName(&value->m_Struct, i);
            for (uint32_t j = 0; j < i; ++j)
                if (GetChildName(&value->m_Struct, j) == name)
                    return DATA_RESULT_ALREADY_EXISTS;
        }
    }
    *out_size = (size_t)total;
    return DATA_RESULT_OK;
}

// Validate reference payloads before reserving or publishing any native rows.
DataResult ValidateNativeReferences(const DataTable* table, const DataFieldMeta& meta, const uint8_t* values, uint32_t stride, uint32_t count, uint32_t depth, size_t* payload_size)
{
    if (meta.m_ChildIndex)
    {
        for (uint32_t i = 0; i < meta.m_ChildCount; ++i)
        {
            DataFieldMeta child = GetFieldMeta(table, meta.m_ChildIndex + i);
            DataResult    result = ValidateNativeReferences(table, child, values + (child.m_Offset - meta.m_Offset), stride, count, depth + 1, payload_size);
            if (result != DATA_RESULT_OK)
                return result;
        }
    }
    else if (IsReference(meta.m_Type))
    {
        for (uint32_t r = 0; r < count; ++r)
        {
            DataValue  value = ReadNativeInput(meta.m_Type, 0, values + (size_t)r * stride);
            size_t     size;
            DataResult result = ValidateValue(value.m_Type, &value.m_Value, depth, &size);
            if (result != DATA_RESULT_OK)
                return result;
            if (size > SIZE_MAX - sizeof(DataBlock) - *payload_size)
                return DATA_RESULT_INVALID_ARGUMENT;
            *payload_size += size;
        }
    }
    return DATA_RESULT_OK;
}

// Native bytes are already copied. Replace only input reference slots with
// owned payload addresses or child-row indices. One arena reservation covers
// strings and lists for the complete batch.
void StoreNativeReferences(DataTable* table, uint32_t count, uint8_t* rows)
{
    for (uint32_t f = 0; f < table->m_MetadataCount; ++f)
    {
        const DataFieldMeta& meta = table->m_Owned->m_Fields[f];
        if (meta.m_ChildIndex || !IsReference(meta.m_Type))
            continue;
        for (uint32_t r = 0; r < count; ++r)
        {
            uint8_t*  bytes = rows + (size_t)r * table->m_RowStride + meta.m_Offset;
            DataValue value = ReadNativeInput(meta.m_Type, 0, bytes);
            if (meta.m_Type == DATA_TYPE_STRUCT)
                WriteDataInteger(bytes, StoreStructRow(table, &table->m_Owned->m_BaseValues, &value.m_Value.m_Struct), 8);
            else
                StoreValue(&table->m_Owned->m_BaseValues, bytes, meta.m_Type, &value.m_Value);
        }
    }
}

// Decoded input commonly follows layout order, but matching still uses names.
static uint32_t FindDecodedField(const DataStruct* object, uint64_t name, uint32_t index)
{
    if (object->m_Array.m_Names[index] == name)
        return index;
    for (uint32_t i = 0; i < object->m_Count; ++i)
        if (object->m_Array.m_Names[i] == name)
            return i;
    return UINT32_MAX;
}

DataResult ValidateStoredValue(const DataTable* table, const DataFieldMeta& meta, DataValueType type, const DataValueData* value, uint32_t depth, size_t* payload_size)
{
    if (type != meta.m_Type || depth > DATA_MAX_NESTING)
        return DATA_RESULT_INVALID_ARGUMENT;
    if (!meta.m_ChildIndex)
    {
        // Registration validated fixed kinds and sizes. Only Boolean has a
        // restricted scalar value; references still need payload validation.
        if (!IsReference(type))
            return type != DATA_TYPE_BOOLEAN || value->m_Boolean <= 1 ? DATA_RESULT_OK : DATA_RESULT_INVALID_ARGUMENT;
        size_t     size;
        DataResult result = ValidateValue(type, value, depth, &size);
        if (result != DATA_RESULT_OK)
            return result;
        if (size > SIZE_MAX - sizeof(DataBlock) - *payload_size)
            return DATA_RESULT_INVALID_ARGUMENT;
        *payload_size += size;
        return DATA_RESULT_OK;
    }

    const DataStruct* object = &value->m_Struct;
    if (object->m_Count != meta.m_ChildCount || (object->m_Source == DATA_STRUCT_ARRAY && object->m_Count && (!object->m_Array.m_Names || !object->m_Array.m_Types || !object->m_Array.m_Values)))
        return DATA_RESULT_INVALID_ARGUMENT;

    const DataFieldMeta* fields = table->m_Blob ? 0 : table->m_Owned->m_Fields.Begin() + meta.m_ChildIndex;
    for (uint32_t i = 0; i < meta.m_ChildCount; ++i)
    {
        DataFieldMeta packed_field;
        if (!fields)
            packed_field = GetFieldMeta(table, meta.m_ChildIndex + i);
        const DataFieldMeta& field = fields ? fields[i] : packed_field;
        DataValue            child;
        DataValueType        type;
        const DataValueData* data;
        if (object->m_Source == DATA_STRUCT_ARRAY)
        {
            uint32_t index = FindDecodedField(object, field.m_Name, i);
            if (index == UINT32_MAX)
                return DATA_RESULT_INVALID_ARGUMENT;
            type = object->m_Array.m_Types[index];
            data = &object->m_Array.m_Values[index];
        }
        else
        {
            if (DataGetStructField(object, field.m_Name, &child) != DATA_RESULT_OK)
                return DATA_RESULT_INVALID_ARGUMENT;
            type = child.m_Type;
            data = &child.m_Value;
        }

        // Registration bounds the inline depth. Scalar children need no
        // recursive traversal; references retain their payload/depth checks.
        if (type != field.m_Type || (type == DATA_TYPE_BOOLEAN && data->m_Boolean > 1))
            return DATA_RESULT_INVALID_ARGUMENT;
        if (IsReference(type))
        {
            DataResult result = ValidateStoredValue(table, field, type, data, depth + 1, payload_size);
            if (result != DATA_RESULT_OK)
                return result;
        }
    }
    return DATA_RESULT_OK;
}

void StoreStoredValue(DataTable* table, const DataFieldMeta& meta, DataBlock** blocks, uint8_t* bytes, const DataValueData* value)
{
    if (!meta.m_ChildIndex)
    {
        if (meta.m_Type == DATA_TYPE_STRUCT)
            WriteDataInteger(bytes, StoreStructRow(table, blocks, &value->m_Struct), 8);
        else
            StoreValue(blocks, bytes, meta.m_Type, value);
        return;
    }

    const DataStruct*    object = &value->m_Struct;
    const DataFieldMeta* fields = table->m_Blob ? 0 : table->m_Owned->m_Fields.Begin() + meta.m_ChildIndex;
    for (uint32_t i = 0; i < meta.m_ChildCount; ++i)
    {
        DataFieldMeta packed_field;
        if (!fields)
            packed_field = GetFieldMeta(table, meta.m_ChildIndex + i);
        const DataFieldMeta& field = fields ? fields[i] : packed_field;
        uint8_t*             target = field.m_Size ? bytes + (field.m_Offset - meta.m_Offset) : 0;
        DataValue            child;
        const DataValueData* data;
        if (object->m_Source == DATA_STRUCT_ARRAY)
        {
            uint32_t index = FindDecodedField(object, field.m_Name, i);
            data = &object->m_Array.m_Values[index];
        }
        else
        {
            DataGetStructField(object, field.m_Name, &child);
            data = &child.m_Value;
        }
        if (IsReference(field.m_Type))
            StoreStoredValue(table, field, blocks, target, data);
        else
            StoreScalar(target, field.m_Type, data);
    }
}

// Batch construction keeps ordinary native fields in a small loop. Inline
// structs and references retain their name/payload validation below.
DataResult ValidateRowValues(const DataTable* table, const DataRowDesc* rows, uint32_t count, size_t* payload_size)
{
    const DataFieldMeta* fields = table->m_Owned->m_Fields.Begin();
    uint32_t             field_count = table->m_FieldCount;

    for (uint32_t i = 0; i < count; ++i)
    {
        const DataRowDesc& row = rows[i];
        if (row.m_ValueCount != field_count || (field_count && (!row.m_Types || !row.m_Values)))
            return DATA_RESULT_INVALID_ARGUMENT;

        for (uint32_t j = 0; j < field_count; ++j)
        {
            const DataFieldMeta& meta = fields[j];
            DataValueType        type = row.m_Types[j];
            if (type != meta.m_Type || (type == DATA_TYPE_BOOLEAN && row.m_Values[j].m_Boolean > 1))
                return DATA_RESULT_INVALID_ARGUMENT;
            if (IsReference(type))
            {
                DataResult result = ValidateStoredValue(table, meta, type, &row.m_Values[j], 0, payload_size);
                if (result != DATA_RESULT_OK)
                    return result;
            }
        }
    }
    return DATA_RESULT_OK;
}

// Values have been validated and payload blocks reserved for the whole batch.
void StoreRowValues(DataTable* table, const DataRowDesc* rows, uint32_t count, uint8_t* bytes)
{
    const DataFieldMeta* fields = table->m_Owned->m_Fields.Begin();
    uint32_t             field_count = table->m_FieldCount;

    for (uint32_t i = 0; i < count; ++i)
    {
        for (uint32_t j = 0; j < field_count; ++j)
        {
            const DataFieldMeta& meta = fields[j];
            uint8_t*             target = bytes + meta.m_Offset;
            if (IsReference(meta.m_Type))
                StoreStoredValue(table, meta, &table->m_Owned->m_BaseValues, target, &rows[i].m_Values[j]);
            else
                StoreScalar(target, meta.m_Type, &rows[i].m_Values[j]);
        }

        bytes += table->m_RowStride;
    }
}

DataResult ReadField(DataTable* table, uint32_t row, uint64_t field, DataValue* out_value)
{
    uint32_t index = FindField(table, field);
    if (index == UINT32_MAX)
        return DATA_RESULT_NOT_FOUND;
    *out_value = ReadRowValue(table, &table->m_Rows[row], index);
    return DATA_RESULT_OK;
}

DataResult DataFieldGet(HDataStore store, DataId id, uint64_t field, DataValue* out_value)
{
    DataSlot* slot = FindSlot(store, id);
    return slot ? ReadField(slot->m_Table, slot->m_Row, field, out_value) : DATA_RESULT_NOT_FOUND;
}

// A full-name lookup can start below the root. Include its inline ancestors in
// the same container-depth budget used by row creation and blob validation.
static uint32_t GetInlineDepth(const DataTable* table, uint64_t field)
{
    uint32_t index = FindField(table, field);
    uint32_t depth = 0;
    while (index >= table->m_FieldCount)
    {
        for (uint32_t i = 0; i < index; ++i)
        {
            DataFieldMeta parent = GetFieldMeta(table, i);
            if (index >= parent.m_ChildIndex && index - parent.m_ChildIndex < parent.m_ChildCount)
            {
                index = i;
                ++depth;
                break;
            }
        }
    }
    return depth;
}

DataResult SetRowField(DataTable* table, DataRow* row, const DataFieldMeta& meta, const DataValue* value)
{
    size_t     payload_size = 0;
    uint32_t   depth = meta.m_Type == DATA_TYPE_STRUCT || meta.m_Type == DATA_TYPE_LIST ? GetInlineDepth(table, meta.m_Field) : 0;
    DataResult result = ValidateStoredValue(table, meta, value->m_Type, &value->m_Value, depth, &payload_size);
    if (result != DATA_RESULT_OK)
        return result;

    DataBlock** payloads = &table->m_Payloads;
    if (payload_size)
    {
        if (table->m_Pool)
            payloads = &GetPoolInstance(table->m_Pool, row->m_InstanceIndex)->m_Payloads;
        ReserveData(payloads, payload_size);
    }

    uint8_t* bytes = meta.m_Size ? GetFieldBytes(table, row, meta.m_Offset) : 0;
    if (meta.m_Type == DATA_TYPE_STRUCT)
    {
        // Construct first: value may be a view into the struct being replaced.
        uint8_t          local[256];
        dmArray<uint8_t> replacement;
        if (meta.m_Size <= sizeof(local))
            replacement.Set(local, meta.m_Size, sizeof(local), true);
        else
        {
            replacement.SetCapacity(meta.m_Size);
            replacement.SetSize(meta.m_Size);
        }
        memset(replacement.Begin(), 0, meta.m_Size);
        StoreStoredValue(table, meta, payloads, replacement.Begin(), &value->m_Value);
        ReleaseFieldStructs(table, row, meta, false);
        memcpy(bytes, replacement.Begin(), meta.m_Size);
    }
    else
        StoreStoredValue(table, meta, payloads, bytes, &value->m_Value);
    return DATA_RESULT_OK;
}

DataResult DataSetField(HDataStore store, DataId id, uint64_t field, const DataValue* value)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_ActiveQueries.Size())
        return DATA_RESULT_LOCKED;
    DataSlot* slot = FindSlot(store, id);
    if (!slot)
        return DATA_RESULT_NOT_FOUND;
    DataTable* table = slot->m_Table;
    uint32_t   index = FindField(table, field);
    if (index == UINT32_MAX)
        return DATA_RESULT_NOT_FOUND;
    return SetRowField(table, &table->m_Rows[slot->m_Row], GetFieldMeta(table, index), value);
}

DataResult DataResetField(HDataStore store, DataId id, uint64_t field)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_ActiveQueries.Size())
        return DATA_RESULT_LOCKED;
    DataSlot* slot = FindSlot(store, id);
    if (!slot)
        return DATA_RESULT_NOT_FOUND;
    DataTable* table = slot->m_Table;
    uint32_t   index = FindField(table, field);
    if (index == UINT32_MAX)
        return DATA_RESULT_NOT_FOUND;
    DataRow*      row = &table->m_Rows[slot->m_Row];
    DataFieldMeta meta = GetFieldMeta(table, index);
    if (meta.m_Size)
    {
        if (table->m_Structs)
            ReleaseFieldStructs(table, row, meta, false);
        uint8_t* bytes = GetFieldBytes(table, row, meta.m_Offset);
        memcpy(bytes, GetDefaultRow(table, row) + meta.m_Offset, meta.m_Size);
        if (table->m_Pool)
            TagBlobReferences(table, meta, bytes, 1);
    }
    return DATA_RESULT_OK;
}

DataResult DataResetRow(HDataStore store, DataId id)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_ActiveQueries.Size())
        return DATA_RESULT_LOCKED;
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
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_ActiveQueries.Size())
        return DATA_RESULT_LOCKED;
    DataTable* table = FindTable(store, type);
    if (!table)
        return DATA_RESULT_NOT_FOUND;
    ResetTable(table);
    return DATA_RESULT_OK;
}

DataResult DataResetBlob(HDataBlobInstance instance)
{
    DataBlobPool* pool = instance->m_Pool;
    DM_MUTEX_SCOPED_LOCK(pool->m_Store->m_Mutex);
    if (pool->m_Store->m_ActiveQueries.Size())
        return DATA_RESULT_LOCKED;
    const uint32_t* slots = GetInstanceSlots(instance);
    for (uint32_t i = 0; i < pool->m_Blob->m_RowCount; ++i)
    {
        if (slots[i] == INVALID_SLOT)
            continue;
        const DataSlot& slot = pool->m_Store->m_Slots[slots[i]];
        ResetRow(slot.m_Table, &slot.m_Table->m_Rows[slot.m_Row]);
    }
    DeleteBlocks(&instance->m_Payloads);
    return DATA_RESULT_OK;
}

// Owned and packed metadata share the fields read by the typed lookup.
DM_STATIC_ASSERT(sizeof(DataFieldMeta) == sizeof(DataFileFieldMeta) &&
                 offsetof(DataFieldMeta, m_Field) == offsetof(DataFileFieldMeta, m_NameHash) &&
                 offsetof(DataFieldMeta, m_Type) == offsetof(DataFileFieldMeta, m_Kind) &&
                 offsetof(DataFieldMeta, m_Offset) == offsetof(DataFileFieldMeta, m_ByteOffset),
                 Invalid_typed_lookup_metadata_layout);

template <DataValueType TYPE>
static DataResult ResolveTypedField(HDataStore store, DataId id, uint64_t field, DataTable** out_table, const uint8_t** out_bytes)
{
    DataSlot* slot = FindSlot(store, id);
    if (!slot)
        return DATA_RESULT_NOT_FOUND;

    DataTable* table = slot->m_Table;
    // Probe the table's immutable index and verify the full hash. A collision
    // falls back to scanning names; kind/offset are read only for the match.
    const uint8_t* metadata = table->m_FieldMetadata;
    uint32_t       index = table->m_FieldLookup[field & (DATA_FIELD_LOOKUP_SIZE - 1)];
    if (index == UINT16_MAX)
        return DATA_RESULT_NOT_FOUND;
    uint64_t name;
    memcpy(&name, metadata + (size_t)index * DATA_FIELD_META_SIZE + offsetof(DataFileFieldMeta, m_NameHash), sizeof(name));
    if (name != field)
    {
        for (index = 0; index < table->m_MetadataCount; ++index)
        {
            memcpy(&name, metadata + (size_t)index * DATA_FIELD_META_SIZE + offsetof(DataFileFieldMeta, m_NameHash), sizeof(name));
            if (name == field)
                break;
        }
        if (index == table->m_MetadataCount)
            return DATA_RESULT_NOT_FOUND;
    }

    metadata += (size_t)index * DATA_FIELD_META_SIZE;
    uint32_t kind, offset;
    memcpy(&kind, metadata + offsetof(DataFileFieldMeta, m_Kind), sizeof(kind));
    if (kind != TYPE)
        return DATA_RESULT_INVALID_ARGUMENT;
    memcpy(&offset, metadata + offsetof(DataFileFieldMeta, m_ByteOffset), sizeof(offset));

    // The validated slot already contains the dense row index; no row identity is needed.
    const uint8_t* bytes = table->m_Values.Begin() + (size_t)slot->m_Row * table->m_RowStride + offset;
    *out_table = table;
    *out_bytes = bytes;
    return DATA_RESULT_OK;
}

template <DataValueType TYPE>
static DataResult GetTypedField(HDataStore store, DataId id, uint64_t field, void* out_value)
{
    DataTable*     table;
    const uint8_t* bytes;
    DataResult     result = ResolveTypedField<TYPE>(store, id, field, &table, &bytes);
    return result == DATA_RESULT_OK ? CopyTypedValue<TYPE>(table, bytes, out_value) : result;
}

// A hint only: unsupported compilers still use the staged address/copy loops.
static inline void PrefetchField(const void* address)
{
#if defined(__clang__) || defined(__GNUC__)
    __builtin_prefetch(address, 0, 1);
#endif
}

template <DataValueType TYPE, typename T>
static DataResult GetTypedFields(HDataStore store, uint32_t count, const DataId* ids, uint64_t field, T* values)
{
    const uint32_t batch_size = 64;
    for (uint32_t first = 0; first < count;)
    {
        uint32_t       size = count - first < batch_size ? count - first : batch_size;
        const uint8_t* bytes[batch_size];
        DataTable*     tables[batch_size];

        for (uint32_t i = 0; i < size; ++i)
        {
            uint32_t index = (uint32_t)ids[first + i];
            if (index < store->m_Slots.Size())
                PrefetchField(store->m_Slots.Begin() + index);
        }

        for (uint32_t i = 0; i < size; ++i)
        {
            DataResult result = ResolveTypedField<TYPE>(store, ids[first + i], field, &tables[i], &bytes[i]);
            if (result != DATA_RESULT_OK)
                return result;
            PrefetchField(bytes[i]);
        }

        for (uint32_t i = 0; i < size; ++i)
            CopyTypedValue<TYPE>(tables[i], bytes[i], &values[first + i]);
        first += size;
    }
    return DATA_RESULT_OK;
}

DataResult DataFieldGetNumber(HDataStore store, DataId id, uint64_t field, double* out_value)
{
    return GetTypedField<DATA_TYPE_NUMBER>(store, id, field, out_value);
}

DataResult DataFieldGetNumberBatch(HDataStore store, uint32_t count, const DataId* ids, uint64_t field, double* out_values)
{
    return GetTypedFields<DATA_TYPE_NUMBER>(store, count, ids, field, out_values);
}

DataResult DataSetFieldNumber(HDataStore store, DataId id, uint64_t field, double value)
{
    DataValue input = {
        .m_Type = DATA_TYPE_NUMBER,
        .m_Value = { .m_Number = value }
    };
    return DataSetField(store, id, field, &input);
}

DataResult DataFieldGetBoolean(HDataStore store, DataId id, uint64_t field, uint8_t* out_value)
{
    return GetTypedField<DATA_TYPE_BOOLEAN>(store, id, field, out_value);
}

DataResult DataFieldGetBooleanBatch(HDataStore store, uint32_t count, const DataId* ids, uint64_t field, uint8_t* out_values)
{
    return GetTypedFields<DATA_TYPE_BOOLEAN>(store, count, ids, field, out_values);
}

DataResult DataSetFieldBoolean(HDataStore store, DataId id, uint64_t field, uint8_t value)
{
    DataValue input = {
        .m_Type = DATA_TYPE_BOOLEAN,
        .m_Value = { .m_Boolean = value }
    };
    return DataSetField(store, id, field, &input);
}

DataResult DataFieldGetString(HDataStore store, DataId id, uint64_t field, const char** out_value)
{
    return GetTypedField<DATA_TYPE_STRING>(store, id, field, out_value);
}

DataResult DataFieldGetStringBatch(HDataStore store, uint32_t count, const DataId* ids, uint64_t field, const char** out_values)
{
    return GetTypedFields<DATA_TYPE_STRING>(store, count, ids, field, out_values);
}

DataResult DataSetFieldString(HDataStore store, DataId id, uint64_t field, const char* value)
{
    DataValue input = {
        .m_Type = DATA_TYPE_STRING,
        .m_Value = { .m_String = value }
    };
    return DataSetField(store, id, field, &input);
}

DataResult DataFieldGetVector3(HDataStore store, DataId id, uint64_t field, DataVector3* out_value)
{
    return GetTypedField<DATA_TYPE_VECTOR3>(store, id, field, out_value->m_Values);
}

DataResult DataFieldGetVector3Batch(HDataStore store, uint32_t count, const DataId* ids, uint64_t field, DataVector3* out_values)
{
    return GetTypedFields<DATA_TYPE_VECTOR3>(store, count, ids, field, out_values);
}

DataResult DataSetFieldVector3(HDataStore store, DataId id, uint64_t field, const DataVector3* value)
{
    DataValue input = { .m_Type = DATA_TYPE_VECTOR3 };
    memcpy(input.m_Value.m_Vector3, value->m_Values, sizeof(value->m_Values));
    return DataSetField(store, id, field, &input);
}

DataResult DataFieldGetVector4(HDataStore store, DataId id, uint64_t field, DataVector4* out_value)
{
    return GetTypedField<DATA_TYPE_VECTOR4>(store, id, field, out_value->m_Values);
}

DataResult DataFieldGetVector4Batch(HDataStore store, uint32_t count, const DataId* ids, uint64_t field, DataVector4* out_values)
{
    return GetTypedFields<DATA_TYPE_VECTOR4>(store, count, ids, field, out_values);
}

DataResult DataSetFieldVector4(HDataStore store, DataId id, uint64_t field, const DataVector4* value)
{
    DataValue input = { .m_Type = DATA_TYPE_VECTOR4 };
    memcpy(input.m_Value.m_Vector4, value->m_Values, sizeof(value->m_Values));
    return DataSetField(store, id, field, &input);
}

DataResult DataFieldGetMatrix4(HDataStore store, DataId id, uint64_t field, DataMatrix4* out_value)
{
    return GetTypedField<DATA_TYPE_MATRIX4>(store, id, field, out_value->m_Values);
}

DataResult DataFieldGetMatrix4Batch(HDataStore store, uint32_t count, const DataId* ids, uint64_t field, DataMatrix4* out_values)
{
    return GetTypedFields<DATA_TYPE_MATRIX4>(store, count, ids, field, out_values);
}

DataResult DataSetFieldMatrix4(HDataStore store, DataId id, uint64_t field, const DataMatrix4* value)
{
    DataValue input = { .m_Type = DATA_TYPE_MATRIX4 };
    memcpy(input.m_Value.m_Matrix4, value->m_Values, sizeof(value->m_Values));
    return DataSetField(store, id, field, &input);
}

// Resolve full names once while preparing a query.
bool ResolveField(const DataTable* table, uint64_t field, DataValueType type, DataFieldMeta* out_meta)
{
    uint32_t index = FindField(table, field);
    if (index == UINT32_MAX)
        return false;
    DataFieldMeta meta = GetFieldMeta(table, index);
    if (meta.m_Type != type)
        return false;
    *out_meta = meta;
    return true;
}
