#include "script_timer.h"

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
        stale indexes - the caller to CancelTimer is allowed to call with an id of a timer that already
        has expired.

        We also keep track of all timers associated with a specific owner, we do this by
        creating a linked list (using indirected lookup indexes) inside the timer array and just
        holding the most recently added timer to a specific owner in a hash table for fast lookup.

        This is to avoid scanning the entire timer array for a specific owner for items to remove.

        Each game object needs to call CancelTimers for its owner to clean up potential timers
        that has not yet been cancelled. We don't want each GO to scan the entire array for timers at destuction.
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

    struct Timer
    {
        TimerTrigger    m_Trigger;
        uintptr_t       m_Owner;

        // Store complete timer id with generation here to identify stale timer ids
        HTimer          m_Id;

        // How much time remaining until the timer fires
        float           m_Remaining;

        // The timer interval, we need to keep this for repeating timers
        float           m_Interval;

        int             m_SelfRef;
        int             m_CallbackRef;

        // Linked list of all timers with same owner
        uint16_t        m_PrevOwnerLookupIndex;
        uint16_t        m_NextOwnerLookupIndex;

        // Flag if the timer should repeat
        uint32_t        m_Repeat : 1;
        // Flag if the timer is alive
        uint32_t        m_IsAlive : 1;
    };

    #define INVALID_TIMER_LOOKUP_INDEX  0xffffu
    #define INITIAL_TIMER_CAPACITY      256u
    #define MAX_TIMER_CAPACITY          65000u  // Needs to be less that 65536 since 65535 is reserved for invalid index
    #define MIN_TIMER_CAPACITY_GROWTH   2048u

    struct TimerContext
    {
        dmArray<Timer>                      m_Timers;
        dmArray<uint16_t>                   m_IndexLookup;
        dmIndexPool<uint16_t>               m_IndexPool;
        dmHashTable<uintptr_t, HTimer>      m_OwnerToFirstId;
        uint16_t                            m_Version;   // Incremented to avoid collisions each time we push timer indexes back to the m_IndexPool
        uint16_t                            m_InUpdate : 1;
    };

    static uint16_t GetLookupIndex(HTimer id)
    {
        return (uint16_t)(id & 0xffffu);
    }

    static HTimer MakeId(uint16_t generation, uint16_t lookup_index)
    {
        return (((uint32_t)generation) << 16) | (lookup_index);
    }

    static Timer* AllocateTimer(HTimerContext timer_context, uintptr_t owner)
    {
        assert(timer_context != 0x0);
        uint32_t timer_count = timer_context->m_Timers.Size();
        if (timer_count == MAX_TIMER_CAPACITY)
        {
            dmLogError("Timer could not be stored since the timer buffer is full (%d).", MAX_TIMER_CAPACITY);
            return 0x0;
        }

        HTimer* id_ptr = timer_context->m_OwnerToFirstId.Get(owner);
        if ((id_ptr == 0x0) && timer_context->m_OwnerToFirstId.Full())
        {
            dmLogError("Timer could not be stored since timer max_context_count has been reached (%d).", timer_context->m_OwnerToFirstId.Size());
            return 0x0;
        }

        if (timer_context->m_IndexPool.Remaining() == 0)
        {
            // Growth heuristic is to grow with the mean of MIN_TIMER_CAPACITY_GROWTH and half current capacity, and at least MIN_TIMER_CAPACITY_GROWTH
            uint32_t old_capacity = timer_context->m_IndexPool.Capacity();
            uint32_t growth = dmMath::Min(MIN_TIMER_CAPACITY_GROWTH, (MIN_TIMER_CAPACITY_GROWTH + old_capacity / 2) / 2);
            uint32_t capacity = dmMath::Min(old_capacity + growth, MAX_TIMER_CAPACITY);
            timer_context->m_IndexPool.SetCapacity(capacity);
            timer_context->m_IndexLookup.SetCapacity(capacity);
            timer_context->m_IndexLookup.SetSize(capacity);
            memset(&timer_context->m_IndexLookup[old_capacity], 0u, (capacity - old_capacity) * sizeof(uint16_t));
        }

        HTimer id = MakeId(timer_context->m_Version, timer_context->m_IndexPool.Pop());

        if (timer_context->m_Timers.Full())
        {
            // Growth heuristic is to grow with the mean of MIN_TIMER_CAPACITY_GROWTH and half current capacity, and at least MIN_TIMER_CAPACITY_GROWTH
            uint32_t capacity = timer_context->m_Timers.Capacity();
            uint32_t growth = dmMath::Min(MIN_TIMER_CAPACITY_GROWTH, (MIN_TIMER_CAPACITY_GROWTH + capacity / 2) / 2);
            capacity = dmMath::Min(capacity + growth, MAX_TIMER_CAPACITY);
            timer_context->m_Timers.SetCapacity(capacity);
        }

        timer_context->m_Timers.SetSize(timer_count + 1);
        Timer& timer = timer_context->m_Timers[timer_count];
        timer.m_Id = id;
        timer.m_Owner = owner;

        timer.m_PrevOwnerLookupIndex = INVALID_TIMER_LOOKUP_INDEX;
        if (id_ptr == 0x0)
        {
            timer.m_NextOwnerLookupIndex = INVALID_TIMER_LOOKUP_INDEX;
        }
        else
        {
            uint16_t next_lookup_index = GetLookupIndex(*id_ptr);
            uint32_t next_timer_index = timer_context->m_IndexLookup[next_lookup_index];
            Timer& nextTimer = timer_context->m_Timers[next_timer_index];
            nextTimer.m_PrevOwnerLookupIndex = GetLookupIndex(id);
            timer.m_NextOwnerLookupIndex = next_lookup_index;
        }

        uint16_t lookup_index = GetLookupIndex(id);

        timer_context->m_IndexLookup[lookup_index] = timer_count;
        if (id_ptr == 0x0)
        {
            timer_context->m_OwnerToFirstId.Put(owner, id);
        }
        else
        {
            *id_ptr = id;
        }
        return &timer;
    }

    static void EraseTimer(HContext context, uint32_t timer_index)
    {
        assert(context != 0x0);
        HTimerContext timer_context = context->m_TimerContext;
        assert(timer_context != 0x0);
 
        Timer& timer = timer_context->m_Timers[timer_index];
        lua_State* L = dmScript::GetLuaState(context);
        dmScript::Unref(L, LUA_REGISTRYINDEX, timer.m_CallbackRef);
        dmScript::Unref(L, LUA_REGISTRYINDEX, timer.m_SelfRef);

        Timer& movedTimer = timer_context->m_Timers.EraseSwap(timer_index);

        if (timer_index < timer_context->m_Timers.Size())
        {
            uint16_t moved_lookup_index = GetLookupIndex(movedTimer.m_Id);
            timer_context->m_IndexLookup[moved_lookup_index] = timer_index;
        }
    }

    static void FreeTimer(HContext context, Timer& timer)
    {
        assert(context != 0x0);
        HTimerContext timer_context = context->m_TimerContext;
        assert(timer_context != 0x0);
        assert(timer.m_IsAlive == 0);

        uint16_t lookup_index = GetLookupIndex(timer.m_Id);
        uint16_t timer_index = timer_context->m_IndexLookup[lookup_index];
        timer_context->m_IndexPool.Push(lookup_index);

        uint16_t prev_lookup_index = timer.m_PrevOwnerLookupIndex;
        uint16_t next_lookup_index = timer.m_NextOwnerLookupIndex;

        bool is_first_timer = INVALID_TIMER_LOOKUP_INDEX == prev_lookup_index;
        bool is_last_timer = INVALID_TIMER_LOOKUP_INDEX == next_lookup_index;

        if (is_first_timer && is_last_timer)
        {
            timer_context->m_OwnerToFirstId.Erase(timer.m_Owner);
        }
        else
        {
            if (!is_last_timer)
            {
                uint32_t next_timer_index = timer_context->m_IndexLookup[next_lookup_index];
                Timer& next_timer = timer_context->m_Timers[next_timer_index];
                next_timer.m_PrevOwnerLookupIndex = prev_lookup_index;

                if (is_first_timer)
                {
                    HTimer* id_ptr = timer_context->m_OwnerToFirstId.Get(timer.m_Owner);
                    assert(id_ptr != 0x0);
                    *id_ptr = next_timer.m_Id;
                }
            }

            if (!is_first_timer)
            {
                uint32_t prev_timer_index = timer_context->m_IndexLookup[prev_lookup_index];
                Timer& prev_timer = timer_context->m_Timers[prev_timer_index];
                prev_timer.m_NextOwnerLookupIndex = next_lookup_index;
            }
        }

        EraseTimer(context, timer_index);
    }

    HTimerContext NewTimerContext(uint16_t max_owner_count)
    {
        TimerContext* timer_context = new TimerContext();
        timer_context->m_Timers.SetCapacity(INITIAL_TIMER_CAPACITY);
        timer_context->m_IndexLookup.SetCapacity(INITIAL_TIMER_CAPACITY);
        timer_context->m_IndexLookup.SetSize(INITIAL_TIMER_CAPACITY);
        memset(&timer_context->m_IndexLookup[0], 0u, INITIAL_TIMER_CAPACITY * sizeof(uint16_t));
        timer_context->m_IndexPool.SetCapacity(INITIAL_TIMER_CAPACITY);
        const uint32_t table_count = dmMath::Max(1, max_owner_count/3);
        timer_context->m_OwnerToFirstId.SetCapacity(table_count, max_owner_count);
        timer_context->m_Version = 0;
        timer_context->m_InUpdate = 0;
        return timer_context;
    }

    void DeleteTimerContext(HTimerContext timer_context)
    {
        assert(timer_context->m_InUpdate == 0);
        delete timer_context;
    }

    void UpdateTimerContext(HContext context, float dt)
    {
        assert(context != 0x0);
        HTimerContext timer_context = context->m_TimerContext;
        assert(timer_context != 0x0);
        DM_PROFILE(TimerContext, "Update");

        timer_context->m_InUpdate = 1;

        // We only scan timers for trigger if the timer *existed at entry to UpdateTimerContext*, any timers added
        // in a trigger callback will always be added at the end of m_Timers and not triggered in this scope.
        uint32_t size = timer_context->m_Timers.Size();
        DM_COUNTER("timerc", size);

        uint32_t i = 0;
        while (i < size)
        {
            Timer& timer = timer_context->m_Timers[i];
            if (timer.m_IsAlive == 1)
            {
                assert(timer.m_Remaining >= 0.0f);

                timer.m_Remaining -= dt;
                if (timer.m_Remaining <= 0.0f)
                {
                    assert(timer.m_Trigger != 0x0);

                    timer.m_IsAlive = timer.m_Repeat;

                    float elapsed_time = timer.m_Interval - timer.m_Remaining;

                    timer.m_Trigger(context, timer.m_Id, elapsed_time, timer.m_Owner, timer.m_SelfRef, timer.m_CallbackRef);

                    if (timer.m_Repeat == 1)
                    {
                        if (timer.m_Interval == 0.0f)
                        {
                            timer.m_Remaining = timer.m_Interval;
                        }
                        else if (timer.m_Remaining == 0.0f)
                        {
                            timer.m_Remaining = timer.m_Interval;
                        }
                        else if (timer.m_Remaining < 0.0f)
                        {
                            float wrapped_count = ((-timer.m_Remaining) / timer.m_Interval) + 1.f;
                            float offset_to_next_trigger  = floor(wrapped_count) * timer.m_Interval;
                            timer.m_Remaining += offset_to_next_trigger;
                            assert(timer.m_Remaining >= 0.f);
                        }
                    }
                }
            }
            ++i;
        }
        timer_context->m_InUpdate = 0;

        size = timer_context->m_Timers.Size();
        uint32_t original_size = size;
        i = 0;
        while (i < size)
        {
            Timer& timer = timer_context->m_Timers[i];
            if (timer.m_IsAlive == 0)
            {
                FreeTimer(context, timer);
                --size;
            }
            else
            {
                ++i;
            }
        }

        if (size != original_size)
        {
            ++timer_context->m_Version;            
        }
    }

    HTimer AddTimer(HContext context,
                    float delay,
                    bool repeat,
                    TimerTrigger timer_trigger,
                    uintptr_t owner,
                    int self_ref,
                    int callback_ref)
    {
        assert(context != 0x0);
        HTimerContext timer_context = context->m_TimerContext;
        assert(timer_context != 0x0);
        assert(delay >= 0.f);
        Timer* timer = AllocateTimer(timer_context, owner);
        if (timer == 0x0)
        {
            return INVALID_TIMER_ID;
        }

        timer->m_Interval = delay;
        timer->m_Remaining = delay;
        timer->m_SelfRef = self_ref;
        timer->m_CallbackRef = callback_ref;
        timer->m_Trigger = timer_trigger;
        timer->m_Repeat = repeat;
        timer->m_IsAlive = 1;

        return timer->m_Id;
    }

    bool CancelTimer(HContext context, HTimer id)
    {
        assert(context != 0x0);
        HTimerContext timer_context = context->m_TimerContext;
        assert(timer_context != 0x0);
        uint16_t lookup_index = GetLookupIndex(id);
        if (lookup_index >= timer_context->m_IndexLookup.Size())
        {
            return false;
        }

        uint16_t timer_index = timer_context->m_IndexLookup[lookup_index];
        if (timer_index >= timer_context->m_Timers.Size())
        {
            return false;
        }

        Timer& timer = timer_context->m_Timers[timer_index];
        if (timer.m_Id != id)
        {
            return false;
        }

        bool cancelled = timer.m_IsAlive == 1;
        timer.m_IsAlive = 0;

        if (timer_context->m_InUpdate == 0)
        {
            FreeTimer(context, timer);
            ++timer_context->m_Version;
        }
        return cancelled;
    }

    uint32_t CancelTimers(HContext context, uintptr_t owner)
    {
        assert(context != 0x0);
        HTimerContext timer_context = context->m_TimerContext;
        assert(timer_context != 0x0);
        HTimer* id_ptr = timer_context->m_OwnerToFirstId.Get(owner);
        if (id_ptr == 0x0)
            return 0u;

        ++timer_context->m_Version;

        uint32_t cancelled_count = 0u;
        uint16_t lookup_index = GetLookupIndex(*id_ptr);
        do
        {
            timer_context->m_IndexPool.Push(lookup_index);

            uint16_t timer_index = timer_context->m_IndexLookup[lookup_index];
            Timer& timer = timer_context->m_Timers[timer_index];
            lookup_index = timer.m_NextOwnerLookupIndex;

            if (timer.m_IsAlive == 1)
            {
                timer.m_IsAlive = 0;
                ++cancelled_count;
            }

            if (timer_context->m_InUpdate == 0)
            {
                EraseTimer(context, timer_index);
            }
        } while (lookup_index != INVALID_TIMER_LOOKUP_INDEX);

        timer_context->m_OwnerToFirstId.Erase(owner);
        return cancelled_count;
    }

    uint32_t GetAliveTimers(HContext context)
    {
        assert(context != 0x0);
        HTimerContext timer_context = context->m_TimerContext;
        assert(timer_context != 0x0);

        uint32_t alive_timers = 0u;
        uint32_t size = timer_context->m_Timers.Size();
        uint32_t i = 0;
        while (i < size)
        {
            Timer& timer = timer_context->m_Timers[i];
            if(timer.m_IsAlive == 1)
            {
                ++alive_timers;
            }
            ++i;
        }
        return alive_timers;
    }
    
    void LuaTimerCallback(dmScript::HContext context, dmScript::HTimer timer_id, float time_elapsed, uintptr_t owner, int self_ref, int callback_ref)
    {
        lua_State* L = dmScript::GetLuaState(context);
        int top = lua_gettop(L);
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback_ref);
        lua_rawgeti(L, LUA_REGISTRYINDEX, self_ref);
        lua_pushvalue(L, -1);
        dmScript::SetInstance(L);
        {
            lua_pushinteger(L, timer_id);
            lua_pushnumber(L, time_elapsed);
            int ret = lua_pcall(L, 3, LUA_MULTRET, 0);
            if (ret != 0) {
                dmLogError("Error running timer callback: %s", lua_tostring(L, -1));
                lua_pop(L, 2);
            }
        }
        assert(top == lua_gettop(L));
    }

    HTimer AddTimer(HContext context,
                            float delay,
                            bool repeat,
                            uintptr_t owner,
                            int self_ref,
                            int callback_ref)
    {
        return AddTimer(context,
                    delay,
                    repeat,
                    LuaTimerCallback,
                    owner,
                    self_ref,
                    callback_ref);
    }

    static int TimerDelay(lua_State* L) {
        int top = lua_gettop(L);
        luaL_checktype(L, 1, LUA_TNUMBER);
        luaL_checktype(L, 2, LUA_TBOOLEAN);
        luaL_checktype(L, 3, LUA_TFUNCTION);

        const double seconds = lua_tonumber(L, 1);
        bool repeat = lua_toboolean(L, 2);
    
        lua_pushvalue(L, 3);
        int cb = dmScript::Ref(L, LUA_REGISTRYINDEX);

        dmScript::GetInstance(L);
        int self = dmScript::Ref(L, LUA_REGISTRYINDEX);

        lua_getglobal(L, SCRIPT_CONTEXT);
        dmScript::HContext context = (dmScript::HContext)lua_touserdata(L, -1);
        lua_pop(L, 1);

        dmScript::HTimer id = dmScript::AddTimer(context, seconds, repeat, 0x0, self, cb);
        lua_pushinteger(L, id);
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static int TimerCancel(lua_State* L)
    {
        int top = lua_gettop(L);
        const int id = luaL_checkint(L, 1);

        lua_getglobal(L, SCRIPT_CONTEXT);
        dmScript::HContext context = (dmScript::HContext)lua_touserdata(L, -1);
        lua_pop(L, 1);

        bool cancelled = dmScript::CancelTimer(context, (dmScript::HTimer)id);
        lua_pushboolean(L, cancelled ? 1 : 0);
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static const luaL_reg TIMER_COMP_FUNCTIONS[] = {
        { "delay", TimerDelay },
        { "cancel", TimerCancel },
        { 0, 0 }
    };

    void InitializeTimer(lua_State* L)
    {
        int top = lua_gettop(L);
 
        luaL_register(L, "timer", TIMER_COMP_FUNCTIONS);

        lua_pop(L, 1);
        assert(top == lua_gettop(L));
    }
}
