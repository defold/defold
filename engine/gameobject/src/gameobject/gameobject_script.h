#ifndef __GAMEOBJECTSCRIPT_H__
#define __GAMEOBJECTSCRIPT_H__

#include <dlib/array.h>

#include <script/script.h>

#include <resource/resource.h>
#include "gameobject.h"
#include "gameobject_props.h"

#include "../proto/gameobject/lua_ddf.h"

/**
 * Private header for GameObject
 */

namespace dmGameObject
{
    struct Instance;
    struct UpdateContext;
    typedef Instance* HInstance;

    enum ScriptResult
    {
        SCRIPT_RESULT_FAILED = -1,
        SCRIPT_RESULT_NO_FUNCTION = 0,
        SCRIPT_RESULT_OK = 1
    };

    enum ScriptFunction
    {
        SCRIPT_FUNCTION_INIT,
        SCRIPT_FUNCTION_FINAL,
        SCRIPT_FUNCTION_UPDATE,
        SCRIPT_FUNCTION_ONMESSAGE,
        SCRIPT_FUNCTION_ONINPUT,
        SCRIPT_FUNCTION_ONRELOAD,
        MAX_SCRIPT_FUNCTION_COUNT
    };

    extern const char* SCRIPT_FUNCTION_NAMES[MAX_SCRIPT_FUNCTION_COUNT];

    struct Script
    {
        lua_State*              m_LuaState;
        int                     m_FunctionReferences[MAX_SCRIPT_FUNCTION_COUNT];
        PropertySet             m_PropertySet;
        dmLuaDDF::LuaModule*    m_LuaModule;
        int                     m_InstanceReference;
        // Resources referenced through property values in the script
        dmArray<void*>          m_PropertyResources;
    };

    typedef Script* HScript;

    struct ScriptInstance
    {
        HScript     m_Script;
        Instance*   m_Instance;
        int         m_InstanceReference;
        int         m_ScriptDataReference;
        uint16_t    m_ComponentIndex;
        HProperties m_Properties;
        uint16_t    m_Update : 1;
        uint16_t    m_Padding : 15;
    };

    struct ScriptWorld
    {
        ScriptWorld();

        dmArray<ScriptInstance*> m_Instances;
    };

    void    InitializeScript(HRegister regist, dmScript::HContext context);

    HScript NewScript(lua_State* L, dmLuaDDF::LuaModule* lua_module);
    bool    ReloadScript(HScript script, dmLuaDDF::LuaModule* lua_module);
    void    DeleteScript(HScript script);

    HScriptInstance NewScriptInstance(HScript script, HInstance instance, uint16_t component_index);
    void            DeleteScriptInstance(HScriptInstance script_instance);

    PropertyResult PropertiesToLuaTable(HInstance instance, HScript script, const HProperties properties, lua_State* L, int index);
}

#endif //__GAMEOBJECTSCRIPT_H__
