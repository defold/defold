// Copyright 2020-2024 The Defold Foundation
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

#include "glsl_uniform_parser.h"

#include <string.h>
#include <stdio.h> // scanf

namespace dmGraphics
{
    static bool IsWS(const char c)
    {
        return c == ' ' || c == '\n' || c == '\t' || c == '\r';
    }

    static const char* SkipLine(const char* cursor)
    {
        while (*cursor != '\n' && *cursor != '\0')
        {
            ++cursor;
        }
        return cursor;
    }

    static const char* SkipWS(const char* cursor)
    {
        while (IsWS(*cursor) && *cursor != '\0')
        {
            ++cursor;
        }
        return cursor;
    }

    static const char* FindWS(const char* cursor)
    {
        while (!IsWS(*cursor) && *cursor != '\0')
        {
            ++cursor;
        }
        return cursor;
    }

    static const char* FindChar(const char* cursor, char c)
    {
        while (*cursor != c && *cursor != '\0')
        {
            ++cursor;
        }
        if (*cursor == c)
            return cursor;
        return 0;
    }

#define STRNCMP(s1, s2, count)\
    strncmp(s1, s2, strnlen(s1, 64)) == 0

    static bool ParseType(const char* string, uint32_t count, Type* out_type)
    {
        if (STRNCMP("int", string, count))
        {
            *out_type = TYPE_INT;
            return true;
        }
        else if (STRNCMP("uint", string, count))
        {
            *out_type = TYPE_UNSIGNED_INT;
            return true;
        }
        else if (STRNCMP("float", string, count))
        {
            *out_type = TYPE_FLOAT;
            return true;
        }
        else if (STRNCMP("vec2", string, count))
        {
            *out_type = TYPE_FLOAT_VEC2;
            return true;
        }
        else if (STRNCMP("vec3", string, count))
        {
            *out_type = TYPE_FLOAT_VEC3;
            return true;
        }
        else if (STRNCMP("vec4", string, count))
        {
            *out_type = TYPE_FLOAT_VEC4;
            return true;
        }
        else if (STRNCMP("mat2", string, count))
        {
            *out_type = TYPE_FLOAT_MAT2;
            return true;
        }
        else if (STRNCMP("mat3", string, count))
        {
            *out_type = TYPE_FLOAT_MAT3;
            return true;
        }
        else if (STRNCMP("mat4", string, count))
        {
            *out_type = TYPE_FLOAT_MAT4;
            return true;
        }
        else if (STRNCMP("sampler2D", string, count))
        {
            *out_type = TYPE_SAMPLER_2D;
            return true;
        }
        else if (STRNCMP("samplerCube", string, count))
        {
            *out_type = TYPE_SAMPLER_CUBE;
            return true;
        }
        else if (STRNCMP("sampler2DArray", string, count))
        {
            *out_type = TYPE_SAMPLER_2D_ARRAY;
            return true;
        }
        return false;
    }

    static void NextWord(const char** word_start, const char** word_end, uint32_t* size)
    {
        *word_start = SkipWS(*word_end);
        *word_end = FindWS(*word_start);
        *size = (uint32_t) (*word_end - *word_start);
    }

    static bool CheckUniformSize(const char* start, uint32_t* array_size)
    {
        start = SkipWS(start);
        const char* begin = FindChar(start, '[');
        if (!begin)
            return false;

        uint32_t out = 1;
        int num_scanned = sscanf(begin, "[%u]", &out);
        if (num_scanned == 1)
        {
            *array_size = out;
            return true;
        }
        return false;
    }

    static const char* GetKeyword(ShaderDesc::Language language, GLSLUniformParserBindingType binding_type)
    {
        if (binding_type == GLSLUniformParserBindingType::UNIFORM)
        {
            return "uniform";
        }
        else if (binding_type == GLSLUniformParserBindingType::ATTRIBUTE)
        {
            if (language == ShaderDesc::LANGUAGE_GLSL_SM140 || language == ShaderDesc::LANGUAGE_GLES_SM300)
            {
                return "in";
            }
            return "attribute";
        }
        assert(0);
        return 0;
    }

    static bool ShaderBindingParse(ShaderDesc::Language language, GLSLUniformParserBindingType binding_type, const char* buffer, ShaderBindingCallback cb, uintptr_t userdata)
    {
        if (buffer == 0x0)
        {
            return true;
        }

        const char* keyword    = GetKeyword(language, binding_type);
        const char* line_end   = buffer;
        const char* line_start = buffer;
        const char* word_start = 0;
        const char* word_end   = 0;
        const char* buffer_end = buffer + strlen(buffer);

        uint32_t size = 0;
        while (line_end < buffer_end)
        {
            line_end   = SkipLine(line_start) + 1;
            word_start = line_start;
            word_end   = line_start;

        #if 0
            char line_buf[256];
            size_t line_len = line_end - line_start;
            memcpy(line_buf, line_start, line_len);
            line_buf[line_len] = '\0';
            printf("LINE: %s", line_buf);
        #endif

            while(word_start < line_end)
            {
                NextWord(&word_start, &word_end, &size);

                if (size == 0 || word_start >= line_end)
                {
                    break;
                }

                if (STRNCMP(keyword, word_start, size))
                {
                    Type type;

                    NextWord(&word_start, &word_end, &size);

                    bool found_type = ParseType(word_start, size, &type);
                    // Assume precision, look for type again
                    if (!found_type)
                    {
                        NextWord(&word_start, &word_end, &size);
                        found_type = ParseType(word_start, size, &type);
                    }

                    if (found_type)
                    {
                        // Check name
                        NextWord(&word_start, &word_end, &size);

                        // Check if it's an array
                        uint32_t uniform_size = 1;
                        if (CheckUniformSize(word_start, &uniform_size))
                        {
                            const char* array_begin = FindChar(word_start, '[');
                            size = array_begin - word_start + 1;
                        }

                        cb(binding_type, word_start, size-1, type, uniform_size, userdata);
                        break;
                    }
                    else
                    {
                        return false;
                    }
                }
            }

            line_start = line_end;
        }
        return true;
    }

    bool GLSLAttributeParse(ShaderDesc::Language language, const char* buffer, ShaderBindingCallback cb, uintptr_t userdata)
    {
        return ShaderBindingParse(language, GLSLUniformParserBindingType::ATTRIBUTE, buffer, cb, userdata);
    }

    bool GLSLUniformParse(ShaderDesc::Language language, const char* buffer, ShaderBindingCallback cb, uintptr_t userdata)
    {
        return ShaderBindingParse(language, GLSLUniformParserBindingType::UNIFORM, buffer, cb, userdata);
    }

#undef STRNCMP

}
