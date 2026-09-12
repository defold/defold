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

#ifndef DM_DEBUGGER_PRIVATE_H
#define DM_DEBUGGER_PRIVATE_H

#include "debugger.h"
#include "dap.h"
#include <dlib/socket.h>
extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

namespace dmDebugger
{
    struct State;
    struct CallSite
    {
        int m_Depth;
        int m_Line;
    };

    struct Thread
    {
        State*            m_State;
        uint32_t          m_Id;
        lua_Hook          m_OldHook;
        int               m_OldMask;
        int               m_OldCount;
        uint8_t           m_Hooked : 1;
        uint8_t           m_Main : 1;
        uint8_t           m_Exited : 1;
        dmArray<CallSite> m_CallSites;
    };

    struct State
    {
        lua_State* m_L;
        char*      m_Name;
        int        m_ThreadsRef;
        int        m_ObservedFunctionsRef;
        int        m_CreateRef;
        int        m_ResumeRef;
        int        m_WrapRef;
        bool       m_JitEnabled;
        bool       m_JitAvailable;
        Thread*    m_Main;
    };

    struct Breakpoint
    {
        uint32_t m_Id;
        char*    m_Path;
        char*    m_Condition;
        char*    m_LogMessage;
        int      m_Line;
        uint32_t m_Hits;
        uint32_t m_HitTarget;
        uint8_t  m_Verified : 1;
        uint8_t  m_LogPoint : 1;
    };

    struct Source
    {
        char*        m_Path;
        dmArray<int> m_Lines;
    };

    struct Frame
    {
        uint32_t   m_Id;
        lua_State* m_L;
        int        m_Level;
        uint32_t   m_ThreadId;
        int        m_ThreadRef;
    };
    enum ReferenceKind
    {
        REFERENCE_LOCALS,
        REFERENCE_UPVALUES,
        REFERENCE_GLOBALS,
        REFERENCE_VALUE
    };
    struct Reference
    {
        uint32_t      m_Id;
        lua_State*    m_L;
        int           m_Level;
        int           m_LuaRef;
        ReferenceKind m_Kind;
        char*         m_EvaluateName;
    };
    enum Step
    {
        STEP_NONE,
        STEP_IN,
        STEP_OVER,
        STEP_OUT
    };

    struct Debugger
    {
        dmSocket::Socket      m_Listener;
        dmSocket::Socket      m_Client;
        Buffer                m_Input;
        Buffer                m_Output;
        dmArray<State*>       m_States;
        dmArray<Thread*>      m_Threads;
        dmArray<Breakpoint*>  m_Breakpoints;
        dmArray<Source*>      m_Sources;
        dmArray<Frame>        m_Frames;
        dmArray<Reference>    m_References;
        UserdataTableResolver m_UserdataTableResolver;
        char*                 m_LocalRoot;
        Buffer                m_Exception;
        uint64_t              m_CloseDeadline;
        uint32_t              m_Sequence;
        uint32_t              m_NextId;
        uint32_t              m_AttachSeq;
        uint32_t              m_StoppedThread;
        uint32_t              m_StepThread;
        int32_t               m_StepDepth; // Signed for Lua stack levels and tail-call stepping.
        uint32_t              m_Connections;
        Step                  m_Step;
        uint16_t              m_Port;
        uint16_t              m_StepNativeTailCall : 1;
        uint16_t              m_Initialized : 1;
        uint16_t              m_Attached : 1;
        uint16_t              m_Configured : 1;
        uint16_t              m_Paused : 1;
        uint16_t              m_PauseRequested : 1;
        uint16_t              m_StopOnEntry : 1;
        uint16_t              m_BreakOnError : 1;
        uint16_t              m_Evaluating : 1;
        uint16_t              m_Updating : 1;
        uint16_t              m_ClosePending : 1;
        uint16_t              m_LinesStartAt1 : 1;
        uint16_t              m_ColumnsStartAt1 : 1;
        uint16_t              m_VariableType : 1;
        uint16_t              m_VariablePaging : 1;
        uint16_t              m_InvalidatedEvent : 1;
        Debugger();
    };

    template <typename T>
    void Push(dmArray<T>& array, const T& value)
    {
        if (array.Full())
            array.OffsetCapacity(16);
        array.Push(value);
    }
    void       Queue(Debugger* d, Buffer& message);
    void       Respond(Debugger* d, uint32_t seq, const char* command, const Buffer* body = 0, const char* error = 0);
    void       Event(Debugger* d, const char* event, const Buffer* body = 0);
    lua_State* GetThread(Thread* thread);
    Thread*    FindThread(Debugger* d, uint32_t id, bool include_exited = false);
    Thread*    TrackThread(Debugger* d, lua_State* L);
    void       ClearReferences(Debugger* d);
    void       CaptureFrames(Debugger* d);
    int        StackDepth(lua_State* L);
    void       ClientPath(Debugger* d, const char* source, Buffer& path);
    void       FormatValue(lua_State* L, int index, Buffer& value);
    void       QuoteLuaString(const char* text, uint32_t size, Buffer& value);
    void       Output(Debugger* d, const char* text);
    // Evaluation leaves exactly one result (or error string) on the stack.
    // A negative level selects the Lua thread's global environment.
    bool Evaluate(Debugger* d, lua_State* L, int level, const char* expression, bool repl, const char* assignment = 0);
    int  LocalIndex(lua_State* L, lua_Debug* ar, const char* name);
    int  UpvalueIndex(lua_State* L, int function, const char* name);
    void PushEnvironment(lua_State* L, int level);
    bool IsIdentifier(const char* name);
    void KeyExpression(lua_State* L, int index, const char* parent, Buffer& expression);
    // Inspection leaves exactly one value or error string and never executes Lua.
    bool Inspect(Debugger* d, lua_State* L, int level, const char* expression);
    // Pushes the backing table of an inspectable value, or preserves the stack.
    bool PushValueTable(Debugger* d, lua_State* L, int index);
    bool Completions(Debugger* d, lua_State* L, int level, const Json& request, int args, Buffer& body);
    bool ValueRequest(Debugger* d, const Json& request, const char* command, int seq, int args);
} // namespace dmDebugger
#endif
