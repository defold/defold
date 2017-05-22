#ifndef DM_SCRIPT_PROFILER_H
#define DM_SCRIPT_PROFILER_H

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    void InitializeProfiler(lua_State* L);
}

#endif // DM_SCRIPT_PROFILER_H
