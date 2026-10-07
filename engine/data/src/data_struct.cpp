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

#include "data.h"

static const uint32_t INVALID_ROW = UINT32_MAX;

// Inputs commonly follow layout order; names remain authoritative when reordered.
static uint32_t FindStructField(const DataStruct* object, uint64_t name, uint32_t hint)
{
    if (GetChildName(object, hint) == name)
        return hint;
    for (uint32_t i = 0; i < object->m_Count; ++i)
        if (GetChildName(object, i) == name)
            return i;
    return UINT32_MAX;
}

static uint32_t FindStructLayout(const DataStructStorage* storage, const DataStruct* object)
{
    for (uint32_t t = 0; t < storage->m_Tables.Size(); ++t)
    {
        const DataStructTable* table = storage->m_Tables[t];
        if (table->m_Fields.Size() != object->m_Count)
            continue;
        uint32_t matched = 0;
        for (; matched < object->m_Count; ++matched)
        {
            const DataStructField& field = table->m_Fields[matched];
            uint32_t               index = FindStructField(object, field.m_Name, matched);
            if (index == UINT32_MAX || GetStructFieldType(object, index) != field.m_Type)
                break;
        }
        if (matched == object->m_Count)
            return t;
    }
    return UINT32_MAX;
}

static uint32_t AddStructLayout(DataStructStorage* storage, const DataStruct* object)
{
    DataStructTable* table = new DataStructTable;
    table->m_FreeRow = INVALID_ROW;
    table->m_RowCount = 0;
    table->m_Fields.SetCapacity(object->m_Count);
    uint32_t size = 0;
    for (uint32_t i = 0; i < object->m_Count; ++i)
    {
        DataValueType type = GetStructFieldType(object, i);
        uint32_t      alignment = DataTypeAlignment(type);
        size = (size + alignment - 1) & ~(alignment - 1);
        DataStructField field = { .m_Name = GetChildName(object, i), .m_Type = type, .m_Offset = size };
        table->m_Fields.Push(field);
        size += DataTypeSize(type);
    }
    table->m_RowStride = size ? (size + 7) & ~7u : 8;
    Reserve(storage->m_Tables, storage->m_Tables.Size() + 1);
    uint32_t index = storage->m_Tables.Size();
    storage->m_Tables.Push(table);
    return index;
}

uint64_t StoreStructRow(DataTable* owner, DataBlock** blocks, const DataStruct* object)
{
    if (!owner->m_Structs)
        owner->m_Structs = new DataStructStorage;
    DataStructStorage* storage = owner->m_Structs;
    uint32_t           index = FindStructLayout(storage, object);
    if (index == UINT32_MAX)
        index = AddStructLayout(storage, object);
    assert(index < (1u << 30));
    DataStructTable* table = storage->m_Tables[index];
    uint32_t         row = table->m_FreeRow;
    if (row == INVALID_ROW)
    {
        row = table->m_Values.Size() / table->m_RowStride;
        assert(table->m_Values.Size() <= UINT32_MAX - table->m_RowStride);
        Reserve(table->m_Values, table->m_Values.Size() + table->m_RowStride);
        table->m_Values.SetSize(table->m_Values.Size() + table->m_RowStride);
    }
    else
        table->m_FreeRow = (uint32_t)ReadDataInteger(table->m_Values.Begin() + (size_t)row * table->m_RowStride, 4);
    ++table->m_RowCount;
    memset(table->m_Values.Begin() + (size_t)row * table->m_RowStride, 0, table->m_RowStride);
    DataValue source = { .m_Type = DATA_TYPE_STRUCT, .m_Value = { .m_Struct = *object } };
    for (uint32_t i = 0; i < table->m_Fields.Size(); ++i)
    {
        const DataStructField& field = table->m_Fields[i];
        uint32_t               source_index = FindStructField(object, field.m_Name, i);
        DataValue              value = GetChildValue(DATA_TYPE_STRUCT, &source.m_Value, source_index);
        if (field.m_Type == DATA_TYPE_STRUCT)
        {
            // Recursion can grow this same layout's row array. Resolve the
            // destination after insertion, and keep source views as indices.
            uint64_t reference = StoreStructRow(owner, blocks, &value.m_Value.m_Struct);
            WriteDataInteger(table->m_Values.Begin() + (size_t)row * table->m_RowStride + field.m_Offset, reference, 8);
        }
        else
            StoreValue(blocks, table->m_Values.Begin() + (size_t)row * table->m_RowStride + field.m_Offset, field.m_Type, &value.m_Value);
    }
    return ((uint64_t)row << 32) | ((uint64_t)index << 2) | 2;
}

DataStruct ReadStructRow(const DataTable* owner, uint64_t reference)
{
    const DataStructTable* table = owner->m_Structs->m_Tables[(uint32_t)reference >> 2];
    DataStruct             object = {
                    .m_Count = table->m_Fields.Size(),
                    .m_Source = DATA_STRUCT_ROW,
                    .m_View = { .m_Offset = (uint32_t)(reference >> 32), .m_Table = owner, .m_StructTable = table }
    };
    return object;
}

void ReleaseStructRow(DataTable* owner, uint64_t reference)
{
    DataStructTable* table = owner->m_Structs->m_Tables[(uint32_t)reference >> 2];
    uint32_t         row = (uint32_t)(reference >> 32);
    uint8_t*         bytes = table->m_Values.Begin() + (size_t)row * table->m_RowStride;
    for (uint32_t i = 0; i < table->m_Fields.Size(); ++i)
    {
        const DataStructField& field = table->m_Fields[i];
        if (field.m_Type == DATA_TYPE_STRUCT)
            ReleaseStructRow(owner, ReadDataInteger(bytes + field.m_Offset, 8));
    }
    WriteDataInteger(bytes, table->m_FreeRow, 4);
    table->m_FreeRow = row;
    --table->m_RowCount;
}

void DeleteStructStorage(DataTable* table)
{
    if (!table->m_Structs)
        return;
    for (uint32_t i = 0; i < table->m_Structs->m_Tables.Size(); ++i)
        delete table->m_Structs->m_Tables[i];
    delete table->m_Structs;
}
