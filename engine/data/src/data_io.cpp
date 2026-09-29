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
#include <dlib/hash.h>
#include <dlib/static_assert.h>
#include <dlib/hashtable.h>
#include "data.h"

// The fixed layouts must match the serialized format.
DM_STATIC_ASSERT(sizeof(DataFileHeader) == 8 && offsetof(DataFileHeader, m_Version) == 4, Invalid_data_file_header);
DM_STATIC_ASSERT(sizeof(DataFileDirectory) == 8 && offsetof(DataFileDirectory, m_StringsOffset) == 4, Invalid_data_file_directory);
DM_STATIC_ASSERT(sizeof(DataTableHeader) == 24 && offsetof(DataTableHeader, m_TagCount) == 8 && offsetof(DataTableHeader, m_Reserved) == 22, Invalid_data_table_header);
DM_STATIC_ASSERT(sizeof(DataFileFieldMeta) == 24 && offsetof(DataFileFieldMeta, m_Kind) == 8 && offsetof(DataFileFieldMeta, m_ByteSize) == 20, Invalid_data_file_field_meta);
DM_STATIC_ASSERT(sizeof(DataFileContainerHeader) == 8 && offsetof(DataFileContainerHeader, m_Count) == 4, Invalid_data_file_container_header);
static const uint32_t INVALID_STRING = UINT32_MAX;

struct PackedString
{
    const char* m_String;
    uint32_t    m_Size;
    uint32_t    m_Offset;
    uint32_t    m_Next;
};

struct PackedStrings
{
    dmHashTable64<uint32_t> m_Buckets;
    dmArray<PackedString>   m_Entries;
    uint32_t                m_Size;
};

static uint64_t Align8(uint64_t offset)
{
    return (offset + 7) & ~(uint64_t)7;
}

static const PackedString* FindString(const PackedStrings* strings, const char* string, uint32_t size, uint64_t hash)
{
    const uint32_t* head = strings->m_Buckets.Get(hash);
    for (uint32_t index = head ? *head : INVALID_STRING; index != INVALID_STRING; index = strings->m_Entries[index].m_Next)
    {
        const PackedString* entry = &strings->m_Entries[index];
        // Hashes only select a bucket; distinct strings must never alias on collision.
        if (entry->m_Size == size && memcmp(entry->m_String, string, size) == 0)
            return entry;
    }
    return 0;
}

static bool AddString(PackedStrings* strings, const char* string)
{
    size_t size = strlen(string) + 1;
    if (size > UINT32_MAX)
        return false;

    uint64_t hash = dmHashBufferNoReverse64(string, (uint32_t)size);
    if (FindString(strings, string, (uint32_t)size, hash))
        return true;

    const uint32_t max_count = UINT32_MAX / sizeof(PackedString);
    if (size > UINT32_MAX - strings->m_Size || strings->m_Entries.Size() == max_count)
        return false;
    if (strings->m_Entries.Full())
    {
        uint32_t capacity = strings->m_Entries.Capacity();
        capacity = capacity > max_count / 2 ? max_count : (capacity ? capacity * 2 : 16);
        strings->m_Entries.SetCapacity(capacity);
        strings->m_Buckets.SetCapacity(capacity);
    }

    const uint32_t* head = strings->m_Buckets.Get(hash);
    PackedString    entry = {
           .m_String = string,
           .m_Size = (uint32_t)size,
           .m_Offset = strings->m_Size,
           .m_Next = head ? *head : INVALID_STRING
    };
    strings->m_Buckets.Put(hash, strings->m_Entries.Size());
    strings->m_Entries.Push(entry);
    strings->m_Size += (uint32_t)size;
    return true;
}

static uint32_t ChildCount(const DataValue* value)
{
    return value->m_Type == DATA_VALUE_TYPE_STRUCT ? value->m_Value.m_Struct.m_Count : value->m_Value.m_List.m_Count;
}

static bool IsContainer(DataValueType type)
{
    return type == DATA_VALUE_TYPE_STRUCT || type == DATA_VALUE_TYPE_LIST;
}

static bool IsReference(DataValueType type)
{
    return type == DATA_VALUE_TYPE_STRING || IsContainer(type);
}

