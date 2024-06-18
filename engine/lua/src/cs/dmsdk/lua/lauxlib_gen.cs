// Generated, do not edit!
// Generated with cwd=/Users/mathiaswesterdahl/work/defold/engine/lua and cmd=../../scripts/dmsdk/gen_sdk.py -i ./sdk_gen.json

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

using static dmSDK.Lua.Lua;

namespace dmSDK.Lua {
    public unsafe partial class LuaL
    {
        /*#
        * Generated from [ref:luaL_openlib]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_openlib", CallingConvention = CallingConvention.Cdecl)]
        public static extern void openlib(State * L, String libname, Reg * l, int nup);

        /*#
        * Generated from [ref:luaL_register]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_register", CallingConvention = CallingConvention.Cdecl)]
        public static extern void register(State * L, String libname, Reg * l);

        /*#
        * Generated from [ref:luaL_getmetafield]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_getmetafield", CallingConvention = CallingConvention.Cdecl)]
        public static extern int getmetafield(State * L, int obj, String e);

        /*#
        * Generated from [ref:luaL_callmeta]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_callmeta", CallingConvention = CallingConvention.Cdecl)]
        public static extern int callmeta(State * L, int obj, String e);

        /*#
        * Generated from [ref:luaL_typerror]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_typerror", CallingConvention = CallingConvention.Cdecl)]
        public static extern int typerror(State * L, int narg, String tname);

        /*#
        * Generated from [ref:luaL_argerror]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_argerror", CallingConvention = CallingConvention.Cdecl)]
        public static extern int argerror(State * L, int numarg, String extramsg);

        /*#
        * Generated from [ref:luaL_checklstring]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_checklstring", CallingConvention = CallingConvention.Cdecl)]
        public static extern String checklstring(State * L, int numArg, int * l);

        /*#
        * Generated from [ref:luaL_optlstring]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_optlstring", CallingConvention = CallingConvention.Cdecl)]
        public static extern String optlstring(State * L, int numArg, String def, int * l);

        /*#
        * Generated from [ref:luaL_checknumber]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_checknumber", CallingConvention = CallingConvention.Cdecl)]
        public static extern double checknumber(State * L, int numArg);

        /*#
        * Generated from [ref:luaL_optnumber]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_optnumber", CallingConvention = CallingConvention.Cdecl)]
        public static extern double optnumber(State * L, int nArg, double def);

        /*#
        * Generated from [ref:luaL_checkinteger]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_checkinteger", CallingConvention = CallingConvention.Cdecl)]
        public static extern int checkinteger(State * L, int numArg);

        /*#
        * Generated from [ref:luaL_optinteger]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_optinteger", CallingConvention = CallingConvention.Cdecl)]
        public static extern int optinteger(State * L, int nArg, int def);

        /*#
        * Generated from [ref:luaL_checkstack]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_checkstack", CallingConvention = CallingConvention.Cdecl)]
        public static extern void checkstack(State * L, int sz, String msg);

        /*#
        * Generated from [ref:luaL_checktype]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_checktype", CallingConvention = CallingConvention.Cdecl)]
        public static extern void checktype(State * L, int narg, int t);

        /*#
        * Generated from [ref:luaL_checkany]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_checkany", CallingConvention = CallingConvention.Cdecl)]
        public static extern void checkany(State * L, int narg);

        /*#
        * Generated from [ref:luaL_newmetatable]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_newmetatable", CallingConvention = CallingConvention.Cdecl)]
        public static extern int newmetatable(State * L, String tname);

        /*#
        * Generated from [ref:luaL_checkudata]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_checkudata", CallingConvention = CallingConvention.Cdecl)]
        public static extern void * checkudata(State * L, int ud, String tname);

        /*#
        * Generated from [ref:luaL_where]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_where", CallingConvention = CallingConvention.Cdecl)]
        public static extern void where(State * L, int lvl);

        /*#
        * Generated from [ref:luaL_error]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_error", CallingConvention = CallingConvention.Cdecl)]
        public static extern int error(State * L, String fmt);

        /*#
        * Generated from [ref:luaL_ref]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_ref", CallingConvention = CallingConvention.Cdecl)]
        public static extern int incref(State * L, int t);

        /*#
        * Generated from [ref:luaL_unref]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_unref", CallingConvention = CallingConvention.Cdecl)]
        public static extern void decref(State * L, int t, int _ref);

        /*#
        * Generated from [ref:luaL_loadfile]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_loadfile", CallingConvention = CallingConvention.Cdecl)]
        public static extern int loadfile(State * L, String filename);

        /*#
        * Generated from [ref:luaL_loadstring]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_loadstring", CallingConvention = CallingConvention.Cdecl)]
        public static extern int loadstring(State * L, String s);

        /*#
        * Generated from [ref:luaL_newstate]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_newstate", CallingConvention = CallingConvention.Cdecl)]
        public static extern State * newstate();

        /*#
        * Generated from [ref:luaL_gsub]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_gsub", CallingConvention = CallingConvention.Cdecl)]
        public static extern String gsub(State * L, String s, String p, String r);

        /*#
        * Generated from [ref:luaL_findtable]
        */
        [DllImport("luajit-5.1", EntryPoint="luaL_findtable", CallingConvention = CallingConvention.Cdecl)]
        public static extern String findtable(State * L, int idx, String fname, int szhint);

    } // LuaL
} // dmSDK.Lua

