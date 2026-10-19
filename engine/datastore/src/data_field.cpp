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
struct DSNativeAlignment
{
    uint8_t m_Padding;
    T       m_Value;
};
DM_STATIC_ASSERT(sizeof(double) == 8 && offsetof(DSNativeAlignment<double>, m_Value) <= 8, Invalid_number_layout);
DM_STATIC_ASSERT(sizeof(float) == 4 && offsetof(DSNativeAlignment<float>, m_Value) <= 4, Invalid_float_layout);
DM_STATIC_ASSERT(sizeof(DSVector3) == 12 && offsetof(DSNativeAlignment<DSVector3>, m_Value) <= 4, Invalid_vector3_layout);
DM_STATIC_ASSERT(sizeof(DSVector4) == 16 && offsetof(DSNativeAlignment<DSVector4>, m_Value) <= 4, Invalid_vector4_layout);
DM_STATIC_ASSERT(sizeof(DSMatrix4) == 64 && offsetof(DSNativeAlignment<DSMatrix4>, m_Value) <= 4, Invalid_matrix4_layout);
DM_STATIC_ASSERT(sizeof(DSReference) == 8, Invalid_reference_input_layout);

// Row storage contains payload bytes, not DSValue objects. DSFieldMeta
// supplies each field's fixed type and byte offset once per table. Numeric
// payloads use little-endian bytes with C-compatible member/row alignment.
// DSValue is a temporary tagged input/result for internal generic operations. Its tag selects
// the union member and validates writes; typed getters do not construct one.
// Declared fixed structs are inline bytes with shared child metadata. Dynamic
// structs share child-table metadata; lists retain packed child kind/offset arrays.

// Size of a field's slot in a byte row. Eight-byte reference slots hold a blob
// offset, an owned string/list pointer, or owned struct table/row indices.
uint32_t DSTypeSize(DSValueType type)
{
    switch (type)
    {
        case DS_TYPE_NULL:
            return 0;
        case DS_TYPE_BOOLEAN:
            return 1;
        case DS_TYPE_FLOAT:
        case DS_TYPE_UINT32:
            return 4;
        case DS_TYPE_NUMBER:
        case DS_TYPE_STRING:
        case DS_TYPE_STRUCT:
        case DS_TYPE_LIST:
            return 8;
        case DS_TYPE_VECTOR3:
            return 12;
        case DS_TYPE_VECTOR4:
            return 16;
        case DS_TYPE_MATRIX4:
            return 64;
        default:
            return UINT32_MAX;
    }
}

uint32_t DSTypeAlignment(DSValueType type)
{
    switch (type)
    {
        case DS_TYPE_NUMBER:
        case DS_TYPE_STRING:
        case DS_TYPE_STRUCT:
        case DS_TYPE_LIST:
            return 8;
        case DS_TYPE_VECTOR3:
        case DS_TYPE_VECTOR4:
        case DS_TYPE_MATRIX4:
        case DS_TYPE_FLOAT:
        case DS_TYPE_UINT32:
            return 4;
        default:
            return 1;
    }
}

