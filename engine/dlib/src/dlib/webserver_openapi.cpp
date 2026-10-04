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

#include <string.h>
#include "webserver_openapi.h"

namespace dmWebServer
{
    enum ObjectType
    {
        OBJECT_ANY,
        OBJECT_PATH_ITEM,
        OBJECT_OPERATION,
        OBJECT_RESPONSES
    };

    static void SkipSpace(const char*& p)
    {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            ++p;
    }

    static bool Consume(const char*& p, char c)
    {
        SkipSpace(p);
        if (*p != c)
            return false;
        ++p;
        SkipSpace(p);
        return true;
    }

    static void AppendChar(dmArray<char>* out, char c)
    {
        if (out)
        {
            if (out->Full())
                out->OffsetCapacity(64);
            out->Push(c);
        }
    }

    static bool ReadHex(const char*& p, uint32_t* value)
    {
        *value = 0;
        for (uint32_t i = 0; i < 4; ++i)
        {
            uint32_t digit;
            if (*p >= '0' && *p <= '9') digit = *p - '0';
            else if (*p >= 'a' && *p <= 'f') digit = *p - 'a' + 10;
            else if (*p >= 'A' && *p <= 'F') digit = *p - 'A' + 10;
            else return false;
            *value = (*value << 4) | digit;
            ++p;
        }
        return true;
    }

    static void AppendCodepoint(dmArray<char>* out, uint32_t c)
    {
        if (c >= 0x10000) AppendChar(out, (char)(0xf0 | (c >> 18)));
        else if (c >= 0x800) AppendChar(out, (char)(0xe0 | (c >> 12)));
        else if (c >= 0x80) AppendChar(out, (char)(0xc0 | (c >> 6)));
        if (c >= 0x10000) AppendChar(out, (char)(0x80 | ((c >> 12) & 63)));
        if (c >= 0x800) AppendChar(out, (char)(0x80 | ((c >> 6) & 63)));
        AppendChar(out, c >= 0x80 ? (char)(0x80 | (c & 63)) : (char)c);
    }

