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

#include "shaderc_rootsig.h"
#include "shaderc_private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <d3d12.h>
#endif

using namespace dmShaderc;
static void Check(bool condition, const char* message)
{
    if (!condition) { fprintf(stderr, "%s\n", message); exit(1); }
}
static void CopyBlob(IDxcBlob* blob, dmArray<uint8_t>& data)
{
    data.SetCapacity((uint32_t)blob->GetBufferSize());
    data.SetSize(data.Capacity());
    memcpy(data.Begin(), blob->GetBufferPointer(), data.Size());
}
static void Compile(const char* source, const char* profile, ShaderCompileResult& shader)
{
    IDxcBlob* object = 0;
    IDxcBlobUtf8* errors = 0;
    HRESULT hr = CompileDXC(source, strlen(source), "main", profile, &object, &errors, 0);
    if (errors && errors->GetStringLength()) fprintf(stderr, "%s", errors->GetStringPointer());
    Check(SUCCEEDED(hr), "Fixture compilation failed");
    IDxcBlob* root = 0;
    Check(SUCCEEDED(GetDXCRootSignature(object, &root)), "Missing root signature");
    CopyBlob(object, shader.m_Data); CopyBlob(root, shader.m_HLSLRootSignature);
    root->Release(); object->Release(); if (errors) errors->Release();
}

