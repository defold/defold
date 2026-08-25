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

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <wrl/client.h>

#include <d3d12shader.h>
#include <d3d12.h>
#include <d3dcompiler.h>

#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/dstrings.h>
#include <dmsdk/dlib/sys.h>

#include "shaderc_private.h"

#include <stdio.h>

//#define DM_SHADERC_TRACE
#if defined(DM_SHADERC_TRACE)
    #define DM_TRACE_LINE()         printf("%s:%d\n", __FUNCTION__, __LINE__)
    #define DM_TRACE_MSG(_MESSAGE)  printf("%s:%d: %s\n", __FUNCTION__, __LINE__, (_MESSAGE))
#else
    #define DM_TRACE_LINE()
    #define DM_TRACE_MSG(_MESSAGE)
#endif

namespace dmShaderc
{
    static bool IsNullOrEmpty(const char* str)
    {
        return str == 0 || str[0] == '\0';
    }

    static bool WriteBytesToFile(const char* path, const void* data, uint32_t data_size)
    {
        FILE* file = fopen(path, "wb");
        if (!file)
            return false;

        size_t written = fwrite(data, 1, data_size, file);
        fclose(file);
        return written == data_size;
    }

    static bool GetFileSize(const char* path, uint32_t* out_file_size);

    static bool ReadBytesFromFile(const char* path, dmArray<uint8_t>& out)
    {
        uint32_t file_size = 0;
        if (!GetFileSize(path, &file_size))
        {
            return false;
        }

        FILE* file = fopen(path, "rb");
        if (!file)
            return false;

        out.SetCapacity(file_size);
        out.SetSize(file_size);

        size_t read_size = fread(out.Begin(), 1, file_size, file);
        fclose(file);
        return read_size == file_size;
    }

    static bool GetFileSize(const char* path, uint32_t* out_file_size)
    {
        dmSys::StatInfo stat_info;
        if (dmSys::Stat(path, &stat_info) != dmSys::RESULT_OK || !dmSys::StatIsFile(&stat_info) || stat_info.m_Size > UINT32_MAX)
        {
            return false;
        }

        *out_file_size = (uint32_t) stat_info.m_Size;
        return true;
    }

    static bool CreateTempFilePath(const char* prefix, char* out_path, uint32_t out_path_size)
    {
        if (out_path_size == 0)
            return false;

        char tmp_name[L_tmpnam];
        if (tmpnam_s(tmp_name, sizeof(tmp_name)) != 0)
        {
            return false;
        }

        if (prefix && prefix[0] != '\0')
        {
            int written = dmSnPrintf(out_path, out_path_size, "%s_%s", tmp_name, prefix);
            return written > 0 && (uint32_t) written < out_path_size;
        }

        return dmStrlCpy(out_path, tmp_name, out_path_size) < out_path_size;
    }

    // Replaces every occurrence of token in the null-terminated buffer while keeping
    // the final result within text_size, returning false if the replacement would overflow.
    static bool ReplaceAllInPlace(char* text, uint32_t text_size, const char* token, const char* value)
    {
        if (!text || !token || !value || text_size == 0)
            return false;

        size_t token_len = strlen(token);
        if (token_len == 0)
            return true;

        char* out = (char*) malloc(text_size);
        if (!out)
            return false;

        const char* read_ptr = text;
        uint32_t out_pos = 0;

        while (1)
        {
            const char* match = strstr(read_ptr, token);
            if (!match)
                break;

            uint32_t prefix_len = (uint32_t) (match - read_ptr);
            uint32_t value_len = (uint32_t) strlen(value);

            if (out_pos + prefix_len + value_len + 1 > text_size)
            {
                free(out);
                return false;
            }

            memcpy(out + out_pos, read_ptr, prefix_len);
            out_pos += prefix_len;
            memcpy(out + out_pos, value, value_len);
            out_pos += value_len;

            read_ptr = match + token_len;
        }

        uint32_t tail_len = (uint32_t) strlen(read_ptr);
        if (out_pos + tail_len + 1 > text_size)
        {
            free(out);
            return false;
        }

        memcpy(out + out_pos, read_ptr, tail_len);
        out_pos += tail_len;
        out[out_pos] = '\0';

        dmStrlCpy(text, out, text_size);
        free(out);
        return true;
    }

    static const char* GetStageShortName(ShaderStage stage);

    static bool BuildExternalCompilerCommand(const ShaderCompilerOptions* options, const char* input_path, const char* output_path, const char* profile, ShaderStage stage, int version, char* out_command, uint32_t out_command_size)
    {
        char args[32768];
        if (dmStrlCpy(args, options->m_ExternalCompilerArgs, sizeof(args)) >= sizeof(args))
        {
            dmLogError("External HLSL compiler args are too long");
            return false;
        }

        int version_major = version / 10;
        int version_minor = version % 10;
        char version_str[16] = {0};
        char version_major_str[16] = {0};
        char version_minor_str[16] = {0};

        dmSnPrintf(version_str, sizeof(version_str), "%d", version);
        dmSnPrintf(version_major_str, sizeof(version_major_str), "%d", version_major);
        dmSnPrintf(version_minor_str, sizeof(version_minor_str), "%d", version_minor);

        if (!ReplaceAllInPlace(args, sizeof(args), "{input}", input_path) ||
            !ReplaceAllInPlace(args, sizeof(args), "{output}", output_path) ||
            !ReplaceAllInPlace(args, sizeof(args), "{entry}", options->m_EntryPoint ? options->m_EntryPoint : "main") ||
            !ReplaceAllInPlace(args, sizeof(args), "{profile}", profile) ||
            !ReplaceAllInPlace(args, sizeof(args), "{stage}", GetStageShortName(stage)) ||
            !ReplaceAllInPlace(args, sizeof(args), "{version}", version_str) ||
            !ReplaceAllInPlace(args, sizeof(args), "{version_major}", version_major_str) ||
            !ReplaceAllInPlace(args, sizeof(args), "{version_minor}", version_minor_str))
        {
            dmLogError("External HLSL compiler args overflow while replacing tokens");
            return false;
        }

        int written = 0;
        if (args[0] != '\0')
        {
            // Use one cmd layer via system(); "call" handles quoted executable paths with spaces.
            written = dmSnPrintf(out_command, out_command_size, "call \"%s\" %s", options->m_ExternalCompilerPath, args);
        }
        else
        {
            written = dmSnPrintf(out_command, out_command_size, "call \"%s\"", options->m_ExternalCompilerPath);
        }
        return written > 0 && (uint32_t) written < out_command_size;
    }

    static void BuildShaderProfile(ShaderStage stage, int version, char profile[32])
    {
        int version_major = version / 10;
        int version_minor = version % 10;

        switch(stage)
        {
        case SHADER_STAGE_VERTEX:
            dmSnPrintf(profile, sizeof(profile), "vs_%d_%d", version_major, version_minor);
            break;
        case SHADER_STAGE_FRAGMENT:
            dmSnPrintf(profile, sizeof(profile), "ps_%d_%d", version_major, version_minor);
            break;
        case SHADER_STAGE_COMPUTE:
            dmSnPrintf(profile, sizeof(profile), "cs_%d_%d", version_major, version_minor);
            break;
        }
    }

    static const char* GetStageShortName(ShaderStage stage)
    {
        switch (stage)
        {
        case SHADER_STAGE_VERTEX:   return "vs";
        case SHADER_STAGE_FRAGMENT: return "ps";
        case SHADER_STAGE_COMPUTE:  return "cs";
        default:                    return "";
        }
    }

