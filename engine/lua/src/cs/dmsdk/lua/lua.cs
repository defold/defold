using System.Runtime.InteropServices;
using System.Reflection.Emit;

namespace dmSDK.Lua {

    using LUA_NUMBER = double;
    using LUA_INTEGER = int;

    public unsafe partial class Lua
    {
        public static string LUA_VERSION        = "Lua 5.1";
        public static string LUA_RELEASE        = "Lua 5.1.4";
        public static int    LUA_VERSION_NUM    = 501;
        public static string LUA_COPYRIGHT      = "Copyright (C) 1994-2008 Lua.org, PUC-Rio";
        public static string LUA_AUTHORS        = "R. Ierusalimschy, L. H. de Figueiredo & W. Celes";

        /* mark for precompiled code (`<esc>Lua') */
        public static string LUA_SIGNATURE      =   "\033Lua";

        /* option for multiple returns in `lua_pcall' and `lua_call' */
        public static int LUA_MULTRET           = -1;

        /*
        ** pseudo-indices
        */
        public static int LUA_REGISTRYINDEX     = -10000;
        public static int LUA_ENVIRONINDEX      = -10001;
        public static int LUA_GLOBALSINDEX      = -10002;

        /*
        ** basic types
        */
        public static int LUA_TNONE             = -1;
        public static int LUA_TNIL              = 0;
        public static int LUA_TBOOLEAN          = 1;
        public static int LUA_TLIGHTUSERDATA    = 2;
        public static int LUA_TNUMBER           = 3;
        public static int LUA_TSTRING           = 4;
        public static int LUA_TTABLE            = 5;
        public static int LUA_TFUNCTION         = 6;
        public static int LUA_TUSERDATA         = 7;
        public static int LUA_TTHREAD           = 8;

        /*
        ** garbage-collection function and options
        */
        public static int LUA_GCSTOP            = 0;
        public static int LUA_GCRESTART         = 1;
        public static int LUA_GCCOLLECT         = 2;
        public static int LUA_GCCOUNT           = 3;
        public static int LUA_GCCOUNTB          = 4;
        public static int LUA_GCSTEP            = 5;
        public static int LUA_GCSETPAUSE        = 6;
        public static int LUA_GCSETSTEPMUL      = 7;

        /*
        ** ===============================================================
        ** some useful macros
        ** ===============================================================
        */

        public static void pop(State* L, int n)
        {
            settop(L, -(n)-1);
        }

        public static void newtable(State* L)
        {
            createtable(L, 0, 0);
        }


        // #define lua_register(L,n,f) (lua_pushcfunction(L, (f)), lua_setglobal(L, (n)))
        // #define lua_pushcfunction(L,f)  lua_pushcclosure(L, (f), 0)
        // #define lua_strlen(L,i)     lua_objlen(L, (i))

        public static bool isfunction(State* L, int n)
        {
            return Lua.type(L, n) == LUA_TFUNCTION;
        }

        public static bool istable(State* L, int n)
        {
            return Lua.type(L, n) == LUA_TTABLE;
        }

        public static bool islightuserdata(State* L, int n)
        {
            return Lua.type(L, n) == LUA_TLIGHTUSERDATA;
        }

        public static bool isnil(State* L, int n)
        {
            return Lua.type(L, n) == LUA_TNIL;
        }

        public static bool isboolean(State* L, int n)
        {
            return Lua.type(L, n) == LUA_TBOOLEAN;
        }

        public static bool isthread(State* L, int n)
        {
            return Lua.type(L, n) == LUA_TTHREAD;
        }

        public static bool isnone(State* L, int n)
        {
            return Lua.type(L, n) == LUA_TNONE;
        }

        public static bool isnoneornil(State* L, int n)
        {
            return Lua.type(L, n) <= 0;
        }

        public static void pushliteral(State* L, string s)
        {
            Lua.pushstring(L, s);
        }

        public static void setglobal(State* L, string s)
        {
            Lua.setfield(L, LUA_GLOBALSINDEX, s);
        }

        public static void getglobal(State* L, string s)
        {
            Lua.getfield(L, LUA_GLOBALSINDEX, s);
        }

        public static string tostring(State* L, int n)
        {
            return Lua.tolstring(L, n, (int*)0);
        }
    }
}
