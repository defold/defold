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

#if defined(DM_PLATFORM_XBOX)
    #include "graphics_dx12_xbox.h"
#else
    #include <d3d12.h>
    #include <d3dx12.h>
#endif

namespace dmGraphics
{

void InitializeTextureResourceStates(ID3D12Device* device, DX12Texture* texture, D3D12_RESOURCE_STATES state)
{
    texture->m_ResourceDesc = texture->m_Resource->GetDesc();
    const D3D12_RESOURCE_DESC& desc = texture->m_ResourceDesc;
    const uint32_t layers = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1 : desc.DepthOrArraySize;
    const uint32_t planes = D3D12GetFormatPlaneCount(device, desc.Format);
    assert(planes > 0);
    const uint32_t count = desc.MipLevels * layers * planes;
    texture->m_ResourceStates.SetCapacity(count);
    texture->m_ResourceStates.SetSize(count);
    for (uint32_t i = 0; i < count; ++i)
        texture->m_ResourceStates[i] = state;
}

void TransitionTexture(ID3D12GraphicsCommandList* commands, DX12Texture* texture, D3D12_RESOURCE_STATES state, uint32_t first, uint32_t count)
{
    assert(first <= texture->m_ResourceStates.Size());
    count = dmMath::Min(count, texture->m_ResourceStates.Size() - first);
    // Bound the temporary array independently of the resource's layer/mip/plane count.
    D3D12_RESOURCE_BARRIER barriers[32];
    uint32_t barrier_count = 0;
    for (uint32_t i = first; i < first + count; ++i)
    {
        if (texture->m_ResourceStates[i] == state)
            continue;
        barriers[barrier_count++] = CD3DX12_RESOURCE_BARRIER::Transition(texture->m_Resource, texture->m_ResourceStates[i], state, i);
        texture->m_ResourceStates[i] = state;
        if (barrier_count == DM_ARRAY_SIZE(barriers))
        {
            commands->ResourceBarrier(barrier_count, barriers);
            barrier_count = 0;
        }
    }
    if (barrier_count)
        commands->ResourceBarrier(barrier_count, barriers);
}

static D3D12_COMPARISON_FUNC GetCompareFunc(uint32_t func)
{
    const D3D12_COMPARISON_FUNC funcs[] = {
        D3D12_COMPARISON_FUNC_NEVER, D3D12_COMPARISON_FUNC_LESS,
        D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_COMPARISON_FUNC_GREATER,
        D3D12_COMPARISON_FUNC_GREATER_EQUAL, D3D12_COMPARISON_FUNC_EQUAL,
        D3D12_COMPARISON_FUNC_NOT_EQUAL, D3D12_COMPARISON_FUNC_ALWAYS
    };
    assert(func < DM_ARRAY_SIZE(funcs));
    return funcs[func];
}

static D3D12_STENCIL_OP GetStencilOp(uint32_t op)
{
    const D3D12_STENCIL_OP ops[] = {
        D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_ZERO, D3D12_STENCIL_OP_REPLACE,
        D3D12_STENCIL_OP_INCR_SAT, D3D12_STENCIL_OP_INCR,
        D3D12_STENCIL_OP_DECR_SAT, D3D12_STENCIL_OP_DECR, D3D12_STENCIL_OP_INVERT
    };
    assert(op < DM_ARRAY_SIZE(ops));
    return ops[op];
}

D3D12_DEPTH_STENCIL_DESC GetDepthStencilState(const PipelineState& state)
{
    D3D12_DEPTH_STENCIL_DESC desc = {};
    desc.DepthEnable = state.m_DepthTestEnabled;
    desc.DepthWriteMask = state.m_WriteDepth ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthFunc = state.m_DepthTestEnabled ? GetCompareFunc(state.m_DepthTestFunc) : D3D12_COMPARISON_FUNC_ALWAYS;
    desc.StencilEnable = state.m_StencilEnabled;
    desc.StencilReadMask = state.m_StencilCompareMask;
    desc.StencilWriteMask = state.m_StencilWriteMask;
    desc.FrontFace.StencilFailOp = GetStencilOp(state.m_StencilFrontOpFail);
    desc.FrontFace.StencilDepthFailOp = GetStencilOp(state.m_StencilFrontOpDepthFail);
    desc.FrontFace.StencilPassOp = GetStencilOp(state.m_StencilFrontOpPass);
    desc.FrontFace.StencilFunc = GetCompareFunc(state.m_StencilFrontTestFunc);
    desc.BackFace.StencilFailOp = GetStencilOp(state.m_StencilBackOpFail);
    desc.BackFace.StencilDepthFailOp = GetStencilOp(state.m_StencilBackOpDepthFail);
    desc.BackFace.StencilPassOp = GetStencilOp(state.m_StencilBackOpPass);
    desc.BackFace.StencilFunc = GetCompareFunc(state.m_StencilBackTestFunc);
    return desc;
}

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
