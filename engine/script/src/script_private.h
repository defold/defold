#ifndef SCRIPT_PRIVATE_H
#define SCRIPT_PRIVATE_H

#include <dlib/hashtable.h>

#define SCRIPT_CONTEXT "__script_context"
#define SCRIPT_MAIN_THREAD "__script_main_thread"
#define SCRIPT_ERROR_HANDLER_VAR "__error_handler"

namespace dmScript
{
    struct Module
    {
        char*       m_Script;
        uint32_t    m_ScriptSize;
        char*       m_Name;
        void*       m_Resource;
    };

    #define DM_SCRIPT_MAX_EXTENSIONS (sizeof(uint32_t) * 8 * 16)
    struct Context
    {
        dmConfigFile::HConfig   m_ConfigFile;
        dmResource::HFactory    m_ResourceFactory;
        dmHashTable64<Module>   m_Modules;
        dmHashTable64<Module*>  m_PathToModule;
        dmHashTable64<int>      m_HashInstances;
        lua_State*              m_LuaState;
        bool                    m_EnableExtensions;
        uint32_t                m_InitializedExtensions[DM_SCRIPT_MAX_EXTENSIONS / (8 * sizeof(uint32_t))];
    };

    bool ResolvePath(lua_State* L, const char* path, uint32_t path_size, dmhash_t& out_hash);

    bool GetURL(lua_State* L, dmMessage::URL& out_url);

    bool GetUserData(lua_State* L, uintptr_t& out_user_data, const char* user_type);

    bool IsValidInstance(lua_State* L);

    /**
     * Remove all modules.
     * @param context script context
     */
    void ClearModules(HContext context);

    typedef struct TimerContext* HTimerContext;

    /**
     * Get the number of alive timers for a timer_context, currently only useful for unit tests
     * You may query the number of alive timers from inside a timer callback
     * 
     * @param timer_context the timer context
     * @return number of active timers that was cancelled
     */
    uint32_t GetAliveTimers(HTimerContext timer_context);
}

#endif // SCRIPT_PRIVATE_H
