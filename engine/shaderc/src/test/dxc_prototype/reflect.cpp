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

// Native *nix reflection proof using DXC's own portable COM declarations and
// its pinned DirectX-Headers revision. No Windows SDK or D3D runtime required.
#include <dxc/dxcapi.h>
#define interface struct
#include <d3d12shader.h>
#undef interface
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Check(bool condition, const char* message)
{
    if (!condition)
    {
        fprintf(stderr, "%s\n", message);
        exit(1);
    }
}

// Verifies native DXIL reflection preserves fixture bindings and constant buffer layout.
int main(int argc, char** argv)
{
    Check(argc == 3, "Usage: reflect <libdxcompiler.so/dylib> <shader.reflection>");
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    Check(library != 0, "Cannot load DXC library");
    DxcCreateInstanceProc create = (DxcCreateInstanceProc)dlsym(library, "DxcCreateInstance");
    Check(create != 0, "Cannot find DxcCreateInstance");
    IDxcUtils* utils = 0;
    Check(SUCCEEDED(create(CLSID_DxcUtils, IID_PPV_ARGS(&utils))), "Cannot create IDxcUtils");

    FILE* file = fopen(argv[2], "rb");
    Check(file != 0, "Cannot open reflection data");
    Check(fseek(file, 0, SEEK_END) == 0, "Cannot seek reflection data");
    long length = ftell(file);
    Check(length > 0, "Empty reflection data");
    rewind(file);
    void* data = malloc((size_t)length);
    Check(data != 0, "Cannot allocate reflection data");
    Check(fread(data, 1, length, file) == (size_t)length, "Cannot read reflection data");
    fclose(file);
    DxcBuffer buffer = {data, (size_t)length, 0};
    ID3D12ShaderReflection* reflection = 0;
    Check(SUCCEEDED(utils->CreateReflection(&buffer, IID_PPV_ARGS(&reflection))),
          "IDxcUtils::CreateReflection failed");
    D3D12_SHADER_DESC desc;
    Check(SUCCEEDED(reflection->GetDesc(&desc)), "GetDesc failed");
    UINT stage = D3D12_SHVER_GET_TYPE(desc.Version);
    UINT expected_count = stage == D3D12_SHVER_VERTEX_SHADER ? 1 : 3;
    Check(desc.BoundResources == expected_count, "Wrong resource count");
    for (UINT i = 0; i < desc.BoundResources; ++i)
    {
        D3D12_SHADER_INPUT_BIND_DESC binding;
        Check(SUCCEEDED(reflection->GetResourceBindingDesc(i, &binding)), "GetResourceBindingDesc failed");
        bool expected =
            (strcmp(binding.Name, "Params") == 0 && binding.Type == D3D_SIT_CBUFFER &&
             binding.BindPoint == 0 && binding.Space == 1 && binding.BindCount == 1) ||
            (strcmp(binding.Name, "textures") == 0 && binding.Type == D3D_SIT_TEXTURE &&
             binding.BindPoint == 0 && binding.Space == 2 && binding.BindCount == 2) ||
            (strcmp(binding.Name, "texture_sampler") == 0 && binding.Type == D3D_SIT_SAMPLER &&
             binding.BindPoint == 0 && binding.Space == 2 && binding.BindCount == 1) ||
            (strcmp(binding.Name, "output_buffer") == 0 && binding.Type == D3D_SIT_UAV_RWSTRUCTURED &&
             binding.BindPoint == 0 && binding.Space == 3 && binding.BindCount == 1);
        Check(expected, "Unexpected resource binding");
        printf("%s: register=%u space=%u count=%u type=%u\n", binding.Name,
               binding.BindPoint, binding.Space, binding.BindCount, binding.Type);
    }
    ID3D12ShaderReflectionConstantBuffer* constants = reflection->GetConstantBufferByName("Params");
    D3D12_SHADER_BUFFER_DESC constant_desc;
    Check(SUCCEEDED(constants->GetDesc(&constant_desc)) && constant_desc.Size == 80 &&
          constant_desc.Variables == 2, "Incorrect constant buffer layout");
    const char* names[] = {"tint", "transform"};
    UINT offsets[] = {0, 16};
    UINT sizes[] = {16, 64};
    for (UINT i = 0; i < 2; ++i)
    {
        ID3D12ShaderReflectionVariable* variable = constants->GetVariableByName(names[i]);
        D3D12_SHADER_VARIABLE_DESC variable_desc;
        Check(SUCCEEDED(variable->GetDesc(&variable_desc)) &&
              variable_desc.StartOffset == offsets[i] && variable_desc.Size == sizes[i],
              "Incorrect member layout");
    }
    if (stage == D3D12_SHVER_COMPUTE_SHADER)
    {
        UINT x, y, z;
        reflection->GetThreadGroupSize(&x, &y, &z);
        Check(x == 8 && y == 1 && z == 1, "Incorrect compute group dimensions");
    }
    reflection->Release();
    utils->Release();
    free(data);
    dlclose(library);
    puts("PASS: native DXC reflection API");
    return 0;
}
