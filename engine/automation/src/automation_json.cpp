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
#include <ctype.h>

namespace dmAutomation
{
    void JsonSkipWhitespace(const char** cursor)
    {
        while (**cursor == ' ' || **cursor == '\t' || **cursor == '\r' || **cursor == '\n')
        {
            ++*cursor;
        }
    }

    static int JsonHexValue(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    static bool JsonAppendCodepoint(StringBuffer* out, uint32_t codepoint)
    {
        if (codepoint == 0) return false; // Protocol string fields are NUL-terminated.
        if (codepoint <= 0x7f)
        {
            StringBufferAppendChar(out, (char)codepoint);
        }
        else if (codepoint <= 0x7ff)
        {
            StringBufferAppendChar(out, (char)(0xc0 | (codepoint >> 6)));
            StringBufferAppendChar(out, (char)(0x80 | (codepoint & 0x3f)));
        }
        else if (codepoint <= 0xffff)
        {
            StringBufferAppendChar(out, (char)(0xe0 | (codepoint >> 12)));
            StringBufferAppendChar(out, (char)(0x80 | ((codepoint >> 6) & 0x3f)));
            StringBufferAppendChar(out, (char)(0x80 | (codepoint & 0x3f)));
        }
        else if (codepoint <= 0x10ffff)
        {
            StringBufferAppendChar(out, (char)(0xf0 | (codepoint >> 18)));
            StringBufferAppendChar(out, (char)(0x80 | ((codepoint >> 12) & 0x3f)));
            StringBufferAppendChar(out, (char)(0x80 | ((codepoint >> 6) & 0x3f)));
            StringBufferAppendChar(out, (char)(0x80 | (codepoint & 0x3f)));
        }
        else
        {
            return false;
        }
        return !out->m_Failed;
    }

    static bool JsonParseUtf8Codepoint(unsigned char first, const char** cursor, uint32_t* value)
    {
        uint32_t length;
        uint32_t minimum;
        uint32_t codepoint;
        if (first >= 0xc2 && first <= 0xdf)
        {
            length = 2;
            minimum = 0x80;
            codepoint = first & 0x1f;
        }
        else if (first >= 0xe0 && first <= 0xef)
        {
            length = 3;
            minimum = 0x800;
            codepoint = first & 0x0f;
        }
        else if (first >= 0xf0 && first <= 0xf4)
        {
            length = 4;
            minimum = 0x10000;
            codepoint = first & 0x07;
        }
        else
        {
            return false;
        }
        for (uint32_t i = 1; i < length; ++i)
        {
            unsigned char next = (unsigned char)**cursor;
            if ((next & 0xc0) != 0x80) return false;
            ++*cursor;
            codepoint = (codepoint << 6) | (next & 0x3f);
        }
        if (codepoint < minimum || codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff)) return false;
        *value = codepoint;
        return true;
    }