static uint64_t ValueSize(const DataValue* value)
{
    if (value->m_Type == DATA_VALUE_TYPE_STRING || value->m_Type == DATA_VALUE_TYPE_NULL)
        return 0;
    if (!IsContainer(value->m_Type))
        return DataTypeSize(value->m_Type);
    uint32_t count = ChildCount(value);
    uint64_t size = sizeof(DataFileContainerHeader) + (uint64_t)count * (value->m_Type == DATA_VALUE_TYPE_STRUCT ? 16 : 8);
    for (uint32_t i = 0; i < count; ++i)
    {
        DataValue child = GetChildValue(value->m_Type, &value->m_Value, i);
        uint64_t  child_size = ValueSize(&child);
        if (child_size)
            size = Align8(size) + child_size;
        if (size > UINT32_MAX)
            break;
    }
    return Align8(size);
}

static bool AddValueStrings(PackedStrings* strings, const DataValue* value)
{
    if (value->m_Type == DATA_VALUE_TYPE_STRING)
        return AddString(strings, value->m_Value.m_String);
    if (IsContainer(value->m_Type))
    {
        for (uint32_t i = 0; i < ChildCount(value); ++i)
        {
            DataValue child = GetChildValue(value->m_Type, &value->m_Value, i);
            if (!AddValueStrings(strings, &child))
                return false;
        }
    }
    return true;
}

static void WriteScalar(const DataValue* value, uint8_t* bytes)
{
    if (value->m_Type == DATA_VALUE_TYPE_NUMBER)
    {
        uint64_t bits;
        memcpy(&bits, &value->m_Value.m_Number, 8);
        WriteDataInteger(bytes, bits, 8);
    }
    else if (value->m_Type == DATA_VALUE_TYPE_BOOLEAN)
        *bytes = value->m_Value.m_Boolean;
    else if (value->m_Type == DATA_VALUE_TYPE_VECTOR3 || value->m_Type == DATA_VALUE_TYPE_VECTOR4 || value->m_Type == DATA_VALUE_TYPE_MATRIX4)
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
    }
}

// Appends a nested value, writing its absolute offset to a row slot or container index.
static uint32_t WriteValue(const DataValue* value, uint8_t* file, uint32_t offset, uint8_t* out_offset, uint32_t offset_size, uint32_t string_offset, const PackedStrings* strings)
{
    if (value->m_Type == DATA_VALUE_TYPE_NULL)
    {
        WriteDataInteger(out_offset, 0, offset_size);
        return offset;
    }
    if (value->m_Type == DATA_VALUE_TYPE_STRING)
    {
        const char*         string = value->m_Value.m_String;
        uint32_t            size = (uint32_t)strlen(string) + 1;
        const PackedString* entry = FindString(strings, string, size, dmHashBufferNoReverse64(string, size));
        assert(entry);
        WriteDataInteger(out_offset, (uint64_t)string_offset + entry->m_Offset, offset_size);
        return offset;
    }
    offset = (uint32_t)Align8(offset);
    WriteDataInteger(out_offset, offset, offset_size);
    if (!IsContainer(value->m_Type))
    {
        WriteScalar(value, file + offset);
        return offset + DataTypeSize(value->m_Type);
    }
    uint32_t start = offset;
    uint8_t* data = file + start;
    uint32_t count = ChildCount(value);
    bool     named = value->m_Type == DATA_VALUE_TYPE_STRUCT;
    uint32_t types = sizeof(DataFileContainerHeader) + (named ? count * 8 : 0);
    uint32_t offsets = types + count * 4;
    offset += offsets + count * 4;
    WriteDataInteger(data + offsetof(DataFileContainerHeader, m_Count), count, 4);
    for (uint32_t i = 0; i < count; ++i)
    {
        DataValue child = GetChildValue(value->m_Type, &value->m_Value, i);
        if (named)
            WriteDataInteger(data + sizeof(DataFileContainerHeader) + i * 8, GetChildName(&value->m_Value.m_Struct, i), 8);
        WriteDataInteger(data + types + i * 4, child.m_Type, 4);
        offset = WriteValue(&child, file, offset, data + offsets + i * 4, 4, string_offset, strings);
    }
    offset = (uint32_t)Align8(offset);
    WriteDataInteger(data + offsetof(DataFileContainerHeader, m_ByteSize), offset - start, 4);
    return offset;
}

// Inline members occupy the parent's row bytes; only dynamic children add payloads.
static uint64_t FieldPayloadSize(const DataTable* table, const DataFieldMeta& meta, const DataValue* value)
{
    if (!meta.m_ChildIndex)
        return IsContainer(value->m_Type) ? ValueSize(value) : 0;
    uint64_t size = 0;
    for (uint32_t i = 0; i < meta.m_ChildCount; ++i)
    {
        DataValue child = GetChildValue(value->m_Type, &value->m_Value, i);
        size += FieldPayloadSize(table, GetFieldMeta(table, meta.m_ChildIndex + i), &child);
    }
    return size;
}

