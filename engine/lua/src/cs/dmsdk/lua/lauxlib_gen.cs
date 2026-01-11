// Generated, do not edit!
// Generated with cwd=/Users/bjornritzl/projects/defold/engine/lua and cmd=/Users/bjornritzl/projects/defold/scripts/dmsdk/gen_sdk.py -i /Users/bjornritzl/projects/defold/engine/lua/sdk_gen.json

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


/*# 
 *
 * 
 *
 * @document
 * @language C#
 * @name Lauxlib_gen
 * @namespace Lua
 */

using System.Runtime.InteropServices;
using System.Reflection.Emit;

using static dmSDK.Lua.Lua;

namespace dmSDK.Lua {
    public unsafe partial class LuaL
    {
        /// <summary>
        /// Generated from [ref:luaL_openlib]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_openlib", CallingConvention = CallingConvention.Cdecl)]
        public static extern void openlib(State * L, String libname, Reg * l, int nup);

        /// <summary>
        /// Generated from [ref:luaL_register]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_register", CallingConvention = CallingConvention.Cdecl)]
        public static extern void register(State * L, String libname, Reg * l);

        /// <summary>
        /// Generated from [ref:luaL_getmetafield]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_getmetafield", CallingConvention = CallingConvention.Cdecl)]
        public static extern int getmetafield(State * L, int obj, String e);

        /// <summary>
        /// Generated from [ref:luaL_callmeta]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_callmeta", CallingConvention = CallingConvention.Cdecl)]
        public static extern int callmeta(State * L, int obj, String e);

        /// <summary>
        /// Generated from [ref:luaL_typerror]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_typerror", CallingConvention = CallingConvention.Cdecl)]
        public static extern int typerror(State * L, int narg, String tname);

        /// <summary>
        /// Generated from [ref:luaL_argerror]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_argerror", CallingConvention = CallingConvention.Cdecl)]
        public static extern int argerror(State * L, int numarg, String extramsg);

        /// <summary>
        /// Generated from [ref:luaL_checklstring]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_checklstring", CallingConvention = CallingConvention.Cdecl)]
        public static extern String checklstring(State * L, int numArg, int * l);

        /// <summary>
        /// Generated from [ref:luaL_optlstring]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_optlstring", CallingConvention = CallingConvention.Cdecl)]
        public static extern String optlstring(State * L, int numArg, String def, int * l);

        /// <summary>
        /// Generated from [ref:luaL_checknumber]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_checknumber", CallingConvention = CallingConvention.Cdecl)]
        public static extern double checknumber(State * L, int numArg);

        /// <summary>
        /// Generated from [ref:luaL_optnumber]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_optnumber", CallingConvention = CallingConvention.Cdecl)]
        public static extern double optnumber(State * L, int nArg, double def);

        /// <summary>
        /// Generated from [ref:luaL_checkinteger]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_checkinteger", CallingConvention = CallingConvention.Cdecl)]
        public static extern int checkinteger(State * L, int numArg);

        /// <summary>
        /// Generated from [ref:luaL_optinteger]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_optinteger", CallingConvention = CallingConvention.Cdecl)]
        public static extern int optinteger(State * L, int nArg, int def);

        /// <summary>
        /// Generated from [ref:luaL_checkstack]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_checkstack", CallingConvention = CallingConvention.Cdecl)]
        public static extern void checkstack(State * L, int sz, String msg);

        /// <summary>
        /// Generated from [ref:luaL_checktype]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_checktype", CallingConvention = CallingConvention.Cdecl)]
        public static extern void checktype(State * L, int narg, int t);

        /// <summary>
        /// Generated from [ref:luaL_checkany]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_checkany", CallingConvention = CallingConvention.Cdecl)]
        public static extern void checkany(State * L, int narg);

        /// <summary>
        /// Generated from [ref:luaL_newmetatable]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_newmetatable", CallingConvention = CallingConvention.Cdecl)]
        public static extern int newmetatable(State * L, String tname);

        /// <summary>
        /// Generated from [ref:luaL_checkudata]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_checkudata", CallingConvention = CallingConvention.Cdecl)]
        public static extern void * checkudata(State * L, int ud, String tname);

        /// <summary>
        /// Generated from [ref:luaL_where]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_where", CallingConvention = CallingConvention.Cdecl)]
        public static extern void where(State * L, int lvl);

        /// <summary>
        /// Generated from [ref:luaL_error]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_error", CallingConvention = CallingConvention.Cdecl)]
        public static extern int error(State * L, String fmt);

        /// <summary>
        /// Generated from [ref:luaL_ref]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_ref", CallingConvention = CallingConvention.Cdecl)]
        public static extern int incref(State * L, int t);

        /// <summary>
        /// Generated from [ref:luaL_unref]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_unref", CallingConvention = CallingConvention.Cdecl)]
        public static extern void decref(State * L, int t, int _ref);

        /// <summary>
        /// Generated from [ref:luaL_loadfile]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_loadfile", CallingConvention = CallingConvention.Cdecl)]
        public static extern int loadfile(State * L, String filename);

        /// <summary>
        /// Generated from [ref:luaL_loadstring]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_loadstring", CallingConvention = CallingConvention.Cdecl)]
        public static extern int loadstring(State * L, String s);

        /// <summary>
        /// Generated from [ref:luaL_newstate]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_newstate", CallingConvention = CallingConvention.Cdecl)]
        public static extern State * newstate();

        /// <summary>
        /// Generated from [ref:luaL_gsub]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_gsub", CallingConvention = CallingConvention.Cdecl)]
        public static extern String gsub(State * L, String s, String p, String r);

        /// <summary>
        /// Generated from [ref:luaL_findtable]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="luaL_findtable", CallingConvention = CallingConvention.Cdecl)]
        public static extern String findtable(State * L, int idx, String fname, int szhint);

    } // LuaL
} // dmSDK.Lua

