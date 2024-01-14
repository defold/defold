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

    struct LuaRequest
    {
        dmScript::LuaCallbackInfo* m_CallbackInfo;
        dmLoadQueue::HRequest      m_LoadRequest;
    };

    static int Sys_LoadBuffer(lua_State* L)
    {
        int top = lua_gettop(L);
        const char* filename = luaL_checkstring(L, 1);

        void* buf;
        uint32_t size;

        dmMutex::ScopedLock lk(dmResource::GetLoadMutex(g_SysModule.m_Factory));
        dmResource::Result res = dmResource::LoadResource(g_SysModule.m_Factory, filename, "", &buf, &size);

        if (res != dmResource::RESULT_OK)
        {
            return luaL_error(L, "sys.load_buffer failed to create callback");
        }

        dmBuffer::StreamDeclaration streams_decl[] = {{ dmHashString64("data"), dmBuffer::VALUE_TYPE_UINT8, 1 }};

        dmBuffer::HBuffer buffer = 0;
        dmBuffer::Create(size, streams_decl, 1, &buffer);

        uint8_t* buffer_data     = 0;
        uint32_t buffer_datasize = 0;
        dmBuffer::GetBytes(buffer, (void**) &buffer_data, &buffer_datasize);
        memcpy(buffer_data, buf, size);

        dmScript::LuaHBuffer luabuf(buffer, dmScript::OWNER_LUA);
        dmScript::PushBuffer(L, luabuf);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static dmResource::Result LoadBufferAsyncOnComplete(const dmResource::ResourcePreloadParams& params)
    {
        LuaRequest* request = (LuaRequest*) params.m_Context;

        dmLoadQueue::FreeLoad(g_SysModule.m_LoadQueue, request->m_LoadRequest);

        if (!dmScript::IsCallbackValid(request->m_CallbackInfo))
        {
            return dmResource::RESULT_UNKNOWN_ERROR;
        }

        lua_State* L = dmScript::GetCallbackLuaContext(request->m_CallbackInfo);

        DM_LUA_STACK_CHECK(L, 0);

        if (!dmScript::SetupCallback(request->m_CallbackInfo))
        {
            dmLogError("Failed to setup state changed callback (has the calling script been destroyed?)");
            dmScript::DestroyCallback(request->m_CallbackInfo);
            request->m_CallbackInfo = 0x0;
            return dmResource::RESULT_UNKNOWN_ERROR;
        }

        dmBuffer::StreamDeclaration streams_decl[] = {{ dmHashString64("data"), dmBuffer::VALUE_TYPE_UINT8, 1 }};

        dmBuffer::HBuffer buffer = 0;
        dmBuffer::Create(params.m_BufferSize, streams_decl, 1, &buffer);

        uint8_t* buffer_data     = 0;
        uint32_t buffer_datasize = 0;
        dmBuffer::GetBytes(buffer, (void**) &buffer_data, &buffer_datasize);
        memcpy(buffer_data, params.m_Buffer, params.m_BufferSize);

        dmScript::LuaHBuffer luabuf(buffer, dmScript::OWNER_LUA);
        dmScript::PushBuffer(L, luabuf);

        dmScript::PCall(L, 2, 0);

        dmScript::TeardownCallback(request->m_CallbackInfo);
        dmScript::DestroyCallback(request->m_CallbackInfo);
        request->m_CallbackInfo = 0x0;

        return dmResource::RESULT_OK;
    }

    static dmResource::Result LoadBufferAsyncOnLoad(dmResource::HFactory factory, const char* path, const char* original_name, uint32_t* resource_size, dmResource::LoadBufferType* buffer)
    {
        dmResource::Result res = dmResource::DoLoadResource(factory, path, original_name, resource_size, buffer);

        if (res != dmResource::RESULT_OK)
        {
            FILE* file = fopen(path, "rb");
            if (file == 0x0)
            {
                return dmResource::RESULT_RESOURCE_NOT_FOUND;
            }

            fseek(file, 0L, SEEK_END);
            uint32_t file_size = ftell(file);
            fseek(file, 0L, SEEK_SET);

            buffer->SetCapacity(file_size);
            buffer->SetSize(file_size);

            size_t nread = fread(buffer->Begin(), 1, file_size, file);
            bool result = ferror(file) == 0;
            fclose(file);

            if (!result)
            {
                buffer->SetCapacity(0);
                return dmResource::RESULT_IO_ERROR;
            }

            *resource_size = file_size;
        }

        return dmResource::RESULT_OK;
    }

    static int Sys_LoadBufferAsync(lua_State* L)
    {
        int top = lua_gettop(L);
        const char* filename = luaL_checkstring(L, 1);

        LuaRequest* request = new LuaRequest;

        request->m_CallbackInfo = dmScript::CreateCallback(dmScript::GetMainThread(L), 2);
        if (request->m_CallbackInfo == 0x0)
        {
            return luaL_error(L, "sys.load_buffer_async failed to create callback");
        }

        dmLoadQueue::PreloadInfo info = {};
        info.m_CompleteFunction     = LoadBufferAsyncOnComplete;
        info.m_LoadResourceFunction = LoadBufferAsyncOnLoad;
        info.m_Context              = request;

        request->m_LoadRequest = dmLoadQueue::BeginLoad(g_SysModule.m_LoadQueue, filename, filename, &info);

        // assert(top + 1 == lua_gettop(L));
        // return 1;
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
