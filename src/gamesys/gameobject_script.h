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

    struct Script
    {
        int m_FunctionsReference;
    };
    typedef Script* HScript;

    struct ScriptInstance
    {
        HScript   m_Script;
        Instance* m_Instance;
        int       m_InstanceReference;
        int       m_ScriptDataReference;
    };

    void    InitializeScript();
    void    FinalizeScript();

    HScript NewScript(const void* buffer, uint32_t buffer_size, const char* filename);
    bool    ReloadScript(HScript script, const void* buffer, uint32_t buffer_size, const char* filename);
    void    DeleteScript(HScript script);

    HScriptInstance NewScriptInstance(HScript script, HInstance instance);
    void            DeleteScriptInstance(HScriptInstance script_instance);

    bool    RunScript(HCollection collection, HScript script, const char* function_name, HScriptInstance script_instance, const UpdateContext* update_context);
    bool    DispatchScriptEventsFunction(dmEvent::Event *event_object, const UpdateContext* m_UpdateContext);
}

#endif //__GAMEOBJECTSCRIPT_H__
