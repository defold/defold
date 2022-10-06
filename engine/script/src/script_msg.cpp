// Copyright 2020-2022 The Defold Foundation
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

#include "script.h"

#include "script_private.h"

#include <stdint.h>
#include <string.h>

#include <dlib/dlib.h>
#include <dlib/dstrings.h>
#include <dlib/math.h>
#include <dlib/message.h>

#include <ddf/ddf.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript
{
#define SCRIPT_LIB_NAME "msg"
#define SCRIPT_TYPE_NAME_URL "url"

    static uint32_t SCRIPT_URL_TYPE_HASH = 0;


    /*# Messaging API documentation
     *
     * Functions for passing messages and constructing URL objects.
     *
     * @document
     * @name Message
     * @namespace msg
     */

    const uint32_t MAX_MESSAGE_DATA_SIZE = 2048;

    bool IsURL(lua_State *L, int index)
    {
        return (dmMessage::URL*)dmScript::ToUserType(L, index, SCRIPT_URL_TYPE_HASH);
    }

    const char* UrlToString(const dmMessage::URL* url, char* buffer, uint32_t buffer_size)
    {
        char tmp[32];
        *buffer = '\0';

        const char* unknown = "<unknown>";
        const char* socketname = 0;
        if (dmMessage::IsSocketValid(url->m_Socket))
        {
            // Backwards compatibility: If, for some reason, they want the socket name in Release mode, we keep this check
            const char* s = dmMessage::GetSocketName(url->m_Socket);
            socketname = s;
        }

        if( !socketname )
        {
            dmSnPrintf(tmp, sizeof(tmp), "%s", dmHashReverseSafe64(url->m_Socket));
            socketname = tmp;
        }

        dmStrlCpy(buffer, socketname ? socketname : unknown, buffer_size);
        dmStrlCat(buffer, ":", buffer_size);
        if (url->m_Path != 0)
        {
            dmSnPrintf(tmp, sizeof(tmp), "%s", dmHashReverseSafe64(url->m_Path));
            dmStrlCat(buffer, tmp, buffer_size);
        }
        if (url->m_Fragment != 0)
        {
            dmStrlCat(buffer, "#", buffer_size);
            dmSnPrintf(tmp, sizeof(tmp), "%s", dmHashReverseSafe64(url->m_Fragment));
            dmStrlCat(buffer, tmp, buffer_size);
        }
        return buffer;
    }

    static int URL_tostring(lua_State *L)
    {
        dmMessage::URL* url = (dmMessage::URL*)lua_touserdata(L, 1);
        char buffer[64];
        UrlToString(url, buffer, sizeof(buffer));
        lua_pushfstring(L, "%s: [%s]", SCRIPT_TYPE_NAME_URL, buffer);
        return 1;
    }

    static int URL_concat(lua_State *L)
    {
        const char* s = luaL_checkstring(L, 1);
        dmMessage::URL* url = CheckURL(L, 2);
        char buffer[64];
        UrlToString(url, buffer, sizeof(buffer));
        lua_pushfstring(L, "%s[%s]", s, buffer);
        return 1;
    }

    static int URL_index(lua_State *L)
    {
        dmMessage::URL* url = (dmMessage::URL*)lua_touserdata(L, 1);

        const char* key = luaL_checkstring(L, 2);
        if (strcmp("socket", key) == 0)
        {
            if (url->m_Socket != 0)
            {
                PushHash(L, url->m_Socket);
            }
            else
            {
                lua_pushnil(L);
            }
            return 1;
        }
        else if (strcmp("path", key) == 0)
        {
            if (url->m_Path != 0)
            {
                PushHash(L, url->m_Path);
            }
            else
            {
                lua_pushnil(L);
            }
            return 1;
        }
        else if (strcmp("fragment", key) == 0)
        {
            if (url->m_Fragment != 0)
            {
                PushHash(L, url->m_Fragment);
            }
            else
            {
                lua_pushnil(L);
            }
            return 1;
        }
        else
        {
            return luaL_error(L, "%s.%s only has fields socket, path, fragment.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_URL);
        }
    }

    static int URL_newindex(lua_State *L)
    {
        dmMessage::URL* url = (dmMessage::URL*)lua_touserdata(L, 1);

        const char* key = luaL_checkstring(L, 2);
        if (strcmp("socket", key) == 0)
        {
            if (IsHash(L, 3))
            {
                url->m_Socket = *(dmhash_t*)lua_touserdata(L, 3);
            }
            else if (lua_isstring(L, 3))
            {
                const char* socket_name = lua_tostring(L, 3);
                dmMessage::Result result = dmMessage::GetSocket(socket_name, &url->m_Socket);
                if (!(result == dmMessage::RESULT_OK || result == dmMessage::RESULT_NAME_OK_SOCKET_NOT_FOUND))
                {
                    if(result == dmMessage::RESULT_INVALID_SOCKET_NAME)
                    {
                        return luaL_error(L, "The socket '%s' name is invalid.", socket_name);
                    }
                    else
                    {
                        return luaL_error(L, "Error when getting socket '%s': %d.", socket_name, result);
                    }
                }
            }
            else if (lua_isnil(L, 3))
            {
                url->m_Socket = 0;
            }
            else
            {
                return luaL_error(L, "Invalid type for socket, must be number, string or nil.");
            }
        }
        else if (strcmp("path", key) == 0)
        {
            if (lua_isstring(L, 3))
            {
                url->m_Path = dmHashString64(lua_tostring(L, 3));
            }
            else if (lua_isnil(L, 3))
            {
                url->m_Path = 0;
            }
            else if (IsHash(L, 3))
            {
                url->m_Path = CheckHash(L, 3);
            }
            else
            {
                return luaL_error(L, "Invalid type for path, must be hash, string or nil.");
            }
        }
        else if (strcmp("fragment", key) == 0)
        {
            if (lua_isstring(L, 3))
            {
                url->m_Fragment = dmHashString64(lua_tostring(L, 3));
            }
            else if (lua_isnil(L, 3))
            {
                url->m_Fragment = 0;
            }
            else if (IsHash(L, 3))
            {
                url->m_Fragment = *(dmhash_t*)lua_touserdata(L, 3);
            }
            else
            {
                return luaL_error(L, "Invalid type for fragment, must be hash, string or nil.");
            }
        }
        else
        {
            return luaL_error(L, "%s.%s only has fields socket, path, fragment.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_URL);
        }
        return 0;
    }

    static int URL_eq(lua_State *L)
    {
        dmMessage::URL* url1 = (dmMessage::URL*)ToUserType(L, 1, SCRIPT_URL_TYPE_HASH);
        dmMessage::URL* url2 = (dmMessage::URL*)ToUserType(L, 2, SCRIPT_URL_TYPE_HASH);
        lua_pushboolean(L, url1 && url2 && url1->m_Socket == url2->m_Socket && url1->m_Path == url2->m_Path && url1->m_Fragment == url2->m_Fragment);
        return 1;
    }

    static const luaL_reg URL_methods[] =
    {
        {0,0}
    };

    static const luaL_reg URL_meta[] =
    {
        {"__tostring",  URL_tostring},
        {"__concat",    URL_concat},
        {"__index",     URL_index},
        {"__newindex",  URL_newindex},
        {"__eq",        URL_eq},
        {0,0}
    };

    /*# creates a new URL
     * This is equivalent to `msg.url(nil)` or `msg.url("#")`, which creates an url to the current
     * script component.
     *
     * @name msg.url
     * @return url [type:url] a new URL
     * @examples
     *
     * Create a new URL which will address the current script:
     *
     * ```lua
     * local my_url = msg.url()
     * print(my_url) --> url: [current_collection:/my_instance#my_component]
     * ```
     */

    /*# creates a new URL from a string
     * The format of the string must be `[socket:][path][#fragment]`, which is similar to a HTTP URL.
     * When addressing instances:
     *
     * - `socket` is the name of a valid world (a collection)
     * - `path` is the id of the instance, which can either be relative the instance of the calling script or global
     * - `fragment` would be the id of the desired component
     *
     * In addition, the following shorthands are available:
     *
     * - `"."` the current game object
     * - `"#"` the current component
     *
     * @name msg.url
     * @param urlstring [type:string] string to create the url from
     * @return url [type:url] a new URL
     * @examples
     *
     * ```lua
     * local my_url = msg.url("#my_component")
     * print(my_url) --> url: [current_collection:/my_instance#my_component]
     *
     * local my_url = msg.url("my_collection:/my_sub_collection/my_instance#my_component")
     * print(my_url) --> url: [my_collection:/my_sub_collection/my_instance#my_component]
     *
     * local my_url = msg.url("my_socket:")
     * print(my_url) --> url: [my_collection:]
     * ```
     */

    /*# creates a new URL from separate arguments
     *
     * @name msg.url
     * @param [socket] [type:string|hash] socket of the URL
     * @param [path] [type:string|hash] path of the URL
     * @param [fragment] [type:string|hash] fragment of the URL
     * @return url [type:url] a new URL
     * @examples
     *
     * ```lua
     * local my_socket = "main" -- specify by valid name
     * local my_path = hash("/my_collection/my_gameobject") -- specify as string or hash
     * local my_fragment = "component" -- specify as string or hash
     * local my_url = msg.url(my_socket, my_path, my_fragment)
     *
     * print(my_url) --> url: [main:/my_collection/my_gameobject#component]
     * print(my_url.socket) --> 786443 (internal numeric value)
     * print(my_url.path) --> hash: [/my_collection/my_gameobject]
     * print(my_url.fragment) --> hash: [component]
     * ```
     */
    int URL_new(lua_State* L)
    {
        int top = lua_gettop(L);
        (void)top;
        dmMessage::URL url;
        dmMessage::ResetURL(&url);
        if (top < 2)
        {
            ResolveURL(L, 1, &url, 0x0);
        }
        else if (top == 3)
        {
            dmMessage::URL source;
            if (lua_isnil(L, 1))
            {
                dmMessage::ResetURL(&source);
                GetURL(L, &source);
            }
            if (!lua_isnil(L, 1))
            {
                if (IsHash(L, 1))
                {
                    url.m_Socket = *(dmhash_t*)lua_touserdata(L, 1);
                }
                else
                {
                    const char* s = lua_tostring(L, 1);
                    dmMessage::Result result = dmMessage::GetSocket(s, &url.m_Socket);
                    switch (result)
                    {
                        case dmMessage::RESULT_OK:
                        case dmMessage::RESULT_NAME_OK_SOCKET_NOT_FOUND:
                            break;
                        case dmMessage::RESULT_INVALID_SOCKET_NAME:
                            return luaL_error(L, "The socket '%s' name is invalid.", s);
                        default:
                            return luaL_error(L, "Error when getting socket '%s': %d.", s, result);
                    }
                }
            }
            else
            {
                url.m_Socket = source.m_Socket;
            }
            if (!lua_isnil(L, 2))
            {
                if (lua_isstring(L, 2))
                {
                    const char* path = lua_tostring(L, 2);
                    if (lua_isnil(L, 1) || (lua_isstring(L, 1) && *lua_tostring(L, 1) == '\0'))
                    {
                        size_t path_size = strlen(path);
                        if (path_size > 0)
                        {
                            ResolvePath(L, path, path_size, url.m_Path);
                        }
                        else
                        {
                            dmMessage::URL default_url;
                            dmMessage::ResetURL(&default_url);
                            GetURL(L, &default_url);
                            url.m_Path = default_url.m_Path;
                        }
                    }
                    else
                    {
                        url.m_Path = dmHashString64(path);
                    }
                }
                else
                {
                    url.m_Path = CheckHash(L, 2);
                }
            }
            else
            {
                if (lua_isnil(L, 1))
                {
                    url.m_Path = source.m_Path;
                }
                else if (!lua_isnil(L, 3))
                {
                    return luaL_error(L, "Can't resolve id with specified socket and fragment.");
                }
            }
            if (!lua_isnil(L, 3))
            {
                if (lua_isstring(L, 3))
                {
                    url.m_Fragment = dmHashString64(lua_tostring(L, 3));
                }
                else
                {
                    url.m_Fragment = CheckHash(L, 3);
                }
            }
            else
            {
                if (lua_isnil(L, 1) && lua_isnil(L, 2))
                {
                    url.m_Fragment = source.m_Fragment;
                }
                else
                {
                    url.m_Fragment = 0;
                }
            }
        }
        else if (top > 0)
        {
            luaL_error(L, "Only %s.%s(), %s.%s(\"[socket:][path][#fragment]\") or %s.%s(socket, path, fragment) is supported.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_URL, SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_URL, SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_URL);
        }
        PushURL(L, url);
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# posts a message to a receiving URL
     *
     * Post a message to a receiving URL. The most common case is to send messages
     * to a component. If the component part of the receiver is omitted, the message
     * is broadcast to all components in the game object.
     *
     * The following receiver shorthands are available:
     *
     * - `"."` the current game object
     * - `"#"` the current component
     *
     * [icon:attention] There is a 2 kilobyte limit to the message parameter table size.
     *
     * @name msg.post
     * @param receiver [type:string|url|hash] The receiver must be a string in URL-format, a URL object or a hashed string.
     * @param message_id [type:string|hash] The id must be a string or a hashed string.
     * @param [message] [type:table|nil] a lua table with message parameters to send.
     * @examples
     *
     * Send "enable" to the sprite "my_sprite" in "my_gameobject":
     *
     * ```lua
     * msg.post("my_gameobject#my_sprite", "enable")
     * ```
     *
     * Send a "my_message" to an url with some additional data:
     *
     * ```lua
     * local params = {my_parameter = "my_value"}
     * msg.post(my_url, "my_message", params)
     * ```
     */
    int Msg_Post(lua_State* L)
    {
        int top = lua_gettop(L);
        if (lua_isnil(L, 1))
        {
            return luaL_error(L, "The receiver shouldn't be `nil`");
        }

        dmMessage::URL receiver;
        dmMessage::URL sender;
        ResolveURL(L, 1, &receiver, &sender);

        dmhash_t message_id;
        if (lua_isstring(L, 2))
        {
            message_id = dmHashString64(lua_tostring(L, 2));
        }
        else
        {
            message_id = CheckHash(L, 2);
        }

        char DM_ALIGNED(16) data[MAX_MESSAGE_DATA_SIZE];
        uint32_t data_size = 0;


        const dmDDF::Descriptor* desc = dmDDF::GetDescriptorFromHash(message_id);
        if (desc != 0)
        {
            if (desc->m_Size > MAX_MESSAGE_DATA_SIZE)
            {
                return luaL_error(L, "The message is too large to be sent (%d bytes, max is %d).", desc->m_Size, MAX_MESSAGE_DATA_SIZE);
            }
            if (top > 2)
            {
                luaL_checktype(L, 3, LUA_TTABLE);
                lua_pushvalue(L, 3);
            }
            else
            {
                lua_newtable(L);
            }
            data_size = dmScript::CheckDDF(L, desc, data, MAX_MESSAGE_DATA_SIZE, -1);
            lua_pop(L, 1);
        }
        else if (top > 2)
        {
            if (!lua_isnil(L, 3))
            {
                data_size = dmScript::CheckTable(L, data, MAX_MESSAGE_DATA_SIZE, 3);
            }
        }

        assert(top == lua_gettop(L));

        dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, 0, (uintptr_t) desc, data, data_size, 0);
        if (result == dmMessage::RESULT_SOCKET_NOT_FOUND)
        {
            char receiver_buffer[64];
            UrlToString(&receiver, receiver_buffer, sizeof(receiver_buffer));
            char sender_buffer[64];
            UrlToString(&sender, sender_buffer, sizeof(sender_buffer));
            return luaL_error(L, "Could not send message '%s' from '%s' to '%s'.", dmHashReverseSafe64(message_id), sender_buffer, receiver_buffer);
        }
        else if (result != dmMessage::RESULT_OK)
        {
            return luaL_error(L, "Could not send message to %s.", dmMessage::GetSocketName(receiver.m_Socket));
        }

        return 0;
    }

    static const luaL_reg ScriptMsg_methods[] =
    {
        {SCRIPT_TYPE_NAME_URL, URL_new},
        {"post", Msg_Post},
        {0, 0}
    };

    void InitializeMsg(lua_State* L)
    {
        int top = lua_gettop(L);

        SCRIPT_URL_TYPE_HASH = dmScript::RegisterUserType(L, SCRIPT_TYPE_NAME_URL, URL_methods, URL_meta);

        luaL_register(L, SCRIPT_LIB_NAME, ScriptMsg_methods);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));
    }

    void PushURL(lua_State* L, const dmMessage::URL& url)
    {
        dmMessage::URL* urlp = (dmMessage::URL*)lua_newuserdata(L, sizeof(dmMessage::URL));
        *urlp = url;
        luaL_getmetatable(L, SCRIPT_TYPE_NAME_URL);
        lua_setmetatable(L, -2);
    }

    dmMessage::URL* CheckURL(lua_State* L, int index)
    {
        return (dmMessage::URL*)dmScript::CheckUserType(L, index, SCRIPT_URL_TYPE_HASH, 0);
    }

    bool GetURL(lua_State* L, dmMessage::URL* out_url)
    {
        return GetURL(L, *out_url);
    }

    dmMessage::Result ResolveURL(lua_State* L, const char* url, dmMessage::URL* out_url, dmMessage::URL* default_url)
    {
        // Special handling for "." which means default socket + path
        if (url[0] == '.' && url[1] == '\0')
        {
            out_url->m_Socket = default_url->m_Socket;
            out_url->m_Path = default_url->m_Path;
            return dmMessage::RESULT_OK;
        }
        // Special handling for "#" which means default socket + path + fragment
        if (url[0] == '#' && url[1] == '\0')
        {
            *out_url = *default_url;
            return dmMessage::RESULT_OK;
        }

        // Make sure that m_FunctionRef is 0. A value != 0 is probably a bug due to
        // reused PropertyVar (union) or use of char-buffers with uninitialized data
        assert(out_url->_reserved == 0);
        dmMessage::StringURL string_url;
        dmMessage::Result result = dmMessage::ParseURL(url, &string_url);
        if (result != dmMessage::RESULT_OK)
        {
            return result;
        }
        if (string_url.m_SocketSize > 0)
        {
            char socket_name[64];
            if (string_url.m_SocketSize >= sizeof(socket_name))
                return dmMessage::RESULT_INVALID_SOCKET_NAME;
            dmStrlCpy(socket_name, string_url.m_Socket, dmMath::Min(string_url.m_SocketSize+1, (unsigned int) sizeof(socket_name)));
            result = dmMessage::GetSocket(socket_name, &out_url->m_Socket);
            if (!(result == dmMessage::RESULT_OK || result == dmMessage::RESULT_NAME_OK_SOCKET_NOT_FOUND))
            {
                return result;
            }
            out_url->m_Path = dmHashBuffer64(string_url.m_Path, string_url.m_PathSize);
        }
        else
        {
            out_url->m_Socket = default_url->m_Socket;
            if (string_url.m_PathSize > 0)
            {
                ResolvePath(L, string_url.m_Path, string_url.m_PathSize, out_url->m_Path);
            }
            else
            {
                out_url->m_Path = default_url->m_Path;
            }
        }
        if (string_url.m_FragmentSize > 0)
        {
            out_url->m_Fragment = dmHashBuffer64(string_url.m_Fragment, string_url.m_FragmentSize);
        }
        else if (string_url.m_SocketSize == 0 && string_url.m_PathSize == 0)
        {
            out_url->m_Fragment = default_url->m_Fragment;
        }
        else
        {
            out_url->m_Fragment = 0;
        }
        return dmMessage::RESULT_OK;
    }

    static bool IsURLGlobal(dmMessage::StringURL* url)
    {
        return url->m_SocketSize > 0 && url->m_PathSize > 0 && *url->m_Path == '/';
    }

    int ResolveURL(lua_State* L, int index, dmMessage::URL* out_url, dmMessage::URL* out_default_url)
    {
        if (dmScript::IsURL(L, index))
        {
            *out_url = *(dmMessage::URL*)lua_touserdata(L, index);
            if (out_default_url != 0x0)
            {
                dmMessage::ResetURL(out_default_url);
                GetURL(L, out_default_url);
            }
        }
        else
        {

            const char* url = 0;
            dmMessage::StringURL string_url;
            dmMessage::Result parse_url_result;
            if (lua_isstring(L, index))
            {
                // Make sure we get and parse the url only once
                url = lua_tostring(L, index);
                parse_url_result = dmMessage::ParseURL(url, &string_url);
                if (parse_url_result != dmMessage::RESULT_OK)
                {
                    url = 0;
                }
            }

            // Initial check for global urls to avoid resolving etc
            if (url != 0)
            {
                if (parse_url_result == dmMessage::RESULT_OK)
                {
                    if (IsURLGlobal(&string_url))
                    {
                        char socket_name[64];
                        if (string_url.m_SocketSize >= sizeof(socket_name))
                            return dmMessage::RESULT_INVALID_SOCKET_NAME;
                        dmStrlCpy(socket_name, string_url.m_Socket, dmMath::Min(string_url.m_SocketSize+1, (unsigned int) sizeof(socket_name)));
                        dmMessage::HSocket socket;
                        dmMessage::Result result = dmMessage::GetSocket(socket_name, &socket);
                        switch (result)
                        {
                            case dmMessage::RESULT_OK:
                            case dmMessage::RESULT_NAME_OK_SOCKET_NOT_FOUND:
                                out_url->m_Socket = socket;
                                out_url->m_Path = dmHashBuffer64(string_url.m_Path, string_url.m_PathSize);
                                out_url->m_Fragment = dmHashBuffer64(string_url.m_Fragment, string_url.m_FragmentSize);
                                if (out_default_url != 0x0)
                                {
                                    dmMessage::ResetURL(out_default_url);
                                    GetURL(L, out_default_url);
                                }
                                return 0;
                            case dmMessage::RESULT_INVALID_SOCKET_NAME:
                                return luaL_error(L, "The socket '%s' name is invalid.", socket_name);
                            default:
                                return luaL_error(L, "Error when checking socket '%s': %d.", socket_name, result);
                        }
                    }
                }
            }
            // Fetch default URL from the lua state
            dmMessage::URL default_url;
            dmMessage::ResetURL(&default_url);
            GetURL(L, &default_url);
            if (out_default_url != 0x0)
            {
                *out_default_url = default_url;
            }
            // Check for the URL representation (nil, string, url, hash) at index
            if (lua_gettop(L) < index || lua_isnil(L, index))
            {
                *out_url = default_url;
            }
            else if (url != 0)
            {
                dmMessage::ResetURL(out_url);
                dmMessage::Result result = parse_url_result;
                if (parse_url_result == dmMessage::RESULT_OK)
                {
                    result = ResolveURL(L, url, out_url, &default_url);
                }
                if (result != dmMessage::RESULT_OK)
                {
                    switch (result)
                    {
                    case dmMessage::RESULT_MALFORMED_URL:
                        return luaL_error(L, "Could not parse '%s' because the URL is invalid (should be [socket:][path][#fragment]).", url);
                    case dmMessage::RESULT_INVALID_SOCKET_NAME:
                        return luaL_error(L, "The socket name in '%s' is invalid.", url);
                    case dmMessage::RESULT_SOCKET_NOT_FOUND:
                        return luaL_error(L, "The socket in '%s' could not be found.", url);
                    default:
                        return luaL_error(L, "Error when resolving the URL '%s': %d.", url, result);
                    }
                }
            }
            else if (IsHash(L, index))
            {
                out_url->m_Socket = default_url.m_Socket;
                out_url->m_Path = *(dmhash_t*)lua_touserdata(L, index);
                out_url->m_Fragment = 0;
            }
            else
            {
                return luaL_typerror(L, index, SCRIPT_TYPE_NAME_URL);
            }
        }
        return 0;
    }

#undef SCRIPT_LIB_NAME
#undef SCRIPT_TYPE_NAME_URL
}
