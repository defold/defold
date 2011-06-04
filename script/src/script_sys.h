#ifndef DM_SCRIPT_SYS_H
#define DM_SCRIPT_SYS_H

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    void InitializeSys(lua_State* L);
    void FinalizeSys(lua_State* L);
    void UpdateSys(lua_State*L, float dt);
}

#endif // DM_SCRIPT_SYS_H
