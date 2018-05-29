#ifndef DM_SCRIPT_TIMER_PRIVATE_H
#define DM_SCRIPT_TIMER_PRIVATE_H

#include "script_timer.h"

#include <stdint.h>

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    typedef struct TimerWorld* HTimerWorld;

    typedef uint32_t HTimer;

    HTimerWorld NewTimerWorld(uint16_t max_owner_count);
    void DeleteTimerWorld(HTimerWorld timer_world);

    typedef struct ScriptWorld* HScriptWorld;

    void TimerNewScriptWorld(HScriptWorld script_world);
    void TimerDeleteScriptWorld(HScriptWorld script_world);
    void TimerUpdateScriptWorld(HScriptWorld script_world, float dt);

    void TimerInitializeInstance(lua_State* L, HScriptWorld script_world);
    void TimerFinalizeInstance(lua_State* L, HScriptWorld script_world);

    const HTimer INVALID_TIMER_ID = 0xffffffffu;

    /**
     * Update the all the timers in the world. Any timers whose time is elapsed will be triggered
     * The resolution of all timers are dictated to the time step used when calling UpdateTimers
     * 
     * @param timer_world the timer world created with NewTimerWorld
     * @param dt time step during which to simulate
     */
    void UpdateTimers(HTimerWorld timer_world, float dt);

    /**
     * Cancel a timer
     * You may cancel a timer from inside a timer callback, cancelling a timer that is already
     * executed or cancelled is safe.
     * 
     * If the timer is live the timer callback will be called with the TIMER_EVENT_CANCELLED event
     * 
     * @param timer_world the timer world created with NewTimerWorld
     * @param id the timer id
     * @return true if the timer was active, false if the timer is already cancelled / complete
     */
    bool CancelTimer(HTimerWorld timer_world, HTimer id);

    /**
     * Removes all active timers assiciated with an owner without calling the
     * timer callback (no TIMER_EVENT_CANCELLED will be received)
     * 
     * @param timer_world the timer world created with NewTimerWorld
     * @param owner the owner associated with the timers that should be killed
     * @return number of active timers that was killed
     */
    uint32_t KillTimers(HTimerWorld timer_world, uintptr_t owner);

    enum TimerEventType
    {
        TIMER_EVENT_TRIGGER_WILL_DIE,
        TIMER_EVENT_TRIGGER_WILL_REPEAT,
        TIMER_EVENT_CANCELLED
    };

    typedef void (*TimerCallback)(HTimerWorld timer_world, TimerEventType event_type, HTimer timer_id, float time_elapsed, uintptr_t owner, uintptr_t userdata);

    /**
     * Adds a timer and returns a unique id within the timer_world
     * You may add a timer from inside a timer callback
     * Using a delay of 0.0f will result in a timer that triggers at the next call to UpdateTimers
     * If you want a timer that triggers on each UpdateTimers, set delay to 0.0f and repeat to true
     * 
     * The parameters owner and userdata are just passed on to the trigger callback and are
     * not used by the timer implementation.
     * 
     * @param timer_world the timer world created with NewTimerWorld
     * @param delay the time to wait in same unit as used for time step with UpdateTimers
     * @param repeat indicates if the timer should reset at trigger or die
     * @param timer_trigger the callback to call when the timer triggers
     * @param owner used to group timers for fast removal of associated timers
     * @param userdata user data associated with the timer
     * @return the timer id, returns INVALID_TIMER_ID if the timer can not be created
     */
    HTimer AddTimer(HTimerWorld timer_world,
                            float delay,
                            bool repeat,
                            TimerCallback timer_callback,
                            uintptr_t owner,
                            uintptr_t userdata);

    bool IsTimerAlive(HTimerWorld timer_world, HTimer timer);

    /**
     * Get the number of alive timers for a timer_world, currently only useful for unit tests
     * You may query the number of alive timers from inside a timer callback
     * 
     * @param timer_world the timer world created with NewTimerWorld
     * @return number of active timers that was cancelled
     */
    uint32_t GetAliveTimers(HTimerWorld timer_world);
}

#endif
