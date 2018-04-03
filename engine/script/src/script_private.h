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

    typedef struct TimerContext* HTimerContext;
    
    #define DM_SCRIPT_MAX_EXTENSIONS (sizeof(uint32_t) * 8 * 16)
    struct Context
    {
        dmConfigFile::HConfig   m_ConfigFile;
        dmResource::HFactory    m_ResourceFactory;
        HTimerContext           m_TimerContext;
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

    typedef void (*TimerTrigger)(HContext context, HTimer timer_id, float time_elapsed, uintptr_t owner, int self_ref, int callback_ref);

    /**
     * Adds a timer and returns a unique id within the timer_context
     * You may add a timer from inside a timer callback
     * Using a delay of 0.0f will result in a timer that triggers at the next call to UpdateTimerContext
     * If you want a timer that triggers on each UpdateTimerContext, set delay to 0.0f and repeat to true
     * 
     * The parameters owner, self_ref and callback_ref are just passed on to the trigger callback and are
     * not used by the timer implementation.
     * 
     * @param timer_context the timer context
     * @param delay the time to wait in same unit as used for time step with UpdateTimerContext
     * @param repeat indicates if the timer should reset at trigger or die
     * @param timer_trigger the callback to call when the timer triggers
     * @param owner used to group timers for fast removal of associated timers
     * @param self_ref lua reference to self
     * @param callback_ref lua reference to a callback
     * @return the timer id, returns INVALID_TIMER_ID if the timer can not be created
     */
    HTimer AddTimer(HContext context,
                            float delay,
                            bool repeat,
                            TimerTrigger timer_trigger,
                            uintptr_t owner,
                            int self_ref,
                            int callback_ref);

    /**
     * Get the number of alive timers for a timer_context, currently only useful for unit tests
     * You may query the number of alive timers from inside a timer callback
     * 
     * @param timer_context the timer context
     * @return number of active timers that was cancelled
     */
    uint32_t GetAliveTimers(HContext context);
}

#endif // SCRIPT_PRIVATE_H