    static const dmhash_t HASH_SPIRV_CROSS_NUM_WORKGROUPS = dmHashString64("SPIRV_Cross_NumWorkgroups");

    static bool ExtractBaseSamplerName(const char* combined_name, char* base_texture_name_buffer, uint32_t base_texture_name_buffer_len)
    {
        DM_TRACE_LINE();
        const char* suffix = "_sampler";
        size_t len = strlen(combined_name);
        size_t suffix_len = strlen(suffix);

        // Must start with '_' and end with "_sampler"
        if (len <= suffix_len + 1 || combined_name[0] != '_')
            return false;

        if (strcmp(combined_name + len - suffix_len, suffix) != 0)
            return false;

        size_t base_len = len - suffix_len - 1;

        if (base_texture_name_buffer_len < base_len + 1)
            return false;

        memcpy(base_texture_name_buffer, combined_name + 1, base_len);
        base_texture_name_buffer[base_len] = '\0';

        return true;
    }

    static const char* FindCombinedSampler(dmArray<CombinedSampler>& combined_samplers, const char* name, D3D_SHADER_INPUT_TYPE input_type)
    {
        DM_TRACE_LINE();
        const char* to_test = name;

        // Strip all leading '_'
        while(to_test[0] == '_')
        {
            to_test++;
        }

        char* end_ptr = 0;
        unsigned long id = strtoul(to_test, &end_ptr, 10);
        if (end_ptr == to_test)
        {
            return 0;
        }

        for (int i = 0; i < combined_samplers.Size(); ++i)
        {
            if (id == combined_samplers[i].m_CombinedId)
            {
                if (input_type == D3D_SIT_SAMPLER)
                {
                    return combined_samplers[i].m_SamplerName;
                }
                else if (input_type == D3D_SIT_TEXTURE)
                {
                    return combined_samplers[i].m_ImageName;
                }
            }
        }
        return 0;
    }

    static void FillResourceEntryArray(HShaderContext context, ID3D12ShaderReflection* hlsl_reflection, D3D12_SHADER_DESC* shaderDesc, dmArray<CombinedSampler>& combined_samplers, dmArray<HLSLResourceMapping>& resource_entries)
    {
        DM_TRACE_LINE();
        char base_texture_name_buffer[1024];

        for (uint32_t i = 0; i < shaderDesc->BoundResources; ++i)
        {
            D3D12_SHADER_INPUT_BIND_DESC bindDesc;
            hlsl_reflection->GetResourceBindingDesc(i, &bindDesc);

            dmhash_t resource_name_hash = dmHashString64(bindDesc.Name);
            memset(&resource_entries[i], 0, sizeof(HLSLResourceMapping));
            resource_entries[i].m_Name     = bindDesc.Name;
            resource_entries[i].m_NameHash = resource_name_hash;

            // 1. try to find the resource by name hash
            const ShaderResource* resource = FindShaderResourceUniform(context, resource_name_hash);

            // 2. For samplers and textures, we try to look into the list of "combined" texture samplers.
            //    The combined texture samplers is basically an unwrap of a Sampler2D object into a sampler and a texture object.
            //    We use this information to get the original unwrapped name + resource from the reflection.
            if (!resource)
            {
                if (bindDesc.Type == D3D_SIT_SAMPLER || bindDesc.Type == D3D_SIT_TEXTURE)
                {
                    const char* combined_sampler_name = FindCombinedSampler(combined_samplers, bindDesc.Name, bindDesc.Type);
                    if (combined_sampler_name)
                    {
                        resource = FindShaderResourceUniform(context, dmHashString64(combined_sampler_name));
                    }
                }

                // 2.1 Separated samplers may not be found in the combined samplers array, nor in the general reflection data
                //     So we need to extract the generated base name and check for that instead.
                if (!resource && bindDesc.Type == D3D_SIT_SAMPLER)
                {
                    if (ExtractBaseSamplerName(bindDesc.Name, base_texture_name_buffer, sizeof(base_texture_name_buffer)))
                    {
                        resource = FindShaderResourceUniform(context, dmHashString64(base_texture_name_buffer));
                    }
                }
            }

            if (resource)
            {
                resource_entries[i].m_ShaderResourceSet     = resource->m_Set;
                resource_entries[i].m_ShaderResourceBinding = resource->m_Binding;
            }
            // 3. For compute shaders, we need to deal with this resource separately, since the gl_NumWorkgroups built-in doesn't
            //    exist in HLSL. It will be converted into a cbuffer in hlsl, so we need to keep track of that separately.
            else if (resource_name_hash == HASH_SPIRV_CROSS_NUM_WORKGROUPS)
            {
                resource_entries[i].m_ShaderResourceSet     = HLSL_NUM_WORKGROUPS_SET; // note: we set the explicit set decoration in shaderc_spvc.cpp
                resource_entries[i].m_ShaderResourceBinding = bindDesc.BindPoint;
            }
        }
    }

    static void PrintRootSignatureFromReflection(ID3D12ShaderReflection* reflection, D3D12_SHADER_DESC* shaderDesc)
    {
        DM_TRACE_LINE();
        dmLogInfo("Shader has %u bound resources:", shaderDesc->BoundResources);

        for (uint32_t i = 0; i < shaderDesc->BoundResources; ++i)
        {
            D3D12_SHADER_INPUT_BIND_DESC bindDesc;
            reflection->GetResourceBindingDesc(i, &bindDesc);

            const char* typeStr = "";
            switch (bindDesc.Type)
            {
                case D3D_SIT_CBUFFER:          typeStr = "CBV"; break;
                case D3D_SIT_TBUFFER:          typeStr = "TBUFFER"; break;
                case D3D_SIT_TEXTURE:          typeStr = "SRV (Texture)"; break;
                case D3D_SIT_SAMPLER:          typeStr = "Sampler"; break;
                case D3D_SIT_STRUCTURED:       typeStr = "SRV (StructuredBuffer)"; break;
                case D3D_SIT_UAV_RWTYPED:      typeStr = "UAV"; break;
                case D3D_SIT_UAV_RWSTRUCTURED: typeStr = "UAV (RWStructuredBuffer)"; break;
                default:                       typeStr = "UNDEFINED"; break;
            }

            dmLogInfo("  [%u] Name: %-30s Type: %-25s BindPoint: %u  BindCount: %u",
                i,
                bindDesc.Name,
                typeStr,
                bindDesc.BindPoint,
                bindDesc.BindCount);
        }

        dmLogInfo("Suggested Root Signature:");
        for (uint32_t i = 0; i < shaderDesc->BoundResources; ++i)
        {
            D3D12_SHADER_INPUT_BIND_DESC bindDesc;
            reflection->GetResourceBindingDesc(i, &bindDesc);

            switch (bindDesc.Type)
            {
            case D3D_SIT_CBUFFER:
                dmLogInfo("  RootParameter[%u] = CBV(slot = %u)", i, bindDesc.BindPoint);
                break;
            case D3D_SIT_TEXTURE:
            case D3D_SIT_STRUCTURED:
                dmLogInfo("  RootParameter[%u] = SRV(slot = %u)", i, bindDesc.BindPoint);
                break;
            case D3D_SIT_SAMPLER:
                dmLogInfo("  StaticSampler[%u] = Sampler(slot = %u)", i, bindDesc.BindPoint);
                break;
            case D3D_SIT_UAV_RWTYPED:
            case D3D_SIT_UAV_RWSTRUCTURED:
                dmLogInfo("  RootParameter[%u] = UAV(slot = %u)", i, bindDesc.BindPoint);
                break;
            default:
                dmLogInfo("  RootParameter[%u] = UnknownType(slot = %u)", i, bindDesc.BindPoint);
                break;
            }
        }
    }

