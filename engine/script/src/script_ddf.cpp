// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "script.h"
#include <dlib/hashtable.h>
#include <dlib/align.h>
#include <dlib/profile.h>
#include <script/ddf_script.h>

#include <string.h>
extern "C"
{
#include <dmsdk/dlua/dlua.h>
}

namespace dmScript
{
    static const dmhash_t DDF_TYPE_NAME_HASH_POINT3    = dmHashString64("point3");
    static const dmhash_t DDF_TYPE_NAME_HASH_VECTOR3   = dmHashString64("vector3");
    static const dmhash_t DDF_TYPE_NAME_HASH_VECTOR4   = dmHashString64("vector4");
    static const dmhash_t DDF_TYPE_NAME_HASH_QUAT      = dmHashString64("quat");
    static const dmhash_t DDF_TYPE_NAME_HASH_MATRIX4   = dmHashString64("matrix4");
    static const dmhash_t DDF_TYPE_NAME_HASH_LUAREF    = dmHashString64("dlua_ref");

    dmHashTable<uintptr_t, MessageDecoder> g_Decoders;

    static void DoLuaTableToDDF(dlua_State* L, const dmDDF::Descriptor* descriptor,
                                char* buffer, char** data_start, char** data_last, int index, char* pointer_base);

    static void DefaultLuaValueToDDF(dlua_State* L, const dmDDF::FieldDescriptor* f,
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
                    dluaL_error(L, "Message data doesn't fit");
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
                dluaL_error(L, "Unsupported type %d for default value in field %s", f->m_Type, f->m_Name);
            }
            break;
        }
    }

    static void UnityValueToDDF(dlua_State* L, const dmDDF::FieldDescriptor* f, char* buffer, char** data_start, char** data_end, char* pointer_base)
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
                    dluaL_error(L, "Message data doesn't fit");
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
                dluaL_error(L, "Unsupported type %d for unity value in field %s", f->m_Type, f->m_Name);
            }
            break;
        }
    }

    static void LuaValueToDDF(dlua_State* L, const dmDDF::FieldDescriptor* f,
                              char* buffer, char** data_start, char** data_end, char* pointer_base)
    {
        char* where = &buffer[f->m_Offset];
        bool nil_val = dlua_isnil(L, -1);
        bool array = false;
        uint32_t n = 1;
        uint32_t sz = 0;

        if (f->m_Label == dmDDF::LABEL_REPEATED)
        {
            dluaL_checktype(L, -1, DLUA_TTABLE);

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

            n = dlua_objlen(L, -1);
            *data_start = (char*)DM_ALIGN(*data_start, 16);
            if (*data_start + n * sz > *data_end)
            {
                dluaL_error(L, "Message too large.");
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
                dlua_rawgeti(L, -1, i + 1);
            }

            switch (f->m_Type)
            {
                case dmDDF::TYPE_INT32:
                {
                    if (nil_val)
                        *((int32_t *) where) = 0;
                    else
                        *((int32_t *) where) = (int32_t) dluaL_checkinteger(L, -1);
                }
                break;

                case dmDDF::TYPE_UINT32:
                {
                    if (nil_val)
                        *((uint32_t *) where) = 0;
                    else
                        *((uint32_t *) where) = (uint32_t) dluaL_checkinteger(L, -1);
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
                        *((bool *) where) = (bool)dlua_toboolean(L, -1) ;
                }
                break;

                case dmDDF::TYPE_FLOAT:
                {
                    if (nil_val)
                        *((float *) where) = 0.0f;
                    else
                        *((float *) where) = (float) dluaL_checknumber(L, -1);
                }
                break;

                case dmDDF::TYPE_STRING:
                {
                    const char* s = "";
                    if (!nil_val)
                        s = dluaL_checkstring(L, -1);
                    int size = strlen(s) + 1;
                    if (*data_start + size > *data_end)
                    {
                        dluaL_error(L, "Message data doesn't fit");
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
                        *((int32_t *) where) = (int32_t) dluaL_checkinteger(L, -1);
                }
                break;

                case dmDDF::TYPE_MESSAGE:
                {
                    if (!nil_val)
                    {
                        const dmDDF::Descriptor* d = f->m_MessageDescriptor;
                        const dmhash_t name_hash = d->m_NameHash;
                        bool is_vector3 = name_hash == DDF_TYPE_NAME_HASH_VECTOR3;
                        bool is_point3 = name_hash == DDF_TYPE_NAME_HASH_POINT3;

                        if (is_vector3 || is_point3)
                        {
                            dmVMath::Vector3* v = dmScript::CheckVector3(L, -1);
                            if (is_vector3)
                                *((dmVMath::Vector3 *) where) = *v;
                            else
                                *((dmVMath::Point3 *) where) = dmVMath::Point3(*v);
                        }
                        else if (name_hash == DDF_TYPE_NAME_HASH_VECTOR4)
                        {
                            dmVMath::Vector4* v = dmScript::CheckVector4(L, -1);
                            *((dmVMath::Vector4 *) where) = *v;
                        }
                        else if (name_hash == DDF_TYPE_NAME_HASH_QUAT)
                        {
                            dmVMath::Quat* q = dmScript::CheckQuat(L, -1);
                            *((dmVMath::Quat *) where) = *q;
                        }
                        else if (name_hash == DDF_TYPE_NAME_HASH_MATRIX4)
                        {
                            dmVMath::Matrix4* m = dmScript::CheckMatrix4(L, -1);
                            *((dmVMath::Matrix4*) where) = *m;
                        }
                        else
                        {
                            DoLuaTableToDDF(L, d, where, data_start, data_end, dlua_gettop(L), pointer_base);
                        }
                    }
                }
                break;

                default:
                {
                    dluaL_error(L, "Unsupported type %d in field %s", f->m_Type, f->m_Name);
                }
                break;
            }

            if (array)
            {
                dlua_pop(L, 1);
                where += sz;
            }
        }
    }

    static void DoDefaultLuaTableToDDF(dlua_State* L, const dmDDF::Descriptor* descriptor,
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

    static void DoLuaTableToDDF(dlua_State* L, const dmDDF::Descriptor* descriptor,
                                char* buffer, char** data_start, char** data_last, int index, char* pointer_base)
    {
        dluaL_checktype(L, index, DLUA_TTABLE);

        for (uint32_t i = 0; i < descriptor->m_FieldCount; ++i)
        {
            const dmDDF::FieldDescriptor* f = &descriptor->m_Fields[i];

            dlua_pushstring(L, f->m_Name);
            dlua_rawget(L, index);
            if (dlua_isnil(L, -1))
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
                    dluaL_error(L, "Field %s not specified in table", f->m_Name);
                }
            }
            else
            {
                LuaValueToDDF(L, f, buffer, data_start, data_last, pointer_base);
            }
            dlua_pop(L, 1);
        }
    }

    uint32_t CheckDDF(dlua_State* L, const dmDDF::Descriptor* descriptor, char* buffer, uint32_t buffer_size, int index)
    {
        if (index < 0)
            index = dlua_gettop(L) + 1 + index;
        uint32_t size = descriptor->m_Size;

        if (size > buffer_size)
        {
            dluaL_error(L, "sizeof(%s) > %d", descriptor->m_Name, buffer_size);
        }

        char* data_start = buffer + size;
        char* data_end = data_start + buffer_size - size;

        DoLuaTableToDDF(L, descriptor, buffer, &data_start, &data_end, index, buffer);
        return data_start - buffer;
    }

    void DDFToLuaValue(dlua_State* L, const dmDDF::FieldDescriptor* f, const char* data, uintptr_t pointers_offset)
    {
        DM_PROFILE("DDFToLuaValue");

        const char* where = &data[f->m_Offset];
        uint32_t count;
        bool array;

        uint32_t type = f->m_Type;
        if (f->m_Label == dmDDF::LABEL_REPEATED)
        {
            dmDDF::RepeatedField* repeated = (dmDDF::RepeatedField*) &data[f->m_Offset];
            where = (const char*)(repeated->m_Array + pointers_offset);
            count = repeated->m_ArrayCount;
            array = true;

            dlua_createtable(L, count, 0);
        }
        else
        {
            array = false;
            count = 1;
            where = &data[f->m_Offset];
        }

        const int*      iptr = (int32_t*) where;
        const uint32_t* uptr = (uint32_t*) where;
        const dmhash_t* hptr = (dmhash_t*) where;
        const float*    fptr = (float*) where;
        const bool*     bptr = (bool*) where;

        for (uint32_t i=0;i!=count;i++)
        {
            switch (type)
            {
                case dmDDF::TYPE_ENUM:
                case dmDDF::TYPE_INT32:
                {
                    dlua_pushinteger(L, iptr[i]);
                }
                break;

                case dmDDF::TYPE_UINT32:
                {
                    dlua_pushinteger(L, (int)uptr[i]);
                }
                break;

                case dmDDF::TYPE_UINT64:
                {
                    dmScript::PushHash(L, hptr[i]);
                }
                break;

                case dmDDF::TYPE_BOOL:
                {
                    dlua_pushboolean(L, bptr[i]);
                }
                break;

                case dmDDF::TYPE_FLOAT:
                {
                    dlua_pushnumber(L, fptr[i]);
                }
                break;

                case dmDDF::TYPE_STRING:
                {
                    uintptr_t* ptr = (uintptr_t*) where;
                    uintptr_t loc = ptr[i] + pointers_offset;
                    dlua_pushstring(L, (const char*) loc);
                }
                break;

                case dmDDF::TYPE_MESSAGE:
                {
                    const dmDDF::Descriptor* d = f->m_MessageDescriptor;
                    const char *ptr = where + i * d->m_Size;
                    if (d->m_NameHash == DDF_TYPE_NAME_HASH_VECTOR3)
                    {
                        dmScript::PushVector3(L, *((dmVMath::Vector3*) ptr));
                    }
                    else if (d->m_NameHash == DDF_TYPE_NAME_HASH_POINT3)
                    {
                        dmScript::PushVector3(L, dmVMath::Vector3(*((dmVMath::Vector3*) ptr)));
                    }
                    else if (d->m_NameHash == DDF_TYPE_NAME_HASH_VECTOR4)
                    {
                        dmScript::PushVector4(L, *((dmVMath::Vector4*) ptr));
                    }
                    else if (d->m_NameHash == DDF_TYPE_NAME_HASH_QUAT)
                    {
                        dmScript::PushQuat(L, *((dmVMath::Quat*) ptr));
                    }
                    else if (d->m_NameHash == DDF_TYPE_NAME_HASH_MATRIX4)
                    {
                        dmScript::PushMatrix4(L, *((dmVMath::Matrix4*) ptr));
                    }
                    else if (d->m_NameHash == DDF_TYPE_NAME_HASH_LUAREF)
                    {
                        dmScriptDDF::LuaRef* dlua_ref = (dmScriptDDF::LuaRef*) ptr;
                        if (dlua_ref->m_Ref)
                        {
                            dlua_rawgeti(L, DLUA_REGISTRYINDEX, dlua_ref->m_ContextTableRef);
                            dlua_rawgeti(L, -1, dlua_ref->m_Ref);
                            dlua_remove(L, -2);
                        }
                        else
                        {
                            dlua_pushnil(L);
                        }
                    }
                    else
                    {

                        uint32_t field_count = d->m_FieldCount;
                        const dmDDF::FieldDescriptor* fields = d->m_Fields;

                        dlua_createtable(L, 0, field_count);
                        for (uint32_t j = 0; j < field_count; ++j)
                        {
                            const dmDDF::FieldDescriptor* f2 = &fields[j];
                            DDFToLuaValue(L, &fields[j], ptr, pointers_offset);
                            dlua_setfield(L, -2, f2->m_Name);
                        }
                    }
                }
                break;
                default:
                {
                    dluaL_error(L, "Unsupported type %d in field %s", f->m_Type, f->m_Name);
                }
            }

            if (array)
            {
                dlua_rawseti(L, -2, i+1);
            }
        }
    }

    void PushDDFNoDecoder(dlua_State* L, const dmDDF::Descriptor* d, const char* data, bool pointers_are_offsets)
    {
        DM_PROFILE("PushDDFNoDecoder");
        uintptr_t pointers_offset = 0;
        if (pointers_are_offsets)
            pointers_offset = (uintptr_t) data;

        uint32_t field_count = d->m_FieldCount;
        const dmDDF::FieldDescriptor* fields = d->m_Fields;

        dlua_createtable(L, 0, field_count);
        for (uint32_t i = 0; i < field_count; ++i)
        {
            const dmDDF::FieldDescriptor* f = &fields[i];

            DDFToLuaValue(L, &fields[i], data, pointers_offset);
            dlua_setfield(L, -2, f->m_Name);
        }
    }

    void PushDDF(dlua_State* L, const dmDDF::Descriptor* d, const char* data)
    {
        // TODO: Can/should we use the decoders here too?
        PushDDF(L, d, data, false);
    }

    void PushDDF(dlua_State* L, const dmDDF::Descriptor* d, const char* data, bool pointers_are_offsets)
    {
        DM_PROFILE("PushDDF");
        MessageDecoder* decoder = g_Decoders.Get((uintptr_t) d);
        if (decoder) {
            Result r = (*decoder)(L, d, data); // May call PushDDFNoDecoder with different value on pointers_are_offsets
            if (r != RESULT_OK) {
                dluaL_error(L, "Failed to decode %s message (%d)", d->m_Name, r);
            }
        } else {
            PushDDFNoDecoder(L, d, data, pointers_are_offsets);
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
