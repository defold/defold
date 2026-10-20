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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace dmShaderc
{
    RootSignatureLayout::RootSignatureLayout() { memset(&m_Desc, 0, sizeof(m_Desc)); }
    RootSignatureLayout::~RootSignatureLayout()
    {
        for (UINT i = 0; i < m_Desc.NumParameters; ++i)
            if (m_Desc.pParameters[i].ParameterType == RS_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
                free((void*) m_Desc.pParameters[i].DescriptorTable.pDescriptorRanges);
        free((void*) m_Desc.pParameters);
        free((void*) m_Desc.pStaticSamplers);
    }

    static uint32_t Read32(const uint8_t* p)
    {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }
    static bool Fits(size_t size, uint32_t offset, uint32_t count, uint32_t stride)
    {
        return offset <= size && count <= (size - offset) / stride;
    }

    HRESULT ReadRootSignature(const void* bytes, size_t size, RootSignatureLayout* layout)
    {
        if (!bytes || layout->m_Desc.pParameters || layout->m_Desc.pStaticSamplers)
            return E_INVALIDARG;
        const uint8_t* data = (const uint8_t*) bytes;
        // The public DXC DxilRootSignature format uses little-endian, relative
        // offsets, never native pointers. Check every extent before allocation.
        if (size >= 4 && memcmp(data, "DXBC", 4) == 0)
        {
            if (size < 32 || Read32(data + 24) != size || !Fits(size, 32, Read32(data + 28), 4))
                return E_INVALIDARG;
            const uint8_t* part = 0;
            uint32_t part_size = 0;
            for (uint32_t i = 0; i < Read32(data + 28); ++i)
            {
                uint32_t offset = Read32(data + 32 + i * 4);
                if (!Fits(size, offset, 1, 8) || !Fits(size, offset + 8, Read32(data + offset + 4), 1))
                    return E_INVALIDARG;
                if (memcmp(data + offset, "RTS0", 4) == 0)
                {
                    if (part) return E_INVALIDARG;
                    part = data + offset + 8;
                    part_size = Read32(data + offset + 4);
                }
            }
            if (!part) return E_INVALIDARG;
            data = part; size = part_size;
        }
        if (size < 24) return E_INVALIDARG;
        uint32_t version = Read32(data);
        uint32_t count = Read32(data + 4), offset = Read32(data + 8);
        uint32_t samplers = Read32(data + 12), sampler_offset = Read32(data + 16);
        if ((version != 1 && version != 2) || count > 64 ||
            !Fits(size, offset, count, 12) || !Fits(size, sampler_offset, samplers, 52))
            return E_INVALIDARG;
        RS_ROOT_PARAMETER* params = (RS_ROOT_PARAMETER*) calloc(count, sizeof(*params));
        if (count && !params) return E_OUTOFMEMORY;
        layout->m_Desc.pParameters = params;
        layout->m_Desc.NumParameters = count;
        layout->m_Desc.Flags = (RS_ROOT_SIGNATURE_FLAGS) Read32(data + 20);
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint8_t* p = data + offset + 12 * i;
            RS_ROOT_PARAMETER& param = params[i];
            uint32_t type = Read32(p), visibility = Read32(p + 4), payload = Read32(p + 8);
            if (type > 4 || visibility > 7) return E_INVALIDARG;
            param.ParameterType = (RS_ROOT_PARAMETER_TYPE) type;
            param.ShaderVisibility = (RS_SHADER_VISIBILITY) visibility;
            if (!Fits(size, payload, 1, type == 1 || (version == 2 && type >= 2) ? 12 : 8))
                return E_INVALIDARG;
            const uint8_t* values = data + payload;
            if (type == 0)
            {
                uint32_t n = Read32(values), start = Read32(values + 4), stride = version == 1 ? 20 : 24;
                if (!Fits(size, start, n, stride)) return E_INVALIDARG;
                RS_DESCRIPTOR_RANGE* ranges = (RS_DESCRIPTOR_RANGE*) calloc(n, sizeof(*ranges));
                if (n && !ranges) return E_OUTOFMEMORY;
                param.DescriptorTable.pDescriptorRanges = ranges;
                param.DescriptorTable.NumDescriptorRanges = n;
                for (uint32_t r = 0; r < n; ++r)
                {
                    const uint8_t* v = data + start + r * stride;
                    if (Read32(v) > 3) return E_INVALIDARG;
                    ranges[r].RangeType = (RS_DESCRIPTOR_RANGE_TYPE) Read32(v);
                    ranges[r].NumDescriptors = Read32(v + 4);
                    ranges[r].BaseShaderRegister = Read32(v + 8);
                    ranges[r].RegisterSpace = Read32(v + 12);
                    ranges[r].OffsetInDescriptorsFromTableStart = Read32(v + stride - 4);
                }
            }
            else if (type == 1)
            {
                param.Constants.ShaderRegister = Read32(values);
                param.Constants.RegisterSpace = Read32(values + 4);
                param.Constants.Num32BitValues = Read32(values + 8);
            }
            else
            {
                param.Descriptor.ShaderRegister = Read32(values);
                param.Descriptor.RegisterSpace = Read32(values + 4);
            }
        }
        RS_STATIC_SAMPLER_DESC* ss = (RS_STATIC_SAMPLER_DESC*) calloc(samplers, sizeof(*ss));
        if (samplers && !ss) return E_OUTOFMEMORY;
        layout->m_Desc.pStaticSamplers = ss;
        layout->m_Desc.NumStaticSamplers = samplers;
        // The public static-sampler structure contains thirteen 32-bit fields.
        // Copy through integers so float bits and enum values round-trip exactly.
        for (uint32_t i = 0; i < samplers; ++i)
            for (uint32_t j = 0; j < 13; ++j)
            {
                uint32_t value = Read32(data + sampler_offset + i * 52 + j * 4);
                memcpy((uint8_t*)&ss[i] + j * 4, &value, 4);
            }
        return S_OK;
    }

    HRESULT WriteRootSignature(const RS_ROOT_SIGNATURE_DESC* desc, IDxcBlob** blob, IDxcBlob** errors)
    {
        *blob = 0; *errors = 0;
        if (desc->NumParameters > 64 || (desc->NumParameters && !desc->pParameters) ||
            (desc->NumStaticSamplers && !desc->pStaticSamplers)) return E_INVALIDARG;
        dmArray<char> text;
        if (!RootSignatureDescToText(desc, text)) return E_INVALIDARG;
        // Compile the root definition itself, rather than a dummy shader. DXC
        // validates register overlaps, descriptor ranges, samplers and DWORD
        // limits and produces the standard version 1.0 serialized container.
        const char* prefix = "#define main ";
        const size_t attribute_prefix = strlen("[RootSignature(");
        const size_t definition_length = text.Size() - 1 - attribute_prefix - 2;
        size_t length = strlen(prefix) + definition_length;
        char* source = (char*) malloc(length + 1);
        if (!source) return E_OUTOFMEMORY;
        memcpy(source, prefix, strlen(prefix));
        memcpy(source + strlen(prefix), text.Begin() + attribute_prefix, definition_length);
        source[length] = 0;
        IDxcBlobUtf8* diagnostics = 0;
        HRESULT hr = CompileDXC(source, length, "main", "rootsig_1_0", blob, &diagnostics, 0);
        *errors = diagnostics;
        free(source);
        return hr;
    }
}
