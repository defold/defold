#include "script.h"

#include "script_private.h"

#include <stdint.h>
#include <string.h>

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

    const uint32_t MAX_MESSAGE_DATA_SIZE = 512;

    bool IsURL(lua_State *L, int index)
    {
        void *p = lua_touserdata(L, index);
        bool result = false;
        if (p != 0x0)
        {  /* value is a userdata? */
            if (lua_getmetatable(L, index))
            {  /* does it have a metatable? */
                lua_getfield(L, LUA_REGISTRYINDEX, SCRIPT_TYPE_NAME_URL);  /* get correct metatable */
                if (lua_rawequal(L, -1, -2))
                {  /* does it have the correct mt? */
                    result = true;
                }
                lua_pop(L, 2);  /* remove both metatables */
            }
        }
        return result;
    }

    static int URL_gc(lua_State *L)
    {
        dmMessage::URL* url = CheckURL(L, 1);
        memset(url, 0, sizeof(*url));
        return 0;
    }

    void url_tostring(const dmMessage::URL* url, char* buffer, uint32_t buffer_size)
    {
        char tmp[32];
        *buffer = '\0';
        const char* socket = "<unknown>";
        if (dmMessage::IsSocketValid(url->m_Socket))
        {
            const char* s = dmMessage::GetSocketName(url->m_Socket);
            if (s != 0x0)
            {
                socket = s;
            }
        }
        dmStrlCpy(buffer, socket, buffer_size);
        dmStrlCat(buffer, ":", buffer_size);
        if (url->m_Path != 0)
        {
            DM_SNPRINTF(tmp, sizeof(tmp), "%s", (const char*) dmHashReverse64(url->m_Path, 0));
            dmStrlCat(buffer, tmp, buffer_size);
        }
        if (url->m_Fragment != 0)
        {
            dmStrlCat(buffer, "#", buffer_size);
            DM_SNPRINTF(tmp, sizeof(tmp), "%s", (const char*) dmHashReverse64(url->m_Fragment, 0));
            dmStrlCat(buffer, tmp, buffer_size);
        }
    }

    static int URL_tostring(lua_State *L)
    {
        dmMessage::URL* url = CheckURL(L, 1);
        char buffer[64];
        url_tostring(url, buffer, 64);
        lua_pushfstring(L, "%s: [%s]", SCRIPT_TYPE_NAME_URL, buffer);
        return 1;
    }

    static int URL_concat(lua_State *L)
    {
        const char* s = luaL_checkstring(L, 1);
        dmMessage::URL* url = CheckURL(L, 2);
        char buffer[64];
        url_tostring(url, buffer, 64);
        lua_pushfstring(L, "%s[%s]", s, buffer);
        return 1;
    }

    static int URL_index(lua_State *L)
    {
        dmMessage::URL* url = CheckURL(L, 1);

        const char* key = luaL_checkstring(L, 2);
        if (strcmp("socket", key) == 0)
        {
            if (url->m_Socket != 0)
            {
                lua_pushnumber(L, url->m_Socket);
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
        dmMessage::URL* url = CheckURL(L, 1);

        const char* key = luaL_checkstring(L, 2);
        if (strcmp("socket", key) == 0)
        {
            if (lua_isnumber(L, 3))
            {
                url->m_Socket = (dmMessage::HSocket)luaL_checknumber(L, 3);
                if (dmMessage::GetSocketName(url->m_Socket) == 0x0)
                {
                    return luaL_error(L, "Could not find the socket in %d.", url->m_Socket);
                }
            }
            else if (lua_isstring(L, 3))
            {
                const char* socket_name = lua_tostring(L, 3);
                dmMessage::Result result = dmMessage::GetSocket(socket_name, &url->m_Socket);
                if (result != dmMessage::RESULT_OK)
                {
                    return luaL_error(L, "Could not find the socket '%s'.", socket_name);
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
                url->m_Fragment = CheckHash(L, 3);
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
        dmMessage::URL* url1 = CheckURL(L, 1);
        dmMessage::URL* url2 = CheckURL(L, 2);
        lua_pushboolean(L, url1->m_Socket == url2->m_Socket && url1->m_Path == url2->m_Path && url1->m_Fragment == url2->m_Fragment);
        return 1;
    }

    static const luaL_reg URL_methods[] =
    {
        {0,0}
    };

    static const luaL_reg URL_meta[] =
    {
        {"__gc",        URL_gc},
        {"__tostring",  URL_tostring},
        {"__concat",    URL_concat},
        {"__index",     URL_index},
        {"__newindex",  URL_newindex},
        {"__eq",        URL_eq},
        {0,0}
    };

    /*# creates a new URL
     * This is equivalent to msg.url("").
     *
     * @name msg.url
     * @return a new URL (url)
     */

    /*# creates a new URL from a string
     * The format of the string must be <code>"[socket:][path][#fragment]"</code>, which is similar to a http URL.
     * When addressing instances, <code>socket</code> is the name of the collection. <code>path</code> is the id of the instance,
     * which can either be relative the instance of the calling script or global. <code>fragment</code> would be the id of the desired component.
     *
     * @name msg.url
     * @param urlstring string to create the url from (string)
     * @return a new URL (url)
     * @examples
     * <pre>
     * local my_url = msg.url("#my_component")
     * local my_url = msg.url("my_collection:/my_sub_collection/my_instance#my_component")
     * local my_url = msg.url("my_socket:")
     * </pre>
     */

    /*# creates a new URL from separate arguments
     *
     * @name msg.url
     * @param [socket] socket of the URL (string|socket)
     * @param [path] path of the URL (string|hash)
     * @param [fragment] fragment of the URL (string|hash)
     * @return a new URL (url)
     */
    int URL_new(lua_State* L)
    {
        int top = lua_gettop(L);
        (void)top;
        dmMessage::URL url;
        dmMessage::ResetURL(url);
        dmMessage::URL default_url;
        dmMessage::ResetURL(default_url);
        GetURL(L, &default_url);
        if (top < 2)
        {
            ResolveURL(L, 1, &url, &default_url);
        }
        else if (top == 3)
        {
            if (!lua_isnil(L, 1))
            {
                if (lua_isnumber(L, 1))
                {
                    url.m_Socket = lua_tonumber(L, 1);
                }
                else
                {
                    const char* s = lua_tostring(L, 1);
                    dmMessage::Result result = dmMessage::GetSocket(s, &url.m_Socket);
                    switch (result)
                    {
                        case dmMessage::RESULT_OK:
                            break;
                        case dmMessage::RESULT_INVALID_SOCKET_NAME:
                            return luaL_error(L, "The socket '%s' is invalid.", s);
                        case dmMessage::RESULT_SOCKET_NOT_FOUND:
                            return luaL_error(L, "The socket '%s' could not be found.", s);
                        default:
                            return luaL_error(L, "Error when checking socket '%s': %d.", s, result);
                    }
                }
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
                            lua_getglobal(L, SCRIPT_RESOLVE_PATH_CALLBACK);
                            url.m_Path = ((ResolvePathCallback)lua_touserdata(L, -1))((uintptr_t)L, path, path_size);
                            lua_pop(L, 1);
                        }
                        else
                        {
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
                url.m_Fragment = 0;
            }
        }
        else if (top > 0)
        {
            luaL_error(L, "Only %s.%s(\"[socket:][path][#fragment]\") or %s.%s(socket, path, fragment) is supported.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_URL, SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_URL);
        }
        PushURL(L, url);
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# posts a message to a receiving URL
     *
     * @name msg.post
     * @param receiver The receiver must be a string in URL-format, a URL object, a hashed string or nil. Nil is a short way of sending the message back to the calling script. (string|url|hash|nil)
     * @param message_id The id must be a string or a hashed string. (string|hash)
     * @param [message] lua table message to send (table)
     * @examples
     * <pre>
     * msg.post(my_url, "my_message", {my_parameter = "my_value"})
     * </pre>
     */
    int Msg_Post(lua_State* L)
    {
        int top = lua_gettop(L);

        dmMessage::URL sender;
        dmMessage::ResetURL(sender);
        dmMessage::URL receiver;
        dmMessage::ResetURL(receiver);

        GetURL(L, &sender);
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

        if (!dmMessage::IsSocketValid(receiver.m_Socket))
        {
            const char* message_name = (char*)dmHashReverse64(message_id, 0x0);
            char receiver_buffer[64];
            url_tostring(&receiver, receiver_buffer, 64);
            char sender_buffer[64];
            url_tostring(&sender, sender_buffer, 64);
            return luaL_error(L, "Could not send message '%s' from '%s' to '%s'.", message_name, sender_buffer, receiver_buffer);
        }

        char data[MAX_MESSAGE_DATA_SIZE];
        uint32_t data_size = 0;

        const dmDDF::Descriptor* desc = 0x0;
        lua_getglobal(L, SCRIPT_CONTEXT);
        HContext context = (HContext)lua_touserdata(L, -1);
        lua_pop(L, 1);
        if (context != 0)
        {
            desc = dmDDF::GetDescriptorFromHash(message_id);
            if (desc != 0)
            {
                if (top > 2)
                {
                    if (desc->m_Size > MAX_MESSAGE_DATA_SIZE)
                    {
                        return luaL_error(L, "The message is too large to be sent (%d bytes, max is %d).", desc->m_Size, MAX_MESSAGE_DATA_SIZE);
                    }
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
        }
        if (top > 2)
        {
            if (desc != 0x0)
            {
            }
            else
            {
                data_size = dmScript::CheckTable(L, data, MAX_MESSAGE_DATA_SIZE, 3);
            }
        }

        assert(top == lua_gettop(L));

        uintptr_t user_data;
        GetUserData(L, &user_data);

        dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, user_data, (uintptr_t) desc, data, data_size);
        if (result != dmMessage::RESULT_OK)
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

        const uint32_t type_count = 1;
        struct
        {
            const char* m_Name;
            const luaL_reg* m_Methods;
            const luaL_reg* m_Metatable;
        } types[type_count] =
        {
            {SCRIPT_TYPE_NAME_URL, URL_methods, URL_meta}
        };
        for (uint32_t i = 0; i < type_count; ++i)
        {
            // create methods table, add it to the globals
            luaL_register(L, types[i].m_Name, types[i].m_Methods);
            int methods_index = lua_gettop(L);
            // create metatable for the type, add it to the Lua registry
            luaL_newmetatable(L, types[i].m_Name);
            int metatable = lua_gettop(L);
            // fill metatable
            luaL_register(L, 0, types[i].m_Metatable);

            lua_pushliteral(L, "__metatable");
            lua_pushvalue(L, methods_index);// dup methods table
            lua_settable(L, metatable);

            lua_pop(L, 2);
        }
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
        if (lua_type(L, index) == LUA_TUSERDATA)
        {
            return (dmMessage::URL*)luaL_checkudata(L, index, SCRIPT_TYPE_NAME_URL);
        }
        luaL_typerror(L, index, SCRIPT_TYPE_NAME_URL);
        return 0x0;
    }

    bool GetURL(lua_State* L, dmMessage::URL* out_url)
    {
        lua_getglobal(L, SCRIPT_GET_URL_CALLBACK);
        GetURLCallback callback = (GetURLCallback)lua_touserdata(L, -1);
        if (callback != 0x0)
        {
            callback(L, out_url);
            lua_pop(L, 1);
            return true;
        }
        lua_pop(L, 1);
        return false;
    }

    dmMessage::Result ResolveURL(ResolvePathCallback resolve_path_callback, uintptr_t resolve_user_data, const char* url, dmMessage::URL* out_url, dmMessage::URL* default_url)
    {
        const char* socket;
        uint32_t socket_size;
        const char* path;
        uint32_t path_size;
        const char* fragment;
        uint32_t fragment_size;
        dmMessage::Result result = dmMessage::ParseURL(url, &socket, &socket_size, &path, &path_size, &fragment, &fragment_size);
        if (result != dmMessage::RESULT_OK)
        {
            return result;
        }
        if (socket_size > 0)
        {
            char socket_name[64];
            if (socket_size >= sizeof(socket_name))
                return dmMessage::RESULT_INVALID_SOCKET_NAME;
            dmStrlCpy(socket_name, socket, dmMath::Min(socket_size+1, (unsigned int) sizeof(socket_name)));
            result = dmMessage::GetSocket(socket_name, &out_url->m_Socket);
            if (result != dmMessage::RESULT_OK)
            {
                return result;
            }
            out_url->m_Path = dmHashBuffer64(path, path_size);
        }
        else
        {
            out_url->m_Socket = default_url->m_Socket;
            if (path_size > 0)
            {
                out_url->m_Path = resolve_path_callback(resolve_user_data, path, path_size);
            }
            else
            {
                out_url->m_Path = default_url->m_Path;
            }
        }
        if (fragment_size > 0)
        {
            out_url->m_Fragment = dmHashBuffer64(fragment, fragment_size);
        }
        else if (socket_size == 0 && path_size == 0)
        {
            out_url->m_Fragment = default_url->m_Fragment;
        }
        else
        {
            out_url->m_Fragment = 0;
        }
        return dmMessage::RESULT_OK;
    }

    int ResolveURL(lua_State* L, int index, dmMessage::URL* out_url, dmMessage::URL* default_url)
    {
        if (lua_gettop(L) < index || lua_isnil(L, index))
        {
            *out_url = *default_url;
        }
        else if (lua_isstring(L, index))
        {
            lua_getglobal(L, SCRIPT_RESOLVE_PATH_CALLBACK);
            ResolvePathCallback callback = (ResolvePathCallback)lua_touserdata(L, -1);
            lua_pop(L, 1);
            const char* url = lua_tostring(L, index);
            dmMessage::Result result = ResolveURL(callback, (uintptr_t)L, url, out_url, default_url);
            if (result != dmMessage::RESULT_OK)
            {
                switch (result)
                {
                case dmMessage::RESULT_MALFORMED_URL:
                    return luaL_error(L, "Could not send message to '%s' because the address is invalid (should be [socket:][path][#fragment]).", url);
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
            out_url->m_Socket = default_url->m_Socket;
            out_url->m_Path = CheckHash(L, index);
            out_url->m_Fragment = 0;
        }
        else
        {
            *out_url = *CheckURL(L, index);
        }
        return 0;
    }

    bool GetUserData(lua_State* L, uintptr_t* out_user_data)
    {
        lua_getglobal(L, SCRIPT_GET_USER_DATA_CALLBACK);
        GetUserDataCallback callback = (GetUserDataCallback)lua_touserdata(L, -1);
        if (callback != 0x0)
        {
            *out_user_data = callback(L);
            lua_pop(L, 1);
            return true;
        }
        lua_pop(L, 1);
        return false;
    }

#undef SCRIPT_LIB_NAME
#undef SCRIPT_TYPE_NAME_URL
}
