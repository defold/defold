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

    PropertySet::PropertySet()
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
                PropertySet& set = properties->m_Set[i];
                if (set.m_FreeUserDataCallback != 0)
                {
                    set.m_FreeUserDataCallback(set.m_UserData);
                }
            }
            delete properties;
        }
    }

    void SetPropertySet(HProperties properties, PropertyLayer layer, const PropertySet& set)
    {
        properties->m_Set[layer] = set;
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

    bool GetProperty(const HProperties properties, dmhash_t id, PropertyVar& var)
    {
        for (uint32_t i = 0; i < MAX_PROPERTY_LAYER_COUNT; ++i)
        {
            const PropertySet& set = properties->m_Set[i];
            if (set.m_GetPropertyCallback != 0x0)
            {
                PropertyResult result = set.m_GetPropertyCallback(properties, set.m_UserData, id, var);
                if (result == PROPERTY_RESULT_OK)
                {
                    return true;
                }
            }
        }
        LogNotFound(id);
        return false;
    }
}
