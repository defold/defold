#ifndef DM_SCRIPT_IMAGE_H
#define DM_SCRIPT_IMAGE_H

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    void InitializeImage(lua_State* L);
}

#endif // DM_SCRIPT_IMAGE_H
