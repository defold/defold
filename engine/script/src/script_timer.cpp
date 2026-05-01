// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "script_timer.h"
#include "script_timer_private.h"

#include <string.h>
#include <dlib/object_pool.h>
#include <dlib/profile.h>
#include <dlib/set.h>

#include "script.h"
#include "script_private.h"

DM_PROPERTY_EXTERN(rmtp_Script);
DM_PROPERTY_U32(rmtp_TimerCount, 0, PROFILE_PROPERTY_FRAME_RESET, "# timers", &rmtp_Script);

namespace dmScript
{
    /*# Timer API documentation
     *
     * Timers allow you to set a delay and a callback to be called when the timer completes.
     *
     * The timers created with this API are updated with the collection timer where they
     * are created. If you pause or speed up the collection (using `set_time_step`) it will
     * also affect the new timer.
     *
     * @document
     * @name Timer
     * @namespace timer
     * @language Lua
     */

    /*
        The timer handle is an index into an indirection layer combined with a generation counter,
        this makes it possible to reuse the index for the indirection layer without risk of using
        stale indexes - the caller to CancelTimer is allowed to call with an handle of a timer that already
        has expired.

        Each script instance needs to call KillTimers for its owner to clean up potential timers
        that has not yet been cancelled or completed (one-shot).
    */

    static const char TIMER_WORLD_VALUE_KEY[] = "__dm_timer_world__";
    static const uint32_t TIMER_WORLD_VALUE_KEY_HASH = dmHashBuffer32(TIMER_WORLD_VALUE_KEY, sizeof(TIMER_WORLD_VALUE_KEY) - 1);

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

        // The time that exceeded the delay in previous cycle in repeat timer.
        float           m_PrevCycleExcess;

