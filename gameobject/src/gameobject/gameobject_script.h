#ifndef __GAMEOBJECTSCRIPT_H__
#define __GAMEOBJECTSCRIPT_H__

#include <dlib/array.h>

#include <script/script.h>

#include <resource/resource.h>
#include "gameobject.h"

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
        int m_FunctionReferences[MAX_SCRIPT_FUNCTION_COUNT];
    };
    typedef Script* HScript;

    struct ScriptInstance
    {
        HScript     m_Script;
        Instance*   m_Instance;
        int         m_InstanceReference;
        int         m_ScriptDataReference;
        uint8_t     m_ComponentIndex;
    };

    struct ScriptWorld
    {
        ScriptWorld();

        dmArray<ScriptInstance*> m_Instances;
    };

    void    InitializeScript(dmScript::HContext context);
    void    FinalizeScript();

    HScript NewScript(const void* buffer, uint32_t buffer_size, const char* filename);
    bool    ReloadScript(HScript script, const void* buffer, uint32_t buffer_size, const char* filename);
    void    DeleteScript(HScript script);

    HScriptInstance NewScriptInstance(HScript script, HInstance instance, uint8_t component_index);
    void            DeleteScriptInstance(HScriptInstance script_instance);
}

#endif //__GAMEOBJECTSCRIPT_H__
