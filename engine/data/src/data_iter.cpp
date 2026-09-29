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
#include <string.h>
#include "data.h"

DataIterator DataQueryIterRange(HDataQuery query, uint32_t first, uint32_t count)
{
    assert(first <= query->m_RowCount && count <= query->m_RowCount - first);
    DataIterator iterator = {
        .m_Query = query,
        .m_Remaining = count,
        .m_IsRange = 1
    };
    if (count)
    {
        const dmArray<DataQueryRange>& ranges = query->m_Ranges;
        uint32_t                       low = 0, high = ranges.Size();
        while (low < high)
        {
            uint32_t middle = low + (high - low) / 2;
            if (ranges[middle].m_First + ranges[middle].m_Count <= first)
                low = middle + 1;
            else
                high = middle;
        }
        iterator.m_RangeIndex = low;
        iterator.m_NextRow = first - ranges[low].m_First;
    }
    return iterator;
}

static DataResult NextQueryRange(DataIterator* iterator)
{
    iterator->m_Count = 0;
    if (!iterator->m_Remaining)
        return DATA_RESULT_END;

    HDataQuery            query = iterator->m_Query;
    const DataQueryRange& range = query->m_Ranges[iterator->m_RangeIndex++];
    const DataQueryTable& match = query->m_Tables[range.m_TableIndex];
    DataTable*            table = match.m_Table;
    uint32_t              count = range.m_Count - iterator->m_NextRow;
    if (count > iterator->m_Remaining)
        count = iterator->m_Remaining;

    iterator->m_Table = table;
    iterator->m_TableIndex = range.m_TableIndex;
    iterator->m_StartRow = range.m_Start + iterator->m_NextRow;
    iterator->m_Count = count;
    iterator->m_Rows = table->m_Rows.Begin() + iterator->m_StartRow;
    iterator->m_Fields = match.m_Fields;
    iterator->m_FieldOffsets = match.m_Fields ? GetQueryFieldOffsets(match.m_Fields, query->m_Fields.Size()) : 0;
    iterator->m_Values = table->m_RowStride ? table->m_Values.Begin() + (size_t)iterator->m_StartRow * table->m_RowStride : 0;
    iterator->m_RowStride = table->m_RowStride;
    iterator->m_FieldCount = match.m_Fields ? query->m_Fields.Size() : table->m_FieldCount;

    iterator->m_Remaining -= count;
    iterator->m_NextRow = 0;
    return DATA_RESULT_OK;
}

DataIterator DataQueryIter(HDataQuery query)
{
    assert(query->m_Store->m_LockCount);
    DataIterator iterator = { .m_Query = query };
    return iterator;
}

