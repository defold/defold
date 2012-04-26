#include "gameobject_props.h"

#include <string.h>
#include <stdlib.h>

#include <dlib/log.h>
#include <script/script.h>

namespace dmGameObject
{
    static const char* TYPE_NAMES[dmGameObjectDDF::PROPERTY_TYPE_COUNT] =
    {
            "number",
            "hash",
            "URL"
    };

    Properties::Properties()
    {
        memset(this, 0, sizeof(*this));
    }

    HProperties NewProperties()
    {
        Properties* properties = new Properties();
        return properties;
    }

    void DeleteProperties(HProperties properties)
    {
        if (properties != 0x0)
        {
            if (properties->m_Buffer != 0x0)
                delete [] properties->m_Buffer;
            delete properties;
        }
    }

    void SetProperties(HProperties properties, uint8_t* buffer, uint32_t buffer_size)
    {
        if (properties->m_Buffer != 0x0)
        {
            delete [] properties->m_Buffer;
        }
        properties->m_Buffer = new uint8_t[buffer_size];
        properties->m_BufferSize = buffer_size;
        memcpy(properties->m_Buffer, buffer, buffer_size);
    }

    bool GetProperty(const Properties* properties, dmhash_t id, dmGameObjectDDF::PropertyType* out_type, uint8_t** out_cursor)
    {
        bool found = false;
        uint8_t* cursor = properties->m_Buffer;
        if (cursor != 0x0)
        {
            uint8_t count = *(cursor++);
            for (uint8_t i = 0; i < count; ++i)
            {
                dmhash_t tmp_id = *(dmhash_t*)cursor;
                cursor += sizeof(dmhash_t);
                dmGameObjectDDF::PropertyType type = (dmGameObjectDDF::PropertyType)*(uint8_t*)cursor;
                ++cursor;
                if (id == tmp_id)
                {
                    *out_type = type;
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
                    default:
                        break;
                    }
                }
            }
        }
        if (!found && properties->m_Next != 0x0)
        {
            return GetProperty(properties->m_Next, id, out_type, out_cursor);
        }
        else
        {
            return found;
        }
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

    bool GetProperty(const Properties* properties, dmhash_t id, double& out_value)
    {
        dmGameObjectDDF::PropertyType type;
        uint8_t* cursor;
        if (!GetProperty(properties, id, &type, &cursor))
        {
            LogNotFound(id);
            return false;
        }
        else if (type != dmGameObjectDDF::PROPERTY_TYPE_NUMBER)
        {
            LogInvalidType(id, dmGameObjectDDF::PROPERTY_TYPE_NUMBER, type);
            return false;
        }
        else
        {
            out_value = *(double*)cursor;
            return true;
        }
    }

    bool GetProperty(const Properties* properties, dmhash_t id, dmhash_t& out_value)
    {
        dmGameObjectDDF::PropertyType type;
        uint8_t* cursor;
        if (!GetProperty(properties, id, &type, &cursor))
        {
            LogNotFound(id);
            return false;
        }
        else if (type != dmGameObjectDDF::PROPERTY_TYPE_HASH)
        {
            LogInvalidType(id, dmGameObjectDDF::PROPERTY_TYPE_HASH, type);
            return false;
        }
        else
        {
            out_value = *(dmhash_t*)cursor;
            return true;
        }
    }

    bool GetProperty(const Properties* properties, dmhash_t id, dmMessage::URL& out_value)
    {
        dmGameObjectDDF::PropertyType type;
        uint8_t* cursor;
        if (!GetProperty(properties, id, &type, &cursor))
        {
            LogNotFound(id);
            return false;
        }
        else if (type != dmGameObjectDDF::PROPERTY_TYPE_URL)
        {
            LogInvalidType(id, dmGameObjectDDF::PROPERTY_TYPE_URL, type);
            return false;
        }
        else
        {
            out_value = *(dmMessage::URL*)cursor;
            return true;
        }
    }

