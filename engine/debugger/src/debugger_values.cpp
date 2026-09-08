// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0. See https://www.defold.com/license

#if !defined(DM_RELEASE)
#include "debugger_private.h"
#include <dlib/dstrings.h>
#include <string.h>

namespace dmDebugger
{
    int StackDepth(lua_State* L)
    {
        lua_Debug ar;
        int       level = 0;
        while (lua_getstack(L, level, &ar))
            ++level;
        return level;
    }

    void ClearReferences(Debugger* d)
    {
        for (uint32_t i = 0; i < d->m_References.Size(); ++i)
        {
            Reference& r = d->m_References[i];
            if (r.m_LuaRef != LUA_NOREF)
                luaL_unref(r.m_L, LUA_REGISTRYINDEX, r.m_LuaRef);
        }
        d->m_References.SetSize(0);
        for (uint32_t i = 0; i < d->m_Frames.Size(); ++i)
            luaL_unref(d->m_Frames[i].m_L, LUA_REGISTRYINDEX, d->m_Frames[i].m_ThreadRef);
        d->m_Frames.SetSize(0);
    }

    void CaptureFrames(Debugger* d)
    {
        ClearReferences(d);
        for (uint32_t i = 0; i < d->m_Threads.Size(); ++i)
        {
            Thread*    thread = d->m_Threads[i];
            lua_State* L = GetThread(thread);
            if (!L)
                continue;
            lua_Debug ar;
            for (int level = 0; lua_getstack(L, level, &ar); ++level)
            {
                lua_getinfo(L, "S", &ar);
                // C error handlers/debugger callbacks do not have Lua locals.
                if (!strcmp(ar.what, "C"))
                    continue;
                lua_pushthread(L);
                Frame frame = { d->m_NextId++, L, level, thread->m_Id, luaL_ref(L, LUA_REGISTRYINDEX) };
                Push(d->m_Frames, frame);
            }
        }
    }

    static Frame* FindFrame(Debugger* d, int id)
    {
        for (uint32_t i = 0; i < d->m_Frames.Size(); ++i)
            if (d->m_Frames[i].m_Id == id)
                return &d->m_Frames[i];
        return 0;
    }

