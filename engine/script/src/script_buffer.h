#ifndef DM_SCRIPT_BUFFER_H
#define DM_SCRIPT_BUFFER_H

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    void InitializeBuffer(lua_State* L);
}

#endif // DM_SCRIPT_BUFFER_H
