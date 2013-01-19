#include "gameobject_props_lua.h"

#include <stdlib.h>
#include <string.h>

#include <dlib/log.h>

namespace dmGameObject
{
    struct PropertyLua
    {
        dmhash_t m_Id;
        PropertyVar m_Var;
    };

    bool CreatePropertySetUserDataLua(lua_State* L, uint8_t* buffer, uint32_t buffer_size, uintptr_t* user_data)
    {
        int top = lua_gettop(L);
        (void)top;
        if (buffer_size > 0)
        {
            dmArray<PropertyLua> props;
            props.SetCapacity(4);
            dmScript::PushTable(L, (const char*)buffer);
            lua_pushnil(L);
            while (lua_next(L, -2) != 0)
            {
                if (lua_isstring(L, -2))
                {
                    PropertyVar var;
                    bool valid = true;
                    switch (lua_type(L, -1))
                    {
                    case LUA_TNUMBER:
                        var.m_Type = PROPERTY_TYPE_NUMBER;
                        var.m_Number = lua_tonumber(L, -1);
                        break;
                    case LUA_TBOOLEAN:
                        var.m_Type = PROPERTY_TYPE_BOOLEAN;
                        var.m_Bool = lua_toboolean(L, -1);
                        break;
                    case LUA_TUSERDATA:
                        if (dmScript::IsHash(L, -1))
                        {
                            var.m_Type = PROPERTY_TYPE_HASH;
                            var.m_Hash = dmScript::CheckHash(L, -1);
                        }
                        else if (dmScript::IsURL(L, -1))
                        {
                            var.m_Type = PROPERTY_TYPE_URL;
                            var.m_URL = *dmScript::CheckURL(L, -1);
                        }
                        else if (dmScript::IsVector3(L, -1))
                        {
                            var.m_Type = PROPERTY_TYPE_VECTOR3;
                            Vector3 v = *dmScript::CheckVector3(L, -1);
                            var.m_V4[0] = v.getX();
                            var.m_V4[1] = v.getY();
                            var.m_V4[2] = v.getZ();
                        }
                        else if (dmScript::IsVector4(L, -1))
                        {
                            var.m_Type = PROPERTY_TYPE_VECTOR4;
                            Vector4 v = *dmScript::CheckVector4(L, -1);
                            var.m_V4[0] = v.getX();
                            var.m_V4[1] = v.getY();
                            var.m_V4[2] = v.getZ();
                            var.m_V4[3] = v.getW();
                        }
                        else if (dmScript::IsQuat(L, -1))
                        {
                            var.m_Type = PROPERTY_TYPE_QUAT;
                            Quat q = *dmScript::CheckQuat(L, -1);
                            var.m_V4[0] = q.getX();
                            var.m_V4[1] = q.getY();
                            var.m_V4[2] = q.getZ();
                            var.m_V4[3] = q.getW();
                        }
                        else
                        {
                            valid = false;
                        }
                        break;
                    default:
                        valid = false;
                        break;
                    }
                    lua_pop(L, 1);
                    if (valid)
                    {
                        if (props.Full())
                            props.SetCapacity(props.Capacity() + 4);
                        PropertyLua property;
                        property.m_Id = dmHashString64(lua_tostring(L, -1));
                        property.m_Var = var;
                        props.Push(property);
                    }
                    else
                    {
                        lua_pop(L, 1);
                        assert(top == lua_gettop(L));
                        return false;
                    }
                }
            }
            lua_pop(L, 1);
            if (!props.Empty())
            {
                dmArray<PropertyLua>* p = new dmArray<PropertyLua>();
                p->Swap(props);
                *user_data = (uintptr_t)p;
            }
        }
        assert(top == lua_gettop(L));
        return true;
    }

    void DestroyPropertySetUserDataLua(uintptr_t user_data)
    {
        if (user_data != 0)
        {
            dmArray<PropertyLua>* props = (dmArray<PropertyLua>*)user_data;
            delete props;
        }
    }

    PropertyResult GetPropertyCallbackLua(const HProperties properties, uintptr_t user_data, dmhash_t id, PropertyVar& out_var)
    {
        if (user_data != 0)
        {
            dmArray<PropertyLua>& props = *(dmArray<PropertyLua>*)user_data;
            uint32_t count = props.Size();
            for (uint32_t i = 0; i < count; ++i)
            {
                const PropertyLua& prop = props[i];
                if (prop.m_Id == id)
                {
                    out_var = prop.m_Var;
                    return PROPERTY_RESULT_OK;
                }
            }
        }
        return PROPERTY_RESULT_NOT_FOUND;
    }
}
