#include "comp_private.h"
#include "resources/res_textureset.h"
#include <dlib/log.h>

using namespace Vectormath::Aos;
namespace dmGameSystem
{
    static const uint64_t g_TextureResourceTypeHash = dmHashString64("texturec");
    static const uint64_t g_TextureSetResourceTypeHash = dmHashString64("texturesetc");
    static const uint64_t g_MaterialResourceTypeHash = dmHashString64("materialc");

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
            dmRender::Constant& c = constants->m_Constants[i];
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
            dmRender::Constant& c = constants->m_Constants[i];
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
            constants->m_Constants[count] = c;
            constants->m_PrevConstants[count] = c.m_Value;
            v = &(constants->m_Constants[count].m_Value);
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
            if (constants->m_Constants[i].m_NameHash == name_hash)
            {
                constants->m_Constants[i] = constants->m_Constants[size - 1];
                constants->m_PrevConstants[i] = constants->m_PrevConstants[size - 1];
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
            dmRender::Constant& c = constants->m_Constants[i];
            dmHashUpdateBuffer32(state, &c.m_NameHash, sizeof(uint64_t));
            dmHashUpdateBuffer32(state, &c.m_Value, sizeof(Vector4));
            constants->m_PrevConstants[i] = c.m_Value;
        }
    }


    static inline dmGameObject::PropertyResult GetResourceDescriptorRef(dmResource::HFactory factory, dmhash_t resource_hash, dmResource::SResourceDescriptor** rd)
    {
        if(resource_hash == 0)
        {
            *rd = 0;
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        *rd = dmResource::GetDescriptorRef(factory, (dmhash_t) resource_hash);
        return *rd == 0x0 ? dmGameObject::PROPERTY_RESULT_RESOURCE_NOT_FOUND : dmGameObject::PROPERTY_RESULT_OK;
    }

    static inline dmGameObject::PropertyResult GetResourceDescriptorRef(dmResource::HFactory factory, const void* resource_ptr, dmResource::SResourceDescriptor** rd)
    {
        if(resource_ptr == 0)
        {
            *rd = 0;
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        *rd = dmResource::GetDescriptorRef(factory, resource_ptr);
        return *rd == 0x0 ? dmGameObject::PROPERTY_RESULT_RESOURCE_NOT_FOUND : dmGameObject::PROPERTY_RESULT_OK;
    }

    static inline dmGameObject::PropertyResult DoSetRenderTextureSet(CompRenderTextureSet* textureset, dmResource::SResourceDescriptor* rd)
    {
        if(rd == 0x0)
        {
            if(textureset->m_TextureSet)
            {
                dmResource::Release(textureset->m_Factory, textureset->m_TextureSet);
            }
            textureset->m_TextureSet = 0x0;
            textureset->m_TextureSetResourceHash = 0;
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        if (dmResource::GetDescriptorExtension(rd) != g_TextureSetResourceTypeHash)
        {
            dmResource::Release(textureset->m_Factory, rd->m_Resource);
            return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
        }
        if(textureset->m_TextureSet)
        {
            dmResource::Release(textureset->m_Factory, textureset->m_TextureSet);
        }
        textureset->m_TextureSet = (TextureSetResource*) rd->m_Resource;
        textureset->m_TextureSetResourceHash = rd->m_NameHash;
        return dmGameObject::PROPERTY_RESULT_OK;
    }

    dmGameObject::PropertyResult SetRenderTextureSet(CompRenderTextureSet* textureset, dmhash_t resource_hash)
    {
        dmResource::SResourceDescriptor *rd;
        dmGameObject::PropertyResult r = GetResourceDescriptorRef(textureset->m_Factory, (dmhash_t) resource_hash, &rd);
        return r == dmGameObject::PROPERTY_RESULT_OK ? DoSetRenderTextureSet(textureset, rd) : r;
    }

    dmGameObject::PropertyResult SetRenderTextureSet(CompRenderTextureSet* textureset, const void* resource)
    {
        dmResource::SResourceDescriptor *rd;
        dmGameObject::PropertyResult r = GetResourceDescriptorRef(textureset->m_Factory, resource, &rd);
        return r == dmGameObject::PROPERTY_RESULT_OK ? DoSetRenderTextureSet(textureset, rd) : r;
    }

    void ClearRenderTextureSet(CompRenderTextureSet* textureset)
    {
        SetRenderTextureSet(textureset, (dmhash_t) 0);
    }

    dmhash_t GetRenderTexture(const CompRenderTextures* textures, uint32_t unit_index)
    {
        assert(unit_index < dmRender::RenderObject::MAX_TEXTURE_COUNT);
        return textures->m_TextureResourceHashes[unit_index];
    }

    static inline dmGameObject::PropertyResult DoSetRenderTexture(CompRenderTextures* textures, uint32_t unit_index, dmResource::SResourceDescriptor* rd)
    {
        assert(unit_index < dmRender::RenderObject::MAX_TEXTURE_COUNT);
        textures->m_InvalidHash = true;
        if(rd == 0x0)
        {
            if(textures->m_Textures[unit_index])
            {
                dmResource::Release(textures->m_Factory, textures->m_Textures[unit_index]);
            }
            textures->m_Textures[unit_index] = 0x0;
            textures->m_TextureResourceHashes[unit_index] = 0;
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        dmGraphics::HTexture texture;
        if (dmResource::GetDescriptorExtension(rd) == g_TextureResourceTypeHash)
        {
            texture = (dmGraphics::HTexture) rd->m_Resource;
        }
        else
        {
            if (dmResource::GetDescriptorExtension(rd) != g_TextureSetResourceTypeHash)
            {
                dmResource::Release(textures->m_Factory, rd->m_Resource);
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
            else
            {
                TextureSetResource* texture_set = (TextureSetResource*) rd->m_Resource;
                texture = texture_set->m_Texture;
            }
        }
        if(textures->m_Textures[unit_index])
        {
            dmResource::Release(textures->m_Factory, textures->m_Textures[unit_index]);
        }
        textures->m_Textures[unit_index] = (dmGraphics::HTexture) rd->m_Resource;
        textures->m_TextureResourceHashes[unit_index] = rd->m_NameHash;
        return dmGameObject::PROPERTY_RESULT_OK;
    }

    dmGameObject::PropertyResult SetRenderTexture(CompRenderTextures* textures, uint32_t unit_index, dmhash_t resource_hash)
    {
        dmResource::SResourceDescriptor *rd;
        dmGameObject::PropertyResult r = GetResourceDescriptorRef(textures->m_Factory, resource_hash, &rd);
        return r == dmGameObject::PROPERTY_RESULT_OK ? DoSetRenderTexture(textures, unit_index, rd) : r;
    }

    dmGameObject::PropertyResult SetRenderTexture(CompRenderTextures* textures, uint32_t unit_index, const void* resource)
    {
        dmResource::SResourceDescriptor *rd;
        dmGameObject::PropertyResult r = GetResourceDescriptorRef(textures->m_Factory, resource, &rd);
        return r == dmGameObject::PROPERTY_RESULT_OK ? DoSetRenderTexture(textures, unit_index, rd) : r;
    }

    void ClearRenderTextures(CompRenderTextures* textures)
    {
        for(uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            SetRenderTexture(textures, i, (dmhash_t) 0);
        }
    }

    void ApplyRenderTextures(const CompRenderTextures* textures, dmRender::RenderObject* ro)
    {
        for(uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            ro->m_Textures[i] = textures->m_Textures[i];
        }
    }

    void HashRenderTextures(CompRenderTextures* textures, HashState32* state)
    {
        dmHashUpdateBuffer32(state, &textures->m_TextureResourceHashes, sizeof(textures->m_TextureResourceHashes));
        textures->m_InvalidHash = false;
    }

    static inline dmGameObject::PropertyResult DoSetRenderMaterial(CompRenderMaterial* material, dmResource::SResourceDescriptor* rd)
    {
        material->m_InvalidHash = true;
        if(rd == 0x0)
        {
            if(material->m_Material)
            {
                dmResource::Release(material->m_Factory, material->m_Material);
            }
            material->m_Material = 0x0;
            material->m_MaterialResourceHash = 0;
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        if (dmResource::GetDescriptorExtension(rd) != g_MaterialResourceTypeHash)
        {
            dmResource::Release(material->m_Factory, rd->m_Resource);
            return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
        }
        if(material->m_Material)
        {
            dmResource::Release(material->m_Factory, material->m_Material);
        }
        material->m_Material = (dmRender::HMaterial) rd->m_Resource;
        material->m_MaterialResourceHash = rd->m_NameHash;
        return dmGameObject::PROPERTY_RESULT_OK;
    }

    dmGameObject::PropertyResult SetRenderMaterial(CompRenderMaterial* material, dmhash_t resource_hash)
    {
        dmResource::SResourceDescriptor *rd;
        dmGameObject::PropertyResult r = GetResourceDescriptorRef(material->m_Factory, (dmhash_t) resource_hash, &rd);
        return r == dmGameObject::PROPERTY_RESULT_OK ? DoSetRenderMaterial(material, rd) : r;
    }

    dmGameObject::PropertyResult SetRenderMaterial(CompRenderMaterial* material, const void* resource)
    {
        dmResource::SResourceDescriptor *rd;
        dmGameObject::PropertyResult r = GetResourceDescriptorRef(material->m_Factory, resource, &rd);
        return r == dmGameObject::PROPERTY_RESULT_OK ? DoSetRenderMaterial(material, rd) : r;
    }

    void ClearRenderMaterial(CompRenderMaterial* material)
    {
        SetRenderMaterial(material, (dmhash_t) 0);
    }

    void HashRenderMaterial(CompRenderMaterial* material, HashState32* state)
    {
        dmHashUpdateBuffer32(state, &material->m_Material, sizeof(material->m_Material));
        material->m_InvalidHash = false;
    }

}