    static const char* GetRootSignatureVisibility(ShaderStage stage)
    {
        switch(stage)
        {
            case SHADER_STAGE_VERTEX:   return "SHADER_VISIBILITY_VERTEX";
            case SHADER_STAGE_FRAGMENT: return "SHADER_VISIBILITY_PIXEL";
            case SHADER_STAGE_COMPUTE:  return "SHADER_VISIBILITY_ALL";
            default:                    return "SHADER_VISIBILITY_ALL";
        }
    }

    static const char* GetRootSignatureFlags(ShaderStage stage)
    {
        switch (stage)
        {
            case SHADER_STAGE_VERTEX:
            case SHADER_STAGE_FRAGMENT:
                return "ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT|DENY_HULL_SHADER_ROOT_ACCESS|DENY_DOMAIN_SHADER_ROOT_ACCESS|DENY_GEOMETRY_SHADER_ROOT_ACCESS";
            case SHADER_STAGE_COMPUTE:
                return "DENY_VERTEX_SHADER_ROOT_ACCESS|DENY_HULL_SHADER_ROOT_ACCESS|DENY_DOMAIN_SHADER_ROOT_ACCESS|DENY_GEOMETRY_SHADER_ROOT_ACCESS|DENY_PIXEL_SHADER_ROOT_ACCESS";
            default:
                return "";
        }
    }

    static bool AppendRootSignatureText(char* buffer, uint32_t buffer_size, uint32_t* inout_pos, const char* fmt, ...)
    {
        if (*inout_pos >= buffer_size)
            return false;

        va_list args;
        va_start(args, fmt);
        int written = vsnprintf(buffer + *inout_pos, buffer_size - *inout_pos, fmt, args);
        va_end(args);
        if (written <= 0)
            return false;

        uint32_t next_pos = *inout_pos + (uint32_t) written;
        if (next_pos >= buffer_size)
            return false;

        *inout_pos = next_pos;
        return true;
    }

    static const char* RootVisibilityToString(D3D12_SHADER_VISIBILITY visibility)
    {
        switch (visibility)
        {
            case D3D12_SHADER_VISIBILITY_ALL:    return "SHADER_VISIBILITY_ALL";
            case D3D12_SHADER_VISIBILITY_VERTEX: return "SHADER_VISIBILITY_VERTEX";
            case D3D12_SHADER_VISIBILITY_HULL:   return "SHADER_VISIBILITY_HULL";
            case D3D12_SHADER_VISIBILITY_DOMAIN: return "SHADER_VISIBILITY_DOMAIN";
            case D3D12_SHADER_VISIBILITY_GEOMETRY:return "SHADER_VISIBILITY_GEOMETRY";
            case D3D12_SHADER_VISIBILITY_PIXEL:  return "SHADER_VISIBILITY_PIXEL";
            default:                              return "SHADER_VISIBILITY_ALL";
        }
    }

    static const char* DescriptorRangeToString(const D3D12_DESCRIPTOR_RANGE& range, char* tmp, uint32_t tmp_len)
    {
        switch (range.RangeType)
        {
            case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
                dmSnPrintf(tmp, tmp_len, "CBV(b%u,space=%u)", (uint32_t) range.BaseShaderRegister, (uint32_t) range.RegisterSpace);
                return tmp;
            case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
                dmSnPrintf(tmp, tmp_len, "SRV(t%u,space=%u)", (uint32_t) range.BaseShaderRegister, (uint32_t) range.RegisterSpace);
                return tmp;
            case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
                dmSnPrintf(tmp, tmp_len, "UAV(u%u,space=%u)", (uint32_t) range.BaseShaderRegister, (uint32_t) range.RegisterSpace);
                return tmp;
            case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
                dmSnPrintf(tmp, tmp_len, "Sampler(s%u,space=%u)", (uint32_t) range.BaseShaderRegister, (uint32_t) range.RegisterSpace);
                return tmp;
            default:
                dmSnPrintf(tmp, tmp_len, "SRV(t%u,space=%u)", (uint32_t) range.BaseShaderRegister, (uint32_t) range.RegisterSpace);
                return tmp;
        }
    }

