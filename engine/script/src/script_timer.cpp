#include "script_timer.h"
#include "script_timer_private.h"

#include <string.h>
#include <dlib/index_pool.h>
#include <dlib/hashtable.h>
#include <dlib/profile.h>

#include "script.h"
#include "script_private.h"

namespace dmScript
{
    /*
        The timers are stored in a flat array with no holes which we scan sequenctially on each update.

        When a timer is removed the last timer in the list may change location (EraseSwap).

        This is to keep the array sweep fast and handle cases where multiple short lived timers are
        created followed by one long-lived timer. If we did not re-shuffle and keep holes we would keep
        scanning the array to the end where the only live timer exists skipping all the holes.
        How much of an issue this is is guesswork at this point.

        The timer identity is an index into an indirection layer combined with a generation counter,
        this makes it possible to reuse the index for the indirection layer without risk of using
        stale indexes - the caller to CancelTimer is allowed to call with an handle of a timer that already
        has expired.

        We also keep track of all timers associated with a specific owner, we do this by
        creating a linked list (using indirected lookup indexes) inside the timer array and just
        holding the most recently added timer to a specific owner in a hash table for fast lookup.

        This is to avoid scanning the entire timer array for a specific owner for items to remove.

        Each game object needs to call KillTimers for its owner to clean up potential timers
        that has not yet been cancelled or completed (one-shot).
        We don't want each GO to scan the entire array for timers at destuction.
        The m_OwnerToFirstId make the cleanup of a owner with zero timers fast and the linked list
        makes it possible to remove individual timers and still keep the m_OwnerToFirstId lookup valid.

        m_IndexLookup

            Lookup index               Timer array index
             0 <---------------------   3
             1                       |  0  
             2 <------------------   |  2
             3                    |  |  4
             4                    |  |  1
                                  |  |
        m_OwnerToFirstId          |  |
                                  |  |
            Script context        |  |  Lookup index
             1                    |   -- 0
             2                     ----- 2

           -----------------------------------
        0 | m_Id: 0x0000 0001                 | <--------------
          | m_Owner 1                         |            |   |
          | m_PrevOwnerLookupIndex 3          | --------   |   |
          | m_NextOwnerLookupIndex 4          | ---     |  |   |
           -----------------------------------     |    |  |   |
        1 | m_Id: 0x0002 0004                 | <--     |  |   |
          | m_Owner 1                         |         |  |   |
          | m_PrevOwnerLookupIndex 1          | --------|--    |
          | m_NextOwnerLookupIndex -1         |         |      |
           -----------------------------------          |      |
        2 | m_Id: 0x0000 0002                 |         |      |   <-   m_OwnerToFirstId[2] -> m_IndexLookup[2] -> m_Timers[2]
          | m_Owner 2                         |         |      |
          | m_PrevOwnerLookupIndex -1         |         |      |
          | m_NextOwnerLookupIndex -1         |         |      |
           -----------------------------------          |      |
        3 | m_Id: 0x0001 0000                 | <----   |      |   <-   m_OwnerToFirstId[1] -> m_IndexLookup[0] -> m_Timers[3]
          | m_Owner 1                         |      |  |      |
          | m_PrevOwnerLookupIndex -1         |      |  |      |
          | m_NextOwnerLookupIndex 3          | --   |  |      |
           -----------------------------------    |  |  |      |
        4 | m_Id: 0x0000 0003                 | <----|--       |
          | m_Owner 1                         |      |         |
          | m_PrevOwnerLookupIndex 0          | -----          |
          | m_NextOwnerLookupIndex 1          | ---------------
           -----------------------------------
    */

    const char* TIMER_WORLD_VALUE_KEY = "__dm_timer_world__";

    static uint16_t MAX_OWNER_COUNT = 32;

    struct Timer
    {
        TimerCallback   m_Callback;
        uintptr_t       m_Owner;
        uintptr_t       m_UserData;

        // Store complete timer handle with generation here to identify stale timer handles
        HTimer          m_Handle;

