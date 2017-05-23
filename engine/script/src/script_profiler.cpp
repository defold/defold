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
     * Functions for getting profiling data in runtime.
     * More detailed profiling and debugging information can be found under the [Debugging](http://www.defold.com/manuals/debugging/) section in the manuals.
     *
     * @document
     * @name Profiler
     * @namespace profiler
     */

#define SCRIPT_LIB_NAME "profiler"

    /*# get current memory usage for app reported by OS
     * Get the amount of memory used (resident/working set) by the application in bytes, as reported by the OS. (**Not available on HTML5.**)
     *
     * The values are gathered from internal OS functions which correspond to the following;
     *
     * OS                                | Value
     * ----------------------------------|------------------
     * iOS, OSX, Android and Linux       | [Resident memory](https://en.wikipedia.org/wiki/Resident_set_size)
     * Windows                           | [Working set](https://en.wikipedia.org/wiki/Working_set)
     * HTML5                             | **Not available**
     *
     * @name profiler.memory_usage
     * @return bytes [type:number] used by the application
     * @examples
     *
     * Get memory usage before and after loading a collection:
     *
     * ```lua
     * print(profiler.memory_usage())
     * msg.post("#collectionproxy", "load")
     * ...
     * print(profiler.memory_usage()) -- will report a higher number than the initial call
     * ```
     */
    static int Profiler_MemoryUsage(lua_State* L)
    {
        int top = lua_gettop(L);

        dmSys::MemoryInfo mem_info;
        dmSys::GetMemoryInfo(&mem_info);

        lua_pushnumber(L, mem_info.m_Usage);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# get current CPU usage for app reported by OS
     * Get the percent of CPU usage by the application, as reported by the OS. (**Not available on HTML5.**)
     *
     * For some platforms (Android, Linux and Windows), this information is only available
     * by default in the debug version of the engine. It can be enabled in release version as well
     * by checking `track_cpu` under `profiler` in the `game.project` file.
     * (This means that the engine will sample the CPU usage in intervalls during execution even in release mode.)
     *
     * @name profiler.cpu_usage
     * @return percent [type:number] of CPU used by the application
     */
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
