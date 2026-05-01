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
 * @name Lua_gen
 * @namespace Lua
 */

using System.Runtime.InteropServices;
using System.Reflection.Emit;


namespace dmSDK.Lua {
    public unsafe partial class Lua
    {
        /// <summary>
        /// Generated from [ref:lua_State]
        /// </summary>
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack=1)]
        public struct State
        {
        }

        /// <summary>
        /// Generated from [ref:lua_newthread]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_newthread", CallingConvention = CallingConvention.Cdecl)]
        public static extern State * newthread(State * L);

        /// <summary>
        /// Generated from [ref:lua_gettop]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_gettop", CallingConvention = CallingConvention.Cdecl)]
        public static extern int gettop(State * L);

        /// <summary>
        /// Generated from [ref:lua_settop]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_settop", CallingConvention = CallingConvention.Cdecl)]
        public static extern void settop(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_pushvalue]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_pushvalue", CallingConvention = CallingConvention.Cdecl)]
        public static extern void pushvalue(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_remove]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_remove", CallingConvention = CallingConvention.Cdecl)]
        public static extern void remove(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_insert]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_insert", CallingConvention = CallingConvention.Cdecl)]
        public static extern void insert(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_replace]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_replace", CallingConvention = CallingConvention.Cdecl)]
        public static extern void replace(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_checkstack]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_checkstack", CallingConvention = CallingConvention.Cdecl)]
        public static extern int checkstack(State * L, int sz);

        /// <summary>
        /// Generated from [ref:lua_xmove]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_xmove", CallingConvention = CallingConvention.Cdecl)]
        public static extern void xmove(State * from, State * to, int n);

        /// <summary>
        /// Generated from [ref:lua_isnumber]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_isnumber", CallingConvention = CallingConvention.Cdecl)]
        public static extern int isnumber(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_isstring]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_isstring", CallingConvention = CallingConvention.Cdecl)]
        public static extern int isstring(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_iscfunction]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_iscfunction", CallingConvention = CallingConvention.Cdecl)]
        public static extern int iscfunction(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_isuserdata]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_isuserdata", CallingConvention = CallingConvention.Cdecl)]
        public static extern int isuserdata(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_type]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_type", CallingConvention = CallingConvention.Cdecl)]
        public static extern int type(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_typename]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_typename", CallingConvention = CallingConvention.Cdecl)]
        public static extern String typename(State * L, int tp);

        /// <summary>
        /// Generated from [ref:lua_equal]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_equal", CallingConvention = CallingConvention.Cdecl)]
        public static extern int equal(State * L, int idx1, int idx2);

        /// <summary>
        /// Generated from [ref:lua_rawequal]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_rawequal", CallingConvention = CallingConvention.Cdecl)]
        public static extern int rawequal(State * L, int idx1, int idx2);

        /// <summary>
        /// Generated from [ref:lua_lessthan]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_lessthan", CallingConvention = CallingConvention.Cdecl)]
        public static extern int lessthan(State * L, int idx1, int idx2);

        /// <summary>
        /// Generated from [ref:lua_tonumber]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_tonumber", CallingConvention = CallingConvention.Cdecl)]
        public static extern double tonumber(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_tointeger]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_tointeger", CallingConvention = CallingConvention.Cdecl)]
        public static extern int tointeger(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_toboolean]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_toboolean", CallingConvention = CallingConvention.Cdecl)]
        public static extern int toboolean(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_tolstring]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_tolstring", CallingConvention = CallingConvention.Cdecl)]
        public static extern String tolstring(State * L, int idx, int * len);

        /// <summary>
        /// Generated from [ref:lua_objlen]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_objlen", CallingConvention = CallingConvention.Cdecl)]
        public static extern int objlen(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_tocfunction]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_tocfunction", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr tocfunction(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_touserdata]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_touserdata", CallingConvention = CallingConvention.Cdecl)]
        public static extern void * touserdata(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_tothread]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_tothread", CallingConvention = CallingConvention.Cdecl)]
        public static extern State * tothread(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_topointer]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_topointer", CallingConvention = CallingConvention.Cdecl)]
        public static extern void* topointer(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_pushnil]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_pushnil", CallingConvention = CallingConvention.Cdecl)]
        public static extern void pushnil(State * L);

        /// <summary>
        /// Generated from [ref:lua_pushnumber]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_pushnumber", CallingConvention = CallingConvention.Cdecl)]
        public static extern void pushnumber(State * L, double n);

        /// <summary>
        /// Generated from [ref:lua_pushinteger]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_pushinteger", CallingConvention = CallingConvention.Cdecl)]
        public static extern void pushinteger(State * L, int n);

        /// <summary>
        /// Generated from [ref:lua_pushlstring]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_pushlstring", CallingConvention = CallingConvention.Cdecl)]
        public static extern void pushlstring(State * L, String s, int l);

        /// <summary>
        /// Generated from [ref:lua_pushstring]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_pushstring", CallingConvention = CallingConvention.Cdecl)]
        public static extern void pushstring(State * L, String s);

        /// <summary>
        /// Generated from [ref:lua_pushfstring]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_pushfstring", CallingConvention = CallingConvention.Cdecl)]
        public static extern String pushfstring(State * L, String fmt);

        /// <summary>
        /// Generated from [ref:lua_pushcclosure]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_pushcclosure", CallingConvention = CallingConvention.Cdecl)]
        public static extern void pushcclosure(State * L, IntPtr fn, int n);

        /// <summary>
        /// Generated from [ref:lua_pushboolean]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_pushboolean", CallingConvention = CallingConvention.Cdecl)]
        public static extern void pushboolean(State * L, int b);

        /// <summary>
        /// Generated from [ref:lua_pushlightuserdata]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_pushlightuserdata", CallingConvention = CallingConvention.Cdecl)]
        public static extern void pushlightuserdata(State * L, void * p);

        /// <summary>
        /// Generated from [ref:lua_pushthread]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_pushthread", CallingConvention = CallingConvention.Cdecl)]
        public static extern int pushthread(State * L);

        /// <summary>
        /// Generated from [ref:lua_gettable]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_gettable", CallingConvention = CallingConvention.Cdecl)]
        public static extern void gettable(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_getfield]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_getfield", CallingConvention = CallingConvention.Cdecl)]
        public static extern void getfield(State * L, int idx, String k);

        /// <summary>
        /// Generated from [ref:lua_rawget]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_rawget", CallingConvention = CallingConvention.Cdecl)]
        public static extern void rawget(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_rawgeti]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_rawgeti", CallingConvention = CallingConvention.Cdecl)]
        public static extern void rawgeti(State * L, int idx, int n);

        /// <summary>
        /// Generated from [ref:lua_createtable]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_createtable", CallingConvention = CallingConvention.Cdecl)]
        public static extern void createtable(State * L, int narr, int nrec);

        /// <summary>
        /// Generated from [ref:lua_newuserdata]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_newuserdata", CallingConvention = CallingConvention.Cdecl)]
        public static extern void * newuserdata(State * L, int sz);

        /// <summary>
        /// Generated from [ref:lua_getmetatable]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_getmetatable", CallingConvention = CallingConvention.Cdecl)]
        public static extern int getmetatable(State * L, int objindex);

        /// <summary>
        /// Generated from [ref:lua_getfenv]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_getfenv", CallingConvention = CallingConvention.Cdecl)]
        public static extern void getfenv(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_settable]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_settable", CallingConvention = CallingConvention.Cdecl)]
        public static extern void settable(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_setfield]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_setfield", CallingConvention = CallingConvention.Cdecl)]
        public static extern void setfield(State * L, int idx, String k);

        /// <summary>
        /// Generated from [ref:lua_rawset]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_rawset", CallingConvention = CallingConvention.Cdecl)]
        public static extern void rawset(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_rawseti]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_rawseti", CallingConvention = CallingConvention.Cdecl)]
        public static extern void rawseti(State * L, int idx, int n);

        /// <summary>
        /// Generated from [ref:lua_setmetatable]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_setmetatable", CallingConvention = CallingConvention.Cdecl)]
        public static extern int setmetatable(State * L, int objindex);

        /// <summary>
        /// Generated from [ref:lua_setfenv]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_setfenv", CallingConvention = CallingConvention.Cdecl)]
        public static extern int setfenv(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_call]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_call", CallingConvention = CallingConvention.Cdecl)]
        public static extern void call(State * L, int nargs, int nresults);

        /// <summary>
        /// Generated from [ref:lua_pcall]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_pcall", CallingConvention = CallingConvention.Cdecl)]
        public static extern int pcall(State * L, int nargs, int nresults, int errfunc);

        /// <summary>
        /// Generated from [ref:lua_cpcall]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_cpcall", CallingConvention = CallingConvention.Cdecl)]
        public static extern int cpcall(State * L, IntPtr func, void * ud);

        /// <summary>
        /// Generated from [ref:lua_yield]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_yield", CallingConvention = CallingConvention.Cdecl)]
        public static extern int yield(State * L, int nresults);

        /// <summary>
        /// Generated from [ref:lua_resume]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_resume", CallingConvention = CallingConvention.Cdecl)]
        public static extern int resume(State * L, int narg);

        /// <summary>
        /// Generated from [ref:lua_status]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_status", CallingConvention = CallingConvention.Cdecl)]
        public static extern int status(State * L);

        /// <summary>
        /// Generated from [ref:lua_gc]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_gc", CallingConvention = CallingConvention.Cdecl)]
        public static extern int gc(State * L, int what, int data);

        /// <summary>
        /// Generated from [ref:lua_error]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_error", CallingConvention = CallingConvention.Cdecl)]
        public static extern int error(State * L);

        /// <summary>
        /// Generated from [ref:lua_next]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_next", CallingConvention = CallingConvention.Cdecl)]
        public static extern int next(State * L, int idx);

        /// <summary>
        /// Generated from [ref:lua_concat]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_concat", CallingConvention = CallingConvention.Cdecl)]
        public static extern void concat(State * L, int n);

        /// <summary>
        /// Generated from [ref:lua_setlevel]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_setlevel", CallingConvention = CallingConvention.Cdecl)]
        public static extern void setlevel(State * from, State * to);

        /// <summary>
        /// Generated from [ref:lua_getupvalue]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_getupvalue", CallingConvention = CallingConvention.Cdecl)]
        public static extern String getupvalue(State * L, int funcindex, int n);

        /// <summary>
        /// Generated from [ref:lua_setupvalue]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_setupvalue", CallingConvention = CallingConvention.Cdecl)]
        public static extern String setupvalue(State * L, int funcindex, int n);

        /// <summary>
        /// Generated from [ref:lua_gethookmask]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_gethookmask", CallingConvention = CallingConvention.Cdecl)]
        public static extern int gethookmask(State * L);

        /// <summary>
        /// Generated from [ref:lua_gethookcount]
        /// </summary>
        [DllImport("luajit-5.1", EntryPoint="lua_gethookcount", CallingConvention = CallingConvention.Cdecl)]
        public static extern int gethookcount(State * L);

    } // Lua
} // dmSDK.Lua

