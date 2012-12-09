#include "gameobject_props_ddf.h"

#include <stdlib.h>
#include <string.h>

#include <dlib/log.h>

namespace dmGameObject
{
    static const char* TYPE_NAMES[dmGameObjectDDF::PROPERTY_TYPE_COUNT] =
    {
            "number",
            "hash",
            "URL",
            "vector3",
            "vector4",
            "quat"
    };

    struct PropertyVarDDF
    {
        dmhash_t m_Id;
        dmGameObjectDDF::PropertyType m_Type;
        union
        {
            double m_Number;
            dmhash_t m_Hash;
            const char* m_URL;
            float m_V4[4];
        };
    };

    bool CreatePropertyDataUserData(const dmGameObjectDDF::PropertyDesc* prop_descs, uint32_t prop_desc_count, uintptr_t* user_data)
    {
        if (prop_desc_count > 0)
        {
            dmArray<PropertyVarDDF> vars;
            vars.SetCapacity(prop_desc_count);
            vars.SetSize(prop_desc_count);
            for (uint32_t i = 0; i < prop_desc_count; ++i)
            {
                const dmGameObjectDDF::PropertyDesc& desc = prop_descs[i];
                PropertyVarDDF& var = vars[i];
                var.m_Id = dmHashString64(desc.m_Id);
                var.m_Type = desc.m_Type;
                switch (desc.m_Type)
                {
                case dmGameObjectDDF::PROPERTY_TYPE_NUMBER:
                    var.m_Number = atof(desc.m_Value);
                    break;
                case dmGameObjectDDF::PROPERTY_TYPE_HASH:
                    var.m_Hash = dmHashString64(desc.m_Value);
                    break;
                case dmGameObjectDDF::PROPERTY_TYPE_URL:
                    var.m_URL = strdup(desc.m_Value);
                    break;
                case dmGameObjectDDF::PROPERTY_TYPE_VECTOR3:
                case dmGameObjectDDF::PROPERTY_TYPE_VECTOR4:
                case dmGameObjectDDF::PROPERTY_TYPE_QUAT:
                    dmLogError("The property type %s is not yet supported in collections and game objects.", TYPE_NAMES[desc.m_Type]);
                    return false;
                default:
                    dmLogError("Unknown property type %d.", desc.m_Type);
                    return false;
                }
            }
            dmArray<PropertyVarDDF>* result = new dmArray<PropertyVarDDF>();
            vars.Swap(*result);
            *user_data = (uintptr_t)result;
        }
        return true;
    }

    void DestroyPropertyDataUserData(uintptr_t user_data)
    {
        if (user_data != 0)
        {
            dmArray<PropertyVarDDF>* var_array = (dmArray<PropertyVarDDF>*)user_data;
            uint32_t count = var_array->Size();
            PropertyVarDDF* vars = var_array->Begin();
            for (uint32_t i = 0; i < count; ++i)
            {
                if (vars[i].m_Type == dmGameObjectDDF::PROPERTY_TYPE_URL)
                    free((void*)vars[i].m_URL);
            }
            delete var_array;
        }
    }

    static bool ResolveURL(Properties* properties, const char* url, dmMessage::URL& out_url)
    {
        dmMessage::URL default_url;
        properties->m_GetURLCallback((lua_State*)properties->m_ResolvePathUserData, &default_url);
        dmMessage::Result result = dmScript::ResolveURL(properties->m_ResolvePathCallback, properties->m_ResolvePathUserData, url, &out_url, &default_url);
        if (result != dmMessage::RESULT_OK)
        {
            return false;
        }
        return true;
    }

    PropertyResult GetPropertyCallbackDDF(const HProperties properties, uintptr_t user_data, dmhash_t id, PropertyVar& out_var)
    {
        if (user_data != 0)
        {
            const dmArray<PropertyVarDDF>& vars = *(dmArray<PropertyVarDDF>*)user_data;
            uint32_t count = vars.Size();
            for (uint32_t i = 0; i < count; ++i)
            {
                const PropertyVarDDF& var = vars[i];
                if (var.m_Id == id)
                {
                    switch (var.m_Type)
                    {
                    case dmGameObjectDDF::PROPERTY_TYPE_NUMBER:
                        out_var.m_Number = var.m_Number;
                        break;
                    case dmGameObjectDDF::PROPERTY_TYPE_HASH:
                        out_var.m_Hash = var.m_Hash;
                        break;
                    case dmGameObjectDDF::PROPERTY_TYPE_URL:
                        if (!ResolveURL(properties, var.m_URL, out_var.m_URL))
                            return PROPERTY_RESULT_INVALID_FORMAT;
                        break;
                    case dmGameObjectDDF::PROPERTY_TYPE_VECTOR3:
                    case dmGameObjectDDF::PROPERTY_TYPE_VECTOR4:
                    case dmGameObjectDDF::PROPERTY_TYPE_QUAT:
                        out_var.m_V4[0] = var.m_V4[0];
                        out_var.m_V4[1] = var.m_V4[1];
                        out_var.m_V4[2] = var.m_V4[2];
                        out_var.m_V4[3] = var.m_V4[3];
                        break;
                    default:
                        return PROPERTY_RESULT_UNKNOWN_TYPE;
                    }
                    out_var.m_Type = var.m_Type;
                    return PROPERTY_RESULT_OK;
                }
            }
        }
        return PROPERTY_RESULT_NOT_FOUND;
    }
}