    bool JsonParseString(const char** cursor, char** value)
    {
        if (**cursor != '"') return false;
        ++*cursor;
        StringBuffer decoded;
        StringBufferInit(&decoded);
        while (**cursor && **cursor != '"')
        {
            unsigned char c = (unsigned char)*(*cursor)++;
            if (c < 0x20)
            {
                StringBufferFree(&decoded);
                return false;
            }
            if (c != '\\')
            {
                uint32_t codepoint = c;
                if ((c >= 0x80 && !JsonParseUtf8Codepoint(c, cursor, &codepoint)) ||
                    !JsonAppendCodepoint(&decoded, codepoint))
                {
                    StringBufferFree(&decoded);
                    return false;
                }
                continue;
            }
            char escaped = *(*cursor)++;
            if (!escaped)
            {
                StringBufferFree(&decoded);
                return false;
            }
            switch (escaped)
            {
            case '"': StringBufferAppendChar(&decoded, '"'); break;
            case '\\': StringBufferAppendChar(&decoded, '\\'); break;
            case '/': StringBufferAppendChar(&decoded, '/'); break;
            case 'b': StringBufferAppendChar(&decoded, '\b'); break;
            case 'f': StringBufferAppendChar(&decoded, '\f'); break;
            case 'n': StringBufferAppendChar(&decoded, '\n'); break;
            case 'r': StringBufferAppendChar(&decoded, '\r'); break;
            case 't': StringBufferAppendChar(&decoded, '\t'); break;
            case 'u':
            {
                uint32_t codepoint = 0;
                for (uint32_t i = 0; i < 4; ++i)
                {
                    int digit = JsonHexValue(*(*cursor)++);
                    if (digit < 0)
                    {
                        StringBufferFree(&decoded);
                        return false;
                    }
                    codepoint = (codepoint << 4) | (uint32_t)digit;
                }
                if (codepoint >= 0xd800 && codepoint <= 0xdbff)
                {
                    if ((*cursor)[0] != '\\' || (*cursor)[1] != 'u')
                    {
                        StringBufferFree(&decoded);
                        return false;
                    }
                    *cursor += 2;
                    uint32_t low = 0;
                    for (uint32_t i = 0; i < 4; ++i)
                    {
                        int digit = JsonHexValue(*(*cursor)++);
                        if (digit < 0)
                        {
                            StringBufferFree(&decoded);
                            return false;
                        }
                        low = (low << 4) | (uint32_t)digit;
                    }
                    if (low < 0xdc00 || low > 0xdfff)
                    {
                        StringBufferFree(&decoded);
                        return false;
                    }
                    codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                }
                else if (codepoint >= 0xdc00 && codepoint <= 0xdfff)
                {
                    StringBufferFree(&decoded);
                    return false;
                }
                if (!JsonAppendCodepoint(&decoded, codepoint))
                {
                    StringBufferFree(&decoded);
                    return false;
                }
                break;
            }
            default:
                StringBufferFree(&decoded);
                return false;
            }
        }
        if (**cursor != '"')
        {
            StringBufferFree(&decoded);
            return false;
        }
        ++*cursor;
        *value = StringBufferDetach(&decoded);
        return *value != 0;
    }

    static bool JsonSkipValue(const char** cursor, uint32_t depth);

    bool JsonPushParam(dmArray<QueryParam>* params, const char* key, char* value)
    {
        QueryParam param;
        memset(&param, 0, sizeof(param));
        param.m_Key = DuplicateString(key);
        param.m_Value = value;
        if (!param.m_Key || !param.m_Value || !ArrayPush(params, &param))
        {
            FreeString(&param.m_Key);
            FreeString(&param.m_Value);
            return false;
        }
        return true;
    }

    static bool JsonSkipArray(const char** cursor, uint32_t depth)
    {
        if (depth > MAX_JSON_NESTING || **cursor != '[') return false;
        ++*cursor;
        JsonSkipWhitespace(cursor);
        if (**cursor == ']') { ++*cursor; return true; }
        while (**cursor)
        {
            if (!JsonSkipValue(cursor, depth + 1)) return false;
            JsonSkipWhitespace(cursor);
            if (**cursor == ']') { ++*cursor; return true; }
            if (**cursor != ',') return false;
            ++*cursor;
            JsonSkipWhitespace(cursor);
        }
        return false;
    }

    static bool JsonSkipObject(const char** cursor, uint32_t depth)
    {
        if (depth > MAX_JSON_NESTING || **cursor != '{') return false;
        ++*cursor;
        JsonSkipWhitespace(cursor);
        if (**cursor == '}') { ++*cursor; return true; }
        dmArray<QueryParam> keys;
        bool valid = false;
        while (**cursor)
        {
            char* key = 0;
            if (!JsonParseString(cursor, &key)) break;
            bool duplicate = GetParam(&keys, key) != 0;
            bool retained = !duplicate && JsonPushParam(&keys, key, DuplicateString("1"));
            free(key);
            if (!retained) break;
            JsonSkipWhitespace(cursor);
            if (**cursor != ':') break;
            ++*cursor;
            if (!JsonSkipValue(cursor, depth + 1)) break;
            JsonSkipWhitespace(cursor);
            if (**cursor == '}') { ++*cursor; valid = true; break; }
            if (**cursor != ',') break;
            ++*cursor;
            JsonSkipWhitespace(cursor);
        }
        FreeQueryParams(&keys);
        return valid;
    }

