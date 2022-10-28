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

    static dmScript::LuaCallbackInfo* g_InteractionCallback = 0x0;
    static bool isOperationSuccessful;
    extern "C" void EMSCRIPTEN_KEEPALIVE dmScript_Html5ReportOperationSuccess(int result)
    {
        isOperationSuccessful = (bool)result;
    }

    extern "C" void EMSCRIPTEN_KEEPALIVE dmScript_RunInteractionCallback()
    {
        if (g_InteractionCallback == 0)
        {
            return;
        }

        // Save a reference to the callback in case the callback removes the listener
        dmScript::LuaCallbackInfo* callback = g_InteractionCallback;

        lua_State* L = dmScript::GetCallbackLuaContext(callback);
        DM_LUA_STACK_CHECK(L, 0);

        if (!dmScript::SetupCallback(callback))
        {
            return;
        }

        dmScript::PCall(L, 1, 0); // self + # user arguments

        dmScript::TeardownCallback(callback);
    }

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

        char* result = (char*)EM_ASM_INT({
            var jsResult;
            var isSuccess = 1;
            try
            {
                jsResult = eval(UTF8ToString($0));
            }
            catch (err)
            {
                isSuccess = 0;
                jsResult = err;
            }
            _dmScript_Html5ReportOperationSuccess(isSuccess);
            jsResult += "";
            var lengthBytes = lengthBytesUTF8(jsResult) + 1;
            var stringOnWasmHeap = _malloc(lengthBytes);
            stringToUTF8(jsResult, stringOnWasmHeap, lengthBytes);
            return stringOnWasmHeap;
        }, code);
        if (!isOperationSuccessful)
        {
            luaL_error(L, "%s", result);
            free(result);
            return 0;
        }
        lua_pushstring(L, result);
        free(result);
        return 1;
    }

    /*# set a JavaScript interaction listener callback from lua
     *
     * Set a JavaScript interaction listener callaback from lua that will be
     * invoked when a user interacts with the web page by clicking, touching or typing.
     * The callback can then call DOM restricted actions like requesting a pointer lock,
     * or start playing sounds the first time the callback is invoked.
     *
     * @name html5.set_interaction_listener
     * @param callback [type:function(self] The interaction callback. Pass an empty function or nil if you no longer wish to receive callbacks.
     *
     * `self`
     * : [type:object] The calling script
     *
     * @examples
     *
     * ```lua
     * local function on_interaction(self)
     *     print("on_interaction called")
     *     html5.set_interaction_listener(nil)
     * end
     *
     * function init(self)
     *     html5.set_interaction_listener(on_interaction)
     * end
     * ```
     */
    static int Html5_SetInteractionListener(lua_State* L)
    {
        luaL_checkany(L, 1);

        if (lua_isnil(L, 1))
        {
            bool remove_js_callbacks = g_InteractionCallback != 0;
            if (g_InteractionCallback)
            {
                EM_ASM({
                    document.removeEventListener('click', Module.__defold_interaction_listener);
                    document.removeEventListener('keyup', Module.__defold_interaction_listener);
                    document.removeEventListener('touchend', Module.__defold_interaction_listener);
                    Module.__defold_interaction_listener = undefined;
                });
                dmScript::DestroyCallback(g_InteractionCallback);
            }
            g_InteractionCallback = 0;

            return 0;
        }

        if (g_InteractionCallback)
        {
            dmScript::DestroyCallback(g_InteractionCallback);
        }

        g_InteractionCallback = dmScript::CreateCallback(L, 1);
        if (!dmScript::IsCallbackValid(g_InteractionCallback))
        {
            return luaL_error(L, "Failed to create callback");
        }

        EM_ASM({
            Module.__defold_interaction_listener = function () {
                _dmScript_RunInteractionCallback();
            };
            document.addEventListener('click',    Module.__defold_interaction_listener);
            document.addEventListener('keyup',    Module.__defold_interaction_listener);
            document.addEventListener('touchend', Module.__defold_interaction_listener);
        });

        return 0;
    }

    static const luaL_reg ScriptHtml5_methods[] =
    {
        {"run",                      Html5_Run},
        {"set_interaction_listener", Html5_SetInteractionListener},
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
