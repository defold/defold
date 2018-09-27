#include "gameobject_props_lua.h"

#include <stdlib.h>
#include <string.h>

#include <dlib/log.h>

namespace dmGameObject
{
    static PropertyType GetPropertyType(lua_State* L, int index)
    {
        int type = lua_type(L, index);
        switch (type)
        {
            case LUA_TNUMBER:
                return PROPERTY_TYPE_NUMBER;
            case LUA_TBOOLEAN:
                return PROPERTY_TYPE_BOOLEAN;
            case LUA_TUSERDATA:
                if (dmScript::IsHash(L, index))
                {
                    return PROPERTY_TYPE_HASH;
                }
                else if (dmScript::IsURL(L, index))
                {
                    return PROPERTY_TYPE_URL;
                }
                else if (dmScript::IsVector3(L, index))
                {
                    return PROPERTY_TYPE_VECTOR3;
                }
                else if (dmScript::IsVector4(L, index))
                {
                    return PROPERTY_TYPE_VECTOR4;
                }
                else if (dmScript::IsQuat(L, index))
                {
                    return PROPERTY_TYPE_QUAT;
                }
                else
                {
                    dmLogError("Properties type can not be determined.");
                    return PROPERTY_TYPE_COUNT;
                }
                break;
            default:
                dmLogError("Properties can not be of type '%s'.", lua_typename(L, type));
                return PROPERTY_TYPE_COUNT;
        }
    }

    PropertyResult LuaToVar(lua_State* L, int index, PropertyVar& out_var)
    {
        out_var.m_Type = GetPropertyType(L, index);
        switch(out_var.m_Type)
        {
            case PROPERTY_TYPE_NUMBER:
                out_var.m_Number = lua_tonumber(L, index);
                return PROPERTY_RESULT_OK;
            case PROPERTY_TYPE_HASH:
                out_var.m_Hash = dmScript::CheckHash(L, index);
                return PROPERTY_RESULT_OK;
            case PROPERTY_TYPE_URL:
                {
                    dmMessage::URL* url = (dmMessage::URL*) out_var.m_URL;
                    *url = *dmScript::CheckURL(L, index);
                }
                return PROPERTY_RESULT_OK;
            case PROPERTY_TYPE_VECTOR3:
                {
                    Vector3 v = *dmScript::CheckVector3(L, index);
                    out_var.m_V4[0] = v.getX();
                    out_var.m_V4[1] = v.getY();
                    out_var.m_V4[2] = v.getZ();
                }
                return PROPERTY_RESULT_OK;
            case PROPERTY_TYPE_VECTOR4:
                {
                    Vector4 v = *dmScript::CheckVector4(L, index);
                    out_var.m_V4[0] = v.getX();
                    out_var.m_V4[1] = v.getY();
                    out_var.m_V4[2] = v.getZ();
                    out_var.m_V4[3] = v.getW();
                }
                return PROPERTY_RESULT_OK;
            case PROPERTY_TYPE_QUAT:
                {
                    Quat q = *dmScript::CheckQuat(L, index);
                    out_var.m_V4[0] = q.getX();
                    out_var.m_V4[1] = q.getY();
                    out_var.m_V4[2] = q.getZ();
                    out_var.m_V4[3] = q.getW();
                }
                return PROPERTY_RESULT_OK;
            case PROPERTY_TYPE_BOOLEAN:
                out_var.m_Bool = (bool) lua_toboolean(L, index);
                return PROPERTY_RESULT_OK;
            case PROPERTY_TYPE_COUNT:
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

    HPropertyContainer CreatePropertyContainerLua(void* component_context, uint8_t* buffer, uint32_t buffer_size)
    {
        lua_State* L = dmScript::GetLuaState((dmScript::HContext)component_context);
        DM_LUA_STACK_CHECK(L, 0);

        PropertyContainerParameters params;
        if (buffer_size == 0)
        {
            HPropertyContainerBuilder builder = CreatePropertyContainerBuilder(params);
            return builder == 0x0 ? 0x0 : CreatePropertyContainer(builder);
        }

        dmScript::PushTable(L, (const char*)buffer, buffer_size);

        lua_pushnil(L);
        while (lua_next(L, -2) != 0)
        {
            if (lua_isstring(L, -2))
            {
                switch(GetPropertyType(L, -1))
                {
                    case PROPERTY_TYPE_NUMBER:
                        ++params.m_NumberCount;
                        break;
                    case PROPERTY_TYPE_HASH:
                        ++params.m_HashCount;
                        break;
                    case PROPERTY_TYPE_URL:
                        ++params.m_URLCount;
                        break;
                    case PROPERTY_TYPE_VECTOR3:
                        ++params.m_Vector3Count;
                        break;
                    case PROPERTY_TYPE_VECTOR4:
                        ++params.m_Vector4Count;
                        break;
                    case PROPERTY_TYPE_QUAT:
                        ++params.m_QuatCount;
                        break;
                    case PROPERTY_TYPE_BOOLEAN:
                        ++params.m_BoolCount;
                        break;
                    case PROPERTY_TYPE_COUNT:
                        lua_pop(L, 3);
                        return 0x0;
                }
            }
            lua_pop(L, 1);
        }

        HPropertyContainerBuilder builder = CreatePropertyContainerBuilder(params);
        lua_pushnil(L);
        while (lua_next(L, -2) != 0)
        {
            if (lua_isstring(L, -2))
            {
                dmhash_t id = dmHashString64(lua_tostring(L, -2));
                switch(GetPropertyType(L, -1))
                {
                    case PROPERTY_TYPE_NUMBER:
                        PushNumber(builder, id, lua_tonumber(L, -1));
                        break;
                    case PROPERTY_TYPE_HASH:
                        PushHash(builder, id, dmScript::CheckHash(L, -1));
                        break;
                    case PROPERTY_TYPE_URL:
                        PushURL(builder, id, (const char*)dmScript::CheckURL(L, -1));
                        break;
                    case PROPERTY_TYPE_VECTOR3:
                        PushVector3(builder, id, (const float*)dmScript::CheckVector3(L, -1));
                        break;
                    case PROPERTY_TYPE_VECTOR4:
                        PushVector4(builder, id, (const float*)dmScript::CheckVector4(L, -1));
                        break;
                    case PROPERTY_TYPE_QUAT:
                        PushQuat(builder, id, (const float*)dmScript::CheckQuat(L, -1));
                        break;
                    case PROPERTY_TYPE_BOOLEAN:
                        PushBool(builder, id, lua_toboolean(L, -1));
                        break;
                    case PROPERTY_TYPE_COUNT:
                        assert(false);
                        break;
                }
            }
            lua_pop(L, 1);
        }

        lua_pop(L, 1);
        return CreatePropertyContainer(builder);
    }

}
