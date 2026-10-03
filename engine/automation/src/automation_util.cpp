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

#include "automation_private.h"

#include <dmsdk/dlib/uri.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace dmAutomation
{
    char* DuplicateStringN(const char* value, uint32_t length)
    {
        char* copy = (char*)malloc((size_t)length + 1);
        if (!copy)
        {
            return 0;
        }
        if (length > 0 && value)
        {
            memcpy(copy, value, length);
        }
        copy[length] = 0;
        return copy;
    }

    char* DuplicateString(const char* value)
    {
        if (!value)
        {
            value = "";
        }
        return DuplicateStringN(value, (uint32_t)strlen(value));
    }

    bool SetString(char** target, const char* value)
    {
        char* copy = DuplicateString(value);
        if (!copy)
        {
            return false;
        }
        free(*target);
        *target = copy;
        return true;
    }

    void FreeString(char** value)
    {
        free(*value);
        *value = 0;
    }

    void StringBufferInit(StringBuffer* buffer)
    {
        memset(buffer, 0, sizeof(*buffer));
    }

    void StringBufferFree(StringBuffer* buffer)
    {
        free(buffer->m_Data);
        memset(buffer, 0, sizeof(*buffer));
    }

    bool StringBufferReserve(StringBuffer* buffer, uint32_t size)
    {
        if (size <= buffer->m_Capacity)
        {
            return true;
        }

        uint32_t new_capacity = buffer->m_Capacity ? buffer->m_Capacity : 256;
        while (new_capacity < size)
        {
            if (new_capacity > 0x7fffffffU / 2)
            {
                new_capacity = size;
                break;
            }
            new_capacity *= 2;
        }

        char* data = (char*)realloc(buffer->m_Data, new_capacity);
        if (!data)
        {
            buffer->m_Failed = true;
            return false;
        }

        buffer->m_Data = data;
        buffer->m_Capacity = new_capacity;
        return true;
    }

    void StringBufferAppendN(StringBuffer* buffer, const char* value, uint32_t length)
    {
        if (buffer->m_Failed)
        {
            return;
        }
        if (!value)
        {
            value = "";
            length = 0;
        }
        if (!StringBufferReserve(buffer, buffer->m_Size + length + 1))
        {
            return;
        }
        if (length > 0)
        {
            memcpy(buffer->m_Data + buffer->m_Size, value, length);
        }
        buffer->m_Size += length;
        buffer->m_Data[buffer->m_Size] = 0;
    }

    void StringBufferAppend(StringBuffer* buffer, const char* value)
    {
        StringBufferAppendN(buffer, value, value ? (uint32_t)strlen(value) : 0);
    }

    void StringBufferAppendChar(StringBuffer* buffer, char value)
    {
        StringBufferAppendN(buffer, &value, 1);
    }

    char* StringBufferDetach(StringBuffer* buffer)
    {
        if (!buffer->m_Data)
        {
            return DuplicateString("");
        }

        char* data = buffer->m_Data;
        buffer->m_Data = 0;
        buffer->m_Size = 0;
        buffer->m_Capacity = 0;
        buffer->m_Failed = false;
        return data;
    }

    void AppendNumber(StringBuffer* out, double value)
    {
        char buffer[64];
        if (!IsFiniteDouble(value))
        {
            value = 0.0;
        }
        dmSnPrintf(buffer, sizeof(buffer), "%.17g", value);
        StringBufferAppend(out, buffer);
    }

    void AppendJsonString(StringBuffer* out, const char* value)
    {
        StringBufferAppendChar(out, '"');
        if (value)
        {
            while (*value)
            {
                unsigned char c = (unsigned char)*value++;
                switch (c)
                {
                case '\\':
                    StringBufferAppend(out, "\\\\");
                    break;
                case '"':
                    StringBufferAppend(out, "\\\"");
                    break;
                case '\n':
                    StringBufferAppend(out, "\\n");
                    break;
                case '\r':
                    StringBufferAppend(out, "\\r");
                    break;
                case '\t':
                    StringBufferAppend(out, "\\t");
                    break;
                default:
                    if (c < 32)
                    {
                        char buffer[8];
                        dmSnPrintf(buffer, sizeof(buffer), "\\u%04x", c);
                        StringBufferAppend(out, buffer);
                    }
                    else
                    {
                        StringBufferAppendChar(out, (char)c);
                    }
                    break;
                }
            }
        }
        StringBufferAppendChar(out, '"');
    }

    bool SetHashString(char** out, dmhash_t hash)
    {
        const char* value = dmHashReverseSafe64(hash);
        if (value)
        {
            return SetString(out, value);
        }

        char buffer[32];
        dmSnPrintf(buffer, sizeof(buffer), "hash:%016llx", (unsigned long long)hash);
        return SetString(out, buffer);
    }

    static char* UrlDecode(const char* value, uint32_t length)
    {
        char* encoded = DuplicateStringN(value, length);
        if (!encoded)
        {
            return 0;
        }

        char* decoded = (char*)malloc((size_t)length + 1);
        if (!decoded)
        {
            free(encoded);
            return 0;
        }

        dmURI::Decode(encoded, decoded);
        free(encoded);
        return decoded;
    }

    static void FreeQueryParam(QueryParam* param)
    {
        FreeString(&param->m_Key);
        FreeString(&param->m_Value);
    }

    void FreeQueryParams(dmArray<QueryParam>* query)
    {
        for (uint32_t i = 0; i < query->Size(); ++i)
        {
            FreeQueryParam(&query->Begin()[i]);
        }
        ArrayFree(query);
    }

    void ParseResource(const char* resource, char** path, dmArray<QueryParam>* query)
    {
        SetString(path, "");
        if (!resource)
        {
            return;
        }

        const char* question = strchr(resource, '?');
        if (!question)
        {
            free(*path);
            *path = UrlDecode(resource, strlen(resource));
            return;
        }

        free(*path);
        *path = UrlDecode(resource, (uint32_t)(question - resource));

        const char* cursor = question + 1;
        while (*cursor)
        {
            const char* amp = strchr(cursor, '&');
            if (!amp)
            {
                amp = cursor + strlen(cursor);
            }

            const char* equals = (const char*)memchr(cursor, '=', (size_t)(amp - cursor));
            QueryParam param;
            memset(&param, 0, sizeof(param));
            if (equals)
            {
                param.m_Key = UrlDecode(cursor, (uint32_t)(equals - cursor));
                param.m_Value = UrlDecode(equals + 1, (uint32_t)(amp - equals - 1));
            }
            else
            {
                param.m_Key = UrlDecode(cursor, (uint32_t)(amp - cursor));
                param.m_Value = DuplicateString("");
            }

            if (!IsEmpty(param.m_Key))
            {
                if (!ArrayPush(query, &param))
                {
                    FreeQueryParam(&param);
                }
            }
            else
            {
                FreeQueryParam(&param);
            }

            cursor = *amp ? amp + 1 : amp;
        }
    }

    const char* GetParam(const dmArray<QueryParam>* query, const char* key)
    {
        for (uint32_t i = 0; i < query->Size(); ++i)
        {
            if (StringsEqual(query->Begin()[i].m_Key, key))
            {
                return query->Begin()[i].m_Value;
            }
        }
        return 0;
    }

    bool GetFloatParam(const dmArray<QueryParam>* query, const char* key, float* value)
    {
        const char* text = GetParam(query, key);
        if (IsEmpty(text))
        {
            return false;
        }

        char* end = 0;
        double parsed = strtod(text, &end);
        if (end == text || *end != 0 || !IsFiniteDouble(parsed) || !IsFiniteFloat((float)parsed))
        {
            return false;
        }

        *value = (float)parsed;
        return true;
    }

    bool GetBoolParam(const dmArray<QueryParam>* query, const char* key, bool* value)
    {
        const char* text = GetParam(query, key);
        if (!text)
        {
            return false;
        }

        *value = StringsEqual(text, "1") || StringsEqual(text, "true") || StringsEqual(text, "yes");
        return true;
    }

    bool ContainsCaseInsensitive(const char* haystack, const char* needle)
    {
        if (IsEmpty(needle))
        {
            return true;
        }
        if (!haystack)
        {
            return false;
        }

        uint32_t haystack_len = (uint32_t)strlen(haystack);
        uint32_t needle_len = (uint32_t)strlen(needle);
        if (haystack_len < needle_len)
        {
            return false;
        }

        for (uint32_t i = 0; i <= haystack_len - needle_len; ++i)
        {
            bool match = true;
            for (uint32_t j = 0; j < needle_len; ++j)
            {
                if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j]))
                {
                    match = false;
                    break;
                }
            }
            if (match)
            {
                return true;
            }
        }
        return false;
    }

    bool ContainsCaseSensitive(const char* haystack, const char* needle)
    {
        if (IsEmpty(needle))
        {
            return true;
        }
        return haystack && strstr(haystack, needle) != 0;
    }


}
