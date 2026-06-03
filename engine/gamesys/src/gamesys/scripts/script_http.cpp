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

#include <float.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <dmsdk/dlib/configfile.h>
#include <dmsdk/gameobject/script.h>

#include <ddf/ddf.h>
#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/http/http_cache.h>
#include <dlib/http/http_internal.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/uri.h>

#include <script/script.h>
#include <script/http_ddf.h>

#include <extension/extension.hpp>

#include "script_http.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

#include "script_http_util.h"

namespace dmGameSystem
{
    /*# HTTP API documentation
     *
     * Functions for performing HTTP and HTTPS requests.
     *
     * @document
     * @name HTTP
     * @namespace http
     * @language Lua
     */

    static HttpService* g_Service = 0;
    static uint64_t     g_Timeout = 0;
    static const char*  SCRIPT_HTTP_SERVICE_CONTEXT_NAME = HTTP_SERVICE_CONTEXT_NAME;

    struct ScriptHttpRequest
    {
        ScriptHttpRequest()
            : m_Callback(0)
            , m_HttpRequest(0)
            , m_Handle(HTTP_REQUEST_HANDLE_INVALID)
        {
        }

        dmMessage::URL    m_Sender;
        uintptr_t         m_Callback;
        HttpRequest*      m_HttpRequest;
        HttpRequestHandle m_Handle;
        dmArray<char>     m_ResponseHeaders;
        dmArray<char>     m_Response;
    };

    static char* DuplicateString(const char* value)
    {
        return value ? strdup(value) : 0;
    }

    static void DestroyScriptHttpRequest(ScriptHttpRequest* request)
    {
        if (request)
        {
            if (request->m_HttpRequest)
            {
                HttpDeleteRequest(request->m_HttpRequest);
            }
            delete request;
        }
    }

    static void MessageDestroyCallback(dmMessage::Message* message)
    {
        dmHttpDDF::HttpResponse* response = (dmHttpDDF::HttpResponse*)message->m_Data;
        free((void*)response->m_Headers);
        free((void*)response->m_Response);
        free((void*)response->m_Path);
        free((void*)response->m_Url);
    }

    static void AppendBytes(dmArray<char>& buffer, const void* data, uint32_t data_size)
    {
        if (!data || data_size == 0)
        {
            return;
        }

        uint32_t left = buffer.Capacity() - buffer.Size();
        if (left < data_size)
        {
            buffer.OffsetCapacity((int32_t)dmMath::Max(data_size - left, 1024U));
        }
        buffer.PushArray((const char*)data, data_size);
    }

    static bool CopyBuffer(const dmArray<char>& source, uint64_t* target)
    {
        *target = 0;
        if (source.Size() == 0)
        {
            return true;
        }

        void* copy = malloc(source.Size());
        if (!copy)
        {
            return false;
        }

        memcpy(copy, source.Begin(), source.Size());
        *target = (uint64_t)copy;
        return true;
    }

    static void FreeHttpResponse(dmHttpDDF::HttpResponse* response)
    {
        free((void*)response->m_Headers);
        free((void*)response->m_Response);
        free((void*)response->m_Path);
        free((void*)response->m_Url);
    }

    static void PostResponse(ScriptHttpRequest* request, const HttpResponseInfo* response)
    {
        dmHttpDDF::HttpResponse resp;
        memset(&resp, 0, sizeof(resp));
        resp.m_Status = response->m_StatusCode;
        resp.m_HeadersLength = request->m_ResponseHeaders.Size();
        resp.m_ResponseLength = request->m_Response.Size();
        resp.m_RangeStart = response->m_RangeStart;
        resp.m_RangeEnd = response->m_RangeEnd;
        resp.m_DocumentSize = response->m_DocumentSize;

        if (!CopyBuffer(request->m_ResponseHeaders, &resp.m_Headers))
        {
            dmLogWarning("Failed to allocate http-response headers.");
            return;
        }

        if (!CopyBuffer(request->m_Response, &resp.m_Response))
        {
            free((void*)resp.m_Headers);
            dmLogWarning("Failed to allocate http-response body.");
            return;
        }

        resp.m_Path = DuplicateString(response->m_Path);
        resp.m_Url = DuplicateString(response->m_Url);
        if ((response->m_Path && !resp.m_Path) || (response->m_Url && !resp.m_Url))
        {
            FreeHttpResponse(&resp);
            dmLogWarning("Failed to allocate http-response strings.");
            return;
        }

        if (dmMessage::RESULT_OK != dmMessage::Post(0, &request->m_Sender, dmHttpDDF::HttpResponse::m_DDFHash, 0, request->m_Callback, (uintptr_t)dmHttpDDF::HttpResponse::m_DDFDescriptor, &resp, sizeof(resp), MessageDestroyCallback))
        {
            FreeHttpResponse(&resp);
            dmLogWarning("Failed to return http-response. Requester deleted?");
        }
    }