static uint32_t WriteField(const DataTable* table, const DataFieldMeta& meta, const DataValue* value, uint8_t* field, uint8_t* file, uint32_t offset, uint32_t strings_offset, const PackedStrings* strings)
{
    if (meta.m_ChildIndex)
    {
        for (uint32_t i = 0; i < meta.m_ChildCount; ++i)
        {
            DataFieldMeta member = GetFieldMeta(table, meta.m_ChildIndex + i);
            DataValue     child = GetChildValue(value->m_Type, &value->m_Value, i);
            offset = WriteField(table, member, &child, field + member.m_Offset, file, offset, strings_offset, strings);
        }
    }
    else if (IsReference(value->m_Type))
        offset = WriteValue(value, file, offset, field, 8, strings_offset, strings);
    else
        WriteScalar(value, field);
    return offset;
}

DataResult DataWriteBlob(HDataStore store, void* buffer, uint32_t buffer_size, uint32_t* out_size)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_ActiveQueries.Size())
        return DATA_RESULT_LOCKED;

    PackedStrings strings;
    strings.m_Size = 0;
    uint32_t tables = store->m_Tables.Size();
    uint64_t string_offset = Align8(DATA_TABLE_OFFSETS_OFFSET + (uint64_t)tables * sizeof(uint32_t));
    for (uint32_t t = 0; t < tables; ++t)
    {
        const DataTable* table = store->m_Tables[t];
        uint32_t         rows = table->m_Rows.Size();
        string_offset = Align8(string_offset + DATA_TABLE_HEADER_SIZE + (uint64_t)table->m_TagCount * 8 +
                               (uint64_t)table->m_MetadataCount * DATA_FIELD_META_SIZE + (uint64_t)rows * 8 + (uint64_t)rows * table->m_RowStride);
        for (uint32_t r = 0; r < rows; ++r)
        {
            for (uint32_t i = 0; i < table->m_FieldCount; ++i)
            {
                DataValue value = ReadRowValue(table, &table->m_Rows[r], i);
                string_offset += FieldPayloadSize(table, GetFieldMeta(table, i), &value);
                if (string_offset > UINT32_MAX || !AddValueStrings(&strings, &value))
                    return DATA_RESULT_INVALID_ARGUMENT;
            }
        }
        if (string_offset > UINT32_MAX)
            return DATA_RESULT_INVALID_ARGUMENT;
    }

    uint64_t size = string_offset + strings.m_Size;
    if (size > UINT32_MAX || (!buffer && buffer_size))
        return DATA_RESULT_INVALID_ARGUMENT;
    *out_size = (uint32_t)size;
    if (!buffer)
        return DATA_RESULT_OK;
    if (buffer_size < size)
        return DATA_RESULT_BUFFER_TOO_SMALL;

    uint8_t* file = (uint8_t*)buffer;
    memset(file, 0, (size_t)size);
    memcpy(file + offsetof(DataFileHeader, m_Magic), "DMDT", 4);
    WriteDataInteger(file + offsetof(DataFileHeader, m_Version), DATA_FILE_VERSION, 4);
    WriteDataInteger(file + sizeof(DataFileHeader) + offsetof(DataFileDirectory, m_TableCount), tables, 4);
    WriteDataInteger(file + sizeof(DataFileHeader) + offsetof(DataFileDirectory, m_StringsOffset), string_offset, 4);

    uint32_t offset = (uint32_t)Align8(DATA_TABLE_OFFSETS_OFFSET + (uint64_t)tables * sizeof(uint32_t));
    for (uint32_t t = 0; t < tables; ++t)
    {
        const DataTable* table = store->m_Tables[t];
        uint32_t         rows = table->m_Rows.Size();
        WriteDataInteger(file + DATA_TABLE_OFFSETS_OFFSET + t * 4, offset, 4);
        uint8_t* data = file + offset;
        WriteDataInteger(data + offsetof(DataTableHeader, m_TypeHash), table->m_Type, 8);
        WriteDataInteger(data + offsetof(DataTableHeader, m_TagCount), table->m_TagCount, 2);
        WriteDataInteger(data + offsetof(DataTableHeader, m_FieldCount), table->m_FieldCount, 2);
        WriteDataInteger(data + offsetof(DataTableHeader, m_RowCount), rows, 4);
        WriteDataInteger(data + offsetof(DataTableHeader, m_RowStride), table->m_RowStride, 4);
        WriteDataInteger(data + offsetof(DataTableHeader, m_MetadataCount), table->m_MetadataCount, 2);

        data += DATA_TABLE_HEADER_SIZE;
        for (uint32_t i = 0; i < table->m_TagCount; ++i)
            WriteDataInteger(data + i * 8, GetTableTag(table, i), 8);

        data += table->m_TagCount * 8;
        for (uint32_t i = 0; i < table->m_MetadataCount; ++i)
        {
            DataFieldMeta meta = GetFieldMeta(table, i);
            uint8_t*      field = data + i * DATA_FIELD_META_SIZE;
            WriteDataInteger(field + offsetof(DataFileFieldMeta, m_NameHash), meta.m_Field, 8);
            WriteDataInteger(field + offsetof(DataFileFieldMeta, m_Kind), meta.m_Type, 4);
            WriteDataInteger(field + offsetof(DataFileFieldMeta, m_ByteOffset), meta.m_Offset, 4);
            WriteDataInteger(field + offsetof(DataFileFieldMeta, m_ChildIndex), meta.m_ChildIndex, 2);
            WriteDataInteger(field + offsetof(DataFileFieldMeta, m_ChildCount), meta.m_ChildCount, 2);
            WriteDataInteger(field + offsetof(DataFileFieldMeta, m_ByteSize), meta.m_Size, 4);
        }

        data += table->m_MetadataCount * DATA_FIELD_META_SIZE;
        for (uint32_t r = 0; r < rows; ++r)
            WriteDataInteger(data + r * 8, GetRowComponentId(table, &table->m_Rows[r]), 8);

        data += rows * 8;
        offset = (uint32_t)Align8((uint64_t)(data - file) + (uint64_t)rows * table->m_RowStride);
        for (uint32_t r = 0; r < rows; ++r)
        {
            for (uint32_t i = 0; i < table->m_FieldCount; ++i)
            {
                DataFieldMeta meta = GetFieldMeta(table, i);
                DataValue     value = ReadRowValue(table, &table->m_Rows[r], i);
                uint8_t*      field = data + (size_t)r * table->m_RowStride + meta.m_Offset;
                offset = WriteField(table, meta, &value, field, file, offset, (uint32_t)string_offset, &strings);
            }
        }
    }

    for (uint32_t i = 0; i < strings.m_Entries.Size(); ++i)
    {
        const PackedString* entry = &strings.m_Entries[i];
        memcpy(file + string_offset + entry->m_Offset, entry->m_String, entry->m_Size);
    }
    return DATA_RESULT_OK;
}

