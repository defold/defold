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

#include <dlib/static_assert.h>
#include "data.h"

DM_STATIC_ASSERT(sizeof(DataStoreValue) <= sizeof(DataStoreValueView), Invalid_value_view_storage);

// Keep construction values private. Public views carry their borrowed result
// without exposing a tagged value union or allocating an intermediate tree.
static void WriteView(const DataStoreValue& value, DataStoreValueView* out_view)
{
    memcpy(out_view->m_Storage, &value, sizeof(value));
}

static DataStoreValue ReadView(const DataStoreValueView* view)
{
    DataStoreValue value;
    memcpy(&value, view->m_Storage, sizeof(value));
    return value;
}

DataStoreResult DataStoreFieldGetView(HDataStoreContext store, DataStoreId id, uint64_t field, DataStoreValueView* out_view)
{
    DataStoreValue  value;
    DataStoreResult result = DataStoreFieldGet(store, id, field, &value);
    if (result == DATASTORE_RESULT_OK)
    {
        WriteView(value, out_view);
    }
    return result;
}

void DataStoreRowIterGetView(const DataStoreRowIterator* iterator, DataStoreFieldId field, DataStoreValueView* out_view)
{
    const DataStoreIterator*     batch   = iterator->m_Parent;
    const DataStoreQueryBinding& binding = ((const DataStoreQueryBinding*)batch->m_Fields)[field];
    const DataStoreRow*          row     = (const DataStoreRow*)batch->m_Rows + iterator->m_Index;
    WriteView(ReadBoundValue((const DataStoreTable*)batch->m_Table, row, binding.m_Meta), out_view);
}

DataStoreValueType DataStoreViewGetType(const DataStoreValueView* view)
{
    return ReadView(view).m_Type;
}

DataStoreResult DataStoreViewGetCount(const DataStoreValueView* view, uint32_t* out_count)
{
    DataStoreValue value = ReadView(view);
    if (value.m_Type == DATASTORE_TYPE_STRUCT)
    {
        *out_count = value.m_Value.m_Struct.m_Count;
    }
    else if (value.m_Type == DATASTORE_TYPE_LIST)
    {
        *out_count = value.m_Value.m_List.m_Count;
    }
    else
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    return DATASTORE_RESULT_OK;
}

DataStoreResult DataStoreViewGetField(const DataStoreValueView* view, uint64_t field, DataStoreValueView* out_view)
{
    DataStoreValue value = ReadView(view);
    if (value.m_Type != DATASTORE_TYPE_STRUCT)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    DataStoreValue  child;
    DataStoreResult result = DataStoreStructGetField(&value.m_Value.m_Struct, field, &child);
    if (result == DATASTORE_RESULT_OK)
    {
        WriteView(child, out_view);
    }
    return result;
}

DataStoreResult DataStoreViewGetElement(const DataStoreValueView* view, uint32_t index, DataStoreValueView* out_view)
{
    DataStoreValue value = ReadView(view);
    if (value.m_Type != DATASTORE_TYPE_LIST)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    DataStoreValue  child;
    DataStoreResult result = DataStoreListGetValue(&value.m_Value.m_List, index, &child);
    if (result == DATASTORE_RESULT_OK)
    {
        WriteView(child, out_view);
    }
    return result;
}

DataStoreViewIterator DataStoreViewIter(const DataStoreValueView* view)
{
    uint32_t count = 0;
    DataStoreViewGetCount(view, &count);
    DataStoreViewIterator iterator = { view, 0, count };
    return iterator;
}

bool DataStoreViewIterNext(DataStoreViewIterator* iterator, DataStoreValueView* out_view)
{
    if (iterator->m_Index == iterator->m_Count)
    {
        return false;
    }
    DataStoreValue parent = ReadView(iterator->m_Parent);
    DataStoreValue child = GetChildValue(parent.m_Type, &parent.m_Value, iterator->m_Index++);
    WriteView(child, out_view);
    return true;
}

DataStoreResult DataStoreViewGetNumber(const DataStoreValueView* view, double* out_value)
{
    DataStoreValue value = ReadView(view);
    if (value.m_Type != DATASTORE_TYPE_NUMBER)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    *out_value = value.m_Value.m_Number;
    return DATASTORE_RESULT_OK;
}

DataStoreResult DataStoreViewGetFloat(const DataStoreValueView* view, float* out_value)
{
    DataStoreValue value = ReadView(view);
    if (value.m_Type != DATASTORE_TYPE_FLOAT)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    *out_value = value.m_Value.m_Float;
    return DATASTORE_RESULT_OK;
}

DataStoreResult DataStoreViewGetUInt32(const DataStoreValueView* view, uint32_t* out_value)
{
    DataStoreValue value = ReadView(view);
    if (value.m_Type != DATASTORE_TYPE_UINT32)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    *out_value = value.m_Value.m_UInt32;
    return DATASTORE_RESULT_OK;
}

DataStoreResult DataStoreViewGetBoolean(const DataStoreValueView* view, uint8_t* out_value)
{
    DataStoreValue value = ReadView(view);
    if (value.m_Type != DATASTORE_TYPE_BOOLEAN)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    *out_value = value.m_Value.m_Boolean;
    return DATASTORE_RESULT_OK;
}

DataStoreResult DataStoreViewGetString(const DataStoreValueView* view, const char** out_value)
{
    DataStoreValue value = ReadView(view);
    if (value.m_Type != DATASTORE_TYPE_STRING)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    *out_value = value.m_Value.m_String;
    return DATASTORE_RESULT_OK;
}

DataStoreResult DataStoreViewGetVector3(const DataStoreValueView* view, DataStoreVector3* out_value)
{
    DataStoreValue value = ReadView(view);
    if (value.m_Type != DATASTORE_TYPE_VECTOR3)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    memcpy(out_value, value.m_Value.m_Vector3, sizeof(*out_value));
    return DATASTORE_RESULT_OK;
}

DataStoreResult DataStoreViewGetVector4(const DataStoreValueView* view, DataStoreVector4* out_value)
{
    DataStoreValue value = ReadView(view);
    if (value.m_Type != DATASTORE_TYPE_VECTOR4)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    memcpy(out_value, value.m_Value.m_Vector4, sizeof(*out_value));
    return DATASTORE_RESULT_OK;
}

DataStoreResult DataStoreViewGetMatrix4(const DataStoreValueView* view, DataStoreMatrix4* out_value)
{
    DataStoreValue value = ReadView(view);
    if (value.m_Type != DATASTORE_TYPE_MATRIX4)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    memcpy(out_value, value.m_Value.m_Matrix4, sizeof(*out_value));
    return DATASTORE_RESULT_OK;
}