    static bool JsonSkipValue(const char** cursor, uint32_t depth)
    {
        if (depth > MAX_JSON_NESTING) return false;
        JsonSkipWhitespace(cursor);
        if (**cursor == '{') return JsonSkipObject(cursor, depth);
        if (**cursor == '[') return JsonSkipArray(cursor, depth);
        if (**cursor == '"')
        {
            char* value = 0;
            bool ok = JsonParseString(cursor, &value);
            free(value);
            return ok;
        }
        if (StartsWith(*cursor, "true")) { *cursor += 4; return true; }
        if (StartsWith(*cursor, "false")) { *cursor += 5; return true; }
        if (StartsWith(*cursor, "null")) { *cursor += 4; return true; }
        const char* number = *cursor;
        if (*number == '-') ++number;
        if (*number == '0')
        {
            ++number;
            if (isdigit((unsigned char)*number)) return false;
        }
        else
        {
            if (*number < '1' || *number > '9') return false;
            while (isdigit((unsigned char)*number)) ++number;
        }
        if (*number == '.')
        {
            ++number;
            if (!isdigit((unsigned char)*number)) return false;
            while (isdigit((unsigned char)*number)) ++number;
        }
        if (*number == 'e' || *number == 'E')
        {
            ++number;
            if (*number == '+' || *number == '-') ++number;
            if (!isdigit((unsigned char)*number)) return false;
            while (isdigit((unsigned char)*number)) ++number;
        }
        *cursor = number;
        return true;
    }

    static bool JsonParsePointObject(const char** cursor, dmArray<QueryParam>* params)
    {
        if (**cursor != '{') return false;
        ++*cursor;
        JsonSkipWhitespace(cursor);
        if (**cursor == '}') { ++*cursor; return true; }
        while (**cursor)
        {
            char* key = 0;
            if (!JsonParseString(cursor, &key)) return false;
            JsonSkipWhitespace(cursor);
            if (**cursor != ':') { free(key); return false; }
            ++*cursor;
            JsonSkipWhitespace(cursor);
            if (StringsEqual(key, "x") || StringsEqual(key, "y"))
            {
                if (!(**cursor == '-' || isdigit((unsigned char)**cursor))) { free(key); return false; }
                char* value = 0;
                if (**cursor == '"')
                {
                    if (!JsonParseString(cursor, &value)) { free(key); return false; }
                }
                else
                {
                    const char* start = *cursor;
                    if (!JsonSkipValue(cursor, 2)) { free(key); return false; }
                    value = DuplicateStringN(start, (uint32_t)(*cursor - start));
                }
                char flattened_key[16];
                dmSnPrintf(flattened_key, sizeof(flattened_key), "point.%s", key);
                if (GetParam(params, flattened_key)) { free(value); free(key); return false; }
                if (!JsonPushParam(params, flattened_key, value)) { free(key); return false; }
            }
            else if (!JsonSkipValue(cursor, 2))
            {
                free(key);
                return false;
            }
            free(key);
            JsonSkipWhitespace(cursor);
            if (**cursor == '}') { ++*cursor; return true; }
            if (**cursor != ',') return false;
            ++*cursor;
            JsonSkipWhitespace(cursor);
        }
        return false;
    }