    bool RootSignatureBlobToText(const void* blob_data, uint32_t blob_size, dmArray<char>& out_text)
    {
        if (!blob_data || blob_size == 0)
            return false;

        ID3D12RootSignatureDeserializer* deserializer = 0;
        HRESULT hr = D3D12CreateRootSignatureDeserializer(blob_data, blob_size, IID_PPV_ARGS(&deserializer));
        if (FAILED(hr) || !deserializer)
        {
            return false;
        }

        const D3D12_ROOT_SIGNATURE_DESC* desc = deserializer->GetRootSignatureDesc();

        const uint32_t MAX_ROOTSIG_TEXT = 16384;
        out_text.SetCapacity(MAX_ROOTSIG_TEXT);
        out_text.SetSize(MAX_ROOTSIG_TEXT);
        memset(out_text.Begin(), 0, MAX_ROOTSIG_TEXT);

        uint32_t pos = 0;
        bool ok = AppendRootSignatureText(out_text.Begin(), MAX_ROOTSIG_TEXT, &pos, "[RootSignature(\"");

        for (uint32_t i = 0; ok && i < desc->NumParameters; ++i)
        {
            const D3D12_ROOT_PARAMETER& param = desc->pParameters[i];
            if (i > 0)
                ok = AppendRootSignatureText(out_text.Begin(), MAX_ROOTSIG_TEXT, &pos, ",");

            const char* visibility = RootVisibilityToString(param.ShaderVisibility);
            if (!ok)
                break;

            switch (param.ParameterType)
            {
                case D3D12_ROOT_PARAMETER_TYPE_CBV:
                    ok = AppendRootSignatureText(out_text.Begin(), MAX_ROOTSIG_TEXT, &pos, "CBV(b%u,space=%u,visibility=%s)",
                        (uint32_t) param.Descriptor.ShaderRegister,
                        (uint32_t) param.Descriptor.RegisterSpace,
                        visibility);
                    break;
                case D3D12_ROOT_PARAMETER_TYPE_SRV:
                    ok = AppendRootSignatureText(out_text.Begin(), MAX_ROOTSIG_TEXT, &pos, "SRV(t%u,space=%u,visibility=%s)",
                        (uint32_t) param.Descriptor.ShaderRegister,
                        (uint32_t) param.Descriptor.RegisterSpace,
                        visibility);
                    break;
                case D3D12_ROOT_PARAMETER_TYPE_UAV:
                    ok = AppendRootSignatureText(out_text.Begin(), MAX_ROOTSIG_TEXT, &pos, "UAV(u%u,space=%u,visibility=%s)",
                        (uint32_t) param.Descriptor.ShaderRegister,
                        (uint32_t) param.Descriptor.RegisterSpace,
                        visibility);
                    break;
                case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
                {
                    ok = AppendRootSignatureText(out_text.Begin(), MAX_ROOTSIG_TEXT, &pos, "DescriptorTable(");
                    for (uint32_t r = 0; ok && r < param.DescriptorTable.NumDescriptorRanges; ++r)
                    {
                        if (r > 0)
                            ok = AppendRootSignatureText(out_text.Begin(), MAX_ROOTSIG_TEXT, &pos, ",");

                        if (!ok)
                            break;

                        char range_tmp[128];
                        const char* range_text = DescriptorRangeToString(param.DescriptorTable.pDescriptorRanges[r], range_tmp, sizeof(range_tmp));
                        ok = AppendRootSignatureText(out_text.Begin(), MAX_ROOTSIG_TEXT, &pos, "%s", range_text);
                    }
                    if (ok)
                    {
                        ok = AppendRootSignatureText(out_text.Begin(), MAX_ROOTSIG_TEXT, &pos, ",visibility=%s)", visibility);
                    }
                    break;
                }
                default:
                    ok = false;
                    break;
            }
        }

        if (ok && desc->Flags != D3D12_ROOT_SIGNATURE_FLAG_NONE)
        {
            struct FlagEntry
            {
                D3D12_ROOT_SIGNATURE_FLAGS m_Flag;
                const char*                m_Name;
            };

            static const FlagEntry kFlagEntries[] = {
                { D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, "ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT" },
                { D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS, "DENY_VERTEX_SHADER_ROOT_ACCESS" },
                { D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS, "DENY_HULL_SHADER_ROOT_ACCESS" },
                { D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS, "DENY_DOMAIN_SHADER_ROOT_ACCESS" },
                { D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS, "DENY_GEOMETRY_SHADER_ROOT_ACCESS" },
                { D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS, "DENY_PIXEL_SHADER_ROOT_ACCESS" },
                { D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT, "ALLOW_STREAM_OUTPUT" },
            };

            D3D12_ROOT_SIGNATURE_FLAGS known_flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
            for (uint32_t i = 0; i < (uint32_t) DM_ARRAY_SIZE(kFlagEntries); ++i)
            {
                known_flags = (D3D12_ROOT_SIGNATURE_FLAGS) (known_flags | kFlagEntries[i].m_Flag);
            }

            D3D12_ROOT_SIGNATURE_FLAGS flags_to_write = (D3D12_ROOT_SIGNATURE_FLAGS) (desc->Flags & known_flags);
            if (flags_to_write != D3D12_ROOT_SIGNATURE_FLAG_NONE)
            {
                if (desc->NumParameters > 0)
                    ok = AppendRootSignatureText(out_text.Begin(), MAX_ROOTSIG_TEXT, &pos, ",");

                if (ok)
                    ok = AppendRootSignatureText(out_text.Begin(), MAX_ROOTSIG_TEXT, &pos, "RootFlags(");
            }

            bool wrote_flag = false;
            for (uint32_t i = 0; ok && i < (uint32_t) DM_ARRAY_SIZE(kFlagEntries); ++i)
            {
                if ((flags_to_write & kFlagEntries[i].m_Flag) == 0)
                    continue;

                if (wrote_flag)
                    ok = AppendRootSignatureText(out_text.Begin(), MAX_ROOTSIG_TEXT, &pos, "|");

                if (ok)
                    ok = AppendRootSignatureText(out_text.Begin(), MAX_ROOTSIG_TEXT, &pos, "%s", kFlagEntries[i].m_Name);

                wrote_flag = true;
            }

            if (ok && flags_to_write != D3D12_ROOT_SIGNATURE_FLAG_NONE)
                ok = AppendRootSignatureText(out_text.Begin(), MAX_ROOTSIG_TEXT, &pos, ")");
        }

        if (ok)
        {
            ok = AppendRootSignatureText(out_text.Begin(), MAX_ROOTSIG_TEXT, &pos, "\")]");
        }

        if (ok)
        {
            out_text.SetSize(pos + 1);
        }
        else
        {
            out_text.SetSize(0);
        }

        deserializer->Release();
        return ok;
    }

    static void GenerateRootSignatureFromReflection(ID3D12ShaderReflection* reflection, const D3D12_SHADER_DESC* shader_desc, ShaderStage stage, dmArray<char>& buffer)
    {
        DM_TRACE_LINE();
        const uint32_t BUFFER_SIZE = 1024 * 64;
        const char* visibility = GetRootSignatureVisibility(stage);
        const char* root_signature_flags = GetRootSignatureFlags(stage);
        const bool has_root_signature_flags = root_signature_flags[0] != '\0';

        buffer.SetCapacity(BUFFER_SIZE);
        buffer.SetSize(BUFFER_SIZE);

        char* out = buffer.Begin();
        memset(out, 0, BUFFER_SIZE);
        uint32_t pos = 0;
        bool ok = AppendRootSignatureText(out, BUFFER_SIZE, &pos, "[RootSignature(\"");

        for (uint32_t i = 0; ok && i < shader_desc->BoundResources; ++i)
        {
            D3D12_SHADER_INPUT_BIND_DESC bind_desc;
            reflection->GetResourceBindingDesc(i, &bind_desc);

            if (i > 0)
            {
                ok = AppendRootSignatureText(out, BUFFER_SIZE, &pos, ",");
            }

            switch (bind_desc.Type)
            {
            case D3D_SIT_CBUFFER:
                ok = AppendRootSignatureText(out, BUFFER_SIZE, &pos, "CBV(b%u,space=%u,visibility=%s)", bind_desc.BindPoint, bind_desc.Space, visibility);
                break;
            case D3D_SIT_TEXTURE:
                ok = AppendRootSignatureText(out, BUFFER_SIZE, &pos, "DescriptorTable(SRV(t%u,space=%u),visibility=%s)", bind_desc.BindPoint, bind_desc.Space, visibility);
                break;
            case D3D_SIT_SAMPLER:
                ok = AppendRootSignatureText(out, BUFFER_SIZE, &pos, "DescriptorTable(Sampler(s%u,space=%u),visibility=%s)", bind_desc.BindPoint, bind_desc.Space, visibility);
                break;
            case D3D_SIT_UAV_RWTYPED:
                ok = AppendRootSignatureText(out, BUFFER_SIZE, &pos, "DescriptorTable(UAV(u%u,space=%u),visibility=%s)", bind_desc.BindPoint, bind_desc.Space, visibility);
                break;
            default:
                break;
            }
        }

        // Scope the root signature to the shader stages in the pipeline to reduce command processor work.
        if (ok && has_root_signature_flags)
        {
            if (shader_desc->BoundResources > 0)
            {
                ok = AppendRootSignatureText(out, BUFFER_SIZE, &pos, ",");
            }
            if (ok)
            {
                ok = AppendRootSignatureText(out, BUFFER_SIZE, &pos, "RootFlags(%s)", root_signature_flags);
            }
        }

        if (ok)
        {
            ok = AppendRootSignatureText(out, BUFFER_SIZE, &pos, "\")]");
        }

        if (!ok)
        {
            char fallback[512];
            if (has_root_signature_flags)
            {
                dmSnPrintf(fallback, sizeof(fallback), "[RootSignature(\"RootFlags(%s)\")]", root_signature_flags);
            }
            else
            {
                dmStrlCpy(fallback, "[RootSignature(\"\")]", sizeof(fallback));
            }
            uint32_t fallback_len = (uint32_t) strlen(fallback) + 1;
            dmLogError("%s: Root signature text exceeded output buffer. Falling back to minimal root signature text.", __FUNCTION__);
            buffer.SetCapacity(fallback_len);
            buffer.SetSize(fallback_len);
            memcpy(buffer.Begin(), fallback, fallback_len);
            return;
        }

        buffer.SetSize(pos + 1);
    }