// Verifies real DXIL stage merging, resource remapping, arrays/constants/static
// samplers, platform recompilation text, and rejection of malformed root data.
int main(int argc, char** argv)
{
    const char* vertex =
        "cbuffer Params : register(b0,space1) { float4 color; };"
        "[RootSignature(\"CBV(b0,space=1,visibility=SHADER_VISIBILITY_VERTEX),RootConstants(num32BitConstants=4,b3,visibility=SHADER_VISIBILITY_VERTEX),RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)\")]"
        "float4 main(float4 p : POSITION) : SV_Position { return p + color; }";
    const char* fragment =
        "cbuffer Params : register(b0,space1) { float4 color; };"
        "Texture2D<float4> tex[4] : register(t2,space3); SamplerState smp : register(s1);"
        "[RootSignature(\"CBV(b0,space=1,visibility=SHADER_VISIBILITY_PIXEL),DescriptorTable(SRV(t2,space=3,numDescriptors=4,offset=2),visibility=SHADER_VISIBILITY_PIXEL),StaticSampler(s1,filter=FILTER_MIN_MAG_MIP_LINEAR,addressU=TEXTURE_ADDRESS_CLAMP,visibility=SHADER_VISIBILITY_PIXEL)\")]"
        "float4 main() : SV_Target { return tex[0].Sample(smp,float2(0,0)) * color; }";
    ShaderCompileResult shaders[2];
    Compile(vertex, "vs_6_0", shaders[0]); Compile(fragment, "ps_6_0", shaders[1]);
    for (uint32_t i = 0; i < 2; ++i)
    {
        shaders[i].m_HLSLResourceMappings.SetCapacity(i + 1);
        shaders[i].m_HLSLResourceMappings.SetSize(i + 1);
        memset(shaders[i].m_HLSLResourceMappings.Begin(), 0, (i + 1) * sizeof(HLSLResourceMapping));
    }
    shaders[1].m_HLSLResourceMappings[1].m_RootParameterIndex = 1;
    shaders[1].m_HLSLResourceMappings[1].m_ShaderResourceBinding = 2;
    HLSLRootSignature* merged = HLSLMergeRootSignatures(shaders, 2);
    Check(merged && !merged->m_LastError[0], "Merge failed");
    RootSignatureLayout layout;
    Check(SUCCEEDED(ReadRootSignature(merged->m_HLSLRootSignature.Begin(), merged->m_HLSLRootSignature.Size(), &layout)), "Cannot read merged signature");
    Check(layout.m_Desc.NumParameters == 3 && layout.m_Desc.NumStaticSamplers == 1, "Parameters lost or duplicated");
    Check(layout.m_Desc.pParameters[0].ShaderVisibility == RS_SHADER_VISIBILITY_ALL, "Shared visibility was not widened");
    Check(layout.m_Desc.pParameters[1].Constants.Num32BitValues == 4, "Root constants lost");
    const RS_DESCRIPTOR_RANGE& range = layout.m_Desc.pParameters[2].DescriptorTable.pDescriptorRanges[0];
    Check(range.NumDescriptors == 4 && range.OffsetInDescriptorsFromTableStart == 2 && range.RegisterSpace == 3, "Descriptor array layout changed");
    Check(shaders[1].m_HLSLResourceMappings[1].m_RootParameterIndex == 2, "Root mapping was not updated");
    dmArray<char> text;
    Check(RootSignatureBlobToText(merged->m_HLSLRootSignature.Begin(), merged->m_HLSLRootSignature.Size(), text), "Text conversion failed");
    const char* function = "float4 main(float4 p:POSITION):SV_Position{return p;}";
    size_t length = text.Size() + strlen(function);
    char* source = (char*) malloc(length);
    snprintf(source, length, "%s%s", text.Begin(), function);
    ShaderCompileResult recompiled;
    Compile(source, "vs_6_0", recompiled);
    free(source);
    RootSignatureLayout roundtrip;
    Check(SUCCEEDED(ReadRootSignature(recompiled.m_HLSLRootSignature.Begin(), recompiled.m_HLSLRootSignature.Size(), &roundtrip)), "Cannot read recompiled signature");
    IDxcBlob* normalized = 0; IDxcBlob* errors = 0;
    Check(SUCCEEDED(WriteRootSignature(&roundtrip.m_Desc, &normalized, &errors)), "Cannot normalize recompiled layout");
    Check(normalized->GetBufferSize() == merged->m_HLSLRootSignature.Size() &&
          memcmp(normalized->GetBufferPointer(), merged->m_HLSLRootSignature.Begin(), normalized->GetBufferSize()) == 0,
          "Platform recompilation text changed the root layout");
    normalized->Release(); if (errors) errors->Release();
    for (uint32_t n = 0; n < merged->m_HLSLRootSignature.Size(); ++n)
    {
        RootSignatureLayout truncated;
        Check(FAILED(ReadRootSignature(merged->m_HLSLRootSignature.Begin(), n, &truncated)), "Accepted truncated root container");
    }
    // The SDK remains an independent Windows oracle, never a production dependency.
#if defined(_WIN32)
    ID3D12RootSignatureDeserializer* oracle = 0;
    Check(SUCCEEDED(D3D12CreateRootSignatureDeserializer(merged->m_HLSLRootSignature.Begin(), merged->m_HLSLRootSignature.Size(), IID_PPV_ARGS(&oracle))), "Windows rejected portable signature");
    const D3D12_ROOT_SIGNATURE_DESC* sdk_desc = oracle->GetRootSignatureDesc();
    Check(sdk_desc->NumParameters == 3 && sdk_desc->NumStaticSamplers == 1 &&
          sdk_desc->pParameters[1].Constants.Num32BitValues == 4 &&
          sdk_desc->pParameters[2].DescriptorTable.pDescriptorRanges[0].NumDescriptors == 4 &&
          sdk_desc->pParameters[2].DescriptorTable.pDescriptorRanges[0].OffsetInDescriptorsFromTableStart == 2 &&
          sdk_desc->pStaticSamplers[0].Filter == D3D12_FILTER_MIN_MAG_MIP_LINEAR, "SDK layout differs");
    oracle->Release();
#endif
    // DXC must reject overlapping bindings instead of emitting an invalid layout.
    RS_ROOT_PARAMETER overlapping[2] = {layout.m_Desc.pParameters[0], layout.m_Desc.pParameters[0]};
    RS_ROOT_SIGNATURE_DESC invalid = layout.m_Desc;
    invalid.NumParameters = 2; invalid.pParameters = overlapping;
    normalized = 0; errors = 0;
    Check(FAILED(WriteRootSignature(&invalid, &normalized, &errors)) && !normalized && errors && errors->GetBufferSize(), "Accepted overlapping bindings");
    errors->Release();
    // An offset outside the RTS0 payload must fail before following any pointer.
    dmArray<uint8_t> corrupt;
    corrupt.SetCapacity(24); corrupt.SetSize(24); memset(corrupt.Begin(), 0, 24);
    corrupt[0] = 1; corrupt[4] = 1; memset(corrupt.Begin() + 8, 255, 4);
    RootSignatureLayout bad_offset;
    Check(FAILED(ReadRootSignature(corrupt.Begin(), corrupt.Size(), &bad_offset)), "Accepted out-of-range offset");
    if (argc > 1)
    {
        FILE* file = fopen(argv[1], "wb");
        Check(file && fwrite(merged->m_HLSLRootSignature.Begin(), 1, merged->m_HLSLRootSignature.Size(), file) == merged->m_HLSLRootSignature.Size(), "Cannot save cross-host fixture");
        fclose(file);
    }
    HLSLRootSignature* again = HLSLMergeRootSignatures(shaders, 2);
    Check(again && !again->m_LastError[0] && shaders[1].m_HLSLResourceMappings[1].m_RootParameterIndex == 2, "Repeated merge changed mapping");
    delete again;
    shaders[1].m_HLSLResourceMappings[0].m_ShaderResourceBinding = 99;
    HLSLRootSignature* conflict = HLSLMergeRootSignatures(shaders, 2);
    Check(conflict && conflict->m_LastError[0], "Accepted conflicting resource mapping");
    delete conflict; delete merged;
    printf("Portable root merge, mapping, text round-trip, and malformed input tests passed\n");
    return 0;
}
