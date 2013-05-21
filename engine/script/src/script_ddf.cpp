#include "script.h"
#include <dlib/hashtable.h>

#include <string.h>
extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

namespace dmScript
{
#define DDF_TYPE_NAME_POINT3    "point3"
#define DDF_TYPE_NAME_VECTOR3   "vector3"
#define DDF_TYPE_NAME_VECTOR4   "vector4"
#define DDF_TYPE_NAME_QUAT      "quat"
#define DDF_TYPE_NAME_MATRIX4   "matrix4"

    dmHashTable<uintptr_t, MessageDecoder> g_Decoders;

    static void DoLuaTableToDDF(lua_State* L, const dmDDF::Descriptor* descriptor,
                                char* buffer, char** data_start, char** data_last, int index);

    static void DefaultLuaValueToDDF(lua_State* L, const dmDDF::FieldDescriptor* f,
                                     char* buffer, char** data_start, char** data_end, const char* default_value)
    {
        switch (f->m_Type)
        {
            case dmDDF::TYPE_INT32:
            {
                *((int32_t *) &buffer[f->m_Offset]) = *((int32_t*) default_value);
            }
            break;

            case dmDDF::TYPE_UINT32:
            {
                *((uint32_t *) &buffer[f->m_Offset]) = *((uint32_t*) default_value);
            }
            break;

            case dmDDF::TYPE_UINT64:
            {
                *((dmhash_t *) &buffer[f->m_Offset]) = *((dmhash_t*) default_value);
            }
            break;

            case dmDDF::TYPE_BOOL:
            {
                *((bool *) &buffer[f->m_Offset]) = *((bool*) default_value);
            }
            break;

            case dmDDF::TYPE_FLOAT:
            {
                *((float *) &buffer[f->m_Offset]) = *((float*) default_value);
            }
            break;

            case dmDDF::TYPE_STRING:
            {
                const char* s = default_value;
                int size = strlen(s) + 1;
                if (*data_start + size > *data_end)
                {
                    luaL_error(L, "Message data doesn't fit");
                }
                else
                {
                    memcpy(*data_start, s, size);
                    // NOTE: We store offset here an relocate later...
                    *((const char**) &buffer[f->m_Offset]) = (const char*) (*data_start - buffer);
                }
                *data_start += size;
            }
            break;

            case dmDDF::TYPE_ENUM:
            {
                *((int32_t *) &buffer[f->m_Offset]) = *((int32_t*) default_value);
            }
            break;

            default:
            {
                luaL_error(L, "Unsupported type %d for default value in field %s", f->m_Type, f->m_Name);
            }
            break;
        }
    }

    static void UnityValueToDDF(lua_State* L, const dmDDF::FieldDescriptor* f, char* buffer, char** data_start, char** data_end)
    {
        switch (f->m_Type)
        {
            case dmDDF::TYPE_INT32:
            {
                *((int32_t *) &buffer[f->m_Offset]) = 0;
            }
            break;

            case dmDDF::TYPE_UINT32:
            {
                *((uint32_t *) &buffer[f->m_Offset]) = 0;
            }
            break;

            case dmDDF::TYPE_UINT64:
            {
                *((dmhash_t *) &buffer[f->m_Offset]) = 0;
            }
            break;

            case dmDDF::TYPE_BOOL:
            {
                *((bool *) &buffer[f->m_Offset]) = false;
            }
            break;

            case dmDDF::TYPE_FLOAT:
            {
                *((float *) &buffer[f->m_Offset]) = 0;
            }
            break;

            case dmDDF::TYPE_STRING:
            {
                const char* s = "";
                int size = strlen(s) + 1;
                if (*data_start + size > *data_end)
                {
                    luaL_error(L, "Message data doesn't fit");
                }
                else
                {
                    memcpy(*data_start, s, size);
                    // NOTE: We store offset here an relocate later...
                    *((const char**) &buffer[f->m_Offset]) = (const char*) (*data_start - buffer);
                }
                *data_start += size;
            }
            break;

            case dmDDF::TYPE_ENUM:
            {
                *((int32_t *) &buffer[f->m_Offset]) = 0;
            }
            break;

            default:
            {
                luaL_error(L, "Unsupported type %d for unity value in field %s", f->m_Type, f->m_Name);
            }
            break;
        }
    }

