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
#include "dstrings.h"
#include "json.h"
#include "webserver_openapi.h"

namespace dmWebServer
{
    static bool AddUniqueHash(dmSet<dmhash_t>* hashes, dmhash_t hash)
    {
        if (hashes->Full())
            hashes->OffsetCapacity(8);
        return hashes->Add(hash);
    }

    static bool IsMethod(const char* key)
    {
        return dmStrEq(key, "get") || dmStrEq(key, "put") || dmStrEq(key, "post")
               || dmStrEq(key, "delete") || dmStrEq(key, "options") || dmStrEq(key, "head")
               || dmStrEq(key, "patch") || dmStrEq(key, "trace");
    }

    static bool ValidatePathItem(const dmJson::Document& json, int path)
    {
        if (json.Get(path).m_Type != dmJson::TYPE_OBJECT)
        {
            return false;
        }
        bool has_operation = false;
        for (int field = json.Get(path).m_First; field >= 0; field = json.Get(field).m_Next)
        {
            const dmJson::Node& node = json.Get(field);
            if (IsMethod(node.m_Name))
            {
                const dmJson::Node& responses = json.Get(json.Field(field, "responses"));
                if (responses.m_Type != dmJson::TYPE_OBJECT || responses.m_First < 0)
                {
                    return false;
                }
                has_operation = true;
            }
            else if (dmStrEq(node.m_Name, "$ref"))
            {
                if (node.m_Type != dmJson::TYPE_STRING)
                {
                    return false;
                }
                has_operation = true;
            }
            else if (!dmStrEq(node.m_Name, "summary") && !dmStrEq(node.m_Name, "description")
                     && !dmStrEq(node.m_Name, "parameters") && !dmStrEq(node.m_Name, "servers")
                     && strncmp(node.m_Name, "x-", 2) != 0)
            {
                return false;
            }
        }
        return has_operation;
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

    static bool ParseError(uint32_t offset, const char* message, OpenAPIError* error)
    {
        if (error)
        {
            error->m_Message = message;
            error->m_Offset = offset;
        }
        return false;
    }

    bool ParseOpenAPI(const char* prefix, const char* json, OpenAPI* openapi, OpenAPIError* error)
    {
        if (!json)
        {
            return ParseError(0, "Expected a non-empty Paths Object", error);
        }
        dmJson::Document document;
        uint32_t offset;
        if (!document.Parse(json, (uint32_t)strlen(json), &offset, dmJson::PARSE_PRESERVE_VALUES))
        {
            return ParseError(offset, document.m_Valid ? "Unexpected data after Paths Object" : "Invalid JSON", error);
        }
        const dmJson::Node& root = document.Get(0);
        if (root.m_Type != dmJson::TYPE_OBJECT || root.m_First < 0)
        {
            return ParseError(root.m_Offset + (root.m_Type == dmJson::TYPE_OBJECT), "Expected a non-empty Paths Object", error);
        }
        // Retain the original encoded members for aggregation and conflict diagnostics.
        uint32_t start = (uint32_t)(document.Get(root.m_First).m_Name - document.m_Text - 1);
        openapi->m_Json = json + start;
        openapi->m_Length = root.m_Offset + root.m_Length - 1 - start;
        dmArray<char> path;
        for (int field = root.m_First; field >= 0; field = document.Get(field).m_Next)
        {
            const dmJson::Node& node = document.Get(field);
            uint32_t key_offset = (uint32_t)(node.m_Name - document.m_Text - 1);
            if (node.m_Name[0] != '/' || strncmp(node.m_Name, prefix, strlen(prefix)) != 0)
            {
                return ParseError(key_offset, "Path must start with the handler prefix", error);
            }
            uint32_t length = (uint32_t)strlen(node.m_Name) + 1;
            if (path.Capacity() < length)
            {
                path.SetCapacity(length);
            }
            path.SetSize(length);
            memcpy(path.Begin(), node.m_Name, length);
            if (!NormalizePath(&path))
            {
                return ParseError(key_offset, "Invalid path or path parameter", error);
            }
            dmhash_t hash = dmHashString64(path.Begin());
            if (!AddUniqueHash(&openapi->m_Paths, hash))
            {
                return ParseError(key_offset, "Duplicate path (including renamed path parameters)", error);
            }
            if (!ValidatePathItem(document, field))
            {
                return ParseError(node.m_Offset, "Invalid Path Item; operations require non-empty responses", error);
            }
            OpenAPIPath info;
            info.m_Hash = hash;
            info.m_Offset = key_offset + 1 - start;
            info.m_Length = node.m_NameLength;
            if (openapi->m_PathInfo.Full())
            {
                openapi->m_PathInfo.OffsetCapacity(8);
            }
            openapi->m_PathInfo.Push(info);
        }
        return true;
    }

    const OpenAPIPath* FindOpenAPIPath(const OpenAPI* openapi, dmhash_t hash)
    {
        for (uint32_t i = 0; i < openapi->m_PathInfo.Size(); ++i)
            if (openapi->m_PathInfo[i].m_Hash == hash)
                return &openapi->m_PathInfo[i];
        return 0;
    }
}
