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

DM_STATIC_ASSERT(sizeof(DSValue) <= sizeof(DSValueView), Invalid_value_view_storage);

// Keep construction values private. Public views carry their borrowed result
// without exposing a tagged value union or allocating an intermediate tree.
static void WriteView(const DSValue& value, DSValueView* out_view)
{
    memcpy(out_view->m_Storage, &value, sizeof(value));
}

static DSValue ReadView(const DSValueView* view)
{
    DSValue value;
    memcpy(&value, view->m_Storage, sizeof(value));
    return value;
}

DSResult DSFieldGetView(HDSContext store, DSId id, uint64_t field, DSValueView* out_view)
{
    DSValue  value;
    DSResult result = DSFieldGet(store, id, field, &value);
    if (result == DS_RESULT_OK)
        WriteView(value, out_view);
    return result;
}

void DSRowIterGetView(const DSRowIterator* iterator, uint32_t field, DSValueView* out_view)
{
    const DSIterator*     batch = iterator->m_Parent;
    const DSQueryBinding& binding = ((const DSQueryBinding*)batch->m_Fields)[field];
    const DSRow*          row = (const DSRow*)batch->m_Rows + iterator->m_Index;
    WriteView(ReadBoundValue((const DSTable*)batch->m_Table, row, binding.m_Meta), out_view);
}

DSValueType DSViewGetType(const DSValueView* view)
{
    return ReadView(view).m_Type;
}

DSResult DSViewGetCount(const DSValueView* view, uint32_t* out_count)
{
    DSValue value = ReadView(view);
    if (value.m_Type == DS_TYPE_STRUCT)
        *out_count = value.m_Value.m_Struct.m_Count;
    else if (value.m_Type == DS_TYPE_LIST)
        *out_count = value.m_Value.m_List.m_Count;
    else
        return DS_RESULT_INVALID_ARGUMENT;
    return DS_RESULT_OK;
}

DSResult DSViewGetField(const DSValueView* view, uint64_t field, DSValueView* out_view)
{
    DSValue value = ReadView(view);
    if (value.m_Type != DS_TYPE_STRUCT)
        return DS_RESULT_INVALID_ARGUMENT;
    DSValue  child;
    DSResult result = DSGetStructField(&value.m_Value.m_Struct, field, &child);
    if (result == DS_RESULT_OK)
        WriteView(child, out_view);
    return result;
}

DSResult DSViewGetElement(const DSValueView* view, uint32_t index, DSValueView* out_view)
{
    DSValue value = ReadView(view);
    if (value.m_Type != DS_TYPE_LIST)
        return DS_RESULT_INVALID_ARGUMENT;
    DSValue  child;
    DSResult result = DSGetListValue(&value.m_Value.m_List, index, &child);
    if (result == DS_RESULT_OK)
        WriteView(child, out_view);
    return result;
}

DSViewIterator DSViewIter(const DSValueView* view)
{
    uint32_t count = 0;
    DSViewGetCount(view, &count);
    DSViewIterator iterator = { .m_Parent = view, .m_Count = count };
    return iterator;
}

bool DSViewIterNext(DSViewIterator* iterator, DSValueView* out_view)
{
    if (iterator->m_Index == iterator->m_Count)
        return false;
    DSValue parent = ReadView(iterator->m_Parent);
    DSValue child = GetChildValue(parent.m_Type, &parent.m_Value, iterator->m_Index++);
    WriteView(child, out_view);
    return true;
}

DSResult DSViewGetNumber(const DSValueView* view, double* out_value)
{
    DSValue value = ReadView(view);
    if (value.m_Type != DS_TYPE_NUMBER)
        return DS_RESULT_INVALID_ARGUMENT;
    *out_value = value.m_Value.m_Number;
    return DS_RESULT_OK;
}

DSResult DSViewGetFloat(const DSValueView* view, float* out_value)
{
    DSValue value = ReadView(view);
    if (value.m_Type != DS_TYPE_FLOAT)
        return DS_RESULT_INVALID_ARGUMENT;
    *out_value = value.m_Value.m_Float;
    return DS_RESULT_OK;
}

DSResult DSViewGetUInt32(const DSValueView* view, uint32_t* out_value)
{
    DSValue value = ReadView(view);
    if (value.m_Type != DS_TYPE_UINT32)
        return DS_RESULT_INVALID_ARGUMENT;
    *out_value = value.m_Value.m_UInt32;
    return DS_RESULT_OK;
}

DSResult DSViewGetBoolean(const DSValueView* view, uint8_t* out_value)
{
    DSValue value = ReadView(view);
    if (value.m_Type != DS_TYPE_BOOLEAN)
        return DS_RESULT_INVALID_ARGUMENT;
    *out_value = value.m_Value.m_Boolean;
    return DS_RESULT_OK;
}

DSResult DSViewGetString(const DSValueView* view, const char** out_value)
{
    DSValue value = ReadView(view);
    if (value.m_Type != DS_TYPE_STRING)
        return DS_RESULT_INVALID_ARGUMENT;
    *out_value = value.m_Value.m_String;
    return DS_RESULT_OK;
}

DSResult DSViewGetVector3(const DSValueView* view, DSVector3* out_value)
{
    DSValue value = ReadView(view);
    if (value.m_Type != DS_TYPE_VECTOR3)
        return DS_RESULT_INVALID_ARGUMENT;
    memcpy(out_value, value.m_Value.m_Vector3, sizeof(*out_value));
    return DS_RESULT_OK;
}

DSResult DSViewGetVector4(const DSValueView* view, DSVector4* out_value)
{
    DSValue value = ReadView(view);
    if (value.m_Type != DS_TYPE_VECTOR4)
        return DS_RESULT_INVALID_ARGUMENT;
    memcpy(out_value, value.m_Value.m_Vector4, sizeof(*out_value));
    return DS_RESULT_OK;
}

DSResult DSViewGetMatrix4(const DSValueView* view, DSMatrix4* out_value)
{
    DSValue value = ReadView(view);
    if (value.m_Type != DS_TYPE_MATRIX4)
        return DS_RESULT_INVALID_ARGUMENT;
    memcpy(out_value, value.m_Value.m_Matrix4, sizeof(*out_value));
    return DS_RESULT_OK;
}
