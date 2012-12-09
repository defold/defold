#include "gameobject_props.h"

#include <string.h>
#include <stdlib.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <script/script.h>

#include "gameobject_private.h"

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

    PropertyData::PropertyData()
    {
        memset(this, 0, sizeof(*this));
    }

    Properties::Properties()
    {
        memset(this, 0, sizeof(*this));
    }

    NewPropertiesParams::NewPropertiesParams()
    {
        memset(this, 0, sizeof(*this));
    }

    HProperties NewProperties(const NewPropertiesParams& params)
    {
        Properties* properties = new Properties();
        properties->m_ResolvePathCallback = params.m_ResolvePathCallback;
        properties->m_ResolvePathUserData = params.m_ResolvePathUserData;
        properties->m_GetURLCallback = params.m_GetURLCallback;
        return properties;
    }

    void DeleteProperties(HProperties properties)
    {
        if (properties != 0x0)
        {
            for (uint32_t i = 0; i < MAX_PROPERTY_LAYER_COUNT; ++i)
            {
                PropertyData& data = properties->m_Data[i];
                if (data.m_FreeUserDataCallback != 0)
                {
                    data.m_FreeUserDataCallback(data.m_UserData);
                }
            }
            delete properties;
        }
    }

    void SetPropertyData(HProperties properties, PropertyLayer layer, const PropertyData& data)
    {
        properties->m_Data[layer] = data;
    }

    void LogNotFound(dmhash_t id)
    {
        const char* name = (const char*)dmHashReverse64(id, 0x0);
        if (name != 0x0)
        {
            dmLogError("The property with id '%s' could not be found.", name);
        }
        else
        {
            dmLogError("The property with id %llX could not be found.", id);
        }
    }

    void LogInvalidType(dmhash_t id, dmGameObjectDDF::PropertyType expected, dmGameObjectDDF::PropertyType actual)
    {
        const char* name = (const char*)dmHashReverse64(id, 0x0);
        if (name != 0x0)
        {
            dmLogError("The property with id '%s' should be a %s, not a %s.", name, TYPE_NAMES[expected], TYPE_NAMES[actual]);
        }
        else
        {
            dmLogError("The property with id %llX should be a %s, not a %s.", id, TYPE_NAMES[expected], TYPE_NAMES[actual]);
        }
    }

    static bool GetProperty(const HProperties properties, dmhash_t id, PropertyVar& var)
    {
        for (uint32_t i = 0; i < MAX_PROPERTY_LAYER_COUNT; ++i)
        {
            const PropertyData& data = properties->m_Data[i];
            if (data.m_GetPropertyCallback != 0x0)
            {
                PropertyResult result = data.m_GetPropertyCallback(properties, data.m_UserData, id, var);
                if (result == PROPERTY_RESULT_OK)
                {
                    return true;
                }
            }
        }
/*            uint8_t* cursor = props->m_Buffer;
            if (cursor != 0x0)
            {
                uint32_t count = *cursor;
                cursor += 4;
                for (uint32_t i = 0; i < count; ++i)
                {
                    dmhash_t tmp_id = *(dmhash_t*)cursor;
                    cursor += sizeof(dmhash_t);
                    dmGameObjectDDF::PropertyType type = (dmGameObjectDDF::PropertyType)*(uint32_t*)cursor;
                    cursor += 4;
                    if (id == tmp_id)
                    {
                        if (expected_type != type)
                        {
                            LogInvalidType(id, expected_type, type);
                            return false;
                        }
                        *out_cursor = cursor;
                        return true;
                    }
                    else
                    {
                        switch (type)
                        {
                        case dmGameObjectDDF::PROPERTY_TYPE_NUMBER:
                            cursor += sizeof(double);
                            break;
                        case dmGameObjectDDF::PROPERTY_TYPE_HASH:
                            cursor += sizeof(dmhash_t);
                            break;
                        case dmGameObjectDDF::PROPERTY_TYPE_URL:
                            cursor += sizeof(dmMessage::URL);
                            break;
                        case dmGameObjectDDF::PROPERTY_TYPE_VECTOR3:
                            cursor += sizeof(Vectormath::Aos::Vector3);
                            break;
                        case dmGameObjectDDF::PROPERTY_TYPE_VECTOR4:
                            cursor += sizeof(Vectormath::Aos::Vector4);
                            break;
                        case dmGameObjectDDF::PROPERTY_TYPE_QUAT:
                            cursor += sizeof(Vectormath::Aos::Quat);
                            break;
                        default:
                            break;
                        }
                    }
                }
            }*/
        LogNotFound(id);
        return false;
    }

    bool GetProperty(const HProperties properties, dmhash_t id, double& out_value)
    {
        PropertyVar var;
        if (!GetProperty(properties, id, var))
            return false;
        if (var.m_Type != dmGameObjectDDF::PROPERTY_TYPE_NUMBER)
            return false;
        out_value = var.m_Number;
        return true;
    }

    bool GetProperty(const HProperties properties, dmhash_t id, dmhash_t& out_value)
    {
        PropertyVar var;
        if (!GetProperty(properties, id, var))
            return false;
        if (var.m_Type != dmGameObjectDDF::PROPERTY_TYPE_HASH)
            return false;
        out_value = var.m_Hash;
        return true;
    }

    bool GetProperty(const HProperties properties, dmhash_t id, dmMessage::URL& out_value)
    {
        PropertyVar var;
        if (!GetProperty(properties, id, var))
            return false;
        if (var.m_Type != dmGameObjectDDF::PROPERTY_TYPE_URL)
            return false;
        out_value = var.m_URL;
        return true;
    }

    bool GetProperty(const HProperties properties, dmhash_t id, Vectormath::Aos::Vector3& out_value)
    {
        PropertyVar var;
        if (!GetProperty(properties, id, var))
            return false;
        if (var.m_Type != dmGameObjectDDF::PROPERTY_TYPE_VECTOR3)
            return false;
        out_value = Vector3(var.m_V4[0], var.m_V4[1], var.m_V4[2]);
        return true;
    }

    bool GetProperty(const HProperties properties, dmhash_t id, Vectormath::Aos::Vector4& out_value)
    {
        PropertyVar var;
        if (!GetProperty(properties, id, var))
            return false;
        if (var.m_Type != dmGameObjectDDF::PROPERTY_TYPE_VECTOR4)
            return false;
        out_value = Vector4(var.m_V4[0], var.m_V4[1], var.m_V4[2], var.m_V4[3]);
        return true;
    }

    bool GetProperty(const HProperties properties, dmhash_t id, Vectormath::Aos::Quat& out_value)
    {
        PropertyVar var;
        if (!GetProperty(properties, id, var))
            return false;
        if (var.m_Type != dmGameObjectDDF::PROPERTY_TYPE_QUAT)
            return false;
        out_value = Quat(var.m_V4[0], var.m_V4[1], var.m_V4[2], var.m_V4[3]);
        return true;
    }

    uint32_t GetValueSize(dmGameObjectDDF::PropertyType type)
    {
        switch (type)
        {
        case dmGameObjectDDF::PROPERTY_TYPE_NUMBER:
            return sizeof(double);
        case dmGameObjectDDF::PROPERTY_TYPE_HASH:
            return sizeof(dmhash_t);
        case dmGameObjectDDF::PROPERTY_TYPE_URL:
            return sizeof(dmMessage::URL);
        case dmGameObjectDDF::PROPERTY_TYPE_VECTOR3:
            return sizeof(Vectormath::Aos::Vector3);
        case dmGameObjectDDF::PROPERTY_TYPE_VECTOR4:
            return sizeof(Vectormath::Aos::Vector4);
        case dmGameObjectDDF::PROPERTY_TYPE_QUAT:
            return sizeof(Vectormath::Aos::Quat);
        default:
            return 0;
        }
    }

}
