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

DataStoreIterator DataStoreQueryIterRange(HDataStoreQuery query, uint32_t first, uint32_t count)
{
    assert(query->m_ActiveSlot != UINT32_MAX);
    assert(first <= query->m_RowCount && count <= query->m_RowCount - first);
    DataStoreIterator iterator = {};
    iterator.m_Query = query;
    iterator.m_Remaining = count;
    iterator.m_Flags = query->m_Flags;
    if (count)
    {
        const dmArray<DataStoreQueryRange>& ranges = query->m_Ranges;
        uint32_t                            low    = 0, high = ranges.Size();
        while (low < high)
        {
            uint32_t middle = low + (high - low) / 2;
            if (ranges[middle].m_First + ranges[middle].m_Count <= first)
            {
                low = middle + 1;
            }
            else
            {
                high = middle;
            }
        }
        iterator.m_RangeIndex = low;
        iterator.m_NextRow = first - ranges[low].m_First;
    }
    return iterator;
}

bool DataStoreIterNext(DataStoreIterator* iterator)
{
    iterator->m_Count = 0;
    if (!iterator->m_Remaining)
    {
        return false;
    }

    HDataStoreQuery            query = iterator->m_Query;
    const DataStoreQueryRange& range = query->m_Ranges[iterator->m_RangeIndex++];
    const DataStoreQueryTable& match = query->m_Tables[range.m_TableIndex];
    DataStoreTable*            table = match.m_Table;
    uint32_t                   count = range.m_Count - iterator->m_NextRow;
    if (count > iterator->m_Remaining)
    {
        count = iterator->m_Remaining;
    }

    uint32_t start = range.m_Start + iterator->m_NextRow;
    iterator->m_Table = table;
    iterator->m_Count = count;
    iterator->m_Rows = table->m_Rows.Begin() + start;
    iterator->m_Fields = match.m_Fields;
    iterator->m_FieldOffsets = match.m_Fields ? GetQueryFieldOffsets(match.m_Fields, query->m_Fields.Size()) : 0;
    iterator->m_Values = GetRowBytes(table, start);
    iterator->m_RowStride = table->m_RowStride;
    iterator->m_FieldCount = match.m_Fields ? query->m_Fields.Size() : table->m_FieldCount;

    iterator->m_Remaining -= count;
    iterator->m_NextRow = 0;
    return true;
}

DataStoreIterator DataStoreQueryIter(HDataStoreQuery query)
{
    return DataStoreQueryIterRange(query, 0, query->m_RowCount);
}

uint32_t DataStoreIterGetCount(const DataStoreIterator* iterator)
{
    return iterator->m_Count;
}

uint64_t DataStoreIterGetType(const DataStoreIterator* iterator)
{
    return DataStoreIterGetCount(iterator) ? ((const DataStoreTable*)iterator->m_Table)->m_Type : 0;
}

DataStoreId DataStoreIterGetId(const DataStoreIterator* iterator, uint32_t row)
{
    if (row >= DataStoreIterGetCount(iterator))
    {
        return 0;
    }
    return MakeId(iterator->m_Query->m_Store, ((const DataStoreRow*)iterator->m_Rows)[row].m_Slot);
}

DataStoreId DataStoreIterGetGroupId(const DataStoreIterator* iterator, uint32_t row)
{
    if (row >= DataStoreIterGetCount(iterator))
    {
        return 0;
    }
    return ((const DataStoreRow*)iterator->m_Rows)[row].m_Group;
}

DataStoreResult DataStoreIterGetFieldByHash(const DataStoreIterator* iterator, uint32_t row, uint64_t field, DataStoreValue* out_value)
{
    if (row >= iterator->m_Count)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    DataStoreTable* table = (DataStoreTable*)iterator->m_Table;
    uint32_t index = (uint32_t)((const DataStoreRow*)iterator->m_Rows - table->m_Rows.Begin()) + row;
    return ReadField(table, index, field, out_value);
}

DataStoreResult DataStoreIterGetField(const DataStoreIterator* iterator, uint32_t row, DataStoreFieldId field, DataStoreValue* out_value)
{
    if (row >= iterator->m_Count || !iterator->m_Fields || field >= iterator->m_FieldCount)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    const DataStoreQueryBinding& binding = ((const DataStoreQueryBinding*)iterator->m_Fields)[field];
    *out_value = ReadBoundValue((const DataStoreTable*)iterator->m_Table, &((const DataStoreRow*)iterator->m_Rows)[row], binding.m_Meta);
    return DATASTORE_RESULT_OK;
}

DataStoreResult DataStoreIterSetField(const DataStoreIterator* iterator, uint32_t row, DataStoreFieldId field, const DataStoreValue* value)
{
    if (row >= iterator->m_Count || !iterator->m_Fields || field >= iterator->m_FieldCount)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    if (iterator->m_Query->m_Fields[field].m_Access != DATASTORE_FIELD_MUTABLE)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    const DataStoreQueryBinding& binding = ((const DataStoreQueryBinding*)iterator->m_Fields)[field];
    return SetRowField((DataStoreTable*)iterator->m_Table, &((DataStoreRow*)iterator->m_Rows)[row], binding.m_Meta, value);
}

DataStoreId DataStoreRowIterGetId(const DataStoreRowIterator* iterator)
{
    return DataStoreIterGetId(iterator->m_Parent, iterator->m_Index);
}

DataStoreId DataStoreRowIterGetGroupId(const DataStoreRowIterator* iterator)
{
    return DataStoreIterGetGroupId(iterator->m_Parent, iterator->m_Index);
}
