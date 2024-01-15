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

#include <dlib/opaque_handle_container.h>

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
    struct LuaRequest
    {
        enum State
        {
            STATE_ERROR      = -1,
            STATE_NONE       = 0,
            STATE_INITALIZED = 0,
            STATE_PENDING    = 1,
            STATE_FINISHED   = 2,
        };
        dmScript::LuaCallbackInfo* m_CallbackInfo;
        HOpaqueHandle              m_Handle;
        dmBuffer::HBuffer          m_Payload;
        dmLoadQueue::HRequest      m_Request;
        const char*                m_Path;
        State                      m_State;
    };

    struct SysModule
    {
        dmResource::HFactory                m_Factory;
        dmLoadQueue::HQueue                 m_LoadQueue;

        dmOpaqueHandleContainer<LuaRequest> m_LoadRequests;
        dmMutex::HMutex                     m_LoadRequestsMutex;
    } g_SysModule;

    // Called from load queue thread
    static dmResource::Result LoadBufferComplete(const dmResource::ResourcePreloadParams& params)
    {
        dmMutex::ScopedLock lk(g_SysModule.m_LoadRequestsMutex);
        HOpaqueHandle request_handle = (HOpaqueHandle) (uintptr_t) params.m_Context;
        LuaRequest* request          = g_SysModule.m_LoadRequests.Get(request_handle);
        request->m_State             = LuaRequest::STATE_FINISHED;

        dmBuffer::StreamDeclaration streams_decl[] = {{ dmHashString64("data"), dmBuffer::VALUE_TYPE_UINT8, 1 }};
        dmBuffer::Create(params.m_BufferSize, streams_decl, 1, &request->m_Payload);

        uint8_t* buffer_data     = 0;
        uint32_t buffer_datasize = 0;
        dmBuffer::GetBytes(request->m_Payload, (void**) &buffer_data, &buffer_datasize);
        memcpy(buffer_data, params.m_Buffer, params.m_BufferSize);

        return dmResource::RESULT_OK;
    }

    static dmResource::Result LoadBufferFunction(dmResource::HFactory factory, const char* path, const char* original_name, uint32_t* resource_size, dmResource::LoadBufferType* buffer)
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

    static int Sys_LoadBuffer(lua_State* L)
    {
        int top = lua_gettop(L);
        const char* filename = luaL_checkstring(L, 1);

        uint32_t load_buffer_size = 0;
        dmResource::LoadBufferType load_buffer_data;
        dmResource::Result res = LoadBufferFunction(g_SysModule.m_Factory, filename, filename, &load_buffer_size, &load_buffer_data);

        if (res != dmResource::RESULT_OK)
        {
            return luaL_error(L, "sys.load_buffer failed to create callback (code=%d)", (int) res);
        }

        assert(load_buffer_data.Size() == load_buffer_size);

        dmBuffer::StreamDeclaration streams_decl[] = {{ dmHashString64("data"), dmBuffer::VALUE_TYPE_UINT8, 1 }};

        dmBuffer::HBuffer buffer = 0;
        dmBuffer::Create(load_buffer_size, streams_decl, 1, &buffer);

        uint8_t* buffer_data     = 0;
        uint32_t buffer_datasize = 0;
        dmBuffer::GetBytes(buffer, (void**) &buffer_data, &buffer_datasize);
        memcpy(buffer_data, load_buffer_data.Begin(), load_buffer_size);

        dmScript::LuaHBuffer luabuf(buffer, dmScript::OWNER_LUA);
        dmScript::PushBuffer(L, luabuf);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static void DispatchRequest(LuaRequest* request)
    {
        dmLoadQueue::PreloadInfo info = {};
        info.m_CompleteFunction       = LoadBufferComplete;
        info.m_LoadResourceFunction   = LoadBufferFunction;
        info.m_Context                = (void*) (uintptr_t) request->m_Handle;
        request->m_Request            = dmLoadQueue::BeginLoad(g_SysModule.m_LoadQueue, request->m_Path, request->m_Path, &info);
        request->m_State              = request->m_Request ? LuaRequest::STATE_PENDING : LuaRequest::STATE_INITALIZED;
    }

    static int Sys_LoadBufferAsync(lua_State* L)
    {
        int top = lua_gettop(L);
        const char* filename                     = luaL_checkstring(L, 1);
        dmScript::LuaCallbackInfo* callback_info = dmScript::CreateCallback(dmScript::GetMainThread(L), 2);

        if (callback_info == 0x0)
        {
            return luaL_error(L, "sys.load_buffer_async failed to create callback");
        }

        LuaRequest* request     = new LuaRequest();
        request->m_Path         = filename;
        request->m_CallbackInfo = callback_info;
        request->m_State        = LuaRequest::STATE_NONE;
        request->m_Handle       = INVALID_OPAQUE_HANDLE;
        {
            dmMutex::ScopedLock lk(g_SysModule.m_LoadRequestsMutex);

            if (g_SysModule.m_LoadRequests.Full())
            {
                g_SysModule.m_LoadRequests.Allocate(4);
            }

            request->m_Handle = g_SysModule.m_LoadRequests.Put(request);
            DispatchRequest(request);
        }

        lua_pushnumber(L, request->m_Handle);
        assert(top + 1 == lua_gettop(L));
        return 1;
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

        g_SysModule.m_Factory           = context.m_Factory;
        g_SysModule.m_LoadQueue         = dmLoadQueue::CreateQueue(context.m_Factory);
        g_SysModule.m_LoadRequestsMutex = dmMutex::New();
    }

    static inline uint32_t GetRequestCount()
    {
        dmMutex::ScopedLock lk(g_SysModule.m_LoadRequestsMutex);
        return g_SysModule.m_LoadRequests.Capacity();
    }

    static inline LuaRequest* GetRequest(uint32_t index)
    {
        dmMutex::ScopedLock lk(g_SysModule.m_LoadRequestsMutex);
        return g_SysModule.m_LoadRequests.GetByIndex(index);
    }

    static inline void DeleteRequest(LuaRequest* request)
    {
        dmMutex::ScopedLock lk(g_SysModule.m_LoadRequestsMutex);
        g_SysModule.m_LoadRequests.Release(request->m_Handle);
        delete request;
    }

    void ScriptSysGameSysUpdate(const ScriptLibContext& context)
    {
        uint32_t request_count = GetRequestCount();
        for (int i = 0; i < request_count; ++i)
        {
            LuaRequest* request = GetRequest(i);

            if (request)
            {
                // If we tried to dispatch the request, but the queue was full, we try again
                if (request->m_State == LuaRequest::STATE_INITALIZED)
                {
                    DispatchRequest(request);
                }
                else if (request->m_State == LuaRequest::STATE_FINISHED)
                {
                    dmLoadQueue::FreeLoad(g_SysModule.m_LoadQueue, request->m_Request);

                    if (dmScript::IsCallbackValid(request->m_CallbackInfo))
                    {
                        lua_State* L = dmScript::GetCallbackLuaContext(request->m_CallbackInfo);
                        DM_LUA_STACK_CHECK(L, 0);

                        dmScript::LuaHBuffer luabuf(request->m_Payload, dmScript::OWNER_LUA);

                        if (dmScript::SetupCallback(request->m_CallbackInfo))
                        {
                            dmScript::PushBuffer(L, luabuf);
                            dmScript::PushHash(L, dmHashString64(request->m_Path));
                            dmScript::PCall(L, 3, 0);
                            dmScript::TeardownCallback(request->m_CallbackInfo);
                        }
                        else
                        {
                            dmLogError("Failed to setup sys.load_buffer_async callback (has the calling script been destroyed?)");
                        }
                    }

                    dmScript::DestroyCallback(request->m_CallbackInfo);
                    request->m_CallbackInfo = 0x0;
                    DeleteRequest(request);
                }
            }
        }
    }

    void ScriptSysGameSysFinalize(const ScriptLibContext& context)
    {
        dmLoadQueue::DeleteQueue(g_SysModule.m_LoadQueue);
        g_SysModule.m_Factory   = 0;
        g_SysModule.m_LoadQueue = 0;
    }
}
