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

#include <stdlib.h>
#include <string.h>

#include <wrl/client.h>

#include <d3d12shader.h>
#include <d3d12.h>
#include <d3dcompiler.h>

#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/dstrings.h>

#include "shaderc_private.h"

// TODO: Oof this feels brittle, how can we avoid it?
interface DECLSPEC_UUID("5A58797D-A72C-478D-8BA2-EFC6B0EFE88E") ID3D12ShaderReflection;

namespace dmShaderc
{
    static const dmhash_t HASH_SPIRV_CROSS_NUM_WORKGROUPS = dmHashString64("SPIRV_Cross_NumWorkgroups");

    static char* ExtractBaseName(const char* combined_name)
    {
        const char* suffix = "_sampler";
        size_t len = strlen(combined_name);
        size_t suffix_len = strlen(suffix);

        // Must start with '_' and end with "_sampler"
        if (len <= suffix_len + 1 || combined_name[0] != '_')
            return NULL;

        if (strcmp(combined_name + len - suffix_len, suffix) != 0)
            return NULL;

        // Allocate new string for base name (exclude leading '_' and trailing '_sampler')
        size_t base_len = len - suffix_len - 1;
        char* base_name = (char*)malloc(base_len + 1);
        if (!base_name)
            return NULL;

        memcpy(base_name, combined_name + 1, base_len);
        base_name[base_len] = '\0';

        return base_name;
    }

    static const char* FindCombinedSampler(dmArray<CombinedSampler>& combined_samplers, const char* name, D3D_SHADER_INPUT_TYPE input_type)
    {
        const char* to_test = name;

        // Strip all leading '_'
        while(to_test[0] == '_')
            to_test++;

        long int id = strtol(to_test, 0, 10);

        dmLogInfo("Id: %d, size: %d", (int) id, combined_samplers.Size());

        for (int i = 0; i < combined_samplers.Size(); ++i)
        {
            dmLogInfo("  Checking %d, %d, %d", combined_samplers[i].m_CombinedId, combined_samplers[i].m_ImageId, combined_samplers[i].m_SamplerId);

            if (id == combined_samplers[i].m_CombinedId)
            {
                if (input_type == D3D_SIT_SAMPLER)
                    return combined_samplers[i].m_SamplerName;
                else if (input_type == D3D_SIT_TEXTURE)
                    return combined_samplers[i].m_ImageName;
            }
        }
        return 0;
    }

    static void FillResourceEntryArray(HShaderContext context, ID3D12ShaderReflection* hlsl_reflection, D3D12_SHADER_DESC* shaderDesc, dmArray<CombinedSampler>& combined_samplers, dmArray<HLSLResourceMapping>& resource_entries)
    {
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
                    dmLogInfo("Checking in combined samplers for %s", bindDesc.Name);

                    const char* combined_sampler_name = FindCombinedSampler(combined_samplers, bindDesc.Name, bindDesc.Type);
                    if (combined_sampler_name)
                    {
                        dmLogInfo("Combined sampler name: %s", combined_sampler_name);
                        resource = FindShaderResourceUniform(context, dmHashString64(combined_sampler_name));
                    }
                }