    static bool JsonParseRootObject(const char* body, dmArray<QueryParam>* params, dmArray<QueryParam>* fields)
    {
        const char* cursor = body;
        JsonSkipWhitespace(&cursor);
        if (*cursor != '{') return false;
        ++cursor;
        JsonSkipWhitespace(&cursor);
        if (*cursor == '}') { ++cursor; JsonSkipWhitespace(&cursor); return *cursor == 0; }
        while (*cursor)
        {
            QueryParam param;
            memset(&param, 0, sizeof(param));
            if (!JsonParseString(&cursor, &param.m_Key)) return false;
            if (GetParam(fields, param.m_Key) || !JsonPushParam(fields, param.m_Key, DuplicateString("1")))
            {
                FreeString(&param.m_Key);
                return false;
            }
            JsonSkipWhitespace(&cursor);
            if (*cursor != ':') { FreeString(&param.m_Key); return false; }
            ++cursor;
            JsonSkipWhitespace(&cursor);

            const char* value_start = cursor;
            if (!JsonSkipValue(&cursor, 1)) { FreeString(&param.m_Key); return false; }
            char* raw = DuplicateStringN(value_start, (uint32_t)(cursor - value_start));
            QueryParam* field = &fields->Begin()[fields->Size() - 1];
            FreeString(&field->m_Value);
            field->m_Value = raw;
            if (*value_start == '"' && !StringsEqual(param.m_Key, "data") && !StringsEqual(param.m_Key, "arguments"))
            {
                const char* decoded = value_start;
                if (!JsonParseString(&decoded, &param.m_Value)) { FreeString(&param.m_Key); return false; }
            }
            else if (StringsEqual(param.m_Key, "point") && *value_start == '{')
            {
                const char* point = value_start;
                if (!JsonParsePointObject(&point, params)) { FreeString(&param.m_Key); return false; }
            }
            else if (!StringsEqual(raw, "null") || StringsEqual(param.m_Key, "data") || StringsEqual(param.m_Key, "arguments"))
                param.m_Value = DuplicateString(raw);

            if (param.m_Value)
            {
                if (!ArrayPush(params, &param))
                {
                    FreeString(&param.m_Key);
                    FreeString(&param.m_Value);
                    return false;
                }
            }
            else
            {
                FreeString(&param.m_Key);
            }
            JsonSkipWhitespace(&cursor);
            if (*cursor == '}')
            {
                ++cursor;
                JsonSkipWhitespace(&cursor);
                return *cursor == 0;
            }
            if (*cursor != ',') return false;
            ++cursor;
            JsonSkipWhitespace(&cursor);
        }
        return false;
    }

    static bool ValidateJsonTypes(const dmArray<QueryParam>* fields)
    {
        static const char* numbers[] = {"x", "y", "x1", "y1", "x2", "y2", "expected_scene_sequence", "limit", "offset", "from_x", "from_y", "to_x", "to_y", "duration", "hold_before", "hold_after", "hold", "lease", "pointer_lease", "width", "height", "fps", "frames", "after_frames", "timeout_ms", "pointer_id", "input_id", "capture_id", "operation_id", "command_id", "frame", "after_revision", "scene_sequence"};
        static const char* booleans[] = {"audio", "visualize", "release", "all", "screenshot"};
        static const char* arrays[] = {"points", "durations", "modifiers", "ids"};
        for (uint32_t i = 0; i < fields->Size(); ++i)
        {
            const QueryParam& field = fields->Begin()[i];
            for (uint32_t j = 0; j < DM_ARRAY_SIZE(numbers); ++j)
                if (StringsEqual(field.m_Key, numbers[j]) && !(field.m_Value[0] == '-' || isdigit((unsigned char)field.m_Value[0]))) return false;
            for (uint32_t j = 0; j < DM_ARRAY_SIZE(booleans); ++j)
                if (StringsEqual(field.m_Key, booleans[j]) && !StringsEqual(field.m_Value, "true") && !StringsEqual(field.m_Value, "false")) return false;
            for (uint32_t j = 0; j < DM_ARRAY_SIZE(arrays); ++j)
                if (StringsEqual(field.m_Key, arrays[j]) && field.m_Value[0] != '[') return false;
        }
        return true;
    }

    bool JsonValidate(const char* json, uint32_t max_bytes)
    {
        if (!json || !*json || strlen(json) > max_bytes) return false;
        const char* cursor = json;
        if (!JsonSkipValue(&cursor, 0)) return false;
        JsonSkipWhitespace(&cursor);
        return *cursor == 0;
    }

    bool JsonParseRequest(const char* body, dmArray<QueryParam>* params, dmArray<QueryParam>* fields)
    {
        return JsonValidate(body, MAX_JSON_REQUEST_BYTES) &&
               JsonParseRootObject(body, params, fields) && ValidateJsonTypes(fields);
    }
}