DataResult DataIterNext(DataIterator* iterator)
{
    if (iterator->m_IsRange)
        return NextQueryRange(iterator);

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
            iterator->m_FieldOffsets = iterator->m_Fields ? GetQueryFieldOffsets(query->m_Tables[iterator->m_TableIndex].m_Fields, query->m_Fields.Size()) : 0;
            iterator->m_Values = table->m_RowStride ? table->m_Values.Begin() + (size_t)iterator->m_StartRow * table->m_RowStride : 0;
            iterator->m_RowStride = table->m_RowStride;
            iterator->m_FieldCount = iterator->m_Fields ? query->m_Fields.Size() : table->m_FieldCount;
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

DataResult DataIterGetFieldByHash(const DataIterator* iterator, uint32_t row, uint64_t field, DataValue* out_value)
{
    if (row >= iterator->m_Count)
        return DATA_RESULT_INVALID_ARGUMENT;
    return ReadField((DataTable*)iterator->m_Table, iterator->m_StartRow + row, field, out_value);
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
    if (iterator->m_IsRange && iterator->m_Query->m_Fields[field].m_Access != DATA_ACCESS_READ_WRITE)
        return DATA_RESULT_INVALID_ARGUMENT;
    const DataQueryBinding& binding = ((const DataQueryBinding*)iterator->m_Fields)[field];
    return SetRowField((DataTable*)iterator->m_Table, &((DataRow*)iterator->m_Rows)[row], binding.m_Meta, value);
}

DataId DataRowIterGetId(const DataRowIterator* iterator)
{
    return DataIterGetId(iterator->m_Parent, iterator->m_Index);
}

DataOwnerId DataRowIterGetOwnerId(const DataRowIterator* iterator)
{
    return DataIterGetOwnerId(iterator->m_Parent, iterator->m_Index);
}

// Cursors contain indices only. Resolve shared metadata when a caller accesses
// a value or explicitly asks for its kind/name, never when advancing a cursor.
static DataQueryBinding GetFieldBinding(const DataFieldIterator* iterator)
{
    const DataIterator* batch = iterator->m_Batch;
    if (batch->m_Fields)
        return ((const DataQueryBinding*)batch->m_Fields)[iterator->m_Index];
    DataQueryBinding binding = { .m_Meta = GetFieldMeta((const DataTable*)batch->m_Table, iterator->m_Index) };
    return binding;
}

DataValueType DataFieldIterGetType(const DataFieldIterator* iterator)
{
    return iterator->m_Index == UINT32_MAX ? DATA_VALUE_TYPE_NULL : GetFieldBinding(iterator).m_Meta.m_Type;
}

uint64_t DataFieldIterGetNameHash(const DataFieldIterator* iterator)
{
    return iterator->m_Index == UINT32_MAX ? 0 : GetFieldBinding(iterator).m_Meta.m_Field;
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
    return ReadTypedValue<TYPE>(table, row, GetFieldMeta(table, field), out_value);
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
    if (batch->m_IsRange && (!batch->m_Fields || batch->m_Query->m_Fields[iterator->m_Index].m_Access != DATA_ACCESS_READ_WRITE))
        return DATA_RESULT_INVALID_ARGUMENT;

    DataQueryBinding binding = GetFieldBinding(iterator);
    DataRow*         row = &((DataRow*)batch->m_Rows)[iterator->m_RowIndex];
    return SetRowField((DataTable*)batch->m_Table, row, binding.m_Meta, value);
}

DataResult DataGetFieldNumberInternal(const DataIterator* batch, uint32_t row, uint32_t field, double* out_value)
{
    return GetTypedField<DATA_VALUE_TYPE_NUMBER>(batch, row, field, out_value);
}

DataResult DataFieldIterSetNumber(const DataFieldIterator* iterator, double value)
{
    DataValue input = {
        .m_Type = DATA_VALUE_TYPE_NUMBER,
        .m_Value = { .m_Number = value }
    };
    return SetField(iterator, &input);
}

DataResult DataGetFieldBooleanInternal(const DataIterator* batch, uint32_t row, uint32_t field, uint8_t* out_value)
{
    return GetTypedField<DATA_VALUE_TYPE_BOOLEAN>(batch, row, field, out_value);
}

DataResult DataFieldIterSetBoolean(const DataFieldIterator* iterator, uint8_t value)
{
    DataValue input = {
        .m_Type = DATA_VALUE_TYPE_BOOLEAN,
        .m_Value = { .m_Boolean = value }
    };
    return SetField(iterator, &input);
}

DataResult DataGetFieldStringInternal(const DataIterator* batch, uint32_t row, uint32_t field, const char** out_value)
{
    return GetTypedField<DATA_VALUE_TYPE_STRING>(batch, row, field, out_value);
}

DataResult DataFieldIterSetString(const DataFieldIterator* iterator, const char* value)
{
    DataValue input = {
        .m_Type = DATA_VALUE_TYPE_STRING,
        .m_Value = { .m_String = value }
    };
    return SetField(iterator, &input);
}

DataResult DataGetFieldVector3Internal(const DataIterator* batch, uint32_t row, uint32_t field, DataVector3* out_value)
{
    return GetTypedField<DATA_VALUE_TYPE_VECTOR3>(batch, row, field, out_value->m_Values);
}

DataResult DataFieldIterSetVector3(const DataFieldIterator* iterator, const DataVector3* value)
{
    DataValue input = { .m_Type = DATA_VALUE_TYPE_VECTOR3 };
    memcpy(input.m_Value.m_Vector3, value->m_Values, sizeof(value->m_Values));
    return SetField(iterator, &input);
}

DataResult DataGetFieldVector4Internal(const DataIterator* batch, uint32_t row, uint32_t field, DataVector4* out_value)
{
    return GetTypedField<DATA_VALUE_TYPE_VECTOR4>(batch, row, field, out_value->m_Values);
}

DataResult DataFieldIterSetVector4(const DataFieldIterator* iterator, const DataVector4* value)
{
    DataValue input = { .m_Type = DATA_VALUE_TYPE_VECTOR4 };
    memcpy(input.m_Value.m_Vector4, value->m_Values, sizeof(value->m_Values));
    return SetField(iterator, &input);
}

DataResult DataGetFieldMatrix4Internal(const DataIterator* batch, uint32_t row, uint32_t field, DataMatrix4* out_value)
{
    return GetTypedField<DATA_VALUE_TYPE_MATRIX4>(batch, row, field, out_value->m_Values);
}

DataResult DataFieldIterSetMatrix4(const DataFieldIterator* iterator, const DataMatrix4* value)
{
    DataValue input = { .m_Type = DATA_VALUE_TYPE_MATRIX4 };
    memcpy(input.m_Value.m_Matrix4, value->m_Values, sizeof(value->m_Values));
    return SetField(iterator, &input);
}
