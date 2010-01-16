#ifndef __GAMEOBJECTSCRIPT_H__
#define __GAMEOBJECTSCRIPT_H__

/**
 * Private header for GameObject
 */

namespace dmGameObject
{
    struct Instance;
    struct UpdateContext;
    typedef Instance* HInstance;

    struct ScriptInstance
    {
        Instance* m_Instance;
        int       m_InstanceReference;
        int       m_ScriptDataReference;
    };
    typedef ScriptInstance* HScriptInstance;

    struct Script
    {
        int m_FunctionsReference;
    };
    typedef Script* HScript;

    void    InitializeScript();
    void    FinalizeScript();

    HScript NewScript(const void* memory);
    void    DeleteScript(HScript script);

    HScriptInstance NewScriptInstance(HInstance instance);
    void            DeleteScriptInstance(HScriptInstance script_instance);

    bool    RunScript(HScript script, const char* function_name, HScriptInstance script_instance, const UpdateContext* update_context);
}

#endif //__GAMEOBJECTSCRIPT_H__
