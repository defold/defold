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
#include <string.h>
#include "data.h"

// Resolve names during table matching and retain the field metadata/byte offset.
// Dynamic containers do not expose fixed member offsets.
static bool ResolveQueryField(const DataTable* table, const DataQueryFieldInfo& field, DataQueryBinding* binding)
{
    return ResolveField(table, field.m_Field, field.m_Type, &binding->m_Meta);
}

static DataQueryBinding* AllocateQueryFields(HDataQuery query)
{
    if (query->m_FreeFields)
    {
        DataQueryBinding* fields = query->m_FreeFields;
        memcpy(&query->m_FreeFields, (const void*)fields, sizeof(query->m_FreeFields));
        return fields;
    }
    size_t size = query->m_Fields.Size() * (sizeof(DataQueryBinding) + sizeof(uint32_t));
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

void MatchTable(HDataQuery query, DataTable* table)
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
    for (uint32_t i = 0; i < query->m_Fields.Size(); ++i)
    {
        DataQueryBinding binding;
        if (!ResolveQueryField(table, query->m_Fields[i], &binding))
            return;
    }

    DataQueryTable match = { .m_Table = table };
    if (query->m_Fields.Size())
        match.m_Fields = AllocateQueryFields(query);
    for (uint32_t i = 0; i < query->m_Fields.Size(); ++i)
    {
        ResolveQueryField(table, query->m_Fields[i], &match.m_Fields[i]);
        GetQueryFieldOffsets(match.m_Fields, query->m_Fields.Size())[i] = match.m_Fields[i].m_Meta.m_Offset;
    }

    Reserve(query->m_Tables, query->m_Tables.Size() + 1);
    query->m_Tables.Push(match);
}

void UnmatchTable(HDataQuery query, DataTable* table)
{
    dmArray<DataQueryTable>& tables = query->m_Tables;
    for (uint32_t j = 0; j < tables.Size(); ++j)
    {
        if (tables[j].m_Table == table)
        {
            ReleaseQueryFields(query, tables[j].m_Fields);
            tables.EraseSwap(j);
            break;
        }
    }
}

static bool SupportsConcurrentWrite(DataValueType type)
{
    return type == DATA_TYPE_NUMBER || type == DATA_TYPE_BOOLEAN ||
    (type >= DATA_TYPE_VECTOR3 && type <= DATA_TYPE_MATRIX4);
}

DataResult DataCreateQuery(HDataStore store, const DataQueryDesc* desc, HDataQuery* out_query)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (store->m_ActiveQueries.Size())
        return DATA_RESULT_LOCKED;
    if (desc->m_GroupIdCount > UINT32_MAX / sizeof(DataGroupId) || desc->m_AllTagCount > UINT32_MAX / sizeof(uint64_t) ||
        desc->m_FieldCount > UINT32_MAX / (sizeof(DataQueryBinding) + sizeof(uint32_t)))
        return DATA_RESULT_INVALID_ARGUMENT;

    for (uint32_t i = 0; i < desc->m_FieldCount; ++i)
    {
        const DataQueryField& field = desc->m_Fields[i];
        if ((uint32_t)field.m_Type > DATA_TYPE_MATRIX4 || (uint32_t)field.m_Access > DATA_ACCESS_READ_WRITE ||
            (field.m_Access == DATA_ACCESS_READ_WRITE && !SupportsConcurrentWrite(field.m_Type)))
            return DATA_RESULT_INVALID_ARGUMENT;
    }

    HDataQuery query = new DataQuery;
    query->m_Store = store;
    query->m_Bindings = 0;
    query->m_FreeFields = 0;
    query->m_Revision = 0;
    query->m_RowCount = 0;
    query->m_ActiveSlot = UINT32_MAX;
    if (desc->m_GroupIdCount)
    {
        query->m_Groups.SetCapacity(desc->m_GroupIdCount);
        query->m_Groups.PushArray(desc->m_GroupIds, desc->m_GroupIdCount);
    }
    if (desc->m_AllTagCount)
    {
        query->m_Tags.SetCapacity(desc->m_AllTagCount);
        query->m_Tags.PushArray(desc->m_AllTags, desc->m_AllTagCount);
    }
    if (desc->m_FieldCount)
    {
        query->m_Fields.SetCapacity(desc->m_FieldCount);
        for (uint32_t i = 0; i < desc->m_FieldCount; ++i)
        {
            const DataQueryField& input = desc->m_Fields[i];
            DataQueryFieldInfo    field = {
                   .m_Field = input.m_Field,
                   .m_Type = input.m_Type,
                   .m_Access = (uint8_t)input.m_Access
            };
            query->m_Fields.Push(field);
        }
    }

    query->m_Tables.SetCapacity(store->m_Tables.Size());
    for (uint32_t i = 0; i < store->m_Tables.Size(); ++i)
    {
        MatchTable(query, store->m_Tables[i]);
    }

    Reserve(store->m_Queries, store->m_Queries.Size() + 1);
    // Reserve one active slot per live query before workers can begin admission.
    if (store->m_ActiveQueries.Capacity() < store->m_Queries.Capacity())
        store->m_ActiveQueries.SetCapacity(store->m_Queries.Capacity());
    store->m_Queries.Push(query);
    *out_query = query;
    return DATA_RESULT_OK;
}

