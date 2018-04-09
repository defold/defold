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
            "quat",
            "bool"
    };

    PropertySet::PropertySet()
    {
        memset(this, 0, sizeof(*this));
    }

    PropertyDesc::PropertyDesc()
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

    static void LogNotFound(dmhash_t id)
    {
        dmLogError("The property with id '%s' could not be found.", dmHashReverseSafe64(id));
    }

    PropertyResult GetProperty(const HProperties properties, dmhash_t id, PropertyVar& var)
    {
        for (uint32_t i = 0; i < MAX_PROPERTY_LAYER_COUNT; ++i)
        {
            const PropertySet& set = properties->m_Set[i];
            if (set.m_GetPropertyCallback != 0x0)
            {
                PropertyResult result = set.m_GetPropertyCallback(properties, set.m_UserData, id, var);
                if (result != PROPERTY_RESULT_NOT_FOUND)
                {
                    return result;
                }
            }
        }
        LogNotFound(id);
        return PROPERTY_RESULT_NOT_FOUND;
    }

    dmResource::Result LoadPropertyResources(dmResource::HFactory factory, const char** resource_paths, uint32_t resource_path_count, dmArray<void*>& out_resources)
    {
        assert(out_resources.Size() == 0);
        out_resources.SetCapacity(resource_path_count);
        for(uint32_t i = 0; i < resource_path_count; ++i)
        {
            void* resource;
            dmResource::Result res = dmResource::Get(factory, resource_paths[i], &resource);
            if(res != dmResource::RESULT_OK)
            {
                dmLogError("Could not load property resource '%s' (%d)", resource_paths[i], res);
                UnloadPropertyResources(factory, out_resources);
                return res;
            }
            out_resources.Push(resource);
        }
        return dmResource::RESULT_OK;
    }

    void UnloadPropertyResources(dmResource::HFactory factory, dmArray<void*>& resources)
    {
        for(uint32_t i = 0; i < resources.Size(); ++i)
        {
            dmResource::Release(factory, resources[i]);
        }
        resources.SetSize(0);
        resources.SetCapacity(0);
    }
}
