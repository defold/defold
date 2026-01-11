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
#include <string.h>

#include <emscripten.h>

#include <dmsdk/gameobject/script.h>
#include <ddf/ddf.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include <script/script.h>
#include <script/http_ddf.h>

#include "script_http.h"
#include <script/http_service.h>

#include <extension/extension.hpp>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

#include "script_http_util.h"

namespace dmGameSystem
{
    uint64_t g_Timeout = 0;

    typedef void (*OnLoad)(void* context, int status, void* content, int content_size, const char* headers);
    typedef void (*OnError)(void* context, int status);
    typedef void (*OnProgress)(void* context, uint32_t loaded, uint32_t total);

    struct RequestContext
    {
        dmMessage::URL  m_Requester;
        void*           m_RequestData;
        const char*     m_Path;
        const char*     m_Url;  // The request url
        int             m_Callback;
    };

    struct RequestParams
    {
        const char* m_Method;
        const char* m_Url;
        const char* m_Headers;
        void* m_Args;
        void* m_SendData;
        size_t m_SendDataLength;
        uint64_t m_RequestTimeout;
        bool     m_ReportProgress;
    };

    extern "C" void dmScriptHttpRequestAsync(const char* method, const char* url, const char* headers, void* arg, 
        OnLoad onload, OnError onerror, OnProgress onprogress,
        const void* send_data, int send_data_length, int timeout);

    void HttpRequestAsync(const RequestParams& request_params, OnLoad ol, OnError oe, OnProgress op)
    {
        dmScriptHttpRequestAsync(request_params.m_Method, request_params.m_Url, request_params.m_Headers,
            request_params.m_Args,
            ol, oe, request_params.m_ReportProgress ? op : NULL,
            request_params.m_SendData,
            request_params.m_SendDataLength, request_params.m_RequestTimeout);
    }

    static void MessageDestroyCallback(dmMessage::Message* message)
    {
        dmHttpDDF::HttpResponse* response = (dmHttpDDF::HttpResponse*)message->m_Data;
        free((void*) response->m_Headers);
        free((void*) response->m_Response);
        free((void*) response->m_Path);
        free((void*) response->m_Url);
    }

    static void SendResponse(const RequestContext* ctx, int status,
                             const char* headers, uint32_t headers_length,
                             const char* response, uint32_t response_length)
    {
        dmHttpDDF::HttpResponse resp;
        resp.m_Status = status;
        resp.m_Headers = (uint64_t) headers;
        resp.m_HeadersLength = headers_length;
        resp.m_Response = (uint64_t) response;
        resp.m_ResponseLength = response_length;
        resp.m_Path = ctx->m_Path;
        resp.m_Url = ctx->m_Url;

        resp.m_Headers = (uint64_t) malloc(headers_length);
        memcpy((void*) resp.m_Headers, headers, headers_length);
        resp.m_Response = (uint64_t) malloc(response_length);
        memcpy((void*) resp.m_Response, response, response_length);

        if (dmMessage::RESULT_OK != dmMessage::Post(0, &ctx->m_Requester, dmHttpDDF::HttpResponse::m_DDFHash, 0, (uintptr_t)ctx->m_Callback, (uintptr_t) dmHttpDDF::HttpResponse::m_DDFDescriptor, &resp, sizeof(resp), MessageDestroyCallback) )
        {
            free((void*) resp.m_Headers);
            free((void*) resp.m_Response);
            free((void*) resp.m_Path);
            free((void*) resp.m_Url);
            dmLogWarning("Failed to return http-response. Requester deleted?");
        }
    }

    void OnHttpLoad(void* context, int status, void* content, int content_size, const char* headers)
    {
        RequestContext* ctx = (RequestContext*) context;
        SendResponse(ctx, status, headers, strlen(headers), (const char*) content, content_size);
        free(ctx->m_RequestData);
        delete ctx;
    }

    void OnHttpError(void* context, int status)
    {
        RequestContext* ctx = (RequestContext*) context;
        SendResponse(ctx, status, 0, 0, 0, 0);
    }

    void OnHttpProgress(void* context, uint32_t loaded, uint32_t total)
    {
        RequestContext* ctx = (RequestContext*) context;
        dmHttpDDF::HttpRequestProgress progress = {};
        progress.m_BytesReceived                = loaded;
        progress.m_BytesTotal                   = total;
        progress.m_Url                          = ctx->m_Url;
        if (dmGameObject::RESULT_OK != dmGameObject::PostDDF(&progress, 0, &ctx->m_Requester, ctx->m_Callback, false))
        {
            dmLogWarning("Failed to return http-progress. Requester deleted?");
        }
    }