        // How much time remaining until the timer fires
        float           m_Remaining;

        // The timer delay, we need to keep this for repeating timers
        float           m_Delay;

        // Linked list of all timers with same owner
        uint16_t        m_PrevOwnerLookupIndex;
        uint16_t        m_NextOwnerLookupIndex;

        // Flag if the timer should repeat
        uint32_t        m_Repeat : 1;
        // Flag if the timer is alive
        uint32_t        m_IsAlive : 1;
    };

    #define INVALID_TIMER_LOOKUP_INDEX  0xffffu
    #define INITIAL_TIMER_CAPACITY      16u
    #define MAX_TIMER_CAPACITY          65000u  // Needs to be less that 65535 since 65535 is reserved for invalid index
    #define MIN_TIMER_CAPACITY_GROWTH   64u

    struct TimerWorld
    {
        dmArray<Timer>                      m_Timers;
        dmArray<uint16_t>                   m_IndexLookup;
        dmIndexPool<uint16_t>               m_IndexPool;
        dmHashTable<uintptr_t, HTimer>      m_OwnerToFirstHandle;
        uint16_t                            m_Version;   // Incremented to avoid collisions each time we push timer indexes back to the m_IndexPool
        uint16_t                            m_InUpdate : 1;
    };

    static uint16_t GetLookupIndex(HTimer handle)
    {
        return (uint16_t)(handle & 0xffffu);
    }

    static HTimer MakeHandle(uint16_t generation, uint16_t lookup_index)
    {
        return (((uint32_t)generation) << 16) | (lookup_index);
    }

    static Timer* AllocateTimer(HTimerWorld timer_world, uintptr_t owner)
    {
        assert(timer_world != 0x0);
        uint32_t timer_count = timer_world->m_Timers.Size();
        if (timer_count == MAX_TIMER_CAPACITY)
        {
            dmLogError("Timer could not be stored since the timer buffer is full (%d).", MAX_TIMER_CAPACITY);
            return 0x0;
        }

        HTimer* handle_ptr = timer_world->m_OwnerToFirstHandle.Get(owner);
        if ((handle_ptr == 0x0) && timer_world->m_OwnerToFirstHandle.Full())
        {
            dmLogError("Timer could not be stored since timer max_owner_count has been reached (%d).", timer_world->m_OwnerToFirstHandle.Size());
            return 0x0;
        }

        if (timer_world->m_IndexPool.Remaining() == 0)
        {
            // Growth heuristic is to grow with the mean of MIN_TIMER_CAPACITY_GROWTH and half current capacity, and at least MIN_TIMER_CAPACITY_GROWTH
            uint32_t old_capacity = timer_world->m_IndexPool.Capacity();
            uint32_t growth = dmMath::Min(MIN_TIMER_CAPACITY_GROWTH, (MIN_TIMER_CAPACITY_GROWTH + old_capacity / 2) / 2);
            uint32_t capacity = dmMath::Min(old_capacity + growth, MAX_TIMER_CAPACITY);
            timer_world->m_IndexPool.SetCapacity(capacity);
            timer_world->m_IndexLookup.SetCapacity(capacity);
            timer_world->m_IndexLookup.SetSize(capacity);
            memset(&timer_world->m_IndexLookup[old_capacity], 0u, (capacity - old_capacity) * sizeof(uint16_t));
        }

        HTimer handle = MakeHandle(timer_world->m_Version, timer_world->m_IndexPool.Pop());

        if (timer_world->m_Timers.Full())
        {
            // Growth heuristic is to grow with the mean of MIN_TIMER_CAPACITY_GROWTH and half current capacity, and at least MIN_TIMER_CAPACITY_GROWTH
            uint32_t capacity = timer_world->m_Timers.Capacity();
            uint32_t growth = dmMath::Min(MIN_TIMER_CAPACITY_GROWTH, (MIN_TIMER_CAPACITY_GROWTH + capacity / 2) / 2);
            capacity = dmMath::Min(capacity + growth, MAX_TIMER_CAPACITY);
            timer_world->m_Timers.SetCapacity(capacity);
        }

        timer_world->m_Timers.SetSize(timer_count + 1);
        Timer& timer = timer_world->m_Timers[timer_count];
        timer.m_Handle = handle;
        timer.m_Owner = owner;

        timer.m_PrevOwnerLookupIndex = INVALID_TIMER_LOOKUP_INDEX;
        if (handle_ptr == 0x0)
        {
            timer.m_NextOwnerLookupIndex = INVALID_TIMER_LOOKUP_INDEX;
        }
        else
        {
            uint16_t next_lookup_index = GetLookupIndex(*handle_ptr);
            uint32_t next_timer_index = timer_world->m_IndexLookup[next_lookup_index];
            Timer& nextTimer = timer_world->m_Timers[next_timer_index];
            nextTimer.m_PrevOwnerLookupIndex = GetLookupIndex(handle);
            timer.m_NextOwnerLookupIndex = next_lookup_index;
        }

        uint16_t lookup_index = GetLookupIndex(handle);

        timer_world->m_IndexLookup[lookup_index] = timer_count;
        if (handle_ptr == 0x0)
        {
            timer_world->m_OwnerToFirstHandle.Put(owner, handle);
        }
        else
        {
            *handle_ptr = handle;
        }
        return &timer;
    }

