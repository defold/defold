#ifndef DM_SCRIPT_HTML5_H
#define DM_SCRIPT_HTML5_H

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    void InitializeHtml5(lua_State* L);
}

#endif // DM_SCRIPT_HTML5_H
