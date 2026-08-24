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

    dmHashTable64<Constant>     m_Constants;
    dmArray<dmVMath::Vector4>   m_Values;
};

// Immutable, single-allocation representation used by queued render commands.
// The descriptors follow the header and the aligned Vector4 data follows them.
struct NamedConstantBufferSnapshot
{
    struct Constant
    {
        dmhash_t                                m_NameHash;
        uint32_t                                m_ValueIndex;
        uint32_t                                m_NumValues;
        dmRenderDDF::MaterialDesc::ConstantType m_Type;
    };

    Constant*         m_Constants;
    dmVMath::Vector4* m_Values;
    uint32_t          m_ConstantCount;
};

struct FillNamedConstantBufferSnapshotContext
{
    NamedConstantBufferSnapshot::Constant* m_Constants;
    uint32_t                                m_Index;
};

static inline void FillNamedConstantBufferSnapshot(FillNamedConstantBufferSnapshotContext* context, const uint64_t* name_hash, NamedConstantBuffer::Constant* constant)
{
    NamedConstantBufferSnapshot::Constant& snapshot_constant = context->m_Constants[context->m_Index++];
    snapshot_constant.m_NameHash   = *name_hash;
    snapshot_constant.m_ValueIndex = constant->m_ValueIndex;
    snapshot_constant.m_NumValues  = constant->m_NumValues;
    snapshot_constant.m_Type       = constant->m_Type;
}

HNamedConstantBufferSnapshot NewNamedConstantBufferSnapshot(HNamedConstantBuffer buffer)
{
    if (!buffer)
        return 0;

    const uint32_t constant_count = buffer->m_Constants.Size();
    const uint32_t value_count = buffer->m_Values.Size();
    const uint32_t values_alignment = alignof(dmVMath::Vector4);
    const uint64_t constants_end = sizeof(NamedConstantBufferSnapshot) + sizeof(NamedConstantBufferSnapshot::Constant) * (uint64_t)constant_count;
    const uint64_t values_offset = (constants_end + values_alignment - 1) & ~(uint64_t)(values_alignment - 1);
    const uint64_t allocation_size = values_offset + sizeof(dmVMath::Vector4) * (uint64_t)value_count;

    if (allocation_size > 0xffffffffU)
        return 0;

    void* memory = 0;
    const uint32_t allocation_alignment = values_alignment > alignof(NamedConstantBufferSnapshot::Constant) ? values_alignment : alignof(NamedConstantBufferSnapshot::Constant);
    if (dmMemory::AlignedMalloc(&memory, allocation_alignment, (uint32_t)allocation_size) != dmMemory::RESULT_OK)
        return 0;

    HNamedConstantBufferSnapshot snapshot = (HNamedConstantBufferSnapshot)memory;
    snapshot->m_Constants = (NamedConstantBufferSnapshot::Constant*)(snapshot + 1);
    snapshot->m_Values = (dmVMath::Vector4*)((uint8_t*)snapshot + values_offset);
    snapshot->m_ConstantCount = constant_count;

    FillNamedConstantBufferSnapshotContext context;
    context.m_Constants = snapshot->m_Constants;
    context.m_Index = 0;
    buffer->m_Constants.Iterate(FillNamedConstantBufferSnapshot, &context);

    if (value_count > 0)
        memcpy(snapshot->m_Values, buffer->m_Values.Begin(), sizeof(dmVMath::Vector4) * value_count);

    return snapshot;
}

void DeleteNamedConstantBufferSnapshot(HNamedConstantBufferSnapshot snapshot)
{
    if (snapshot)
        dmMemory::AlignedFree(snapshot);
}

bool GetNamedConstantSnapshot(HNamedConstantBufferSnapshot snapshot, dmhash_t name_hash, dmVMath::Vector4** values, uint32_t* num_values, dmRenderDDF::MaterialDesc::ConstantType* constant_type)
{
    NamedConstantBufferSnapshot::Constant* constants = snapshot->m_Constants;
    for (uint32_t i = 0; i < snapshot->m_ConstantCount; ++i)
    {
        if (constants[i].m_NameHash == name_hash)
        {
            *values = &snapshot->m_Values[constants[i].m_ValueIndex];
            *num_values = constants[i].m_NumValues;
            *constant_type = constants[i].m_Type;
            return true;
        }
    }
    return false;
}

HNamedConstantBuffer NewNamedConstantBuffer()
{
    HNamedConstantBuffer buffer = new NamedConstantBuffer();
    buffer->m_Constants.SetCapacity(9, 16);
    return buffer;
}

void DeleteNamedConstantBuffer(HNamedConstantBuffer buffer)
{
    delete buffer;
}

void ClearNamedConstantBuffer(HNamedConstantBuffer buffer)
{
    buffer->m_Constants.Clear();
    buffer->m_Values.SetSize(0);
}