    static bool ReadString(const char*& p, dmArray<char>* out)
    {
        if (*p != '"')
            return false;
        ++p;
        if (out)
            out->SetSize(0);
        while (*p && *p != '"')
        {
            uint32_t c = (uint8_t)*p++;
            if (c < 32)
                return false;
            if (c == '\\')
            {
                if (!*p)
                    return false;
                c = (uint8_t)*p++;
                switch (c)
                {
                    case '"': case '\\': case '/': break;
                    case 'b': c = '\b'; break;
                    case 'f': c = '\f'; break;
                    case 'n': c = '\n'; break;
                    case 'r': c = '\r'; break;
                    case 't': c = '\t'; break;
                    case 'u':
                    {
                        if (!ReadHex(p, &c))
                            return false;
                        if (c >= 0xd800 && c <= 0xdbff)
                        {
                            uint32_t low;
                            if (strncmp(p, "\\u", 2) != 0)
                                return false;
                            p += 2;
                            if (!ReadHex(p, &low) || low < 0xdc00 || low > 0xdfff)
                                return false;
                            c = 0x10000 + ((c - 0xd800) << 10) + low - 0xdc00;
                        }
                        else if (c >= 0xdc00 && c <= 0xdfff)
                            return false;
                        break;
                    }
                    default: return false;
                }
            }
            else if (c >= 0x80)
            {
                uint32_t count;
                uint32_t minimum;
                if (c >= 0xc2 && c <= 0xdf) { count = 1; minimum = 0x80; c &= 0x1f; }
                else if (c >= 0xe0 && c <= 0xef) { count = 2; minimum = 0x800; c &= 0x0f; }
                else if (c >= 0xf0 && c <= 0xf4) { count = 3; minimum = 0x10000; c &= 7; }
                else return false;
                for (uint32_t i = 0; i < count; ++i)
                {
                    if (((uint8_t)*p & 0xc0) != 0x80)
                        return false;
                    c = (c << 6) | ((uint8_t)*p++ & 63);
                }
                if (c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
                    return false;
            }
            AppendCodepoint(out, c);
        }
        if (*p != '"')
            return false;
        ++p;
        AppendChar(out, 0);
        return true;
    }

    static bool AddUniqueHash(dmSet<dmhash_t>* hashes, dmhash_t hash)
    {
        if (hashes->Full())
            hashes->OffsetCapacity(8);
        return hashes->Add(hash);
    }

    static bool IsMethod(const char* key)
    {
        return strcmp(key, "get") == 0 || strcmp(key, "put") == 0 || strcmp(key, "post") == 0
            || strcmp(key, "delete") == 0 || strcmp(key, "options") == 0 || strcmp(key, "head") == 0
            || strcmp(key, "patch") == 0 || strcmp(key, "trace") == 0;
    }

    static bool ReadValue(const char*& p, uint32_t depth, ObjectType type);

    static bool ReadObject(const char*& p, uint32_t depth, ObjectType type)
    {
        if (!Consume(p, '{'))
            return false;
        dmArray<char> key;
        dmSet<dmhash_t> keys;
        bool has_operation = false;
        bool has_responses = false;
        while (*p != '}')
        {
            if (!ReadString(p, &key) || !AddUniqueHash(&keys, dmHashBufferNoReverse64(key.Begin(), key.Size() - 1)) || !Consume(p, ':'))
                return false;
            // Embedded zero characters must not change how a field name is interpreted.
            if (strlen(key.Begin()) != key.Size() - 1)
                return false;
            ObjectType child_type = OBJECT_ANY;
            if (type == OBJECT_PATH_ITEM)
            {
                if (IsMethod(key.Begin()))
                {
                    has_operation = true;
                    child_type = OBJECT_OPERATION;
                }
                else if (strcmp(key.Begin(), "$ref") == 0)
                    has_operation = *p == '"';
                else if (strcmp(key.Begin(), "summary") != 0 && strcmp(key.Begin(), "description") != 0
                    && strcmp(key.Begin(), "parameters") != 0 && strcmp(key.Begin(), "servers") != 0
                    && strncmp(key.Begin(), "x-", 2) != 0)
                    return false;
            }
            else if (type == OBJECT_OPERATION && strcmp(key.Begin(), "responses") == 0)
            {
                has_responses = true;
                child_type = OBJECT_RESPONSES;
            }
            if (!ReadValue(p, depth + 1, child_type))
                return false;
            SkipSpace(p);
            if (*p == '}')
                break;
            if (!Consume(p, ',') || *p == '}')
                return false;
        }
        if (!Consume(p, '}'))
            return false;
        return (type != OBJECT_PATH_ITEM || has_operation)
            && (type != OBJECT_OPERATION || has_responses)
            && (type != OBJECT_RESPONSES || !keys.Empty());
    }

    static bool ReadValue(const char*& p, uint32_t depth, ObjectType type)
    {
        if (depth > 64)
            return false;
        SkipSpace(p);
        if (type != OBJECT_ANY || *p == '{')
            return ReadObject(p, depth, type);
        if (*p == '"')
            return ReadString(p, 0);
        if (Consume(p, '['))
        {
            if (*p == ']')
                return Consume(p, ']');
            do
            {
                if (!ReadValue(p, depth + 1, OBJECT_ANY))
                    return false;
                SkipSpace(p);
                if (*p == ']')
                    return Consume(p, ']');
            } while (Consume(p, ','));
            return false;
        }
        const char* literals[] = {"true", "false", "null"};
        for (uint32_t i = 0; i < 3; ++i)
        {
            uint32_t length = (uint32_t)strlen(literals[i]);
            if (strncmp(p, literals[i], length) == 0)
            {
                p += length;
                return true;
            }
        }
        if (*p == '-') ++p;
        if (*p == '0') ++p;
        else
        {
            if (*p < '1' || *p > '9')
                return false;
            while (*p >= '0' && *p <= '9') ++p;
        }
        if (*p == '.')
        {
            ++p;
            if (*p < '0' || *p > '9') return false;
            while (*p >= '0' && *p <= '9') ++p;
        }
        if (*p == 'e' || *p == 'E')
        {
            ++p;
            if (*p == '+' || *p == '-') ++p;
            if (*p < '0' || *p > '9') return false;
            while (*p >= '0' && *p <= '9') ++p;
        }
        return true;
    }

    static bool NormalizePath(dmArray<char>* path)
    {
        uint32_t write = 0;
        bool parameter = false;
        for (uint32_t read = 0; read + 1 < path->Size(); ++read)
        {
            char c = (*path)[read];
            if ((uint8_t)c <= 32 || c == '?' || c == '#')
                return false;
            if (c == '{')
            {
                if (parameter || (*path)[read + 1] == '}') return false;
                parameter = true;
                (*path)[write++] = c;
            }
            else if (c == '}')
            {
                if (!parameter) return false;
                parameter = false;
                (*path)[write++] = c;
            }
            else if (!parameter)
                (*path)[write++] = c;
            else if (c == '/')
                return false;
        }
        (*path)[write] = 0;
        return !parameter;
    }

    static bool ParseError(const char* json, const char* p, const char* message, OpenAPIError* error)
    {
        if (error)
        {
            error->m_Message = message;
            error->m_Offset = json ? (uint32_t)(p - json) : 0;
        }
        return false;
    }

    bool ParseOpenAPI(const char* prefix, const char* json, OpenAPI* openapi, OpenAPIError* error)
    {
        if (!json)
            return ParseError(0, 0, "Expected a non-empty Paths Object", error);
        const char* p = json;
        if (!Consume(p, '{') || *p == '}')
            return ParseError(json, p, "Expected a non-empty Paths Object", error);
        openapi->m_Json = p;
        dmArray<char> path;
        do
        {
            const char* key = p;
            if (!ReadString(p, &path))
                return ParseError(json, p, "Expected a JSON path string", error);
            if (path[0] != '/' || strncmp(path.Begin(), prefix, strlen(prefix)) != 0)
                return ParseError(json, key, "Path must start with the handler prefix", error);
            if (!NormalizePath(&path))
                return ParseError(json, key, "Invalid path or path parameter", error);
            dmhash_t hash = dmHashString64(path.Begin());
            if (!AddUniqueHash(&openapi->m_Paths, hash))
                return ParseError(json, key, "Duplicate path (including renamed path parameters)", error);
            OpenAPIPath info;
            info.m_Hash = hash;
            info.m_Offset = (uint32_t)(key + 1 - openapi->m_Json);
            info.m_Length = (uint32_t)(p - key - 2);
            if (openapi->m_PathInfo.Full())
                openapi->m_PathInfo.OffsetCapacity(8);
            openapi->m_PathInfo.Push(info);
            if (!Consume(p, ':'))
                return ParseError(json, p, "Expected ':' after path", error);
            if (!ReadValue(p, 1, OBJECT_PATH_ITEM))
                return ParseError(json, p, "Invalid JSON or Path Item; operations require non-empty responses", error);
            SkipSpace(p);
            if (*p == '}')
            {
                openapi->m_Length = (uint32_t)(p - openapi->m_Json);
                Consume(p, '}');
                if (*p != 0)
                    return ParseError(json, p, "Unexpected data after Paths Object", error);
                return true;
            }
        } while (Consume(p, ','));
        return ParseError(json, p, "Expected ',' or '}' after Path Item", error);
    }

    const OpenAPIPath* FindOpenAPIPath(const OpenAPI* openapi, dmhash_t hash)
    {
        for (uint32_t i = 0; i < openapi->m_PathInfo.Size(); ++i)
            if (openapi->m_PathInfo[i].m_Hash == hash)
                return &openapi->m_PathInfo[i];
        return 0;
    }
}
