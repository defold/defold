#ifndef __GAMEOBJECTSCRIPT_H__
#define __GAMEOBJECTSCRIPT_H__

#include <dlib/array.h>

#include <script/script.h>

#include <resource/resource.h>
#include "gameobject.h"
#include "gameobject_props.h"

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

    struct UnresolvedURL
    {
        dmMessage::URL m_URL;
        const char* m_UnresolvedPath;
    };

    struct ScriptPropertyDef
    {
        const char* m_Name;
        dmhash_t m_Id;
        dmGameObjectDDF::PropertyType m_Type;
        union
        {
            double m_Number;
            dmhash_t m_Hash;
            UnresolvedURL m_URL;
            float m_V4[4];
        };
    };

    struct Script
    {
        dmArray<ScriptPropertyDef> m_PropertyDefs;
        // Used for reloading scripts
        dmArray<ScriptPropertyDef> m_OldPropertyDefs;
        int m_FunctionReferences[MAX_SCRIPT_FUNCTION_COUNT];
        PropertyData m_PropertyData;
    };

    typedef Script* HScript;

    struct ScriptInstance
    {
        HScript     m_Script;
        Instance*   m_Instance;
        int         m_InstanceReference;
        int         m_ScriptDataReference;
        uint8_t     m_ComponentIndex;
        HProperties m_Properties;
    };

    struct ScriptWorld
    {
        ScriptWorld();

        dmArray<ScriptInstance*> m_Instances;
    };

    void    InitializeScript(dmScript::HContext context, dmResource::HFactory factory);
    void    FinalizeScript(dmResource::HFactory factory);

    HScript NewScript(const void* buffer, uint32_t buffer_size, const char* filename);
    bool    ReloadScript(HScript script, const void* buffer, uint32_t buffer_size, const char* filename);
    void    DeleteScript(HScript script);

    HScriptInstance NewScriptInstance(HScript script, HInstance instance, uint8_t component_index);
    void            DeleteScriptInstance(HScriptInstance script_instance);

    extern lua_State* g_LuaState;
    extern dmScript::HContext g_ScriptContext;

    void ClearPropertiesFromLuaTable(const dmArray<ScriptPropertyDef>& property_defs, lua_State* L, int index);
    void PropertiesToLuaTable(HInstance instance, const dmArray<ScriptPropertyDef>& property_defs, const HProperties properties, lua_State* L, int index);
}

#endif //__GAMEOBJECTSCRIPT_H__
