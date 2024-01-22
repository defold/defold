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
    /*# System API documentation
     *
     * Functions and messages for using system resources, controlling the engine,
     * error handling and debugging.
     *
     * @document
     * @name System
     * @namespace sys
     */

    enum RequestStatus
    {
        REQUEST_STATUS_ERROR_IO_ERROR  = -2,
        REQUEST_STATUS_ERROR_NOT_FOUND = -1,
        REQUEST_STATUS_INITALIZED      = 1,
        REQUEST_STATUS_PENDING         = 2,
        REQUEST_STATUS_FINISHED        = 3,
    };

    struct LuaRequest
    {
        dmScript::LuaCallbackInfo* m_CallbackInfo;
        HOpaqueHandle              m_Handle;
        dmBuffer::HBuffer          m_Payload;
        dmLoadQueue::HRequest      m_Request;
        char*                      m_Path;
        dmhash_t                   m_PathHash;
        RequestStatus              m_Status;
    };

    struct SysModule
    {
        dmResource::HFactory                m_Factory;
        dmLoadQueue::HQueue                 m_LoadQueue;
        dmOpaqueHandleContainer<LuaRequest> m_LoadRequests;
        dmMutex::HMutex                     m_LoadRequestsMutex;
        uint8_t                             m_LastUpdateResult : 1;
    } g_SysModule;

    // Assumes the g_SysModule.m_LoadRequestsMutex is held
    static bool HandleRequestCompleted(LuaRequest* request)
    {
        // The request buffer can not be touched after this call
        dmLoadQueue::FreeLoad(g_SysModule.m_LoadQueue, request->m_Request);
        bool result = true;

        if (dmScript::IsCallbackValid(request->m_CallbackInfo))
        {
            lua_State* L = dmScript::GetCallbackLuaContext(request->m_CallbackInfo);
            DM_LUA_STACK_CHECK(L, 0);

            // callback has the format:
            // function(self, request_id, result)
            //  result contains:
            //      - status: request status
            //      - buffer: if successfull, this contains the payload
            if (dmScript::SetupCallback(request->m_CallbackInfo))
            {
                lua_pushnumber(L, request->m_Handle);

                lua_newtable(L);
                lua_pushnumber(L, request->m_Status);
                lua_setfield(L, -2, "status");

                if (request->m_Status == REQUEST_STATUS_FINISHED)
                {
                    dmScript::LuaHBuffer luabuf(request->m_Payload, dmScript::OWNER_LUA);
                    dmScript::PushBuffer(L, luabuf);
                    lua_setfield(L, -2, "buffer");
                }

                result = dmScript::PCall(L, 3, 0) == 0;
                dmScript::TeardownCallback(request->m_CallbackInfo);
            }
            else
            {
                dmLogError("Failed to setup sys.load_buffer_async callback (has the calling script been destroyed?)");
                result = false;
            }
        }

        dmScript::DestroyCallback(request->m_CallbackInfo);
        request->m_CallbackInfo = 0x0;

        g_SysModule.m_LoadRequests.Release(request->m_Handle);
        free(request->m_Path);
        delete request;
        return result;
    }

    // Assumes the g_SysModule.m_LoadRequestsMutex is held (if needed)
    static dmResource::Result HandleRequestLoading(dmResource::HFactory factory, const char* path, const char* original_name, uint32_t* resource_size, dmResource::LoadBufferType* buffer, LuaRequest* request)
    {
        dmResource::Result res = dmResource::LoadResourceFromBuffer(factory, path, original_name, resource_size, buffer);

        if (res != dmResource::RESULT_OK)
        {
            FILE* file = fopen(path, "rb");
            if (file == 0x0)
            {
                request->m_Status = REQUEST_STATUS_ERROR_NOT_FOUND;
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
                request->m_Status = REQUEST_STATUS_ERROR_IO_ERROR;
                return dmResource::RESULT_IO_ERROR;
            }

            assert(nread == file_size);
            *resource_size = file_size;
        }

        return dmResource::RESULT_OK;
    }

    // Called from load queue thread
    static dmResource::Result LoadBufferFunctionCallback(dmResource::HFactory factory, const char* path, const char* original_name, uint32_t* resource_size, dmResource::LoadBufferType* buffer, void* context)
    {
        dmMutex::ScopedLock lk(g_SysModule.m_LoadRequestsMutex);

        HOpaqueHandle request_handle = (HOpaqueHandle) (uintptr_t) context;
        LuaRequest* request          = g_SysModule.m_LoadRequests.Get(request_handle);
        return HandleRequestLoading(factory, path, original_name, resource_size, buffer, request);
    }

    // Called from load queue thread
    static dmResource::Result LoadBufferCompleteCallback(const dmResource::ResourcePreloadParams& params)
    {
        dmMutex::ScopedLock lk(g_SysModule.m_LoadRequestsMutex);

        HOpaqueHandle request_handle = (HOpaqueHandle) (uintptr_t) params.m_Context;
        LuaRequest* request          = g_SysModule.m_LoadRequests.Get(request_handle);
        request->m_Status            = REQUEST_STATUS_FINISHED;

        dmBuffer::StreamDeclaration streams_decl[] = {{ dmHashString64("data"), dmBuffer::VALUE_TYPE_UINT8, 1 }};
        dmBuffer::Create(params.m_BufferSize, streams_decl, 1, &request->m_Payload);

        uint8_t* buffer_data     = 0;
        uint32_t buffer_datasize = 0;
        dmBuffer::GetBytes(request->m_Payload, (void**) &buffer_data, &buffer_datasize);
        memcpy(buffer_data, params.m_Buffer, params.m_BufferSize);

        return dmResource::RESULT_OK;
    }

    // Assumes the g_SysModule.m_LoadRequestsMutex is held
    static void DispatchRequest(LuaRequest* request)
    {
        dmLoadQueue::PreloadInfo info = {};
        info.m_CompleteFunction       = LoadBufferCompleteCallback;
        info.m_LoadResourceFunction   = LoadBufferFunctionCallback;
        info.m_Context                = (void*) (uintptr_t) request->m_Handle;
        request->m_Request            = dmLoadQueue::BeginLoad(g_SysModule.m_LoadQueue, request->m_Path, request->m_Path, &info);

        // Beginload returns zero if a load could not be started
        if (request->m_Request)
            request->m_Status = REQUEST_STATUS_PENDING;
    }

    /*# loads a buffer from a resource or disk path
     * The sys.load_buffer function will first try to load the resource
     * from any of the mounted resource locations and return the data if
     * any matching entries found. If not, the path will be tried
     * as is from the primary disk on the device.
     *
     * In order for the engine to include custom resources in the build process, you need
     * to specify them in the "custom_resources" key in your "game.project" settings file.
     * You can specify single resource files or directories. If a directory is included
     * in the resource list, all files and directories in that directory is recursively
     * included:
     *
     * For example "main/data/,assets/level_data.json".
     *
     * @name sys.load_buffer
     * @param path [type:string] the path to load the buffer from
     * @return buffer [type:buffer] the buffer with data
     * @examples
     *
     * Load binary data from a custom project resource:
     *
     * ```lua
     * local my_buffer = sys.load_buffer("/assets/my_level_data.bin")
     * local data_str = buffer.get_bytes(my_buffer, "data")
     * local has_my_header = string.sub(data_str,1,6) == "D3F0LD"
     * ```
     *
     * Load binary data from non-custom resource files on disk:
     *
     * ```lua
     * local asset_1 = sys.load_buffer("folder_next_to_binary/my_level_asset.txt")
     * local asset_2 = sys.load_buffer("/my/absolute/path")
     * ```
     */
    static int Sys_LoadBuffer(lua_State* L)
    {
        int top = lua_gettop(L);
        const char* filename = luaL_checkstring(L, 1);

        LuaRequest request;
        uint32_t load_buffer_size = 0;
        dmResource::LoadBufferType load_buffer_data;
        dmResource::Result res = HandleRequestLoading(g_SysModule.m_Factory, filename, filename, &load_buffer_size, &load_buffer_data, &request);

        if (res != dmResource::RESULT_OK)
        {
            return luaL_error(L, "sys.load_buffer failed to load the resource (code=%d)", (int) res);
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

    /*# loads a buffer from a resource or disk path asynchronously
     * The sys.load_buffer function will first try to load the resource
     * from any of the mounted resource locations and return the data if
     * any matching entries found. If not, the path will be tried
     * as is from the primary disk on the device.
     *
     * In order for the engine to include custom resources in the build process, you need
     * to specify them in the "custom_resources" key in your "game.project" settings file.
     * You can specify single resource files or directories. If a directory is included
     * in the resource list, all files and directories in that directory is recursively
     * included:
     *
     * For example "main/data/,assets/level_data.json".
     *
     * Note that issuing multiple requests of the same resource will yield
     * individual buffers per request. There is no implic caching of the buffers
     * based on request path.
     *
     * @name sys.load_buffer_async
     * @param path [type:string] the path to load the buffer from
     * @param status_callback [type:function(self, request_id, result)] A status callback that will be invoked when a request has been handled, or an error occured. The result is a table containing:
     *
     * `status`
     * : [type:number] The status of the request, supported values are:
     *
     * - `resource.REQUEST_STATUS_FINISHED`
     * - `resource.REQUEST_STATUS_ERROR_IO_ERROR`
     * - `resource.REQUEST_STATUS_ERROR_NOT_FOUND`
     *
     * `buffer`
     * : [type:buffer] If the request was successfull, this will contain the request payload in a buffer object, and nil otherwise. Make sure to check the status before doing anything with the buffer value!
     *
     * @return handle [type:handle] a handle to the request
     * @examples
     *
     * Load binary data from a custom project resource and update a texture resource:
     *
     * ```lua
     * function my_callback(self, request_id, result)
     *   if result.status == resource.REQUEST_STATUS_FINISHED then
     *      resource.set_texture("/my_texture", { ... }, result.buf)
     *   end
     * end
     *
     * local my_request = sys.load_buffer_async("/assets/my_level_data.bin", my_callback)
     * ```
     *
     * Load binary data from non-custom resource files on disk:
     *
     * ```lua
     * function my_callback(self, request_id, result)
     *   if result.status ~= sys.REQUEST_STATUS_FINISHED then
     *     -- uh oh! File could not be found, do something graceful
     *   elseif request_id == self.first_asset then
     *     -- result.buffer contains data from my_level_asset.bin
     *   elif request_id == self.second_asset then
     *     -- result.buffer contains data from 'my_level.bin'
     *   end
     * end
     *
     * function init(self)
     *   self.first_asset = hash("folder_next_to_binary/my_level_asset.bin")
     *   self.second_asset = hash("/some_absolute_path/my_level.bin")
     *   self.first_request = sys.load_buffer_async(self.first_asset, my_callback)
     *   self.second_request = sys.load_buffer_async(self.second_asset, my_callback)
     * end
     * ```
     */
    static int Sys_LoadBufferAsync(lua_State* L)
    {
        int top          = lua_gettop(L);
        const char* path = luaL_checkstring(L, 1);
        dmScript::LuaCallbackInfo* callback_info = dmScript::CreateCallback(dmScript::GetMainThread(L), 2);

        if (callback_info == 0x0)
        {
            return luaL_error(L, "sys.load_buffer_async failed to create callback");
        }

        dmhash_t path_hash = dmHashString64(path);
        {
            dmMutex::ScopedLock lk(g_SysModule.m_LoadRequestsMutex);

            // We currently don't do anything about duplicated requests here,
            // it is up to the user to manage this by themselves
            for (int i = 0; i < g_SysModule.m_LoadRequests.Capacity(); ++i)
            {
                LuaRequest* active_request = g_SysModule.m_LoadRequests.GetByIndex(i);
                if (active_request && active_request->m_PathHash == path_hash)
                {
                    dmLogWarning("sys.load_buffer_async called with path '%s' that is already pending", path);
                    break;
                }
            }

            if (g_SysModule.m_LoadRequests.Full())
            {
                g_SysModule.m_LoadRequests.Allocate(4);
            }

            LuaRequest* request     = new LuaRequest();
            request->m_Path         = strdup(path);
            request->m_PathHash     = path_hash;
            request->m_CallbackInfo = callback_info;
            request->m_Status       = REQUEST_STATUS_INITALIZED;
            request->m_Handle       = g_SysModule.m_LoadRequests.Put(request);
            request->m_Payload      = 0;
            DispatchRequest(request);
            lua_pushnumber(L, request->m_Handle);
        }

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static const luaL_reg ScriptImage_methods[] =
    {
        {"load_buffer",       Sys_LoadBuffer},
        {"load_buffer_async", Sys_LoadBufferAsync},
        {0, 0}
    };

    /*# an asyncronous request has finished successfully
     * @name sys.REQUEST_STATUS_FINISHED
     * @variable
     */

    /*# an asyncronous request is unable to read the resource
     * @name sys.REQUEST_STATUS_ERROR_IO_ERROR
     * @variable
     */

    /*# an asyncronous request is unable to locate the resource
     * @name sys.REQUEST_STATUS_ERROR_NOT_FOUND
     * @variable
     */

    void ScriptSysGameSysRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        int top = lua_gettop(L);
        (void)top;

        luaL_register(L, "sys", ScriptImage_methods);

    #define SETCONSTANT(name, val) \
        lua_pushnumber(L, (lua_Number) val); \
        lua_setfield(L, -2, #name);\

        SETCONSTANT(REQUEST_STATUS_FINISHED,        REQUEST_STATUS_FINISHED);
        SETCONSTANT(REQUEST_STATUS_ERROR_IO_ERROR,  REQUEST_STATUS_ERROR_IO_ERROR);
        SETCONSTANT(REQUEST_STATUS_ERROR_NOT_FOUND, REQUEST_STATUS_ERROR_NOT_FOUND);
    #undef SETCONSTANT

        lua_pop(L, 1);
        assert(top == lua_gettop(L));

        g_SysModule.m_Factory           = context.m_Factory;
        g_SysModule.m_LoadQueue         = dmLoadQueue::CreateQueue(context.m_Factory);
        g_SysModule.m_LoadRequestsMutex = dmMutex::New();
    }

    void ScriptSysGameSysUpdate(const ScriptLibContext& context)
    {
        bool result = true;
        if (dmMutex::TryLock(g_SysModule.m_LoadRequestsMutex))
        {
            uint32_t request_count = g_SysModule.m_LoadRequests.Capacity();
            for (int i = 0; i < request_count; ++i)
            {
                LuaRequest* request = g_SysModule.m_LoadRequests.GetByIndex(i);

                if (request)
                {
                    // If we tried to dispatch the request, but the queue was full, we try again
                    if (request->m_Status == REQUEST_STATUS_INITALIZED)
                    {
                        DispatchRequest(request);
                    }
                    else if (request->m_Status == REQUEST_STATUS_FINISHED        ||
                             request->m_Status == REQUEST_STATUS_ERROR_NOT_FOUND ||
                             request->m_Status == REQUEST_STATUS_ERROR_IO_ERROR)
                    {
                        result &= HandleRequestCompleted(request);
                    }
                    else
                    {
                        void* buf;
                        uint32_t size;
                        dmLoadQueue::LoadResult load_result;
                        if (dmLoadQueue::EndLoad(g_SysModule.m_LoadQueue, request->m_Request, &buf, &size, &load_result) == dmLoadQueue::RESULT_OK)
                        {
                            result &= HandleRequestCompleted(request);
                        }
                    }
                }
            }
            dmMutex::Unlock(g_SysModule.m_LoadRequestsMutex);
        }

        g_SysModule.m_LastUpdateResult = result;
    }

    void ScriptSysGameSysFinalize(const ScriptLibContext& context)
    {
        if (g_SysModule.m_LoadQueue)
            dmLoadQueue::DeleteQueue(g_SysModule.m_LoadQueue);
        if (g_SysModule.m_LoadRequestsMutex)
            dmMutex::Delete(g_SysModule.m_LoadRequestsMutex);

        for (int i = 0; i < g_SysModule.m_LoadRequests.Capacity(); ++i)
        {
            LuaRequest* request = g_SysModule.m_LoadRequests.GetByIndex(i);
            if (request)
            {
                if (dmBuffer::IsBufferValid(request->m_Payload))
                {
                    dmBuffer::Destroy(request->m_Payload);
                }
                dmScript::DestroyCallback(request->m_CallbackInfo);
                delete request;
            }
        }

        g_SysModule.m_Factory           = 0;
        g_SysModule.m_LoadQueue         = 0;
        g_SysModule.m_LoadRequestsMutex = 0;
    }

    // For tests
    bool GetScriptSysGameSysLastUpdateResult()
    {
        return g_SysModule.m_LastUpdateResult;
    }
}
