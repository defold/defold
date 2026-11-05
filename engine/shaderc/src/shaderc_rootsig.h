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
    enum RS_DESCRIPTOR_RANGE_TYPE
    {
        RS_DESCRIPTOR_RANGE_TYPE_SRV     = 0,
        RS_DESCRIPTOR_RANGE_TYPE_UAV     = 1,
        RS_DESCRIPTOR_RANGE_TYPE_CBV     = 2,
        RS_DESCRIPTOR_RANGE_TYPE_SAMPLER = 3
    };

    enum RS_ROOT_PARAMETER_TYPE
    {
        RS_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE = 0,
        RS_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS  = 1,
        RS_ROOT_PARAMETER_TYPE_CBV              = 2,
        RS_ROOT_PARAMETER_TYPE_SRV              = 3,
        RS_ROOT_PARAMETER_TYPE_UAV              = 4
    };

    enum RS_SHADER_VISIBILITY
    {
        RS_SHADER_VISIBILITY_ALL           = 0,
        RS_SHADER_VISIBILITY_VERTEX        = 1,
        RS_SHADER_VISIBILITY_HULL          = 2,
        RS_SHADER_VISIBILITY_DOMAIN        = 3,
        RS_SHADER_VISIBILITY_GEOMETRY      = 4,
        RS_SHADER_VISIBILITY_PIXEL         = 5,
        RS_SHADER_VISIBILITY_AMPLIFICATION = 6,
        RS_SHADER_VISIBILITY_MESH          = 7
    };

    enum RS_ROOT_SIGNATURE_FLAGS
    {
        RS_ROOT_SIGNATURE_FLAG_NONE                                  = 0,
        RS_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT    = 1,
        RS_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS        = 2,
        RS_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS          = 4,
        RS_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS        = 8,
        RS_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS      = 16,
        RS_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS         = 32,
        RS_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT                   = 64,
        RS_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE                  = 128,
        RS_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS = 256,
        RS_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS          = 512,
        RS_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED     = 1024,
        RS_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED         = 2048
    };

    static const uint32_t RS_DESCRIPTOR_RANGE_OFFSET_APPEND = 0xffffffff;

    struct RS_DESCRIPTOR_RANGE
    {
        RS_DESCRIPTOR_RANGE_TYPE m_RangeType;
        uint32_t                 m_NumDescriptors;
        uint32_t                 m_BaseShaderRegister;
        uint32_t                 m_RegisterSpace;
        uint32_t                 m_OffsetInDescriptorsFromTableStart;
    };

    struct RS_ROOT_PARAMETER
    {
        RS_ROOT_PARAMETER_TYPE m_ParameterType;
        union
        {
            struct
            {
                uint32_t                   m_NumDescriptorRanges;
                const RS_DESCRIPTOR_RANGE* m_DescriptorRanges;
            } m_DescriptorTable;
            struct
            {
                uint32_t                   m_ShaderRegister;
                uint32_t                   m_RegisterSpace;
                uint32_t                   m_Num32BitValues;
            } m_Constants;
            struct
            {
                uint32_t                   m_ShaderRegister;
                uint32_t                   m_RegisterSpace;
            } m_Descriptor;
        };
        RS_SHADER_VISIBILITY   m_ShaderVisibility;
    };

    struct RS_STATIC_SAMPLER_DESC
    {
        uint32_t             m_Filter;
        uint32_t             m_AddressU;
        uint32_t             m_AddressV;
        uint32_t             m_AddressW;
        float                m_MipLODBias;
        uint32_t             m_MaxAnisotropy;
        uint32_t             m_ComparisonFunc;
        uint32_t             m_BorderColor;
        float                m_MinLOD;
        float                m_MaxLOD;
        uint32_t             m_ShaderRegister;
        uint32_t             m_RegisterSpace;
        RS_SHADER_VISIBILITY m_ShaderVisibility;
    };

    struct RS_ROOT_SIGNATURE_DESC
    {
        uint32_t                      m_NumParameters;
        const RS_ROOT_PARAMETER*      m_Parameters;
        uint32_t                      m_NumStaticSamplers;
        const RS_STATIC_SAMPLER_DESC* m_StaticSamplers;
        RS_ROOT_SIGNATURE_FLAGS       m_Flags;
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
    bool    RootSignatureDescToText(const RS_ROOT_SIGNATURE_DESC* desc, dmArray<char>& text);
    HRESULT WriteRootSignature(const RS_ROOT_SIGNATURE_DESC* desc, IDxcBlob** blob, IDxcBlob** errors);
}
#endif