    static void LuaValueToDDF(lua_State* L, const dmDDF::FieldDescriptor* f,
                              char* buffer, char** data_start, char** data_end)
    {
    	bool nil_val = lua_isnil(L, -1);
        switch (f->m_Type)
        {
            case dmDDF::TYPE_INT32:
            {
                if (nil_val)
                    *((int32_t *) &buffer[f->m_Offset]) = 0;
                else
                    *((int32_t *) &buffer[f->m_Offset]) = (int32_t) luaL_checkinteger(L, -1);
            }
            break;

            case dmDDF::TYPE_UINT32:
            {
                if (nil_val)
                    *((uint32_t *) &buffer[f->m_Offset]) = 0;
                else
                    *((uint32_t *) &buffer[f->m_Offset]) = (uint32_t) luaL_checkinteger(L, -1);
            }
            break;

            case dmDDF::TYPE_UINT64:
            {
                if (nil_val)
                    *((dmhash_t *) &buffer[f->m_Offset]) = 0;
                else
                    *((dmhash_t *) &buffer[f->m_Offset]) = dmScript::CheckHash(L, -1);
            }
            break;

            case dmDDF::TYPE_BOOL:
            {
                if (nil_val)
                    *((bool *) &buffer[f->m_Offset]) = false;
                else
                    *((bool *) &buffer[f->m_Offset]) = (bool)lua_toboolean(L, -1) ;
            }
            break;

            case dmDDF::TYPE_FLOAT:
            {
                if (nil_val)
                    *((float *) &buffer[f->m_Offset]) = 0.0f;
                else
                    *((float *) &buffer[f->m_Offset]) = (float) luaL_checknumber(L, -1);
            }
            break;

            case dmDDF::TYPE_STRING:
            {
                const char* s = "";
                if (!nil_val)
                    s = luaL_checkstring(L, -1);
                int size = strlen(s) + 1;
                if (*data_start + size > *data_end)
                {
                    luaL_error(L, "Message data doesn't fit");
                }
                else
                {
                    memcpy(*data_start, s, size);
                    // NOTE: We store offset here an relocate later...
                    *((const char**) &buffer[f->m_Offset]) = (const char*) (*data_start - buffer);
                }
                *data_start += size;
            }
            break;

            case dmDDF::TYPE_ENUM:
            {
                if (nil_val)
                    *((int32_t *) &buffer[f->m_Offset]) = 0;
                else
                    *((int32_t *) &buffer[f->m_Offset]) = (int32_t) luaL_checkinteger(L, -1);
            }
            break;

            case dmDDF::TYPE_MESSAGE:
            {
                if (!nil_val)
                {
                    const dmDDF::Descriptor* d = f->m_MessageDescriptor;
                    bool is_vector3 = strncmp(d->m_Name, DDF_TYPE_NAME_VECTOR3, sizeof(DDF_TYPE_NAME_VECTOR3)) == 0;
                    bool is_point3 = strncmp(d->m_Name, DDF_TYPE_NAME_POINT3, sizeof(DDF_TYPE_NAME_POINT3)) == 0;
                    if (is_vector3 || is_point3)
                    {
                        Vectormath::Aos::Vector3* v = dmScript::CheckVector3(L, -1);
                        if (is_vector3)
                            *((Vectormath::Aos::Vector3 *) &buffer[f->m_Offset]) = *v;
                        else
                            *((Vectormath::Aos::Point3 *) &buffer[f->m_Offset]) = Vectormath::Aos::Point3(*v);
                    }
                    else if (strncmp(d->m_Name, DDF_TYPE_NAME_VECTOR4, sizeof(DDF_TYPE_NAME_VECTOR4)) == 0)
                    {
                        Vectormath::Aos::Vector4* v = dmScript::CheckVector4(L, -1);
                        *((Vectormath::Aos::Vector4 *) &buffer[f->m_Offset]) = *v;
                    }
                    else if (strncmp(d->m_Name, DDF_TYPE_NAME_QUAT, sizeof(DDF_TYPE_NAME_QUAT)) == 0)
                    {
                        Vectormath::Aos::Quat* q = dmScript::CheckQuat(L, -1);
                        *((Vectormath::Aos::Quat *) &buffer[f->m_Offset]) = *q;
                    }
                    else if (strncmp(d->m_Name, DDF_TYPE_NAME_MATRIX4, sizeof(DDF_TYPE_NAME_MATRIX4)) == 0)
                    {
                        Vectormath::Aos::Matrix4* m = dmScript::CheckMatrix4(L, -1);
                        *((Vectormath::Aos::Matrix4*) &buffer[f->m_Offset]) = *m;
                    }
                    else
                    {
                        DoLuaTableToDDF(L, d, &buffer[f->m_Offset], data_start, data_end, lua_gettop(L));
                    }
                }
            }
            break;

            default:
            {
                luaL_error(L, "Unsupported type %d in field %s", f->m_Type, f->m_Name);
            }
            break;
        }
    }

