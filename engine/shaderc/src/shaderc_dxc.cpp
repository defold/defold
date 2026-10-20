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

namespace dmShaderc
{
    HRESULT CompileDXC(const void* source, size_t size, const char* entry, const char* profile,
                       IDxcBlob** object, IDxcBlobUtf8** errors, ID3D12ShaderReflection** reflection)
    {
        *object = 0;
        *errors = 0;
        if (reflection)
            *reflection = 0;
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
        if (SUCCEEDED(hr))
            hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
        if (SUCCEEDED(hr))
            hr = utils->CreateBlob(entry, (UINT32) strlen(entry), DXC_CP_UTF8, &entry_blob);
        if (SUCCEEDED(hr))
            hr = utils->GetBlobAsWide(entry_blob, &wide_entry);
        if (SUCCEEDED(hr))
            hr = utils->CreateBlob(profile, (UINT32) strlen(profile), DXC_CP_UTF8, &profile_blob);
        if (SUCCEEDED(hr))
            hr = utils->GetBlobAsWide(profile_blob, &wide_profile);
        if (SUCCEEDED(hr))
        {
            // Keep generated HLSL's historical language semantics, including
            // vector conditionals. HLSL 2021 changes those independently of SM6.
            const wchar_t* args[] = {L"-E", wide_entry->GetStringPointer(),
                L"-T", wide_profile->GetStringPointer(), L"-HV", L"2018", L"-Ges"};
            hr = compiler->Compile(&buffer, args, sizeof(args) / sizeof(args[0]), 0, IID_PPV_ARGS(&result));
        }
        if (SUCCEEDED(hr))
        {
            result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors), 0);
            hr = result->GetStatus(&status);
            if (SUCCEEDED(hr))
                hr = status;
        }
        if (SUCCEEDED(hr))
            hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(object), 0);
        if (SUCCEEDED(hr) && reflection)
        {
            hr = result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflection_blob), 0);
            if (SUCCEEDED(hr))
            {
                DxcBuffer data = {reflection_blob->GetBufferPointer(), reflection_blob->GetBufferSize(), 0};
                hr = utils->CreateReflection(&data, IID_PPV_ARGS(reflection));
            }
        }
        if (FAILED(hr) && *object)
        {
            (*object)->Release();
            *object = 0;
        }
        if (reflection_blob) reflection_blob->Release();
        if (wide_profile) wide_profile->Release();
        if (wide_entry) wide_entry->Release();
        if (profile_blob) profile_blob->Release();
        if (entry_blob) entry_blob->Release();
        if (result) result->Release();
        if (compiler) compiler->Release();
        if (utils) utils->Release();
        return hr;
    }

    static HRESULT GetRootSignaturePart(IDxcBlob* object, IDxcBlob** root_signature);

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
        if (SUCCEEDED(hr)) hr = DxcCreateInstance(CLSID_DxcContainerBuilder, IID_PPV_ARGS(&builder));
        if (SUCCEEDED(hr)) hr = utils->CreateBlob(object, (UINT32) object_size, 0, &object_blob);
        if (SUCCEEDED(hr)) hr = utils->CreateBlob(root, (UINT32) root_size, 0, &root_blob);
        if (SUCCEEDED(hr))
        {
            if (root_size >= 4 && memcmp(root, "DXBC", 4) == 0)
                hr = GetRootSignaturePart(root_blob, &root_part);
            else
            {
                root_part = root_blob;
                root_part->AddRef();
            }
        }
        if (SUCCEEDED(hr)) hr = builder->Load(object_blob);
        if (SUCCEEDED(hr))
        {
            // Removing an absent part is harmless; AddPart still reports other failures.
            builder->RemovePart(kind);
            hr = builder->AddPart(kind, root_part);
        }
        if (SUCCEEDED(hr)) hr = builder->SerializeContainer(&result);
        if (SUCCEEDED(hr)) hr = result->GetStatus(&status);
        if (SUCCEEDED(hr)) hr = status;
        if (SUCCEEDED(hr)) hr = result->GetResult(output);
        if (result) result->Release();
        if (root_part) root_part->Release();
        if (root_blob) root_blob->Release();
        if (object_blob) object_blob->Release();
        if (builder) builder->Release();
        if (utils) utils->Release();
        return hr;
    }

    static HRESULT GetRootSignaturePart(IDxcBlob* object, IDxcBlob** root_signature)
    {
        *root_signature = 0;
        IDxcContainerReflection* container = 0;
        UINT32 index = 0;
        const UINT32 root_signature_kind = 'R' | ('T' << 8) | ('S' << 16) | ('0' << 24);
        HRESULT hr = DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&container));
        if (SUCCEEDED(hr)) hr = container->Load(object);
        if (SUCCEEDED(hr)) hr = container->FindFirstPartKind(root_signature_kind, &index);
        if (SUCCEEDED(hr)) hr = container->GetPartContent(index, root_signature);
        if (container) container->Release();
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
        if (SUCCEEDED(hr)) hr = DxcCreateInstance(CLSID_DxcContainerBuilder, IID_PPV_ARGS(&builder));
        if (SUCCEEDED(hr)) hr = builder->AddPart(kind, part);
        if (SUCCEEDED(hr)) hr = builder->SerializeContainer(&result);
        if (SUCCEEDED(hr)) hr = result->GetStatus(&status);
        if (SUCCEEDED(hr)) hr = status;
        if (SUCCEEDED(hr)) hr = result->GetResult(root_signature);
        if (result) result->Release();
        if (builder) builder->Release();
        if (part) part->Release();
        return hr;
    }

}