static bool ValidateValue(const uint8_t* file, uint32_t file_size, DataValueType type, uint32_t value_offset, uint64_t* cursor, uint32_t end, uint32_t strings, uint32_t* next_string, uint32_t depth)
{
    if (type == DATA_VALUE_TYPE_NULL)
        return value_offset == 0;
    if (type == DATA_VALUE_TYPE_STRING)
    {
        if (value_offset < strings || value_offset >= file_size || value_offset > *next_string ||
            (value_offset != strings && file[value_offset - 1] != 0))
            return false;
        if (value_offset == *next_string)
        {
            const uint8_t* terminator = (const uint8_t*)memchr(file + value_offset, 0, file_size - value_offset);
            if (!terminator)
                return false;
            *next_string = (uint32_t)(terminator - file) + 1;
        }
        return true;
    }
    *cursor = Align8(*cursor);
    uint32_t size = DataTypeSize(type);
    if (size == UINT32_MAX || *cursor > end || size > end - *cursor || value_offset != *cursor)
        return false;
    if (!IsContainer(type))
    {
        if (type == DATA_VALUE_TYPE_BOOLEAN && file[value_offset] > 1)
            return false;
        *cursor += size;
        return true;
    }
    if (depth == DATA_MAX_NESTING)
        return false;
    const uint8_t* data = file + value_offset;
    size = (uint32_t)ReadDataInteger(data + offsetof(DataFileContainerHeader, m_ByteSize), 4);
    uint32_t count = (uint32_t)ReadDataInteger(data + offsetof(DataFileContainerHeader, m_Count), 4);
    bool     named = type == DATA_VALUE_TYPE_STRUCT;
    uint64_t types = sizeof(DataFileContainerHeader) + (named ? (uint64_t)count * 8 : 0);
    uint64_t offsets = types + (uint64_t)count * 4;
    uint64_t child_cursor = offsets + (uint64_t)count * 4;
    if (size > end - *cursor || size < child_cursor || count > UINT32_MAX / sizeof(DataValue))
        return false;
    child_cursor += value_offset;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (named)
        {
            uint64_t name = ReadDataInteger(data + sizeof(DataFileContainerHeader) + i * 8, 8);
            for (uint32_t j = 0; j < i; ++j)
                if (name == ReadDataInteger(data + sizeof(DataFileContainerHeader) + j * 8, 8))
                    return false;
        }
        DataValueType child_type = (DataValueType)ReadDataInteger(data + types + i * 4, 4);
        uint32_t      child_offset = (uint32_t)ReadDataInteger(data + offsets + i * 4, 4);
        if (!ValidateValue(file, file_size, child_type, child_offset, &child_cursor, value_offset + size, strings, next_string, depth + 1))
            return false;
    }
    if (Align8(child_cursor) != (uint64_t)value_offset + size)
        return false;
    *cursor += size;
    return true;
}

