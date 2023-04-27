// Copyright 2020-2023 The Defold Foundation
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <dlib/log.h>

#include <dmsdk/gamesys/script.h>

#include <graphics/graphics.h>

#include "../gamesys.h"
#include "../gamesys_private.h"

#include "script_buffer.h"

extern "C"
{
    #include <lua/lua.h>
    #include <lua/lauxlib.h>
}

namespace dmGameSystem
{
    static const char* MODULE_LIB_NAME = "graphics";

    static dmGraphics::HContext g_GraphicsContext = 0;

    static int Graphics_StorageBuffer(lua_State* L)
    {
        assert(g_GraphicsContext);
        int top = lua_gettop(L);

        dmArray<dmGraphics::StorageBufferElement> elements;
        elements.SetCapacity(4);

        luaL_checktype(L, 1, LUA_TTABLE);
        {
            lua_pushnil(L);
            while (lua_next(L, 1) != 0)
            {
                dmhash_t key;
                dmGraphics::Type type;
                uint32_t size;

                luaL_checktype(L, -1, LUA_TTABLE);
                lua_pushvalue(L, -1);

                lua_getfield(L, -1, "name");
                key  = dmScript::CheckHashOrString(L, -1);
                lua_pop(L, 1);

                lua_getfield(L, -1, "type");
                type = (dmGraphics::Type) luaL_checkinteger(L, -1);
                lua_pop(L, 1);

                lua_getfield(L, -1, "size");
                size = luaL_checkinteger(L, -1);
                lua_pop(L, 1);

                dmGraphics::StorageBufferElement new_element;
                new_element.m_NameHash = key;
                new_element.m_Type     = type;
                new_element.m_Size     = size;

                elements.Push(new_element);

                lua_pop(L, 2);
            }
        }

        dmGraphics::HStorageBuffer storage_buffer = dmGraphics::NewStorageBuffer(g_GraphicsContext, elements.Begin(), elements.Size());
        lua_pushnumber(L, storage_buffer);

        assert((top+1) == lua_gettop(L));
        return 1;
    }

    static dmGraphics::StorageBufferElement* GetStorageBufferElement(dmhash_t name_hash, dmGraphics::StorageBufferElement* elements, uint32_t element_count)
    {
        for (int i = 0; i < element_count; ++i)
        {
            if (elements[i].m_NameHash == name_hash)
            {
                return elements + i;
            }
        }
        return 0;
    }

    /*
    TYPE_BYTE             = 0,
    TYPE_UNSIGNED_BYTE    = 1,
    TYPE_SHORT            = 2,
    TYPE_UNSIGNED_SHORT   = 3,
    TYPE_INT              = 4,
    TYPE_UNSIGNED_INT     = 5,
    TYPE_FLOAT            = 6,
    TYPE_FLOAT_VEC4       = 7,
    TYPE_FLOAT_MAT4       = 8,
    TYPE_SAMPLER_2D       = 9,
    TYPE_SAMPLER_CUBE     = 10,
    TYPE_SAMPLER_2D_ARRAY = 11,
    */

    static int Graphics_SetStorageBuffer(lua_State* L)
    {
        assert(g_GraphicsContext);
        int top = lua_gettop(L);

        dmGraphics::HStorageBuffer storage_buffer = luaL_checknumber(L, 1);

        if (!dmGraphics::IsAssetHandleValid(g_GraphicsContext, storage_buffer))
        {
            return luaL_error(L, "Storage buffer handle is not valid.");
        }

        dmGraphics::StorageBufferElement* elements;
        uint32_t element_count;
        uint32_t data_size;
        dmGraphics::GetStorageBufferInfo(g_GraphicsContext, storage_buffer, &data_size, &elements, &element_count);

        uint8_t* data_ptr = (uint8_t*) malloc(data_size);
        memset(data_ptr, 0, data_size);

        if (lua_istable(L, 2))
        {
            lua_pushnil(L);
            while (lua_next(L, 2) != 0)
            {
                dmhash_t name_hash = dmScript::CheckHashOrString(L, -2);
                dmGraphics::StorageBufferElement* ele = GetStorageBufferElement(name_hash, elements, element_count);

                if (ele)
                {
                    luaL_checktype(L, -1, LUA_TTABLE);
                    lua_pushvalue(L, -1);
                    lua_pushnil(L);

                    uint8_t* data_write_ptr = data_ptr + ele->m_Offset;

                    while (lua_next(L, -2))
                    {
                        lua_pushvalue(L, -2);

                        switch(ele->m_Type)
                        {
                            case dmGraphics::TYPE_FLOAT:
                                *((float*) data_write_ptr) = luaL_checknumber(L, -2);
                                data_write_ptr += sizeof(float);
                                break;
                            case dmGraphics::TYPE_FLOAT_VEC4:
                                memcpy(data_write_ptr, dmScript::CheckVector4(L, -2), sizeof(dmVMath::Vector4));
                                data_write_ptr += sizeof(dmVMath::Vector4);
                                break;
                        }

                        lua_pop(L, 2);
                    }
                    lua_pop(L, 1);
                }
                else
                {
                    char buf[128];
                    dmLogWarning("Storage buffer %s does not have any member called %s",
                        dmGraphics::AssetHandleToString(storage_buffer, buf, sizeof(buf)), dmHashReverseSafe64(name_hash));
                }

                lua_pop(L, 1);
            }
        }

        dmGraphics::SetStorageBufferData(g_GraphicsContext, storage_buffer, data_ptr, data_size);

        free(data_ptr);

        assert(top == lua_gettop(L));
        return 0;
    }

    static const luaL_reg ScriptGraphics_methods[] =
    {
        {"storage_buffer",     Graphics_StorageBuffer},
        {"set_storage_buffer", Graphics_SetStorageBuffer},
        {0, 0}
    };

    static void LuaInit(lua_State* L)
    {
        int top = lua_gettop(L);

        luaL_register(L, MODULE_LIB_NAME, ScriptGraphics_methods);

    #define SETGRAPHICSCONSTANT(name) \
        lua_pushnumber(L, (lua_Number) dmGraphics:: name); \
        lua_setfield(L, -2, #name); \

        SETGRAPHICSCONSTANT(TYPE_BYTE);
        SETGRAPHICSCONSTANT(TYPE_UNSIGNED_BYTE);
        SETGRAPHICSCONSTANT(TYPE_SHORT);
        SETGRAPHICSCONSTANT(TYPE_UNSIGNED_SHORT);
        SETGRAPHICSCONSTANT(TYPE_INT);
        SETGRAPHICSCONSTANT(TYPE_UNSIGNED_INT);
        SETGRAPHICSCONSTANT(TYPE_FLOAT);
        SETGRAPHICSCONSTANT(TYPE_FLOAT_VEC4);
        SETGRAPHICSCONSTANT(TYPE_FLOAT_MAT4);
        SETGRAPHICSCONSTANT(TYPE_SAMPLER_2D);
        SETGRAPHICSCONSTANT(TYPE_SAMPLER_CUBE);
        SETGRAPHICSCONSTANT(TYPE_SAMPLER_2D_ARRAY);
    #undef SETGRAPHICSCONSTANT

        lua_pop(L, 1);

        assert(top == lua_gettop(L));
    }

    void ScriptGraphicsRegister(const ScriptLibContext& context)
    {
        LuaInit(context.m_LuaState);
        g_GraphicsContext = context.m_GraphicsContext;
    }

    void ScriptGraphicsFinalize(const ScriptLibContext& context)
    {
    }
}
