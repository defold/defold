#include "script.h"
#include <dlib/hashtable.h>
#include <dlib/align.h>
#include <script/ddf_script.h>

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
#define DDF_TYPE_NAME_LUAREF    "lua_ref"

    dmHashTable<uintptr_t, MessageDecoder> g_Decoders;

    static void DoLuaTableToDDF(lua_State* L, const dmDDF::Descriptor* descriptor,
                                char* buffer, char** data_start, char** data_last, int index, char* pointer_base);

    static void DefaultLuaValueToDDF(lua_State* L, const dmDDF::FieldDescriptor* f,
                                     char* buffer, char** data_start, char** data_end, const char* default_value, char* pointer_base)
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
                    *((const char**) &buffer[f->m_Offset]) = (const char*) (*data_start - pointer_base);
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

    static void UnityValueToDDF(lua_State* L, const dmDDF::FieldDescriptor* f, char* buffer, char** data_start, char** data_end, char* pointer_base)
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
                    *((const char**) &buffer[f->m_Offset]) = (const char*) (*data_start - pointer_base);
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
                              char* buffer, char** data_start, char** data_end, char* pointer_base)
    {
        char* where = &buffer[f->m_Offset];
        bool nil_val = lua_isnil(L, -1);
        bool array = false;
        uint32_t n = 1;
        uint32_t sz = 0;

        if (f->m_Label == dmDDF::LABEL_REPEATED)
        {
            luaL_checktype(L, -1, LUA_TTABLE);

            switch (f->m_Type)
            {
                case dmDDF::TYPE_INT32:
                case dmDDF::TYPE_UINT32:
                    sz = sizeof(int32_t);
                    break;
                case dmDDF::TYPE_UINT64:
                    sz = sizeof(uint64_t);
                    break;
                case dmDDF::TYPE_BOOL:
                    sz = sizeof(bool);
                    break;
                case dmDDF::TYPE_FLOAT:
                    sz = sizeof(float);
                    break;
                case dmDDF::TYPE_STRING:
                    sz = sizeof(const char*);
                    break;
                case dmDDF::TYPE_ENUM:
                    sz = sizeof(int);
                    break;
                case dmDDF::TYPE_MESSAGE:
                    {
                        const dmDDF::Descriptor* d = f->m_MessageDescriptor;
                        sz = d->m_Size;
                        break;
                    }
                default:
                    assert(false);
            }

            n = lua_objlen(L, -1);
            *data_start = (char*)DM_ALIGN(*data_start, 16);
            if (*data_start + n * sz > *data_end)
            {
                luaL_error(L, "Message too large.");
                return;
            }

            dmDDF::RepeatedField* repeated = (dmDDF::RepeatedField*) where;
            repeated->m_ArrayCount = n;
            repeated->m_Array = (uintptr_t)*data_start - (uintptr_t)buffer;

            where = *data_start;
            *data_start += n * sz;
            array = true;
        }

        for (uint32_t i=0;i!=n;i++)
        {
            if (array)
            {
                lua_rawgeti(L, -1, i + 1);
            }

            switch (f->m_Type)
            {
                case dmDDF::TYPE_INT32:
                {
                    if (nil_val)
                        *((int32_t *) where) = 0;
                    else
                        *((int32_t *) where) = (int32_t) luaL_checkinteger(L, -1);
                }
                break;

                case dmDDF::TYPE_UINT32:
                {
                    if (nil_val)
                        *((uint32_t *) where) = 0;
                    else
                        *((uint32_t *) where) = (uint32_t) luaL_checkinteger(L, -1);
                }
                break;

                case dmDDF::TYPE_UINT64:
                {
                    if (nil_val)
                        *((dmhash_t *) where) = 0;
                    else
                        *((dmhash_t *) where) = dmScript::CheckHash(L, -1);
                }
                break;

                case dmDDF::TYPE_BOOL:
                {
                    if (nil_val)
                        *((bool *) where) = false;
                    else
                        *((bool *) where) = (bool)lua_toboolean(L, -1) ;
                }
                break;

                case dmDDF::TYPE_FLOAT:
                {
                    if (nil_val)
                        *((float *) where) = 0.0f;
                    else
                        *((float *) where) = (float) luaL_checknumber(L, -1);
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
                        *((const char**) where) = (const char*) (*data_start - pointer_base);
                    }
                    *data_start += size;
                }
                break;

                case dmDDF::TYPE_ENUM:
                {
                    if (nil_val)
                        *((int32_t *) where) = 0;
                    else
                        *((int32_t *) where) = (int32_t) luaL_checkinteger(L, -1);
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
                                *((Vectormath::Aos::Vector3 *) where) = *v;
                            else
                                *((Vectormath::Aos::Point3 *) where) = Vectormath::Aos::Point3(*v);
                        }
                        else if (strncmp(d->m_Name, DDF_TYPE_NAME_VECTOR4, sizeof(DDF_TYPE_NAME_VECTOR4)) == 0)
                        {
                            Vectormath::Aos::Vector4* v = dmScript::CheckVector4(L, -1);
                            *((Vectormath::Aos::Vector4 *) where) = *v;
                        }
                        else if (strncmp(d->m_Name, DDF_TYPE_NAME_QUAT, sizeof(DDF_TYPE_NAME_QUAT)) == 0)
                        {
                            Vectormath::Aos::Quat* q = dmScript::CheckQuat(L, -1);
                            *((Vectormath::Aos::Quat *) where) = *q;
                        }
                        else if (strncmp(d->m_Name, DDF_TYPE_NAME_MATRIX4, sizeof(DDF_TYPE_NAME_MATRIX4)) == 0)
                        {
                            Vectormath::Aos::Matrix4* m = dmScript::CheckMatrix4(L, -1);
                            *((Vectormath::Aos::Matrix4*) where) = *m;
                        }
                        else
                        {
                            DoLuaTableToDDF(L, d, where, data_start, data_end, lua_gettop(L), pointer_base);
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

            if (array)
            {
                lua_pop(L, 1);
                where += sz;
            }
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
                DefaultLuaValueToDDF(L, f, buffer, data_start, data_last, f->m_DefaultValue, buffer);
            }
        }
    }

    static void DoLuaTableToDDF(lua_State* L, const dmDDF::Descriptor* descriptor,
                                char* buffer, char** data_start, char** data_last, int index, char* pointer_base)
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
                        DefaultLuaValueToDDF(L, f, buffer, data_start, data_last, f->m_DefaultValue, pointer_base);
                    }
                    else if (f->m_Type == dmDDF::TYPE_MESSAGE)
                    {
                        DoDefaultLuaTableToDDF(L, f->m_MessageDescriptor, &buffer[f->m_Offset], data_start, data_last);
                    }
                    else
                    {
                        // No default value specified and the type != MESSAGE
                        // Set appropriate unit value, e.g. 0 and empty string ""
                        UnityValueToDDF(L, f, buffer, data_start, data_last, pointer_base);
                    }
                }
                else
                {
                    luaL_error(L, "Field %s not specified in table", f->m_Name);
                }
            }
            else
            {
                LuaValueToDDF(L, f, buffer, data_start, data_last, pointer_base);
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

        DoLuaTableToDDF(L, descriptor, buffer, &data_start, &data_end, index, buffer);
        return data_start - buffer;
    }

    void DDFToLuaValue(lua_State* L, const dmDDF::FieldDescriptor* f, const char* data, uintptr_t pointers_offset)
    {
        const char *where = &data[f->m_Offset];
        uint32_t count = 1;
        bool array = false;

        if (f->m_Label == dmDDF::LABEL_REPEATED)
        {
            dmDDF::RepeatedField* repeated = (dmDDF::RepeatedField*) &data[f->m_Offset];
            where = (const char*)(repeated->m_Array + pointers_offset);
            count = repeated->m_ArrayCount;
            array = true;
            lua_newtable(L);
        }

        for (uint32_t i=0;i!=count;i++)
        {
            switch (f->m_Type)
            {
                case dmDDF::TYPE_INT32:
                {
                    int32_t* ptr = (int32_t*) where;
                    lua_pushinteger(L, (int) ptr[i]);
                }
                break;

                case dmDDF::TYPE_UINT32:
                {
                    uint32_t* ptr = (uint32_t*) where;
                    lua_pushinteger(L, (int) ptr[i]);
                }
                break;

                case dmDDF::TYPE_UINT64:
                {
                    dmhash_t* ptr = (dmhash_t*) where;
                    dmScript::PushHash(L, ptr[i]);
                }
                break;

                case dmDDF::TYPE_BOOL:
                {
                    bool* ptr = (bool*) where;
                    lua_pushboolean(L, ptr[i]);
                }
                break;

                case dmDDF::TYPE_FLOAT:
                {
                    float* ptr = (float*) where;
                    lua_pushnumber(L, ptr[i]);
                }
                break;

                case dmDDF::TYPE_STRING:
                {
                    uintptr_t* ptr = (uintptr_t*) where;
                    uintptr_t loc = ptr[i] + pointers_offset;
                    lua_pushstring(L, (const char*) loc);
                }
                break;

                case dmDDF::TYPE_ENUM:
                {
                    int* ptr = (int*) where;
                    lua_pushinteger(L, ptr[i]);
                }
                break;

                case dmDDF::TYPE_MESSAGE:
                {
                    const dmDDF::Descriptor* d = f->m_MessageDescriptor;
                    const char *ptr = where + i * d->m_Size;
                    if (strncmp(d->m_Name, DDF_TYPE_NAME_VECTOR3, sizeof(DDF_TYPE_NAME_VECTOR3)) == 0)
                    {
                        dmScript::PushVector3(L, *((Vectormath::Aos::Vector3*) ptr));
                    }
                    else if (strncmp(d->m_Name, DDF_TYPE_NAME_POINT3, sizeof(DDF_TYPE_NAME_POINT3)) == 0)
                    {
                        dmScript::PushVector3(L, Vectormath::Aos::Vector3(*((Vectormath::Aos::Vector3*) ptr)));
                    }
                    else if (strncmp(d->m_Name, DDF_TYPE_NAME_VECTOR4, sizeof(DDF_TYPE_NAME_VECTOR4)) == 0)
                    {
                        dmScript::PushVector4(L, *((Vectormath::Aos::Vector4*) ptr));
                    }
                    else if (strncmp(d->m_Name, DDF_TYPE_NAME_QUAT, sizeof(DDF_TYPE_NAME_QUAT)) == 0)
                    {
                        dmScript::PushQuat(L, *((Vectormath::Aos::Quat*) ptr));
                    }
                    else if (strncmp(d->m_Name, DDF_TYPE_NAME_MATRIX4, sizeof(DDF_TYPE_NAME_MATRIX4)) == 0)
                    {
                        dmScript::PushMatrix4(L, *((Vectormath::Aos::Matrix4*) ptr));
                    }
                    else if (strncmp(d->m_Name, DDF_TYPE_NAME_LUAREF, sizeof(DDF_TYPE_NAME_LUAREF)) == 0)
                    {
                        dmScriptDDF::LuaRef* lua_ref = (dmScriptDDF::LuaRef*) ptr;
                        if (lua_ref->m_Ref)
                        {
                            lua_rawgeti(L, LUA_REGISTRYINDEX, lua_ref->m_ReferenceTableRef);
                            lua_rawgeti(L, -1, lua_ref->m_Ref);
                            lua_remove(L, -2);
                        }
                    }
                    else
                    {
                        lua_newtable(L);
                        for (uint32_t j = 0; j < d->m_FieldCount; ++j)
                        {
                            const dmDDF::FieldDescriptor* f2 = &d->m_Fields[j];
                            lua_pushstring(L, f2->m_Name);
                            DDFToLuaValue(L, &d->m_Fields[j], ptr, pointers_offset);
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

            if (array)
            {
                lua_rawseti(L, -2, i+1);
            }
        }
    }

    void PushDDF(lua_State*L, const dmDDF::Descriptor* d, const char* data)
    {
        PushDDF(L, d, data, false);
    }

    void PushDDF(lua_State*L, const dmDDF::Descriptor* d, const char* data, bool pointers_are_offsets)
    {
        MessageDecoder* decoder = g_Decoders.Get((uintptr_t) d);
        if (decoder) {
            Result r = (*decoder)(L, d, data);
            if (r != RESULT_OK) {
                luaL_error(L, "Failed to decode %s message (%d)", d->m_Name, r);
            }
        } else {
            uintptr_t pointers_offset = 0;

            if (pointers_are_offsets)
                pointers_offset = (uintptr_t) data;

            lua_newtable(L);
            for (uint32_t i = 0; i < d->m_FieldCount; ++i)
            {
                const dmDDF::FieldDescriptor* f = &d->m_Fields[i];

                lua_pushstring(L, f->m_Name);
                DDFToLuaValue(L, &d->m_Fields[i], data, pointers_offset);
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