        // Flag if the timer should repeat
        uint32_t        m_Repeat : 1;
        // Flag if the timer is alive
        uint32_t        m_IsAlive : 1;
    };

    #define INITIAL_TIMER_CAPACITY      8u
    #define TIMER_CAPACITY_GROWTH       16u

    struct TimerWorld
    {
        dmObjectPool<Timer> m_Timers;
        dmSet<uint16_t>     m_Instances; // the pool indices
        dmArray<uint16_t>   m_ScratchBuffer; // When doing operations on the timer instances, we need a copy to avoid self modification of m_Instances
        uint16_t            m_Version;   // Incremented to avoid collisions each time we push timer indexes back to the m_IndexPool
        uint16_t            m_InUpdate : 1;
        uint16_t            m_IsDirty : 1;
    };

    dmArray<TimerWorld*> g_Worlds;

    static uint16_t GetIndexFromHandle(HTimer handle)
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

        if (timer_world->m_Timers.Full() && timer_world->m_Timers.Capacity() == MAX_TIMER_CAPACITY)
        {
            dmLogError("Timer could not be stored since the timer buffer is full (%d).", MAX_TIMER_CAPACITY);
            return 0x0;
        }

        if (timer_world->m_Timers.Full())
        {
            uint32_t cap = timer_world->m_Timers.Capacity() + TIMER_CAPACITY_GROWTH;
            if (cap > MAX_TIMER_CAPACITY)
                cap = MAX_TIMER_CAPACITY;
            timer_world->m_Timers.SetCapacity(cap);
            timer_world->m_Instances.SetCapacity(cap);
        }

        uint16_t timer_index = (uint16_t)timer_world->m_Timers.Alloc();
        Timer* timer = &timer_world->m_Timers.Get(timer_index);
        timer_world->m_Instances.Add(timer_index);

        memset(timer, 0, sizeof(Timer));
        timer->m_Handle = MakeHandle(timer_world->m_Version, timer_index);
        timer->m_Owner = owner;

        timer_world->m_IsDirty = 1;
        return timer;
    }

    static void DeallocateTimer(HTimerWorld timer_world, uint16_t timer_index)
    {
        assert(timer_world != 0x0);

        timer_world->m_Instances.Remove(timer_index);
        timer_world->m_Timers.Free(timer_index, true);
        timer_world->m_IsDirty = 1;
    }

    static void FreeTimer(HTimerWorld timer_world, Timer* timer)
    {
        assert(timer_world != 0x0);
        assert(timer->m_IsAlive == 0);

        LuaCallbackInfo* callback = (LuaCallbackInfo*) timer->m_UserData;
        timer->m_UserData = 0;
        if (IsCallbackValid(callback))
        {
            DestroyCallback(callback);
        }

        uint16_t index = GetIndexFromHandle(timer->m_Handle);
        DeallocateTimer(timer_world, index);
    }

    HTimerWorld NewTimerWorld()
    {
        TimerWorld* timer_world = new TimerWorld();
        timer_world->m_Timers.SetCapacity(INITIAL_TIMER_CAPACITY);
        timer_world->m_Instances.SetCapacity(INITIAL_TIMER_CAPACITY);

        timer_world->m_Version = 0;
        timer_world->m_InUpdate = 0;
        timer_world->m_IsDirty = 0;
        return timer_world;
    }

    void DeleteTimerWorld(HTimerWorld timer_world)
    {
        assert(timer_world->m_InUpdate == 0);
        delete timer_world;
    }

    static Timer* GetTimerFromIndex(HTimerWorld timer_world, uint16_t index)
    {
        assert(timer_world != 0x0);
        if (!timer_world->m_Instances.Contains(index))
        {
            return 0;
        }
        return &timer_world->m_Timers.Get(index);
    }

    static Timer* GetTimerFromHandle(HTimerWorld timer_world, HTimer handle)
    {
        uint16_t index = GetIndexFromHandle(handle);
        Timer* timer = GetTimerFromIndex(timer_world, index);
        if (!timer)
            return 0;
        if (timer->m_Handle != handle)
            return 0; // Stale handle
        return timer;
    }

    static uint32_t CopyIndices(dmSet<uint16_t>& src, dmArray<uint16_t>& tgt)
    {
        uint32_t size = src.Size();
        if (size > tgt.Capacity())
        {
            tgt.SetCapacity(size);
        }
        tgt.SetSize(size);
        for (uint32_t i = 0; i < size; ++i)
        {
            tgt[i] = src[i];
        }
        return size;
    }

    void UpdateTimers(HTimerWorld timer_world, float dt)
    {
        assert(timer_world != 0x0);
        DM_PROFILE("Update");

        timer_world->m_InUpdate = 1;

        // We make a copy of the indices, as it's possible the m_Instances may be altered
        // Any timers added during this update call, will be updated the next frame
        uint32_t size = CopyIndices(timer_world->m_Instances, timer_world->m_ScratchBuffer);

        DM_PROPERTY_ADD_U32(rmtp_TimerCount, size);

        for (uint32_t i = 0; i < size; ++i)
        {
            uint16_t index = timer_world->m_ScratchBuffer[i];
            Timer* timer = GetTimerFromIndex(timer_world, index);
            if (!timer)
            {
                continue;
            }

            if (timer->m_IsAlive == 0)
            {
                continue;
            }

            timer->m_Remaining -= dt;
            if (timer->m_Remaining > 0.0f)
            {
                continue;
            }

            float elapsed_time = timer->m_Delay - timer->m_Remaining - timer->m_PrevCycleExcess;

            TimerEventType eventType = timer->m_Repeat == 0 ? TIMER_EVENT_TRIGGER_WILL_DIE : TIMER_EVENT_TRIGGER_WILL_REPEAT;
            timer->m_Callback(timer_world, eventType, timer->m_Handle, elapsed_time, timer->m_Owner, timer->m_UserData);

            if (timer_world->m_IsDirty)
            {
                // If the containers were reallocated, the current timer pointer needs to get fetched again
                timer = GetTimerFromIndex(timer_world, index);
                assert(timer);
            }

            if (timer->m_IsAlive == 0)
            {
                continue;
            }

            if (timer->m_Repeat == 0)
            {
                timer->m_IsAlive = 0;
                continue;
            }

            if (timer->m_Delay == 0.0f)
            {
                timer->m_Remaining = 0.0f;
                timer->m_PrevCycleExcess = 0.0f;
                continue;
            }

            float wrapped_count = ((-timer->m_Remaining) / timer->m_Delay) + 1.f;
            float offset_to_next_trigger  = floor(wrapped_count) * timer->m_Delay;
            timer->m_Remaining += offset_to_next_trigger;
            timer->m_PrevCycleExcess = timer->m_Delay - timer->m_Remaining;
            if (timer->m_Remaining < 0) {// If the delay is very small, the floating point precision might produce issues
                timer->m_Remaining = timer->m_Delay; // reset the timer
                timer->m_PrevCycleExcess = 0.0f;
            }
        }

        timer_world->m_InUpdate = 0;

        // We need to do the deletes in a separate pass, as the
        // callbacks may remove/add indices from the pool, thus messing up the current set of indices
        for (uint32_t i = 0; i < size; ++i)
        {
            uint16_t index = timer_world->m_ScratchBuffer[i];
            Timer* timer = GetTimerFromIndex(timer_world, index);
            if (timer && timer->m_IsAlive == 0)
            {
                FreeTimer(timer_world, timer);
            }
        }

        if (timer_world->m_IsDirty)
        {
            ++timer_world->m_Version;
        }
        timer_world->m_IsDirty = 0;
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
        timer->m_PrevCycleExcess = 0.0f;
        timer->m_IsAlive = 1;

        return timer->m_Handle;
    }

    bool CancelTimer(HTimerWorld timer_world, HTimer handle)
    {
        Timer* timer = GetTimerFromHandle(timer_world, handle);
        if (!timer)
        {
            return false;
        }
        if (timer->m_IsAlive == 0)
        {
            return false;
        }

        timer->m_IsAlive = 0;
        timer->m_Callback(timer_world, TIMER_EVENT_CANCELLED, timer->m_Handle, 0.f, timer->m_Owner, timer->m_UserData);

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

        uint32_t size = CopyIndices(timer_world->m_Instances, timer_world->m_ScratchBuffer);
        uint32_t cancelled_count = 0;
        for (uint32_t i = 0; i < size; ++i)
        {
            uint16_t index = timer_world->m_ScratchBuffer[i];
            Timer* timer = GetTimerFromIndex(timer_world, index);

            if (!timer)
            {
                continue;
            }

            if (timer->m_Owner != owner)
            {
                continue;
            }

            if (timer->m_IsAlive == 1)
            {
                timer->m_IsAlive = 0;
                ++cancelled_count;
            }

            if (timer_world->m_InUpdate == 0)
            {
                FreeTimer(timer_world, timer);
            }
        }

        return cancelled_count;
    }

    // For testing
    uint32_t GetAliveTimers(HTimerWorld timer_world)
    {
        assert(timer_world != 0x0);

        uint32_t alive_timers = 0u;
        uint32_t size = timer_world->m_Instances.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            uint16_t index = timer_world->m_Instances[i]; // the pool indices
            Timer* timer = GetTimerFromIndex(timer_world, index);

            alive_timers += timer->m_IsAlive != 0 ? 1 : 0;
        }
        return alive_timers;
    }

    static void SetTimerWorld(HScriptWorld script_world, HTimerWorld timer_world)
    {
        HContext context = GetScriptWorldContext(script_world);
        lua_pushinteger(context->m_LuaState, (lua_Integer)TIMER_WORLD_VALUE_KEY_HASH);
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

        lua_pushinteger(L, (lua_Integer)TIMER_WORLD_VALUE_KEY_HASH);
        GetScriptWorldContextValue(script_world);
        HTimerWorld timer_world = (HTimerWorld)lua_touserdata(L, -1);
        lua_pop(L, 1);
        return timer_world;
    }

    static void TimerNewScriptWorld(HScriptWorld script_world)
    {
        assert(script_world != 0x0);
        HContext context = GetScriptWorldContext(script_world);
        assert(context != 0x0);
        lua_State* L = context->m_LuaState;
        assert(L != 0x0);
        DM_LUA_STACK_CHECK(L, 0);

        HTimerWorld timer_world = NewTimerWorld();
        lua_pushinteger(L, (lua_Integer)TIMER_WORLD_VALUE_KEY_HASH);
        lua_pushlightuserdata(L, timer_world);
        SetScriptWorldContextValue(script_world);
    }

    static void TimerDeleteScriptWorld(HScriptWorld script_world)
    {
        assert(script_world != 0x0);
        HTimerWorld timer_world = GetTimerWorld(script_world);
        if (timer_world != 0x0)
        {
            SetTimerWorld(script_world, 0x0);
            DeleteTimerWorld(timer_world);
        }
    }

    static void TimerUpdateScriptWorld(HScriptWorld script_world, float dt)
    {
        assert(script_world != 0x0);
        HTimerWorld timer_world = GetTimerWorld(script_world);
        if (timer_world != 0x0)
        {
            UpdateTimers(timer_world, dt);
        }
    }

    static void TimerInitializeScriptInstance(HScriptWorld script_world)
    {
        HContext context = GetScriptWorldContext(script_world);
        lua_State* L = GetLuaState(context);
        DM_LUA_STACK_CHECK(L, 0);

        lua_pushinteger(L, (lua_Integer)TIMER_WORLD_VALUE_KEY_HASH);
        HTimerWorld timer_world = GetTimerWorld(script_world);
        lua_pushlightuserdata(L, timer_world);
        SetInstanceContextValue(L);
    }

    static void TimerFinalizeScriptInstance(HScriptWorld script_world)
    {
        HContext context = GetScriptWorldContext(script_world);
        lua_State* L = GetLuaState(context);
        DM_LUA_STACK_CHECK(L, 0);

        uintptr_t owner = dmScript::GetInstanceId(L);
        HTimerWorld timer_world = GetTimerWorld(script_world);
        KillTimers(timer_world, owner);

        lua_pushinteger(L, (lua_Integer)TIMER_WORLD_VALUE_KEY_HASH);
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
        if (!IsCallbackValid(callback))
        {
            return;
        }

        if (event_type != TIMER_EVENT_CANCELLED)
        {
            LuaTimerCallbackArgs args = { timer_handle, time_elapsed };

            InvokeCallback(callback, LuaTimerCallbackArgsCB, &args);
        }

        if ((event_type != TIMER_EVENT_TRIGGER_WILL_REPEAT) && IsCallbackValid(callback))
        {
            DestroyCallback(callback);

            Timer* timer = GetTimerFromHandle(timer_world, timer_handle);
            timer->m_UserData = 0;
        }
    }

    static dmScript::HTimerWorld GetTimerWorld(lua_State* L)
    {
        lua_pushinteger(L, (lua_Integer)TIMER_WORLD_VALUE_KEY_HASH);
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

    static dmScript::HTimerWorld CheckTimerWorld(lua_State* L)
    {
        dmScript::HTimerWorld timer_world = GetTimerWorld(L);
        if (timer_world == 0x0)
        {
            luaL_error(L, "Unable to trigger callback, the lua context does not have a timer world");
            return 0;
        }
        return timer_world;
    }

    /*# create a timer
     * Adds a timer and returns a unique handle.
     *
     * You may create more timers from inside a timer callback.
     *
     * Using a delay of 0 will result in a timer that triggers at the next frame just before
     * script update functions.
     *
     * If you want a timer that triggers on each frame, set delay to 0.0f and repeat to true.
     *
     * Timers created within a script will automatically die when the script is deleted.
     *
     * @name timer.delay
     * @param delay [type:number] time interval in seconds
     * @param repeating [type:boolean] true = repeat timer until cancel, false = one-shot timer
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
     * @return handle [type:number] identifier for the create timer, returns timer.INVALID_TIMER_HANDLE if the timer can not be created
     * @examples
     *
     * A simple one-shot timer
     * ```lua
     * timer.delay(1, false, function() print("print in one second") end)
     * ```
     *
     * Repetitive timer which canceled after 10 calls
     * ```lua
     * local function call_every_second(self, handle, time_elapsed)
     *   self.counter = self.counter + 1
     *   print("Call #", self.counter)
     *   if self.counter == 10 then
     *     timer.cancel(handle) -- cancel timer after 10 calls
     *   end
     * end
     *
     * self.counter = 0
     * timer.delay(1, true, call_every_second)
     * ```
     *
     */
    static int TimerDelay(lua_State* L) {
        int top = lua_gettop(L);
        luaL_checktype(L, 1, LUA_TNUMBER);
        luaL_checktype(L, 2, LUA_TBOOLEAN);
        luaL_checktype(L, 3, LUA_TFUNCTION);

        const double seconds = lua_tonumber(L, 1);
        if (seconds < 0.0)
        {
            return luaL_error(L, "timer.delay does not support negative delay times");
        }

        bool repeat = lua_toboolean(L, 2);
        dmScript::HTimerWorld timer_world = CheckTimerWorld(L);

        uintptr_t owner = dmScript::GetInstanceId(L);

        LuaCallbackInfo* user_data = dmScript::CreateCallback(L, 3);

        dmScript::HTimer handle = dmScript::AddTimer(timer_world, seconds, repeat, LuaTimerCallback, (uintptr_t)owner, (uintptr_t)user_data);

        lua_pushinteger(L, handle);
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# cancel a timer
     *
     * You may cancel a timer from inside a timer callback.
     * Cancelling a timer that is already executed or cancelled is safe.
     *
     * @name timer.cancel
     * @param handle [type:number] the timer handle returned by timer.delay()
     * @return true [type:boolean] if the timer was active, false if the timer is already cancelled / complete
     * @examples
     *
     * ```lua
     * self.handle = timer.delay(1, true, function() print("print every second") end)
     * ...
     * local result = timer.cancel(self.handle)
     * if not result then
     *    print("the timer is already cancelled")
     * end
     * ```
     *
     */
    static int TimerCancel(lua_State* L)
    {
        int top = lua_gettop(L);
        const int handle = luaL_checkint(L, 1);

        dmScript::HTimerWorld timer_world = CheckTimerWorld(L);

        bool cancelled = dmScript::CancelTimer(timer_world, (dmScript::HTimer)handle);
        lua_pushboolean(L, cancelled ? 1 : 0);
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# trigger a callback
     *
     * Manual triggering a callback for a timer.
     *
     * @name timer.trigger
     * @param handle [type:number] the timer handle returned by timer.delay()
     * @return true [type:boolean] if the timer was active, false if the timer is already cancelled / complete
     * @examples
     *
     * ```lua
     * self.handle = timer.delay(1, true, function() print("print every second or manually by timer.trigger") end)
     * ...
     * local result = timer.trigger(self.handle)
     * if not result then
     *    print("the timer is already cancelled or complete")
     * end
     * ```
     */
    static int TimerTrigger(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        const int timer_handle = luaL_checkint(L, 1);
        dmScript::HTimerWorld timer_world = CheckTimerWorld(L);

        Timer* timer = GetTimerFromHandle(timer_world, timer_handle);
        if (!timer)
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        LuaCallbackInfo* callback = (LuaCallbackInfo*)timer->m_UserData;
        if (!IsCallbackValid(callback))
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        LuaTimerCallbackArgs args = { timer->m_Handle, timer->m_Delay - timer->m_Remaining };
        InvokeCallback(callback, LuaTimerCallbackArgsCB, &args);

        lua_pushboolean(L, 1);
        return 1;
    }

    /*# get information about timer
     *
     * Get information about timer.
     *
     * @name  timer.get_info
     * @param handle [type:number] the timer handle returned by timer.delay()
     * @return data [type:table|nil] table or `nil` if timer is cancelled/completed. table with data in the following fields:
     *
     * `time_remaining`
     * : [type:number] Time remaining until the next time a timer.delay() fires.
     *
     * `delay`
     * : [type:number] Time interval.
     *
     * `repeating`
     * : [type:boolean] true = repeat timer until cancel, false = one-shot timer.
     * @examples
     *
     * ```lua
     * self.handle = timer.delay(1, true, function() print("print every second") end)
     * ...
     * local result = timer.get_info(self.handle)
     * if not result then
     *    print("the timer is already cancelled or complete")
     * else
     *    pprint(result) -- delay, time_remaining, repeating
     * end
     *
     * ```
     *
     */
    static int TimerGetInfo(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        const int timer_handle = luaL_checkint(L, 1);
        dmScript::HTimerWorld timer_world = CheckTimerWorld(L);

        Timer* timer = GetTimerFromHandle(timer_world, timer_handle);
        if (!timer)
        {
            lua_pushnil(L);
            return 1;
        }

        lua_newtable(L);
        lua_pushnumber(L,timer->m_Remaining);
        lua_setfield(L, -2, "time_remaining");
        lua_pushnumber(L,timer->m_Delay);
        lua_setfield(L, -2, "delay");
        lua_pushboolean(L,timer->m_Repeat==1);
        lua_setfield(L, -2, "repeating");
        return 1;
    }

    static const luaL_reg TIMER_COMP_FUNCTIONS[] = {
        { "delay", TimerDelay },
        { "cancel", TimerCancel },
        { "trigger", TimerTrigger},
        { "get_info", TimerGetInfo},
        { 0, 0 }
    };

    void TimerInitialize(HContext context)
    {
        lua_State* L = context->m_LuaState;
        DM_LUA_STACK_CHECK(L, 0);

        luaL_register(L, "timer", TIMER_COMP_FUNCTIONS);

        #define SETCONSTANT(name) \
            lua_pushnumber(L, (lua_Number) name); \
            lua_setfield(L, -2, #name);\

        /*# Indicates an invalid timer handle
         *
         * @name timer.INVALID_TIMER_HANDLE
         * @constant
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
        sl.FixedUpdateScriptWorld = 0;
        sl.InitializeScriptInstance = TimerInitializeScriptInstance;
        sl.FinalizeScriptInstance = TimerFinalizeScriptInstance;
        RegisterScriptExtension(context, &sl);
    }
}
