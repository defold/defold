// Copyright 2020-2022 The Defold Foundation
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

#include "comp_private.h"
#include "resources/res_textureset.h"
#include <dlib/log.h>
#include <dmsdk/dlib/vmath.h>
#include <render/render.h>
#include <dmsdk/gamesys/render_constants.h>

namespace dmGameSystem
{

using namespace dmVMath;

dmGameObject::PropertyResult GetProperty(dmGameObject::PropertyDesc& out_value, dmhash_t get_property, const Vector3& ref_value, const PropVector3& property)
{
    dmGameObject::PropertyResult result = dmGameObject::PROPERTY_RESULT_OK;

    // We deliberately do not provide a value pointer, two reasons:
    // 1. Deleting a gameobject (that included sprite(s)) will rearrange the
    //    object pool for components (due to EraseSwap in the Free for the object pool);
    //    this result in the original animation value pointer will still point
    //    to the original memory location in the component object pool.
    // 2. If it's a read only variable, we can't do it in order to ensure that it
    //    is not used in any write optimisation (see animation system).
    out_value.m_ReadOnly = property.m_ReadOnly;
    out_value.m_ValuePtr = 0x0;
    if (get_property == property.m_Vector)
    {
        out_value.m_ElementIds[0] = property.m_X;
        out_value.m_ElementIds[1] = property.m_Y;
        out_value.m_ElementIds[2] = property.m_Z;
        out_value.m_Variant = dmGameObject::PropertyVar(ref_value);
    }
    else if (get_property == property.m_X)
    {
        out_value.m_Variant = dmGameObject::PropertyVar(ref_value.getX());
    }
    else if (get_property == property.m_Y)
    {
        out_value.m_Variant = dmGameObject::PropertyVar(ref_value.getY());
    }
    else if (get_property == property.m_Z)
    {
        out_value.m_Variant = dmGameObject::PropertyVar(ref_value.getZ());
    }
    else
    {
        result = dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    return result;
}


dmGameObject::PropertyResult SetProperty(dmhash_t set_property, const dmGameObject::PropertyVar& in_value, Vector3& set_value, const PropVector3& property)
{
    dmGameObject::PropertyResult result = dmGameObject::PROPERTY_RESULT_OK;

    if (property.m_ReadOnly)
    {
        result = dmGameObject::PROPERTY_RESULT_UNSUPPORTED_OPERATION;
    }

    if (set_property == property.m_Vector)
    {
        if (in_value.m_Type == dmGameObject::PROPERTY_TYPE_VECTOR3)
        {
            set_value.setX(in_value.m_V4[0]);
            set_value.setY(in_value.m_V4[1]);
            set_value.setZ(in_value.m_V4[2]);
        }
        else
        {
            result = dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
        }
    }
    else if (set_property == property.m_X)
    {
        if (in_value.m_Type == dmGameObject::PROPERTY_TYPE_NUMBER)
        {
            set_value.setX(in_value.m_Number);
        }
        else
        {
            result = dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
        }
    }
    else if (set_property == property.m_Y)
    {
        if (in_value.m_Type == dmGameObject::PROPERTY_TYPE_NUMBER)
        {
            set_value.setY(in_value.m_Number);
        }
        else
        {
            result = dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
        }
    }
    else if (set_property == property.m_Z)
    {
        if (in_value.m_Type == dmGameObject::PROPERTY_TYPE_NUMBER)
        {
            set_value.setZ(in_value.m_Number);
        }
        else
        {
            result = dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
        }
    }
    else
    {
        result = dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    return result;
}


dmGameObject::PropertyResult GetProperty(dmGameObject::PropertyDesc& out_value, dmhash_t get_property, const Vector4& ref_value, const PropVector4& property)
{
    dmGameObject::PropertyResult result = dmGameObject::PROPERTY_RESULT_OK;

    // We deliberately do not provide a value pointer, two reasons:
    // 1. Deleting a gameobject (that included sprite(s)) will rearrange the
    //    object pool for components (due to EraseSwap in the Free for the object pool);
    //    this result in the original animation value pointer will still point
    //    to the original memory location in the component object pool.
    // 2. If it's a read only variable, we can't do it in order to ensure that it
    //    is not used in any write optimisation (see animation system).
    out_value.m_ReadOnly = property.m_ReadOnly;
    out_value.m_ValuePtr = 0x0;
    if (get_property == property.m_Vector)
    {
        out_value.m_ElementIds[0] = property.m_X;
        out_value.m_ElementIds[1] = property.m_Y;
        out_value.m_ElementIds[2] = property.m_Z;
        out_value.m_ElementIds[3] = property.m_W;
        out_value.m_Variant = dmGameObject::PropertyVar(ref_value);
    }
    else if (get_property == property.m_X)
    {
        out_value.m_Variant = dmGameObject::PropertyVar(ref_value.getX());
    }
    else if (get_property == property.m_Y)
    {
        out_value.m_Variant = dmGameObject::PropertyVar(ref_value.getY());
    }
    else if (get_property == property.m_Z)
    {
        out_value.m_Variant = dmGameObject::PropertyVar(ref_value.getZ());
    }
    else if (get_property == property.m_W)
    {
        out_value.m_Variant = dmGameObject::PropertyVar(ref_value.getW());
    }
    else
    {
        result = dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    return result;
}


dmGameObject::PropertyResult SetProperty(dmhash_t set_property, const dmGameObject::PropertyVar& in_value, Vector4& set_value, const PropVector4& property)
{
    dmGameObject::PropertyResult result = dmGameObject::PROPERTY_RESULT_OK;

    if (property.m_ReadOnly)
    {
        result = dmGameObject::PROPERTY_RESULT_UNSUPPORTED_OPERATION;
    }

    if (set_property == property.m_Vector)
    {
        if (in_value.m_Type == dmGameObject::PROPERTY_TYPE_VECTOR4)
        {
            set_value = Vector4(in_value.m_V4[0], in_value.m_V4[1], in_value.m_V4[2], in_value.m_V4[3]);
        }
        else
        {
            result = dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
        }
    }
    else if (set_property == property.m_X)
    {
        if (in_value.m_Type == dmGameObject::PROPERTY_TYPE_NUMBER)
        {
            set_value.setX(in_value.m_Number);
        }
        else
        {
            result = dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
        }
    }
    else if (set_property == property.m_Y)
    {
        if (in_value.m_Type == dmGameObject::PROPERTY_TYPE_NUMBER)
        {
            set_value.setY(in_value.m_Number);
        }
        else
        {
            result = dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
        }
    }
    else if (set_property == property.m_Z)
    {
        if (in_value.m_Type == dmGameObject::PROPERTY_TYPE_NUMBER)
        {
            set_value.setZ(in_value.m_Number);
        }
        else
        {
            result = dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
        }
    }
    else if (set_property == property.m_W)
    {
        if (in_value.m_Type == dmGameObject::PROPERTY_TYPE_NUMBER)
        {
            set_value.setW(in_value.m_Number);
        }
        else
        {
            result = dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
        }
    }
    else
    {
        result = dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    return result;
}

dmGameObject::PropertyResult GetResourceProperty(dmResource::HFactory factory, void* resource, dmGameObject::PropertyDesc& out_value)
{
    dmhash_t path;
    dmResource::Result res = dmResource::GetPath(factory, resource, &path);
    if (res == dmResource::RESULT_OK)
    {
        out_value.m_Variant = path;
        return dmGameObject::PROPERTY_RESULT_OK;
    }
    return dmGameObject::PROPERTY_RESULT_RESOURCE_NOT_FOUND;
}

dmGameObject::PropertyResult SetResourceProperty(dmResource::HFactory factory, const dmGameObject::PropertyVar& value, dmhash_t* exts, uint32_t ext_count, void** out_resource)
{
    if (value.m_Type != dmGameObject::PROPERTY_TYPE_HASH) {
        return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
    }
    dmResource::SResourceDescriptor rd;
    dmResource::Result res = dmResource::GetDescriptorWithExt(factory, value.m_Hash, exts, ext_count, &rd);
    if (res == dmResource::RESULT_OK)
    {
        if (*out_resource != rd.m_Resource) {
            dmResource::IncRef(factory, rd.m_Resource);
            if (*out_resource) {
                dmResource::Release(factory, *out_resource);
            }
            *out_resource = rd.m_Resource;
        }
        return dmGameObject::PROPERTY_RESULT_OK;
    }
    switch (res)
    {
    case dmResource::RESULT_INVALID_FILE_EXTENSION:
        return dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE;
    default:
        return dmGameObject::PROPERTY_RESULT_RESOURCE_NOT_FOUND;
    }
}

dmGameObject::PropertyResult SetResourceProperty(dmResource::HFactory factory, const dmGameObject::PropertyVar& value, dmhash_t ext, void** out_resource)
{
    return SetResourceProperty(factory, value, &ext, 1, out_resource);
}

// ********************************************************************************************************************************************

struct CompRenderConstants
{
    CompRenderConstants();
    dmArray<dmRender::HConstant>    m_RenderConstants;
    dmHashTable64<dmhash_t>         m_ConstantChecksums; // Maps name_hash to data hash
    dmRender::HNamedConstantBuffer  m_ConstantBuffer;
    bool                            m_Updated; // true if the hashes of the values has changed
};

CompRenderConstants::CompRenderConstants()
{
    // We need to make sure these arrays aren't reallocated, since we might hand pointers off to comp_anim.cpp
    m_RenderConstants.SetCapacity(4);
    m_ConstantChecksums.SetCapacity(5, 8);
    m_ConstantBuffer = dmRender::NewNamedConstantBuffer();
    m_Updated = false;
}

HComponentRenderConstants CreateRenderConstants()
{
    return new CompRenderConstants;
}

void DestroyRenderConstants(HComponentRenderConstants constants)
{
    dmRender::DeleteNamedConstantBuffer(constants->m_ConstantBuffer);
    delete constants;
}

uint32_t GetRenderConstantCount(HComponentRenderConstants constants)
{
    return constants->m_RenderConstants.Size();
}

dmRender::HConstant GetRenderConstant(HComponentRenderConstants constants, uint32_t index)
{
    return constants->m_RenderConstants[index];
}

static int FindRenderConstant(HComponentRenderConstants constants, dmhash_t name_hash)
{
    int count = (int)constants->m_RenderConstants.Size();
    for (int i = 0; i < count; ++i)
    {
        dmhash_t constant_name_hash = GetConstantName(constants->m_RenderConstants[i]);
        if (constant_name_hash == name_hash)
        {
            return i;
        }
    }
    return -1;
}

bool GetRenderConstant(HComponentRenderConstants constants, dmhash_t name_hash, dmRender::HConstant* out_constant)
{
    int index = FindRenderConstant(constants, name_hash);
    if (index < 0)
        return false;
    *out_constant = constants->m_RenderConstants[index];
    return true;
}

static inline dmhash_t HashConstant(dmhash_t name_hash, const dmVMath::Vector4* values, uint32_t num_values)
{
    HashState32 state;
    dmHashInit32(&state, false);
    dmHashUpdateBuffer32(&state, &name_hash, sizeof(name_hash));
    dmHashUpdateBuffer32(&state, values, sizeof(values[0]) * num_values);
    return dmHashFinal32(&state);
}

// TODO: Is a hash really necessary? Isn't enough to compare the bits before/after?
//       Previously we did "if (lengthSqr(value - prev_value) > 0)"
static void UpdateChecksums(HComponentRenderConstants constants, dmhash_t name_hash, dmVMath::Vector4* values, uint32_t num_values)
{
    // Update hash
    dmhash_t new_hash = HashConstant(name_hash, values, num_values);
    dmhash_t* old_hash = constants->m_ConstantChecksums.Get(name_hash);

    if (old_hash == 0 || *old_hash != new_hash)
    {
        constants->m_Updated = true;
    }

    if (constants->m_ConstantChecksums.Full())
    {
        uint32_t capacity = constants->m_ConstantChecksums.Capacity() + 8;
        constants->m_ConstantChecksums.SetCapacity(capacity, capacity*2);
    }

    constants->m_ConstantChecksums.Put(name_hash, new_hash);
}

static dmRender::HConstant FindOrCreateConstant(HComponentRenderConstants constants, dmhash_t name_hash, dmRender::HMaterial material)
{
    int index = FindRenderConstant(constants, name_hash);
    if (index >= 0)
    {
        return constants->m_RenderConstants[index];
    }
    // it didn't exist, so we'll add it
    dmRender::HConstant constant = dmRender::NewConstant(name_hash);
    if (constants->m_RenderConstants.Full())
    {
        constants->m_RenderConstants.OffsetCapacity(4);
    }
    constants->m_RenderConstants.Push(constant);

    dmRender::HConstant material_constant;
    if (!dmRender::GetMaterialProgramConstant(material, name_hash, material_constant))
    {
        return 0;
    }

    uint32_t num_values;
    dmVMath::Vector4* values = dmRender::GetConstantValues(material_constant, &num_values);
    dmRenderDDF::MaterialDesc::ConstantType constant_type = dmRender::GetConstantType(material_constant);

    if (values)
    {
        dmRender::SetConstantValues(constant, values, num_values);
        dmRender::SetConstantType(constant, constant_type);
    } else {
        if (constant_type == dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4)
        {
            dmVMath::Matrix4 zero(0.0f);
            dmRender::SetConstantValues(constant, (dmVMath::Vector4*) &zero, 4);
        }
        else
        {
            dmVMath::Vector4 zero(0,0,0,0);
            dmRender::SetConstantValues(constant, &zero, 1);
        }
    }

    return constant;
}

static dmRender::HConstant FindOrCreateConstant(HComponentRenderConstants constants, dmhash_t name_hash)
{
    int index = FindRenderConstant(constants, name_hash);
    if (index >= 0)
    {
        return constants->m_RenderConstants[index];
    }
    // it didn't exist, so we'll add it
    dmRender::HConstant constant = dmRender::NewConstant(name_hash);
    if (constants->m_RenderConstants.Full())
    {
        constants->m_RenderConstants.OffsetCapacity(4);
    }
    constants->m_RenderConstants.Push(constant);
    return constant;
}

void SetRenderConstant(HComponentRenderConstants constants, dmRender::HMaterial material, dmhash_t name_hash, int32_t value_index, uint32_t* element_index, const dmGameObject::PropertyVar& var)
{
    dmRender::HConstant constant = FindOrCreateConstant(constants, name_hash, material);

    uint32_t num_values = 0;
    dmVMath::Vector4* values = dmRender::GetConstantValues(constant, &num_values);
    bool is_matrix4_type = dmRender::GetConstantType(constant) == dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4;

    if (is_matrix4_type)
    {
        value_index *= 4;
    }

    if (value_index >= num_values)
    {
        dmLogError("Tried to set index outside of bounds for property %s[%u]: %u", dmHashReverseSafe64(name_hash), num_values, value_index);
        return; // We should really handle
    }
    dmVMath::Vector4* v = &values[value_index];

    if (is_matrix4_type)
    {
        if (element_index != 0x0)
        {
            dmLogError("Setting a specific element in a matrix constant for the property %s[%u] is not supported.", dmHashReverseSafe64(name_hash), value_index);
            return;
        }
        memcpy(v, var.m_M4, sizeof(var.m_M4));
    }
    else
    {
        if (element_index == 0x0)
            *v = Vector4(var.m_V4[0], var.m_V4[1], var.m_V4[2], var.m_V4[3]);
        else
            v->setElem(*element_index, (float)var.m_Number);
    }

    UpdateChecksums(constants, name_hash, values, num_values);
}

void SetRenderConstant(HComponentRenderConstants constants, dmhash_t name_hash, dmVMath::Vector4* values, uint32_t num_values)
{
    dmRender::HConstant constant = FindOrCreateConstant(constants, name_hash);
    dmRender::SetConstantValues(constant, values, num_values);
    UpdateChecksums(constants, name_hash, values, num_values);
}

int ClearRenderConstant(HComponentRenderConstants constants, dmhash_t name_hash)
{
    int index = FindRenderConstant(constants, name_hash);
    if (index >= 0)
    {
        constants->m_RenderConstants.EraseSwap(index);
    }

    dmhash_t* checksum = constants->m_ConstantChecksums.Get(name_hash);
    if (checksum)
    {
        constants->m_ConstantChecksums.Erase(name_hash);
        constants->m_Updated |= checksum != 0; // did it exist?
    }
    return checksum != 0 ? 1 : 0;
}

void HashRenderConstants(HComponentRenderConstants constants, HashState32* state)
{
    // Padding in the SetConstant-struct forces us to hash the individual fields

    uint32_t size = constants->m_RenderConstants.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        dmRender::HConstant constant = constants->m_RenderConstants[i];
        uint32_t num_values;
        dmVMath::Vector4* values = dmRender::GetConstantValues(constant, &num_values);
        dmhash_t name_hash = dmRender::GetConstantName(constant);
        dmHashUpdateBuffer32(state, &name_hash, sizeof(name_hash));
        dmHashUpdateBuffer32(state, values, sizeof(values[0]) * num_values);
    }

    constants->m_Updated = false;
}

int AreRenderConstantsUpdated(HComponentRenderConstants constants)
{
    return constants->m_Updated ? 1 : 0;
}

void EnableRenderObjectConstants(dmRender::RenderObject* ro, HComponentRenderConstants constants)
{
    ro->m_ConstantBuffer = constants->m_ConstantBuffer;
    dmRender::ClearNamedConstantBuffer(ro->m_ConstantBuffer);
    dmRender::SetNamedConstants(ro->m_ConstantBuffer, constants->m_RenderConstants.Begin(), constants->m_RenderConstants.Size());
}


}