    static bool InjectRootSignatureIntoSource(const char* source, const char* root_signature, dmArray<char>& injected_buffer)
    {
        DM_TRACE_LINE();
        const char* insert_pos = NULL;
        const char* markers[] = {
            "SPIRV_Cross_Output main(", // VS/FS
            "void main("                // Compute
        };

        for (int i = 0; i < DM_ARRAY_SIZE(markers) && !insert_pos; ++i)
        {
            insert_pos = strstr(source, markers[i]);
        }

        if (!insert_pos)
        {
            return false;
        }

        size_t prefix_len   = insert_pos - source;
        size_t root_sig_len = strlen(root_signature);
        size_t source_len   = strlen(source);

        // estimate: prefix + root + newline + suffix
        size_t total_len = prefix_len + root_sig_len + 1 + (source_len - prefix_len);

        injected_buffer.SetCapacity(total_len);

        char* result = (char*)injected_buffer.Begin();

        // Write prefix
        memcpy(result, source, prefix_len);

        // Insert root signature
        int offset = (int)prefix_len;
        offset += dmSnPrintf(result + offset, total_len - offset, "%s\n", root_signature);

        // Copy the rest of the source
        size_t rest_len = source_len - prefix_len;
        memcpy(result + offset, source + prefix_len, rest_len);
        offset += rest_len;

        injected_buffer.SetCapacity(offset);
        injected_buffer.SetSize(offset);

        return true;
    }

    static bool StripExistingRootSignatureAttributes(const char* source, dmArray<char>& stripped_source, bool* out_stripped_any)
    {
        const char* needle = "[RootSignature(";
        const uint32_t source_len = (uint32_t) strlen(source);

        stripped_source.SetCapacity(source_len + 1);
        stripped_source.SetSize(source_len + 1);

        char* out = stripped_source.Begin();
        uint32_t out_pos = 0;
        const char* cursor = source;
        bool stripped_any = false;

        while (true)
        {
            const char* attr_begin = strstr(cursor, needle);
            if (!attr_begin)
            {
                break;
            }

            const char* attr_end = strstr(attr_begin, ")]");
            if (!attr_end)
            {
                // Malformed attribute; keep source unchanged from this point.
                break;
            }

            uint32_t prefix_len = (uint32_t) (attr_begin - cursor);
            if (out_pos + prefix_len + 1 > source_len + 1)
            {
                return false;
            }

            memcpy(out + out_pos, cursor, prefix_len);
            out_pos += prefix_len;

            cursor = attr_end + 2;
            if (*cursor == '\r')
                ++cursor;
            if (*cursor == '\n')
                ++cursor;

            stripped_any = true;
        }

        uint32_t tail_len = (uint32_t) strlen(cursor);
        if (out_pos + tail_len + 1 > source_len + 1)
        {
            return false;
        }

        memcpy(out + out_pos, cursor, tail_len);
        out_pos += tail_len;
        out[out_pos] = '\0';
        stripped_source.SetSize(out_pos + 1);

        if (out_stripped_any)
        {
            *out_stripped_any = stripped_any;
        }
        return true;
    }

    static uint32_t GetD3DCompileFlags(const ShaderCompilerOptions* options)
    {
        uint32_t compile_flags = D3DCOMPILE_ENABLE_STRICTNESS;
        // SPIRV-Cross can generate harmless flow-control warnings for shader model 5.x HLSL.
        // Keep strict validation, but do not reject generated HLSL solely because D3DCompile
        // decided to unroll a statically small loop.
        if (!options || options->m_Version < 50)
        {
            compile_flags |= D3DCOMPILE_WARNINGS_ARE_ERRORS;
        }
        return compile_flags;
    }

    static bool CompileShaderD3DCompiler(const ShaderCompilerOptions* options, const void* source, uint32_t source_size, const char* entry_point, const char* profile, ID3DBlob** out_shader_blob, ID3DBlob** out_error_blob, const char* error_prefix)
    {
        HRESULT hr = D3DCompile(
            source,
            source_size,
            NULL,
            NULL,
            NULL,
            entry_point,
            profile,
            GetD3DCompileFlags(options),
            0,
            out_shader_blob,
            out_error_blob);

        if (FAILED(hr))
        {
            if (out_error_blob && *out_error_blob)
            {
                dmLogError("%s:\n%s", error_prefix, (char*) (*out_error_blob)->GetBufferPointer());
            }
            else
            {
                dmLogError("%s (HRESULT 0x%08X)", error_prefix, (unsigned int) hr);
            }
            return false;
        }
        return true;
    }

    static bool CompileShaderToolPath(const ShaderCompilerOptions* options, const dmArray<char>& source, const char* profile, ShaderStage stage, int version, dmArray<uint8_t>& out_blob)
    {
        if (IsNullOrEmpty(options->m_ExternalCompilerPath) || IsNullOrEmpty(options->m_ExternalCompilerArgs))
        {
            dmLogError("External HLSL compile skipped: missing external compiler path/args");
            return false;
        }

        const uint32_t TEMP_PATH_BUFFER_SIZE = 1024;
        const uint32_t COMMAND_BUFFER_SIZE = 65536;
        char input_path[TEMP_PATH_BUFFER_SIZE] = {0};
        char output_path[TEMP_PATH_BUFFER_SIZE] = {0};
        char stdout_path[TEMP_PATH_BUFFER_SIZE] = {0};
        char command[COMMAND_BUFFER_SIZE] = {0};
        char command_with_redirect[COMMAND_BUFFER_SIZE] = {0};
        bool success = false;

        if (!CreateTempFilePath("dmh", input_path, sizeof(input_path)) ||
            !CreateTempFilePath("dmo", output_path, sizeof(output_path)) ||
            !CreateTempFilePath("dml", stdout_path, sizeof(stdout_path)))
        {
            dmLogError("Failed to allocate temporary files for external HLSL compiler");
            goto cleanup;
        }

        if (!WriteBytesToFile(input_path, source.Begin(), source.Size()))
        {
            dmLogError("Failed to write temporary HLSL source file for external compiler: %s", input_path);
            goto cleanup;
        }

        if (!BuildExternalCompilerCommand(options, input_path, output_path, profile, stage, version, command, sizeof(command)))
        {
            dmLogError("Failed to build external HLSL compiler command");
            goto cleanup;
        }

        int redirect_written = dmSnPrintf(command_with_redirect, sizeof(command_with_redirect), "%s > \"%s\" 2>&1", command, stdout_path);
        if (redirect_written <= 0 || redirect_written >= (int) sizeof(command_with_redirect))
        {
            dmLogError("Failed to build external HLSL compiler command with stdout/stderr capture");
            goto cleanup;
        }

        dmLogDebug("External HLSL tool command: %s", command_with_redirect);
        // TODO: Do we need a better mechanism of doing this?
        int command_result = system(command_with_redirect);

        if (command_result != 0)
        {
            dmLogError("External HLSL compiler failed with code %d. Command: %s", command_result, command);
            goto cleanup;
        }

        uint32_t output_file_size = 0;
        if (!GetFileSize(output_path, &output_file_size))
        {
            dmLogError("External HLSL output file does not exist or size could not be read: %s", output_path);
            goto cleanup;
        }

        if (!ReadBytesFromFile(output_path, out_blob))
        {
            dmLogError("Failed to read external HLSL compiler output: %s", output_path);
            goto cleanup;
        }
        if (out_blob.Size() == 0)
        {
            dmLogError("External HLSL compiler output was empty. Verify that args write to {output}.");
            goto cleanup;
        }

        success = true;

    cleanup:
        if (!success && stdout_path[0] != '\0')
        {
            uint32_t stdout_file_size = 0;
            if (GetFileSize(stdout_path, &stdout_file_size))
            {
                dmLogError("External HLSL compiler stdout/stderr file size: %u (%s)", stdout_file_size, stdout_path);
            }
        }
        if (input_path[0] != '\0')
            dmSys::Unlink(input_path);
        if (output_path[0] != '\0')
            dmSys::Unlink(output_path);
        if (stdout_path[0] != '\0')
            dmSys::Unlink(stdout_path);

        return success;
    }

