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

interface DECLSPEC_UUID("5A58797D-A72C-478D-8BA2-EFC6B0EFE88E") ID3D12ShaderReflection;

namespace dmShaderc
{
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
            case D3D_SIT_CBUFFER:        typeStr = "CBV"; break;
            case D3D_SIT_TBUFFER:        typeStr = "TBUFFER"; break;
            case D3D_SIT_TEXTURE:        typeStr = "SRV (Texture)"; break;
            case D3D_SIT_SAMPLER:        typeStr = "Sampler"; break;
            case D3D_SIT_STRUCTURED:     typeStr = "SRV (StructuredBuffer)"; break;
            case D3D_SIT_UAV_RWTYPED:    typeStr = "UAV"; break;
            case D3D_SIT_UAV_RWSTRUCTURED: typeStr = "UAV (RWStructuredBuffer)"; break;
            default:                     typeStr = "Other"; break;
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
        char* msg = nullptr;

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

    static inline void EscapeRootSignatureString(const char* input, char* output) {
        while (*input)
        {
            if (*input == '"')
            {
                *output++ = '\\';
                *output++ = '"';
            }
            else
            {
                *output++ = *input;
            }
            input++;
        }
        *output = '\0';
    }

    static bool InjectRootSignatureIntoSource(const char* source, const char* root_signature, dmArray<char>& injected_buffer)
    {
        // TODO: Support non-main names (this is passed from options table)
        const char* marker = "SPIRV_Cross_Output main(";
        const char* insert_pos = strstr(source, marker);

        // "main" function not found
        if (!insert_pos)
        {
            return false;
        }

        size_t prefix_len   = insert_pos - source;
        size_t root_sig_len = strlen(root_signature);
        size_t total_len    = prefix_len + root_sig_len + 64 + strlen(source); // generous padding

        injected_buffer.SetCapacity(total_len);
        injected_buffer.SetSize(total_len);

        char* result = (char*) injected_buffer.Begin();

        // Copy everything before the main() declaration
        memcpy(result, source, prefix_len);

        // Write the [RootSignature("...")] string
        int offset = (int)prefix_len;
        offset += dmSnPrintf(result + offset, total_len - offset, "%s\n", root_signature);

        // Copy the rest of the source
        strcpy(result + offset, source + prefix_len);

        return true;
    }

    ShaderCompileResult* CompileRawHLSLToBinary(HShaderContext context, ShaderCompileResult* raw_hlsl)
    {
        ID3DBlob* shader_blob = NULL;
        ID3DBlob* error_blob = NULL;

        const char* profile = "";

        switch(context->m_Stage)
        {
        case SHADER_STAGE_VERTEX:
            profile = "vs_5_0";
            break;
        case SHADER_STAGE_FRAGMENT:
            profile = "ps_5_0";
            break;
        case SHADER_STAGE_COMPUTE:
            profile = "cs_5_0";
            break;
        }

        HRESULT hr = D3DCompile(
            raw_hlsl->m_Data.Begin(),
            raw_hlsl->m_Data.Size(),
            NULL, NULL, NULL,
            "main",
            profile,
            D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS,
            0,
            &shader_blob,
            &error_blob
        );

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
        hr = D3DReflect(
            shader_blob->GetBufferPointer(),
            shader_blob->GetBufferSize(),
            __uuidof(ID3D12ShaderReflection),
            (void**)&reflection
        );

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

        PrintRootSignatureFromReflection(reflection, &shaderDesc);

        dmArray<char> root_signature_buffer;
        GenerateRootSignatureFromReflection(reflection, &shaderDesc, root_signature_buffer);

        dmArray<char> injected_source_buffer;
        InjectRootSignatureIntoSource((const char*) raw_hlsl->m_Data.Begin(), root_signature_buffer.Begin(), injected_source_buffer);

        ShaderCompileResult* result = (ShaderCompileResult*) malloc(sizeof(ShaderCompileResult));
        memset(result, 0, sizeof(ShaderCompileResult));

        result->m_Data.SetCapacity(injected_source_buffer.Size());
        result->m_Data.SetSize(injected_source_buffer.Size());
        result->m_LastError = "";

        memcpy(result->m_Data.Begin(), injected_source_buffer.Begin(), result->m_Data.Size());

        return result;
    }
}
