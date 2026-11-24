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

using System.Runtime.InteropServices;
using System.Reflection.Emit;

using dmSDK.Lua;

public unsafe class TestLua
{
    private static IntPtr GetFunctionPointer(Delegate d)
    {
        ArgumentNullException.ThrowIfNull(d);
        var method = d.Method;

        if (d.Target is {} || !method.IsStatic || method is DynamicMethod)
        {
            return (IntPtr)0;
        }

        return method.MethodHandle.GetFunctionPointer();
    }

    private static int CsLuaAdd(Lua.State* L)
    {
        int a = LuaL.checkinteger(L, 1);
        int b = LuaL.checkinteger(L, 2);
        Lua.pushinteger(L, a + b);
        return 1;
    }

    private static int CsLuaMul(Lua.State* L)
    {
        int a = LuaL.checkinteger(L, 1);
        int b = LuaL.checkinteger(L, 2);
        Lua.pushinteger(L, a * b);
        return 1;
    }

    [UnmanagedCallersOnly(EntryPoint = "csRunTestsLua")]
    public static int csRunTestsLua(Lua.State* L)
    {
        int top = Lua.gettop(L);

        // Register a new function
        LuaL.RegHelper[] functions = {
            new() {name = "add", func = GetFunctionPointer(CsLuaAdd)},
            new() {name = "mul", func = GetFunctionPointer(CsLuaMul)},
            new() {name = null, func = 0}
        };

        LuaL.Register(L, "csfuncs", functions);
        Lua.pop(L, 1);

        Lua.pushinteger(L, 17);
        String s = Lua.tostring(L, -1); // converts the integer into a string
        Console.WriteLine(String.Format("    tostring(17): {0}", s));
        Lua.pop(L, 1); // Pop the string

        // Create the return value
        Lua.newtable(L);

        Lua.pushstring(L, "hello");
        Lua.setfield(L, -2, "string");

        Lua.pushnumber(L, 3.0);
        Lua.setfield(L, -2, "number");

        Lua.pushinteger(L, 2);
        Lua.setfield(L, -2, "integer");

        Lua.pushnumber(L, 3.0);
        Lua.setfield(L, -2, "number");

        int newtop = Lua.gettop(L);
        return (newtop - top) == 1 ? 0 : 1; // We want to leave the table on the stack
    }
}
