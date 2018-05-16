#include <float.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>

#include <ddf/ddf.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/http_cache.h>

#include "script.h"
#include "http_ddf.h"

#include "script_http.h"
#include "http_service.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

#include "script_http_util.h"

namespace dmScript
{
    /*# HTTP API documentation
     *
     * Functions for performing HTTP and HTTPS requests.
     *
     * @document
     * @name HTTP
     * @namespace http
     */

    dmHttpService::HHttpService g_Service = 0;
    int g_ServiceRefCount = 0;
    uint64_t g_Timeout = 0;

    /*# perform a HTTP/HTTPS request
     * Perform a HTTP/HTTPS request.
     *
     * [icon:attention] If no timeout value is passed, the configuration value "network.http_timeout" is used. If that is not set, the timeout value is `0` (which blocks indefinitely).
     *
     * @name http.request
     * @replaces facebook.me
     * @param url [type:string] target url
     * @param method [type:string] HTTP/HTTPS method, e.g. "GET", "PUT", "POST" etc.
     * @param callback [type:function(self, id, response)] response callback function
     *
     * `self`
     * : [type:object] The current object
     *
     * `id`
     * : [type:hash] Internal message identifier. Do not use!
     *
     * `response`
     * : [type:table] The response data. Contains the fields:
     *
     * - [type:number] `status`: the status of the response
     * - [type:string] `response`: the response data
     * - [type:table] `headers`: all the returned headers
     *
     * @param [headers] [type:table] optional table with custom headers
     * @param [post_data] [type:string] optional data to send
     * @param [options] [type:table] optional table with request parameters. Supported entries:
     *
     * - [type:number] `timeout`: timeout in seconds
     *
     * @examples
     *
     * Basic HTTP-GET request. The callback receives a table with the response
     * in the fields status, the response (the data) and headers (a table).
     *
     * ```lua
     * local function http_result(self, _, response)
     *     print(response.status)
     *     print(response.response)
     *     pprint(response.headers)
     * end
     *
     * function init(self)
     *     http.request("http://www.google.com", "GET", http_result)
     * end
     * ```
     */
    int Http_Request(lua_State* L)
    {
        int top = lua_gettop(L);

        dmMessage::URL sender;
        if (dmScript::GetURL(L, &sender)) {

            const char* url = luaL_checkstring(L, 1);
            uint32_t url_len = strlen(url);

            if (url_len > dmHttpCache::MAX_URI_LEN)
            {
                assert(top == lua_gettop(L));
                return luaL_error(L, "http.request does not support URI longer than %d characters.", dmHttpCache::MAX_URI_LEN);
            }
            const char* method = luaL_checkstring(L, 2);
            luaL_checktype(L, 3, LUA_TFUNCTION);
            lua_pushvalue(L, 3);
            // NOTE: By convention m_FunctionRef is offset by LUA_NOREF, see message.h in dlib
            int callback = dmScript::Ref(L, LUA_REGISTRYINDEX) - LUA_NOREF;
            sender.m_FunctionRef = callback;

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
            if (top > 5 && !lua_isnil(L, 6)) {
                luaL_checktype(L, 6, LUA_TTABLE);
                lua_pushvalue(L, 6);
                lua_pushnil(L);
                while (lua_next(L, -2)) {
                    const char* attr = lua_tostring(L, -2);
                    if( strcmp(attr, "timeout") == 0 )
                    {
                        timeout = luaL_checknumber(L, -1) * 1000000.0f;
                    }
                    lua_pop(L, 1);
                }
                lua_pop(L, 1);
            }

            // Really arbitrary length.
            // TODO: Warn if the buffer isn't long enough
            const uint32_t string_buf_len = 1024;
            const uint32_t max_method_len = 16;
            char buf[sizeof(dmHttpDDF::HttpRequest) + string_buf_len];
            dmHttpDDF::HttpRequest* request = (dmHttpDDF::HttpRequest*) buf;
            char* string_buf = buf + sizeof(dmHttpDDF::HttpRequest);
            request->m_Method = (const char*) (sizeof(*request));
            dmStrlCpy(string_buf, method, max_method_len);
            request->m_Url = (const char*) (sizeof(*request) + max_method_len);
            dmStrlCpy(string_buf + max_method_len, url, string_buf_len - max_method_len);

            request->m_Headers = (uint64_t) headers;
            request->m_HeadersLength = headers_length;
            request->m_Request = (uint64_t) request_data;
            request->m_RequestLength = request_data_length;
            request->m_Timeout = timeout;

            uint32_t post_len = sizeof(dmHttpDDF::HttpRequest) + max_method_len + url_len + 1;
            dmMessage::URL receiver;
            dmMessage::ResetURL(receiver);
            receiver.m_Socket = dmHttpService::GetSocket(g_Service);

            dmMessage::Result r = dmMessage::Post(&sender, &receiver, dmHttpDDF::HttpRequest::m_DDFHash, 0, (uintptr_t) dmHttpDDF::HttpRequest::m_DDFDescriptor, buf, post_len, 0);
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

    void SetHttpRequestTimeout(uint64_t timeout)
    {
        g_Timeout = timeout;
    }

    void InitializeHttp(lua_State* L, dmConfigFile::HConfig config_file)
    {
// TODO: Port
#if !defined(__AVM2__)
        int top = lua_gettop(L);

        if (g_Service == 0) {
            g_Service = dmHttpService::New();
            dmScript::RegisterDDFDecoder(dmHttpDDF::HttpResponse::m_DDFDescriptor, &HttpResponseDecoder);
        }
        g_ServiceRefCount++;

        if (config_file) {
            float timeout = dmConfigFile::GetFloat(config_file, "network.http_timeout", 0.0f);
            g_Timeout = (uint64_t) (timeout * 1000000.0f);
        }

        luaL_register(L, "http", HTTP_COMP_FUNCTIONS);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));
#endif
    }

    void FinalizeHttp(lua_State* L)
    {
#if !defined(__AVM2__)
        assert(g_ServiceRefCount > 0);
        g_ServiceRefCount--;
        if (g_ServiceRefCount == 0) {
            dmHttpService::Delete(g_Service);
            g_Service = 0;
        }
#endif
    }
}