                // 2.1 Separated samplers may not be found in the combined samplers array, nor in the general reflection data
                //     So we need to extract the generated base name and check for that instead.
                if (!resource && bindDesc.Type == D3D_SIT_SAMPLER)
                {
                    char* base_texture_name = ExtractBaseName(bindDesc.Name);
                    dmLogInfo("Maybe sampler ? %s", base_texture_name);

                    resource = FindShaderResourceUniform(context, dmHashString64(base_texture_name));
                    free(base_texture_name);
                }
            }

            if (resource)
            {
                dmLogInfo("Found resource %s (set=%d, binding=%d)", bindDesc.Name, resource->m_Set, resource->m_Binding);

                resource_entries[i].m_ShaderResourceSet     = resource->m_Set;
                resource_entries[i].m_ShaderResourceBinding = resource->m_Binding;
            }
            // 3. For compute shaders, we need to deal with this resource separately, since the gl_NumWorkgroups built-in doesn't
            //    exist in HLSL. It will be converted into a cbuffer in hlsl, so we need to keep track of that separately.
            else if (resource_name_hash == HASH_SPIRV_CROSS_NUM_WORKGROUPS)
            {
                dmLogInfo("Found SPIRV_Cross_NumWorkgroups (binding=%d)", bindDesc.BindPoint);

                resource_entries[i].m_ShaderResourceSet     = HLSL_NUM_WORKGROUPS_SET; // note: we set the explicit set decoration in shaderc_spvc.cpp
                resource_entries[i].m_ShaderResourceBinding = bindDesc.BindPoint;
            }
            else
            {
                dmLogInfo("Did not find resource %s", bindDesc.Name);
            }
        }
    }

    static void PrintRootSignatureFromReflection(ID3D12ShaderReflection* reflection, D3D12_SHADER_DESC* shaderDesc)
    {
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

    static void PrintHResultError(HRESULT hr, const char* context)
    {
        char* msg = NULL;

        FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            hr,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&msg,
            0,
            NULL
        );

        if (msg)
        {
            dmLogError("%s failed: 0x%08X - %s\n", context, (unsigned int)hr, msg);
            LocalFree(msg);
        }
        else
        {
            dmLogError("%s failed: 0x%08X (Unknown error)\n", context, (unsigned int)hr);
        }
    }

    static void GenerateRootSignatureFromReflection(ID3D12ShaderReflection* reflection, const D3D12_SHADER_DESC* shader_desc, dmArray<char>& buffer)
    {
        const int BUFFER_SIZE = 1024;

        buffer.SetCapacity(BUFFER_SIZE);
        buffer.SetSize(BUFFER_SIZE);

        char* write_str = buffer.Begin();
        memset(write_str, 0, BUFFER_SIZE);

        const char* prefix = "[RootSignature(\"";
        const char* suffix = "\")]\n";

        write_str += dmSnPrintf(write_str, BUFFER_SIZE, prefix);

        for (uint32_t i = 0; i < shader_desc->BoundResources; ++i)
        {
            D3D12_SHADER_INPUT_BIND_DESC bind_desc;
            reflection->GetResourceBindingDesc(i, &bind_desc);

            if (i > 0)
            {
                *write_str++ = ',';
            }

            uint32_t left = buffer.Size() - (write_str - buffer.Begin());

            switch (bind_desc.Type)
            {
            case D3D_SIT_CBUFFER:
                write_str += dmSnPrintf(write_str, left, "CBV(b%u,space=%u)", bind_desc.BindPoint, bind_desc.Space);
                break;
            case D3D_SIT_TEXTURE:
                write_str += dmSnPrintf(write_str, left, "DescriptorTable(SRV(t%u,space=%u))", bind_desc.BindPoint, bind_desc.Space);
                break;
            case D3D_SIT_SAMPLER:
                write_str += dmSnPrintf(write_str, left, "DescriptorTable(Sampler(s%u,space=%u))", bind_desc.BindPoint, bind_desc.Space);
                break;
            case D3D_SIT_UAV_RWTYPED:
                write_str += dmSnPrintf(write_str, left, "DescriptorTable(UAV(u%u,space=%u))", bind_desc.BindPoint, bind_desc.Space);
                break;
            default: break;
            }
        }

        dmSnPrintf(write_str, buffer.Size() - (write_str - buffer.Begin()), suffix);
    }

    static bool InjectRootSignatureIntoSource(const char* source, const char* root_signature, dmArray<char>& injected_buffer)
    {
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

    ShaderCompileResult* CompileRawHLSLToBinary(HShaderContext context, ShaderCompileResult* raw_hlsl, dmArray<CombinedSampler>& combined_samplers)
    {
        ID3DBlob* shader_blob = NULL;
        ID3DBlob* error_blob = NULL;

        const char* profile = "";

        // TODO: correct versions here
        switch(context->m_Stage)
        {
        case SHADER_STAGE_VERTEX:
            profile = "vs_5_1";
            break;
        case SHADER_STAGE_FRAGMENT:
            profile = "ps_5_1";
            break;
        case SHADER_STAGE_COMPUTE:
            profile = "cs_5_1";
            break;
        }

        HRESULT hr = D3DCompile(raw_hlsl->m_Data.Begin(), raw_hlsl->m_Data.Size(), NULL, NULL, NULL, "main", profile, D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS, 0, &shader_blob, &error_blob);
        if (FAILED(hr))
        {
            if (error_blob)
            {
                dmLogError("Shader compile error:\n%s", (char*) error_blob->GetBufferPointer());
                error_blob->Release();
            }
            else
            {
                dmLogError("Shader compile failed with HRESULT 0x%08X", (unsigned int)hr);
            }
            return 0;
        }

        ID3D12ShaderReflection* reflection = NULL;
        hr = D3DReflect(shader_blob->GetBufferPointer(), shader_blob->GetBufferSize(), __uuidof(ID3D12ShaderReflection), (void**)&reflection);

        if (FAILED(hr))
        {
            dmLogError("D3DReflect failed");

            PrintHResultError(hr, "D3DReflect");

            shader_blob->Release();
            return 0;
        }

        D3D12_SHADER_DESC shaderDesc;
        hr = reflection->GetDesc(&shaderDesc);
        if (FAILED(hr))
        {
            dmLogError("Failed to get shader description.");
            return 0;
        }

        // Explicitly null-terminate the incoming string.
        char* src_data = (char*) malloc(raw_hlsl->m_Data.Size()+1);
        memcpy((void*) src_data, raw_hlsl->m_Data.Begin(), raw_hlsl->m_Data.Size());
        src_data[raw_hlsl->m_Data.Size()] = '\0';

        PrintRootSignatureFromReflection(reflection, &shaderDesc);

        dmArray<char> root_signature_buffer;
        GenerateRootSignatureFromReflection(reflection, &shaderDesc, root_signature_buffer);

        dmArray<char> injected_source_buffer;
        InjectRootSignatureIntoSource((const char*) src_data, root_signature_buffer.Begin(), injected_source_buffer);

        ShaderCompileResult* result = (ShaderCompileResult*) malloc(sizeof(ShaderCompileResult));
        memset(result, 0, sizeof(ShaderCompileResult));

        uint32_t data_size = injected_source_buffer.Size();

        result->m_Data.SetCapacity(data_size);
        result->m_Data.SetSize(data_size);

        result->m_LastError = "";
        result->m_HLSLNumWorkGroupsId = raw_hlsl->m_HLSLNumWorkGroupsId;
        result->m_HLSLResourceMappings.SetCapacity(shaderDesc.BoundResources);
        result->m_HLSLResourceMappings.SetSize(shaderDesc.BoundResources);

        char* write_str = (char*) result->m_Data.Begin();
        memset(write_str, 0, data_size);
        memcpy(write_str, injected_source_buffer.Begin(), data_size);

        FillResourceEntryArray(context, reflection, &shaderDesc, combined_samplers, result->m_HLSLResourceMappings);

        free(src_data);

        return result;
    }
}