// A packed container is a view. Reading it never allocates or expands its children.
// The type argument comes from table metadata or a container's child metadata;
// there is no type tag alongside the scalar payload at buffer + offset.
static DSValue ReadPackedValue(const uint8_t* buffer, DSValueType type, uint32_t offset)
{
    DSValue        value = { .m_Type = type };
    const uint8_t* data = buffer + offset;
    switch (type)
    {
        case DS_TYPE_NUMBER:
        {
            uint64_t bits = ReadDataInteger(data, 8);
            memcpy(&value.m_Value.m_Number, &bits, 8);
            break;
        }
        case DS_TYPE_FLOAT:
        case DS_TYPE_UINT32:
        {
            uint32_t bits = (uint32_t)ReadDataInteger(data, 4);
            memcpy(&value.m_Value, &bits, 4);
            break;
        }
        case DS_TYPE_BOOLEAN:
            value.m_Value.m_Boolean = *data;
            break;
        case DS_TYPE_STRING:
            value.m_Value.m_String = (const char*)data;
            break;
        case DS_TYPE_STRUCT:
            value.m_Value.m_Struct.m_Source = DS_STRUCT_PACKED;
            value.m_Value.m_Struct.m_View.m_Buffer = buffer;
            value.m_Value.m_Struct.m_View.m_Offset = offset;
            value.m_Value.m_Struct.m_Count = (uint32_t)ReadDataInteger(data + offsetof(DSFileContainerHeader, m_Count), 4);
            break;
        case DS_TYPE_LIST:
            value.m_Value.m_List.m_Buffer = buffer;
            value.m_Value.m_List.m_Offset = offset;
            value.m_Value.m_List.m_Count = (uint32_t)ReadDataInteger(data + offsetof(DSFileContainerHeader, m_Count), 4);
            break;
        case DS_TYPE_VECTOR3:
            ReadDataFloats(value.m_Value.m_Vector3, data, 3);
            break;
        case DS_TYPE_VECTOR4:
            ReadDataFloats(value.m_Value.m_Vector4, data, 4);
            break;
        case DS_TYPE_MATRIX4:
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
static DSValue ReadStoredValue(const DSTable* table, const DSFieldMeta& meta, const uint8_t* bytes)
{
    if (meta.m_ChildIndex)
    {
        DSStruct object = {
            .m_Count = meta.m_ChildCount,
            .m_Source = DS_STRUCT_INLINE,
            .m_View = { .m_Buffer = bytes, .m_Offset = meta.m_ChildIndex, .m_FieldOffset = meta.m_Offset, .m_Table = table }
        };
        DSValue value = {
            .m_Type = DS_TYPE_STRUCT,
            .m_Value = { .m_Struct = object }
        };
        return value;
    }
    if (meta.m_Type == DS_TYPE_NULL)
    {
        DSValue value = { .m_Type = DS_TYPE_NULL };
        return value;
    }
    if (meta.m_Type != DS_TYPE_STRING && meta.m_Type != DS_TYPE_STRUCT && meta.m_Type != DS_TYPE_LIST)
        return ReadPackedValue(bytes, meta.m_Type, 0);
    uint64_t reference = ReadDataInteger(bytes, 8);
    if (meta.m_Type == DS_TYPE_STRUCT && IsStructRow(reference))
    {
        DSValue value = { .m_Type = DS_TYPE_STRUCT, .m_Value = { .m_Struct = ReadStructRow(table, reference) } };
        return value;
    }
    return (reference & 1) ? ReadPackedValue(table->m_Blob, meta.m_Type, (uint32_t)(reference >> 1)) :
                             ReadPackedValue((const uint8_t*)(uintptr_t)reference, meta.m_Type, 0);
}

static bool IsReference(DSValueType type)
{
    return type == DS_TYPE_STRING || type == DS_TYPE_STRUCT || type == DS_TYPE_LIST;
}

// Interpret caller-owned native input without allocating temporary value trees.
static DSValue ReadNativeInput(DSValueType type, const DSStructDesc* layout, const uint8_t* bytes)
{
    DSValue value = { .m_Type = type };
    if (layout)
    {
        value.m_Value.m_Struct.m_Source = DS_STRUCT_NATIVE;
        value.m_Value.m_Struct.m_Input.m_Layout = layout;
        value.m_Value.m_Struct.m_Input.m_Data = bytes;
        value.m_Value.m_Struct.m_Count = layout->m_FieldCount;
    }
    else if (IsReference(type))
    {
        DSReference reference;
        memcpy(&reference, bytes, sizeof(reference));
        switch (type)
        {
            case DS_TYPE_STRING:
                value.m_Value.m_String = reference.m_String;
                break;
            case DS_TYPE_STRUCT:
                if (!reference.m_Struct || !reference.m_Struct->m_Layout)
                    value.m_Type = (DSValueType)UINT32_MAX;
                else
                    return ReadNativeInput(type, reference.m_Struct->m_Layout, (const uint8_t*)reference.m_Struct->m_Values);
                break;
            case DS_TYPE_LIST:
                if (!reference.m_List)
                    value.m_Type = (DSValueType)UINT32_MAX;
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
    else if (type != DS_TYPE_NULL)
        memcpy(&value.m_Value, bytes, DSTypeSize(type));
    return value;
}

uint64_t GetChildName(const DSStruct* object, uint32_t index)
{
    if (object->m_Source == DS_STRUCT_NATIVE)
        return object->m_Input.m_Layout->m_Fields[index].m_Field;
    if (object->m_Source == DS_STRUCT_ROW)
        return object->m_View.m_StructTable->m_Fields[index].m_Name;
    if (object->m_Source == DS_STRUCT_INLINE)
        return GetFieldMeta(object->m_View.m_Table, object->m_View.m_Offset + index).m_Name;
    return object->m_Source == DS_STRUCT_PACKED ? ReadDataInteger(object->m_View.m_Buffer + object->m_View.m_Offset + sizeof(DSFileContainerHeader) + index * 8, 8) : object->m_Array.m_Names[index];
}

DSValueType GetStructFieldType(const DSStruct* object, uint32_t index)
{
    if (object->m_Source == DS_STRUCT_NATIVE)
        return object->m_Input.m_Layout->m_Fields[index].m_Type;
    if (object->m_Source == DS_STRUCT_ROW)
        return object->m_View.m_StructTable->m_Fields[index].m_Type;
    if (object->m_Source == DS_STRUCT_INLINE)
        return GetFieldMeta(object->m_View.m_Table, object->m_View.m_Offset + index).m_Type;
    if (object->m_Source == DS_STRUCT_PACKED)
        return (DSValueType)ReadDataInteger(object->m_View.m_Buffer + object->m_View.m_Offset + sizeof(DSFileContainerHeader) + object->m_Count * 8 + index * 4, 4);
    return object->m_Array.m_Types[index];
}

DSValue GetChildValue(DSValueType type, const DSValueData* value, uint32_t index)
{
    bool            named = type == DS_TYPE_STRUCT;
    const DSStruct* object = &value->m_Struct;
    if (named && object->m_Source == DS_STRUCT_NATIVE)
    {
        const DSFieldDesc&   field = object->m_Input.m_Layout->m_Fields[index];
        const uint8_t*       bytes = object->m_Input.m_Data ? object->m_Input.m_Data + field.m_Offset : 0;
        return ReadNativeInput(field.m_Type, field.m_Struct, bytes);
    }
    if (!named && value->m_List.m_Input)
    {
        const DSListInput* input = value->m_List.m_Input;
        if (input->m_Types)
        {
            DSValueType   type = input->m_Types[index];
            const void*   payload = ((const void* const*)input->m_Values)[index];
            uint32_t      size = DSTypeSize(type);
            if (size == UINT32_MAX || (size && !payload))
            {
                DSValue invalid = { .m_Type = (DSValueType)UINT32_MAX };
                return invalid;
            }
            DSReference reference = {};
            switch (type)
            {
                case DS_TYPE_STRING:
                    reference.m_String = (const char*)payload;
                    break;
                case DS_TYPE_STRUCT:
                    reference.m_Struct = (const DSStructInput*)payload;
                    break;
                case DS_TYPE_LIST:
                    reference.m_List = (const DSListInput*)payload;
                    break;
                default:
                    return ReadNativeInput(type, 0, (const uint8_t*)payload);
            }
            return ReadNativeInput(type, 0, (const uint8_t*)&reference);
        }
        uint32_t       stride = DSTypeSize(input->m_Type);
        const uint8_t* bytes = stride ? (const uint8_t*)input->m_Values + (size_t)index * stride : 0;
        return ReadNativeInput(input->m_Type, 0, bytes);
    }
    if (named && object->m_Source == DS_STRUCT_ROW)
    {
        const DSStructTable*   child_table = object->m_View.m_StructTable;
        const DSStructField&   field = child_table->m_Fields[index];
        DSFieldMeta            meta = { .m_Type = field.m_Type, .m_Size = DSTypeSize(field.m_Type) };
        const uint8_t*         bytes = child_table->m_Values.Begin() + (size_t)object->m_View.m_Offset * child_table->m_RowStride + field.m_Offset;
        return ReadStoredValue(object->m_View.m_Table, meta, bytes);
    }
    if (named && object->m_Source == DS_STRUCT_INLINE)
    {
        DSFieldMeta    meta = GetFieldMeta(object->m_View.m_Table, object->m_View.m_Offset + index);
        const uint8_t* bytes = meta.m_Size ? object->m_View.m_Buffer + (meta.m_Offset - object->m_View.m_FieldOffset) : 0;
        return ReadStoredValue(object->m_View.m_Table, meta, bytes);
    }
    if (named && object->m_Source == DS_STRUCT_ARRAY)
    {
        DSValue child = { .m_Type = object->m_Array.m_Types[index], .m_Value = object->m_Array.m_Values[index] };
        return child;
    }
    if (!named && !value->m_List.m_Buffer)
        return value->m_List.m_Values[index];
    const uint8_t* buffer = named ? value->m_Struct.m_View.m_Buffer : value->m_List.m_Buffer;
    uint32_t       offset = named ? value->m_Struct.m_View.m_Offset : value->m_List.m_Offset;
    uint32_t       count = named ? value->m_Struct.m_Count : value->m_List.m_Count;
    const uint8_t* types = buffer + offset + sizeof(DSFileContainerHeader) + (named ? count * 8 : 0);
    DSValueType    child_type = (DSValueType)ReadDataInteger(types + index * 4, 4);
    uint32_t       child = (uint32_t)ReadDataInteger(types + count * 4 + index * 4, 4);
    return ReadPackedValue(buffer, child_type, child);
}

DSResult DSGetStructField(const DSStruct* object, uint64_t field, DSValue* out_value)
{
    for (uint32_t i = 0; i < object->m_Count; ++i)
    {
        if (GetChildName(object, i) == field)
        {
            DSValue value = {
                .m_Type = DS_TYPE_STRUCT,
                .m_Value = { .m_Struct = *object }
            };
            *out_value = GetChildValue(value.m_Type, &value.m_Value, i);
            return DS_RESULT_OK;
        }
    }
    return DS_RESULT_NOT_FOUND;
}

DSResult DSGetListValue(const DSList* list, uint32_t index, DSValue* out_value)
{
    if (index >= list->m_Count)
        return DS_RESULT_NOT_FOUND;
    DSValue value = {
        .m_Type = DS_TYPE_LIST,
        .m_Value = { .m_List = *list }
    };
    *out_value = GetChildValue(value.m_Type, &value.m_Value, index);
    return DS_RESULT_OK;
}

static void StoreScalar(uint8_t* bytes, DSValueType type, const DSValueData* value)
{
    switch (type)
    {
        case DS_TYPE_NUMBER:
        {
            uint64_t bits;
            memcpy(&bits, &value->m_Number, 8);
            WriteDataInteger(bytes, bits, 8);
            break;
        }
        case DS_TYPE_FLOAT:
        case DS_TYPE_UINT32:
            memcpy(bytes, value, 4);
            break;
        case DS_TYPE_BOOLEAN:
            *bytes = value->m_Boolean;
            break;
        case DS_TYPE_VECTOR3:
            memcpy(bytes, value->m_Vector3, sizeof(value->m_Vector3));
            break;
        case DS_TYPE_VECTOR4:
            memcpy(bytes, value->m_Vector4, sizeof(value->m_Vector4));
            break;
        case DS_TYPE_MATRIX4:
            memcpy(bytes, value->m_Matrix4, sizeof(value->m_Matrix4));
            break;
        default:
            break;
    }
}

// Owned containers share the packed view layout. Child references are 32-bit
// offsets from the root, so no DSValue arrays or child pointers are retained.
static size_t StoreOwnedValue(uint8_t* buffer, size_t offset, DSValueType type, const DSValueData* value)
{
    uint8_t* bytes = buffer + offset;
    if (type == DS_TYPE_STRUCT || type == DS_TYPE_LIST)
    {
        bool     named = type == DS_TYPE_STRUCT;
        uint32_t count = named ? value->m_Struct.m_Count : value->m_List.m_Count;
        uint8_t* types = bytes + sizeof(DSFileContainerHeader) + (named ? count * 8 : 0);
        uint8_t* offsets = types + count * 4;
        WriteDataInteger(bytes + offsetof(DSFileContainerHeader, m_ByteSize), 0, 4);
        WriteDataInteger(bytes + offsetof(DSFileContainerHeader, m_Count), count, 4);
        offset += sizeof(DSFileContainerHeader) + (size_t)count * (named ? 16 : 8);
        for (uint32_t i = 0; i < count; ++i)
        {
            DSValue child = GetChildValue(type, value, i);
            if (named)
                WriteDataInteger(bytes + sizeof(DSFileContainerHeader) + i * 8, GetChildName(&value->m_Struct, i), 8);
            WriteDataInteger(types + i * 4, child.m_Type, 4);
            WriteDataInteger(offsets + i * 4, offset, 4);
            offset = StoreOwnedValue(buffer, offset, child.m_Type, &child.m_Value);
        }
        return offset;
    }
    size_t size = DSTypeSize(type);
    if (type == DS_TYPE_STRING)
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

static size_t MeasureOwnedValue(DSValueType type, const DSValueData* value)
{
    if (type == DS_TYPE_STRING)
        return (strlen(value->m_String) + 8) & ~(size_t)7;
    if (type != DS_TYPE_STRUCT && type != DS_TYPE_LIST)
        return (DSTypeSize(type) + 7) & ~7u;
    bool     named = type == DS_TYPE_STRUCT;
    uint32_t count = named ? value->m_Struct.m_Count : value->m_List.m_Count;
    size_t   size = sizeof(DSFileContainerHeader) + (size_t)count * (named ? 16 : 8);
    for (uint32_t i = 0; i < count; ++i)
    {
        DSValue child = GetChildValue(type, value, i);
        size += MeasureOwnedValue(child.m_Type, &child.m_Value);
    }
    return size;
}

// Callers validate and reserve the complete payload before writing. Blocks never
// move, so values borrowed from this same table remain valid during insertion/set.
void StoreValue(DSPayloadBlock** blocks, uint8_t* bytes, DSValueType type, const DSValueData* value)
{
    if (!IsReference(type))
    {
        StoreScalar(bytes, type, value);
        return;
    }

    uint8_t* buffer = (uint8_t*)AllocatePayload(blocks, MeasureOwnedValue(type, value));
    StoreOwnedValue(buffer, 0, type, value);
    WriteDataInteger(bytes, (uintptr_t)buffer, 8);
}

DSValue ReadBoundValue(const DSTable* table, const DSRow* row, const DSFieldMeta& meta)
{
    const uint8_t* bytes = meta.m_Size ? GetFieldBytes(table, row, meta.m_Offset) : 0;
    return ReadStoredValue(table, meta, bytes);
}

DSValue ReadRowValue(const DSTable* table, const DSRow* row, uint32_t field_index)
{
    return ReadBoundValue(table, row, GetFieldMeta(table, field_index));
}

// Tag only the mutable copy. Fixed structs recurse through shared metadata;
// strings and dynamic containers continue to borrow their original blob payloads.
static void TagBlobReferences(const DSTable* table, const DSFieldMeta& meta, uint8_t* bytes, uint32_t rows)
{
    if (meta.m_ChildIndex)
    {
        for (uint32_t i = 0; i < meta.m_ChildCount; ++i)
        {
            DSFieldMeta child = GetFieldMeta(table, meta.m_ChildIndex + i);
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

void InitializeBlobValues(DSTable* table, uint32_t start, uint32_t count)
{
    if (!table->m_RowStride || !count)
        return;

    for (uint32_t first = 0; first < count;)
    {
        uint32_t span = GetRowSpan(table, start + first, count - first);
        uint8_t* bytes = GetRowBytes(table, start + first);
        memcpy(bytes, table->m_Blob + table->m_Offsets.m_Rows + (size_t)first * table->m_RowStride, (size_t)span * table->m_RowStride);
        if (table->m_HasReferences)
        {
            for (uint32_t i = 0; i < table->m_FieldCount; ++i)
            {
                DSFieldMeta meta = GetFieldMeta(table, i);
                TagBlobReferences(table, meta, bytes + meta.m_Offset, span);
            }
        }
        first += span;
    }
}

static const uint8_t* GetDefaultRow(const DSTable* table, const DSRow* row)
{
    if (table->m_Pool)
        return table->m_Blob + table->m_Offsets.m_Rows + (size_t)row->m_BaseIndex * table->m_RowStride;
    return GetNativeDefaultRow(table, row->m_BaseIndex);
}

void ReleaseReference(DSTable* table, DSValueType type, uint64_t reference)
{
    if (reference & 1) // Immutable blob offset, never owned by this row.
        return;
    if (type == DS_TYPE_STRUCT)
        ReleaseStructRow(table, reference);
    else
        ReleasePayload((void*)(uintptr_t)reference);
}

// Current values initially borrow their reset defaults. Release a replacement
// once; removal also releases the default, or the sole value without history.
static void ReleaseFieldValues(DSTable* table, DSRow* row, const DSFieldMeta& meta, bool defaults)
{
    if (!IsReference(meta.m_Type))
        return;
    if (meta.m_ChildIndex)
    {
        for (uint32_t i = 0; i < meta.m_ChildCount; ++i)
            ReleaseFieldValues(table, row, GetFieldMeta(table, meta.m_ChildIndex + i), defaults);
        return;
    }
    uint64_t current = ReadDataInteger(GetFieldBytes(table, row, meta.m_Offset), 8);
    uint64_t base = table->m_RetainDefaults ? ReadDataInteger(GetDefaultRow(table, row) + meta.m_Offset, 8) : 0;
    if (table->m_Pool || !table->m_RetainDefaults || current != base)
        ReleaseReference(table, meta.m_Type, current);
    if (defaults && !table->m_Pool && table->m_RetainDefaults)
        ReleaseReference(table, meta.m_Type, base);
}

void ReleaseRowValues(DSTable* table, uint32_t row, bool defaults)
{
    for (uint32_t i = 0; i < table->m_FieldCount; ++i)
        ReleaseFieldValues(table, &table->m_Rows[row], GetFieldMeta(table, i), defaults);
}

static void ResetRow(DSTable* table, DSRow* row)
{
    if (!table->m_RowStride)
        return;
    if (table->m_HasReferences)
        ReleaseRowValues(table, (uint32_t)(row - table->m_Rows.Begin()), false);
    uint8_t* bytes = GetFieldBytes(table, row, 0);
    memcpy(bytes, GetDefaultRow(table, row), table->m_RowStride);
    if (table->m_Pool && table->m_HasReferences)
    {
        for (uint32_t i = 0; i < table->m_FieldCount; ++i)
        {
            DSFieldMeta meta = GetFieldMeta(table, i);
            TagBlobReferences(table, meta, bytes + meta.m_Offset, 1);
        }
    }
}

// Measure arena payloads during validation. Struct fields use separate child
// rows; lists retain packed trees with 32-bit offsets.
static DSResult ValidateValue(DSValueType type, const DSValueData* value, uint32_t depth, size_t* out_size, bool packed = false)
{
    uint32_t size = DSTypeSize(type);
    if (size == UINT32_MAX)
        return DS_RESULT_INVALID_ARGUMENT;
    *out_size = ((size_t)size + 7) & ~(size_t)7;
    if (type == DS_TYPE_BOOLEAN)
        return value->m_Boolean <= 1 ? DS_RESULT_OK : DS_RESULT_INVALID_ARGUMENT;
    if (type == DS_TYPE_STRING)
    {
        if (!value->m_String)
            return DS_RESULT_INVALID_ARGUMENT;
        size_t length = strlen(value->m_String);
        if (length > UINT32_MAX - 8 - (packed ? 0 : sizeof(DSPayloadHeader)))
            return DS_RESULT_INVALID_ARGUMENT;
        *out_size = ((length + 8) & ~(size_t)7) + (packed ? 0 : sizeof(DSPayloadHeader));
        return DS_RESULT_OK;
    }
    if (type != DS_TYPE_STRUCT && type != DS_TYPE_LIST)
        return DS_RESULT_OK;
    bool           named = type == DS_TYPE_STRUCT;
    uint32_t       count = named ? value->m_Struct.m_Count : value->m_List.m_Count;
    bool           native = named ? value->m_Struct.m_Source == DS_STRUCT_NATIVE : value->m_List.m_Input != 0;
    bool           arrays = named ? value->m_Struct.m_Source == DS_STRUCT_ARRAY : !native && !value->m_List.m_Buffer;
    bool           missing_input = arrays && (named ? (!value->m_Struct.m_Array.m_Names || !value->m_Struct.m_Array.m_Types || !value->m_Struct.m_Array.m_Values) : !value->m_List.m_Values);
    if (depth == DATA_MAX_NESTING || count > UINT32_MAX / (named ? sizeof(DSValueData) : sizeof(DSValue)) ||
        (count && missing_input))
        return DS_RESULT_INVALID_ARGUMENT;
    if (named && count > DATA_MAX_METADATA_COUNT)
        return DS_RESULT_INVALID_ARGUMENT;
    if (native && named)
    {
        const DSStructDesc* layout = value->m_Struct.m_Input.m_Layout;
        if (layout->m_Size && !value->m_Struct.m_Input.m_Data)
            return DS_RESULT_INVALID_ARGUMENT;
        uint64_t   metadata_count = 0;
        uint32_t   alignment;
        DSResult   result = ValidateLayout(layout->m_Fields, layout->m_FieldCount, layout->m_Size, 0, &metadata_count, &alignment);
        if (result != DS_RESULT_OK)
            return result;
    }
    else if (native)
    {
        const DSListInput* input = value->m_List.m_Input;
        uint32_t           stride = input->m_Types ? sizeof(void*) : DSTypeSize(input->m_Type);
        if (stride == UINT32_MAX || (stride && count && !input->m_Values) || (uint64_t)stride * count > SIZE_MAX)
            return DS_RESULT_INVALID_ARGUMENT;
    }
    uint64_t total = named && !packed ? 0 : sizeof(DSFileContainerHeader) + (uint64_t)count * (named ? 16 : 8);
    for (uint32_t i = 0; i < count; ++i)
    {
        DSValue    child = GetChildValue(type, value, i);
        size_t     child_size;
        DSResult   result = ValidateValue(child.m_Type, &child.m_Value, depth + 1, &child_size, packed || !named);
        if (result != DS_RESULT_OK)
            return result;
        if (packed || !named || IsReference(child.m_Type))
            total += child_size;
        if (total > UINT32_MAX)
            return DS_RESULT_INVALID_ARGUMENT;
        if (named)
        {
            uint64_t name = GetChildName(&value->m_Struct, i);
            for (uint32_t j = 0; j < i; ++j)
                if (GetChildName(&value->m_Struct, j) == name)
                    return DS_RESULT_ALREADY_EXISTS;
        }
    }
    total += !packed && !named ? sizeof(DSPayloadHeader) : 0;
    if (total > SIZE_MAX)
        return DS_RESULT_INVALID_ARGUMENT;
    *out_size = (size_t)total;
    return DS_RESULT_OK;
}

// Validate reference payloads before reserving or publishing any native rows.
DSResult ValidateNativeReferences(const DSTable* table, const DSFieldMeta& meta, const uint8_t* values, uint32_t stride, uint32_t count, uint32_t depth, size_t* payload_size)
{
    if (meta.m_ChildIndex)
    {
        for (uint32_t i = 0; i < meta.m_ChildCount; ++i)
        {
            DSFieldMeta child = GetFieldMeta(table, meta.m_ChildIndex + i);
            DSResult    result = ValidateNativeReferences(table, child, values + (child.m_Offset - meta.m_Offset), stride, count, depth + 1, payload_size);
            if (result != DS_RESULT_OK)
                return result;
        }
    }
    else if (IsReference(meta.m_Type))
    {
        for (uint32_t r = 0; r < count; ++r)
        {
            DSValue    value = ReadNativeInput(meta.m_Type, 0, values + (size_t)r * stride);
            size_t     size;
            DSResult   result = ValidateValue(value.m_Type, &value.m_Value, depth, &size);
            if (result != DS_RESULT_OK)
                return result;
            if (size > SIZE_MAX - sizeof(DSPayloadBlock) - *payload_size)
                return DS_RESULT_INVALID_ARGUMENT;
            *payload_size += size;
        }
    }
    return DS_RESULT_OK;
}

// Native bytes are already copied. Replace only input reference slots with
// owned payload addresses or child-row indices. One arena reservation covers
// strings and lists for the complete batch.
void StoreNativeReferences(DSTable* table, uint32_t count, uint8_t* rows)
{
    for (uint32_t f = 0; f < table->m_MetadataCount; ++f)
    {
        const DSFieldMeta& meta = table->m_Owned->m_Fields[f];
        if (meta.m_ChildIndex || !IsReference(meta.m_Type))
            continue;
        for (uint32_t r = 0; r < count; ++r)
        {
            uint8_t*  bytes = rows + (size_t)r * table->m_RowStride + meta.m_Offset;
            DSValue   value = ReadNativeInput(meta.m_Type, 0, bytes);
            if (meta.m_Type == DS_TYPE_STRUCT)
                WriteDataInteger(bytes, StoreStructRow(table, GetNativePayloads(table), &value.m_Value.m_Struct), 8);
            else
                StoreValue(GetNativePayloads(table), bytes, meta.m_Type, &value.m_Value);
        }
    }
}

// Decoded input commonly follows layout order, but matching still uses names.
static uint32_t FindDecodedField(const DSStruct* object, uint64_t name, uint32_t index)
{
    if (object->m_Array.m_Names[index] == name)
        return index;
    for (uint32_t i = 0; i < object->m_Count; ++i)
        if (object->m_Array.m_Names[i] == name)
            return i;
    return UINT32_MAX;
}

DSResult ValidateStoredValue(const DSTable* table, const DSFieldMeta& meta, DSValueType type, const DSValueData* value, uint32_t depth, size_t* payload_size)
{
    if (type != meta.m_Type || depth > DATA_MAX_NESTING)
        return DS_RESULT_INVALID_ARGUMENT;
    if (!meta.m_ChildIndex)
    {
        // Registration validated fixed kinds and sizes. Only Boolean has a
        // restricted scalar value; references still need payload validation.
        if (!IsReference(type))
            return type != DS_TYPE_BOOLEAN || value->m_Boolean <= 1 ? DS_RESULT_OK : DS_RESULT_INVALID_ARGUMENT;
        size_t     size;
        DSResult   result = ValidateValue(type, value, depth, &size);
        if (result != DS_RESULT_OK)
            return result;
        if (size > SIZE_MAX - sizeof(DSPayloadBlock) - *payload_size)
            return DS_RESULT_INVALID_ARGUMENT;
        *payload_size += size;
        return DS_RESULT_OK;
    }

    const DSStruct* object = &value->m_Struct;
    if (object->m_Count != meta.m_ChildCount || (object->m_Source == DS_STRUCT_ARRAY && object->m_Count && (!object->m_Array.m_Names || !object->m_Array.m_Types || !object->m_Array.m_Values)))
        return DS_RESULT_INVALID_ARGUMENT;

    const DSFieldMeta* fields = table->m_Blob ? 0 : table->m_Owned->m_Fields.Begin() + meta.m_ChildIndex;
    for (uint32_t i = 0; i < meta.m_ChildCount; ++i)
    {
        DSFieldMeta packed_field;
        if (!fields)
            packed_field = GetFieldMeta(table, meta.m_ChildIndex + i);
        const DSFieldMeta& field = fields ? fields[i] : packed_field;
        DSValue            child;
        DSValueType        type;
        const DSValueData* data;
        if (object->m_Source == DS_STRUCT_ARRAY)
        {
            uint32_t index = FindDecodedField(object, field.m_Name, i);
            if (index == UINT32_MAX)
                return DS_RESULT_INVALID_ARGUMENT;
            type = object->m_Array.m_Types[index];
            data = &object->m_Array.m_Values[index];
        }
        else
        {
            if (DSGetStructField(object, field.m_Name, &child) != DS_RESULT_OK)
                return DS_RESULT_INVALID_ARGUMENT;
            type = child.m_Type;
            data = &child.m_Value;
        }

        // Registration bounds the inline depth. Scalar children need no
        // recursive traversal; references retain their payload/depth checks.
        if (type != field.m_Type || (type == DS_TYPE_BOOLEAN && data->m_Boolean > 1))
            return DS_RESULT_INVALID_ARGUMENT;
        if (IsReference(type))
        {
            DSResult result = ValidateStoredValue(table, field, type, data, depth + 1, payload_size);
            if (result != DS_RESULT_OK)
                return result;
        }
    }
    return DS_RESULT_OK;
}

void StoreStoredValue(DSTable* table, const DSFieldMeta& meta, DSPayloadBlock** blocks, uint8_t* bytes, const DSValueData* value)
{
    if (!meta.m_ChildIndex)
    {
        if (meta.m_Type == DS_TYPE_STRUCT)
            WriteDataInteger(bytes, StoreStructRow(table, blocks, &value->m_Struct), 8);
        else
            StoreValue(blocks, bytes, meta.m_Type, value);
        return;
    }

    const DSStruct*    object = &value->m_Struct;
    const DSFieldMeta* fields = table->m_Blob ? 0 : table->m_Owned->m_Fields.Begin() + meta.m_ChildIndex;
    for (uint32_t i = 0; i < meta.m_ChildCount; ++i)
    {
        DSFieldMeta packed_field;
        if (!fields)
            packed_field = GetFieldMeta(table, meta.m_ChildIndex + i);
        const DSFieldMeta&   field = fields ? fields[i] : packed_field;
        uint8_t*             target = field.m_Size ? bytes + (field.m_Offset - meta.m_Offset) : 0;
        DSValue              child;
        const DSValueData*   data;
        if (object->m_Source == DS_STRUCT_ARRAY)
        {
            uint32_t index = FindDecodedField(object, field.m_Name, i);
            data = &object->m_Array.m_Values[index];
        }
        else
        {
            DSGetStructField(object, field.m_Name, &child);
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
DSResult ValidateRowValues(const DSTable* table, const DSRowDesc* rows, uint32_t count, size_t* payload_size)
{
    const DSFieldMeta*   fields = table->m_Owned->m_Fields.Begin();
    uint32_t             field_count = table->m_FieldCount;

    for (uint32_t i = 0; i < count; ++i)
    {
        const DSRowDesc& row = rows[i];
        if (row.m_ValueCount != field_count || (field_count && (!row.m_Types || !row.m_Values)))
            return DS_RESULT_INVALID_ARGUMENT;

        for (uint32_t j = 0; j < field_count; ++j)
        {
            const DSFieldMeta& meta = fields[j];
            DSValueType        type = row.m_Types[j];
            if (type != meta.m_Type || (type == DS_TYPE_BOOLEAN && row.m_Values[j].m_Boolean > 1))
                return DS_RESULT_INVALID_ARGUMENT;
            if (IsReference(type))
            {
                DSResult result = ValidateStoredValue(table, meta, type, &row.m_Values[j], 0, payload_size);
                if (result != DS_RESULT_OK)
                    return result;
            }
        }
    }
    return DS_RESULT_OK;
}

// Values have been validated and payload blocks reserved for the whole batch.
void StoreRowValues(DSTable* table, const DSRowDesc* rows, uint32_t count, uint8_t* bytes)
{
    const DSFieldMeta*   fields = table->m_Owned->m_Fields.Begin();
    uint32_t             field_count = table->m_FieldCount;

    for (uint32_t i = 0; i < count; ++i)
    {
        for (uint32_t j = 0; j < field_count; ++j)
        {
            const DSFieldMeta&   meta = fields[j];
            uint8_t*             target = bytes + meta.m_Offset;
            if (IsReference(meta.m_Type))
                StoreStoredValue(table, meta, GetNativePayloads(table), target, &rows[i].m_Values[j]);
            else
                StoreScalar(target, meta.m_Type, &rows[i].m_Values[j]);
        }

        bytes += table->m_RowStride;
    }
}

DSResult ReadField(DSTable* table, uint32_t row, uint64_t field, DSValue* out_value)
{
    uint32_t index = FindField(table, field);
    if (index == UINT32_MAX)
        return DS_RESULT_NOT_FOUND;
    *out_value = ReadRowValue(table, &table->m_Rows[row], index);
    return DS_RESULT_OK;
}

DSResult DSFieldGet(HDSContext store, DSId id, uint64_t field, DSValue* out_value)
{
    DSSlot* slot = FindSlot(store, id);
    return slot ? ReadField(slot->m_Table, slot->m_Row, field, out_value) : DS_RESULT_NOT_FOUND;
}

// A full-name lookup can start below the root. Include its inline ancestors in
// the same container-depth budget used by row creation and blob validation.
static uint32_t GetInlineDepth(const DSTable* table, uint64_t field)
{
    uint32_t index = FindField(table, field);
    uint32_t depth = 0;
    while (index >= table->m_FieldCount)
    {
        for (uint32_t i = 0; i < index; ++i)
        {
            DSFieldMeta parent = GetFieldMeta(table, i);
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

DSResult SetRowField(DSTable* table, DSRow* row, const DSFieldMeta& meta, const DSValue* value)
{
    size_t     payload_size = 0;
    uint32_t   depth = meta.m_Type == DS_TYPE_STRUCT || meta.m_Type == DS_TYPE_LIST ? GetInlineDepth(table, meta.m_Field) : 0;
    DSResult   result = ValidateStoredValue(table, meta, value->m_Type, &value->m_Value, depth, &payload_size);
    if (result != DS_RESULT_OK)
        return result;

    DSPayloadBlock** payloads = &table->m_Payloads;
    if (payload_size)
    {
        if (table->m_Pool)
            payloads = &GetPoolInstance(table->m_Pool, row->m_InstanceIndex)->m_Payloads;
        ReservePayloads(payloads, payload_size);
    }

    uint8_t* bytes = meta.m_Size ? GetFieldBytes(table, row, meta.m_Offset) : 0;
    if (IsReference(meta.m_Type))
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
        ReleaseFieldValues(table, row, meta, false);
        memcpy(bytes, replacement.Begin(), meta.m_Size);
    }
    else
        StoreStoredValue(table, meta, payloads, bytes, &value->m_Value);
    return DS_RESULT_OK;
}

DSResult DSSetField(HDSContext store, DSId id, uint64_t field, const DSValue* value)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_ActiveQueries.Size())
        return DS_RESULT_LOCKED;
    DSSlot* slot = FindSlot(store, id);
    if (!slot)
        return DS_RESULT_NOT_FOUND;
    DSTable*   table = slot->m_Table;
    uint32_t   index = FindField(table, field);
    if (index == UINT32_MAX)
        return DS_RESULT_NOT_FOUND;
    return SetRowField(table, &table->m_Rows[slot->m_Row], GetFieldMeta(table, index), value);
}

DSResult DSResetField(HDSContext store, DSId id, uint64_t field)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_ActiveQueries.Size())
        return DS_RESULT_LOCKED;
    DSSlot* slot = FindSlot(store, id);
    if (!slot)
        return DS_RESULT_NOT_FOUND;
    DSTable* table = slot->m_Table;
    if (!table->m_RetainDefaults)
        return DS_RESULT_INVALID_ARGUMENT;
    uint32_t   index = FindField(table, field);
    if (index == UINT32_MAX)
        return DS_RESULT_NOT_FOUND;
    DSRow*      row = &table->m_Rows[slot->m_Row];
    DSFieldMeta meta = GetFieldMeta(table, index);
    if (meta.m_Size)
    {
        if (table->m_HasReferences)
            ReleaseFieldValues(table, row, meta, false);
        uint8_t* bytes = GetFieldBytes(table, row, meta.m_Offset);
        memcpy(bytes, GetDefaultRow(table, row) + meta.m_Offset, meta.m_Size);
        if (table->m_Pool)
            TagBlobReferences(table, meta, bytes, 1);
    }
    return DS_RESULT_OK;
}

DSResult DSResetRow(HDSContext store, DSId id)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_ActiveQueries.Size())
        return DS_RESULT_LOCKED;
    DSSlot* slot = FindSlot(store, id);
    if (!slot)
        return DS_RESULT_NOT_FOUND;
    DSTable* table = slot->m_Table;
    DSRow*   row = &table->m_Rows[slot->m_Row];
    if (!table->m_RetainDefaults)
        return DS_RESULT_INVALID_ARGUMENT;
    ResetRow(table, row);
    return DS_RESULT_OK;
}

static void ResetTable(DSTable* table)
{
    for (uint32_t i = 0; i < table->m_Rows.Size(); ++i)
        ResetRow(table, &table->m_Rows[i]);
    DeletePayloads(&table->m_Payloads);
}

DSResult DSResetTable(HDSContext store, uint64_t type)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_ActiveQueries.Size())
        return DS_RESULT_LOCKED;
    DSTable* table = FindTable(store, type);
    if (!table)
        return DS_RESULT_NOT_FOUND;
    if (!table->m_RetainDefaults)
        return DS_RESULT_INVALID_ARGUMENT;
    ResetTable(table);
    return DS_RESULT_OK;
}

DSResult DSResetBlob(HDSBlobInstance instance)
{
    DSBlobPool* pool = instance->m_Pool;
    DM_MUTEX_SCOPED_LOCK(pool->m_Store->m_Mutex);
    if (pool->m_Store->m_ActiveQueries.Size())
        return DS_RESULT_LOCKED;
    const uint32_t* slots = GetInstanceSlots(instance);
    for (uint32_t i = 0; i < pool->m_Blob->m_RowCount; ++i)
    {
        if (slots[i] == INVALID_SLOT)
            continue;
        const DSSlot& slot = pool->m_Store->m_Slots[slots[i]];
        ResetRow(slot.m_Table, &slot.m_Table->m_Rows[slot.m_Row]);
    }
    DeletePayloads(&instance->m_Payloads);
    return DS_RESULT_OK;
}

// Owned and packed metadata share the fields read by the typed lookup.
DM_STATIC_ASSERT(sizeof(DSFieldMeta) == sizeof(DSFileFieldMeta) &&
                 offsetof(DSFieldMeta, m_Field) == offsetof(DSFileFieldMeta, m_NameHash) &&
                 offsetof(DSFieldMeta, m_Type) == offsetof(DSFileFieldMeta, m_Kind) &&
                 offsetof(DSFieldMeta, m_Offset) == offsetof(DSFileFieldMeta, m_ByteOffset),
                 Invalid_typed_lookup_metadata_layout);

template <DSValueType TYPE>
static DSResult ResolveTypedField(HDSContext store, DSId id, uint64_t field, DSTable** out_table, const uint8_t** out_bytes)
{
    DSSlot* slot = FindSlot(store, id);
    if (!slot)
        return DS_RESULT_NOT_FOUND;

    DSTable* table = slot->m_Table;
    // Probe the table's immutable index and verify the full hash. A collision
    // falls back to scanning names; kind/offset are read only for the match.
    const uint8_t* metadata = table->m_FieldMetadata;
    uint32_t       index = table->m_FieldLookup[field & (DATA_FIELD_LOOKUP_SIZE - 1)];
    if (index == UINT16_MAX)
        return DS_RESULT_NOT_FOUND;
    uint64_t name;
    memcpy(&name, metadata + (size_t)index * DATA_FIELD_META_SIZE + offsetof(DSFileFieldMeta, m_NameHash), sizeof(name));
    if (name != field)
    {
        for (index = 0; index < table->m_MetadataCount; ++index)
        {
            memcpy(&name, metadata + (size_t)index * DATA_FIELD_META_SIZE + offsetof(DSFileFieldMeta, m_NameHash), sizeof(name));
            if (name == field)
                break;
        }
        if (index == table->m_MetadataCount)
            return DS_RESULT_NOT_FOUND;
    }

    metadata += (size_t)index * DATA_FIELD_META_SIZE;
    uint32_t kind, offset;
    memcpy(&kind, metadata + offsetof(DSFileFieldMeta, m_Kind), sizeof(kind));
    if (kind != TYPE)
        return DS_RESULT_INVALID_ARGUMENT;
    memcpy(&offset, metadata + offsetof(DSFileFieldMeta, m_ByteOffset), sizeof(offset));

    // Owned rows use the dense slot index; views resolve their external source index.
    const uint8_t* bytes = GetRowBytes(table, slot->m_Row) + offset;
    *out_table = table;
    *out_bytes = bytes;
    return DS_RESULT_OK;
}

template <DSValueType TYPE>
static DSResult GetTypedField(HDSContext store, DSId id, uint64_t field, void* out_value)
{
    DSTable*       table;
    const uint8_t* bytes;
    DSResult       result = ResolveTypedField<TYPE>(store, id, field, &table, &bytes);
    return result == DS_RESULT_OK ? CopyTypedValue<TYPE>(table, bytes, out_value) : result;
}

// A hint only: unsupported compilers still use the staged address/copy loops.
static inline void PrefetchField(const void* address)
{
#if defined(__clang__) || defined(__GNUC__)
    __builtin_prefetch(address, 0, 1);
#endif
}

template <DSValueType TYPE, typename T>
static DSResult GetTypedFields(HDSContext store, uint32_t count, const DSId* ids, uint64_t field, T* values)
{
    const uint32_t batch_size = 64;
    for (uint32_t first = 0; first < count;)
    {
        uint32_t       size = count - first < batch_size ? count - first : batch_size;
        const uint8_t* bytes[batch_size];
        DSTable*       tables[batch_size];

        for (uint32_t i = 0; i < size; ++i)
        {
            uint32_t index = (uint32_t)ids[first + i];
            if (index < store->m_Slots.Size())
                PrefetchField(store->m_Slots.Begin() + index);
        }

        for (uint32_t i = 0; i < size; ++i)
        {
            DSResult result = ResolveTypedField<TYPE>(store, ids[first + i], field, &tables[i], &bytes[i]);
            if (result != DS_RESULT_OK)
                return result;
            PrefetchField(bytes[i]);
        }

        for (uint32_t i = 0; i < size; ++i)
            CopyTypedValue<TYPE>(tables[i], bytes[i], &values[first + i]);
        first += size;
    }
    return DS_RESULT_OK;
}

DSResult DSFieldGetNumber(HDSContext store, DSId id, uint64_t field, double* out_value)
{
    return GetTypedField<DS_TYPE_NUMBER>(store, id, field, out_value);
}

DSResult DSFieldGetNumberBatch(HDSContext store, uint32_t count, const DSId* ids, uint64_t field, double* out_values)
{
    return GetTypedFields<DS_TYPE_NUMBER>(store, count, ids, field, out_values);
}

DSResult DSSetFieldNumber(HDSContext store, DSId id, uint64_t field, double value)
{
    DSValue input = {
        .m_Type = DS_TYPE_NUMBER,
        .m_Value = { .m_Number = value }
    };
    return DSSetField(store, id, field, &input);
}

DSResult DSFieldGetFloat(HDSContext store, DSId id, uint64_t field, float* out_value)
{
    return GetTypedField<DS_TYPE_FLOAT>(store, id, field, out_value);
}

DSResult DSFieldGetFloatBatch(HDSContext store, uint32_t count, const DSId* ids, uint64_t field, float* out_values)
{
    return GetTypedFields<DS_TYPE_FLOAT>(store, count, ids, field, out_values);
}

DSResult DSSetFieldFloat(HDSContext store, DSId id, uint64_t field, float value)
{
    DSValue input = {
        .m_Type = DS_TYPE_FLOAT,
        .m_Value = { .m_Float = value }
    };
    return DSSetField(store, id, field, &input);
}

DSResult DSFieldGetUInt32(HDSContext store, DSId id, uint64_t field, uint32_t* out_value)
{
    return GetTypedField<DS_TYPE_UINT32>(store, id, field, out_value);
}

DSResult DSFieldGetUInt32Batch(HDSContext store, uint32_t count, const DSId* ids, uint64_t field, uint32_t* out_values)
{
    return GetTypedFields<DS_TYPE_UINT32>(store, count, ids, field, out_values);
}

DSResult DSSetFieldUInt32(HDSContext store, DSId id, uint64_t field, uint32_t value)
{
    DSValue input = {
        .m_Type = DS_TYPE_UINT32,
        .m_Value = { .m_UInt32 = value }
    };
    return DSSetField(store, id, field, &input);
}

DSResult DSFieldGetBoolean(HDSContext store, DSId id, uint64_t field, uint8_t* out_value)
{
    return GetTypedField<DS_TYPE_BOOLEAN>(store, id, field, out_value);
}

DSResult DSFieldGetBooleanBatch(HDSContext store, uint32_t count, const DSId* ids, uint64_t field, uint8_t* out_values)
{
    return GetTypedFields<DS_TYPE_BOOLEAN>(store, count, ids, field, out_values);
}

DSResult DSSetFieldBoolean(HDSContext store, DSId id, uint64_t field, uint8_t value)
{
    DSValue input = {
        .m_Type = DS_TYPE_BOOLEAN,
        .m_Value = { .m_Boolean = value }
    };
    return DSSetField(store, id, field, &input);
}

DSResult DSFieldGetString(HDSContext store, DSId id, uint64_t field, const char** out_value)
{
    return GetTypedField<DS_TYPE_STRING>(store, id, field, out_value);
}

DSResult DSFieldGetStringBatch(HDSContext store, uint32_t count, const DSId* ids, uint64_t field, const char** out_values)
{
    return GetTypedFields<DS_TYPE_STRING>(store, count, ids, field, out_values);
}

DSResult DSSetFieldString(HDSContext store, DSId id, uint64_t field, const char* value)
{
    DSValue input = {
        .m_Type = DS_TYPE_STRING,
        .m_Value = { .m_String = value }
    };
    return DSSetField(store, id, field, &input);
}

DSResult DSFieldGetVector3(HDSContext store, DSId id, uint64_t field, DSVector3* out_value)
{
    return GetTypedField<DS_TYPE_VECTOR3>(store, id, field, out_value->m_Values);
}

DSResult DSFieldGetVector3Batch(HDSContext store, uint32_t count, const DSId* ids, uint64_t field, DSVector3* out_values)
{
    return GetTypedFields<DS_TYPE_VECTOR3>(store, count, ids, field, out_values);
}

DSResult DSSetFieldVector3(HDSContext store, DSId id, uint64_t field, const DSVector3* value)
{
    DSValue input = { .m_Type = DS_TYPE_VECTOR3 };
    memcpy(input.m_Value.m_Vector3, value->m_Values, sizeof(value->m_Values));
    return DSSetField(store, id, field, &input);
}

DSResult DSFieldGetVector4(HDSContext store, DSId id, uint64_t field, DSVector4* out_value)
{
    return GetTypedField<DS_TYPE_VECTOR4>(store, id, field, out_value->m_Values);
}

DSResult DSFieldGetVector4Batch(HDSContext store, uint32_t count, const DSId* ids, uint64_t field, DSVector4* out_values)
{
    return GetTypedFields<DS_TYPE_VECTOR4>(store, count, ids, field, out_values);
}

DSResult DSSetFieldVector4(HDSContext store, DSId id, uint64_t field, const DSVector4* value)
{
    DSValue input = { .m_Type = DS_TYPE_VECTOR4 };
    memcpy(input.m_Value.m_Vector4, value->m_Values, sizeof(value->m_Values));
    return DSSetField(store, id, field, &input);
}

DSResult DSFieldGetMatrix4(HDSContext store, DSId id, uint64_t field, DSMatrix4* out_value)
{
    return GetTypedField<DS_TYPE_MATRIX4>(store, id, field, out_value->m_Values);
}

DSResult DSFieldGetMatrix4Batch(HDSContext store, uint32_t count, const DSId* ids, uint64_t field, DSMatrix4* out_values)
{
    return GetTypedFields<DS_TYPE_MATRIX4>(store, count, ids, field, out_values);
}

DSResult DSSetFieldMatrix4(HDSContext store, DSId id, uint64_t field, const DSMatrix4* value)
{
    DSValue input = { .m_Type = DS_TYPE_MATRIX4 };
    memcpy(input.m_Value.m_Matrix4, value->m_Values, sizeof(value->m_Values));
    return DSSetField(store, id, field, &input);
}

// Resolve full names once while preparing a query.
bool ResolveField(const DSTable* table, uint64_t field, DSValueType type, DSFieldMeta* out_meta)
{
    uint32_t index = FindField(table, field);
    if (index == UINT32_MAX)
        return false;
    DSFieldMeta meta = GetFieldMeta(table, index);
    if (meta.m_Type != type)
        return false;
    *out_meta = meta;
    return true;
}
