#ifndef DM_GUI_SCRIPT_H
#define DM_GUI_SCRIPT_H

extern "C"
{
#include "lua/lua.h"
#include "lua/lauxlib.h"
}

namespace dmGui
{
    lua_State* InitializeScript();
    void FinalizeScript(lua_State* L);
}

#endif