    static void EraseTimer(HTimerWorld timer_world, uint32_t timer_index)
    {
        assert(timer_world != 0x0);
 
        Timer& movedTimer = timer_world->m_Timers.EraseSwap(timer_index);

        if (timer_index < timer_world->m_Timers.Size())
        {
            uint16_t moved_lookup_index = GetLookupIndex(movedTimer.m_Handle);
            timer_world->m_IndexLookup[moved_lookup_index] = timer_index;
        }
    }

    static void FreeTimer(HTimerWorld timer_world, Timer& timer)
    {
        assert(timer_world != 0x0);
        assert(timer.m_IsAlive == 0);

        uint16_t lookup_index = GetLookupIndex(timer.m_Handle);
        uint16_t timer_index = timer_world->m_IndexLookup[lookup_index];
        timer_world->m_IndexPool.Push(lookup_index);

        uint16_t prev_lookup_index = timer.m_PrevOwnerLookupIndex;
        uint16_t next_lookup_index = timer.m_NextOwnerLookupIndex;

        bool is_first_timer = INVALID_TIMER_LOOKUP_INDEX == prev_lookup_index;
        bool is_last_timer = INVALID_TIMER_LOOKUP_INDEX == next_lookup_index;

        if (is_first_timer && is_last_timer)
        {
            timer_world->m_OwnerToFirstHandle.Erase(timer.m_Owner);
        }
        else
        {
            if (!is_last_timer)
            {
                uint32_t next_timer_index = timer_world->m_IndexLookup[next_lookup_index];
                Timer& next_timer = timer_world->m_Timers[next_timer_index];
                next_timer.m_PrevOwnerLookupIndex = prev_lookup_index;

                if (is_first_timer)
                {
                    HTimer* handle_ptr = timer_world->m_OwnerToFirstHandle.Get(timer.m_Owner);
                    assert(handle_ptr != 0x0);
                    *handle_ptr = next_timer.m_Handle;
                }
            }

            if (!is_first_timer)
            {
                uint32_t prev_timer_index = timer_world->m_IndexLookup[prev_lookup_index];
                Timer& prev_timer = timer_world->m_Timers[prev_timer_index];
                prev_timer.m_NextOwnerLookupIndex = next_lookup_index;
            }
        }

        EraseTimer(timer_world, timer_index);
    }

