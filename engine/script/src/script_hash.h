#ifndef DM_SCRIPT_HASH_H
#define DM_SCRIPT_HASH_H

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    void InitializeHash(lua_State* L);
}

#endif // DM_SCRIPT_HASH_H
