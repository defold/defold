// Copyright 2020-2023 The Defold Foundation
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

#include "graphics_dx12_private.h"

#if defined(DM_PLATFORM_VENDOR)
    #include "graphics_dx12_vendor.h"
#else
    #include <d3d12.h>
    #include <d3dx12.h>
#endif

namespace dmGraphics
{

static const char* RootParamTypeName(D3D12_ROOT_PARAMETER_TYPE t)
{
    switch (t)
    {
        case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:  return "DescriptorTable";
        case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:   return "32BitConstants";
        case D3D12_ROOT_PARAMETER_TYPE_CBV:               return "CBV";
        case D3D12_ROOT_PARAMETER_TYPE_SRV:               return "SRV";
        case D3D12_ROOT_PARAMETER_TYPE_UAV:               return "UAV";
        default: return "Unknown";
    }
}

static const char* ShaderVisName(D3D12_SHADER_VISIBILITY v)
{
    switch (v)
    {
        case D3D12_SHADER_VISIBILITY_ALL:     return "All";
        case D3D12_SHADER_VISIBILITY_VERTEX:  return "VS";
        case D3D12_SHADER_VISIBILITY_HULL:    return "HS";
        case D3D12_SHADER_VISIBILITY_DOMAIN:  return "DS";
        case D3D12_SHADER_VISIBILITY_GEOMETRY:return "GS";
        case D3D12_SHADER_VISIBILITY_PIXEL:   return "PS";
        default: return "Unknown";
    }
}

void DebugPrintRootSignature(const void* blob_ptr, size_t blob_size)
{
    if (!blob_ptr || blob_size == 0) {
        dmLogInfo("RootSig: <null>");
        return;
    }

    ID3D12RootSignatureDeserializer* deser = nullptr;
    HRESULT hr = D3D12CreateRootSignatureDeserializer(blob_ptr, blob_size, DM_IID_PPV_ARGS(&deser));
    if (FAILED(hr) || !deser)
    {
        dmLogInfo("RootSig: failed to deserialize (hr=0x%08x)", (unsigned)hr);
        return;
    }

    const D3D12_ROOT_SIGNATURE_DESC* desc = deser->GetRootSignatureDesc();
    dmLogInfo("RootSig: %u params, %u static samplers, flags=0x%08x", desc->NumParameters, desc->NumStaticSamplers, desc->Flags);

    for (UINT i = 0; i < desc->NumParameters; ++i)
    {
        const D3D12_ROOT_PARAMETER& p = desc->pParameters[i];
        dmLogInfo("  Param[%u]: %s, vis=%s", i, RootParamTypeName(p.ParameterType), ShaderVisName(p.ShaderVisibility));
        if (p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
        {
            dmLogInfo("    Constants: reg=%u, space=%u, count=%u", p.Constants.ShaderRegister, p.Constants.RegisterSpace, p.Constants.Num32BitValues);
        }
        else if (p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            dmLogInfo("    Table: %u ranges", p.DescriptorTable.NumDescriptorRanges);
            for (UINT r = 0; r < p.DescriptorTable.NumDescriptorRanges; ++r)
            {
                    const auto& rng = p.DescriptorTable.pDescriptorRanges[r];
                    const char* rangeType =
                            rng.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV ? "SRV" :
                            rng.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV ? "UAV" :
                            rng.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV ? "CBV" : "SAMPLER";
                    dmLogInfo("      Range[%u]: %s reg=%u space=%u num=%u offset=%u",
                                        r, rangeType, rng.BaseShaderRegister, rng.RegisterSpace, rng.NumDescriptors, rng.OffsetInDescriptorsFromTableStart);
            }
        }
        else
        {
            dmLogInfo("    Descriptor: reg=%u space=%u", p.Descriptor.ShaderRegister, p.Descriptor.RegisterSpace);
        }
    }

    for (UINT i = 0; i < desc->NumStaticSamplers; ++i)
    {
        const auto& s = desc->pStaticSamplers[i];
        dmLogInfo("  StaticSampler[%u]: reg=%u space=%u filter=%d addr=(%d,%d,%d) vis=%s",
                            i, s.ShaderRegister, s.RegisterSpace, s.Filter,
                            s.AddressU, s.AddressV, s.AddressW, ShaderVisName(s.ShaderVisibility));
    }

    deser->Release();
}

} // namespace