    HTimerWorld NewTimerWorld(uint16_t max_owner_count)
    {
        TimerWorld* timer_world = new TimerWorld();
        timer_world->m_Timers.SetCapacity(INITIAL_TIMER_CAPACITY);
        timer_world->m_IndexLookup.SetCapacity(INITIAL_TIMER_CAPACITY);
        timer_world->m_IndexLookup.SetSize(INITIAL_TIMER_CAPACITY);
        memset(&timer_world->m_IndexLookup[0], 0u, INITIAL_TIMER_CAPACITY * sizeof(uint16_t));
        timer_world->m_IndexPool.SetCapacity(INITIAL_TIMER_CAPACITY);
        const uint32_t table_count = dmMath::Max(1, max_owner_count/3);
        timer_world->m_OwnerToFirstHandle.SetCapacity(table_count, max_owner_count);
        timer_world->m_Version = 0;
        timer_world->m_InUpdate = 0;
        return timer_world;
    }

    void DeleteTimerWorld(HTimerWorld timer_world)
    {
        assert(timer_world->m_InUpdate == 0);
        delete timer_world;
    }

    void UpdateTimers(HTimerWorld timer_world, float dt)
    {
        assert(timer_world != 0x0);
        DM_PROFILE(TimerWorld, "Update");

        timer_world->m_InUpdate = 1;

        // We only scan timers for trigger if the timer *existed at entry to UpdateTimers*, any timers added
        // in a trigger callback will always be added at the end of m_Timers and not triggered in this scope.
        uint32_t size = timer_world->m_Timers.Size();
        DM_COUNTER("timerc", size);

        for (uint32_t i = 0; i < size; ++i)
        {
            Timer& timer = timer_world->m_Timers[i];
            if (timer.m_IsAlive == 1)
            {
                assert(timer.m_Remaining >= 0.0f);

                timer.m_Remaining -= dt;
                if (timer.m_Remaining > 0.0f)
                {
                    continue;
                }
                assert(timer.m_Callback != 0x0);

                float elapsed_time = timer.m_Delay - timer.m_Remaining;

                TimerEventType eventType = timer.m_Repeat == 0 ? TIMER_EVENT_TRIGGER_WILL_DIE : TIMER_EVENT_TRIGGER_WILL_REPEAT;

                timer.m_Callback(timer_world, eventType, timer.m_Handle, elapsed_time, timer.m_Owner, timer.m_UserData);

                if (timer.m_IsAlive == 0)
                {
                    continue;
                }

                if (timer.m_Repeat == 0)
                {
                    timer.m_IsAlive = 0;
                }
                else
                {
                    if (timer.m_Delay == 0.0f)
                    {
                        timer.m_Remaining = timer.m_Delay;
                    }
                    else if (timer.m_Remaining == 0.0f)
                    {
                        timer.m_Remaining = timer.m_Delay;
                    }
                    else if (timer.m_Remaining < 0.0f)
                    {
                        float wrapped_count = ((-timer.m_Remaining) / timer.m_Delay) + 1.f;
                        float offset_to_next_trigger  = floor(wrapped_count) * timer.m_Delay;
                        timer.m_Remaining += offset_to_next_trigger;
                        assert(timer.m_Remaining >= 0.f);
                    }
                }
            }
        }
        timer_world->m_InUpdate = 0;

        size = timer_world->m_Timers.Size();
        uint32_t original_size = size;
        uint32_t i = 0;
        while (i < size)
        {
            Timer& timer = timer_world->m_Timers[i];
            if (timer.m_IsAlive == 0)
            {
                FreeTimer(timer_world, timer);
                --size;
            }
            else
            {
                ++i;
            }
        }

        if (size != original_size)
        {
            ++timer_world->m_Version;            
        }
    }

    HTimer AddTimer(HTimerWorld timer_world,
                    float delay,
                    bool repeat,
                    TimerCallback timer_callback,
                    uintptr_t owner,
                    uintptr_t userdata)
    {
        assert(timer_world != 0x0);
        assert(delay >= 0.f);
        assert(timer_callback != 0x0);
        Timer* timer = AllocateTimer(timer_world, owner);
        if (timer == 0x0)
        {
            return INVALID_TIMER_HANDLE;
        }

        timer->m_Delay = delay;
        timer->m_Remaining = delay;
        timer->m_UserData = userdata;
        timer->m_Callback = timer_callback;
        timer->m_Repeat = repeat;
        timer->m_IsAlive = 1;

        return timer->m_Handle;
    }

