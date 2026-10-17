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

DM_STATIC_ASSERT(sizeof(DataValue) <= sizeof(DataValueView), Invalid_value_view_storage);

// Keep construction values private. Public views carry their borrowed result
// without exposing a tagged value union or allocating an intermediate tree.
static void WriteView(const DataValue& value, DataValueView* out_view)
{
    memcpy(out_view->m_Storage, &value, sizeof(value));
}

static DataValue ReadView(const DataValueView* view)
{
    DataValue value;
    memcpy(&value, view->m_Storage, sizeof(value));
    return value;
}

DataResult DataFieldGetView(HDataStore store, DataId id, uint64_t field, DataValueView* out_view)
{
    DataValue  value;
    DataResult result = DataFieldGet(store, id, field, &value);
    if (result == DATA_RESULT_OK)
        WriteView(value, out_view);
    return result;
}

void DataRowIterGetView(const DataRowIterator* iterator, uint32_t field, DataValueView* out_view)
{
    const DataIterator*     batch = iterator->m_Parent;
    const DataQueryBinding& binding = ((const DataQueryBinding*)batch->m_Fields)[field];
    const DataRow*          row = (const DataRow*)batch->m_Rows + iterator->m_Index - batch->m_StartRow;
    WriteView(ReadBoundValue((const DataTable*)batch->m_Table, row, binding.m_Meta), out_view);
}

DataValueType DataViewGetType(const DataValueView* view)
{
    return ReadView(view).m_Type;
}

DataResult DataViewGetCount(const DataValueView* view, uint32_t* out_count)
{
    DataValue value = ReadView(view);
    if (value.m_Type == DATA_TYPE_STRUCT)
        *out_count = value.m_Value.m_Struct.m_Count;
    else if (value.m_Type == DATA_TYPE_LIST)
        *out_count = value.m_Value.m_List.m_Count;
    else
        return DATA_RESULT_INVALID_ARGUMENT;
    return DATA_RESULT_OK;
}

DataResult DataViewGetField(const DataValueView* view, uint64_t field, DataValueView* out_view)
{
    DataValue value = ReadView(view);
    if (value.m_Type != DATA_TYPE_STRUCT)
        return DATA_RESULT_INVALID_ARGUMENT;
    DataValue  child;
    DataResult result = DataGetStructField(&value.m_Value.m_Struct, field, &child);
    if (result == DATA_RESULT_OK)
        WriteView(child, out_view);
    return result;
}

DataResult DataViewGetElement(const DataValueView* view, uint32_t index, DataValueView* out_view)
{
    DataValue value = ReadView(view);
    if (value.m_Type != DATA_TYPE_LIST)
        return DATA_RESULT_INVALID_ARGUMENT;
    DataValue  child;
    DataResult result = DataGetListValue(&value.m_Value.m_List, index, &child);
    if (result == DATA_RESULT_OK)
        WriteView(child, out_view);
    return result;
}

DataResult DataViewGetNumber(const DataValueView* view, double* out_value)
{
    DataValue value = ReadView(view);
    if (value.m_Type != DATA_TYPE_NUMBER)
        return DATA_RESULT_INVALID_ARGUMENT;
    *out_value = value.m_Value.m_Number;
    return DATA_RESULT_OK;
}

DataResult DataViewGetFloat(const DataValueView* view, float* out_value)
{
    DataValue value = ReadView(view);
    if (value.m_Type != DATA_TYPE_FLOAT)
        return DATA_RESULT_INVALID_ARGUMENT;
    *out_value = value.m_Value.m_Float;
    return DATA_RESULT_OK;
}

DataResult DataViewGetUInt32(const DataValueView* view, uint32_t* out_value)
{
    DataValue value = ReadView(view);
    if (value.m_Type != DATA_TYPE_UINT32)
        return DATA_RESULT_INVALID_ARGUMENT;
    *out_value = value.m_Value.m_UInt32;
    return DATA_RESULT_OK;
}

DataResult DataViewGetBoolean(const DataValueView* view, uint8_t* out_value)
{
    DataValue value = ReadView(view);
    if (value.m_Type != DATA_TYPE_BOOLEAN)
        return DATA_RESULT_INVALID_ARGUMENT;
    *out_value = value.m_Value.m_Boolean;
    return DATA_RESULT_OK;
}

DataResult DataViewGetString(const DataValueView* view, const char** out_value)
{
    DataValue value = ReadView(view);
    if (value.m_Type != DATA_TYPE_STRING)
        return DATA_RESULT_INVALID_ARGUMENT;
    *out_value = value.m_Value.m_String;
    return DATA_RESULT_OK;
}

DataResult DataViewGetVector3(const DataValueView* view, DataVector3* out_value)
{
    DataValue value = ReadView(view);
    if (value.m_Type != DATA_TYPE_VECTOR3)
        return DATA_RESULT_INVALID_ARGUMENT;
    memcpy(out_value, value.m_Value.m_Vector3, sizeof(*out_value));
    return DATA_RESULT_OK;
}

DataResult DataViewGetVector4(const DataValueView* view, DataVector4* out_value)
{
    DataValue value = ReadView(view);
    if (value.m_Type != DATA_TYPE_VECTOR4)
        return DATA_RESULT_INVALID_ARGUMENT;
    memcpy(out_value, value.m_Value.m_Vector4, sizeof(*out_value));
    return DATA_RESULT_OK;
}

DataResult DataViewGetMatrix4(const DataValueView* view, DataMatrix4* out_value)
{
    DataValue value = ReadView(view);
    if (value.m_Type != DATA_TYPE_MATRIX4)
        return DATA_RESULT_INVALID_ARGUMENT;
    memcpy(out_value, value.m_Value.m_Matrix4, sizeof(*out_value));
    return DATA_RESULT_OK;
}
