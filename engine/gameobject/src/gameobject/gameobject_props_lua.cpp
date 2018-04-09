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

    PropertyResult LuaToVar(lua_State* L, int index, PropertyVar& out_var)
    {
        int type = lua_type(L, index);
        switch (type)
        {
        case LUA_TNUMBER:
            out_var.m_Type = PROPERTY_TYPE_NUMBER;
            out_var.m_Number = lua_tonumber(L, index);
            return PROPERTY_RESULT_OK;
        case LUA_TBOOLEAN:
            out_var.m_Type = PROPERTY_TYPE_BOOLEAN;
            out_var.m_Bool = (bool) lua_toboolean(L, index);
            return PROPERTY_RESULT_OK;
        case LUA_TUSERDATA:
            if (dmScript::IsHash(L, index))
            {
                out_var.m_Type = PROPERTY_TYPE_HASH;
                out_var.m_Hash = dmScript::CheckHash(L, index);
                return PROPERTY_RESULT_OK;
            }
            else if (dmScript::IsURL(L, index))
            {
                out_var.m_Type = PROPERTY_TYPE_URL;
                dmMessage::URL* url = (dmMessage::URL*) out_var.m_URL;
                *url = *dmScript::CheckURL(L, index);
                return PROPERTY_RESULT_OK;
            }
            else if (dmScript::IsVector3(L, index))
            {
                out_var.m_Type = PROPERTY_TYPE_VECTOR3;
                Vector3 v = *dmScript::CheckVector3(L, index);
                out_var.m_V4[0] = v.getX();
                out_var.m_V4[1] = v.getY();
                out_var.m_V4[2] = v.getZ();
                return PROPERTY_RESULT_OK;
            }
            else if (dmScript::IsVector4(L, index))
            {
                out_var.m_Type = PROPERTY_TYPE_VECTOR4;
                Vector4 v = *dmScript::CheckVector4(L, index);
                out_var.m_V4[0] = v.getX();
                out_var.m_V4[1] = v.getY();
                out_var.m_V4[2] = v.getZ();
                out_var.m_V4[3] = v.getW();
                return PROPERTY_RESULT_OK;
            }
            else if (dmScript::IsQuat(L, index))
            {
                out_var.m_Type = PROPERTY_TYPE_QUAT;
                Quat q = *dmScript::CheckQuat(L, index);
                out_var.m_V4[0] = q.getX();
                out_var.m_V4[1] = q.getY();
                out_var.m_V4[2] = q.getZ();
                out_var.m_V4[3] = q.getW();
                return PROPERTY_RESULT_OK;
            }
            else
            {
                return PROPERTY_RESULT_UNSUPPORTED_TYPE;
            }
            break;
        default:
            dmLogError("Properties can not be of type '%s'.", lua_typename(L, type));
            return PROPERTY_RESULT_UNSUPPORTED_TYPE;
        }
    }

    void LuaPushVar(lua_State* L, const PropertyVar& var)
    {
        switch (var.m_Type)
        {
        case PROPERTY_TYPE_NUMBER:
            lua_pushnumber(L, var.m_Number);
            break;
        case PROPERTY_TYPE_HASH:
            dmScript::PushHash(L, var.m_Hash);
            break;
        case PROPERTY_TYPE_URL:
        {
            dmMessage::URL* url = (dmMessage::URL*) var.m_URL;
            dmScript::PushURL(L, *url);
        }
            break;
        case PROPERTY_TYPE_VECTOR3:
            dmScript::PushVector3(L, Vectormath::Aos::Vector3(var.m_V4[0], var.m_V4[1], var.m_V4[2]));
            break;
        case PROPERTY_TYPE_VECTOR4:
            dmScript::PushVector4(L, Vectormath::Aos::Vector4(var.m_V4[0], var.m_V4[1], var.m_V4[2], var.m_V4[3]));
            break;
        case PROPERTY_TYPE_QUAT:
            dmScript::PushQuat(L, Vectormath::Aos::Quat(var.m_V4[0], var.m_V4[1], var.m_V4[2], var.m_V4[3]));
            break;
        case PROPERTY_TYPE_BOOLEAN:
            lua_pushboolean(L, var.m_Bool);
            break;
        default:
            break;
        }
    }

    PropertyResult CreatePropertySetUserDataLua(void* component_context, uint8_t* buffer, uint32_t buffer_size, uintptr_t* user_data)
    {
        lua_State* L = dmScript::GetLuaState((dmScript::HContext)component_context);
        int top = lua_gettop(L);
        (void)top;
        if (buffer_size > 0)
        {
            dmArray<PropertyLua> props;
            props.SetCapacity(4);
            dmScript::PushTable(L, (const char*)buffer, buffer_size);
            lua_pushnil(L);
            while (lua_next(L, -2) != 0)
            {
                if (lua_isstring(L, -2))
                {
                    PropertyVar var;
                    PropertyResult result = LuaToVar(L, -1, var);
                    lua_pop(L, 1);
                    if (result == PROPERTY_RESULT_OK)
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
                        lua_pop(L, 2);
                        assert(top == lua_gettop(L));
                        return result;
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
        return PROPERTY_RESULT_OK;
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
