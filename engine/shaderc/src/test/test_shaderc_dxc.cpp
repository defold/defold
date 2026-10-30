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

#include "shaderc_dxc.h"
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

// Exercises the production compiler, reflection, and root extraction without a Windows SDK on Unix.
int main(int argc, char** argv)
{
    const char* source =
        "cbuffer Params : register(b2, space1) { float4 tint; };"
        "RWStructuredBuffer<float4> output_data : register(u3, space2);"
        "[RootSignature(\"CBV(b2, space=1), DescriptorTable(UAV(u3, space=2))\")]"
        "[numthreads(4, 2, 1)] void main(uint3 id : SV_DispatchThreadID) { output_data[id.x] = tint; }";
    IDxcBlob* object = 0;
    IDxcBlobUtf8* errors = 0;
    ID3D12ShaderReflection* reflection = 0;
    HRESULT hr = dmShaderc::CompileDXC(source, strlen(source), "main", "cs_6_0", &object, &errors, &reflection);
    if (errors && errors->GetStringLength()) fprintf(stderr, "%s", errors->GetStringPointer());
    Check(SUCCEEDED(hr) && object && reflection, "SM6 compilation failed");
    Check(object->GetBufferSize() > 32 && memcmp(object->GetBufferPointer(), "DXBC", 4) == 0, "Missing shader container");
    D3D12_SHADER_DESC desc;
    Check(SUCCEEDED(reflection->GetDesc(&desc)) && desc.BoundResources == 2, "Incorrect resource count");
    D3D12_SHADER_INPUT_BIND_DESC binding;
    Check(SUCCEEDED(reflection->GetResourceBindingDescByName("Params", &binding)) &&
          binding.Type == D3D_SIT_CBUFFER && binding.BindPoint == 2 && binding.Space == 1, "Incorrect CBV binding");
    Check(SUCCEEDED(reflection->GetResourceBindingDescByName("output_data", &binding)) &&
          binding.Type == D3D_SIT_UAV_RWSTRUCTURED && binding.BindPoint == 3 && binding.Space == 2, "Incorrect UAV binding");
    D3D12_SHADER_VARIABLE_DESC variable;
    Check(SUCCEEDED(reflection->GetConstantBufferByName("Params")->GetVariableByName("tint")->GetDesc(&variable)) &&
          variable.StartOffset == 0 && variable.Size == 16, "Incorrect constant layout");
    UINT x, y, z;
    Check(reflection->GetThreadGroupSize(&x, &y, &z) == 8 && x == 4 && y == 2 && z == 1, "Incorrect workgroup size");
    IDxcBlob* root = 0;
    Check(SUCCEEDED(dmShaderc::GetDXCRootSignature(object, &root)) && root->GetBufferSize() > 0, "Missing root signature");
    IDxcBlob* patched = 0;
    Check(SUCCEEDED(dmShaderc::SetDXCRootSignature(object->GetBufferPointer(), object->GetBufferSize(),
          root->GetBufferPointer(), root->GetBufferSize(), &patched)), "Root signature attachment failed");
    IDxcBlob* extracted = 0;
    Check(SUCCEEDED(dmShaderc::GetDXCRootSignature(patched, &extracted)) &&
          extracted->GetBufferSize() == root->GetBufferSize() &&
          memcmp(extracted->GetBufferPointer(), root->GetBufferPointer(), root->GetBufferSize()) == 0,
          "Root signature changed after attachment");
    extracted->Release();
    patched->Release();
    root->Release();
    reflection->Release();
    object->Release();
    if (errors) errors->Release();

    // All engine stages must produce SM6 bytecode, including when reflection is omitted.
    const char* stages[] = {"vs_6_0", "ps_6_0"};
    const char* sources[] = {
        "float4 main(float4 p : POSITION) : SV_Position { return p; }",
        "float4 main() : SV_Target { return float4(1, 0, 0, 1); }"};
    for (unsigned int i = 0; i < 2; ++i)
    {
        hr = dmShaderc::CompileDXC(sources[i], strlen(sources[i]), "main", stages[i], &object, &errors, 0);
        Check(SUCCEEDED(hr) && object, "Graphics stage compilation failed");
        object->Release();
        if (errors) errors->Release();
    }

    // Invalid HLSL must return diagnostics and no partially usable output.
    const char* invalid = "this is not HLSL";
    hr = dmShaderc::CompileDXC(invalid, strlen(invalid), "main", "ps_6_0", &object, &errors, &reflection);
    Check(FAILED(hr) && !object && !reflection && errors && errors->GetStringLength(), "Missing failure diagnostics");
    errors->Release();
    printf("DXC compilation, reflection, root extraction, and diagnostics passed\n");
    return 0;
}