    static bool CreateReflectionData(const ShaderCompilerOptions* options, const ShaderCompileResult* raw_hlsl, const char* entry_point, const char* reflection_profile, ID3DBlob** out_shader_blob, ID3DBlob** out_error_blob, ID3D12ShaderReflection** out_reflection, D3D12_SHADER_DESC* out_shader_desc)
    {
        if (!CompileShaderD3DCompiler(options, raw_hlsl->m_Data.Begin(), raw_hlsl->m_Data.Size(), entry_point, reflection_profile, out_shader_blob, out_error_blob, "Shader compile error"))
        {
            return false;
        }

        HRESULT hr = D3DReflect((*out_shader_blob)->GetBufferPointer(), (*out_shader_blob)->GetBufferSize(), __uuidof(ID3D12ShaderReflection), (void**) out_reflection);
        if (FAILED(hr))
        {
            dmLogError("Failed to get shader reflection");
            return false;
        }

        hr = (*out_reflection)->GetDesc(out_shader_desc);
        if (FAILED(hr))
        {
            dmLogError("Failed to get shader description.");
            return false;
        }

        return true;
    }

    static bool CreateRootSignature(const ShaderCompilerOptions* options, const char* source, const char* entry_point, const char* reflection_profile, ShaderStage stage, ID3D12ShaderReflection* reflection, const D3D12_SHADER_DESC* shader_desc, dmArray<char>& inout_binary_source, dmArray<uint8_t>& out_root_signature, ID3DBlob** inout_shader_blob, ID3DBlob** inout_error_blob)
    {
        dmArray<char> root_signature_text;
        if (!IsNullOrEmpty(options->m_RootSignatureOverride))
        {
            uint32_t override_len = (uint32_t) strlen(options->m_RootSignatureOverride);
            root_signature_text.SetCapacity(override_len + 1);
            root_signature_text.SetSize(override_len + 1);
            memcpy(root_signature_text.Begin(), options->m_RootSignatureOverride, override_len + 1);
            dmLogDebug("Using HLSL root signature override (bytes=%u)", override_len);
        }
        else
        {
            GenerateRootSignatureFromReflection(reflection, shader_desc, stage, root_signature_text);
        }

        dmArray<char> sanitized_source;
        bool stripped_existing_root_signatures = false;
        if (!StripExistingRootSignatureAttributes(source, sanitized_source, &stripped_existing_root_signatures))
        {
            dmLogError("Failed to sanitize existing RootSignature attributes before HLSL injection");
            return false;
        }
        if (stripped_existing_root_signatures)
        {
            dmLogDebug("Removed existing RootSignature attributes before injecting generated root signature");
        }

        const char* source_for_injection = stripped_existing_root_signatures ? sanitized_source.Begin() : source;
        dmArray<char> injected_source;
        if (!InjectRootSignatureIntoSource(source_for_injection, root_signature_text.Begin(), injected_source))
        {
            dmLogError("Failed to inject generated root signature into HLSL source");
            return false;
        }

        inout_binary_source.SetCapacity(injected_source.Size());
        inout_binary_source.SetSize(injected_source.Size());
        memcpy(inout_binary_source.Begin(), injected_source.Begin(), injected_source.Size());

        if (*inout_shader_blob)
        {
            (*inout_shader_blob)->Release();
            *inout_shader_blob = NULL;
        }
        if (*inout_error_blob)
        {
            (*inout_error_blob)->Release();
            *inout_error_blob = NULL;
        }

        // Root signature extraction stays in-process so Java/C++ can keep using the same merge path.
        if (!CompileShaderD3DCompiler(options, injected_source.Begin(), injected_source.Size(), entry_point, reflection_profile, inout_shader_blob, inout_error_blob, "Failed to compile HLSL source with injected root signature"))
        {
            return false;
        }

        ID3DBlob* root_signature_blob = NULL;
        HRESULT hr = D3DGetBlobPart((*inout_shader_blob)->GetBufferPointer(), (*inout_shader_blob)->GetBufferSize(), D3D_BLOB_ROOT_SIGNATURE, 0, &root_signature_blob);
        if (FAILED(hr))
        {
            dmLogError("Failed to extract hlsl root signature");
            return false;
        }

        uint32_t root_signature_size = root_signature_blob->GetBufferSize();
        out_root_signature.SetCapacity(root_signature_size);
        out_root_signature.SetSize(root_signature_size);
        memcpy(out_root_signature.Begin(), root_signature_blob->GetBufferPointer(), root_signature_size);
        root_signature_blob->Release();
        return true;
    }

