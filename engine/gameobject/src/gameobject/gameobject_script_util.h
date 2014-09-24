#ifndef DM_GAMEOBJECT_SCRIPT_UTIL_H
#define DM_GAMEOBJECT_SCRIPT_UTIL_H

namespace dmGameObject
{
    bool RegisterSubModules(dmResource::HFactory factory, dmScript::HContext script_context, dmLuaDDF::LuaModule* lua_module);
    void FreeModules(dmResource::HFactory factory, dmScript::HContext script_context);
}

#endif // #ifndef DM_GAMEOBJECT_SCRIPT_UTIL_H