    static void SetProperty(const PropertyDef& property, uint8_t** out_cursor)
    {
        uint8_t* cursor = *out_cursor;
        *(dmhash_t*)cursor = property.m_Id;
        cursor += sizeof(dmhash_t);
        *cursor = (uint8_t)property.m_Type;
        ++cursor;
        switch (property.m_Type)
        {
        case dmGameObjectDDF::PROPERTY_TYPE_NUMBER:
            *(double*)cursor = property.m_Number;
            cursor += sizeof(double);
            break;
        case dmGameObjectDDF::PROPERTY_TYPE_HASH:
            *(dmhash_t*)cursor = property.m_Hash;
            cursor += sizeof(dmhash_t);
            break;
        case dmGameObjectDDF::PROPERTY_TYPE_URL:
            *(dmMessage::URL*)cursor = property.m_URL;
            cursor += sizeof(dmMessage::URL);
            break;
        default:
            break;
        }
        *out_cursor = cursor;
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
        default:
            return 0;
        }
    }

    uint32_t SerializeProperties(const dmGameObjectDDF::PropertyDesc* properties, uint32_t count, uint8_t* out_buffer, uint32_t out_buffer_size)
    {
        uint32_t size = 1 + count * (sizeof(dmhash_t) + 1);
        for (uint32_t i = 0; i < count; ++i)
        {
            size += GetValueSize(properties[i].m_Type);
        }
        if (out_buffer_size < size)
            return size;
        uint8_t* cursor = out_buffer;
        *(cursor++) = (uint8_t)count;
        for (uint32_t i = 0; i < count; ++i)
        {
            const dmGameObjectDDF::PropertyDesc& p = properties[i];
            PropertyDef prop;
            prop.m_Id = dmHashString64(p.m_Id);
            prop.m_Number = atof(p.m_Value);
            prop.m_Type = p.m_Type;
            SetProperty(prop, &cursor);
        }
        return size;
    }

    uint32_t SerializeProperties(const dmArray<PropertyDef>& properties, uint8_t* out_buffer, uint32_t out_buffer_size)
    {
        uint32_t count = properties.Size();
        uint32_t size = 1 + count * (sizeof(dmhash_t) + 1);
        for (uint32_t i = 0; i < count; ++i)
        {
            size += GetValueSize(properties[i].m_Type);
        }
        if (out_buffer_size < size)
            return size;
        uint8_t* cursor = out_buffer;
        *(cursor++) = (uint8_t)count;
        for (uint32_t i = 0; i < count; ++i)
        {
            SetProperty(properties[i], &cursor);
        }
        return size;
    }

    void AppendProperties(HProperties properties, HProperties next)
    {
        Properties* props = properties;
        while (props->m_Next != 0x0 && props->m_Next != next)
        {
            props = props->m_Next;
        }
        if (props->m_Next != next)
            props->m_Next = next;
    }

    uint32_t LuaTableToProperties(lua_State* L, int index, uint8_t* buffer, uint32_t buffer_size)
    {
        uint8_t* cursor = buffer;
        // skip count, write later
        ++cursor;
        uint8_t count = 0;
        lua_pushnil(L);
        while (lua_next(L, index) != 0)
        {
            const char* id = luaL_checklstring(L, -2, 0x0);
            PropertyDef p;
            p.m_Id = dmHashString64(id);
            bool valid_type = true;
            if (lua_isnumber(L, -1))
            {
                lua_Number v = lua_tonumber(L, -1);
                p.m_Type = dmGameObjectDDF::PROPERTY_TYPE_NUMBER;
                p.m_Number = v;
            }
            else if (dmScript::IsHash(L, -1))
            {
                dmhash_t v = dmScript::CheckHash(L, -1);
                p.m_Type = dmGameObjectDDF::PROPERTY_TYPE_HASH;
                p.m_Hash = v;
            }
            else if (dmScript::IsURL(L, -1))
            {
                dmMessage::URL v = *dmScript::CheckURL(L, -1);
                p.m_Type = dmGameObjectDDF::PROPERTY_TYPE_URL;
                p.m_URL = v;
            }
            else
            {
                valid_type = false;
            }
            if (valid_type)
            {
                // TODO: Check overflow
                SetProperty(p, &cursor);
                ++count;
            }
            lua_pop(L, 1);
        }
        *buffer = count;
        return cursor - buffer;
    }

    void PropertiesToLuaTable(const dmArray<PropertyDef>& property_defs, const Properties* properties, lua_State* L, int index)
    {
        uint32_t count = property_defs.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            const PropertyDef& def = property_defs[i];
            lua_pushstring(L, def.m_Name);
            bool found_value = false;
            switch (def.m_Type)
            {
            case dmGameObjectDDF::PROPERTY_TYPE_NUMBER:
                {
                    double v;
                    if (GetProperty(properties, def.m_Id, v))
                    {
                        lua_pushnumber(L, v);
                        found_value = true;
                    }
                }
                break;
            case dmGameObjectDDF::PROPERTY_TYPE_HASH:
                {
                    dmhash_t v;
                    if (GetProperty(properties, def.m_Id, v))
                    {
                        dmScript::PushHash(L, v);
                        found_value = true;
                    }
                }
                break;
            case dmGameObjectDDF::PROPERTY_TYPE_URL:
                {
                    dmMessage::URL v;
                    if (GetProperty(properties, def.m_Id, v))
                    {
                        dmScript::PushURL(L, v);
                        found_value = true;
                    }
                }
                break;
            default:
                break;
            }
            if (found_value)
            {
                lua_settable(L, index-2);
            }
            else
            {
                lua_pop(L, 1);
            }
        }
    }

    void ClearPropertiesFromLuaTable(const dmArray<PropertyDef>& property_defs, const Properties* properties, lua_State* L, int index)
    {
        uint32_t count = property_defs.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            const PropertyDef& def = property_defs[i];
            lua_pushstring(L, def.m_Name);
            lua_pushnil(L);
            lua_settable(L, index-2);
        }
    }
}
