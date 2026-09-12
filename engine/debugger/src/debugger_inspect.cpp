// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0. See https://www.defold.com/license

#if !defined(DM_RELEASE)
#include "debugger_private.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>

namespace dmDebugger
{
    static bool IdentifierStart(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }
    static bool IdentifierChar(char c)
    {
        return IdentifierStart(c) || (c >= '0' && c <= '9');
    }
    static bool Space(char c)
    {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }
    static void SkipSpace(const char*& p)
    {
        while (Space(*p))
            ++p;
    }

    bool IsIdentifier(const char* name)
    {
        if (!IdentifierStart(*name))
            return false;
        for (const char* p = name + 1; *p; ++p)
            if (!IdentifierChar(*p))
                return false;
        const char* keywords[] = { "and", "break", "do", "else", "elseif", "end", "false", "for", "function", "if", "in", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while" };
        for (uint32_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); ++i)
            if (!strcmp(name, keywords[i]))
                return false;
        return true;
    }

    void KeyExpression(lua_State* L, int index, const char* parent, Buffer& expression)
    {
        int         type = lua_type(L, index);
        size_t      length = 0;
        const char* key = type == LUA_TSTRING ? lua_tolstring(L, index, &length) : 0;
        if (key && strlen(key) == length && IsIdentifier(key))
        {
            if (parent[0])
            {
                expression.Add(parent);
                expression.Add(".");
            }
            expression.Add(key);
            return;
        }
        if (!parent[0] || (type != LUA_TSTRING && type != LUA_TNUMBER && type != LUA_TBOOLEAN))
            return;
        if (type == LUA_TNUMBER && !(lua_tonumber(L, index) >= -DBL_MAX && lua_tonumber(L, index) <= DBL_MAX))
            return;
        expression.Add(parent);
        expression.Add("[");
        if (key)
        {
            // Lua 5.1 uses decimal byte escapes, not JSON's Unicode escapes.
            expression.Add("\"");
            for (size_t i = 0; i < length; ++i)
            {
                unsigned char c = (unsigned char)key[i];
                if (c < 32 || c >= 127)
                    expression.Format("\\%03u", (unsigned)c);
                else
                {
                    if (c == '\\' || c == '"')
                        expression.Add("\\");
                    expression.Add(key + i, 1);
                }
            }
            expression.Add("\"");
        }
        else if (type == LUA_TNUMBER)
            expression.Format("%.17g", (double)lua_tonumber(L, index));
        else
            expression.Add(lua_toboolean(L, index) ? "true" : "false");
        expression.Add("]");
    }

    static bool ReadIdentifier(const char*& p, Buffer& name)
    {
        SkipSpace(p);
        const char* begin = p;
        if (!IdentifierStart(*p))
            return false;
        while (IdentifierChar(*p))
            ++p;
        name.Add(begin, (uint32_t)(p - begin));
        return IsIdentifier(name.Data());
    }

