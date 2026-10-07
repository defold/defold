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
        if (query->m_Groups.Empty())
        {
            iterator->m_StartRow = row;
            iterator->m_Count = count - row;
            iterator->m_NextRow = count;
        }
        else
        {
            while (row < count && !MatchesGroup(query, table->m_Rows[row].m_Group))
                ++row;
            iterator->m_StartRow = row;
            while (row < count && MatchesGroup(query, table->m_Rows[row].m_Group))
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

DataGroupId DataIterGetGroupId(const DataIterator* iterator, uint32_t row)
{
    if (row >= DataIterGetCount(iterator))
        return 0;
    return ((const DataRow*)iterator->m_Rows)[row].m_Group;
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

DataGroupId DataRowIterGetGroupId(const DataRowIterator* iterator)
{
    return DataIterGetGroupId(iterator->m_Parent, iterator->m_Index);
}
