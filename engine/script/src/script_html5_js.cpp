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
     * HTML5 platform specific functions.
     *
     * [icon:html5] The following functions are only available on HTML5 builds, the `html5.*` Lua namespace will not be available on other platforms.
     *
     * @document
     * @name HTML5
     * @namespace html5
     */

    /*# run JavaScript code, in the browser, from Lua
     *
     * Executes the supplied string as JavaScript inside the browser.
     * A call to this function is blocking, the result is returned as-is, as a string.
     * (Internally this will execute the string using the `eval()` JavaScript function.)
     *
     * @name html5.run
     * @param code [type:string] Javascript code to run
     * @return result [type:string] result as string
     * @examples
     *
     * ```lua
     * local res = html5.run("10 + 20") -- returns the string "30"
     * print(res)
     * local res_num = tonumber(res) -- convert to number
     * print(res_num - 20) -- prints 10
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
