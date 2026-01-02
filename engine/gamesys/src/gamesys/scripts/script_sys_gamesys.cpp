// Copyright 2020-2025 The Defold Foundation
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
#include <dlib/job_thread.h>

#include <resource/resource.h>

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
     * @language Lua
     */

    enum RequestStatus
    {
        REQUEST_STATUS_ERROR_IO_ERROR  = -2,
        REQUEST_STATUS_ERROR_NOT_FOUND = -1,
        REQUEST_STATUS_PENDING         = 1,
        REQUEST_STATUS_FINISHED        = 2,
    };

    struct LuaRequest
    {
        dmScript::LuaCallbackInfo* m_CallbackInfo;
        HOpaqueHandle              m_Handle;
        dmBuffer::HBuffer          m_Payload;
        dmResource::LoadBufferType m_LoadBuffer;
        char*                      m_Path;
        dmhash_t                   m_PathHash;
        RequestStatus              m_Status;
    };

    struct SysModule
    {
        dmResource::HFactory                m_Factory;
        dmJobThread::HContext               m_JobThread;
        dmOpaqueHandleContainer<LuaRequest> m_LoadRequests;
        dmMutex::HMutex                     m_LoadRequestsMutex;
        uint8_t                             m_LastUpdateResult : 1; // For tests

        SysModule()
        {
            memset(this, 0, sizeof(*this));
        }
    } g_SysModule;

    // Assumes the g_SysModule.m_LoadRequestsMutex is held
    static bool HandleRequestCompleted(LuaRequest* request)
    {
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
    static dmResource::Result HandleRequestLoading(dmResource::HFactory factory, const char* path, const char* original_name, dmResource::LoadBufferType* buffer, LuaRequest* request)
    {
        uint32_t buffer_size;
        uint32_t resource_size;
        dmResource::Result res = dmResource::LoadResourceToBuffer(factory, path, original_name, RESOURCE_INVALID_PRELOAD_SIZE, &resource_size, &buffer_size, buffer);

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
        }

        return dmResource::RESULT_OK;
    }

    // Called from job thread
    static int LoadBufferFunctionCallback(dmJobThread::HContext, dmJobThread::HJob hjob, void* context, void* data)
    {
        DM_MUTEX_SCOPED_LOCK(g_SysModule.m_LoadRequestsMutex);
        HOpaqueHandle request_handle = (HOpaqueHandle) (uintptr_t) context;
        LuaRequest* request          = g_SysModule.m_LoadRequests.Get(request_handle);
        return (int) HandleRequestLoading(g_SysModule.m_Factory,
            request->m_Path, request->m_Path, &request->m_LoadBuffer, request);
    }

    // Called from the main thread
    static void LoadBufferCompleteCallback(dmJobThread::HContext, dmJobThread::HJob hjob, dmJobThread::JobStatus status, void* context, void* data, int result)
    {
        if (((dmResource::Result) result) == dmResource::RESULT_OK)
        {
            DM_MUTEX_SCOPED_LOCK(g_SysModule.m_LoadRequestsMutex);
            HOpaqueHandle request_handle = (HOpaqueHandle) (uintptr_t) context;
            LuaRequest* request          = g_SysModule.m_LoadRequests.Get(request_handle);
            request->m_Status            = REQUEST_STATUS_FINISHED;
            dmBuffer::StreamDeclaration streams_decl[] = {{ dmHashString64("data"), dmBuffer::VALUE_TYPE_UINT8, 1 }};
            dmBuffer::Create(request->m_LoadBuffer.Size(), streams_decl, 1, &request->m_Payload);

            uint8_t* buffer_data     = 0;
            uint32_t buffer_datasize = 0;
            dmBuffer::GetBytes(request->m_Payload, (void**) &buffer_data, &buffer_datasize);
            memcpy(buffer_data, request->m_LoadBuffer.Begin(), request->m_LoadBuffer.Size());
        }
    }

    // Assumes the g_SysModule.m_LoadRequestsMutex is held
    static void DispatchRequest(LuaRequest* request)
    {
        dmJobThread::Job job = {0};
        job.m_Process = LoadBufferFunctionCallback;
        job.m_Callback = LoadBufferCompleteCallback;
        job.m_Context = (void*) (uintptr_t) request->m_Handle;
        job.m_Data = 0;

        dmJobThread::HJob hjob = dmJobThread::CreateJob(g_SysModule.m_JobThread, &job);
        dmJobThread::PushJob(g_SysModule.m_JobThread, hjob);
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
        dmResource::LoadBufferType load_buffer_data;
        dmResource::Result res = HandleRequestLoading(g_SysModule.m_Factory, filename, filename, &load_buffer_data, &request);

        if (res != dmResource::RESULT_OK)
        {
            return luaL_error(L, "sys.load_buffer failed to load the resource (code=%d)", (int) res);
        }

        dmBuffer::StreamDeclaration streams_decl[] = {{ dmHashString64("data"), dmBuffer::VALUE_TYPE_UINT8, 1 }};

        dmBuffer::HBuffer buffer = 0;
        dmBuffer::Create(load_buffer_data.Size(), streams_decl, 1, &buffer);

        uint8_t* buffer_data     = 0;
        uint32_t buffer_datasize = 0;
        dmBuffer::GetBytes(buffer, (void**) &buffer_data, &buffer_datasize);
        memcpy(buffer_data, load_buffer_data.Begin(), load_buffer_data.Size());

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
     * @return handle [type:number] a handle to the request
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
            DM_MUTEX_SCOPED_LOCK(g_SysModule.m_LoadRequestsMutex);

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
            request->m_Status       = REQUEST_STATUS_PENDING;
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
     * @constant
     */

    /*# an asyncronous request is unable to read the resource
     * @name sys.REQUEST_STATUS_ERROR_IO_ERROR
     * @constant
     */

    /*# an asyncronous request is unable to locate the resource
     * @name sys.REQUEST_STATUS_ERROR_NOT_FOUND
     * @constant
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
        g_SysModule.m_JobThread         = context.m_JobThread;

        if (g_SysModule.m_LoadRequestsMutex == 0)
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
                if (request && (request->m_Status == REQUEST_STATUS_FINISHED        ||
                                request->m_Status == REQUEST_STATUS_ERROR_NOT_FOUND ||
                                request->m_Status == REQUEST_STATUS_ERROR_IO_ERROR))
                {
                    result &= HandleRequestCompleted(request);
                }
                // Otherwise, result is REQUEST_STATUS_PENDING
                // All jobs should always complete eventually, regardless of status
            }
            dmMutex::Unlock(g_SysModule.m_LoadRequestsMutex);
        }

        g_SysModule.m_LastUpdateResult = result;
    }

    void ScriptSysGameSysFinalize(const ScriptLibContext& context)
    {
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
        g_SysModule.m_LoadRequestsMutex = 0;
    }

    // For tests
    bool GetScriptSysGameSysLastUpdateResult()
    {
        return g_SysModule.m_LastUpdateResult;
    }
}