    static bool ReadKey(lua_State* L, const char*& p)
    {
        SkipSpace(p);
        if (*p == '\'' || *p == '"')
        {
            char   quote = *p++;
            Buffer key;
            while (*p && *p != quote)
            {
                char c = *p++;
                if (c == '\n' || c == '\r')
                    return false;
                if (c == '\\')
                {
                    if (!*p)
                        return false;
                    c = *p++;
                    if (c >= '0' && c <= '9')
                    {
                        unsigned value = (unsigned)(c - '0');
                        for (int i = 1; i < 3 && *p >= '0' && *p <= '9'; ++i)
                            value = value * 10 + (unsigned)(*p++ - '0');
                        if (value > 255)
                            return false;
                        c = (char)value;
                    }
                    else
                    {
                        switch (c)
                        {
                            case 'a':
                                c = '\a';
                                break;
                            case 'b':
                                c = '\b';
                                break;
                            case 'f':
                                c = '\f';
                                break;
                            case 'n':
                                c = '\n';
                                break;
                            case 'r':
                                c = '\r';
                                break;
                            case 't':
                                c = '\t';
                                break;
                            case 'v':
                                c = '\v';
                                break;
                            case '\\':
                            case '\'':
                            case '"':
                                break;
                            default:
                                return false;
                        }
                    }
                }
                key.Add(&c, 1);
            }
            if (*p != quote)
                return false;
            ++p;
            lua_pushlstring(L, key.Data(), key.Size());
            return true;
        }
        if (!strncmp(p, "true", 4) && !IdentifierChar(p[4]))
        {
            p += 4;
            lua_pushboolean(L, true);
            return true;
        }
        if (!strncmp(p, "false", 5) && !IdentifierChar(p[5]))
        {
            p += 5;
            lua_pushboolean(L, false);
            return true;
        }
        if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '.')
        {
            char*  end;
            double value = strtod(p, &end);
            if (end == p || !(value >= -DBL_MAX && value <= DBL_MAX))
                return false;
            p = end;
            lua_pushnumber(L, value);
            return true;
        }
        return false;
    }

    static bool HasIndexMetamethod(lua_State* L, int index)
    {
        if (!lua_getmetatable(L, index))
            return false;
        lua_pushliteral(L, "__index");
        lua_rawget(L, -2);
        bool exists = !lua_isnil(L, -1);
        lua_pop(L, 2);
        return exists;
    }

    static bool RawIndex(lua_State* L)
    {
        if (!lua_istable(L, -2))
            return false;
        lua_rawget(L, -2);
        if (lua_isnil(L, -1) && HasIndexMetamethod(L, -2))
            return false;
        lua_remove(L, -2);
        return true;
    }

    static bool RootValue(lua_State* L, int level, const char* name)
    {
        lua_Debug frame;
        if (level >= 0)
        {
            if (!lua_getstack(L, level, &frame))
                return false;
            int local = LocalIndex(L, &frame, name);
            if (local)
            {
                lua_getlocal(L, &frame, local);
                return true;
            }
            lua_getinfo(L, "f", &frame);
            int upvalue = UpvalueIndex(L, lua_gettop(L), name);
            if (upvalue)
            {
                lua_getupvalue(L, -1, upvalue);
                lua_remove(L, -2);
                return true;
            }
            lua_pop(L, 1);
        }
        PushEnvironment(L, level);
        lua_pushstring(L, name);
        return RawIndex(L);
    }

    bool Inspect(lua_State* L, int level, const char* expression)
    {
        int         top = lua_gettop(L);
        const char* p = expression;
        Buffer      root;
        bool        ok = ReadIdentifier(p, root) && RootValue(L, level, root.Data());
        while (ok)
        {
            SkipSpace(p);
            if (!*p)
                return true;
            if (*p == '.')
            {
                ++p;
                Buffer key;
                ok = ReadIdentifier(p, key);
                if (ok)
                    lua_pushlstring(L, key.Data(), key.Size());
            }
            else if (*p == '[')
            {
                ++p;
                ok = ReadKey(L, p);
                if (ok)
                {
                    SkipSpace(p);
                    if (*p != ']')
                        ok = false;
                    else
                        ++p;
                }
            }
            else
                ok = false;
            if (ok)
                ok = RawIndex(L);
        }
        lua_settop(L, top);
        lua_pushliteral(L, "Inspection supports identifiers and direct table paths; calls, expressions, and metamethods are not evaluated");
        return false;
    }

    struct Completion
    {
        char* m_Name;
        bool  m_Function;
    };

    static void AddCompletion(lua_State* L, int seen, dmArray<Completion>& items, const char* name, const char* prefix, bool methods)
    {
        if (items.Size() >= 256 || !IsIdentifier(name) || strncmp(name, prefix, strlen(prefix)) || (methods && !lua_isfunction(L, -1)))
            return;
        lua_pushstring(L, name);
        lua_rawget(L, seen);
        bool duplicate = !lua_isnil(L, -1);
        lua_pop(L, 1);
        if (duplicate)
            return;
        Completion item = { strdup(name), lua_isfunction(L, -1) != 0 };
        Push(items, item);
        lua_pushstring(L, name);
        lua_pushboolean(L, true);
        lua_rawset(L, seen);
    }

    static int CompareCompletions(const void* a, const void* b)
    {
        return strcmp(((const Completion*)a)->m_Name, ((const Completion*)b)->m_Name);
    }

    static const char* CompletionCursor(const char* text, int line, int column, const char** line_start)
    {
        if (line < 0 || column < 0)
            return 0;
        const char* p = text;
        for (int i = 0; i < line; ++i)
        {
            p = strchr(p, '\n');
            if (!p)
                return 0;
            ++p;
        }
        *line_start = p;
        while (column > 0 && *p && *p != '\r' && *p != '\n')
        {
            unsigned char c = (unsigned char)*p;
            if ((c >= 128 && c < 194) || c > 244)
                return 0;
            int bytes = c < 128 ? 1 : c < 224 ? 2 :
            c < 240                           ? 3 :
                                                4;
            for (int i = 1; i < bytes; ++i)
                if (((unsigned char)p[i] & 0xc0) != 0x80)
                    return 0;
            if ((c == 224 && (unsigned char)p[1] < 160) || (c == 237 && (unsigned char)p[1] >= 160) ||
                (c == 240 && (unsigned char)p[1] < 144) || (c == 244 && (unsigned char)p[1] >= 144))
                return 0;
            int units = bytes == 4 ? 2 : 1;
            if (column < units)
                return 0;
            column -= units;
            p += bytes;
        }
        return column == 0 ? p : 0;
    }

    bool Completions(Debugger* d, lua_State* L, int level, const Json& request, int args, Buffer& body)
    {
        const char* text = request.String(request.Field(args, "text"));
        int         line = request.Integer(request.Field(args, "line"), d->m_LinesStartAt1 ? 1 : 0);
        int         column = request.Integer(request.Field(args, "column"));
        const char* line_start = 0;
        const char* cursor = text && line >= d->m_LinesStartAt1 && column >= d->m_ColumnsStartAt1 ?
        CompletionCursor(text, line - d->m_LinesStartAt1, column - d->m_ColumnsStartAt1, &line_start) :
        0;
        if (!cursor)
        {
            body.Add("Invalid completion text, line, or UTF-16 column");
            return false;
        }
        column -= d->m_ColumnsStartAt1;
        const char* word = cursor;
        while (word > line_start && IdentifierChar(word[-1]))
            --word;
        const char* word_end = cursor;
        while (IdentifierChar(*word_end))
            ++word_end;
        Buffer prefix;
        prefix.Add(word, (uint32_t)(cursor - word));
        const char* separator = word;
        while (separator > line_start && Space(separator[-1]))
            --separator;
        bool member = separator > line_start && (separator[-1] == '.' || separator[-1] == ':');
        bool methods = member && separator[-1] == ':';

        int  top = lua_gettop(L);
        lua_newtable(L);
        int                 seen = lua_gettop(L);
        dmArray<Completion> items;
        bool                table = true;
        if (member)
        {
            const char* end = separator - 1;
            const char* begin = line_start;
            int         brackets = 0;
            char        quote = 0;
            for (const char* p = line_start; p < end; ++p)
            {
                if (quote)
                {
                    if (*p == '\\' && p + 1 < end)
                        ++p;
                    else if (*p == quote)
                        quote = 0;
                }
                else if (*p == '\'' || *p == '"')
                    quote = *p;
                else if (*p == '[')
                    ++brackets;
                else if (*p == ']')
                    --brackets;
                else if (!brackets && !IdentifierChar(*p) && *p != '.')
                    begin = p + 1;
            }
            Buffer target;
            target.Add(begin, (uint32_t)(end - begin));
            table = Inspect(L, level, target.Data()) && lua_istable(L, -1);
        }
        else
        {
            lua_Debug frame;
            if (level >= 0 && lua_getstack(L, level, &frame))
            {
                for (int i = 1;; ++i)
                {
                    const char* name = lua_getlocal(L, &frame, i);
                    if (!name)
                        break;
                    if (LocalIndex(L, &frame, name) == i)
                        AddCompletion(L, seen, items, name, prefix.Data(), false);
                    lua_pop(L, 1);
                }
                lua_getinfo(L, "f", &frame);
                int function = lua_gettop(L);
                for (int i = 1;; ++i)
                {
                    const char* name = lua_getupvalue(L, function, i);
                    if (!name)
                        break;
                    AddCompletion(L, seen, items, name, prefix.Data(), false);
                    lua_pop(L, 1);
                }
                lua_pop(L, 1);
            }
            PushEnvironment(L, level);
        }
        if (table)
        {
            lua_pushnil(L);
            while (lua_next(L, -2))
            {
                if (lua_type(L, -2) == LUA_TSTRING)
                {
                    size_t      length;
                    const char* name = lua_tolstring(L, -2, &length);
                    if (strlen(name) == length)
                        AddCompletion(L, seen, items, name, prefix.Data(), methods);
                }
                lua_pop(L, 1);
            }
        }
        if (items.Size() > 1)
            qsort(items.Begin(), items.Size(), sizeof(Completion), CompareCompletions);
        body.Add("{\"targets\":[");
        for (uint32_t i = 0; i < items.Size(); ++i)
        {
            body.Add(i ? ",{\"label\":" : "{\"label\":");
            body.String(items[i].m_Name);
            body.Format(",\"type\":\"%s\",\"start\":%d,\"length\":%d}",
                        items[i].m_Function ? (methods ? "method" : "function") : (member ? "field" : "variable"),
                        column - (int)(cursor - word) + d->m_ColumnsStartAt1,
                        (int)(word_end - word));
            free(items[i].m_Name);
        }
        body.Add("]}");
        lua_settop(L, top);
        return true;
    }
} // namespace dmDebugger
#endif
