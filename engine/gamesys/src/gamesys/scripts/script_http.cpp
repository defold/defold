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

#include <float.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>

#include <dmsdk/gameobject/script.h>

#include <ddf/ddf.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/http_cache.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/uri.h>

#include <script/script.h>
#include <script/http_ddf.h>
#include <script/http_service.h>

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

    static dmHttpService::HHttpService g_Service = 0;
    static uint64_t g_Timeout                    = 0;

    static void ReportProgressCallback(dmHttpDDF::HttpRequestProgress* msg, dmMessage::URL* url, uintptr_t user_data)
    {
        if (dmGameObject::RESULT_OK != dmGameObject::PostDDF(msg, 0, url, user_data, false))
        {
            dmLogWarning("Failed to return http-progress. Requester deleted?");
        }
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
        int top = lua_gettop(L);

        dmMessage::URL sender;
        if (dmScript::GetURL(L, &sender)) {

            const char* url = luaL_checkstring(L, 1);
            const uint32_t max_url_len = dmURI::MAX_URI_LEN;
            const uint32_t url_len = (uint32_t)strlen(url);
            if (url_len > max_url_len)
            {
                assert(top == lua_gettop(L));
                return luaL_error(L, "http.request does not support URIs longer than %d characters.", max_url_len);
            }

            const char* method = luaL_checkstring(L, 2);
            const uint32_t max_method_len = 16;
            const uint32_t method_len = (uint32_t)strlen(method);
            if (method_len > max_method_len) {
                assert(top == lua_gettop(L));
                return luaL_error(L, "http.request does not support request methods longer than %d characters.", max_method_len);
            }

            // The callback is called from CompScriptOnMessage in comp_script.cpp
            luaL_checktype(L, 3, LUA_TFUNCTION);
            lua_pushvalue(L, 3);
            // NOTE: By convention m_FunctionRef is offset by LUA_NOREF, in order to have 0 for "no function"
            int callback = dmScript::RefInInstance(L) - LUA_NOREF;

            char* headers = 0;
            int headers_length = 0;
            if (top > 3 && !lua_isnil(L, 4)) {
                dmArray<char> h;
                h.SetCapacity(4 * 1024);

                luaL_checktype(L, 4, LUA_TTABLE);
                lua_pushvalue(L, 4);
                lua_pushnil(L);
                while (lua_next(L, -2)) {
                    const char* attr = lua_tostring(L, -2);
                    const char* val = lua_tostring(L, -1);
                    if (attr && val) {
                        uint32_t left = h.Capacity() - h.Size();
                        uint32_t required = strlen(attr) + strlen(val) + 2;
                        if (left < required) {
                            h.OffsetCapacity(dmMath::Max(required, 1024U));
                        }
                        h.PushArray(attr, strlen(attr));
                        h.Push(':');
                        h.PushArray(val, strlen(val));
                        h.Push('\n');
                    } else {
                        // luaL_error would be nice but that would evade call to 'h' destructor
                        dmLogWarning("Ignoring non-string data passed as http request header data");
                    }
                    lua_pop(L, 1);
                }
                lua_pop(L, 1);

                headers = (char*) malloc(h.Size());
                memcpy(headers, h.Begin(), h.Size());
                headers_length = h.Size();
            }

            char* request_data = 0;
            int request_data_length = 0;
            if (top > 4 && !lua_isnil(L, 5)) {
                size_t len;
                luaL_checktype(L, 5, LUA_TSTRING);
                const char* r = luaL_checklstring(L, 5, &len);
                request_data = (char*) malloc(len);
                memcpy(request_data, r, len);
                request_data_length = len;
            }

            uint64_t timeout = g_Timeout;
            const char* path = 0;
            const char* proxy = 0;
            bool ignore_cache = false;
            bool chunked_transfer = true;
            bool report_progress = false;
            if (top > 5 && !lua_isnil(L, 6)) {
                luaL_checktype(L, 6, LUA_TTABLE);
                lua_pushvalue(L, 6);
                lua_pushnil(L);
                while (lua_next(L, -2)) {
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

            // ddf + max method and url string lengths incl. null character
            char buf[sizeof(dmHttpDDF::HttpRequest) + max_method_len + 1 + max_url_len + 1];
            char* string_buf = buf + sizeof(dmHttpDDF::HttpRequest);
            dmStrlCpy(string_buf, method, method_len + 1);
            dmStrlCpy(string_buf + method_len + 1, url, url_len + 1);

            dmHttpDDF::HttpRequest* request = (dmHttpDDF::HttpRequest*) buf;
            request->m_Method = (const char*) (sizeof(*request));
            request->m_Url = (const char*) (sizeof(*request) + method_len + 1);
            request->m_Headers = (uint64_t) headers;
            request->m_HeadersLength = headers_length;
            request->m_Request = (uint64_t) request_data;
            request->m_RequestLength = request_data_length;
            request->m_Timeout = timeout;
            request->m_Path = path;
            request->m_IgnoreCache = ignore_cache;
            request->m_ChunkedTransfer = chunked_transfer;
            request->m_ReportProgress = report_progress;
            request->m_Proxy = proxy;

            uint32_t post_len = sizeof(dmHttpDDF::HttpRequest) + method_len + 1 + url_len + 1;
            dmMessage::URL receiver;
            dmMessage::ResetURL(&receiver);
            receiver.m_Socket = dmHttpService::GetSocket(g_Service);

            dmMessage::Result r = dmMessage::Post(&sender, &receiver, dmHttpDDF::HttpRequest::m_DDFHash, 0, (uintptr_t)callback, (uintptr_t) dmHttpDDF::HttpRequest::m_DDFDescriptor, buf, post_len, 0);
            if (r != dmMessage::RESULT_OK) {
                dmLogError("Failed to create HTTP request");
            }
            assert(top == lua_gettop(L));
            return 0;
        } else {
            assert(top == lua_gettop(L));
            return luaL_error(L, "http.request is not available from this script-type.");
        }
        return 0;
    }

    static const luaL_reg HTTP_COMP_FUNCTIONS[] =
    {
        {"request", Http_Request},
        {0, 0}
    };

    // Used for unit test
    void SetHttpRequestTimeout(uint64_t timeout)
    {
        g_Timeout = timeout;
    }

    static dmExtension::Result ScriptHttpInitialize(dmExtension::Params* params)
    {
        lua_State* L = dmExtension::GetContextAsType<lua_State*>(params, "lua");
        assert(L != 0);

        dmConfigFile::HConfig config_file = dmExtension::GetContextAsType<dmConfigFile::HConfig>(params, "config");
        assert(config_file != 0);

        int top = lua_gettop(L);

        if (g_Service == 0)
        {
            dmHttpService::Params service_params;
            service_params.m_ReportProgressCallback = ReportProgressCallback;

            if (config_file)
            {
                service_params.m_ThreadCount = dmConfigFile::GetInt(config_file, "network.http_thread_count", service_params.m_ThreadCount);
            }

            service_params.m_HttpCache = dmExtension::GetContextAsType<dmHttpCache::HCache>(params, "http_cache");

            g_Service = dmHttpService::New(&service_params);
            dmScript::RegisterDDFDecoder(dmHttpDDF::HttpResponse::m_DDFDescriptor, &HttpResponseDecoder);
            dmScript::RegisterDDFDecoder(dmHttpDDF::HttpRequestProgress::m_DDFDescriptor, &HttpRequestProgressDecoder);
        }

        if (config_file)
        {
            float timeout = dmConfigFile::GetFloat(config_file, "network.http_timeout", 0.0f);
            g_Timeout = (uint64_t) (timeout * 1000000.0f);
        }

        luaL_register(L, "http", HTTP_COMP_FUNCTIONS);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));

        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result ScriptHttpFinalize(dmExtension::Params* params)
    {
        if (g_Service != 0)
        {
            dmHttpService::Delete(g_Service);
            g_Service = 0;
        }
        return dmExtension::RESULT_OK;
    }

    DM_DECLARE_EXTENSION(ScriptHttp, "ScriptHttp", 0, 0, ScriptHttpInitialize, 0, 0, ScriptHttpFinalize);
}
