#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <dlib/array.h>
#include <dlib/math.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include "script.h"

#include <emscripten/emscripten.h>

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

#include "script_private.h"

namespace dmScript
{
    #define LIB_NAME "html5"

    /*# HTML5 API documentation
     *
     * Interop functions
     *
     * @document
     * @name HTML5
     * @namespace html5
     */


    /*# Run Javascript code, in the browser, from Lua
     *
     * @name html5.run
     * @param code [type:string] Javascript code to run
     * @return result [type:string] result as string
     * @examples
     *
     * ```lua
     * print(html5.run("10 + 20"))
     * ```
     */
    int Html5_Run(lua_State* L)
    {
        const char* code = luaL_checkstring(L, 1);
        char *ret = emscripten_run_script_string(code);
        lua_pushstring(L, ret);
        return 1;
    }

    static const luaL_reg ScriptHtml5_methods[] =
    {
        {"run", Html5_Run},
        {0, 0}
    };

    void InitializeHtml5(lua_State* L)
    {
        int top = lua_gettop(L);

        lua_pushvalue(L, LUA_GLOBALSINDEX);
        luaL_register(L, LIB_NAME, ScriptHtml5_methods);
        lua_pop(L, 2);

        assert(top == lua_gettop(L));
    }
}
