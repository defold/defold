#ifndef DM_GUI_SCRIPT_H
#define DM_GUI_SCRIPT_H

#include <script/script.h>

extern "C"
{
#include "lua/lua.h"
#include "lua/lauxlib.h"
}

namespace dmGui
{
    lua_State* InitializeScript(dmScript::HContext script_context);
    void FinalizeScript(lua_State* L, dmScript::HContext script_context);
}

#endif
