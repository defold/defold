#ifndef DM_SCRIPT_PICKLE_H
#define DM_SCRIPT_PICKLE_H

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    void InitializePickle(lua_State* L);
}

#endif // DM_SCRIPT_PICKLE_H
