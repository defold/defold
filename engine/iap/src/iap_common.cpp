#include "iap.h"
#include <string.h>
#include <stdlib.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <script/script.h>


char* IAP_List_CreateBuffer(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushnil(L);
    int length = 0;
    while (lua_next(L, 1) != 0) {
        if (length > 0) {
            ++length;
        }
        const char* p = luaL_checkstring(L, -1);
        length += strlen(p);
        lua_pop(L, 1);
    }

    char* buf = (char*)malloc(length+1);
    if( buf == 0 )
    {
        dmLogError("Could not allocate buffer of size %d", length+1);
        return 0;
    }
    buf[0] = '\0';

    int i = 0;
    lua_pushnil(L);
    int index =0;
    while (lua_next(L, 1) != 0) {
        if (i > 0) {
            dmStrlCat(buf, ",", length+1);
        }
        const char* p = luaL_checkstring(L, -1);
        dmStrlCat(buf, p, length+1);
        lua_pop(L, 1);
        ++i;
    }
    return buf;
}
