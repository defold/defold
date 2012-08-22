#ifndef DM_SCRIPT_MODULE
#define DM_SCRIPT_MODULE

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    void InitializeModule(lua_State* L);
}

#endif // DM_SCRIPT_MODULE