    static void DoDefaultLuaTableToDDF(lua_State* L, const dmDDF::Descriptor* descriptor,
                                       char* buffer, char** data_start, char** data_last)
    {
        for (uint32_t i = 0; i < descriptor->m_FieldCount; ++i)
        {
            const dmDDF::FieldDescriptor* f = &descriptor->m_Fields[i];

            if (f->m_DefaultValue)
            {
                DefaultLuaValueToDDF(L, f, buffer, data_start, data_last, f->m_DefaultValue);
            }
        }
    }

    static void DoLuaTableToDDF(lua_State* L, const dmDDF::Descriptor* descriptor,
                                char* buffer, char** data_start, char** data_last, int index)
    {
        luaL_checktype(L, index, LUA_TTABLE);

        for (uint32_t i = 0; i < descriptor->m_FieldCount; ++i)
        {
            const dmDDF::FieldDescriptor* f = &descriptor->m_Fields[i];

            lua_pushstring(L, f->m_Name);
            lua_rawget(L, index);
            if (lua_isnil(L, -1))
            {
                if (f->m_Label == dmDDF::LABEL_OPTIONAL)
                {
                    if (f->m_DefaultValue)
                    {
                        DefaultLuaValueToDDF(L, f, buffer, data_start, data_last, f->m_DefaultValue);
                    }
                    else if (f->m_Type == dmDDF::TYPE_MESSAGE)
                    {
                        DoDefaultLuaTableToDDF(L, f->m_MessageDescriptor, &buffer[f->m_Offset], data_start, data_last);
                    }
                    else
                    {
                        // No default value specified and the type != MESSAGE
                        // Set appropriate unit value, e.g. 0 and empty string ""
                        UnityValueToDDF(L, f, buffer, data_start, data_last);
                    }
                }
                else
                {
                    luaL_error(L, "Field %s not specified in table", f->m_Name);
                }
            }
            else
            {
                LuaValueToDDF(L, f, buffer, data_start, data_last);
            }
            lua_pop(L, 1);
        }
    }

    uint32_t CheckDDF(lua_State* L, const dmDDF::Descriptor* descriptor, char* buffer, uint32_t buffer_size, int index)
    {
        if (index < 0)
            index = lua_gettop(L) + 1 + index;
        uint32_t size = descriptor->m_Size;

        if (size > buffer_size)
        {
            luaL_error(L, "sizeof(%s) > %d", descriptor->m_Name, buffer_size);
        }

        char* data_start = buffer + size;
        char* data_end = data_start + buffer_size - size;

        DoLuaTableToDDF(L, descriptor, buffer, &data_start, &data_end, index);
        return data_start - buffer;
    }

