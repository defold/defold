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

#include "debugger_private.h"
#include <dlib/dstrings.h>
#include <dlib/time.h>
#include <stdlib.h>
#include <string.h>

namespace dmDebugger
{
    static char g_DebuggerKey;
    static char g_StateKey;
    static void Disconnect(Debugger* d);
    static void SetHooks(Debugger* d, bool enable);
    static void RefreshSources(Debugger* d);

    Debugger::Debugger()
        : m_Listener(dmSocket::INVALID_SOCKET_HANDLE)
        , m_Client(dmSocket::INVALID_SOCKET_HANDLE)
        , m_UserdataTableResolver(0)
        , m_UserdataFormatter(0)
        , m_LocalRoot(0)
        , m_CloseDeadline(0)
        , m_Sequence(1)
        , m_NextId(1)
        , m_AttachSeq(0)
        , m_StoppedThread(0)
        , m_StepThread(0)
        , m_StepDepth(0)
        , m_Connections(0)
        , m_EvaluationTimeout(1000)
        , m_Step(STEP_NONE)
        , m_Port(0)
        , m_StepNativeTailCall(false)
        , m_Initialized(false)
        , m_Attached(false)
        , m_Configured(false)
        , m_Paused(false)
        , m_PauseRequested(false)
        , m_StopOnEntry(false)
        , m_BreakOnError(false)
        , m_Evaluating(false)
        , m_Updating(false)
        , m_ClosePending(false)
        , m_LinesStartAt1(true)
        , m_ColumnsStartAt1(true)
        , m_VariableType(false)
        , m_VariablePaging(false)
        , m_InvalidatedEvent(false)
    {
    }

    static void SetPointer(lua_State* L, void* key, void* value)
    {
        lua_pushlightuserdata(L, key);
        if (value)
            lua_pushlightuserdata(L, value);
        else
            lua_pushnil(L);
        lua_rawset(L, LUA_REGISTRYINDEX);
    }
    static void* GetPointer(lua_State* L, void* key)
    {
        lua_pushlightuserdata(L, key);
        lua_rawget(L, LUA_REGISTRYINDEX);
        void* value = lua_touserdata(L, -1);
        lua_pop(L, 1);
        return value;
    }

    static int NewWeakTable(lua_State* L, const char* mode)
    {
        lua_newtable(L);
        lua_newtable(L);
        lua_pushstring(L, mode);
        lua_setfield(L, -2, "__mode");
        lua_setmetatable(L, -2);
        return luaL_ref(L, LUA_REGISTRYINDEX);
    }

    void Queue(Debugger* d, Buffer& message)
    {
        if (!message.m_Valid || message.Size() > MAX_MESSAGE_SIZE)
        {
            d->m_ClosePending = true;
            return;
        }
        d->m_Output.Format("Content-Length: %u\r\n\r\n", message.Size());
        d->m_Output.Add(message.Data(), message.Size());
        if (!d->m_Output.m_Valid)
            d->m_ClosePending = true;
    }
    void Respond(Debugger* d, uint32_t seq, const char* command, const Buffer* body, const char* error)
    {
        Buffer message;
        uint32_t response_seq = d->m_Sequence++;
        message.Format("{\"seq\":%u,\"type\":\"response\",\"request_seq\":%u,\"command\":", response_seq, seq);
        message.String(command);
        message.Add(error ? ",\"success\":false,\"message\":" : ",\"success\":true");
        if (error)
            message.String(error);
        if (body)
        {
            message.Add(",\"body\":");
            message.Add(body->Data(), body->Size());
            if (!body->m_Valid)
                message.m_Valid = false;
        }
        message.Add("}");
        if (!message.m_Valid || message.Size() > MAX_MESSAGE_SIZE)
        {
            // A valid inspection request can produce more data than one DAP
            // message allows. Fail that request without detaching and resuming
            // the application; the client can retry with paging or a slice.
            message.Clear();
            message.Format("{\"seq\":%u,\"type\":\"response\",\"request_seq\":%u,\"command\":", response_seq, seq);
            message.String(command);
            message.Add(",\"success\":false,\"message\":\"Response exceeds 1 MiB; request fewer variables or evaluate a smaller value\"}");
        }
        Queue(d, message);
    }
    void Event(Debugger* d, const char* event, const Buffer* body)
    {
        Buffer message;
        message.Format("{\"seq\":%u,\"type\":\"event\",\"event\":", d->m_Sequence++);
        message.String(event);
        if (body)
        {
            message.Add(",\"body\":");
            message.Add(body->Data(), body->Size());
            if (!body->m_Valid)
                message.m_Valid = false;
        }
        message.Add("}");
        Queue(d, message);
    }
    void Output(Debugger* d, const char* text)
    {
        Buffer body;
        body.Add("{\"category\":\"console\",\"output\":");
        body.String(text);
        body.Add("}");
        Event(d, "output", &body);
    }