struct ShiftConstantsContext
{
    uint32_t m_Index;
    uint32_t m_NumValues : 31;
    uint32_t m_Direction : 1; // 0: left, 1: right
};

static inline void ShiftConstantIndices(ShiftConstantsContext* context, const uint64_t* name_hash, NamedConstantBuffer::Constant* constant)
{
    if (context->m_Direction == 0 && constant->m_ValueIndex > context->m_Index)
    {
        constant->m_ValueIndex -= context->m_NumValues;
    }
    else if (context->m_Direction == 1 && constant->m_ValueIndex > context->m_Index)
    {
        constant->m_ValueIndex += context->m_NumValues;
    }
}

void RemoveNamedConstant(HNamedConstantBuffer buffer, dmhash_t name_hash)
{
    NamedConstantBuffer::Constant* c = buffer->m_Constants.Get(name_hash);
    if (!c)
        return;

    uint32_t values_index = c->m_ValueIndex;
    uint32_t num_values = c->m_NumValues;

    dmVMath::Vector4* p_current = &buffer->m_Values[values_index];
    // shift the data "left" by num_values
    uint32_t remaining = buffer->m_Values.Size() - (values_index + num_values);

    dmVMath::Vector4* p_next = p_current + num_values;
    memmove(p_current, p_next, remaining * sizeof(dmVMath::Vector4)); // if it's the last item, then "remaining" will be 0

    buffer->m_Constants.Erase(name_hash);
    buffer->m_Values.SetSize(buffer->m_Values.Size() - num_values);

    ShiftConstantsContext shift_context;
    shift_context.m_Index     = values_index;
    shift_context.m_NumValues = num_values;
    shift_context.m_Direction = 0;
    buffer->m_Constants.Iterate(ShiftConstantIndices, &shift_context);
}

Result SetNamedConstantAtIndex(HNamedConstantBuffer buffer, dmhash_t name_hash, dmVMath::Vector4* values,
    uint32_t num_values, uint32_t value_index, dmRenderDDF::MaterialDesc::ConstantType constant_type)
{
    dmHashTable64<NamedConstantBuffer::Constant>& constants = buffer->m_Constants;
    NamedConstantBuffer::Constant* c = constants.Get(name_hash);

    uint32_t value_size = value_index + num_values;
    if (c == 0)
    {
        if (constants.Full())
        {
            uint32_t capacity = constants.Capacity() + 8;
            constants.SetCapacity(capacity, capacity * 2);
        }

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
        constants.Put(name_hash, constant);

        // Get the pointer
        c = constants.Get(name_hash);
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
        buffer->m_Constants.Iterate(ShiftConstantIndices, &shift_context);
    }

    dmVMath::Vector4* values_start = &buffer->m_Values[c->m_ValueIndex];
    memcpy(&values_start[value_index], values, sizeof(dmVMath::Vector4) * num_values);

    return RESULT_OK;
}

