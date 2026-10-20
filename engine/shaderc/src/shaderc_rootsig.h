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

#ifndef DM_SHADERC_ROOTSIG_H
#define DM_SHADERC_ROOTSIG_H
#include "shaderc_dxc.h"
#include <stdint.h>
#include <dmsdk/dlib/array.h>

namespace dmShaderc
{
    // Numeric values are the public DXIL root-signature wire format. These
    // layout types intentionally have no dependency on D3D12 device interfaces.
    enum RS_DESCRIPTOR_RANGE_TYPE { RS_DESCRIPTOR_RANGE_TYPE_SRV, RS_DESCRIPTOR_RANGE_TYPE_UAV, RS_DESCRIPTOR_RANGE_TYPE_CBV, RS_DESCRIPTOR_RANGE_TYPE_SAMPLER };
    enum RS_ROOT_PARAMETER_TYPE { RS_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, RS_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS, RS_ROOT_PARAMETER_TYPE_CBV, RS_ROOT_PARAMETER_TYPE_SRV, RS_ROOT_PARAMETER_TYPE_UAV };
    enum RS_SHADER_VISIBILITY { RS_SHADER_VISIBILITY_ALL, RS_SHADER_VISIBILITY_VERTEX, RS_SHADER_VISIBILITY_HULL, RS_SHADER_VISIBILITY_DOMAIN, RS_SHADER_VISIBILITY_GEOMETRY, RS_SHADER_VISIBILITY_PIXEL, RS_SHADER_VISIBILITY_AMPLIFICATION, RS_SHADER_VISIBILITY_MESH };
    typedef uint32_t RS_ROOT_SIGNATURE_FLAGS;
    const uint32_t RS_DESCRIPTOR_RANGE_OFFSET_APPEND = 0xffffffff;
    const RS_ROOT_SIGNATURE_FLAGS RS_ROOT_SIGNATURE_FLAG_NONE = 0;
    const RS_ROOT_SIGNATURE_FLAGS RS_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT = 1;
    const RS_ROOT_SIGNATURE_FLAGS RS_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS = 2;
    const RS_ROOT_SIGNATURE_FLAGS RS_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS = 4;
    const RS_ROOT_SIGNATURE_FLAGS RS_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS = 8;
    const RS_ROOT_SIGNATURE_FLAGS RS_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS = 16;
    const RS_ROOT_SIGNATURE_FLAGS RS_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS = 32;
    const RS_ROOT_SIGNATURE_FLAGS RS_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT = 64;
    struct RS_DESCRIPTOR_RANGE
    {
        RS_DESCRIPTOR_RANGE_TYPE RangeType;
        uint32_t NumDescriptors, BaseShaderRegister, RegisterSpace, OffsetInDescriptorsFromTableStart;
    };
    struct RS_ROOT_PARAMETER
    {
        RS_ROOT_PARAMETER_TYPE ParameterType;
        union
        {
            struct { uint32_t NumDescriptorRanges; const RS_DESCRIPTOR_RANGE* pDescriptorRanges; } DescriptorTable;
            struct { uint32_t ShaderRegister, RegisterSpace, Num32BitValues; } Constants;
            struct { uint32_t ShaderRegister, RegisterSpace; } Descriptor;
        };
        RS_SHADER_VISIBILITY ShaderVisibility;
    };
    struct RS_STATIC_SAMPLER_DESC
    {
        uint32_t Filter, AddressU, AddressV, AddressW;
        float MipLODBias;
        uint32_t MaxAnisotropy, ComparisonFunc, BorderColor;
        float MinLOD, MaxLOD;
        uint32_t ShaderRegister, RegisterSpace;
        RS_SHADER_VISIBILITY ShaderVisibility;
    };
    struct RS_ROOT_SIGNATURE_DESC
    {
        uint32_t NumParameters;
        const RS_ROOT_PARAMETER* pParameters;
        uint32_t NumStaticSamplers;
        const RS_STATIC_SAMPLER_DESC* pStaticSamplers;
        RS_ROOT_SIGNATURE_FLAGS Flags;
    };
    // Owns a version 1.0 layout. Version 1.1 input is conservatively normalized
    // to 1.0 (volatile descriptors/data), matching the previous SDK conversion.
    struct RootSignatureLayout
    {
        RS_ROOT_SIGNATURE_DESC m_Desc;
        RootSignatureLayout();
        ~RootSignatureLayout();
    private:
        RootSignatureLayout(const RootSignatureLayout&);
        RootSignatureLayout& operator=(const RootSignatureLayout&);
    };
    HRESULT ReadRootSignature(const void* data, size_t size, RootSignatureLayout* layout);
    bool RootSignatureDescToText(const RS_ROOT_SIGNATURE_DESC* desc, dmArray<char>& text);
    HRESULT WriteRootSignature(const RS_ROOT_SIGNATURE_DESC* desc, IDxcBlob** blob, IDxcBlob** errors);
}
#endif
