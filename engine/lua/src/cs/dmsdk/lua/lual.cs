// Copyright 2020-2024 The Defold Foundation
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

namespace dmSDK.Lua {
    public unsafe partial class LuaL
    {
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack=1)]
        public unsafe struct Reg
        {
            public IntPtr   name;
            public IntPtr   func;
        }

        // A way to help pass managed strings
        public struct RegHelper
        {
            public String?  name;
            public IntPtr   func;
        }

        public static void Register(Lua.State* L, String libname, RegHelper[] inregs)
        {
            Reg[] regs = new Reg[inregs.Length];
            for (int i = 0; i < inregs.Length; i++)
            {
                if (inregs[i].name != null)
                {
                    regs[i].name = (IntPtr)Marshal.StringToHGlobalAnsi(inregs[i].name);
                }
                else
                {
                   regs[i].name = 0;
                }
                regs[i].func = inregs[i].func;
            }

            fixed (Reg* pregs = regs)
            {
                LuaL.register(L, libname, pregs);
            }

            for (int i = 0; i < regs.Length; i++)
            {
                if (regs[i].name != 0)
                    Marshal.FreeHGlobal(regs[i].name);
            }
        }

        /*
        ** ===============================================================
        ** some useful macros
        ** ===============================================================
        */

        public static String checkstring(Lua.State* L, int n)
        {
            return LuaL.checklstring(L, n, (int*)0);
        }

        // #define luaL_argcheck(L, cond,numarg,extramsg)  \
        //         ((void)((cond) || luaL_argerror(L, (numarg), (extramsg))))
        // #define luaL_checkstring(L,n)   (luaL_checklstring(L, (n), NULL))
        // #define luaL_optstring(L,n,d)   (luaL_optlstring(L, (n), (d), NULL))
        // #define luaL_checkint(L,n)  ((int)luaL_checkinteger(L, (n)))
        // #define luaL_optint(L,n,d)  ((int)luaL_optinteger(L, (n), (d)))
        // #define luaL_checklong(L,n) ((long)luaL_checkinteger(L, (n)))
        // #define luaL_optlong(L,n,d) ((long)luaL_optinteger(L, (n), (d)))

        // #define luaL_typename(L,i)  lua_typename(L, lua_type(L,(i)))

        // #define luaL_dofile(L, fn) \
        //     (luaL_loadfile(L, fn) || lua_pcall(L, 0, LUA_MULTRET, 0))

        // #define luaL_dostring(L, s) \
        //     (luaL_loadstring(L, s) || lua_pcall(L, 0, LUA_MULTRET, 0))

        // #define luaL_getmetatable(L,n)  (lua_getfield(L, LUA_REGISTRYINDEX, (n)))

        // #define luaL_opt(L,f,n,d)   (lua_isnoneornil(L,(n)) ? (d) : f(L,(n)))


    } // LuaL
} // dmSDK.Lua