    static void PostProgress(ScriptHttpRequest* request, const HttpResponseInfo* response)
    {
        dmHttpDDF::HttpRequestProgress msg = {};
        msg.m_BytesSent = response->m_BytesSent;
        msg.m_BytesReceived = response->m_BytesReceived;
        msg.m_BytesTotal = response->m_BytesTotal;
        msg.m_Url = response->m_Url;

        if (dmGameObject::RESULT_OK != dmGameObject::PostDDF(&msg, 0, &request->m_Sender, request->m_Callback, false))
        {
            dmLogWarning("Failed to return http-progress. Requester deleted?");
        }
    }

    static HttpCallbackResult ResponseCallback(HttpRequest* http_request, void* user_data, const HttpResponseInfo* response)
    {
        ScriptHttpRequest* request = (ScriptHttpRequest*)user_data;
        (void)http_request;

        switch (response->m_Event)
        {
            case HTTP_RESPONSE_EVENT_HEADER:
            {
                AppendBytes(request->m_ResponseHeaders, response->m_Header, response->m_HeaderSize);
                char newline = '\n';
                AppendBytes(request->m_ResponseHeaders, &newline, 1);
            }
            break;

            case HTTP_RESPONSE_EVENT_DATA:
                AppendBytes(request->m_Response, response->m_Data, response->m_DataSize);
                break;

            case HTTP_RESPONSE_EVENT_PROGRESS:
                PostProgress(request, response);
                break;

            case HTTP_RESPONSE_EVENT_COMPLETE:
                PostResponse(request, response);
                request->m_HttpRequest = 0;
                DestroyScriptHttpRequest(request);
                break;
        }

        return HTTP_CALLBACK_RESULT_CONTINUE;
    }

    static ScriptHttpRequest* NewScriptHttpRequest()
    {
        ScriptHttpRequest* request = new ScriptHttpRequest;
        if (!request)
        {
            return 0;
        }

        if (HttpNewRequest(&request->m_HttpRequest) != HTTP_RESULT_OK)
        {
            delete request;
            return 0;
        }

        if (HttpSetResponseCallback(request->m_HttpRequest, ResponseCallback, request) != HTTP_RESULT_OK)
        {
            DestroyScriptHttpRequest(request);
            return 0;
        }
        return request;
    }