    static int AddReference(Debugger* d, lua_State* L, ReferenceKind kind, int level, int index = 0)
    {
        for (uint32_t i = 0; i < d->m_References.Size(); ++i)
        {
            Reference& r = d->m_References[i];
            if (r.m_L != L || r.m_Kind != kind || r.m_Level != level)
                continue;
            if (kind != REFERENCE_TABLE)
                return r.m_Id;
            if (kind == REFERENCE_TABLE)
            {
                const void* pointer = lua_topointer(L, index);
                lua_rawgeti(L, LUA_REGISTRYINDEX, r.m_LuaRef);
                bool same = lua_topointer(L, -1) == pointer;
                lua_pop(L, 1);
                if (same)
                    return r.m_Id;
            }
        }
        Reference r = { d->m_NextId++, L, level, LUA_NOREF, kind };
        if (kind == REFERENCE_TABLE)
        {
            lua_pushvalue(L, index);
            r.m_LuaRef = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        Push(d->m_References, r);
        return r.m_Id;
    }

    void FormatValue(lua_State* L, int index, Buffer& value)
    {
        switch (lua_type(L, index))
        {
            case LUA_TNIL:
                value.Add("nil");
                break;
            case LUA_TBOOLEAN:
                value.Add(lua_toboolean(L, index) ? "true" : "false");
                break;
            case LUA_TNUMBER:
                value.Format("%.17g", (double)lua_tonumber(L, index));
                break;
            case LUA_TSTRING:
            {
                size_t      length = 0;
                const char* text = lua_tolstring(L, index, &length);
                value.String(text, (uint32_t)length);
                break;
            }
            default:
                value.Format("%s: %p", lua_typename(L, lua_type(L, index)), lua_topointer(L, index));
                break;
        }
    }

    static void ValueBody(Debugger* d, lua_State* L, int level, int index, Buffer& body, const char* value_key)
    {
        Buffer value;
        FormatValue(L, index, value);
        body.String(value_key);
        body.Add(":");
        body.String(value.Data(), value.Size());
        body.Add(",\"type\":");
        body.String(lua_typename(L, lua_type(L, index)));
        int reference = lua_istable(L, index) ? AddReference(d, L, REFERENCE_TABLE, level, index) : 0;
        body.Format(",\"variablesReference\":%d", reference);
    }

    struct Evaluation
    {
        lua_State* m_L;
        int        m_FunctionRef;
        int        m_EnvironmentRef;
        int        m_Depth;
        bool       m_Active;
    };

    // Search backwards so inner locals shadow outer locals, even when nil.
    static int LocalIndex(lua_State* L, lua_Debug* ar, const char* name)
    {
        int result = 0;
        for (int i = 1;; ++i)
        {
            const char* local = lua_getlocal(L, ar, i);
            if (!local)
                break;
            if (local[0] != '(' && !strcmp(local, name))
                result = i;
            lua_pop(L, 1);
        }
        return result;
    }

    static int UpvalueIndex(lua_State* L, int function, const char* name)
    {
        for (int i = 1;; ++i)
        {
            const char* upvalue = lua_getupvalue(L, function, i);
            if (!upvalue)
                break;
            bool found = !strcmp(upvalue, name);
            lua_pop(L, 1);
            if (found)
                return i;
        }
        return 0;
    }

    static int EvaluationIndex(lua_State* L)
    {
        Evaluation* e = (Evaluation*)lua_touserdata(L, lua_upvalueindex(1));
        if (!e->m_Active)
            return luaL_error(L, "Evaluation frame is no longer active");
        const char* name = lua_type(L, 2) == LUA_TSTRING ? lua_tostring(L, 2) : 0;
        lua_Debug   frame;
        if (!lua_getstack(e->m_L, StackDepth(e->m_L) - e->m_Depth, &frame))
            return luaL_error(L, "Evaluation frame is no longer active");
        int local = name ? LocalIndex(e->m_L, &frame, name) : 0;
        if (local)
        {
            lua_getlocal(e->m_L, &frame, local);
            lua_xmove(e->m_L, L, 1);
            return 1;
        }
        lua_rawgeti(L, LUA_REGISTRYINDEX, e->m_FunctionRef);
        int upvalue = name ? UpvalueIndex(L, lua_gettop(L), name) : 0;
        if (upvalue)
        {
            lua_getupvalue(L, -1, upvalue);
            return 1;
        }
        lua_rawgeti(L, LUA_REGISTRYINDEX, e->m_EnvironmentRef);
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
        return 1;
    }

    static int EvaluationNewIndex(lua_State* L)
    {
        Evaluation* e = (Evaluation*)lua_touserdata(L, lua_upvalueindex(1));
        if (!e->m_Active)
            return luaL_error(L, "Evaluation frame is no longer active");
        const char* name = lua_type(L, 2) == LUA_TSTRING ? lua_tostring(L, 2) : 0;
        lua_Debug   frame;
        if (!lua_getstack(e->m_L, StackDepth(e->m_L) - e->m_Depth, &frame))
            return luaL_error(L, "Evaluation frame is no longer active");
        int local = name ? LocalIndex(e->m_L, &frame, name) : 0;
        if (local)
        {
            lua_pushvalue(L, 3);
            lua_xmove(L, e->m_L, 1);
            lua_setlocal(e->m_L, &frame, local);
            return 0;
        }
        lua_rawgeti(L, LUA_REGISTRYINDEX, e->m_FunctionRef);
        int upvalue = name ? UpvalueIndex(L, lua_gettop(L), name) : 0;
        if (upvalue)
        {
            lua_pushvalue(L, 3);
            lua_setupvalue(L, -2, upvalue);
            return 0;
        }
        lua_rawgeti(L, LUA_REGISTRYINDEX, e->m_EnvironmentRef);
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);
        lua_settable(L, -3);
        return 0;
    }

