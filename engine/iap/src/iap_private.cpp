#include "iap.h"
#include "iap_private.h"
#include <string.h>
#include <stdlib.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <script/script.h>

// Creates a comma separated string, given a table where all values are strings (or numbers)
// Returns a malloc'ed string, which the caller must free
char* IAP_List_CreateBuffer(lua_State* L)
{
    int top = lua_gettop(L);

    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushnil(L);
    int length = 0;
    while (lua_next(L, 1) != 0) {
        if (length > 0) {
            ++length;
        }
        const char* p = lua_tostring(L, -1);
        if(!p)
        {
            luaL_error(L, "IAP: Failed to get value (string) from table");
        }
        length += strlen(p);
        lua_pop(L, 1);
    }

    char* buf = (char*)malloc(length+1);
    if( buf == 0 )
    {
        dmLogError("Could not allocate buffer of size %d", length+1);
        assert(top == lua_gettop(L));
        return 0;
    }
    buf[0] = '\0';

    int i = 0;
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        if (i > 0) {
            dmStrlCat(buf, ",", length+1);
        }
        const char* p = lua_tostring(L, -1);
        if(!p)
        {
            luaL_error(L, "IAP: Failed to get value (string) from table");
        }
        dmStrlCat(buf, p, length+1);
        lua_pop(L, 1);
        ++i;
    }

    assert(top == lua_gettop(L));
    return buf;
}

void IAP_PushError(lua_State* L, const char* error, int reason)
{
    if (error != 0) {
        lua_newtable(L);
        lua_pushstring(L, "error");
        lua_pushstring(L, error);
        lua_rawset(L, -3);
        lua_pushstring(L, "reason");
        lua_pushnumber(L, reason);
        lua_rawset(L, -3);
    } else {
        lua_pushnil(L);
    }
}

void IAP_PushConstants(lua_State* L)
{
    #define SETCONSTANT(name) \
            lua_pushnumber(L, (lua_Number) name); \
            lua_setfield(L, -2, #name);\

        SETCONSTANT(TRANS_STATE_PURCHASING)
        SETCONSTANT(TRANS_STATE_PURCHASED)
        SETCONSTANT(TRANS_STATE_FAILED)
        SETCONSTANT(TRANS_STATE_RESTORED)
        SETCONSTANT(TRANS_STATE_UNVERIFIED)

        SETCONSTANT(REASON_UNSPECIFIED)
        SETCONSTANT(REASON_USER_CANCELED)

        SETCONSTANT(PROVIDER_ID_GOOGLE)
        SETCONSTANT(PROVIDER_ID_AMAZON)
        SETCONSTANT(PROVIDER_ID_APPLE)
        SETCONSTANT(PROVIDER_ID_FACEBOOK)
        SETCONSTANT(PROVIDER_ID_GAMEROOM)

    #undef SETCONSTANT
}