// Child ranges follow their parent ranges in construction order. Validating this
// canonical tree rejects cycles/aliases and lets all runtime access trust offsets.
static bool ValidateLayout(const uint8_t* metadata, uint32_t total, uint32_t first, uint32_t count, uint32_t size, uint32_t* cursor, uint32_t depth, uint32_t* out_alignment)
{
    if (depth > DATA_MAX_NESTING || first > total || count > total - first)
        return false;
    uint32_t alignment = 1;
    for (uint32_t i = 0; i < count; ++i)
    {
        DataFieldMeta meta = ReadFileFieldMeta(metadata, first + i);
        uint32_t      field_size = DataTypeSize(meta.m_Type);
        uint32_t      field_alignment = DataTypeAlignment(meta.m_Type);
        if (field_size == UINT32_MAX || meta.m_Offset > size || meta.m_Size > size - meta.m_Offset)
            return false;
        if (meta.m_ChildIndex)
        {
            if (meta.m_Type != DATA_VALUE_TYPE_STRUCT || meta.m_ChildIndex != *cursor || *cursor > total || meta.m_ChildCount > total - *cursor)
                return false;
            *cursor += meta.m_ChildCount;
            if (!ValidateLayout(metadata, total, meta.m_ChildIndex, meta.m_ChildCount, meta.m_Size, cursor, depth + 1, &field_alignment))
                return false;
        }
        else if (meta.m_ChildCount || meta.m_Size != field_size)
            return false;
        if (meta.m_Offset % field_alignment)
            return false;
        if (field_alignment > alignment)
            alignment = field_alignment;
        for (uint32_t j = 0; j < i; ++j)
        {
            DataFieldMeta other = ReadFileFieldMeta(metadata, first + j);
            if (other.m_Field == meta.m_Field || (meta.m_Size && other.m_Size && meta.m_Offset < other.m_Offset + other.m_Size && other.m_Offset < meta.m_Offset + meta.m_Size))
                return false;
        }
    }
    if (size % alignment)
        return false;
    *out_alignment = alignment;
    return true;
}

static bool ValidateField(const uint8_t* file, uint32_t size, const uint8_t* metadata, const DataFieldMeta& meta, const uint8_t* field, uint64_t* cursor, uint32_t end, uint32_t strings, uint32_t* next_string, uint32_t depth)
{
    if (meta.m_ChildIndex)
    {
        for (uint32_t i = 0; i < meta.m_ChildCount; ++i)
        {
            DataFieldMeta child = ReadFileFieldMeta(metadata, meta.m_ChildIndex + i);
            if (!ValidateField(file, size, metadata, child, field + child.m_Offset, cursor, end, strings, next_string, depth + 1))
                return false;
        }
        return true;
    }
    if (meta.m_Type == DATA_VALUE_TYPE_BOOLEAN && *field > 1)
        return false;
    if (IsReference(meta.m_Type))
    {
        uint64_t reference = ReadDataInteger(field, 8);
        return reference <= UINT32_MAX && ValidateValue(file, size, meta.m_Type, (uint32_t)reference, cursor, end, strings, next_string, depth);
    }
    return true;
}

