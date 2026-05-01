// Copyright 2020-2025 The Defold Foundation
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

#include "../shaderc.h"
#include "../shaderc_private.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/time.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct Params
{
    const char*               m_PathIn;
    const char*               m_PathOut;
    dmShaderc::ShaderLanguage m_Language;
    dmShaderc::ShaderStage    m_Stage;
    int                       m_Version;
    bool                      m_CrossCompile;
    bool                      m_Reflect;
};

Params GetDefaultParams()
{
    Params params = {};
    params.m_Language= dmShaderc::SHADER_LANGUAGE_GLSL;
    params.m_Version = 330;
    params.m_CrossCompile = true;
    params.m_Reflect = false;
    return params;
}

static void* ReadFile(const char* path, uint32_t* file_size)
{
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        printf("Failed to load %s\n", path);
        return 0;
    }

    fseek(file, 0, SEEK_END);
    size_t size = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    void* mem = malloc(size);

    size_t nread = fread(mem, 1, size, file);
    fclose(file);

    if (nread != size)
    {
        printf("Failed to read %u bytes from %s\n", (uint32_t)size, path);
        free(mem);
        return 0;
    }

    if (file_size)
    {
        *file_size = size;
    }

    return mem;
}

static int WriteFile(const char* path, const void* data, uint32_t data_size)
{
    FILE* file = fopen(path, "wb");
    if (!file)
    {
        printf("Failed to open %s for writing\n", path);
        return 0;
    }

    size_t nwritten = fwrite(data, 1, data_size, file);
    fclose(file);

    if (nwritten != data_size)
    {
        printf("Failed to write %u bytes to %s\n", data_size, path);
        return 0;
    }

    return 1;
}

static int ExecuteStandAlone(const Params& p)
{
    printf("Running with file %s\n", p.m_PathIn);

    uint32_t data_size;
    void* data = ReadFile(p.m_PathIn, &data_size);

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext(p.m_Stage, data, data_size);

    if (p.m_Reflect)
    {
        const dmShaderc::ShaderReflection* reflection = dmShaderc::GetReflection(shader_ctx);
        dmShaderc::DebugPrintReflection(reflection);
    }
    else if (p.m_CrossCompile)
    {
        dmShaderc::ShaderCompilerOptions options = {};
        options.m_Version                        = p.m_Version;
        options.m_EntryPoint                     = "main";

        dmShaderc::HShaderCompiler compiler = dmShaderc::NewShaderCompiler(shader_ctx, p.m_Language);
        dmShaderc::ShaderCompileResult* dst = dmShaderc::Compile(shader_ctx, compiler, options);

        if (p.m_PathOut)
        {
            WriteFile(p.m_PathOut, dst->m_Data.Begin(), dst->m_Data.Size());
        }
        else
        {
            dst->m_Data.Begin()[dst->m_Data.Size()-1] = 0;
            printf("%s\n", (char*) dst->m_Data.Begin());
        }

        dmShaderc::DeleteShaderCompiler(compiler);
    }

    dmShaderc::DeleteShaderContext(shader_ctx);

    return 0;
}

inline bool IsArg(const char* arg)
{
    size_t str_len = strlen(arg);
    return str_len > 2 && arg[0] == '-' && arg[1] == '-';
}

template <typename T>
struct ArgNameToTypeValue
{
    const char* m_Name;
    const T     m_TypeValue;
};

template <typename T>
T GetArgTypeValue(const char* arg, ArgNameToTypeValue<T>* args, T default_value)
{
    int i=0;
    while(args[i].m_Name)
    {
        if (dmStrCaseCmp(args[i].m_Name, arg) == 0)
        {
            return args[i].m_TypeValue;
        }
        i++;
    }

    return default_value;
}

ArgNameToTypeValue<dmShaderc::ShaderLanguage> g_lang_lut[] = {
    {"none", dmShaderc::SHADER_LANGUAGE_NONE},
    {"glsl", dmShaderc::SHADER_LANGUAGE_GLSL},
    {"hlsl", dmShaderc::SHADER_LANGUAGE_HLSL},
    {"spirv", dmShaderc::SHADER_LANGUAGE_SPIRV},
    {0}
};

ArgNameToTypeValue<dmShaderc::ShaderStage> g_stage_lut[] = {
    {"vert", dmShaderc::SHADER_STAGE_VERTEX},
    {"frag", dmShaderc::SHADER_STAGE_FRAGMENT},
    {"comp", dmShaderc::SHADER_STAGE_COMPUTE},
    {0}
};

static void GetParamsFromArgs(int argc, const char* argv[], Params& params)
{
    for (int i = 1; i < argc; ++i)
    {
        if (IsArg(argv[i]))
        {
            const char* actual_arg    = argv[i]+2;
            #define CMP_ARG(name)      (dmStrCaseCmp(actual_arg, name) == 0)
            #define CMP_ARG_1_OP(name) (CMP_ARG(name) && (i+1) < argc)

            printf("Argument[%d]: %s\n", i, actual_arg);

            if (CMP_ARG("language"))
                params.m_Language = GetArgTypeValue(argv[++i], g_lang_lut, params.m_Language);
            else if (CMP_ARG("version"))
                params.m_Version = atoi(argv[++i]);
            else if (CMP_ARG("stage"))
                params.m_Stage = GetArgTypeValue(argv[++i], g_stage_lut, params.m_Stage);
            else if (CMP_ARG("out"))
                params.m_PathOut = argv[++i];

            #undef CMP_ARG_1_OP
            #undef CMP_ARG
        }
        else if (params.m_PathIn == NULL)
        {
            params.m_PathIn = argv[i];
        }
    }
}

int main(int argc, char const *argv[])
{
    Params params = GetDefaultParams();
    GetParamsFromArgs(argc, argv, params);

    if (!params.m_PathIn)
    {
        printf("No input file found in arguments\n");
        return -1;
    }

    return ExecuteStandAlone(params);
}