    void DDFToLuaValue(lua_State* L, const dmDDF::FieldDescriptor* f, const char* data)
    {
        switch (f->m_Type)
        {
            case dmDDF::TYPE_INT32:
            {
                lua_pushinteger(L, (int) *((int32_t*) &data[f->m_Offset]));
            }
            break;

            case dmDDF::TYPE_UINT32:
            {
                lua_pushinteger(L, (int) *((uint32_t*) &data[f->m_Offset]));
            }
            break;

            case dmDDF::TYPE_UINT64:
            {
                dmScript::PushHash(L, (dmhash_t) *((dmhash_t*) &data[f->m_Offset]));
            }
            break;

            case dmDDF::TYPE_BOOL:
            {
                lua_pushboolean(L, (int) *((bool*) &data[f->m_Offset]));
            }
            break;

            case dmDDF::TYPE_FLOAT:
            {
                lua_pushnumber(L, (lua_Number) *((float*) &data[f->m_Offset]));
            }
            break;

            case dmDDF::TYPE_STRING:
            {
                lua_pushstring(L, *((const char**) &data[f->m_Offset]));
            }
            break;

            case dmDDF::TYPE_ENUM:
            {
                lua_pushinteger(L, (int) *((int32_t*) &data[f->m_Offset]));
            }
            break;

            case dmDDF::TYPE_MESSAGE:
            {
                const dmDDF::Descriptor* d = f->m_MessageDescriptor;

                if (strncmp(d->m_Name, DDF_TYPE_NAME_VECTOR3, sizeof(DDF_TYPE_NAME_VECTOR3)) == 0)
                {
                    dmScript::PushVector3(L, *((Vectormath::Aos::Vector3*) &data[f->m_Offset]));
                }
                else if (strncmp(d->m_Name, DDF_TYPE_NAME_POINT3, sizeof(DDF_TYPE_NAME_POINT3)) == 0)
                {
                    dmScript::PushVector3(L, Vectormath::Aos::Vector3(*((Vectormath::Aos::Vector3*) &data[f->m_Offset])));
                }
                else if (strncmp(d->m_Name, DDF_TYPE_NAME_VECTOR4, sizeof(DDF_TYPE_NAME_VECTOR4)) == 0)
                {
                    dmScript::PushVector4(L, *((Vectormath::Aos::Vector4*) &data[f->m_Offset]));
                }
                else if (strncmp(d->m_Name, DDF_TYPE_NAME_QUAT, sizeof(DDF_TYPE_NAME_QUAT)) == 0)
                {
                    dmScript::PushQuat(L, *((Vectormath::Aos::Quat*) &data[f->m_Offset]));
                }
                else if (strncmp(d->m_Name, DDF_TYPE_NAME_MATRIX4, sizeof(DDF_TYPE_NAME_MATRIX4)) == 0)
                {
                    dmScript::PushMatrix4(L, *((Vectormath::Aos::Matrix4*) &data[f->m_Offset]));
                }
                else
                {
                    lua_newtable(L);
                    for (uint32_t i = 0; i < d->m_FieldCount; ++i)
                    {
                        const dmDDF::FieldDescriptor* f2 = &d->m_Fields[i];
                        lua_pushstring(L, f2->m_Name);
                        DDFToLuaValue(L, &d->m_Fields[i], &data[f->m_Offset]);
                        lua_rawset(L, -3);
                    }
                }
            }
            break;

            default:
            {
                luaL_error(L, "Unsupported type %d in field %s", f->m_Type, f->m_Name);
            }
        }
    }

    void PushDDF(lua_State*L, const dmDDF::Descriptor* d, const char* data)
    {
        MessageDecoder* decoder = g_Decoders.Get((uintptr_t) d);
        if (decoder) {
            Result r = (*decoder)(L, d, data);
            if (r != RESULT_OK) {
                luaL_error(L, "Failed to decode %s message (%d)", d->m_Name, r);
            }
        } else {
            lua_newtable(L);
            for (uint32_t i = 0; i < d->m_FieldCount; ++i)
            {
                const dmDDF::FieldDescriptor* f = &d->m_Fields[i];

                lua_pushstring(L, f->m_Name);
                DDFToLuaValue(L, &d->m_Fields[i], data);
                lua_rawset(L, -3);
            }
        }
    }

    void RegisterDDFDecoder(void* descriptor, MessageDecoder decoder)
    {
        if (g_Decoders.Full())
        {
            uint32_t capacity = g_Decoders.Capacity() + 128;
            g_Decoders.SetCapacity((100 * capacity) / 80, capacity);
        }
        g_Decoders.Put((uintptr_t) descriptor, decoder);
    }

}