DataResult DataDestroyQuery(HDataQuery query)
{
    DM_MUTEX_SCOPED_LOCK(query->m_Store->m_Mutex);
    if (query->m_Store->m_ActiveQueries.Size())
        return DATA_RESULT_LOCKED;
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
    return DATA_RESULT_OK;
}

// Distinct read-only queries can run together, including overlapping fields. A
// tag-only query reads the full row. Group filters deliberately stay conservative.
static bool HasWrites(HDataQuery query)
{
    for (uint32_t i = 0; i < query->m_Fields.Size(); ++i)
        if (query->m_Fields[i].m_Access == DATA_ACCESS_READ_WRITE)
            return true;
    return false;
}

static bool QueriesConflict(HDataQuery a, HDataQuery b)
{
    if (!HasWrites(a) && !HasWrites(b))
        return false;

    for (uint32_t i = 0; i < a->m_Tables.Size(); ++i)
    {
        const DataQueryTable& at = a->m_Tables[i];
        for (uint32_t j = 0; j < b->m_Tables.Size(); ++j)
        {
            const DataQueryTable& bt = b->m_Tables[j];
            if (at.m_Table != bt.m_Table)
                continue;
            if (a->m_Fields.Empty() || b->m_Fields.Empty())
                return true;
            for (uint32_t af = 0; af < a->m_Fields.Size(); ++af)
            {
                const DataFieldMeta& am = at.m_Fields[af].m_Meta;
                for (uint32_t bf = 0; bf < b->m_Fields.Size(); ++bf)
                {
                    const DataFieldMeta& bm = bt.m_Fields[bf].m_Meta;
                    if ((a->m_Fields[af].m_Access == DATA_ACCESS_READ_WRITE ||
                         b->m_Fields[bf].m_Access == DATA_ACCESS_READ_WRITE) &&
                        am.m_Size && bm.m_Size && am.m_Offset < bm.m_Offset + bm.m_Size && bm.m_Offset < am.m_Offset + am.m_Size)
                        return true;
                }
            }
        }
    }
    return false;
}

static void RefreshQueryRanges(HDataQuery query)
{
    if (query->m_Revision == query->m_Store->m_Revision)
        return;

    query->m_Ranges.SetSize(0);
    query->m_RowCount = 0;
    for (uint32_t t = 0; t < query->m_Tables.Size(); ++t)
    {
        const DataTable* table = query->m_Tables[t].m_Table;
        uint32_t         count = table->m_Rows.Size();
        uint32_t         row = 0;
        while (row < count)
        {
            while (row < count && !MatchesGroup(query, table->m_Rows[row].m_Group))
                ++row;
            uint32_t first = row;
            if (query->m_Groups.Empty())
                row = count;
            else
                while (row < count && MatchesGroup(query, table->m_Rows[row].m_Group))
                    ++row;
            if (row != first)
            {
                DataQueryRange range = {
                    .m_TableIndex = t,
                    .m_Start = first,
                    .m_Count = row - first,
                    .m_First = query->m_RowCount
                };
                Reserve(query->m_Ranges, query->m_Ranges.Size() + 1);
                query->m_Ranges.Push(range);
                query->m_RowCount += range.m_Count;
            }
        }
    }

    query->m_Revision = query->m_Store->m_Revision;
}

DataResult DataQueryTryBegin(HDataQuery query)
{
    HDataStore store = query->m_Store;
    if (!dmMutex::TryLock(store->m_Mutex))
        return DATA_RESULT_BUSY;
    if (store->m_LockCount || query->m_ActiveSlot != UINT32_MAX)
    {
        dmMutex::Unlock(store->m_Mutex);
        return DATA_RESULT_BUSY;
    }

    const dmArray<HDataQuery>& active = store->m_ActiveQueries.GetRawObjects();
    for (uint32_t i = 0; i < active.Size(); ++i)
    {
        if (QueriesConflict(query, active[i]))
        {
            dmMutex::Unlock(store->m_Mutex);
            return DATA_RESULT_BUSY;
        }
    }

    RefreshQueryRanges(query);
    query->m_ActiveSlot = store->m_ActiveQueries.Alloc();
    store->m_ActiveQueries.Set(query->m_ActiveSlot, query);
    dmMutex::Unlock(store->m_Mutex);
    return DATA_RESULT_OK;
}

uint32_t DataQueryGetRowCount(HDataQuery query)
{
    return query->m_RowCount;
}

void DataQueryEnd(HDataQuery query)
{
    HDataStore store = query->m_Store;
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    store->m_ActiveQueries.Free(query->m_ActiveSlot, false);
    query->m_ActiveSlot = UINT32_MAX;
}

uint32_t DataQueryFindField(HDataQuery query, const DataQueryField* field)
{
    switch (field->m_Type)
    {
        case DATA_TYPE_NUMBER:
        case DATA_TYPE_BOOLEAN:
        case DATA_TYPE_VECTOR3:
        case DATA_TYPE_VECTOR4:
        case DATA_TYPE_MATRIX4:
            break;
        default:
            return UINT32_MAX;
    }

    const dmArray<DataQueryFieldInfo>& fields = query->m_Fields;
    for (uint32_t i = 0; i < fields.Size(); ++i)
    {
        const DataQueryFieldInfo& candidate = fields[i];
        if ((uint32_t)field->m_Access <= (uint32_t)candidate.m_Access &&
            candidate.m_Field == field->m_Field && candidate.m_Type == field->m_Type)
            return i;
    }
    return UINT32_MAX;
}