    bool CancelTimer(HTimerWorld timer_world, HTimer handle)
    {
        assert(timer_world != 0x0);
        uint16_t lookup_index = GetLookupIndex(handle);
        if (lookup_index >= timer_world->m_IndexLookup.Size())
        {
            return false;
        }

        uint16_t timer_index = timer_world->m_IndexLookup[lookup_index];
        if (timer_index >= timer_world->m_Timers.Size())
        {
            return false;
        }

        Timer& timer = timer_world->m_Timers[timer_index];
        if (timer.m_Handle != handle)
        {
            return false;
        }

        if (timer.m_IsAlive == 0)
        {
            return false;
        }

        timer.m_IsAlive = 0;
        timer.m_Callback(timer_world, TIMER_EVENT_CANCELLED, timer.m_Handle, 0.f, timer.m_Owner, timer.m_UserData);

        if (timer_world->m_InUpdate == 0)
        {
            FreeTimer(timer_world, timer);
            ++timer_world->m_Version;
        }
        return true;
    }

    uint32_t KillTimers(HTimerWorld timer_world, uintptr_t owner)
    {
        assert(timer_world != 0x0);
        HTimer* handle_ptr = timer_world->m_OwnerToFirstHandle.Get(owner);
        if (handle_ptr == 0x0)
            return 0u;

        ++timer_world->m_Version;

        uint32_t cancelled_count = 0u;
        uint16_t lookup_index = GetLookupIndex(*handle_ptr);
        do
        {
            timer_world->m_IndexPool.Push(lookup_index);

            uint16_t timer_index = timer_world->m_IndexLookup[lookup_index];
            Timer& timer = timer_world->m_Timers[timer_index];
            lookup_index = timer.m_NextOwnerLookupIndex;

            if (timer.m_IsAlive == 1)
            {
                timer.m_IsAlive = 0;
                ++cancelled_count;
            }

            if (timer_world->m_InUpdate == 0)
            {
                EraseTimer(timer_world, timer_index);
            }
        } while (lookup_index != INVALID_TIMER_LOOKUP_INDEX);

        timer_world->m_OwnerToFirstHandle.Erase(owner);
        return cancelled_count;
    }

    uint32_t GetAliveTimers(HTimerWorld timer_world)
    {
        assert(timer_world != 0x0);

        uint32_t alive_timers = 0u;
        uint32_t size = timer_world->m_Timers.Size();
        uint32_t i = 0;
        while (i < size)
        {
            Timer& timer = timer_world->m_Timers[i];
            if(timer.m_IsAlive == 1)
            {
                ++alive_timers;
            }
            ++i;
        }
        return alive_timers;
    }

    static void SetTimerWorld(HScriptWorld script_world, HTimerWorld timer_world)
    {
        HContext context = GetScriptWorldContext(script_world);
        lua_pushstring(context->m_LuaState, TIMER_WORLD_VALUE_KEY);
        lua_pushlightuserdata(context->m_LuaState, timer_world);
        SetScriptWorldContextValue(script_world);
    }

    static HTimerWorld GetTimerWorld(HScriptWorld script_world)
    {
        assert(script_world != 0x0);
        HContext context = GetScriptWorldContext(script_world);
        assert(context != 0x0);
        lua_State* L = context->m_LuaState;
        assert(L != 0x0);
        DM_LUA_STACK_CHECK(L, 0);

        lua_pushstring(L, TIMER_WORLD_VALUE_KEY);
        GetScriptWorldContextValue(script_world);
        HTimerWorld timer_world = (HTimerWorld)lua_touserdata(L, -1);
        lua_pop(L, 1);
        return timer_world;
    }