    lua_State* GetThread(Thread* thread)
    {
        lua_State* L = thread->m_State->m_L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, thread->m_State->m_ThreadsRef);
        lua_rawgeti(L, -1, thread->m_Id);
        lua_State* result = lua_tothread(L, -1);
        lua_pop(L, 2);
        return result;
    }
    Thread* FindThread(Debugger* d, uint32_t id, bool include_exited)
    {
        Thread** entry = d->m_ThreadIds.Get(id);
        Thread* thread = entry ? *entry : 0;
        return thread && (include_exited || (!thread->m_Exited && GetThread(thread))) ? thread : 0;
    }

    static void InstallHook(Thread* thread)
    {
        lua_State* L = GetThread(thread);
        if (!L || thread->m_Hooked)
            return;
        thread->m_OldHook = lua_gethook(L);
        thread->m_OldMask = lua_gethookmask(L);
        thread->m_OldCount = lua_gethookcount(L);
        // Lua 5.1 inherits hooks when creating threads. LuaJIT shares hooks
        // between threads. Neither should save our hook as the previous owner.
        if (thread->m_OldHook == Hook)
        {
            Thread* main = thread->m_State->m_Main;
            thread->m_OldHook = main->m_OldHook;
            thread->m_OldMask = main->m_OldMask;
            thread->m_OldCount = main->m_OldCount;
        }
        thread->m_Hooked = true;
        lua_sethook(L, Hook, LUA_MASKLINE | LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1000);
    }

    static void ThreadEvent(Debugger* d, Thread* thread, const char* reason)
    {
        if (!d->m_Attached)
            return;
        Buffer body;
        body.Format("{\"reason\":\"%s\",\"threadId\":%u}", reason, thread->m_Id);
        Event(d, "thread", &body);
    }

    Thread* TrackThread(Debugger* d, lua_State* L)
    {
        State* state = (State*)GetPointer(L, &g_StateKey);
        if (!state)
            return 0;
        // Weak thread keys preserve Lua's object identity without keeping
        // coroutines alive or confusing a reused lua_State address with its
        // previous occupant. Hook lookups do not scan other live threads.
        lua_rawgeti(L, LUA_REGISTRYINDEX, state->m_ThreadLookupRef);
        lua_pushthread(L);
        lua_rawget(L, -2);
        Thread* thread = (Thread*)lua_touserdata(L, -1);
        lua_pop(L, 2);
        if (thread)
            return thread;
        thread = new Thread();
        thread->m_State = state;
        thread->m_Id = d->m_NextId++;
        thread->m_Main = state->m_L == L;
        if (thread->m_Main)
            state->m_Main = thread;
        lua_rawgeti(L, LUA_REGISTRYINDEX, state->m_ThreadsRef);
        lua_pushthread(L);
        lua_rawseti(L, -2, thread->m_Id);
        lua_pop(L, 1);
        lua_rawgeti(L, LUA_REGISTRYINDEX, state->m_ThreadLookupRef);
        lua_pushthread(L);
        lua_pushlightuserdata(L, thread);
        lua_rawset(L, -3);
        lua_pop(L, 1);
        if (d->m_ThreadIds.Full())
            d->m_ThreadIds.SetCapacity(d->m_ThreadIds.Capacity() ? d->m_ThreadIds.Capacity() * 2 : 16);
        d->m_ThreadIds.Put(thread->m_Id, thread);
        Push(d->m_Threads, thread);
        if (d->m_Attached)
            InstallHook(thread);
        ThreadEvent(d, thread, "started");
        return thread;
    }

    struct ThreadDiscovery
    {
        lua_State* m_L;
        int        m_Seen;
        int        m_Queue;
        int        m_Count;

        ThreadDiscovery(lua_State* L)
            : m_L(L)
            , m_Count(0)
        {
            lua_newtable(L);
            m_Seen = lua_gettop(L);
            lua_newtable(L);
            m_Queue = lua_gettop(L);
            // Walking the active thread also sees our temporary stack values.
            // Do not traverse the discovery tables themselves.
            lua_pushvalue(L, m_Seen);
            lua_pushboolean(L, true);
            lua_rawset(L, m_Seen);
            lua_pushvalue(L, m_Queue);
            lua_pushboolean(L, true);
            lua_rawset(L, m_Seen);
        }

        void Add(int index)
        {
            int type = lua_type(m_L, index);
            if (type != LUA_TTABLE && type != LUA_TFUNCTION && type != LUA_TTHREAD && type != LUA_TUSERDATA)
                return;
            if (index < 0 && index > LUA_REGISTRYINDEX)
                index += lua_gettop(m_L) + 1;
            lua_pushvalue(m_L, index);
            lua_rawget(m_L, m_Seen);
            bool seen = !lua_isnil(m_L, -1);
            lua_pop(m_L, 1);
            if (seen)
                return;
            lua_pushvalue(m_L, index);
            lua_pushboolean(m_L, true);
            lua_rawset(m_L, m_Seen);
            lua_pushvalue(m_L, index);
            lua_rawseti(m_L, m_Queue, ++m_Count);
        }

        void PopFrom(lua_State* L)
        {
            lua_xmove(L, m_L, 1);
            Add(-1);
            lua_pop(m_L, 1);
        }
    };

    static void DiscoverThreads(Debugger* d, lua_State* L)
    {
        int             top = lua_gettop(L);
        ThreadDiscovery discovery(L);
        discovery.Add(LUA_REGISTRYINDEX);
        discovery.Add(LUA_GLOBALSINDEX);
        lua_pushthread(L);
        discovery.PopFrom(L);

        // Scan reachable Lua objects at activation, attachment, and detachment.
        // Raw inspection avoids invoking application code, and a queue handles
        // cycles without growing the native call stack. The temporary tables pin
        // objects only for this scan; TrackThread retains them through weak values.
        for (int i = 1; i <= discovery.m_Count; ++i)
        {
            lua_rawgeti(L, discovery.m_Queue, i);
            int object = lua_gettop(L);
            int type = lua_type(L, object);
            if (type == LUA_TTABLE)
            {
                lua_pushnil(L);
                while (lua_next(L, object))
                {
                    discovery.Add(-2);
                    discovery.Add(-1);
                    lua_pop(L, 1);
                }
            }
            else if (type == LUA_TFUNCTION)
            {
                for (int upvalue = 1; lua_getupvalue(L, object, upvalue); ++upvalue)
                    discovery.PopFrom(L);
            }
            else if (type == LUA_TTHREAD)
            {
                lua_State* thread = lua_tothread(L, object);
                if (lua_checkstack(thread, 8))
                {
                    TrackThread(d, thread);
                    int thread_top = thread == L ? top : lua_gettop(thread);
                    for (int slot = 1; slot <= thread_top; ++slot)
                    {
                        lua_pushvalue(thread, slot);
                        discovery.PopFrom(thread);
                    }
                    lua_Debug frame;
                    for (int level = 0; lua_getstack(thread, level, &frame); ++level)
                    {
                        lua_getinfo(thread, "f", &frame);
                        discovery.PopFrom(thread);
                        for (int local = 1; lua_getlocal(thread, &frame, local); ++local)
                            discovery.PopFrom(thread);
                    }
                }
            }
            if (type == LUA_TFUNCTION || type == LUA_TUSERDATA || type == LUA_TTHREAD)
            {
                lua_getfenv(L, object);
                discovery.PopFrom(L);
            }
            if (lua_getmetatable(L, object))
                discovery.PopFrom(L);
            lua_pop(L, 1);
        }
        lua_settop(L, top);
    }

    static void Track(lua_State* L, lua_State* coroutine)
    {
        Debugger* d = (Debugger*)GetPointer(L, &g_DebuggerKey);
        if (d && !d->m_Evaluating && coroutine)
            TrackThread(d, coroutine);
    }
    static bool HasEnded(Thread* thread, lua_State* L)
    {
        if (thread->m_Main)
            return false;
        if (!L)
            return true;
        if (lua_status(L) == LUA_YIELD)
            return false;
        if (lua_status(L) != 0)
            return true;
        lua_Debug ar;
        return lua_gettop(L) == 0 && !lua_getstack(L, 0, &ar);
    }
    static void CoroutineReturned(Debugger* d, lua_State* L, Thread* thread)
    {
        if (HasEnded(thread, GetThread(thread)) && !thread->m_Exited)
        {
            thread->m_Exited = true;
            ThreadEvent(d, thread, "exited");
        }
        if (d->m_Step != STEP_NONE && d->m_StepThread == thread->m_Id && (d->m_Step == STEP_OUT || thread->m_Exited))
        {
            Thread* parent = TrackThread(d, L);
            d->m_Step = STEP_IN;
            d->m_StepThread = parent->m_Id;
            d->m_StepDepth = StackDepth(L);
        }
    }
    static void CoroutineReturned(lua_State* L, lua_State* coroutine)
    {
        Debugger* d = (Debugger*)GetPointer(L, &g_DebuggerKey);
        if (!d || d->m_Evaluating || !coroutine)
            return;
        Thread* thread = TrackThread(d, coroutine);
        if (thread)
            CoroutineReturned(d, L, thread);
    }
    static int CoroutineCreate(lua_State* L)
    {
        int args = lua_gettop(L);
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_insert(L, 1);
        lua_call(L, args, 1);
        Track(L, lua_tothread(L, -1));
        return 1;
    }
    static int CoroutineResume(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TTHREAD);
        lua_State* coroutine = lua_tothread(L, 1);
        Track(L, coroutine);
        lua_pushvalue(L, 1);
        int ref = luaL_ref(L, LUA_REGISTRYINDEX);
        int args = lua_gettop(L);
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_insert(L, 1);
        lua_call(L, args, LUA_MULTRET);
        CoroutineReturned(L, coroutine);
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        return lua_gettop(L);
    }
    static int WrappedResume(lua_State* L)
    {
        lua_State* coroutine = lua_tothread(L, lua_upvalueindex(1));
        Track(L, coroutine);
        // Delegate to the original resume function so each runtime retains its
        // argument/result checks and C-call depth bookkeeping.
        int args = lua_gettop(L);
        lua_pushvalue(L, lua_upvalueindex(2));
        lua_insert(L, 1);
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_insert(L, 2);
        lua_call(L, args + 1, LUA_MULTRET);
        CoroutineReturned(L, coroutine);
        bool success = lua_toboolean(L, 1) != 0;
        lua_remove(L, 1);
        if (!success)
        {
            if (lua_isstring(L, -1))
            {
                luaL_where(L, 1);
                lua_insert(L, -2);
                lua_concat(L, 2);
            }
            return lua_error(L);
        }
        return lua_gettop(L);
    }
    static int CoroutineWrap(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TFUNCTION);
        lua_State* coroutine = lua_newthread(L);
        lua_pushvalue(L, 1);
        lua_xmove(L, coroutine, 1);
        Track(L, coroutine);
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_pushcclosure(L, WrappedResume, 2);
        return 1;
    }

    static int Replace(lua_State* L, const char* name, lua_CFunction wrapper, int delegate_ref = LUA_NOREF)
    {
        lua_getfield(L, -1, name);
        int ref = luaL_ref(L, LUA_REGISTRYINDEX);
        lua_rawgeti(L, LUA_REGISTRYINDEX, delegate_ref == LUA_NOREF ? ref : delegate_ref);
        lua_pushcclosure(L, wrapper, 1);
        lua_setfield(L, -2, name);
        return ref;
    }
    static void Restore(lua_State* L, const char* name, lua_CFunction wrapper, int ref)
    {
        lua_getfield(L, -1, name);
        bool ours = lua_iscfunction(L, -1) && lua_tocfunction(L, -1) == wrapper;
        lua_pop(L, 1);
        if (ours)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            lua_setfield(L, -2, name);
        }
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
    }

    static bool JitEnabled(lua_State* L, bool* available)
    {
        int  top = lua_gettop(L);
        bool enabled = false;
        *available = false;
        lua_getglobal(L, "jit");
        if (lua_istable(L, -1))
        {
            lua_getfield(L, -1, "status");
            if (lua_isfunction(L, -1) && lua_pcall(L, 0, 1, 0) == 0)
            {
                *available = true;
                enabled = lua_toboolean(L, -1) != 0;
            }
        }
        lua_settop(L, top);
        return enabled;
    }
    static void Jit(lua_State* L, const char* command)
    {
        int top = lua_gettop(L);
        lua_getglobal(L, "jit");
        if (lua_istable(L, -1))
        {
            lua_getfield(L, -1, command);
            if (lua_isfunction(L, -1))
                lua_pcall(L, 0, 0, 0);
        }
        lua_settop(L, top);
    }
    static void SetHooks(Debugger* d, bool enable)
    {
        for (uint32_t i = 0; i < d->m_States.Size(); ++i)
        {
            State* state = d->m_States[i];
            if (enable)
            {
                // Source mapping can change on reattachment. Function keys stay
                // weak so observing a closure never extends its lifetime.
                luaL_unref(state->m_L, LUA_REGISTRYINDEX, state->m_ObservedFunctionsRef);
                state->m_ObservedFunctionsRef = NewWeakTable(state->m_L, "k");
            }
            // Cached coroutine APIs and evaluation can bypass tracking. Discover
            // these threads both before installing hooks and before restoring
            // hooks inherited from the debugger in Lua 5.1.
            if (enable || state->m_Main->m_Hooked)
                DiscoverThreads(d, state->m_L);
            if (enable)
            {
                state->m_JitEnabled = JitEnabled(state->m_L, &state->m_JitAvailable);
                Jit(state->m_L, "off");
                Jit(state->m_L, "flush");
            }
        }
        for (uint32_t i = 0; i < d->m_Threads.Size(); ++i)
        {
            Thread*    thread = d->m_Threads[i];
            lua_State* L = GetThread(thread);
            if (enable)
                InstallHook(thread);
            else
            {
                if (L && lua_gethook(L) == Hook)
                {
                    // Newly discovered threads inherited our hook without
                    // saving its previous owner. Match InstallHook's fallback.
                    Thread* original = thread->m_Hooked ? thread : thread->m_State->m_Main;
                    lua_sethook(L, original->m_OldHook, original->m_OldMask, original->m_OldCount);
                }
                thread->m_Hooked = false;
            }
        }
        if (!enable)
            for (uint32_t i = 0; i < d->m_States.Size(); ++i)
                if (d->m_States[i]->m_JitEnabled)
                {
                    Jit(d->m_States[i]->m_L, "on");
                    d->m_States[i]->m_JitEnabled = false;
                }
    }

    void AddLuaState(HDebugger d, lua_State* L, const char* name)
    {
        if (!d || GetPointer(L, &g_DebuggerKey))
            return;
        State* state = new State();
        memset(state, 0, sizeof(*state));
        state->m_L = L;
        state->m_Name = strdup(name);
        state->m_ThreadsRef = NewWeakTable(L, "v");
        state->m_ThreadLookupRef = NewWeakTable(L, "k");
        state->m_ObservedFunctionsRef = NewWeakTable(L, "k");
        SetPointer(L, &g_DebuggerKey, d);
        SetPointer(L, &g_StateKey, state);
        Push(d->m_States, state);
        if (d->m_Attached)
        {
            state->m_JitEnabled = JitEnabled(L, &state->m_JitAvailable);
            Jit(L, "off");
            Jit(L, "flush");
        }
        TrackThread(d, L);
        DiscoverThreads(d, L);
        lua_getglobal(L, "coroutine");
        state->m_CreateRef = state->m_ResumeRef = state->m_WrapRef = LUA_NOREF;
        if (lua_istable(L, -1))
        {
            state->m_CreateRef = Replace(L, "create", CoroutineCreate);
            state->m_ResumeRef = Replace(L, "resume", CoroutineResume);
            state->m_WrapRef = Replace(L, "wrap", CoroutineWrap, state->m_ResumeRef);
        }
        lua_pop(L, 1);
    }

    void RemoveLuaState(HDebugger d, lua_State* L)
    {
        if (!d)
            return;
        State* state = (State*)GetPointer(L, &g_StateKey);
        if (!state)
            return;
        ClearReferences(d);
        for (uint32_t i = 0; i < d->m_Threads.Size();)
        {
            Thread* thread = d->m_Threads[i];
            if (thread->m_State != state)
            {
                ++i;
                continue;
            }
            lua_State* T = GetThread(thread);
            if (T && thread->m_Hooked && lua_gethook(T) == Hook)
                lua_sethook(T, thread->m_OldHook, thread->m_OldMask, thread->m_OldCount);
            if (!thread->m_Exited)
                ThreadEvent(d, thread, "exited");
            d->m_ThreadIds.Erase(thread->m_Id);
            delete thread;
            d->m_Threads.EraseSwap(i);
        }
        if (state->m_JitEnabled)
            Jit(L, "on");
        lua_getglobal(L, "coroutine");
        if (lua_istable(L, -1) && state->m_CreateRef != LUA_NOREF)
        {
            Restore(L, "create", CoroutineCreate, state->m_CreateRef);
            Restore(L, "resume", CoroutineResume, state->m_ResumeRef);
            Restore(L, "wrap", CoroutineWrap, state->m_WrapRef);
        }
        else
        {
            luaL_unref(L, LUA_REGISTRYINDEX, state->m_CreateRef);
            luaL_unref(L, LUA_REGISTRYINDEX, state->m_ResumeRef);
            luaL_unref(L, LUA_REGISTRYINDEX, state->m_WrapRef);
        }
        lua_pop(L, 1);
        luaL_unref(L, LUA_REGISTRYINDEX, state->m_ThreadsRef);
        luaL_unref(L, LUA_REGISTRYINDEX, state->m_ThreadLookupRef);
        luaL_unref(L, LUA_REGISTRYINDEX, state->m_ObservedFunctionsRef);
        SetPointer(L, &g_DebuggerKey, 0);
        SetPointer(L, &g_StateKey, 0);
        for (uint32_t i = 0; i < d->m_States.Size(); ++i)
            if (d->m_States[i] == state)
            {
                d->m_States.EraseSwap(i);
                break;
            }
        free(state->m_Name);
        delete state;
        if (d->m_Attached)
            RefreshSources(d);
    }

    static void FreeBreakpoint(Breakpoint* bp)
    {
        free(bp->m_Path);
        free(bp->m_Condition);
        free(bp->m_LogMessage);
        delete bp;
    }
    static void Disconnect(Debugger* d)
    {
        if (d->m_Client != dmSocket::INVALID_SOCKET_HANDLE)
            dmSocket::Delete(d->m_Client);
        d->m_Client = dmSocket::INVALID_SOCKET_HANDLE;
        // Detach discovery must not install hooks or emit thread-start events.
        d->m_Attached = false;
        SetHooks(d, false);
        for (uint32_t i = 0; i < d->m_Threads.Size(); ++i)
            d->m_Threads[i]->m_CallSites.SetSize(0);
        ClearReferences(d);
        d->m_Input.Clear();
        d->m_Output.Clear();
        d->m_Exception.Clear();
        d->m_Initialized = d->m_Configured = d->m_Paused = false;
        d->m_PauseRequested = d->m_ClosePending = d->m_BreakOnError = d->m_StopOnEntry = false;
        d->m_Step = STEP_NONE;
        d->m_AttachSeq = 0;
        d->m_CloseDeadline = 0;
        free(d->m_LocalRoot);
        d->m_LocalRoot = 0;
        for (uint32_t i = 0; i < d->m_Breakpoints.Size(); ++i)
            FreeBreakpoint(d->m_Breakpoints[i]);
        d->m_Breakpoints.SetSize(0);
        for (uint32_t i = 0; i < d->m_Sources.Size(); ++i)
        {
            free(d->m_Sources[i]->m_Path);
            delete d->m_Sources[i];
        }
        d->m_Sources.SetSize(0);
    }

    HDebugger New(uint16_t port, const char* address)
    {
        Debugger*         d = new Debugger();
        dmSocket::Address bind_address;
        if (dmSocket::GetHostByName(address, &bind_address, true, false) != dmSocket::RESULT_OK ||
            dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &d->m_Listener) != dmSocket::RESULT_OK ||
            dmSocket::SetReuseAddress(d->m_Listener, true) != dmSocket::RESULT_OK ||
            dmSocket::Bind(d->m_Listener, bind_address, port) != dmSocket::RESULT_OK ||
            dmSocket::Listen(d->m_Listener, 1) != dmSocket::RESULT_OK ||
            dmSocket::SetBlocking(d->m_Listener, false) != dmSocket::RESULT_OK ||
            dmSocket::GetName(d->m_Listener, &bind_address, &d->m_Port) != dmSocket::RESULT_OK)
        {
            Delete(d);
            return 0;
        }
        return d;
    }
    uint16_t GetPort(HDebugger d)
    {
        return d->m_Port;
    }
    void SetUserdataTableResolver(HDebugger d, UserdataTableResolver resolver)
    {
        d->m_UserdataTableResolver = resolver;
    }
    void SetUserdataFormatter(HDebugger d, UserdataFormatter formatter)
    {
        d->m_UserdataFormatter = formatter;
    }
    void Delete(HDebugger d)
    {
        if (!d)
            return;
        if (d->m_Attached)
        {
            Event(d, "terminated");
            Update(d);
        }
        Disconnect(d);
        while (d->m_States.Size())
            RemoveLuaState(d, d->m_States[0]->m_L);
        if (d->m_Listener != dmSocket::INVALID_SOCKET_HANDLE)
            dmSocket::Delete(d->m_Listener);
        delete d;
    }

    static void NormalizePath(const char* path, Buffer& result)
    {
        if (*path == '@')
            ++path;
        while (path[0] == '.' && (path[1] == '/' || path[1] == '\\'))
            path += 2;
        if ((path[0] == '/' || path[0] == '\\') && (path[1] == '/' || path[1] == '\\'))
        {
            // Keep the UNC server/share prefix; collapsing it changes a
            // network path into a path on the client's current drive.
            result.Add("//");
            path += 2;
        }
        else if (path[0] >= 'A' && path[0] <= 'Z' && path[1] == ':')
        {
            // DAP clients may lowercase Windows drive letters independently of localRoot.
            char drive = *path++ + ('a' - 'A');
            result.Add(&drive, 1);
        }
        for (; *path; ++path)
        {
            char c = *path == '\\' ? '/' : *path;
            if (c == '/' && result.Size() && result.Data()[result.Size() - 1] == '/')
                continue;
            result.Add(&c, 1);
        }
        while (result.Size() > 1 && result.Data()[result.Size() - 1] == '/')
            result.m_Data.SetSize(result.Size() - 1);
        if (result.Size())
            result.m_Data.Begin()[result.Size()] = 0;
    }
    static void RuntimePath(Debugger* d, const char* path, Buffer& result)
    {
        Buffer normalized;
        NormalizePath(path, normalized);
        const char* p = normalized.Data();
        if (d->m_LocalRoot)
        {
            size_t size = strlen(d->m_LocalRoot);
            if (!strncmp(p, d->m_LocalRoot, size) && p[size] == '/')
                p += size;
        }
        if (*p != '/' && !(p[0] && p[1] == ':'))
            result.Add("/");
        result.Add(p);
    }
    void ClientPath(Debugger* d, const char* source, Buffer& path)
    {
        Buffer normalized;
        NormalizePath(source, normalized);
        if (d->m_LocalRoot && strncmp(normalized.Data(), d->m_LocalRoot, strlen(d->m_LocalRoot)))
        {
            path.Add(d->m_LocalRoot);
            if (normalized.Data()[0] != '/')
                path.Add("/");
        }
        path.Add(normalized.Data());
    }

    static int CompareLines(const void* a, const void* b)
    {
        int left = *(const int*)a, right = *(const int*)b;
        return (left > right) - (left < right);
    }

    static bool HasLine(Source* source, int line)
    {
        uint32_t first = 0, last = source->m_Lines.Size();
        while (first < last)
        {
            uint32_t middle = first + (last - first) / 2;
            if (source->m_Lines[middle] == line)
                return true;
            if (source->m_Lines[middle] < line)
                first = middle + 1;
            else
                last = middle;
        }
        return false;
    }
    static void BreakpointBody(Debugger* d, Breakpoint* bp, Buffer& body)
    {
        body.Format("{\"id\":%u,\"verified\":%s,\"line\":%d", bp->m_Id, bp->m_Verified ? "true" : "false", bp->m_Line - !d->m_LinesStartAt1);
        if (!bp->m_Verified)
            body.Add(",\"message\":\"Pending an executable Lua line\"");
        body.Add("}");
    }
    static void VerifyBreakpoint(Debugger* d, Breakpoint* bp, bool verified = true)
    {
        if (bp->m_Verified == verified)
            return;
        bp->m_Verified = verified;
        Buffer body;
        body.Add("{\"reason\":\"changed\",\"breakpoint\":");
        BreakpointBody(d, bp, body);
        body.Add("}");
        Event(d, "breakpoint", &body);
    }
    static Source* FindSource(Debugger* d, const char* path)
    {
        for (uint32_t i = 0; i < d->m_Sources.Size(); ++i)
            if (!strcmp(d->m_Sources[i]->m_Path, path))
                return d->m_Sources[i];
        return 0;
    }
    static void RefreshSources(Debugger* d)
    {
        // Rebuild the union at stops and source requests, not in the hot hook
        // path. Weak function keys let collected code disappear, while live
        // closures and other Lua states still contribute their executable lines.
        for (uint32_t i = 0; i < d->m_Sources.Size(); ++i)
            d->m_Sources[i]->m_Lines.SetSize(0);
        for (uint32_t i = 0; i < d->m_States.Size(); ++i)
        {
            State* state = d->m_States[i];
            lua_State* L = state->m_L;
            lua_rawgeti(L, LUA_REGISTRYINDEX, state->m_ObservedFunctionsRef);
            lua_pushnil(L);
            while (lua_next(L, -2))
            {
                lua_getfield(L, -1, "source");
                const char* path = lua_tostring(L, -1);
                Source* source = FindSource(d, path);
                if (!source)
                {
                    source = new Source();
                    source->m_Path = strdup(path);
                    Push(d->m_Sources, source);
                }
                lua_pop(L, 1);
                lua_pushnil(L);
                while (lua_next(L, -2))
                {
                    if (lua_type(L, -2) == LUA_TNUMBER)
                        Push(source->m_Lines, (int)lua_tointeger(L, -2));
                    lua_pop(L, 1);
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
        for (uint32_t i = 0; i < d->m_Sources.Size();)
        {
            Source* source = d->m_Sources[i];
            if (!source->m_Lines.Size())
            {
                free(source->m_Path);
                delete source;
                d->m_Sources.EraseSwap(i);
                continue;
            }
            qsort(source->m_Lines.Begin(), source->m_Lines.Size(), sizeof(int), CompareLines);
            uint32_t count = 0;
            for (uint32_t j = 0; j < source->m_Lines.Size(); ++j)
                if (!count || source->m_Lines[j] != source->m_Lines[count - 1])
                    source->m_Lines[count++] = source->m_Lines[j];
            source->m_Lines.SetSize(count);
            ++i;
        }
        for (uint32_t i = 0; i < d->m_Breakpoints.Size(); ++i)
        {
            Breakpoint* bp = d->m_Breakpoints[i];
            Source* source = FindSource(d, bp->m_Path);
            VerifyBreakpoint(d, bp, source && HasLine(source, bp->m_Line));
        }
    }
    static void ObserveSource(Debugger* d, lua_State* L, lua_Debug* ar)
    {
        if (!ar->source || ar->source[0] != '@' || !strcmp(ar->what, "C"))
            return;
        State* state = (State*)GetPointer(L, &g_StateKey);
        lua_rawgeti(L, LUA_REGISTRYINDEX, state->m_ObservedFunctionsRef);
        lua_getinfo(L, "f", ar);
        lua_pushvalue(L, -1);
        lua_rawget(L, -3);
        bool observed = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        if (observed)
        {
            lua_pop(L, 2);
            return;
        }
        Buffer path;
        RuntimePath(d, ar->source, path);
        lua_getinfo(L, "L", ar);
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 3);
            return;
        }
        // Keep the line set as the weak cache value. It never references the
        // function key, so recording source information cannot retain closures.
        lua_pushstring(L, path.Data());
        lua_setfield(L, -2, "source");
        for (uint32_t i = 0; i < d->m_Breakpoints.Size(); ++i)
        {
            Breakpoint* bp = d->m_Breakpoints[i];
            if (!strcmp(bp->m_Path, path.Data()))
            {
                lua_rawgeti(L, -1, bp->m_Line);
                if (lua_toboolean(L, -1))
                    VerifyBreakpoint(d, bp);
                lua_pop(L, 1);
            }
        }
        lua_rawset(L, -3);
        lua_pop(L, 1);
    }

    static void SetBreakpoints(Debugger* d, const Json& json, int seq, int args)
    {
        const char* source = json.String(json.Field(json.Field(args, "source"), "path"));
        int         breakpoints = json.Field(args, "breakpoints");
        if (!source || !source[0] || (breakpoints >= 0 && json.Get(breakpoints).m_Type != JSON_ARRAY))
        {
            Respond(d, seq, "setBreakpoints", 0, "Expected source.path and breakpoints array");
            return;
        }
        // Validate the complete replacement before modifying the current list.
        for (int i = json.Get(breakpoints).m_First; i >= 0; i = json.Get(i).m_Next)
        {
            int line = json.Integer(json.Field(i, "line"));
            if (line < (d->m_LinesStartAt1 ? 1 : 0) || line == 2147483647)
            {
                Respond(d, seq, "setBreakpoints", 0, "Invalid breakpoint line");
                return;
            }
            const char* fields[] = { "condition", "hitCondition", "logMessage" };
            for (uint32_t f = 0; f < sizeof(fields) / sizeof(fields[0]); ++f)
            {
                int field = json.Field(i, fields[f]);
                if (field >= 0 && json.Get(field).m_Type != JSON_STRING)
                {
                    Respond(d, seq, "setBreakpoints", 0, "Breakpoint expressions and log messages must be strings");
                    return;
                }
            }
        }
        Buffer path;
        RuntimePath(d, source, path);
        RefreshSources(d);
        for (uint32_t i = 0; i < d->m_Breakpoints.Size();)
        {
            if (!strcmp(d->m_Breakpoints[i]->m_Path, path.Data()))
            {
                FreeBreakpoint(d->m_Breakpoints[i]);
                d->m_Breakpoints.EraseSwap(i);
            }
            else
                ++i;
        }
        Buffer body;
        body.Add("{\"breakpoints\":[");
        int count = 0;
        for (int i = json.Get(breakpoints).m_First; i >= 0; i = json.Get(i).m_Next)
        {
            if (count++)
                body.Add(",");
            const char*   hit = json.String(json.Field(i, "hitCondition"), "");
            char*         end = 0;
            unsigned long target = strtoul(hit, &end, 10);
            const char*   condition = json.String(json.Field(i, "condition"), "");
            const char*   log = json.String(json.Field(i, "logMessage"), "");
            if (hit[0] && (hit[0] < '1' || hit[0] > '9' || *end || !target || target > 2147483647))
            {
                body.Add("{\"verified\":false,\"message\":\"hitCondition must be a positive integer\"}");
                continue;
            }
            Breakpoint* bp = new Breakpoint();
            memset(bp, 0, sizeof(*bp));
            bp->m_Id = d->m_NextId++;
            bp->m_Path = strdup(path.Data());
            bp->m_Condition = strdup(condition);
            bp->m_LogMessage = strdup(log);
            bp->m_LogPoint = json.Field(i, "logMessage") >= 0;
            bp->m_Line = json.Integer(json.Field(i, "line")) + !d->m_LinesStartAt1;
            bp->m_HitTarget = (uint32_t)target;
            for (uint32_t s = 0; s < d->m_Sources.Size(); ++s)
                if (!strcmp(d->m_Sources[s]->m_Path, bp->m_Path))
                    bp->m_Verified = HasLine(d->m_Sources[s], bp->m_Line);
            Push(d->m_Breakpoints, bp);
            BreakpointBody(d, bp, body);
        }
        body.Add("]}");
        Respond(d, seq, "setBreakpoints", &body);
    }

    static void BreakpointLocations(Debugger* d, const Json& json, int seq, int args)
    {
        const char* source = json.String(json.Field(json.Field(args, "source"), "path"));
        int         first = json.Integer(json.Field(args, "line"));
        int         last = json.Integer(json.Field(args, "endLine"), first);
        int         column = json.Integer(json.Field(args, "column"), d->m_ColumnsStartAt1 ? 1 : 0);
        int         end_column = json.Integer(json.Field(args, "endColumn"), 2147483647);
        if (!source || !source[0] || first < (d->m_LinesStartAt1 ? 1 : 0) || last < first ||
            column < (d->m_ColumnsStartAt1 ? 1 : 0) || end_column < (d->m_ColumnsStartAt1 ? 1 : 0) ||
            (first == last && end_column < column))
        {
            Respond(d, seq, "breakpointLocations", 0, "Invalid source or breakpoint range");
            return;
        }
        Buffer path;
        RuntimePath(d, source, path);
        RefreshSources(d);
        dmArray<int> lines;
        for (uint32_t i = 0; i < d->m_Sources.Size(); ++i)
            if (!strcmp(d->m_Sources[i]->m_Path, path.Data()))
                for (uint32_t j = 0; j < d->m_Sources[i]->m_Lines.Size(); ++j)
                {
                    int line = d->m_Sources[i]->m_Lines[j] - !d->m_LinesStartAt1;
                    if (line >= first && line <= last && (line != first || column == (d->m_ColumnsStartAt1 ? 1 : 0)))
                        Push(lines, line);
                }
        if (lines.Size() > 1)
            qsort(lines.Begin(), lines.Size(), sizeof(int), CompareLines);
        Buffer body;
        body.Add("{\"breakpoints\":[");
        for (uint32_t i = 0; i < lines.Size(); ++i)
            body.Format("%s{\"line\":%d}", i ? "," : "", lines[i]);
        body.Add("]}");
        Respond(d, seq, "breakpointLocations", &body);
    }

    static void Resume(Debugger* d, Step step, uint32_t thread)
    {
        Thread* t = FindThread(d, thread);
        d->m_Step = step;
        d->m_StepNativeTailCall = false;
        d->m_StepThread = thread;
        d->m_StepDepth = t ? StackDepth(GetThread(t)) : 0;
        d->m_Paused = false;
        d->m_PauseRequested = false;
        d->m_Exception.Clear();
        ClearReferences(d);
        Buffer body;
        body.Format("{\"threadId\":%u,\"allThreadsContinued\":true}", thread);
        Event(d, "continued", &body);
    }

    static void Request(Debugger* d, const Json& json)
    {
        const char* command = json.String(json.Field(0, "command"));
        int         seq = json.Integer(json.Field(0, "seq"));
        int         args = json.Field(0, "arguments");
        if (!command || seq < 0 || strcmp(json.String(json.Field(0, "type"), ""), "request"))
        {
            d->m_ClosePending = true;
            return;
        }
        if (args >= 0 && json.Get(args).m_Type != JSON_OBJECT)
        {
            Respond(d, seq, command, 0, "Expected arguments object");
            return;
        }
        if (!strcmp(command, "initialize"))
        {
            if (d->m_Initialized)
            {
                Respond(d, seq, command, 0, "Already initialized");
                return;
            }
            if (strcmp(json.String(json.Field(args, "pathFormat"), "path"), "path"))
            {
                Respond(d, seq, command, 0, "Only native paths are supported");
                return;
            }
            d->m_LinesStartAt1 = json.Boolean(json.Field(args, "linesStartAt1"), true);
            d->m_ColumnsStartAt1 = json.Boolean(json.Field(args, "columnsStartAt1"), true);
            d->m_VariableType = json.Boolean(json.Field(args, "supportsVariableType"));
            d->m_VariablePaging = json.Boolean(json.Field(args, "supportsVariablePaging"));
            d->m_InvalidatedEvent = json.Boolean(json.Field(args, "supportsInvalidatedEvent"));
            d->m_Initialized = true;
            Buffer body;
            body.Add(
            "{\"supportsConfigurationDoneRequest\":true,\"supportsConditionalBreakpoints\":true,"
            "\"supportsHitConditionalBreakpoints\":true,\"supportsLogPoints\":true,\"supportsEvaluateForHovers\":true,"
            "\"supportsSetVariable\":true,\"supportsExceptionInfoRequest\":true,"
            "\"supportsDelayedStackTraceLoading\":true,\"supportsSetExpression\":true,"
            "\"supportsCompletionsRequest\":true,\"completionTriggerCharacters\":[\".\",\":\"],"
            "\"supportsBreakpointLocationsRequest\":true,"
            "\"exceptionBreakpointFilters\":[{\"filter\":\"uncaught\",\"label\":\"Uncaught Lua errors\",\"default\":false}]}");
            Respond(d, seq, command, &body);
            return;
        }
        if (!d->m_Initialized)
        {
            Respond(d, seq, command, 0, "Send initialize first");
            return;
        }
        if (!strcmp(command, "disconnect"))
        {
            if (json.Boolean(json.Field(args, "terminateDebuggee")))
            {
                Respond(d, seq, command, 0, "An attached debugger cannot terminate the engine");
                return;
            }
            Respond(d, seq, command);
            Event(d, "terminated");
            // Flush the ordered response/events, then close and resume Lua.
            d->m_ClosePending = true;
            d->m_CloseDeadline = dmTime::GetTime() + 1000000;
            return;
        }
        if (!strcmp(command, "attach"))
        {
            if (d->m_Attached)
            {
                Respond(d, seq, command, 0, "Already attached");
                return;
            }
            int timeout_arg = json.Field(args, "evaluationTimeout");
            int timeout = timeout_arg < 0 ? 1000 : json.Integer(timeout_arg);
            if (timeout < 1 || timeout > 60000)
            {
                Respond(d, seq, command, 0, "evaluationTimeout must be between 1 and 60000 milliseconds");
                return;
            }
            d->m_EvaluationTimeout = timeout;
            const char* root = json.String(json.Field(args, "localRoot"));
            if (root)
            {
                Buffer normalized;
                NormalizePath(root, normalized);
                d->m_LocalRoot = strdup(normalized.Data());
            }
            d->m_StopOnEntry = json.Boolean(json.Field(args, "stopOnEntry"));
            SetHooks(d, true);
            d->m_Attached = true;
            d->m_AttachSeq = seq;
            Event(d, "initialized");
            return;
        }
        if (!d->m_Attached)
        {
            Respond(d, seq, command, 0, "Send attach first");
            return;
        }
        if (!strcmp(command, "configurationDone"))
        {
            if (d->m_Configured)
            {
                Respond(d, seq, command, 0, "Already configured");
                return;
            }
            d->m_Configured = true;
            Respond(d, seq, command);
            Respond(d, d->m_AttachSeq, "attach");
            return;
        }
        if (!strcmp(command, "setBreakpoints"))
        {
            SetBreakpoints(d, json, seq, args);
            return;
        }
        if (!strcmp(command, "breakpointLocations"))
        {
            BreakpointLocations(d, json, seq, args);
            return;
        }
        if (!strcmp(command, "setExceptionBreakpoints"))
        {
            int filters = json.Field(args, "filters");
            if (json.Get(filters).m_Type != JSON_ARRAY)
            {
                Respond(d, seq, command, 0, "Expected filters array");
                return;
            }
            bool enabled = false;
            for (int i = json.Get(filters).m_First; i >= 0; i = json.Get(i).m_Next)
            {
                if (strcmp(json.String(i, ""), "uncaught"))
                {
                    Respond(d, seq, command, 0, "Unknown exception filter");
                    return;
                }
                enabled = true;
            }
            d->m_BreakOnError = enabled;
            Respond(d, seq, command);
            return;
        }
        if (!strcmp(command, "threads"))
        {
            Buffer body;
            body.Add("{\"threads\":[");
            int count = 0;
            for (uint32_t i = 0; i < d->m_Threads.Size(); ++i)
            {
                Thread* thread = d->m_Threads[i];
                if (thread->m_Exited || !GetThread(thread))
                    continue;
                if (count++)
                    body.Add(",");
                body.Format("{\"id\":%u,\"name\":", thread->m_Id);
                Buffer name;
                name.Add(thread->m_State->m_Name);
                if (!thread->m_Main)
                    name.Format(" / coroutine %u", thread->m_Id);
                body.String(name.Data());
                body.Add("}");
            }
            body.Add("]}");
            Respond(d, seq, command, &body);
            return;
        }
        if (ValueRequest(d, json, command, seq, args))
            return;
        if (!strcmp(command, "pause") || !strcmp(command, "continue") || !strcmp(command, "next") || !strcmp(command, "stepIn") || !strcmp(command, "stepOut"))
        {
            int id = json.Integer(json.Field(args, "threadId"));
            if (!d->m_Configured || id <= 0 || !FindThread(d, (uint32_t)id))
            {
                Respond(d, seq, command, 0, "Invalid thread or session not configured");
                return;
            }
            if (!strcmp(command, "pause"))
            {
                if (d->m_Paused)
                {
                    Respond(d, seq, command, 0, "Already paused");
                    return;
                }
                d->m_PauseRequested = true;
                Respond(d, seq, command);
            }
            else
            {
                if (!d->m_Paused)
                {
                    Respond(d, seq, command, 0, "Lua is running");
                    return;
                }
                if (strcmp(command, "continue") && (uint32_t)id != d->m_StoppedThread)
                {
                    Respond(d, seq, command, 0, "Step the stopped thread");
                    return;
                }
                const char* granularity = json.String(json.Field(args, "granularity"), "line");
                if (strcmp(granularity, "line") && strcmp(granularity, "statement"))
                {
                    Respond(d, seq, command, 0, "Only line stepping is supported");
                    return;
                }
                Buffer body;
                body.Add("{\"allThreadsContinued\":true}");
                Respond(d, seq, command, !strcmp(command, "continue") ? &body : 0);
                Resume(d, !strcmp(command, "next") ? STEP_OVER : !strcmp(command, "stepIn") ? STEP_IN :
                       !strcmp(command, "stepOut")                                          ? STEP_OUT :
                                                                                              STEP_NONE,
                       id);
            }
            return;
        }
        Respond(d, seq, command, 0, "Unsupported DAP request");
    }

    struct UpdateGuard
    {
        Debugger* m_Debugger;
        UpdateGuard(Debugger* d)
            : m_Debugger(d)
        {
            d->m_Updating = true;
        }
        ~UpdateGuard()
        {
            m_Debugger->m_Updating = false;
        }
    };

    void Update(HDebugger d)
    {
        if (!d || d->m_Updating || d->m_Evaluating)
            return;
        UpdateGuard guard(d);
        for (uint32_t i = 0; i < d->m_Threads.Size();)
        {
            Thread*    thread = d->m_Threads[i];
            lua_State* L = GetThread(thread);
            if (HasEnded(thread, L) && !thread->m_Exited)
            {
                thread->m_Exited = true;
                ThreadEvent(d, thread, "exited");
            }
            // Keep only the metadata of a collected step target until a Lua
            // line can transfer the step to its resumer. The coroutine stays weak.
            if (L || (d->m_Step != STEP_NONE && d->m_StepThread == thread->m_Id))
            {
                ++i;
                continue;
            }
            d->m_ThreadIds.Erase(thread->m_Id);
            delete thread;
            d->m_Threads.EraseSwap(i);
        }
        if (d->m_Client == dmSocket::INVALID_SOCKET_HANDLE)
        {
            dmSocket::Address address;
            if (dmSocket::Accept(d->m_Listener, &address, &d->m_Client) != dmSocket::RESULT_OK)
                return;
            ++d->m_Connections;
            if (dmSocket::SetBlocking(d->m_Client, false) != dmSocket::RESULT_OK)
            {
                Disconnect(d);
                return;
            }
            dmSocket::SetNoDelay(d->m_Client, true);
            d->m_Sequence = 1;
        }
        // Bound work per pump so a client cannot starve game execution by
        // streaming requests. Fragmented and coalesced frames use the same path.
        char             incoming[8192];
        int              received = 0;
        dmSocket::Result result = dmSocket::Receive(d->m_Client, incoming, sizeof(incoming), &received);
        if (result == dmSocket::RESULT_OK)
        {
            if (!received)
            {
                Disconnect(d);
                return;
            }
            d->m_Input.Add(incoming, received);
        }
        else if (result != dmSocket::RESULT_WOULDBLOCK)
        {
            Disconnect(d);
            return;
        }
        if (!d->m_Input.m_Valid)
        {
            Disconnect(d);
            return;
        }
        for (int i = 0; i < 32 && !d->m_ClosePending; ++i)
        {
            uint32_t offset = 0, size = 0;
            int      frame = ReadFrame(d->m_Input, &offset, &size);
            if (frame < 0)
            {
                Disconnect(d);
                return;
            }
            if (!frame)
                break;
            Json request;
            if (!request.Parse(d->m_Input.Data() + offset, size))
            {
                Disconnect(d);
                return;
            }
            Request(d, request);
            d->m_Input.Consume(offset + size);
        }
        if (d->m_Output.Size())
        {
            int sent = 0;
            result = dmSocket::Send(d->m_Client, d->m_Output.Data(), d->m_Output.Size(), &sent);
            if (result == dmSocket::RESULT_OK)
                d->m_Output.Consume(sent);
            else if (result != dmSocket::RESULT_WOULDBLOCK)
            {
                Disconnect(d);
                return;
            }
        }
        if (d->m_ClosePending && (!d->m_Output.Size() || !d->m_Output.m_Valid || dmTime::GetTime() >= d->m_CloseDeadline))
            Disconnect(d);
    }

    void WaitForClient(HDebugger d)
    {
        uint32_t connections = d ? d->m_Connections : 0;
        while (d && !d->m_Configured)
        {
            Update(d);
            if (d->m_Client == dmSocket::INVALID_SOCKET_HANDLE && d->m_Connections != connections)
                break;
            dmTime::Sleep(1000);
        }
    }

    static void Stop(Debugger* d, lua_State* L, const char* reason, uint32_t breakpoint = 0)
    {
        Thread* thread = TrackThread(d, L);
        if (!thread)
            return;
        d->m_Paused = true;
        d->m_PauseRequested = false;
        d->m_StopOnEntry = false;
        d->m_Step = STEP_NONE;
        d->m_StoppedThread = thread->m_Id;
        CaptureFrames(d);
        // A late attachment can stop inside functions whose call hooks were
        // never observed. Their debug information still supplies executable lines.
        for (uint32_t i = 0; i < d->m_Frames.Size(); ++i)
        {
            Frame&    frame = d->m_Frames[i];
            lua_Debug ar;
            if (lua_getstack(frame.m_L, frame.m_Level, &ar) && lua_getinfo(frame.m_L, "S", &ar))
                ObserveSource(d, frame.m_L, &ar);
        }
        RefreshSources(d);
        Buffer body;
        body.Format("{\"reason\":\"%s\",\"threadId\":%u,\"allThreadsStopped\":true", reason, thread->m_Id);
        if (breakpoint)
            body.Format(",\"hitBreakpointIds\":[%u]", breakpoint);
        if (d->m_Exception.Size())
        {
            body.Add(",\"text\":");
            body.String(d->m_Exception.Data());
        }
        body.Add("}");
        Event(d, "stopped", &body);
        while (d->m_Paused)
        {
            Update(d);
            if (d->m_Paused)
                dmTime::Sleep(1000);
        }
    }

    static const char* LogExpressionEnd(const char* p)
    {
        int depth = 1;
        while (*p)
        {
            if (*p == '\'' || *p == '"')
            {
                char quote = *p++;
                while (*p && *p != quote)
                {
                    if (*p == '\\' && p[1])
                        ++p;
                    ++p;
                }
                if (!*p)
                    return 0;
            }
            else if (*p == '{')
                ++depth;
            else if (*p == '}' && --depth == 0)
                return p;
            ++p;
        }
        return 0;
    }

    static void Logpoint(Debugger* d, lua_State* L, const char* message)
    {
        Buffer text;
        for (const char* p = message; *p;)
        {
            if ((p[0] == '{' && p[1] == '{') || (p[0] == '}' && p[1] == '}'))
            {
                text.Add(p++, 1);
                ++p;
                continue;
            }
            if (*p != '{')
            {
                text.Add(p++, 1);
                continue;
            }
            const char* end = LogExpressionEnd(p + 1);
            if (!end)
            {
                text.Add("<unclosed logpoint expression>");
                break;
            }
            Buffer expression;
            expression.Add(p + 1, (uint32_t)(end - p - 1));
            if (Evaluate(d, L, 0, expression.Data(), false))
            {
                if (lua_type(L, -1) == LUA_TSTRING)
                    text.Add(lua_tostring(L, -1));
                else
                    FormatValue(d, L, -1, text);
            }
            else
            {
                text.Add("<");
                text.Add(lua_tostring(L, -1) ? lua_tostring(L, -1) : "evaluation failed");
                text.Add(">");
            }
            lua_pop(L, 1);
            p = end + 1;
        }
        text.Add("\n");
        Output(d, text.Data());
    }

    static void RecordCallSite(Thread* thread, lua_State* L, int depth)
    {
        if (!thread->m_State->m_JitAvailable)
            return;
        dmArray<CallSite>& sites = thread->m_CallSites;
        // A new invocation can replace a frame through a tail call or after an
        // exception. Keep only the callers that are still awaiting a return.
        while (sites.Size() && sites.Back().m_Depth >= depth - 1)
            sites.Pop();
        lua_Debug caller;
        if (lua_getstack(L, 1, &caller))
        {
            lua_getinfo(L, "l", &caller);
            if (caller.currentline > 0)
            {
                CallSite site = { depth - 1, caller.currentline };
                Push(sites, site);
            }
        }
    }

    static bool SkipCallReturn(Thread* thread, lua_State* L, int line)
    {
        dmArray<CallSite>& sites = thread->m_CallSites;
        while (sites.Size())
        {
            CallSite  site = sites.Back();
            lua_Debug frame;
            // Probe the saved depth directly instead of measuring the whole
            // stack. A deeper stack means the callee is still executing.
            if (lua_getstack(L, site.m_Depth, &frame))
                return false;
            sites.Pop();
            if (lua_getstack(L, site.m_Depth - 1, &frame))
            {
                // LuaJIT emits a line event on return to the call site. Consume
                // it once; later visits can be real loop iterations.
                return site.m_Line == line;
            }
        }
        return false;
    }

    void Hook(lua_State* L, lua_Debug* ar)
    {
        Debugger* d = (Debugger*)GetPointer(L, &g_DebuggerKey);
        if (!d)
            return;
        if (d->m_Evaluating)
        {
            if (ar->event == LUA_HOOKCOUNT)
                CheckEvaluation(L);
            return;
        }
        if (ar->event == LUA_HOOKCOUNT)
            Update(d);
        if (!d->m_Attached)
            return;
        if (ar->event == LUA_HOOKRET || ar->event == LUA_HOOKTAILRET)
        {
            if (d->m_Step == STEP_OVER)
            {
                Thread*   thread = TrackThread(d, L);
                lua_Debug caller;
                // Once a callback actually returns to the engine, step into the
                // next callback instead of mistaking it for a tail call. Lua 5.1
                // reports the final return of a tail-call chain separately.
                if (d->m_StepThread == thread->m_Id && !lua_getstack(L, 1, &caller))
                    d->m_Step = STEP_IN;
            }
            return;
        }
        Thread* thread = TrackThread(d, L);
        if (ar->event == LUA_HOOKLINE && d->m_Step != STEP_NONE && thread->m_Id != d->m_StepThread)
        {
            Thread* stepped = FindThread(d, d->m_StepThread, true);
            if (stepped && !stepped->m_Main && stepped->m_State == thread->m_State)
            {
                // Original resume functions and pre-existing wrap closures do
                // not call our wrappers. Check the actual coroutine state at
                // the next line instead of relying on balanced return hooks.
                // A coroutine awaiting a nested resume still has status 0 and
                // live frames, so stepping out does not follow its child.
                lua_State* coroutine = GetThread(stepped);
                if (HasEnded(stepped, coroutine) || lua_status(coroutine) == LUA_YIELD)
                    CoroutineReturned(d, L, stepped);
            }
        }
        lua_getinfo(L, "nSl", ar);
        if (ar->event == LUA_HOOKCALL)
        {
            int depth = StackDepth(L);
            if (d->m_Step == STEP_OVER && d->m_StepThread == thread->m_Id)
            {
                // LuaJIT omits return hooks for fast native tail calls. A later
                // call at the root depth means that native invocation has ended;
                // Lua callbacks invoked inside it still have a deeper stack.
                if (depth == 1 && d->m_StepNativeTailCall)
                    d->m_Step = STEP_IN;
                // LuaJIT replaces the caller's frame on a tail call. Keep
                // stepping over the new invocation until its caller resumes.
                else if (depth <= d->m_StepDepth)
                {
                    d->m_StepDepth = depth - 1;
                    d->m_StepNativeTailCall = depth == 1 && !strcmp(ar->what, "C");
                }
            }
            RecordCallSite(thread, L, depth);
            ObserveSource(d, L, ar);
            return;
        }
        if (!d->m_Configured)
            return;
        bool skip_line = ar->event == LUA_HOOKLINE && SkipCallReturn(thread, L, ar->currentline);
        if (ar->event == LUA_HOOKLINE || ar->event == LUA_HOOKCOUNT)
        {
            if (d->m_PauseRequested || d->m_StopOnEntry)
            {
                Stop(d, L, d->m_StopOnEntry ? "entry" : "pause");
                return;
            }
        }
        if (ar->event != LUA_HOOKLINE || skip_line)
            return;
        Buffer path;
        RuntimePath(d, ar->source ? ar->source : "", path);
        for (uint32_t i = 0; i < d->m_Breakpoints.Size(); ++i)
        {
            Breakpoint* bp = d->m_Breakpoints[i];
            if (ar->currentline != bp->m_Line || strcmp(path.Data(), bp->m_Path))
                continue;
            VerifyBreakpoint(d, bp);
            if (bp->m_Condition[0])
            {
                bool ok = Evaluate(d, L, 0, bp->m_Condition, false);
                bool hit = ok && lua_toboolean(L, -1);
                if (!ok)
                    Output(d, lua_tostring(L, -1) ? lua_tostring(L, -1) : "Breakpoint condition failed");
                lua_pop(L, 1);
                if (!hit)
                    continue;
            }
            if (bp->m_Hits != 0xffffffff)
                ++bp->m_Hits;
            if (bp->m_HitTarget && bp->m_Hits != bp->m_HitTarget)
                continue;
            if (bp->m_LogPoint)
            {
                Logpoint(d, L, bp->m_LogMessage);
                continue;
            }
            Stop(d, L, "breakpoint", bp->m_Id);
            return;
        }
        if (d->m_Step == STEP_NONE)
            return;
        Thread* step_thread = FindThread(d, d->m_StepThread);
        if (thread->m_Id == d->m_StepThread || (d->m_Step == STEP_IN && step_thread && step_thread->m_State == thread->m_State))
        {
            int depth = StackDepth(L);
            if (d->m_Step == STEP_IN || (d->m_Step == STEP_OVER && depth <= d->m_StepDepth) || (d->m_Step == STEP_OUT && depth < d->m_StepDepth))
                Stop(d, L, "step");
        }
    }

    void OnError(HDebugger d, lua_State* L)
    {
        if (!d || !d->m_Configured || !d->m_BreakOnError || d->m_Evaluating)
            return;
        d->m_Exception.Clear();
        if (lua_type(L, 1) == LUA_TSTRING)
            d->m_Exception.Add(lua_tostring(L, 1));
        else
            FormatValue(d, L, 1, d->m_Exception);
        Stop(d, L, "exception");
    }
} // namespace dmDebugger
