#ifndef DM_GAMEOBJECT_SCRIPT_UTIL_H
#define DM_GAMEOBJECT_SCRIPT_UTIL_H

#include <gameobject/gameobject.h>

namespace dmGameObject
{
    bool RegisterSubModules(dmResource::HFactory factory, dmScript::HContext script_context, dmLuaDDF::LuaModule* lua_module);
    Result LuaLoad(dmResource::HFactory factory, dmScript::HContext context, dmLuaDDF::LuaModule* module);
}

#endif // #ifndef DM_GAMEOBJECT_SCRIPT_UTIL_H