void SetNamedConstant(HNamedConstantBuffer buffer, dmhash_t name_hash, dmVMath::Vector4* values, uint32_t num_values, dmRenderDDF::MaterialDesc::ConstantType type)
{
    dmHashTable64<NamedConstantBuffer::Constant>& constants = buffer->m_Constants;

    NamedConstantBuffer::Constant* c = constants.Get(name_hash);
    if (c && c->m_NumValues != num_values)
    {
        RemoveNamedConstant(buffer, name_hash);
        c = 0;
    }

    if (c == 0)
    {
        if (constants.Full())
        {
            uint32_t capacity = constants.Capacity() + 8;
            constants.SetCapacity(capacity, capacity * 2);
        }

        if (buffer->m_Values.Remaining() < num_values)
            buffer->m_Values.OffsetCapacity(num_values - buffer->m_Values.Remaining());

        uint32_t values_index = buffer->m_Values.Size();

        buffer->m_Values.SetSize(buffer->m_Values.Size() + num_values);

        NamedConstantBuffer::Constant constant;
        constant.m_NameHash    = name_hash;
        constant.m_NumValues   = num_values;
        constant.m_ValueIndex  = values_index;
        constant.m_Type        = type;
        constants.Put(name_hash, constant);

        // Get the pointer
        c = constants.Get(name_hash);
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
    NamedConstantBuffer::Constant* c = buffer->m_Constants.Get(name_hash);
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

struct IterateConstantCtx
{
    void (*m_Callback)(dmhash_t name_hash, void* ctx);
    void* m_Ctx;
};

static inline void IterateConstants(IterateConstantCtx* context, const uint64_t* name_hash, NamedConstantBuffer::Constant* constant)
{
    context->m_Callback(constant->m_NameHash, context->m_Ctx);
}

void IterateNamedConstants(HNamedConstantBuffer buffer, IterateNamedConstantsFn callback, void* ctx)
{
    IterateConstantCtx context;
    context.m_Ctx = ctx;
    context.m_Callback = callback;
    buffer->m_Constants.Iterate(IterateConstants, &context);
}

struct ApplyConstantContext
{
    dmGraphics::HContext m_GraphicsContext;
    HNamedConstantBuffer m_ConstantBuffer;

    union
    {
        HMaterial       m_Material;
        HComputeProgram m_ComputeProgram;
    };

    ApplyConstantContext(dmGraphics::HContext graphics_context, HMaterial material, HNamedConstantBuffer constant_buffer)
    {
        m_GraphicsContext = graphics_context;
        m_Material        = material;
        m_ConstantBuffer  = constant_buffer;
    }

    ApplyConstantContext(dmGraphics::HContext graphics_context, HComputeProgram program, HNamedConstantBuffer constant_buffer)
    {
        m_GraphicsContext = graphics_context;
        m_ComputeProgram  = program;
        m_ConstantBuffer  = constant_buffer;
    }
};

static inline void ApplyConstant(ApplyConstantContext* context, const uint64_t* name_hash, NamedConstantBuffer::Constant* constant)
{
    dmGraphics::HUniformLocation* location = context->m_Material->m_NameHashToLocation.Get(*name_hash);
    if (location)
    {
        dmVMath::Vector4* values = &context->m_ConstantBuffer->m_Values[constant->m_ValueIndex];

        if (constant->m_Type == dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4)
        {
            dmGraphics::SetConstantM4(context->m_GraphicsContext, values, constant->m_NumValues / 4, *location);
        }
        else
        {
            dmGraphics::SetConstantV4(context->m_GraphicsContext, values, constant->m_NumValues, *location);
        }
    }
}

void ApplyNamedConstantBuffer(dmRender::HRenderContext render_context, HMaterial material, HNamedConstantBuffer buffer)
{
    dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
    ApplyConstantContext context(graphics_context, material, buffer);
    buffer->m_Constants.Iterate(ApplyConstant, &context);
}

static inline void ApplyConstantCompute(ApplyConstantContext* context, const uint64_t* name_hash, NamedConstantBuffer::Constant* constant)
{
    dmGraphics::HUniformLocation* location = context->m_ComputeProgram->m_NameHashToLocation.Get(*name_hash);
    if (location)
    {
        dmVMath::Vector4* values = &context->m_ConstantBuffer->m_Values[constant->m_ValueIndex];

        if (constant->m_Type == dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4)
        {
            dmGraphics::SetConstantM4(context->m_GraphicsContext, values, constant->m_NumValues / 4, *location);
        }
        else
        {
            dmGraphics::SetConstantV4(context->m_GraphicsContext, values, constant->m_NumValues, *location);
        }
    }
}

void ApplyNamedConstantBuffer(dmRender::HRenderContext render_context, HComputeProgram program, HNamedConstantBuffer buffer)
{
    dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
    ApplyConstantContext context(graphics_context, program, buffer);
    buffer->m_Constants.Iterate(ApplyConstantCompute, &context);
}

void ApplyNamedConstantBufferSnapshot(dmRender::HRenderContext render_context, HMaterial material, HNamedConstantBufferSnapshot snapshot)
{
    dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
    NamedConstantBufferSnapshot::Constant* constants = snapshot->m_Constants;
    dmVMath::Vector4* values = snapshot->m_Values;

    for (uint32_t i = 0; i < snapshot->m_ConstantCount; ++i)
    {
        NamedConstantBufferSnapshot::Constant& constant = constants[i];
        dmGraphics::HUniformLocation* location = material->m_NameHashToLocation.Get(constant.m_NameHash);
        if (!location)
            continue;

        dmVMath::Vector4* constant_values = &values[constant.m_ValueIndex];
        if (constant.m_Type == dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4)
            dmGraphics::SetConstantM4(graphics_context, constant_values, constant.m_NumValues / 4, *location);
        else
            dmGraphics::SetConstantV4(graphics_context, constant_values, constant.m_NumValues, *location);
    }
}

void ApplyNamedConstantBufferSnapshot(dmRender::HRenderContext render_context, HComputeProgram program, HNamedConstantBufferSnapshot snapshot)
{
    dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
    NamedConstantBufferSnapshot::Constant* constants = snapshot->m_Constants;
    dmVMath::Vector4* values = snapshot->m_Values;

    for (uint32_t i = 0; i < snapshot->m_ConstantCount; ++i)
    {
        NamedConstantBufferSnapshot::Constant& constant = constants[i];
        dmGraphics::HUniformLocation* location = program->m_NameHashToLocation.Get(constant.m_NameHash);
        if (!location)
            continue;

        dmVMath::Vector4* constant_values = &values[constant.m_ValueIndex];
        if (constant.m_Type == dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4)
            dmGraphics::SetConstantM4(graphics_context, constant_values, constant.m_NumValues / 4, *location);
        else
            dmGraphics::SetConstantV4(graphics_context, constant_values, constant.m_NumValues, *location);
    }
}

}