    ShaderCompileResult* CompileRawHLSLToBinary(HShaderContext context, HShaderCompiler compiler, const ShaderCompilerOptions* options, ShaderCompileResult* raw_hlsl)
    {
        DM_TRACE_LINE();
        ID3DBlob* shader_blob = NULL;
        ID3DBlob* error_blob = NULL;
        ID3D12ShaderReflection* reflection = NULL;
        char* src_data = 0;
        bool success = false;
        ShaderCompileResult* result = 0;

        dmArray<uint8_t> output_blob;
        dmArray<uint8_t> root_signature_data;
        dmArray<char> binary_source;
        dmArray<CombinedSampler> combined_samplers;

        const int version = options->m_Version;
        assert(version == 50 || version == 51 || version >= 60);

        const bool use_external_compiler = !IsNullOrEmpty(options->m_ExternalCompilerPath) && !IsNullOrEmpty(options->m_ExternalCompilerArgs);
        if (version >= 60 && !use_external_compiler)
        {
            dmLogError("HLSL shader model %d requires an external compiler (DXC/FXC) configured from Java options.", version);
            goto cleanup;
        }

        const char* entry_point = options->m_EntryPoint ? options->m_EntryPoint : "main";

        char profile[32];
        BuildShaderProfile(context->m_Stage, version, profile);

        int reflection_version = version >= 60 ? 51 : version;
        char reflection_profile[32];
        BuildShaderProfile(context->m_Stage, reflection_version, reflection_profile);

        D3D12_SHADER_DESC shaderDesc;
        // Step 1: Reflection pass with D3DCompiler to build deterministic resource mappings.
        if (!CreateReflectionData(options, raw_hlsl, entry_point, reflection_profile, &shader_blob, &error_blob, &reflection, &shaderDesc))
        {
            goto cleanup;
        }

    #if 0 // DEBUG
        PrintRootSignatureFromReflection(reflection, &shaderDesc);
    #endif

        // Explicitly null-terminate the incoming string before source text processing.
        src_data = (char*) malloc(raw_hlsl->m_Data.Size()+1);
        if (!src_data)
        {
            dmLogError("Out of memory while preparing HLSL source");
            goto cleanup;
        }

        memcpy((void*) src_data, raw_hlsl->m_Data.Begin(), raw_hlsl->m_Data.Size());
        src_data[raw_hlsl->m_Data.Size()] = '\0';

        binary_source.SetCapacity(raw_hlsl->m_Data.Size());
        binary_source.SetSize(raw_hlsl->m_Data.Size());
        memcpy(binary_source.Begin(), raw_hlsl->m_Data.Begin(), raw_hlsl->m_Data.Size());

        // Step 2: Create root signature and inject it into the source for SM5.1+.
        if (version > 50)
        {
            if (!CreateRootSignature(options, (const char*) src_data, entry_point, reflection_profile, context->m_Stage, reflection, &shaderDesc, binary_source, root_signature_data, &shader_blob, &error_blob))
            {
                goto cleanup;
            }
        }

        // Step 3: Compile final shader blob using either external toolchain or D3DCompiler fallback.
        if (use_external_compiler)
        {
            if (!CompileShaderToolPath(options, binary_source, profile, context->m_Stage, version, output_blob))
            {
                goto cleanup;
            }
        }
        else if (version > 50)
        {
            uint32_t shader_size = shader_blob->GetBufferSize();
            output_blob.SetCapacity(shader_size);
            output_blob.SetSize(shader_size);
            memcpy(output_blob.Begin(), shader_blob->GetBufferPointer(), shader_size);
        }
        else
        {
            // Backwards-compatible path: keep shipping HLSL source when no external toolchain is configured.
            uint32_t data_size = raw_hlsl->m_Data.Size();
            output_blob.SetCapacity(data_size);
            output_blob.SetSize(data_size);
            memcpy(output_blob.Begin(), raw_hlsl->m_Data.Begin(), data_size);
        }

        result = (ShaderCompileResult*) malloc(sizeof(ShaderCompileResult));
        if (!result)
        {
            dmLogError("Out of memory while allocating shader compile result");
            goto cleanup;
        }

        memset(result, 0, sizeof(ShaderCompileResult));

        result->m_Data.SetCapacity(output_blob.Size());
        result->m_Data.SetSize(output_blob.Size());
        if (output_blob.Size() > 0)
        {
            memcpy(result->m_Data.Begin(), output_blob.Begin(), output_blob.Size());
        }

        result->m_HLSLRootSignature.SetCapacity(root_signature_data.Size());
        result->m_HLSLRootSignature.SetSize(root_signature_data.Size());
        if (root_signature_data.Size() > 0)
        {
            memcpy(result->m_HLSLRootSignature.Begin(), root_signature_data.Begin(), root_signature_data.Size());
        }

        result->m_LastError = "";
        result->m_HLSLNumWorkGroupsId = raw_hlsl->m_HLSLNumWorkGroupsId;
        result->m_HLSLResourceMappings.SetCapacity(shaderDesc.BoundResources);
        result->m_HLSLResourceMappings.SetSize(shaderDesc.BoundResources);

        GetCombinedSamplerMapSPIRV(context, (ShaderCompilerSPVC*) compiler, combined_samplers);

        FillResourceEntryArray(context, reflection, &shaderDesc, combined_samplers, result->m_HLSLResourceMappings);

        success = true;

    cleanup:
        if (error_blob)
            error_blob->Release();
        if (shader_blob)
            shader_blob->Release();
        if (reflection)
            reflection->Release();
        if (src_data)
            free(src_data);

        if (!success)
        {
            if (result)
            {
                result->m_Data.SetCapacity(0);
                result->m_HLSLResourceMappings.SetCapacity(0);
                result->m_HLSLRootSignature.SetCapacity(0);
                free(result);
            }
            return 0;
        }

        return result;
    }

    // We may want to update this to support more than 2 root signatures
    // Concatenate two serialized root signature blobs given as raw pointer + size
    static HRESULT EnsureRootSignatureFlags(const void* blob_ptr, size_t blob_size, D3D12_ROOT_SIGNATURE_FLAGS flags_to_or, ID3DBlob** out_blob)
    {
        DM_TRACE_LINE();
        if (!blob_ptr || blob_size == 0 || !out_blob)
        {
            DM_TRACE_LINE();
            return E_INVALIDARG;
        }

        *out_blob = NULL;

        ID3D12RootSignatureDeserializer* deserializer = NULL;
        HRESULT hr = D3D12CreateRootSignatureDeserializer(blob_ptr, blob_size, IID_PPV_ARGS(&deserializer));
        if (FAILED(hr))
        {
            DM_TRACE_LINE();
            return hr;
        }

        const D3D12_ROOT_SIGNATURE_DESC* desc = deserializer->GetRootSignatureDesc();
        if (!desc)
        {
            DM_TRACE_LINE();
            deserializer->Release();
            return E_FAIL;
        }

        D3D12_ROOT_SIGNATURE_DESC patched_desc = *desc;
        patched_desc.Flags = (D3D12_ROOT_SIGNATURE_FLAGS) (desc->Flags | flags_to_or);

        ID3DBlob* error_blob = NULL;
        hr = D3D12SerializeRootSignature(&patched_desc, D3D_ROOT_SIGNATURE_VERSION_1, out_blob, &error_blob);
        if (FAILED(hr) && error_blob)
        {
            DM_TRACE_LINE();
            dmLogError("%s", (const char*) error_blob->GetBufferPointer());
            error_blob->Release();
        }
        else if (error_blob)
        {
            error_blob->Release();
        }

        deserializer->Release();
        return hr;
    }

    // We may want to update this to support more than 2 root signatures
    // Concatenate two serialized root signature blobs given as raw pointer + size
    static HRESULT ConcatenateRootSignatures(const void* blob_a_ptr, size_t blob_a_size, const void* blob_b_ptr, size_t blob_b_size, ID3DBlob** out_merged_blob)
    {
        DM_TRACE_LINE();
        if (!blob_a_ptr || !blob_b_ptr || !out_merged_blob)
        {
        DM_TRACE_LINE();
            return E_INVALIDARG;
        }

        ID3D12RootSignatureDeserializer* deserializer_a = NULL;
        ID3D12RootSignatureDeserializer* deserializer_b = NULL;

        DM_TRACE_LINE();
        HRESULT hr = D3D12CreateRootSignatureDeserializer(blob_a_ptr, blob_a_size, IID_PPV_ARGS(&deserializer_a));
        if (FAILED(hr))
        {
        DM_TRACE_LINE();
            return hr;
        }

        DM_TRACE_LINE();
        hr = D3D12CreateRootSignatureDeserializer(blob_b_ptr, blob_b_size, IID_PPV_ARGS(&deserializer_b));
        if (FAILED(hr))
        {
        DM_TRACE_LINE();
            deserializer_a->Release();
            return hr;
        }

        DM_TRACE_LINE();
        const D3D12_ROOT_SIGNATURE_DESC* desc_a = deserializer_a->GetRootSignatureDesc();
        const D3D12_ROOT_SIGNATURE_DESC* desc_b = deserializer_b->GetRootSignatureDesc();

        // Allocate combined arrays
        UINT total_params = desc_a->NumParameters + desc_b->NumParameters;
        UINT total_samplers = desc_a->NumStaticSamplers + desc_b->NumStaticSamplers;

        D3D12_ROOT_PARAMETER* root_params = (D3D12_ROOT_PARAMETER*) calloc(total_params, sizeof(D3D12_ROOT_PARAMETER));
        D3D12_STATIC_SAMPLER_DESC* static_samplers = NULL;

        if (total_samplers > 0)
        {
            static_samplers = (D3D12_STATIC_SAMPLER_DESC*) calloc(total_samplers, sizeof(D3D12_STATIC_SAMPLER_DESC));
        }

        // Copy root parameters
        memcpy(root_params, desc_a->pParameters, sizeof(D3D12_ROOT_PARAMETER) * desc_a->NumParameters);
        memcpy(root_params + desc_a->NumParameters, desc_b->pParameters, sizeof(D3D12_ROOT_PARAMETER) * desc_b->NumParameters);

        // Copy static samplers if any
        if (static_samplers)
        {
            memcpy(static_samplers, desc_a->pStaticSamplers, sizeof(D3D12_STATIC_SAMPLER_DESC) * desc_a->NumStaticSamplers);
            memcpy(static_samplers + desc_a->NumStaticSamplers, desc_b->pStaticSamplers, sizeof(D3D12_STATIC_SAMPLER_DESC) * desc_b->NumStaticSamplers);
        }

        D3D12_ROOT_SIGNATURE_DESC merged_desc;
        merged_desc.NumParameters     = total_params;
        merged_desc.pParameters       = root_params;
        merged_desc.NumStaticSamplers = total_samplers;
        merged_desc.pStaticSamplers   = static_samplers;
        const D3D12_ROOT_SIGNATURE_FLAGS graphics_root_flags =
            (D3D12_ROOT_SIGNATURE_FLAGS)
            (
                D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
            );
        merged_desc.Flags             = (D3D12_ROOT_SIGNATURE_FLAGS) (desc_a->Flags | desc_b->Flags | graphics_root_flags);

        DM_TRACE_LINE();
        ID3DBlob* error_blob = NULL;
        hr = D3D12SerializeRootSignature(&merged_desc, D3D_ROOT_SIGNATURE_VERSION_1, out_merged_blob, &error_blob);
        if (FAILED(hr))
        {
        DM_TRACE_LINE();
            if (error_blob)
            {
                dmLogError("%s", error_blob->GetBufferPointer());
                error_blob->Release();
            }
        }

        // Cleanup
        deserializer_a->Release();
        deserializer_b->Release();
        free(root_params);
        if (static_samplers)
        {
            free(static_samplers);
        }

        DM_TRACE_LINE();
        return hr;
    }