    static bool SnapshotContains(lua_State* L)
    {
        lua_pushvalue(L, 2);
        lua_rawget(L, lua_upvalueindex(1));
        bool contains = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        return contains;
    }
    static int SnapshotIndex(lua_State* L)
    {
        bool local = SnapshotContains(L);
        lua_pushvalue(L, 2);
        if (local)
            lua_rawget(L, lua_upvalueindex(2));
        else
            lua_gettable(L, lua_upvalueindex(3));
        return 1;
    }
    static int SnapshotNewIndex(lua_State* L)
    {
        bool local = SnapshotContains(L);
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);
        if (local)
            lua_rawset(L, lua_upvalueindex(2));
        else
            lua_settable(L, lua_upvalueindex(3));
        return 0;
    }
    static void SnapshotValue(lua_State* L, int names, int values, const char* name)
    {
        lua_setfield(L, values, name);
        lua_pushboolean(L, true);
        lua_setfield(L, names, name);
    }

    // Evaluated closures can escape to the program. Retain a snapshot of the
    // visible bindings for them, rather than a pointer into a suspended frame.
    static void SnapshotEnvironment(lua_State* L, Evaluation* e, int level, int environment)
    {
        int top = lua_gettop(L);
        lua_newtable(L);
        int names = lua_gettop(L);
        lua_newtable(L);
        int values = lua_gettop(L);
        lua_rawgeti(L, LUA_REGISTRYINDEX, e->m_FunctionRef);
        int function = lua_gettop(L);
        for (int i = 1;; ++i)
        {
            const char* name = lua_getupvalue(L, function, i);
            if (!name)
                break;
            SnapshotValue(L, names, values, name);
        }
        lua_Debug frame;
        lua_getstack(L, level, &frame);
        for (int i = 1;; ++i)
        {
            const char* name = lua_getlocal(L, &frame, i);
            if (!name)
                break;
            if (name[0] != '(')
                SnapshotValue(L, names, values, name);
            else
                lua_pop(L, 1);
        }
        lua_rawgeti(L, LUA_REGISTRYINDEX, environment);
        lua_newtable(L);
        lua_CFunction functions[] = { SnapshotIndex, SnapshotNewIndex };
        const char*   fields[] = { "__index", "__newindex" };
        for (int i = 0; i < 2; ++i)
        {
            lua_pushvalue(L, names);
            lua_pushvalue(L, values);
            lua_rawgeti(L, LUA_REGISTRYINDEX, e->m_EnvironmentRef);
            lua_pushcclosure(L, functions[i], 3);
            lua_setfield(L, -2, fields[i]);
        }
        lua_setmetatable(L, -2);
        lua_settop(L, top);
    }

    bool Evaluate(Debugger* d, lua_State* L, int level, const char* expression, bool repl)
    {
        lua_Debug frame;
        if (!lua_getstack(L, level, &frame))
        {
            lua_pushliteral(L, "Invalid frame");
            return false;
        }
        // A yielded Lua 5.1 thread cannot execute a protected call without
        // disturbing its suspended VM state. Run the expression on a temporary
        // thread while the environment continues to read/write the selected frame.
        lua_State* evaluation_L = L;
        int        thread_ref = LUA_NOREF;
        if (lua_status(L) == LUA_YIELD)
        {
            evaluation_L = lua_newthread(L);
            thread_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        Buffer source;
        source.Add("return ");
        source.Add(expression);
        int result = luaL_loadbuffer(evaluation_L, source.Data(), source.Size(), "=(DAP evaluation)");
        if (result && repl)
        {
            lua_pop(evaluation_L, 1);
            result = luaL_loadbuffer(evaluation_L, expression, strlen(expression), "=(DAP evaluation)");
        }
        if (result)
        {
            lua_xmove(evaluation_L, L, 1);
            luaL_unref(L, LUA_REGISTRYINDEX, thread_ref);
            return false;
        }

        Evaluation* e = (Evaluation*)lua_newuserdata(evaluation_L, sizeof(Evaluation));
        e->m_L = L;
        e->m_Depth = StackDepth(L) - level;
        e->m_Active = true;
        int context = lua_gettop(evaluation_L);
        lua_getinfo(L, "f", &frame);
        lua_getfenv(L, -1);
        e->m_EnvironmentRef = luaL_ref(L, LUA_REGISTRYINDEX);
        e->m_FunctionRef = luaL_ref(L, LUA_REGISTRYINDEX);
        lua_newtable(evaluation_L);
        lua_newtable(evaluation_L);
        lua_pushvalue(evaluation_L, context);
        lua_pushcclosure(evaluation_L, EvaluationIndex, 1);
        lua_setfield(evaluation_L, -2, "__index");
        lua_pushvalue(evaluation_L, context);
        lua_pushcclosure(evaluation_L, EvaluationNewIndex, 1);
        lua_setfield(evaluation_L, -2, "__newindex");
        lua_setmetatable(evaluation_L, -2);
        lua_pushvalue(evaluation_L, -1);
        int environment_ref = luaL_ref(evaluation_L, LUA_REGISTRYINDEX);
        lua_setfenv(evaluation_L, context - 1);
        // Keep the userdata alive throughout the call and invalidate it afterwards.
        int context_ref = luaL_ref(evaluation_L, LUA_REGISTRYINDEX);
        d->m_Evaluating = true;
        result = lua_pcall(evaluation_L, 0, 1, 0);
        lua_xmove(evaluation_L, L, 1);
        SnapshotEnvironment(L, e, level, environment_ref);
        d->m_Evaluating = false;
        e->m_Active = false;
        luaL_unref(L, LUA_REGISTRYINDEX, e->m_FunctionRef);
        luaL_unref(L, LUA_REGISTRYINDEX, e->m_EnvironmentRef);
        luaL_unref(L, LUA_REGISTRYINDEX, context_ref);
        luaL_unref(L, LUA_REGISTRYINDEX, environment_ref);
        luaL_unref(L, LUA_REGISTRYINDEX, thread_ref);
        return result == 0;
    }

    static bool FrameInfo(const Frame& frame, lua_Debug* ar)
    {
        if (!lua_getstack(frame.m_L, frame.m_Level, ar))
            return false;
        return lua_getinfo(frame.m_L, "nSl", ar) != 0;
    }

    static void KeyName(lua_State* L, int index, bool globals, Buffer& name)
    {
        if (globals && lua_type(L, index) == LUA_TSTRING)
            name.Add(lua_tostring(L, index));
        else
        {
            name.Add("[");
            FormatValue(L, index, name);
            name.Add("]");
        }
    }

    static void Variable(Debugger* d, lua_State* L, int level, const char* name, Buffer& body, int* count)
    {
        if ((*count)++)
            body.Add(",");
        body.Add("{\"name\":");
        body.String(name);
        body.Add(",");
        ValueBody(d, L, level, -1, body, "value");
        body.Add("}");
    }

    static bool Page(int ordinal, int start, int count)
    {
        return ordinal >= start && (!count || ordinal - start < count);
    }

    static void Variables(Debugger* d, const Reference& r, Buffer& body, int start, int count, const char* filter)
    {
        lua_State* L = r.m_L;
        int        top = lua_gettop(L);
        int        ordinal = 0, emitted = 0;
        lua_Debug  ar;
        bool       indexed = !strcmp(filter, "indexed");
        body.Add("{\"variables\":[");
        if (r.m_Kind == REFERENCE_LOCALS || r.m_Kind == REFERENCE_UPVALUES)
        {
            lua_getstack(L, r.m_Level, &ar);
            if (r.m_Kind == REFERENCE_UPVALUES)
                lua_getinfo(L, "f", &ar);
            int function = lua_gettop(L);
            for (int i = 1; !indexed; ++i)
            {
                const char* name = r.m_Kind == REFERENCE_LOCALS ? lua_getlocal(L, &ar, i) : lua_getupvalue(L, function, i);
                if (!name)
                    break;
                if (name[0] != '(' && (r.m_Kind != REFERENCE_LOCALS || LocalIndex(L, &ar, name) == i) && Page(ordinal++, start, count))
                    Variable(d, L, r.m_Level, name, body, &emitted);
                lua_pop(L, 1);
            }
        }
        else
        {
            if (r.m_Kind == REFERENCE_GLOBALS)
            {
                lua_getstack(L, r.m_Level, &ar);
                lua_getinfo(L, "f", &ar);
                lua_getfenv(L, -1);
            }
            else
                lua_rawgeti(L, LUA_REGISTRYINDEX, r.m_LuaRef);
            int table = lua_gettop(L);
            lua_pushnil(L);
            while (lua_next(L, table))
            {
                bool numeric = lua_type(L, -2) == LUA_TNUMBER;
                bool include = !filter[0] || (indexed ? numeric : !numeric);
                if (include && Page(ordinal++, start, count))
                {
                    Buffer name;
                    KeyName(L, -2, r.m_Kind == REFERENCE_GLOBALS, name);
                    Variable(d, L, r.m_Level, name.Data(), body, &emitted);
                }
                lua_pop(L, 1);
            }
        }
        lua_settop(L, top);
        body.Add("]}");
    }

    static bool SetVariable(Debugger* d, const Reference& r, const char* name, const char* expression, Buffer& body)
    {
        lua_State* L = r.m_L;
        int        top = lua_gettop(L);
        if (!Evaluate(d, L, r.m_Level, expression, false))
        {
            body.Add(lua_tostring(L, -1) ? lua_tostring(L, -1) : "Evaluation failed");
            lua_settop(L, top);
            return false;
        }
        int       value = lua_gettop(L);
        bool      found = false;
        lua_Debug ar;
        if (r.m_Kind == REFERENCE_LOCALS || r.m_Kind == REFERENCE_UPVALUES)
        {
            lua_getstack(L, r.m_Level, &ar);
            if (r.m_Kind == REFERENCE_LOCALS)
            {
                int local = LocalIndex(L, &ar, name);
                if (local)
                {
                    lua_pushvalue(L, value);
                    lua_setlocal(L, &ar, local);
                    found = true;
                }
            }
            else
            {
                lua_getinfo(L, "f", &ar);
                int upvalue = UpvalueIndex(L, lua_gettop(L), name);
                if (upvalue)
                {
                    lua_pushvalue(L, value);
                    lua_setupvalue(L, -2, upvalue);
                    found = true;
                }
            }
        }
        else
        {
            if (r.m_Kind == REFERENCE_GLOBALS)
            {
                lua_getstack(L, r.m_Level, &ar);
                lua_getinfo(L, "f", &ar);
                lua_getfenv(L, -1);
            }
            else
                lua_rawgeti(L, LUA_REGISTRYINDEX, r.m_LuaRef);
            int table = lua_gettop(L);
            lua_pushnil(L);
            while (lua_next(L, table))
            {
                Buffer key;
                KeyName(L, -2, r.m_Kind == REFERENCE_GLOBALS, key);
                lua_pop(L, 1);
                if (!strcmp(key.Data(), name))
                {
                    lua_pushvalue(L, value);
                    lua_rawset(L, table);
                    found = true;
                    break;
                }
            }
        }
        if (found)
        {
            body.Add("{");
            ValueBody(d, L, r.m_Level, value, body, "value");
            body.Add("}");
        }
        else
            body.Add("Unknown variable");
        lua_settop(L, top);
        return found;
    }

    bool ValueRequest(Debugger* d, const Json& request, const char* command, int seq, int args)
    {
        if (strcmp(command, "stackTrace") && strcmp(command, "scopes") && strcmp(command, "variables") &&
            strcmp(command, "evaluate") && strcmp(command, "setVariable") && strcmp(command, "exceptionInfo"))
            return false;
        if (!d->m_Paused)
        {
            Respond(d, seq, command, 0, "Lua is running");
            return true;
        }
        Buffer      body;
        const char* error = 0;
        if (!strcmp(command, "stackTrace"))
        {
            int thread = request.Integer(request.Field(args, "threadId"));
            int start = request.Integer(request.Field(args, "startFrame"), 0);
            int count = request.Integer(request.Field(args, "levels"), 0);
            if (!FindThread(d, thread) || start < 0 || count < 0)
                error = "Invalid thread or stack range";
            else
            {
                body.Add("{\"stackFrames\":[");
                int ordinal = 0, emitted = 0;
                for (uint32_t i = 0; i < d->m_Frames.Size(); ++i)
                {
                    Frame& frame = d->m_Frames[i];
                    if (frame.m_ThreadId != thread)
                        continue;
                    if (!Page(ordinal++, start, count))
                        continue;
                    lua_Debug ar;
                    if (!FrameInfo(frame, &ar))
                        continue;
                    if (emitted++)
                        body.Add(",");
                    body.Format("{\"id\":%d,\"name\":", frame.m_Id);
                    body.String(ar.name ? ar.name : ar.what);
                    body.Format(",\"line\":%d,\"column\":%d", ar.currentline > 0 ? ar.currentline - !d->m_LinesStartAt1 : 0, d->m_ColumnsStartAt1 ? 1 : 0);
                    if (ar.source && ar.source[0] == '@')
                    {
                        Buffer path;
                        ClientPath(d, ar.source, path);
                        body.Add(",\"source\":{\"path\":");
                        body.String(path.Data());
                        body.Add(",\"sourceReference\":0}");
                    }
                    body.Add("}");
                }
                body.Format("],\"totalFrames\":%d}", ordinal);
            }
        }
        else if (!strcmp(command, "scopes"))
        {
            Frame* frame = FindFrame(d, request.Integer(request.Field(args, "frameId")));
            if (!frame)
                error = "Invalid frameId";
            else
            {
                body.Add("{\"scopes\":[");
                const char* names[] = { "Locals", "Upvalues", "Globals" };
                for (int i = 0; i < 3; ++i)
                {
                    if (i)
                        body.Add(",");
                    body.Format("{\"name\":\"%s\",\"variablesReference\":%d,\"expensive\":%s}", names[i], AddReference(d, frame->m_L, (ReferenceKind)i, frame->m_Level), i == 2 ? "true" : "false");
                }
                body.Add("]}");
            }
        }
        else if (!strcmp(command, "evaluate"))
        {
            Frame* frame = FindFrame(d, request.Integer(request.Field(args, "frameId")));
            if (request.Field(args, "frameId") < 0)
                for (uint32_t i = 0; i < d->m_Frames.Size(); ++i)
                    if (d->m_Frames[i].m_ThreadId == d->m_StoppedThread)
                    {
                        frame = &d->m_Frames[i];
                        break;
                    }
            const char* expression = request.String(request.Field(args, "expression"));
            if (!frame || !expression)
                error = "Invalid frameId or expression";
            else
            {
                lua_State* L = frame->m_L;
                if (Evaluate(d, L, frame->m_Level, expression, !strcmp(request.String(request.Field(args, "context"), ""), "repl")))
                {
                    body.Add("{");
                    ValueBody(d, L, frame->m_Level, -1, body, "result");
                    body.Add("}");
                }
                else
                {
                    body.Add(lua_tostring(L, -1) ? lua_tostring(L, -1) : "Evaluation failed");
                    error = body.Data();
                }
                lua_pop(L, 1);
            }
        }
        else if (!strcmp(command, "exceptionInfo"))
        {
            if (request.Integer(request.Field(args, "threadId")) != d->m_StoppedThread || !d->m_Exception.Size())
                error = "No exception on this thread";
            else
            {
                body.Add("{\"exceptionId\":\"LuaError\",\"breakMode\":\"unhandled\",\"description\":");
                body.String(d->m_Exception.Data(), d->m_Exception.Size());
                body.Add("}");
            }
        }
        else
        {
            int id = request.Integer(request.Field(args, "variablesReference"));
            // Copy: rendering nested tables can grow and relocate the reference array.
            Reference r = {};
            bool      found = false;
            for (uint32_t i = 0; i < d->m_References.Size(); ++i)
                if (d->m_References[i].m_Id == id)
                {
                    r = d->m_References[i];
                    found = true;
                    break;
                }
            if (!found)
                error = "Invalid variablesReference";
            else if (!strcmp(command, "variables"))
            {
                int         start = request.Integer(request.Field(args, "start"), 0);
                int         count = request.Integer(request.Field(args, "count"), 0);
                const char* filter = request.String(request.Field(args, "filter"), "");
                if (start < 0 || count < 0 || (filter[0] && strcmp(filter, "named") && strcmp(filter, "indexed")))
                    error = "Invalid variable range or filter";
                else
                    Variables(d, r, body, start, count, filter);
            }
            else
            {
                const char* name = request.String(request.Field(args, "name"));
                const char* value = request.String(request.Field(args, "value"));
                if (!name || !value)
                    error = "Missing variable name or value";
                else if (!SetVariable(d, r, name, value, body))
                    error = body.Data();
            }
        }
        Respond(d, seq, command, error ? 0 : &body, error);
        return true;
    }
} // namespace dmDebugger
#endif
