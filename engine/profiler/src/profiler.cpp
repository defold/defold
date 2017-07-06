#include <dlib/dlib.h>
#include <dlib/profile.h>
#include <dlib/log.h>
#include <extension/extension.h>
#include "profiler_private.h"

static bool g_TrackCpuUsage = false;

/*# Profiler API documentation
 *
 * Functions for getting profiling data in runtime.
 * More detailed profiling and debugging information can be found under the [Debugging](http://www.defold.com/manuals/debugging/) section in the manuals.
 *
 * @document
 * @name Profiler
 * @namespace profiler
 */

/*# get current memory usage for app reported by OS
 * Get the amount of memory used (resident/working set) by the application in bytes, as reported by the OS.
 *
 * [icon:attention] This function is not available on [icon:html5] HTML5.
 *
 * The values are gathered from internal OS functions which correspond to the following;
 *
 * OS                                | Value
 * ----------------------------------|------------------
 * [icon:ios] iOS<br/>[icon:macos] MacOS<br/>[icon:android]<br/>Androd<br/>[icon:linux] Linux | [Resident memory](https://en.wikipedia.org/wiki/Resident_set_size)
 * [icon:windows] Windows            | [Working set](https://en.wikipedia.org/wiki/Working_set)
 * [icon:html5] HTML5                | [icon:attention] Not available
 *
 * @name profiler.get_memory_usage
 * @return bytes [type:number] used by the application
 * @examples
 *
 * Get memory usage before and after loading a collection:
 *
 * ```lua
 * print(profiler.get_memory_usage())
 * msg.post("#collectionproxy", "load")
 * ...
 * print(profiler.get_memory_usage()) -- will report a higher number than the initial call
 * ```
 */
static int Profiler_MemoryUsage(lua_State* L)
{
    int top = lua_gettop(L);
    lua_pushnumber(L, dmProfilerExt::GetMemoryUsage());
    assert(top + 1 == lua_gettop(L));
    return 1;
}

/*# get current CPU usage for app reported by OS
 * Get the percent of CPU usage by the application, as reported by the OS.
 *
 * [icon:attention] This function is not available on [icon:html5] HTML5.
 *
 * For some platforms ([icon:android] Android, [icon:linux] Linux and [icon:windows] Windows), this information is only available
 * by default in the debug version of the engine. It can be enabled in release version as well
 * by checking `track_cpu` under `profiler` in the `game.project` file.
 * (This means that the engine will sample the CPU usage in intervalls during execution even in release mode.)
 *
 * @name profiler.get_cpu_usage
 * @return percent [type:number] of CPU used by the application
 */
static int Profiler_CPUUsage(lua_State* L)
{
    int top = lua_gettop(L);
    lua_pushnumber(L, dmProfilerExt::GetCpuUsage());
    assert(top + 1 == lua_gettop(L));
    return 1;
}

static dmExtension::Result InitializeProfiler(dmExtension::Params* params)
{
    // Should CPU sample tracking be run each step?
    // This is enabled by default in debug, but can be turned on in release via project config.
    g_TrackCpuUsage = dLib::IsDebugMode();
    if (dmConfigFile::GetInt(params->m_ConfigFile, "profiler.track_cpu", 0) == 1)
    {
        g_TrackCpuUsage = true;
    }

    static const luaL_reg Module_methods[] =
    {
        {"get_memory_usage", Profiler_MemoryUsage},
        {"get_cpu_usage", Profiler_CPUUsage},
        {0, 0}
    };

    luaL_register(params->m_L, "profiler", Module_methods);
    lua_pop(params->m_L, 1);

    return dmExtension::RESULT_OK;
}

static dmExtension::Result UpdateProfiler(dmExtension::Params* params)
{
    if (g_TrackCpuUsage)
    {
        dmProfilerExt::SampleCpuUsage();
    }

    if (dLib::IsDebugMode()) {
        DM_COUNTER("CPU Usage", dmProfilerExt::GetCpuUsage()*100.0);
        DM_COUNTER("Mem Usage", dmProfilerExt::GetMemoryUsage());
    }

    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeProfiler(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppInitializeProfiler(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeProfiler(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(ProfilerExt, "Profiler", AppInitializeProfiler, AppFinalizeProfiler, InitializeProfiler, UpdateProfiler, 0, FinalizeProfiler)
