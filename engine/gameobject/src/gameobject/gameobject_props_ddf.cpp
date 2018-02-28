#include "gameobject_props_ddf.h"

#include <stdlib.h>
#include <string.h>

#include <dlib/log.h>

namespace dmGameObject
{
    bool CreatePropertySetUserData(const dmPropertiesDDF::PropertyDeclarations* prop_descs, uintptr_t* user_data, uint32_t* userdata_size)
    {
        uint32_t save_size = 0;
        dmDDF::Result result = dmDDF::SaveMessageSize(prop_descs, dmPropertiesDDF::PropertyDeclarations::m_DDFDescriptor, &save_size);
        *user_data = 0;
        if (result == dmDDF::RESULT_OK && save_size > 0)
        {
            dmArray<unsigned char> buffer_array;
            buffer_array.SetCapacity(save_size);
            buffer_array.SetSize(save_size);
            result = dmDDF::SaveMessageToArray(prop_descs, dmPropertiesDDF::PropertyDeclarations::m_DDFDescriptor, buffer_array);
            if (result != dmDDF::RESULT_OK)
                return false;
            dmPropertiesDDF::PropertyDeclarations* copy = 0;
            result = dmDDF::LoadMessage(buffer_array.Begin(), save_size, &copy);
            if (result != dmDDF::RESULT_OK)
                return false;
            *user_data = (uintptr_t)copy;
        }
        if(userdata_size)
        {
            *userdata_size = save_size;
        }
        return true;
    }

    void DestroyPropertySetUserData(uintptr_t user_data)
    {
        if (user_data != 0)
        {
            dmDDF::FreeMessage((void*)user_data);
        }
    }

    static bool ResolveURL(Properties* properties, const char* url, dmMessage::URL* out_url)
    {
        dmMessage::URL default_url;
        properties->m_GetURLCallback((lua_State*)properties->m_ResolvePathUserData, &default_url);
        dmMessage::Result result = dmScript::ResolveURL((lua_State*)properties->m_ResolvePathUserData, url, out_url, &default_url);
        if (result != dmMessage::RESULT_OK)
        {
            return false;
        }
        return true;
    }

    static bool GetPropertyEntryIndex(dmhash_t id, dmPropertiesDDF::PropertyDeclarationEntry* entries, uint32_t entry_count, uint32_t* index)
    {
        for (uint32_t i = 0; i < entry_count; ++i)
        {
            if (entries[i].m_Id == id)
            {
                *index = entries[i].m_Index;
                return true;
            }
        }
        return false;
    }

    PropertyResult GetPropertyCallbackDDF(const HProperties properties, uintptr_t user_data, dmhash_t id, PropertyVar& out_var)
    {
        if (user_data != 0)
        {
            dmPropertiesDDF::PropertyDeclarations* copy = (dmPropertiesDDF::PropertyDeclarations*)user_data;
            uint32_t index = 0;
            if (GetPropertyEntryIndex(id, copy->m_NumberEntries.m_Data, copy->m_NumberEntries.m_Count, &index))
            {
                out_var.m_Number = copy->m_FloatValues[index];
                out_var.m_Type = PROPERTY_TYPE_NUMBER;
            }
            else if (GetPropertyEntryIndex(id, copy->m_HashEntries.m_Data, copy->m_HashEntries.m_Count, &index))
            {
                out_var.m_Hash = copy->m_HashValues[index];
                out_var.m_Type = PROPERTY_TYPE_HASH;
            }
            else if (GetPropertyEntryIndex(id, copy->m_UrlEntries.m_Data, copy->m_UrlEntries.m_Count, &index))
            {
                if (!ResolveURL(properties, copy->m_StringValues[index], (dmMessage::URL*) out_var.m_URL))
                    return PROPERTY_RESULT_INVALID_FORMAT;
                out_var.m_Type = PROPERTY_TYPE_URL;
            }
            else if (GetPropertyEntryIndex(id, copy->m_Vector3Entries.m_Data, copy->m_Vector3Entries.m_Count, &index))
            {
                out_var.m_V4[0] = copy->m_FloatValues[index+0];
                out_var.m_V4[1] = copy->m_FloatValues[index+1];
                out_var.m_V4[2] = copy->m_FloatValues[index+2];
                out_var.m_Type = PROPERTY_TYPE_VECTOR3;
            }
            else if (GetPropertyEntryIndex(id, copy->m_Vector4Entries.m_Data, copy->m_Vector4Entries.m_Count, &index))
            {
                out_var.m_V4[0] = copy->m_FloatValues[index+0];
                out_var.m_V4[1] = copy->m_FloatValues[index+1];
                out_var.m_V4[2] = copy->m_FloatValues[index+2];
                out_var.m_V4[3] = copy->m_FloatValues[index+3];
                out_var.m_Type = PROPERTY_TYPE_VECTOR4;
            }
            else if (GetPropertyEntryIndex(id, copy->m_QuatEntries.m_Data, copy->m_QuatEntries.m_Count, &index))
            {
                out_var.m_V4[0] = copy->m_FloatValues[index+0];
                out_var.m_V4[1] = copy->m_FloatValues[index+1];
                out_var.m_V4[2] = copy->m_FloatValues[index+2];
                out_var.m_V4[3] = copy->m_FloatValues[index+3];
                out_var.m_Type = PROPERTY_TYPE_QUAT;
            }
            else if (GetPropertyEntryIndex(id, copy->m_BoolEntries.m_Data, copy->m_BoolEntries.m_Count, &index))
            {
                out_var.m_Bool = copy->m_FloatValues[index] != 0;
                out_var.m_Type = PROPERTY_TYPE_BOOLEAN;
            }
            else
            {
                return PROPERTY_RESULT_NOT_FOUND;
            }
            return PROPERTY_RESULT_OK;
        }
        return PROPERTY_RESULT_NOT_FOUND;
    }
}
