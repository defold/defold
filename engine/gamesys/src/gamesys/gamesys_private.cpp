#include <stdarg.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include "gamesys_private.h"
#include "components/comp_private.h"

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    const dmhash_t PROP_TEXTURESET = dmHashString64("textureset");
    const dmhash_t PROP_MATERIAL = dmHashString64("material");

    static const dmhash_t PROP_TEXTURE[dmRender::RenderObject::MAX_TEXTURE_COUNT] = {
        dmHashString64("texture0"),
        dmHashString64("texture1"),
        dmHashString64("texture2"),
        dmHashString64("texture3"),
        dmHashString64("texture4"),
        dmHashString64("texture5"),
        dmHashString64("texture6"),
        dmHashString64("texture7")
    };

    void LogMessageError(dmMessage::Message* message, const char* format, ...)
    {
        va_list lst;
        va_start(lst, format);

        char buf[512];

        int n = vsnprintf(buf, sizeof(buf), format, lst);

        if (n < (int) sizeof(buf))
        {
            const char* id_str = dmHashReverseSafe64(message->m_Id);

            const dmMessage::URL* sender = &message->m_Sender;
            const char* socket_name_sender = dmMessage::GetSocketName(sender->m_Socket);
            const char* path_name_sender = dmHashReverseSafe64(sender->m_Path);
            const char* fragment_name_sender = dmHashReverseSafe64(sender->m_Fragment);

            const dmMessage::URL* receiver = &message->m_Receiver;
            const char* socket_name_receiver = dmMessage::GetSocketName(receiver->m_Socket);
            const char* path_name_receiver = dmHashReverseSafe64(receiver->m_Path);
            const char* fragment_name_receiver = dmHashReverseSafe64(receiver->m_Fragment);

            n+= DM_SNPRINTF(buf + n, sizeof(buf) - n, " Message '%s' sent from %s:%s#%s to %s:%s#%s.",
                            id_str,
                            socket_name_sender, path_name_sender, fragment_name_sender,
                            socket_name_receiver, path_name_receiver, fragment_name_receiver);
        }

        if (n >= (int) sizeof(buf) - 1) {
            dmLogError("Buffer underflow when formatting message-error (LogMessageError)");
        }

        dmLogError("%s", buf);

        va_end(lst);

    }

    dmGameObject::PropertyResult GetMaterialConstant(dmRender::HMaterial material, dmhash_t name_hash, dmGameObject::PropertyDesc& out_desc, bool use_value_ptr, CompGetConstantCallback callback, void* callback_user_data)
    {
        dmhash_t constant_id = 0;
        dmhash_t* element_ids = 0x0;
        uint32_t element_index = ~0u;
        bool result = dmRender::GetMaterialProgramConstantInfo(material, name_hash, &constant_id, &element_ids, &element_index);
        if (result)
        {
            Vector4* value = 0x0;
            dmRender::Constant* comp_constant = 0x0;
            if (callback(callback_user_data, constant_id, &comp_constant))
                value = &comp_constant->m_Value;
            if (constant_id == name_hash)
            {
                if (element_ids != 0x0)
                {
                    out_desc.m_ElementIds[0] = element_ids[0];
                    out_desc.m_ElementIds[1] = element_ids[1];
                    out_desc.m_ElementIds[2] = element_ids[2];
                    out_desc.m_ElementIds[3] = element_ids[3];
                }

                if (value != 0x0 && use_value_ptr)
                {
                    out_desc.m_ValuePtr = (float*)value;
                    out_desc.m_Variant = dmGameObject::PropertyVar(*value);
                }
                else
                {
                    dmRender::Constant c;
                    dmRender::GetMaterialProgramConstant(material, constant_id, c);
                    out_desc.m_Variant = dmGameObject::PropertyVar(c.m_Value);
                }
            }
            else
            {
                if (value != 0x0)
                {
                    if (use_value_ptr)
                    {
                        out_desc.m_ValuePtr = ((float*)value) + element_index;
                        out_desc.m_Variant = *out_desc.m_ValuePtr;
                    }
                    else
                    {
                        float val = *(((float*)value) + element_index);
                        out_desc.m_Variant = dmGameObject::PropertyVar(val);
                    }
                }
                else
                {
                    float v;
                    dmRender::GetMaterialProgramConstantElement(material, constant_id, element_index, v);
                    out_desc.m_Variant = dmGameObject::PropertyVar(v);
                }
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    dmGameObject::PropertyResult SetMaterialConstant(dmRender::HMaterial material, dmhash_t name_hash, const dmGameObject::PropertyVar& var, CompSetConstantCallback callback, void* callback_user_data)
    {
        dmhash_t constant_id = 0;
        dmhash_t* element_ids = 0x0;
        uint32_t element_index = ~0u;
        bool result = dmRender::GetMaterialProgramConstantInfo(material, name_hash, &constant_id, &element_ids, &element_index);
        if (result)
        {
            int32_t location = dmRender::GetMaterialConstantLocation(material, constant_id);
            if (location >= 0)
            {
                if (constant_id == name_hash)
                {
                    if (var.m_Type != dmGameObject::PROPERTY_TYPE_VECTOR4 && var.m_Type != dmGameObject::PROPERTY_TYPE_QUAT)
                    {
                        return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
                    }
                    callback(callback_user_data, constant_id, 0x0, var);
                }
                else
                {
                    if (var.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
                    {
                        return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
                    }
                    callback(callback_user_data, constant_id, &element_index, var);
                }
                return dmGameObject::PROPERTY_RESULT_OK;
            }
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    dmGameObject::PropertyResult GetComponentTextureSet(CompRenderTextureSet* textureset, dmGameObject::PropertyDesc& out_desc)
    {
        out_desc.m_Variant = textureset->m_TextureSetResourceHash;
        return dmGameObject::PROPERTY_RESULT_OK;
    }

    dmGameObject::PropertyResult SetComponentTextureSet(CompRenderTextureSet* textureset, const dmGameObject::PropertyVar& var)
    {
        return SetRenderTextureSet(textureset, var.m_Hash);
    }

    dmGameObject::PropertyResult GetComponentTexture(CompRenderTextures* textures, dmhash_t unit_property_hash, dmGameObject::PropertyDesc& out_desc)
    {
        for(uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            if(unit_property_hash == PROP_TEXTURE[i])
            {
                out_desc.m_Variant = GetRenderTexture(textures, i);
                return dmGameObject::PROPERTY_RESULT_OK;
            }
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    dmGameObject::PropertyResult SetComponentTexture(CompRenderTextures* textures, dmhash_t unit_property_hash, const dmGameObject::PropertyVar& var)
    {
        for(uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            if(unit_property_hash == PROP_TEXTURE[i])
            {
                return SetRenderTexture(textures, i, var.m_Hash);
            }
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    dmGameObject::PropertyResult GetComponentMaterial(CompRenderMaterial* material, dmGameObject::PropertyDesc& out_desc)
    {
        out_desc.m_Variant = material->m_MaterialResourceHash;
        return dmGameObject::PROPERTY_RESULT_OK;
    }

    dmGameObject::PropertyResult SetComponentMaterial(CompRenderMaterial* material, const dmGameObject::PropertyVar& var)
    {
        return SetRenderMaterial(material, var.m_Hash);
    }

}
