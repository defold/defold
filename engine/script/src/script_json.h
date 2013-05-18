#ifndef DM_SCRIPT_JSON_H
#define DM_SCRIPT_JSON_H

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    void InitializeJson(lua_State* L);
}

#endif // DM_SCRIPT_JSON_H
