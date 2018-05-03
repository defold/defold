#ifndef DM_SCRIPT_TIMER_H
#define DM_SCRIPT_TIMER_H

#include <stdint.h>

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    typedef struct Context* HContext;
    typedef struct TimerContext* HTimerContext;

    typedef uint32_t HTimer;

    HTimerContext NewTimerContext(uint16_t max_owner_count);
    void DeleteTimerContext(HTimerContext timer_context);
    void InitializeTimer(lua_State* L);

    const HTimer INVALID_TIMER_ID = 0xffffffffu;

    /**
     * Update the a timer context. Any timers whose time is elapsed will be triggered
     * The resolution of all timers are dictated to the time step used when calling UpdateTimerContext
     * 
     * @param timer_context the timer context
     * @param dt time step during which to simulate
     */
    void UpdateTimerContext(HContext context, float dt);

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
     * @param owner used to group timers for fast removal of associated timers
     * @param self_ref lua reference to self, will be unrefd when timer dies (use LUA_NOREF for no cleanup)
     * @param callback_ref lua reference to a callback, will be unrefd when timer dies (use LUA_NOREF for no cleanup)
     * @return the timer id, returns INVALID_TIMER_ID if the timer can not be created
     */
    HTimer AddTimer(HContext context,
                            float delay,
                            bool repeat,
                            uintptr_t owner,
                            int self_ref,
                            int callback_ref);

    /**
     * Cancel a timer
     * You may cancel a timer from inside a timer callback, cancelling a timer that is already
     * executed or cancelled is safe.
     * 
     * @param timer_context the timer context
     * @param id the timer id
     * @return true if the timer was active, false if the timer is already cancelled / complete
     */
    bool CancelTimer(HContext context, HTimer id);

    /**
     * Cancels all active timers assiciated with a script_context
     * You may cancel a timers from inside a timer callback
     * 
     * @param timer_context the timer context
     * @param owner the owner associated with the timers that should be cancelled
     * @return number of active timers that was cancelled
     */
    uint32_t CancelTimers(HContext context, uintptr_t owner);


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
     * @param self_ref lua reference to self, will be unrefd when timer dies (use LUA_NOREF for no cleanup)
     * @param callback_ref lua reference to a callback, will be unrefd when timer dies (use LUA_NOREF for no cleanup)
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

#endif