    void TimerNewScriptWorld(HScriptWorld script_world)
    {
        assert(script_world != 0x0);
        HContext context = GetScriptWorldContext(script_world);
        assert(context != 0x0);
        lua_State* L = context->m_LuaState;
        assert(L != 0x0);
        DM_LUA_STACK_CHECK(L, 0);

        HTimerWorld timer_world = NewTimerWorld(MAX_OWNER_COUNT);
        lua_pushstring(L, TIMER_WORLD_VALUE_KEY);
        lua_pushlightuserdata(L, timer_world);
        SetScriptWorldContextValue(script_world);
    }

    void TimerDeleteScriptWorld(HScriptWorld script_world)
    {
        assert(script_world != 0x0);
        HTimerWorld timer_world = GetTimerWorld(script_world);
        if (timer_world != 0x0)
        {
            SetTimerWorld(script_world, 0x0);
            DeleteTimerWorld(timer_world);
        }
    }

    void TimerUpdateScriptWorld(HScriptWorld script_world, float dt)
    {
        assert(script_world != 0x0);
        HTimerWorld timer_world = GetTimerWorld(script_world);
        if (timer_world != 0x0)
        {
            UpdateTimers(timer_world, dt);
        }
    }

    void TimerInitializeInstance(HScriptWorld script_world)
    {
        HContext context = GetScriptWorldContext(script_world);
        lua_State* L = GetLuaState(context);
        DM_LUA_STACK_CHECK(L, 0);

        lua_pushstring(L, TIMER_WORLD_VALUE_KEY);
        HTimerWorld timer_world = GetTimerWorld(script_world);
        lua_pushlightuserdata(L, timer_world);
        SetInstanceContextValue(L);
    }

    void TimerFinalizeInstance(HScriptWorld script_world)
    {
        HContext context = GetScriptWorldContext(script_world);
        lua_State* L = GetLuaState(context);
        DM_LUA_STACK_CHECK(L, 0);

        uintptr_t owner = dmScript::GetInstanceId(L);
        HTimerWorld timer_world = GetTimerWorld(script_world);
        KillTimers(timer_world, owner);        

        lua_pushstring(L, TIMER_WORLD_VALUE_KEY);
        lua_pushnil(L);
        SetInstanceContextValue(L);
    }

    struct LuaTimerCallbackArgs
    {
        dmScript::HTimer timer_handle;
        float time_elapsed;
    };

    static void LuaTimerCallbackArgsCB(lua_State* L, void* user_context)
    {
        LuaTimerCallbackArgs* args = (LuaTimerCallbackArgs*)user_context;
        lua_pushinteger(L, args->timer_handle);
        lua_pushnumber(L, args->time_elapsed);
    }

    static void LuaTimerCallback(HTimerWorld timer_world, TimerEventType event_type, dmScript::HTimer timer_handle, float time_elapsed, uintptr_t owner, uintptr_t userdata)
    {
        LuaCallbackInfo* callback = (LuaCallbackInfo*)userdata;
        if (!IsValidCallback(callback))
        {
            return;
        }

        if (event_type != TIMER_EVENT_CANCELLED)
        {
            LuaTimerCallbackArgs args = { timer_handle, time_elapsed };

            InvokeCallback(callback, LuaTimerCallbackArgsCB, &args);
        }

        if (event_type != TIMER_EVENT_TRIGGER_WILL_REPEAT)
        {
            DeleteCallback(callback);
        }
    }

    static dmScript::HTimerWorld GetTimerWorld(lua_State* L)
    {
        lua_pushstring(L, TIMER_WORLD_VALUE_KEY);
        dmScript::GetInstanceContextValue(L);

        if (lua_type(L, -1) != LUA_TLIGHTUSERDATA)
        {
            lua_pop(L, 1);
            return 0x0;
        }

        dmScript::HTimerWorld world = (dmScript::HTimerWorld)lua_touserdata(L, -1);
        lua_pop(L, 1);
        return world;
    }

