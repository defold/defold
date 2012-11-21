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

    Properties::Properties()
    {
        memset(this, 0, sizeof(*this));
    }

    void DeletePropertyDefs(const dmArray<PropertyDef>& property_defs)
    {
        uint32_t count = property_defs.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            free((void*)property_defs[i].m_Name);
        }
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
        properties->m_BufferSize = buffer_size;
        if (buffer_size > 0u)
        {
            properties->m_Buffer = new uint8_t[buffer_size];
            memcpy(properties->m_Buffer, buffer, buffer_size);
        }
        else
        {
            properties->m_Buffer = 0x0;
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

    bool GetProperty(const Properties* properties, dmhash_t id, dmGameObjectDDF::PropertyType expected_type, uint8_t** out_cursor)
    {
        const Properties* props = properties;
        while (props != 0x0)
        {
            uint8_t* cursor = props->m_Buffer;
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
            }
            props = props->m_Next;
        }
        LogNotFound(id);
        return false;
    }

    bool GetProperty(const Properties* properties, dmhash_t id, double& out_value)
    {
        uint8_t* cursor;
        if (!GetProperty(properties, id, dmGameObjectDDF::PROPERTY_TYPE_NUMBER, &cursor))
            return false;
        out_value = *(double*)cursor;
        return true;
    }

    bool GetProperty(const Properties* properties, dmhash_t id, dmhash_t& out_value)
    {
        uint8_t* cursor;
        if (!GetProperty(properties, id, dmGameObjectDDF::PROPERTY_TYPE_HASH, &cursor))
            return false;
        out_value = *(dmhash_t*)cursor;
        return true;
    }

    bool GetProperty(const Properties* properties, dmhash_t id, dmMessage::URL& out_value)
    {
        uint8_t* cursor;
        if (!GetProperty(properties, id, dmGameObjectDDF::PROPERTY_TYPE_URL, &cursor))
            return false;
        out_value = *(dmMessage::URL*)cursor;
        return true;
    }

    bool GetProperty(const Properties* properties, dmhash_t id, Vectormath::Aos::Vector3& out_value)
    {
        uint8_t* cursor;
        if (!GetProperty(properties, id, dmGameObjectDDF::PROPERTY_TYPE_VECTOR3, &cursor))
            return false;
        out_value = *(Vectormath::Aos::Vector3*)cursor;
        return true;
    }

    bool GetProperty(const Properties* properties, dmhash_t id, Vectormath::Aos::Vector4& out_value)
    {
        uint8_t* cursor;
        if (!GetProperty(properties, id, dmGameObjectDDF::PROPERTY_TYPE_VECTOR4, &cursor))
            return false;
        out_value = *(Vectormath::Aos::Vector4*)cursor;
        return true;
    }

    bool GetProperty(const Properties* properties, dmhash_t id, Vectormath::Aos::Quat& out_value)
    {
        uint8_t* cursor;
        if (!GetProperty(properties, id, dmGameObjectDDF::PROPERTY_TYPE_QUAT, &cursor))
            return false;
        out_value = *(Vectormath::Aos::Quat*)cursor;
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

    static void SetProperty(const PropertyDef& property, uint8_t** out_cursor)
    {
        uint8_t* cursor = *out_cursor;
        *(dmhash_t*)cursor = property.m_Id;
        cursor += sizeof(dmhash_t);
        *(uint32_t*)cursor = (uint32_t)property.m_Type;
        cursor += 4;
        switch (property.m_Type)
        {
        case dmGameObjectDDF::PROPERTY_TYPE_NUMBER:
            *(double*)cursor = property.m_Number;
            break;
        case dmGameObjectDDF::PROPERTY_TYPE_HASH:
            *(dmhash_t*)cursor = property.m_Hash;
            break;
        case dmGameObjectDDF::PROPERTY_TYPE_URL:
            *(dmMessage::URL*)cursor = property.m_URL;
            break;
        case dmGameObjectDDF::PROPERTY_TYPE_VECTOR3:
            *(Vectormath::Aos::Vector3*)cursor = Vectormath::Aos::Vector3(property.m_V4[0], property.m_V4[1], property.m_V4[2]);
            break;
        case dmGameObjectDDF::PROPERTY_TYPE_VECTOR4:
            *(Vectormath::Aos::Vector4*)cursor = Vectormath::Aos::Vector4(property.m_V4[0], property.m_V4[1], property.m_V4[2], property.m_V4[3]);
            break;
        case dmGameObjectDDF::PROPERTY_TYPE_QUAT:
            *(Vectormath::Aos::Quat*)cursor = Vectormath::Aos::Quat(property.m_V4[0], property.m_V4[1], property.m_V4[2], property.m_V4[3]);
            break;
        default:
            break;
        }
        cursor += GetValueSize(property.m_Type);
        *out_cursor = cursor;
    }

    static bool StringToURL(const char* s, dmMessage::URL* out_url)
    {
        const char* socket;
        uint32_t socket_size;
        const char* path;
        uint32_t path_size;
        const char* fragment;
        uint32_t fragment_size;
        dmMessage::Result result = dmMessage::ParseURL(s, &socket, &socket_size, &path, &path_size, &fragment, &fragment_size);
        if (result != dmMessage::RESULT_OK)
        {
            switch (result)
            {
            case dmMessage::RESULT_MALFORMED_URL:
                dmLogError("Error when parsing '%s', must be of the format 'socket:path#fragment'.", s);
                return false;
            case dmMessage::RESULT_INVALID_SOCKET_NAME:
                dmLogError("The socket name in '%s' is invalid.", s);
                return false;
            case dmMessage::RESULT_SOCKET_NOT_FOUND:
                dmLogError("The socket in '%s' could not be found.", s);
                return false;
            default:
                dmLogError("Error when parsing '%s': %d.", s, result);
                return false;
            }
        }
        if (socket_size > 0)
        {
            dmLogError("'%s' contains a socket which is not allowed when specifying properties", s);
            return false;
        }
        if (path_size > 0)
        {
            if (*path != '/')
            {
                dmLogError("'%s' contains a relative path, only global paths are allowed when specifying properties", s);
                return false;
            }
            out_url->m_Path = dmHashBuffer64(path, path_size);
        }
        if (fragment_size > 0)
        {
            out_url->m_Fragment = dmHashBuffer64(fragment, fragment_size);
        }
        return true;
    }

    uint32_t SerializeProperties(const dmGameObjectDDF::PropertyDesc* properties, uint32_t count, uint8_t* out_buffer, uint32_t out_buffer_size)
    {
        uint32_t size = 4 + count * (sizeof(dmhash_t) + 4);
        for (uint32_t i = 0; i < count; ++i)
        {
            size += GetValueSize(properties[i].m_Type);
        }
        if (out_buffer_size < size)
            return size;
        uint8_t* cursor = out_buffer;
        *(uint32_t*)cursor = count;
        cursor += 4;
        for (uint32_t i = 0; i < count; ++i)
        {
            const dmGameObjectDDF::PropertyDesc& p = properties[i];
            PropertyDef prop;
            prop.m_Id = dmHashString64(p.m_Id);
            switch (p.m_Type)
            {
            case dmGameObjectDDF::PROPERTY_TYPE_NUMBER:
                prop.m_Number = atof(p.m_Value);
                break;
            case dmGameObjectDDF::PROPERTY_TYPE_HASH:
                prop.m_Hash = dmHashString64(p.m_Value);
                break;
            case dmGameObjectDDF::PROPERTY_TYPE_URL:
                {
                    dmMessage::ResetURL(prop.m_URL);
                    bool result = StringToURL(p.m_Value, &prop.m_URL);
                    if (!result)
                        return 0;
                }
                break;
            case dmGameObjectDDF::PROPERTY_TYPE_VECTOR3:
            case dmGameObjectDDF::PROPERTY_TYPE_VECTOR4:
            case dmGameObjectDDF::PROPERTY_TYPE_QUAT:
                dmLogError("The property type %s is not yet supported in collections and game objects.", TYPE_NAMES[p.m_Type]);
                return 0;
            default:
                dmLogError("Invalid property type (%d) for '%s'.", p.m_Type, p.m_Id);
                return 0;
            }
            prop.m_Type = p.m_Type;
            SetProperty(prop, &cursor);
        }
        return size;
    }

    uint32_t SerializeProperties(const dmArray<PropertyDef>& properties, uint8_t* out_buffer, uint32_t out_buffer_size)
    {
        uint32_t count = properties.Size();
        uint32_t size = 4 + count * (sizeof(dmhash_t) + 4);
        for (uint32_t i = 0; i < count; ++i)
        {
            size += GetValueSize(properties[i].m_Type);
        }
        if (out_buffer_size < size)
            return size;
        uint8_t* cursor = out_buffer;
        *(uint32_t*)cursor = count;
        cursor += 4;
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
        if (buffer_size <= 4 || !lua_istable(L, index))
            return 0;

        uint8_t* cursor = buffer;
        // skip count, write later
        cursor += 4;
        uint32_t count = 0;
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
            else if (dmScript::IsVector3(L, -1))
            {
                Vectormath::Aos::Vector3 v = *dmScript::CheckVector3(L, -1);
                p.m_Type = dmGameObjectDDF::PROPERTY_TYPE_VECTOR3;
                p.m_V4[0] = v[0];
                p.m_V4[1] = v[1];
                p.m_V4[2] = v[2];
            }
            else if (dmScript::IsVector4(L, -1))
            {
                Vectormath::Aos::Vector4 v = *dmScript::CheckVector4(L, -1);
                p.m_Type = dmGameObjectDDF::PROPERTY_TYPE_VECTOR4;
                p.m_V4[0] = v[0];
                p.m_V4[1] = v[1];
                p.m_V4[2] = v[2];
                p.m_V4[3] = v[3];
            }
            else if (dmScript::IsQuat(L, -1))
            {
                Vectormath::Aos::Quat v = *dmScript::CheckQuat(L, -1);
                p.m_Type = dmGameObjectDDF::PROPERTY_TYPE_QUAT;
                p.m_V4[0] = v[0];
                p.m_V4[1] = v[1];
                p.m_V4[2] = v[2];
                p.m_V4[3] = v[3];
            }
            else
            {
                valid_type = false;
            }
            if (valid_type)
            {
                uint32_t property_size = GetValueSize(p.m_Type);
                uint32_t needed_size = (cursor - buffer) + sizeof(dmhash_t) + 4 + property_size;
                if (needed_size > buffer_size)
                {
                    return needed_size;
                }
                SetProperty(p, &cursor);
                ++count;
            }
            lua_pop(L, 1);
        }
        *(uint32_t*)buffer = count;
        return cursor - buffer;
    }

    void PropertiesToLuaTable(HInstance instance, const dmArray<PropertyDef>& property_defs, const Properties* properties, lua_State* L, int index)
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
                        if (v.m_Socket == 0 && (v.m_Path != 0 || v.m_Fragment != 0))
                        {
                            v.m_Socket = GetMessageSocket(instance->m_Collection);
                        }
                        if (v.m_Path == 0 && v.m_Fragment != 0)
                        {
                            v.m_Path = GetIdentifier(instance);
                        }
                        dmScript::PushURL(L, v);
                        found_value = true;
                    }
                }
                break;
            case dmGameObjectDDF::PROPERTY_TYPE_VECTOR3:
                {
                    Vectormath::Aos::Vector3 v;
                    if (GetProperty(properties, def.m_Id, v))
                    {
                        dmScript::PushVector3(L, v);
                        found_value = true;
                    }
                }
                break;
            case dmGameObjectDDF::PROPERTY_TYPE_VECTOR4:
                {
                    Vectormath::Aos::Vector4 v;
                    if (GetProperty(properties, def.m_Id, v))
                    {
                        dmScript::PushVector4(L, v);
                        found_value = true;
                    }
                }
                break;
            case dmGameObjectDDF::PROPERTY_TYPE_QUAT:
                {
                    Vectormath::Aos::Quat v;
                    if (GetProperty(properties, def.m_Id, v))
                    {
                        dmScript::PushQuat(L, v);
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