static bool ValidateBlob(const uint8_t* file, uint32_t size, uint32_t* out_rows)
{
    if (!file || (uintptr_t)file % 8 || size < DATA_TABLE_OFFSETS_OFFSET || memcmp(file + offsetof(DataFileHeader, m_Magic), "DMDT", 4) || ReadDataInteger(file + offsetof(DataFileHeader, m_Version), 4) != DATA_FILE_VERSION)
        return false;

    uint32_t tables = (uint32_t)ReadDataInteger(file + sizeof(DataFileHeader) + offsetof(DataFileDirectory, m_TableCount), 4);
    uint32_t strings = (uint32_t)ReadDataInteger(file + sizeof(DataFileHeader) + offsetof(DataFileDirectory, m_StringsOffset), 4);
    uint64_t cursor = Align8(DATA_TABLE_OFFSETS_OFFSET + (uint64_t)tables * sizeof(uint32_t));
    if (tables > UINT32_MAX / sizeof(DataTable*) || strings > size || cursor + (uint64_t)tables * DATA_TABLE_HEADER_SIZE > strings)
        return false;

    uint64_t total_rows = 0;
    uint32_t next_string = strings;
    for (uint32_t t = 0; t < tables; ++t)
    {
        uint32_t end = t + 1 < tables ? (uint32_t)ReadDataInteger(file + DATA_TABLE_OFFSETS_OFFSET + (t + 1) * 4, 4) : strings;
        if (ReadDataInteger(file + DATA_TABLE_OFFSETS_OFFSET + t * 4, 4) != cursor || end > strings || cursor + DATA_TABLE_HEADER_SIZE > end)
            return false;
        const uint8_t* table = file + cursor;
        uint32_t       tags = (uint32_t)ReadDataInteger(table + offsetof(DataTableHeader, m_TagCount), 2);
        uint32_t       field_count = (uint32_t)ReadDataInteger(table + offsetof(DataTableHeader, m_FieldCount), 2);
        uint32_t       rows = (uint32_t)ReadDataInteger(table + offsetof(DataTableHeader, m_RowCount), 4);
        uint32_t       stride = (uint32_t)ReadDataInteger(table + offsetof(DataTableHeader, m_RowStride), 4);
        uint32_t       metadata_count = (uint32_t)ReadDataInteger(table + offsetof(DataTableHeader, m_MetadataCount), 2);
        total_rows += rows;
        uint64_t rows_start = cursor + DATA_TABLE_HEADER_SIZE + (uint64_t)tags * 8 + (uint64_t)metadata_count * DATA_FIELD_META_SIZE + (uint64_t)rows * 8;
        uint64_t values_end = Align8(rows_start + (uint64_t)rows * stride);
        if (field_count > metadata_count ||
            ReadDataInteger(table + offsetof(DataTableHeader, m_Reserved), 2) ||
            rows > UINT32_MAX / sizeof(DataRow) || total_rows > UINT32_MAX / sizeof(DataSlot) ||
            values_end > end)
            return false;
        const uint8_t* metadata = table + DATA_TABLE_HEADER_SIZE + (size_t)tags * 8;
        uint32_t       metadata_cursor = field_count;
        uint32_t       alignment;
        if (!ValidateLayout(metadata, metadata_count, 0, field_count, stride, &metadata_cursor, 0, &alignment) || metadata_cursor != metadata_count)
            return false;
        cursor = values_end;
        for (uint32_t r = 0; r < rows; ++r)
        {
            const uint8_t* row = file + rows_start + (uint64_t)r * stride;
            for (uint32_t i = 0; i < field_count; ++i)
            {
                DataFieldMeta meta = ReadFileFieldMeta(metadata, i);
                if (!ValidateField(file, size, metadata, meta, row + meta.m_Offset, &cursor, end, strings, &next_string, 0))
                    return false;
            }
        }
        if (cursor != end)
            return false;
    }

    if (cursor != strings || next_string != size)
        return false;
    *out_rows = (uint32_t)total_rows;
    return true;
}

DataResult DataLoadBlob(const void* buffer, uint32_t buffer_size, HDataBlob* out_blob)
{
    const uint8_t* file = (const uint8_t*)buffer;
    uint32_t       rows;
    if (!ValidateBlob(file, buffer_size, &rows))
        return DATA_RESULT_INVALID_FORMAT;

    HDataBlob blob = new DataBlob;
    blob->m_Data = file;
    blob->m_TableCount = (uint32_t)ReadDataInteger(file + sizeof(DataFileHeader) + offsetof(DataFileDirectory, m_TableCount), 4);
    blob->m_RowCount = rows;
    blob->m_RefCount = 1;
    *out_blob = blob;
    return DATA_RESULT_OK;
}

void DataDestroyBlob(HDataBlob blob)
{
    if (--blob->m_RefCount == 0)
        delete blob;
}