    int Http_Request(lua_State* L)
    {
        int top = lua_gettop(L);

        int callback = 0;
        const char* path = 0;
        dmMessage::URL sender;
        if (dmScript::GetURL(L, &sender)) {
            RequestParams request_params;
            memset(&request_params, 0, sizeof(RequestParams));

            request_params.m_Url = luaL_checkstring(L, 1);
            request_params.m_Method = luaL_checkstring(L, 2);
            luaL_checktype(L, 3, LUA_TFUNCTION);
            lua_pushvalue(L, 3);
            // NOTE: By convention the runctionref is offset by LUA_NOREF
            callback = dmScript::RefInInstance(L) - LUA_NOREF;

            dmArray<char> h;
            h.SetCapacity(4 * 1024);
            if (top > 3 && !lua_isnil(L, 4)) {

                luaL_checktype(L, 4, LUA_TTABLE);
                lua_pushvalue(L, 4);
                lua_pushnil(L);
                while (lua_next(L, -2)) {
                    const char* attr = lua_tostring(L, -2);
                    const char* val = lua_tostring(L, -1);
                    uint32_t left = h.Capacity() - h.Size();
                    uint32_t required = strlen(attr) + strlen(val) + 2;
                    if (left < required) {
                        h.OffsetCapacity(dmMath::Max(required, 1024U));
                    }
                    h.PushArray(attr, strlen(attr));
                    h.Push(':');
                    h.PushArray(val, strlen(val));
                    h.Push('\n');
                    lua_pop(L, 1);
                }
                lua_pop(L, 1);
            }
            h.Push('\0');
            request_params.m_Headers = h.Begin();

            if (top > 4 && !lua_isnil(L, 5)) {
                size_t len;
                luaL_checktype(L, 5, LUA_TSTRING);
                const char* r = luaL_checklstring(L, 5, &len);
                request_params.m_SendData = (char*) malloc(len);
                memcpy(request_params.m_SendData, r, len);
                request_params.m_SendDataLength = len;
            }

            request_params.m_RequestTimeout = g_Timeout;
            if (top > 5 && !lua_isnil(L, 6)) {
                luaL_checktype(L, 6, LUA_TTABLE);
                lua_pushvalue(L, 6);
                lua_pushnil(L);
                while (lua_next(L, -2)) {
                    const char* attr = lua_tostring(L, -2);
                    if(strcmp(attr, "timeout") == 0)
                    {
                        request_params.m_RequestTimeout = luaL_checknumber(L, -1) * 1000000.0f;
                    }
                    else if (strcmp(attr, "report_progress") == 0)
                    {
                        request_params.m_ReportProgress = lua_toboolean(L, -1);
                    }
                    else if (strcmp(attr, "path") == 0)
                    {
                        path = luaL_checkstring(L, -1);
                    }
                    lua_pop(L, 1);
                }
                lua_pop(L, 1);
            }

            RequestContext* ctx = new RequestContext;
            ctx->m_Callback = callback;
            ctx->m_Requester = sender;
            ctx->m_RequestData = request_params.m_SendData;
            ctx->m_Path = path ? strdup(path) : 0;
            ctx->m_Url = request_params.m_Url ? strdup(request_params.m_Url) : 0;

            request_params.m_Args = ctx;

            HttpRequestAsync(request_params, OnHttpLoad, OnHttpError, OnHttpProgress);
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

    static dmExtension::Result ScriptHttpInitialize(dmExtension::Params* params)
    {
        lua_State* L = dmExtension::GetContextAsType<lua_State*>(params, "lua");
        assert(L != 0);

        dmConfigFile::HConfig config_file = dmExtension::GetContextAsType<dmConfigFile::HConfig>(params, "config");
        assert(config_file != 0);

        int top = lua_gettop(L);

        dmScript::RegisterDDFDecoder(dmHttpDDF::HttpResponse::m_DDFDescriptor, &HttpResponseDecoder);
        dmScript::RegisterDDFDecoder(dmHttpDDF::HttpRequestProgress::m_DDFDescriptor, &HttpRequestProgressDecoder);

        if (config_file) {
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
        return dmExtension::RESULT_OK;
    }

    DM_DECLARE_EXTENSION(ScriptHttp, "ScriptHttp", 0, 0, ScriptHttpInitialize, 0, 0, ScriptHttpFinalize);
}
