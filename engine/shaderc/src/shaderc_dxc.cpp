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
#include <stdlib.h>
#include <string.h>

#define RELEASE_IF_EXISTS(b) \
    if (b) \
    { \
        (b)->Release(); \
    }

// Preserve COM cleanup when an operation fails after earlier allocations.
#define BAIL_WHEN_FAIL(hr) \
    if (FAILED(hr)) \
    { \
        goto cleanup; \
    }

namespace dmShaderc
{
    bool HasDXBCHeader(const void* data, size_t size)
    {
        return size >= 4 && memcmp(data, "DXBC", 4) == 0;
    }

    HRESULT CompileDXC(const void* source, size_t size, const char* entry, const char* profile,
                       IDxcBlob** object, IDxcBlobUtf8** errors, ID3D12ShaderReflection** reflection)
    {
        *object = 0;
        *errors = 0;

        if (reflection)
        {
            *reflection = 0;
        }

        IDxcUtils* utils = 0;
        IDxcCompiler3* compiler = 0;
        IDxcResult* result = 0;
        IDxcBlobEncoding* entry_blob = 0;
        IDxcBlobEncoding* profile_blob = 0;
        IDxcBlobWide* wide_entry = 0;
        IDxcBlobWide* wide_profile = 0;
        IDxcBlob* reflection_blob = 0;
        DxcBuffer buffer = {source, size, DXC_CP_UTF8};
        HRESULT status = E_FAIL;
        HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));

        BAIL_WHEN_FAIL(hr);
        hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
        BAIL_WHEN_FAIL(hr);
        hr = utils->CreateBlob(entry, (UINT32) strlen(entry), DXC_CP_UTF8, &entry_blob);
        BAIL_WHEN_FAIL(hr);
        hr = utils->GetBlobAsWide(entry_blob, &wide_entry);
        BAIL_WHEN_FAIL(hr);
        hr = utils->CreateBlob(profile, (UINT32) strlen(profile), DXC_CP_UTF8, &profile_blob);
        BAIL_WHEN_FAIL(hr);
        hr = utils->GetBlobAsWide(profile_blob, &wide_profile);
        BAIL_WHEN_FAIL(hr);

        {
            // Keep generated HLSL's historical language semantics, including
            // vector conditionals. HLSL 2021 changes those independently of SM6.
            const wchar_t* args[] = {L"-E", wide_entry->GetStringPointer(),
                L"-T", wide_profile->GetStringPointer(), L"-HV", L"2018", L"-Ges"};
            hr = compiler->Compile(&buffer, args, sizeof(args) / sizeof(args[0]), 0, IID_PPV_ARGS(&result));
        }
        BAIL_WHEN_FAIL(hr);

        result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors), 0);
        hr = result->GetStatus(&status);
        BAIL_WHEN_FAIL(hr);
        hr = status;
        BAIL_WHEN_FAIL(hr);

        hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(object), 0);
        BAIL_WHEN_FAIL(hr);

        if (reflection)
        {
            hr = result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflection_blob), 0);
            BAIL_WHEN_FAIL(hr);
            DxcBuffer data = {reflection_blob->GetBufferPointer(), reflection_blob->GetBufferSize(), 0};
            hr = utils->CreateReflection(&data, IID_PPV_ARGS(reflection));
        }

    cleanup:
        if (FAILED(hr))
        {
            RELEASE_IF_EXISTS(*object);
            *object = 0;
        }

        RELEASE_IF_EXISTS(reflection_blob);
        RELEASE_IF_EXISTS(wide_profile);
        RELEASE_IF_EXISTS(wide_entry);
        RELEASE_IF_EXISTS(profile_blob);
        RELEASE_IF_EXISTS(entry_blob);
        RELEASE_IF_EXISTS(result);
        RELEASE_IF_EXISTS(compiler);
        RELEASE_IF_EXISTS(utils);

        return hr;
    }

    HRESULT GetHLSLShaderOutputs(ID3D12ShaderReflection* reflection, dmArray<HLSLShaderOutput>& outputs)
    {
        D3D12_SHADER_DESC desc;
        HRESULT hr = reflection->GetDesc(&desc);
        if (FAILED(hr))
        {
            return hr;
        }
        outputs.SetCapacity(desc.OutputParameters);
        outputs.SetSize(0);
        for (uint32_t i = 0; i < desc.OutputParameters; ++i)
        {
            D3D12_SIGNATURE_PARAMETER_DESC parameter;
            hr = reflection->GetOutputParameterDesc(i, &parameter);
            if (FAILED(hr))
            {
                return hr;
            }
            if (parameter.SystemValueType != D3D_NAME_TARGET)
            {
                continue;
            }
            HLSLShaderOutput output;
            // SV_TargetN identifies the render-target slot, including sparse outputs.
            output.m_Location = parameter.SemanticIndex;
            output.m_ComponentCount = ((parameter.Mask & 1) != 0) + ((parameter.Mask & 2) != 0) +
                                      ((parameter.Mask & 4) != 0) + ((parameter.Mask & 8) != 0);
            switch (parameter.ComponentType)
            {
                case D3D_REGISTER_COMPONENT_UINT32:
                    output.m_Type = BASE_TYPE_UINT32;
                    break;
                case D3D_REGISTER_COMPONENT_SINT32:
                    output.m_Type = BASE_TYPE_INT32;
                    break;
                case D3D_REGISTER_COMPONENT_FLOAT32:
                    output.m_Type = BASE_TYPE_FP32;
                    break;
                default:
                    return E_INVALIDARG;
            }
            outputs.Push(output);
        }
        return S_OK;
    }

    // Loads a DXC container and extracts its RTS0 part: the serialized root-signature
    // payload without the container wrapper. Used when attaching a signature to a
    // shader or wrapping an extracted signature in its own container. The caller
    // owns the returned blob and must release it; a missing RTS0 part is an error.
    static HRESULT GetRootSignaturePart(IDxcBlob* object, IDxcBlob** root_signature)
    {
        *root_signature = 0;
        IDxcContainerReflection* container = 0;
        UINT32 index = 0;

        const UINT32 root_signature_kind = 'R' | ('T' << 8) | ('S' << 16) | ('0' << 24);
        HRESULT hr = DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&container));

        BAIL_WHEN_FAIL(hr);
        hr = container->Load(object);
        BAIL_WHEN_FAIL(hr);
        hr = container->FindFirstPartKind(root_signature_kind, &index);
        BAIL_WHEN_FAIL(hr);
        hr = container->GetPartContent(index, root_signature);

    cleanup:
        RELEASE_IF_EXISTS(container);
        return hr;
    }

    HRESULT SetDXCRootSignature(const void* object, size_t object_size, const void* root, size_t root_size, IDxcBlob** output)
    {
        *output = 0;
        IDxcUtils* utils = 0;
        IDxcContainerBuilder* builder = 0;
        IDxcBlobEncoding* object_blob = 0;
        IDxcBlobEncoding* root_blob = 0;
        IDxcBlob* root_part = 0;
        IDxcOperationResult* result = 0;

        HRESULT status = E_FAIL;
        const UINT32 kind = 'R' | ('T' << 8) | ('S' << 16) | ('0' << 24);
        HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));

        BAIL_WHEN_FAIL(hr);
        hr = DxcCreateInstance(CLSID_DxcContainerBuilder, IID_PPV_ARGS(&builder));
        BAIL_WHEN_FAIL(hr);
        hr = utils->CreateBlob(object, (UINT32) object_size, 0, &object_blob);
        BAIL_WHEN_FAIL(hr);
        hr = utils->CreateBlob(root, (UINT32) root_size, 0, &root_blob);
        BAIL_WHEN_FAIL(hr);

        if (HasDXBCHeader(root, root_size))
        {
            hr = GetRootSignaturePart(root_blob, &root_part);
            BAIL_WHEN_FAIL(hr);
        }
        else
        {
            root_part = root_blob;
            root_part->AddRef();
        }

        hr = builder->Load(object_blob);
        BAIL_WHEN_FAIL(hr);

        // Removing an absent part is harmless; AddPart still reports other failures.
        builder->RemovePart(kind);
        hr = builder->AddPart(kind, root_part);
        BAIL_WHEN_FAIL(hr);
        hr = builder->SerializeContainer(&result);
        BAIL_WHEN_FAIL(hr);
        hr = result->GetStatus(&status);
        BAIL_WHEN_FAIL(hr);
        hr = status;
        BAIL_WHEN_FAIL(hr);
        hr = result->GetResult(output);

    cleanup:
        RELEASE_IF_EXISTS(result);
        RELEASE_IF_EXISTS(root_part);
        RELEASE_IF_EXISTS(root_blob);
        RELEASE_IF_EXISTS(object_blob);
        RELEASE_IF_EXISTS(builder);
        RELEASE_IF_EXISTS(utils);
        return hr;
    }

    HRESULT GetDXCRootSignature(IDxcBlob* object, IDxcBlob** root_signature)
    {
        *root_signature = 0;
        IDxcBlob* part = 0;
        IDxcContainerBuilder* builder = 0;
        IDxcOperationResult* result = 0;
        HRESULT status = E_FAIL;
        const UINT32 kind = 'R' | ('T' << 8) | ('S' << 16) | ('0' << 24);
        HRESULT hr = GetRootSignaturePart(object, &part);

        BAIL_WHEN_FAIL(hr);
        hr = DxcCreateInstance(CLSID_DxcContainerBuilder, IID_PPV_ARGS(&builder));
        BAIL_WHEN_FAIL(hr);
        hr = builder->AddPart(kind, part);
        BAIL_WHEN_FAIL(hr);
        hr = builder->SerializeContainer(&result);
        BAIL_WHEN_FAIL(hr);
        hr = result->GetStatus(&status);
        BAIL_WHEN_FAIL(hr);
        hr = status;
        BAIL_WHEN_FAIL(hr);
        hr = result->GetResult(root_signature);

    cleanup:
        RELEASE_IF_EXISTS(result);
        RELEASE_IF_EXISTS(builder);
        RELEASE_IF_EXISTS(part);
        return hr;
    }
}

#undef RELEASE_IF_EXISTS
#undef BAIL_WHEN_FAIL