    /*# perform a HTTP/HTTPS request
     * Perform a HTTP/HTTPS request.
     *
     * [icon:attention] If no timeout value is passed, the configuration value "network.http_timeout" is used. If that is not set, the timeout value is `0` (which blocks indefinitely).
     *
     * @name http.request
     * @param url [type:string] target url
     * @param method [type:string] HTTP/HTTPS method, e.g. "GET", "PUT", "POST" etc.
     * @param callback [type:function(self, id, response)] response callback function
     *
     * `self`
     * : [type:object] The script instance
     *
     * `id`
     * : [type:hash] Internal message identifier. Do not use!
     *
     * `response`
     * : [type:table] The response data. Contains the fields:
     *
     * - [type:number] `status`: the status of the response
     * - [type:string] `response`: the response data (if not saved on disc)
     * - [type:table] `headers`: all the returned headers (if status is 200 or 206)
     * - [type:string] `path`: the stored path (if saved to disc)
     * - [type:string] `error`: if any unforeseen errors occurred (e.g. file I/O)
     * - [type:number] `bytes_received`: the amount of bytes received/sent for a request, only if option `report_progress` is true
     * - [type:number] `bytes_total`: the total amount of bytes for a request, only if option `report_progress` is true
     * - [type:number] `range_start`: the start offset into the requested file
     * - [type:number] `range_end`: the end offset into the requested file (inclusive)
     * - [type:number] `document_size`: the full size of the requested file
     *
     * @param [headers] [type:table] optional table with custom headers
     * @param [post_data] [type:string] optional data to send
     * @param [options] [type:table] optional table with request parameters. Supported entries:
     *
     * - [type:number] `timeout`: timeout in seconds
     * - [type:string] `path`: path on disc where to download the file. Only overwrites the path if status is 200. [icon:attention] Path should be absolute
     * - [type:boolean] `ignore_cache`: don't return cached data if we get a 304. [icon:attention] Not available in HTML5 build
     * - [type:boolean] `chunked_transfer`: use chunked transfer encoding for https requests larger than 16kb. Defaults to true. [icon:attention] Not available in HTML5 build
     * - [type:boolean] `report_progress`: when it is true, the amount of bytes sent and/or received for a request will be passed into the callback function
     *
     *
     * @examples
     *
     * Basic HTTP-GET request. The callback receives a table with the response
     * in the fields status, the response (the data) and headers (a table).
     *
     * ```lua
     * local function http_result(self, _, response)
     *     if response.bytes_total ~= nil then
     *         update_my_progress_bar(self, response.bytes_received / response.bytes_total)
     *     else
     *         print(response.status)
     *         print(response.response)
     *         pprint(response.headers)
     *     end
     * end
     *
     * function init(self)
     *     http.request("http://www.google.com", "GET", http_result, nil, nil, { report_progress = true })
     * end
     * ```
     */
    static int Http_Request(lua_State* L)
    {
        int            top = lua_gettop(L);

        dmMessage::URL sender;
        if (dmScript::GetURL(L, &sender))
        {
            const char*    url = luaL_checkstring(L, 1);
            const uint32_t max_url_len = dmURI::MAX_URI_LEN;
            const uint32_t url_len = (uint32_t)strlen(url);
            if (url_len > max_url_len)
            {
                assert(top == lua_gettop(L));
                return luaL_error(L, "http.request does not support URIs longer than %d characters.", max_url_len);
            }

            const char*    method = luaL_checkstring(L, 2);
            const uint32_t max_method_len = 16;
            const uint32_t method_len = (uint32_t)strlen(method);
            if (method_len > max_method_len)
            {
                assert(top == lua_gettop(L));
                return luaL_error(L, "http.request does not support request methods longer than %d characters.", max_method_len);
            }

            // The callback is called from CompScriptOnMessage in comp_script.cpp
            luaL_checktype(L, 3, LUA_TFUNCTION);
            lua_pushvalue(L, 3);
            // NOTE: By convention m_FunctionRef is offset by LUA_NOREF, in order to have 0 for "no function"
            int   callback = dmScript::RefInInstance(L) - LUA_NOREF;

            char* headers = 0;
            int   headers_length = 0;
            if (top > 3 && !lua_isnil(L, 4))
            {
                dmArray<char> h;
                h.SetCapacity(4 * 1024);

                luaL_checktype(L, 4, LUA_TTABLE);
                lua_pushvalue(L, 4);
                lua_pushnil(L);
                while (lua_next(L, -2))
                {
                    const char* attr = lua_tostring(L, -2);
                    const char* val = lua_tostring(L, -1);
                    if (attr && val)
                    {
                        uint32_t left = h.Capacity() - h.Size();
                        uint32_t required = strlen(attr) + strlen(val) + 2;
                        if (left < required)
                        {
                            h.OffsetCapacity(dmMath::Max(required, 1024U));
                        }
                        h.PushArray(attr, strlen(attr));
                        h.Push(':');
                        h.PushArray(val, strlen(val));
                        h.Push('\n');
                    }
                    else
                    {
                        // luaL_error would be nice but that would evade call to 'h' destructor
                        dmLogWarning("Ignoring non-string data passed as http request header data");
                    }
                    lua_pop(L, 1);
                }
                lua_pop(L, 1);

                headers = (char*)malloc(h.Size() + 1);
                if (!headers)
                {
                    return luaL_error(L, "Failed to create HTTP request headers");
                }
                memcpy(headers, h.Begin(), h.Size());
                headers[h.Size()] = '\0';
                headers_length = h.Size();
            }

            const char* request_data = 0;
            size_t      request_data_length = 0;
            if (top > 4 && !lua_isnil(L, 5))
            {
                size_t len;
                luaL_checktype(L, 5, LUA_TSTRING);
                request_data = luaL_checklstring(L, 5, &len);
                request_data_length = len;
                if (request_data_length > 0xffffffffU)
                {
                    free(headers);
                    return luaL_error(L, "http.request does not support request data larger than 4 GiB.");
                }
            }

            uint64_t    timeout = g_Timeout;
            const char* path = 0;
            const char* proxy = 0;
            bool        ignore_cache = false;
            bool        chunked_transfer = true;
            bool        report_progress = false;
            if (top > 5 && !lua_isnil(L, 6))
            {
                luaL_checktype(L, 6, LUA_TTABLE);
                lua_pushvalue(L, 6);
                lua_pushnil(L);
                while (lua_next(L, -2))
                {
                    const char* attr = lua_tostring(L, -2);
                    if (strcmp(attr, "timeout") == 0)
                    {
                        timeout = luaL_checknumber(L, -1) * 1000000.0f;
                    }
                    else if (strcmp(attr, "path") == 0)
                    {
                        path = luaL_checkstring(L, -1);
                    }
                    else if (strcmp(attr, "ignore_cache") == 0)
                    {
                        ignore_cache = lua_toboolean(L, -1);
                    }
                    else if (strcmp(attr, "chunked_transfer") == 0)
                    {
                        chunked_transfer = lua_toboolean(L, -1);
                    }
                    else if (strcmp(attr, "report_progress") == 0)
                    {
                        report_progress = lua_toboolean(L, -1);
                    }
                    else if (strcmp(attr, "proxy") == 0)
                    {
                        proxy = luaL_checkstring(L, -1);
                    }

                    lua_pop(L, 1);
                }
                lua_pop(L, 1);
            }

            ScriptHttpRequest* request = NewScriptHttpRequest();
            if (!request)
            {
                free(headers);
                return luaL_error(L, "Failed to create HTTP request");
            }

            request->m_Sender = sender;
            request->m_Callback = (uintptr_t)callback;

            HttpResult r = HttpSetMethod(request->m_HttpRequest, method);
            if (r == HTTP_RESULT_OK)
            {
                r = HttpSetURL(request->m_HttpRequest, url);
            }
            if (r == HTTP_RESULT_OK && headers && headers_length > 0)
            {
                char* s;
                char* last;
                s = dmStrTok(headers, "\n", &last);
                while (s && r == HTTP_RESULT_OK)
                {
                    r = HttpAddHeader(request->m_HttpRequest, s);
                    s = dmStrTok(0, "\n", &last);
                }
            }
            free(headers);
            headers = 0;

            if (r == HTTP_RESULT_OK && request_data_length > 0)
            {
                r = HttpSetRequestBody(request->m_HttpRequest, request_data, (uint32_t)request_data_length);
            }
            if (r == HTTP_RESULT_OK)
            {
                r = HttpSetTimeout(request->m_HttpRequest, (uint32_t)timeout);
            }
            if (r == HTTP_RESULT_OK && path)
            {
                r = HttpSetResponsePath(request->m_HttpRequest, path);
            }
            if (r == HTTP_RESULT_OK && proxy)
            {
                r = HttpSetProxy(request->m_HttpRequest, proxy);
            }
            if (r == HTTP_RESULT_OK)
            {
                r = HttpSetIgnoreCache(request->m_HttpRequest, ignore_cache);
            }
            if (r == HTTP_RESULT_OK)
            {
                r = HttpSetChunkedTransfer(request->m_HttpRequest, chunked_transfer);
            }
            if (r == HTTP_RESULT_OK)
            {
                r = HttpSetReportProgress(request->m_HttpRequest, report_progress);
            }
            if (r == HTTP_RESULT_OK)
            {
                HttpRequest* http_request = request->m_HttpRequest;
                request->m_HttpRequest = 0;
                r = HttpPushRequest(g_Service, http_request, &request->m_Handle);
                if (r != HTTP_RESULT_OK)
                {
                    HttpDeleteRequest(http_request);
                }
            }
            if (r != HTTP_RESULT_OK)
            {
                DestroyScriptHttpRequest(request);
                dmLogError("Failed to create HTTP request");
            }
            assert(top == lua_gettop(L));
            return 0;
        }
        else
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "http.request is not available from this script-type.");
        }
        return 0;
    }

    static const luaL_reg HTTP_COMP_FUNCTIONS[] = {
        { "request", Http_Request },
        { 0, 0 }
    };

    // Used for unit test
    void SetHttpRequestTimeout(uint64_t timeout)
    {
        g_Timeout = timeout;
    }

    static dmExtension::Result ScriptHttpAppInitialize(dmExtension::AppParams* params)
    {
        HContextRegistry context_registry = ExtensionAppParamsGetContextRegistry((ExtensionAppParams*)params);

        dmConfigFile::HConfig config_file = (dmConfigFile::HConfig)ContextRegistryGet(context_registry, CONFIGFILE_CONTEXT_NAME);
        assert(config_file != 0);
        assert(g_Service == 0);

        uint32_t thread_count = 4;

        if (config_file)
        {
            thread_count = dmConfigFile::GetInt(config_file, "network.http_thread_count", thread_count);
        }

        HttpResult http_result = HttpNewServiceWithCacheInternal(thread_count, ContextRegistryGet(context_registry, "http_cache"), &g_Service);
        if (http_result != HTTP_RESULT_OK || g_Service == 0)
        {
            return dmExtension::RESULT_INIT_ERROR;
        }

        dmScript::RegisterDDFDecoder(dmHttpDDF::HttpResponse::m_DDFDescriptor, &HttpResponseDecoder);
        dmScript::RegisterDDFDecoder(dmHttpDDF::HttpRequestProgress::m_DDFDescriptor, &HttpRequestProgressDecoder);

        ContextRegistrySet(context_registry, SCRIPT_HTTP_SERVICE_CONTEXT_NAME, g_Service);

        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result ScriptHttpInitialize(dmExtension::Params* params)
    {
        HContextRegistry context_registry = ExtensionParamsGetContextRegistry((ExtensionParams*)params);

        lua_State*       L = (lua_State*)ContextRegistryGet(context_registry, LUA_CONTEXT_NAME);
        assert(L != 0);

        dmConfigFile::HConfig config_file = (dmConfigFile::HConfig)ContextRegistryGet(context_registry, CONFIGFILE_CONTEXT_NAME);
        assert(config_file != 0);

        int top = lua_gettop(L);

        if (config_file)
        {
            float timeout = dmConfigFile::GetFloat(config_file, "network.http_timeout", 0.0f);
            g_Timeout = (uint64_t)(timeout * 1000000.0f);
        }

        luaL_register(L, "http", HTTP_COMP_FUNCTIONS);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));

        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result ScriptHttpFinalize(dmExtension::Params* params)
    {
        (void)params;
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result ScriptHttpAppFinalize(dmExtension::AppParams* params)
    {
        HContextRegistry context_registry = ExtensionAppParamsGetContextRegistry((ExtensionAppParams*)params);
        ContextRegistrySet(context_registry, SCRIPT_HTTP_SERVICE_CONTEXT_NAME, 0);

        if (g_Service != 0)
        {
            HttpDeleteServiceInternal(g_Service);
            g_Service = 0;
        }
        return dmExtension::RESULT_OK;
    }

    DM_DECLARE_EXTENSION(ScriptHttp, "ScriptHttp", ScriptHttpAppInitialize, ScriptHttpAppFinalize, ScriptHttpInitialize, 0, 0, ScriptHttpFinalize);
} // namespace dmGameSystem
