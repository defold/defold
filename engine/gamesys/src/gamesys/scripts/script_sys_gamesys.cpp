// Copyright 2020-2024 The Defold Foundation
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

// #include <stdlib.h>
// #include <stdio.h>
// #include <stdint.h>

// #include <dlib/log.h>

#include <resource/resource.h>
#include <resource/async/load_queue.h>

#include "../gamesys.h"

#include "script_buffer.h"
#include "script_sys_gamesys.h"

extern "C"
{
    #include <lua/lua.h>
    #include <lua/lauxlib.h>
}

namespace dmGameSystem
{
    struct SysModule
    {
        dmResource::HFactory m_Factory;
        dmLoadQueue::HQueue  m_LoadQueue;
    } g_SysModule;

    static int Sys_LoadBuffer(lua_State* L)
    {
        int top = lua_gettop(L);
        const char* filename = luaL_checkstring(L, 1);

        void* buf;
        uint32_t size;

        dmMutex::ScopedLock lk(dmResource::GetLoadMutex(g_SysModule.m_Factory));
        dmResource::Result res = dmResource::LoadResource(g_SysModule.m_Factory, filename, "", &buf, &size);

        dmBuffer::StreamDeclaration streams_decl[] = {{ dmHashString64("data"), dmBuffer::VALUE_TYPE_UINT8, 1 }};

        dmBuffer::HBuffer buffer = 0;
        dmBuffer::Create(size, streams_decl, 1, &buffer);

        uint8_t* buffer_data     = 0;
        uint32_t buffer_datasize = 0;
        dmBuffer::GetBytes(buffer, (void**)&buffer_data, &buffer_datasize);
        memcpy(buffer_data, buf, size);

        dmScript::LuaHBuffer luabuf(buffer, dmScript::OWNER_LUA);
        dmScript::PushBuffer(L, luabuf);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static int Sys_LoadBufferAsync(lua_State* L)
    {
        return 0;
    }

    static const luaL_reg ScriptImage_methods[] =
    {
        {"load_buffer",       Sys_LoadBuffer},
        {"load_buffer_async", Sys_LoadBufferAsync},
        {0, 0}
    };

    void ScriptSysGameSysRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        int top = lua_gettop(L);
        (void)top;

        luaL_register(L, "sys", ScriptImage_methods);
        lua_pop(L, 1);
        assert(top == lua_gettop(L));

        g_SysModule.m_Factory   = context.m_Factory;
        g_SysModule.m_LoadQueue = dmLoadQueue::CreateQueue(context.m_Factory);
    }

    void ScriptSysGameSysFinalize(const ScriptLibContext& context)
    {
        dmLoadQueue::DeleteQueue(g_SysModule.m_LoadQueue);
        g_SysModule.m_Factory   = 0;
        g_SysModule.m_LoadQueue = 0;
    }
}
