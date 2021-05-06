// Copyright 2020 The Defold Foundation
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

using namespace Vectormath::Aos;
namespace dmGameSystem
{


CompRenderConstants::CompRenderConstants()
{
    memset(this, 0x0, sizeof(*this));
}

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

HComponentRenderConstants CreateRenderConstants()
{
    return new CompRenderConstants;
}

void DestroyRenderConstants(HComponentRenderConstants constants)
{
    delete constants;
}

uint32_t GetRenderConstantCount(HComponentRenderConstants constants)
{
    return constants->m_RenderConstants.Size();
}

void GetRenderConstant(HComponentRenderConstants constants, uint32_t index, dmRender::Constant* constant)
{
    *constant = constants->m_RenderConstants[index];
}

bool GetRenderConstant(HComponentRenderConstants constants, dmhash_t name_hash, dmRender::Constant** out_constant)
{
    uint32_t count = constants->m_RenderConstants.Size();
    for (uint32_t i = 0; i < count; ++i)
    {
        dmRender::Constant& c = constants->m_RenderConstants[i];
        if (c.m_NameHash == name_hash)
        {
            *out_constant = &c;
            return true;
        }
    }
    return false;
}

void SetRenderConstant(HComponentRenderConstants constants, dmRender::HMaterial material, dmhash_t name_hash, uint32_t* element_index, const dmGameObject::PropertyVar& var)
{
    Vector4* v = 0x0;
    uint32_t count = constants->m_RenderConstants.Size();
    for (uint32_t i = 0; i < count; ++i)
    {
        dmRender::Constant& c = constants->m_RenderConstants[i];
        if (c.m_NameHash == name_hash)
        {
            v = &c.m_Value;
            break;
        }
    }

    if (v == 0x0)
    {
        if (count == MAX_COMP_RENDER_CONSTANTS)
        {
            dmLogWarning("Out of component constants (%d)", MAX_COMP_RENDER_CONSTANTS);
            return;
        }

        if (constants->m_RenderConstants.Full())
        {
            constants->m_RenderConstants.OffsetCapacity(1);
            constants->m_PrevRenderConstants.OffsetCapacity(1);
        }
        constants->m_RenderConstants.SetSize(count+1);
        constants->m_PrevRenderConstants.SetSize(count+1);

        dmRender::Constant c;
        dmRender::GetMaterialProgramConstant(material, name_hash, c);
        constants->m_RenderConstants[count] = c;
        constants->m_PrevRenderConstants[count] = c.m_Value;
        v = &(constants->m_RenderConstants[count].m_Value);
    }
    if (element_index == 0x0)
        *v = Vector4(var.m_V4[0], var.m_V4[1], var.m_V4[2], var.m_V4[3]);
    else
        v->setElem(*element_index, (float)var.m_Number);
}

int ClearRenderConstant(HComponentRenderConstants constants, dmhash_t name_hash)
{
    uint32_t size = constants->m_RenderConstants.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        if (constants->m_RenderConstants[i].m_NameHash == name_hash)
        {
            constants->m_RenderConstants.EraseSwap(i);
            constants->m_PrevRenderConstants.EraseSwap(i);
            return 1;
        }
    }
    return 0;
}

void ReHashRenderConstants(HComponentRenderConstants constants, HashState32* state)
{
    // Padding in the SetConstant-struct forces us to hash the individual fields
    uint32_t size = constants->m_RenderConstants.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        dmRender::Constant& c = constants->m_RenderConstants[i];
        dmHashUpdateBuffer32(state, &c.m_NameHash, sizeof(c.m_NameHash));
        dmHashUpdateBuffer32(state, &c.m_Value, sizeof(c.m_Value));
        constants->m_PrevRenderConstants[i] = c.m_Value;
    }
}

int AreRenderConstantsUpdated(HComponentRenderConstants constants)
{
    uint32_t size = constants->m_RenderConstants.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        // TODO: Do a faster check for equality!
        if (lengthSqr(constants->m_RenderConstants[i].m_Value - constants->m_PrevRenderConstants[i]) > 0)
        {
            return 1;
        }
    }
    return 0;
}

void EnableRenderObjectConstants(dmRender::RenderObject* ro, HComponentRenderConstants _constants)
{
    const dmArray<dmRender::Constant>& constants = _constants->m_RenderConstants;
    uint32_t size = constants.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        const dmRender::Constant& c = constants[i];
        dmRender::EnableRenderObjectConstant(ro, c.m_NameHash, c.m_Value);
    }
}


}