    static bool PatchShaderBlobRootSignature(dmArray<uint8_t>& inout_shader_blob, const dmArray<uint8_t>& merged_root_signature, char out_error[256])
    {
        if (inout_shader_blob.Size() == 0)
        {
            dmSnPrintf(out_error, 256, "Shader blob was empty");
            return false;
        }

        if (merged_root_signature.Size() == 0)
        {
            dmSnPrintf(out_error, 256, "Merged root signature was empty");
            return false;
        }

        ID3DBlob* patched_blob = 0;
        HRESULT hr = D3DSetBlobPart(
            inout_shader_blob.Begin(),
            inout_shader_blob.Size(),
            D3D_BLOB_ROOT_SIGNATURE,
            0,
            merged_root_signature.Begin(),
            merged_root_signature.Size(),
            &patched_blob);

        if (FAILED(hr))
        {
            dmSnPrintf(out_error, 256, "D3DSetBlobPart failed (HRESULT 0x%08X)", (unsigned int) hr);
            return false;
        }

        uint32_t patched_size = patched_blob->GetBufferSize();
        inout_shader_blob.SetCapacity(patched_size);
        inout_shader_blob.SetSize(patched_size);
        memcpy(inout_shader_blob.Begin(), patched_blob->GetBufferPointer(), patched_size);
        patched_blob->Release();
        return true;
    }

    HLSLRootSignature* HLSLMergeRootSignatures(ShaderCompileResult* shaders, uint32_t shaders_size)
    {
        DM_TRACE_LINE();

        HRESULT hr = S_OK;
        const char* _errstring = 0;

        HLSLRootSignature* result = new HLSLRootSignature;
        ID3DBlob* merged_signature_blob = 0;

        // Validate inputs and gather pointers to root signature blobs
        if (shaders_size == 0)
        {
            static const char* _err = "No shaders provided for root signature merge";
            _errstring = _err;
            goto cleanup;
        }

        if (shaders_size == 1)
        {
            // Just forward the existing root signature blob
            uint32_t merged_size = shaders[0].m_HLSLRootSignature.Size();
            if (merged_size == 0)
            {
                static const char* _err = "Shader has no HLSL root signature to merge";
                _errstring = _err;
                goto cleanup;
            }

            result->m_HLSLRootSignature.SetCapacity(merged_size);
            result->m_HLSLRootSignature.SetSize(merged_size);
            memcpy(result->m_HLSLRootSignature.Begin(), shaders[0].m_HLSLRootSignature.Begin(), merged_size);
        }
        else if (shaders_size == 2)
        {
            const void* a_ptr = shaders[0].m_HLSLRootSignature.Begin();
            size_t      a_sz  = shaders[0].m_HLSLRootSignature.Size();
            const void* b_ptr = shaders[1].m_HLSLRootSignature.Begin();
            size_t      b_sz  = shaders[1].m_HLSLRootSignature.Size();

            if (a_sz == 0 || b_sz == 0)
            {
                static const char* _err = "One or both shaders are missing HLSL root signatures";
                _errstring = _err;
                goto cleanup;
            }

            if (a_sz == b_sz && memcmp(a_ptr, b_ptr, a_sz) == 0)
            {
                // Keep the fast path for equal signatures, but still enforce IA root flag.
                const D3D12_ROOT_SIGNATURE_FLAGS graphics_root_flags =
                    (D3D12_ROOT_SIGNATURE_FLAGS)
                    (
                        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
                    );
                hr = EnsureRootSignatureFlags(a_ptr, a_sz, graphics_root_flags, &merged_signature_blob);
                if (FAILED(hr))
                {
                    static const char* _err = "Failed to apply required root signature flags";
                    _errstring = _err;
                    goto cleanup;
                }
            }
            else
            {
                hr = ConcatenateRootSignatures(a_ptr, a_sz, b_ptr, b_sz, &merged_signature_blob);
                if (FAILED(hr))
                {
                    static const char* _err = "Failed to merge root signatures";
                    _errstring = _err;
                    goto cleanup;
                }
            }
        }
        else
        {
            assert(false && "Currently unsupported"); // Implement a more generic merging function
        }

        if (merged_signature_blob)
        {
            uint32_t merged_size = merged_signature_blob->GetBufferSize();
            result->m_HLSLRootSignature.SetCapacity(merged_size);
            result->m_HLSLRootSignature.SetSize(merged_size);
            memcpy(result->m_HLSLRootSignature.Begin(), merged_signature_blob->GetBufferPointer(), merged_size);
        }

        // Ensure each compiled shader blob emplaces the same merged root signature.
        // Xbox precompiled shader validation compares runtime root signature to each emplaced shader signature.
        for (uint32_t i = 0; i < shaders_size; ++i)
        {
            uint32_t merged_size = result->m_HLSLRootSignature.Size();
            shaders[i].m_HLSLRootSignature.SetCapacity(merged_size);
            shaders[i].m_HLSLRootSignature.SetSize(merged_size);
            memcpy(shaders[i].m_HLSLRootSignature.Begin(), result->m_HLSLRootSignature.Begin(), merged_size);

            if (shaders[i].m_Data.Size() > 0)
            {
                char patch_error[256];
                if (!PatchShaderBlobRootSignature(shaders[i].m_Data, result->m_HLSLRootSignature, patch_error))
                {
                    dmLogError("%s: Failed to patch shader blob %u with merged root signature: %s", __FUNCTION__, i, patch_error);
                    static const char* _err = "Failed to patch shader blob with merged HLSL root signature";
                    _errstring = _err;
                    goto cleanup;
                }
            }
        }

cleanup:
        if (merged_signature_blob)
            merged_signature_blob->Release();

        result->m_LastError = "";
        if (_errstring)
        {
            dmLogError("%s: %s", __FUNCTION__, _errstring);
            result->m_LastError = _errstring;
        }
        return result;
    }

}