    /*# Create a timer
     * Adds a timer and returns a unique handle
     *
     * You may create more timers from inside a timer callback.
     *
     * Using a delay of 0.0f will result in a timer that triggers at the next frame just before
     * script update functions.
     *
     * If you want a timer that triggers on each frame, set delay to 0.0f and repeat to true.
     * 
     * Timers created within a script will automatically die when the script is deleted.
     *
     * @name timer.delay
     * @param delay time interval in seconds
     * @param repeat true = repeat timer until cancel, false = one-shot timer
     * @param callback [type:function(self, handle, time_elapsed)] timer callback function
     *
     * `self`
     * : [type:object] The current object
     *
     * `handle`
     * : [type:number] The handle of the timer
     *
     * `time_elapsed`
     * : [type:number] The elapsed time - on first trigger it is time since timer.delay call, otherwise time since last trigger
     *
     * @return handle identifier for the create timer, returns timer.INVALID_TIMER_HANDLE if the timer can not be created
     */
    static int TimerDelay(lua_State* L) {
        int top = lua_gettop(L);
        luaL_checktype(L, 1, LUA_TNUMBER);
        luaL_checktype(L, 2, LUA_TBOOLEAN);
        luaL_checktype(L, 3, LUA_TFUNCTION);

        const double seconds = lua_tonumber(L, 1);
        bool repeat = lua_toboolean(L, 2);

        dmScript::HTimerWorld timer_world = GetTimerWorld(L);
        if (timer_world == 0x0)
        {
            dmLogError("Unable to create a timer, the lua context does not have a timer world");
            lua_pushnumber(L, dmScript::INVALID_TIMER_HANDLE);
            return 1;
        }
    
        uintptr_t owner = dmScript::GetInstanceId(L);

        LuaCallbackInfo* user_data = dmScript::CreateCallback(L, 3);

        dmScript::HTimer handle = dmScript::AddTimer(timer_world, seconds, repeat, LuaTimerCallback, (uintptr_t)owner, (uintptr_t)user_data);

        lua_pushinteger(L, handle);
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# Cancel a timer
     *
     * You may cancel a timer from inside a timer callback.
     * Cancelling a timer that is already executed or cancelled is safe.
     * 
     * @param handle the timer handle returned by timer.delay()
     * @return true if the timer was active, false if the timer is already cancelled / complete
     */
    static int TimerCancel(lua_State* L)
    {
        int top = lua_gettop(L);
        const int handle = luaL_checkint(L, 1);

        dmScript::HTimerWorld timer_world = GetTimerWorld(L);
        if (timer_world == 0x0)
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        bool cancelled = dmScript::CancelTimer(timer_world, (dmScript::HTimer)handle);
        lua_pushboolean(L, cancelled ? 1 : 0);
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static const luaL_reg TIMER_COMP_FUNCTIONS[] = {
        { "delay", TimerDelay },
        { "cancel", TimerCancel },
        { 0, 0 }
    };

    void TimerInitialize(HContext context)
    {
        lua_State* L = context->m_LuaState;
        dmConfigFile::HConfig config_file = context->m_ConfigFile;
        DM_LUA_STACK_CHECK(L, 0);
 
        MAX_OWNER_COUNT = config_file ? dmConfigFile::GetInt(config_file, "timer.max_owner_count", 32) : 32;

        luaL_register(L, "timer", TIMER_COMP_FUNCTIONS);

        #define SETCONSTANT(name) \
            lua_pushnumber(L, (lua_Number) name); \
            lua_setfield(L, -2, #name);\

        /*# Indicates an invalid timer handle
         *
         * @name timer.INVALID_TIMER_HANDLE
         * @variable
         */
        SETCONSTANT(INVALID_TIMER_HANDLE);

        lua_pop(L, 1);
    }

    void InitializeTimer(HContext context)
    {
        static ScriptExtension sl;
        sl.Initialize = TimerInitialize;
        sl.NewScriptWorld = TimerNewScriptWorld;
        sl.DeleteScriptWorld = TimerDeleteScriptWorld;
        sl.UpdateScriptWorld = TimerUpdateScriptWorld;
        sl.InitializeScriptInstance = TimerInitializeInstance;
        sl.FinalizeScriptInstance = TimerFinalizeInstance;
        RegisterScriptExtension(context, &sl);
    }
}
