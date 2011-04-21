#ifndef DM_SCRIPT_MSG_H
#define DM_SCRIPT_MSG_H

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    void InitializeMsg(lua_State* L);
}

#endif // DM_SCRIPT_MSG_H
