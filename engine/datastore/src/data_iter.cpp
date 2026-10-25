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

DSIterator DSQueryIterRange(HDSQuery query, uint32_t first, uint32_t count)
{
    assert(query->m_ActiveSlot != UINT32_MAX);
    assert(first <= query->m_RowCount && count <= query->m_RowCount - first);
    DSIterator iterator = {
        .m_Query = query,
        .m_Remaining = count,
        .m_Flags = query->m_Flags
    };
    if (count)
    {
        const dmArray<DSQueryRange>& ranges = query->m_Ranges;
        uint32_t                     low = 0, high = ranges.Size();
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

bool DSIterNext(DSIterator* iterator)
{
    iterator->m_Count = 0;
    if (!iterator->m_Remaining)
        return false;

    HDSQuery            query = iterator->m_Query;
    const DSQueryRange& range = query->m_Ranges[iterator->m_RangeIndex++];
    const DSQueryTable& match = query->m_Tables[range.m_TableIndex];
    DSTable*            table = match.m_Table;
    uint32_t            count = range.m_Count - iterator->m_NextRow;
    if (count > iterator->m_Remaining)
        count = iterator->m_Remaining;

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

DSIterator DSQueryIter(HDSQuery query)
{
    return DSQueryIterRange(query, 0, query->m_RowCount);
}

uint32_t DSIterGetCount(const DSIterator* iterator)
{
    return iterator->m_Count;
}

uint64_t DSIterGetType(const DSIterator* iterator)
{
    return DSIterGetCount(iterator) ? ((const DSTable*)iterator->m_Table)->m_Type : 0;
}

DSId DSIterGetId(const DSIterator* iterator, uint32_t row)
{
    if (row >= DSIterGetCount(iterator))
        return 0;
    return MakeId(iterator->m_Query->m_Store, ((const DSRow*)iterator->m_Rows)[row].m_Slot);
}

DSId DSIterGetGroupId(const DSIterator* iterator, uint32_t row)
{
    if (row >= DSIterGetCount(iterator))
        return 0;
    return ((const DSRow*)iterator->m_Rows)[row].m_Group;
}

DSResult DSIterGetFieldByHash(const DSIterator* iterator, uint32_t row, uint64_t field, DSValue* out_value)
{
    if (row >= iterator->m_Count)
        return DS_RESULT_INVALID_ARGUMENT;
    DSTable* table = (DSTable*)iterator->m_Table;
    uint32_t index = (uint32_t)((const DSRow*)iterator->m_Rows - table->m_Rows.Begin()) + row;
    return ReadField(table, index, field, out_value);
}

DSResult DSIterGetField(const DSIterator* iterator, uint32_t row, DSFieldId field, DSValue* out_value)
{
    if (row >= iterator->m_Count || !iterator->m_Fields || field >= iterator->m_FieldCount)
        return DS_RESULT_INVALID_ARGUMENT;
    const DSQueryBinding& binding = ((const DSQueryBinding*)iterator->m_Fields)[field];
    *out_value = ReadBoundValue((const DSTable*)iterator->m_Table, &((const DSRow*)iterator->m_Rows)[row], binding.m_Meta);
    return DS_RESULT_OK;
}

DSResult DSIterSetField(const DSIterator* iterator, uint32_t row, DSFieldId field, const DSValue* value)
{
    if (row >= iterator->m_Count || !iterator->m_Fields || field >= iterator->m_FieldCount)
        return DS_RESULT_INVALID_ARGUMENT;
    if (iterator->m_Query->m_Fields[field].m_Access != DS_FIELD_MUTABLE)
        return DS_RESULT_INVALID_ARGUMENT;
    const DSQueryBinding& binding = ((const DSQueryBinding*)iterator->m_Fields)[field];
    return SetRowField((DSTable*)iterator->m_Table, &((DSRow*)iterator->m_Rows)[row], binding.m_Meta, value);
}

DSId DSRowIterGetId(const DSRowIterator* iterator)
{
    return DSIterGetId(iterator->m_Parent, iterator->m_Index);
}

DSId DSRowIterGetGroupId(const DSRowIterator* iterator)
{
    return DSIterGetGroupId(iterator->m_Parent, iterator->m_Index);
}
