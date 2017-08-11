#include "comp_private.h"
#include <dlib/log.h>

using namespace Vectormath::Aos;
namespace dmGameSystem
{

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

bool GetRenderConstant(CompRenderConstants* constants, dmhash_t name_hash, dmRender::Constant** out_constant)
{
    uint32_t count = constants->m_ConstantCount;
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

void SetRenderConstant(CompRenderConstants* constants, dmRender::HMaterial material, dmhash_t name_hash, uint32_t* element_index, const dmGameObject::PropertyVar& var)
{
    Vector4* v = 0x0;
    uint32_t count = constants->m_ConstantCount;
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
        dmRender::Constant c;
        dmRender::GetMaterialProgramConstant(material, name_hash, c);
        constants->m_RenderConstants[count] = c;
        constants->m_PrevRenderConstants[count] = c.m_Value;
        v = &(constants->m_RenderConstants[count].m_Value);
        constants->m_ConstantCount++;
        assert(constants->m_ConstantCount <= MAX_COMP_RENDER_CONSTANTS);
    }
    if (element_index == 0x0)
        *v = Vector4(var.m_V4[0], var.m_V4[1], var.m_V4[2], var.m_V4[3]);
    else
        v->setElem(*element_index, (float)var.m_Number);
}

int ClearRenderConstant(CompRenderConstants* constants, dmhash_t name_hash)
{
    uint32_t size = constants->m_ConstantCount;
    for (uint32_t i = 0; i < size; ++i)
    {
        if (constants->m_RenderConstants[i].m_NameHash == name_hash)
        {
            constants->m_RenderConstants[i] = constants->m_RenderConstants[size - 1];
            constants->m_PrevRenderConstants[i] = constants->m_PrevRenderConstants[size - 1];
            constants->m_ConstantCount--;
            return 1;
        }
    }
    return 0;
}

void ReHashRenderConstants(CompRenderConstants* constants, HashState32* state)
{
    // Padding in the SetConstant-struct forces us to copy the components by hand
    for (uint32_t i = 0; i < constants->m_ConstantCount; ++i)
    {
        dmRender::Constant& c = constants->m_RenderConstants[i];
        dmHashUpdateBuffer32(state, &c.m_NameHash, sizeof(uint64_t));
        dmHashUpdateBuffer32(state, &c.m_Value, sizeof(Vector4));
        constants->m_PrevRenderConstants[i] = c.m_Value;
    }
}


}
