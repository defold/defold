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

#include <dlib/memory.h>
#include <render/render.h>
#include <render/render_private.h>

namespace dmRender
{

/////////////////////////////////////////////////////////////////////////////////////////////////////////

Constant::Constant() {}
Constant::Constant(dmhash_t name_hash, dmGraphics::HUniformLocation location)
    : m_Values(0)
    , m_NameHash(name_hash)
    , m_Type(dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER)
    , m_Location(location)
    , m_NumValues(0)
{
}

HConstant NewConstant(dmhash_t name_hash)
{
    return new Constant(name_hash, -1);
}

void DeleteConstant(HConstant constant)
{
   if (constant->m_AllocatedValues)
        dmMemory::AlignedFree(constant->m_Values);
    delete constant;
}

dmVMath::Vector4* GetConstantValues(HConstant constant, uint32_t* num_values)
{
    *num_values = constant->m_NumValues;
    return constant->m_Values;
}

Result SetConstantValues(HConstant constant, dmVMath::Vector4* values, uint32_t num_values)
{
    if (num_values > constant->m_NumValues)
    {
        dmVMath::Vector4* newmem = 0;
        if (dmMemory::RESULT_OK != dmMemory::AlignedMalloc((void**)&newmem, 16,  num_values * sizeof(dmVMath::Vector4)))
        {
            return RESULT_OUT_OF_RESOURCES;
        }
        if (constant->m_AllocatedValues)
            dmMemory::AlignedFree(constant->m_Values);
        constant->m_Values = newmem;
    }

    memcpy(constant->m_Values, values, num_values * sizeof(dmVMath::Vector4));
    constant->m_NumValues = num_values;
    constant->m_AllocatedValues = 1;

    return dmRender::RESULT_OK;
}

Result SetConstantValuesRef(HConstant constant, dmVMath::Vector4* values, uint32_t num_values)
{
   if (constant->m_AllocatedValues)
        dmMemory::AlignedFree(constant->m_Values);

    constant->m_AllocatedValues = 0;
    constant->m_NumValues = num_values;
    constant->m_Values    = values;

    return dmRender::RESULT_OK;
}

dmhash_t GetConstantName(HConstant constant)
{
    return constant->m_NameHash;
}

void SetConstantName(HConstant constant, dmhash_t name)
{
    constant->m_NameHash = name;
}

dmGraphics::HUniformLocation GetConstantLocation(HConstant constant)
{
    return constant->m_Location;
}

void SetConstantLocation(HConstant constant, dmGraphics::HUniformLocation location)
{
    constant->m_Location = location;
}

dmRenderDDF::MaterialDesc::ConstantType GetConstantType(HConstant constant)
{
    return constant->m_Type;
}

void SetConstantType(HConstant constant, dmRenderDDF::MaterialDesc::ConstantType type)
{
    constant->m_Type = type;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

struct NamedConstantBuffer
{
    struct Constant
    {
        dmhash_t                                m_NameHash;
        uint32_t                                m_ValueIndex;
        uint32_t                                m_NumValues;
        dmRenderDDF::MaterialDesc::ConstantType m_Type;
    };

    dmArray<Constant>           m_Constants;
    dmArray<dmVMath::Vector4>   m_Values;
};

static inline NamedConstantBuffer::Constant* FindNamedConstant(HNamedConstantBuffer buffer, dmhash_t name_hash)
{
    for (uint32_t i = 0; i < buffer->m_Constants.Size(); ++i)
    {
        if (buffer->m_Constants[i].m_NameHash == name_hash)
            return &buffer->m_Constants[i];
    }
    return 0;
}

HNamedConstantBuffer NewNamedConstantBuffer()
{
    HNamedConstantBuffer buffer = new NamedConstantBuffer();
    buffer->m_Constants.SetCapacity(16);
    return buffer;
}

void DeleteNamedConstantBuffer(HNamedConstantBuffer buffer)
{
    delete buffer;
}

void ClearNamedConstantBuffer(HNamedConstantBuffer buffer)
{
    buffer->m_Constants.SetSize(0);
    buffer->m_Values.SetSize(0);
}

void CopyNamedConstantBuffer(HNamedConstantBuffer destination, HNamedConstantBuffer source)
{
    if (destination == source)
        return;

    const uint32_t constant_count = source->m_Constants.Size();
    const uint32_t value_count = source->m_Values.Size();

    if (destination->m_Constants.Capacity() < constant_count)
        destination->m_Constants.SetCapacity(constant_count);
    if (destination->m_Values.Capacity() < value_count)
        destination->m_Values.SetCapacity(value_count);

    destination->m_Constants.SetSize(constant_count);
    destination->m_Values.SetSize(value_count);

    if (constant_count > 0)
        memcpy(destination->m_Constants.Begin(), source->m_Constants.Begin(), constant_count * sizeof(NamedConstantBuffer::Constant));
    if (value_count > 0)
        memcpy(destination->m_Values.Begin(), source->m_Values.Begin(), value_count * sizeof(dmVMath::Vector4));
}

struct ShiftConstantsContext
{
    uint32_t m_Index;
    uint32_t m_NumValues : 31;
    uint32_t m_Direction : 1; // 0: left, 1: right
};

static inline void ShiftConstantIndices(HNamedConstantBuffer buffer, const ShiftConstantsContext& context)
{
    for (uint32_t i = 0; i < buffer->m_Constants.Size(); ++i)
    {
        NamedConstantBuffer::Constant& constant = buffer->m_Constants[i];
        if (context.m_Direction == 0 && constant.m_ValueIndex > context.m_Index)
            constant.m_ValueIndex -= context.m_NumValues;
        else if (context.m_Direction == 1 && constant.m_ValueIndex > context.m_Index)
            constant.m_ValueIndex += context.m_NumValues;
    }
}

void RemoveNamedConstant(HNamedConstantBuffer buffer, dmhash_t name_hash)
{
    NamedConstantBuffer::Constant* c = FindNamedConstant(buffer, name_hash);
    if (!c)
        return;

    uint32_t values_index = c->m_ValueIndex;
    uint32_t num_values = c->m_NumValues;

    dmVMath::Vector4* p_current = &buffer->m_Values[values_index];
    // shift the data "left" by num_values
    uint32_t remaining = buffer->m_Values.Size() - (values_index + num_values);

    dmVMath::Vector4* p_next = p_current + num_values;
    memmove(p_current, p_next, remaining * sizeof(dmVMath::Vector4)); // if it's the last item, then "remaining" will be 0

    uint32_t constant_index = (uint32_t)(c - buffer->m_Constants.Begin());
    buffer->m_Constants.EraseSwap(constant_index);
    buffer->m_Values.SetSize(buffer->m_Values.Size() - num_values);

    ShiftConstantsContext shift_context;
    shift_context.m_Index     = values_index;
    shift_context.m_NumValues = num_values;
    shift_context.m_Direction = 0;
    ShiftConstantIndices(buffer, shift_context);
}

Result SetNamedConstantAtIndex(HNamedConstantBuffer buffer, dmhash_t name_hash, dmVMath::Vector4* values,
    uint32_t num_values, uint32_t value_index, dmRenderDDF::MaterialDesc::ConstantType constant_type)
{
    dmArray<NamedConstantBuffer::Constant>& constants = buffer->m_Constants;
    NamedConstantBuffer::Constant* c = FindNamedConstant(buffer, name_hash);

    uint32_t value_size = value_index + num_values;
    if (c == 0)
    {
        if (constants.Full())
            constants.OffsetCapacity(8);

        if (buffer->m_Values.Remaining() < value_size)
        {
            buffer->m_Values.OffsetCapacity(value_size - buffer->m_Values.Remaining());
        }

        uint32_t values_index = buffer->m_Values.Size();
        buffer->m_Values.SetSize(buffer->m_Values.Size() + value_size);

        NamedConstantBuffer::Constant constant;
        constant.m_NameHash    = name_hash;
        constant.m_NumValues   = value_size;
        constant.m_ValueIndex  = values_index;
        constant.m_Type        = constant_type;
        constants.Push(constant);
        c = &constants.Back();
    }
    else if (c->m_NumValues > 0 && c->m_Type != constant_type)
    {
        return RESULT_TYPE_MISMATCH;
    }
    else if (c->m_NumValues < value_size)
    {
        uint32_t values_index      = c->m_ValueIndex;
        uint32_t num_values        = c->m_NumValues;
        uint32_t num_values_expand = value_size - num_values;

        if (buffer->m_Values.Remaining() < num_values_expand)
        {
            buffer->m_Values.OffsetCapacity(num_values_expand);
        }

        buffer->m_Values.SetSize(buffer->m_Values.Size() + num_values_expand);

        uint32_t num_values_to_move = buffer->m_Values.Size() - value_size - values_index;

        dmVMath::Vector4* p_src  = &buffer->m_Values[values_index] + num_values;
        dmVMath::Vector4* p_dest = p_src + num_values_expand;

        // Clear all intermediate values to zero so we don't keep old data if
        // the constant has grown more than one index
        memset(p_src, 0, (p_dest - p_src) * sizeof(dmVMath::Vector4));

        memmove(p_dest, p_src, num_values_to_move * sizeof(dmVMath::Vector4));

        // update constant indices
        c->m_NumValues = value_size;

        ShiftConstantsContext shift_context;
        shift_context.m_Index     = values_index;
        shift_context.m_NumValues = num_values_expand;
        shift_context.m_Direction = 1;
        ShiftConstantIndices(buffer, shift_context);
    }

    dmVMath::Vector4* values_start = &buffer->m_Values[c->m_ValueIndex];
    memcpy(&values_start[value_index], values, sizeof(dmVMath::Vector4) * num_values);

    return RESULT_OK;
}

void SetNamedConstant(HNamedConstantBuffer buffer, dmhash_t name_hash, dmVMath::Vector4* values, uint32_t num_values, dmRenderDDF::MaterialDesc::ConstantType type)
{
    dmArray<NamedConstantBuffer::Constant>& constants = buffer->m_Constants;

    NamedConstantBuffer::Constant* c = FindNamedConstant(buffer, name_hash);
    if (c && c->m_NumValues != num_values)
    {
        RemoveNamedConstant(buffer, name_hash);
        c = 0;
    }

    if (c == 0)
    {
        if (constants.Full())
            constants.OffsetCapacity(8);

        if (buffer->m_Values.Remaining() < num_values)
            buffer->m_Values.OffsetCapacity(num_values - buffer->m_Values.Remaining());

        uint32_t values_index = buffer->m_Values.Size();

        buffer->m_Values.SetSize(buffer->m_Values.Size() + num_values);

        NamedConstantBuffer::Constant constant;
        constant.m_NameHash    = name_hash;
        constant.m_NumValues   = num_values;
        constant.m_ValueIndex  = values_index;
        constant.m_Type        = type;
        constants.Push(constant);
        c = &constants.Back();
    }

    dmVMath::Vector4* p = &buffer->m_Values[c->m_ValueIndex];
    memcpy(p, values, sizeof(values[0]) * num_values);
}

void SetNamedConstant(HNamedConstantBuffer buffer, dmhash_t name_hash, dmVMath::Vector4* values, uint32_t num_values)
{
    SetNamedConstant(buffer, name_hash, values, num_values, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER);
}

void SetNamedConstants(HNamedConstantBuffer buffer, HConstant* constants, uint32_t num_constants)
{
    for (uint32_t i = 0; i < num_constants; ++i)
    {
        Constant* c = constants[i];
        SetNamedConstant(buffer, c->m_NameHash, c->m_Values, c->m_NumValues, c->m_Type);
    }
}

bool GetNamedConstant(HNamedConstantBuffer buffer, dmhash_t name_hash, dmVMath::Vector4** values, uint32_t* num_values)
{
    dmRenderDDF::MaterialDesc::ConstantType constant_type;
    return GetNamedConstant(buffer, name_hash, values, num_values, &constant_type);
}

bool GetNamedConstant(HNamedConstantBuffer buffer, dmhash_t name_hash, dmVMath::Vector4** values, uint32_t* num_values, dmRenderDDF::MaterialDesc::ConstantType* constant_type)
{
    NamedConstantBuffer::Constant* c = FindNamedConstant(buffer, name_hash);
    if (!c)
        return false;

    *values = &buffer->m_Values[c->m_ValueIndex];
    *num_values = c->m_NumValues;
    *constant_type = c->m_Type;
    return true;
}

uint32_t GetNamedConstantCount(HNamedConstantBuffer buffer)
{
    return buffer->m_Constants.Size();
}

void IterateNamedConstants(HNamedConstantBuffer buffer, IterateNamedConstantsFn callback, void* ctx)
{
    for (uint32_t i = 0; i < buffer->m_Constants.Size(); ++i)
        callback(buffer->m_Constants[i].m_NameHash, ctx);
}

void ApplyNamedConstantBuffer(dmRender::HRenderContext render_context, HMaterial material, HNamedConstantBuffer buffer)
{
    dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
    for (uint32_t i = 0; i < buffer->m_Constants.Size(); ++i)
    {
        NamedConstantBuffer::Constant& constant = buffer->m_Constants[i];
        dmGraphics::HUniformLocation* location = material->m_NameHashToLocation.Get(constant.m_NameHash);
        if (!location)
            continue;

        dmVMath::Vector4* values = &buffer->m_Values[constant.m_ValueIndex];
        if (constant.m_Type == dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4)
            dmGraphics::SetConstantM4(graphics_context, values, constant.m_NumValues / 4, *location);
        else
            dmGraphics::SetConstantV4(graphics_context, values, constant.m_NumValues, *location);
    }
}

void ApplyNamedConstantBuffer(dmRender::HRenderContext render_context, HComputeProgram program, HNamedConstantBuffer buffer)
{
    dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
    for (uint32_t i = 0; i < buffer->m_Constants.Size(); ++i)
    {
        NamedConstantBuffer::Constant& constant = buffer->m_Constants[i];
        dmGraphics::HUniformLocation* location = program->m_NameHashToLocation.Get(constant.m_NameHash);
        if (!location)
            continue;

        dmVMath::Vector4* constant_values = &buffer->m_Values[constant.m_ValueIndex];
        if (constant.m_Type == dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4)
            dmGraphics::SetConstantM4(graphics_context, constant_values, constant.m_NumValues / 4, *location);
        else
            dmGraphics::SetConstantV4(graphics_context, constant_values, constant.m_NumValues, *location);
    }
}

}
