#include "script.h"

#include <dlib/sys.h>
#include <dlib/log.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript
{
    /*# Profiler API documentation
     *
     * Functions for getting profiling data
     *
     * @document
     * @name Profiler
     * @namespace profiler
     */

#define SCRIPT_LIB_NAME "profiler"

    static int Profiler_MemoryUsage(lua_State* L)
    {
        int top = lua_gettop(L);

        dmSys::MemoryInfo mem_info;
        dmSys::GetMemoryInfo(&mem_info);

        lua_pushnumber(L, mem_info.m_Usage);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static int Profiler_CPUUsage(lua_State* L)
    {
        int top = lua_gettop(L);

        dmSys::CPUInfo cpu_info;
        dmSys::GetCPUInfo(&cpu_info);

        lua_pushnumber(L, cpu_info.m_Usage);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /////////////////////////////////////////////////////////////////////////////
    static const luaL_reg Module_methods[] =
    {
        {"memory_usage", Profiler_MemoryUsage},
        {"cpu_usage", Profiler_CPUUsage},
        {0, 0}
    };

    void InitializeProfiler(lua_State* L)
    {
        int top = lua_gettop(L);
        luaL_register(L, SCRIPT_LIB_NAME, Module_methods);
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
    }

}
