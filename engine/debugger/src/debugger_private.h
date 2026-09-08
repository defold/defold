// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0. See https://www.defold.com/license

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
    struct Thread
    {
        State*   m_State;
        int      m_Id;
        lua_Hook m_OldHook;
        int      m_OldMask;
        int      m_OldCount;
        bool     m_Hooked;
        bool     m_Main;
        bool     m_Exited;
        char*    m_SkipSource;
        int      m_SkipLine;
        int      m_SkipDepth;
        bool     m_SkipCall;
    };

    struct State
    {
        lua_State* m_L;
        char*      m_Name;
        int        m_ThreadsRef;
        int        m_CreateRef;
        int        m_ResumeRef;
        int        m_WrapRef;
        bool       m_JitEnabled;
        bool       m_JitAvailable;
        Thread*    m_Main;
    };

    struct Breakpoint
    {
        int      m_Id;
        char*    m_Path;
        char*    m_Condition;
        char*    m_LogMessage;
        int      m_Line;
        uint32_t m_Hits;
        uint32_t m_HitTarget;
        bool     m_Verified;
        bool     m_LogPoint;
    };

    struct Source
    {
        char*        m_Path;
        dmArray<int> m_Lines;
    };

    struct Frame
    {
        int        m_Id;
        lua_State* m_L;
        int        m_Level;
        int        m_ThreadId;
        int        m_ThreadRef;
    };
    enum ReferenceKind
    {
        REFERENCE_LOCALS,
        REFERENCE_UPVALUES,
        REFERENCE_GLOBALS,
        REFERENCE_TABLE
    };
    struct Reference
    {
        int           m_Id;
        lua_State*    m_L;
        int           m_Level;
        int           m_LuaRef;
        ReferenceKind m_Kind;
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
        dmSocket::Socket     m_Listener;
        dmSocket::Socket     m_Client;
        uint16_t             m_Port;
        Buffer               m_Input;
        Buffer               m_Output;
        dmArray<State*>      m_States;
        dmArray<Thread*>     m_Threads;
        dmArray<Breakpoint*> m_Breakpoints;
        dmArray<Source*>     m_Sources;
        dmArray<Frame>       m_Frames;
        dmArray<Reference>   m_References;
        char*                m_LocalRoot;
        Buffer               m_Exception;
        int                  m_Sequence;
        int                  m_NextId;
        int                  m_AttachSeq;
        int                  m_StoppedThread;
        int                  m_StepThread;
        int                  m_StepDepth;
        uint32_t             m_Connections;
        uint64_t             m_CloseDeadline;
        Step                 m_Step;
        bool                 m_Initialized;
        bool                 m_Attached;
        bool                 m_Configured;
        bool                 m_Paused;
        bool                 m_PauseRequested;
        bool                 m_StopOnEntry;
        bool                 m_BreakOnError;
        bool                 m_Evaluating;
        bool                 m_Updating;
        bool                 m_ClosePending;
        bool                 m_LinesStartAt1;
        bool                 m_ColumnsStartAt1;
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
    void       Respond(Debugger* d, int seq, const char* command, const Buffer* body = 0, const char* error = 0);
    void       Event(Debugger* d, const char* event, const Buffer* body = 0);
    lua_State* GetThread(Thread* thread);
    Thread*    FindThread(Debugger* d, int id);
    Thread*    TrackThread(Debugger* d, lua_State* L);
    void       ClearReferences(Debugger* d);
    void       CaptureFrames(Debugger* d);
    int        StackDepth(lua_State* L);
    void       ClientPath(Debugger* d, const char* source, Buffer& path);
    void       FormatValue(lua_State* L, int index, Buffer& value);
    void       Output(Debugger* d, const char* text);
    // Evaluation leaves exactly one result (or error string) on the stack.
    bool Evaluate(Debugger* d, lua_State* L, int level, const char* expression, bool repl);
    bool ValueRequest(Debugger* d, const Json& request, const char* command, int seq, int args);
} // namespace dmDebugger
#endif
