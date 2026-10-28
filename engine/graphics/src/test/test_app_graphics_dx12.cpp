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

#include "dx12/graphics_dx12_private.h"
#include "test_app_graphics_texture_upload.h"

namespace dmGraphicsTest
{
    bool ReadDX12TextureUpload(dmGraphics::HContext _context, dmGraphics::HTexture handle, const TextureUploadReadback& params, dmArray<uint8_t>& pixels)
    {
        using namespace dmGraphics;
        DX12Context* context = (DX12Context*)_context;
        if (context->m_FrameBegun)
        {
            // Public readback submits the active frame, including its deferred
            // texture uploads, before this independent subresource readback.
            uint8_t pixel[4];
            ReadPixels(_context, 0, 0, 1, 1, pixel, sizeof(pixel));
        }
        DX12Texture* texture = GetAssetFromContainer<DX12Texture>(_context->m_AssetHandleContainer, handle);
        dmArray<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts;
        layouts.SetCapacity(params.m_Layers);
        layouts.SetSize(params.m_Layers);
        uint64_t size = 0;
        uint32_t total_layers = IsTextureType3D(texture->m_Base.m_Type) ? 1 : texture->m_ResourceDesc.DepthOrArraySize;
        for (uint32_t layer = 0; layer < params.m_Layers; ++layer)
        {
            UINT64 bytes;
            uint32_t subresource = D3D12CalcSubresource(params.m_Mip, layer, 0, texture->m_ResourceDesc.MipLevels, total_layers);
            context->m_Device->GetCopyableFootprints(&texture->m_ResourceDesc, subresource, 1, 0, &layouts[layer], 0, 0, &bytes);
            layouts[layer].Offset = (size + 511) & ~511ULL;
            size = layouts[layer].Offset + bytes;
        }
        ID3D12CommandAllocator* allocator = 0;
        ID3D12GraphicsCommandList* command = 0;
        ID3D12Resource* buffer = 0;
        ID3D12Fence* fence = 0;
        HANDLE event = CreateEventEx(0, 0, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
        CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_READBACK);
        D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size);
        HRESULT result = context->m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, DM_IID_PPV_ARGS(&allocator));
        if (SUCCEEDED(result))
            result = context->m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, 0, DM_IID_PPV_ARGS(&command));
        if (SUCCEEDED(result))
            result = context->m_Device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, 0, DM_IID_PPV_ARGS(&buffer));
        if (SUCCEEDED(result))
            result = context->m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, DM_IID_PPV_ARGS(&fence));
        if (SUCCEEDED(result))
        {
            for (uint32_t layer = 0; layer < params.m_Layers; ++layer)
            {
                uint32_t subresource = D3D12CalcSubresource(params.m_Mip, layer, 0, texture->m_ResourceDesc.MipLevels, total_layers);
                D3D12_RESOURCE_STATES previous = texture->m_ResourceStates[subresource];
                TransitionTexture(command, texture, D3D12_RESOURCE_STATE_COPY_SOURCE, subresource, 1);
                CD3DX12_TEXTURE_COPY_LOCATION source(texture->m_Resource, subresource);
                CD3DX12_TEXTURE_COPY_LOCATION destination(buffer, layouts[layer]);
                command->CopyTextureRegion(&destination, 0, 0, 0, &source, 0);
                TransitionTexture(command, texture, previous, subresource, 1);
            }
            result = command->Close();
            if (SUCCEEDED(result))
            {
                ID3D12CommandList* lists[] = { command };
                context->m_CommandQueue->ExecuteCommandLists(1, lists);
                result = context->m_CommandQueue->Signal(fence, 1);
            }
            if (SUCCEEDED(result))
                result = fence->SetEventOnCompletion(1, event);
            if (SUCCEEDED(result) && WaitForSingleObject(event, INFINITE) != WAIT_OBJECT_0)
                result = E_FAIL;
        }
        if (SUCCEEDED(result))
        {
            uint8_t* mapped;
            result = buffer->Map(0, 0, (void**)&mapped);
            if (SUCCEEDED(result))
            {
                for (uint32_t layer = 0; layer < params.m_Layers; ++layer)
                    for (uint32_t row = 0; row < params.m_Rows * params.m_Depth; ++row)
                        memcpy(pixels.Begin() + (layer * params.m_Rows * params.m_Depth + row) * params.m_RowBytes,
                               mapped + layouts[layer].Offset + (uint64_t)row * layouts[layer].Footprint.RowPitch,
                               params.m_RowBytes);
                buffer->Unmap(0, 0);
            }
        }
        if (fence)
            fence->Release();
        if (buffer)
            buffer->Release();
        if (command)
            command->Release();
        if (allocator)
            allocator->Release();
        if (event)
            CloseHandle(event);
        return SUCCEEDED(result);
    }

    uint64_t DX12TextureUploadExpectedBytes(dmGraphics::HContext _context, dmGraphics::TextureType type, const dmGraphics::TextureParams& base, const dmGraphics::TextureParams& mip, uint32_t mip_count, uint32_t layers)
    {
        using namespace dmGraphics;
        DX12Context* context = (DX12Context*)_context;
        TextureFormat format = GetTextureUploadStorageFormat(ADAPTER_FAMILY_DIRECTX, mip.m_Format);
        DXGI_FORMAT native = format == TEXTURE_FORMAT_RGBA ? DXGI_FORMAT_R8G8B8A8_UNORM :
        format == TEXTURE_FORMAT_RGBA16F                   ? DXGI_FORMAT_R16G16B16A16_FLOAT :
        format == TEXTURE_FORMAT_RGBA32F                   ? DXGI_FORMAT_R32G32B32A32_FLOAT :
        format == TEXTURE_FORMAT_RGB32F                    ? DXGI_FORMAT_R32G32B32_FLOAT :
                                                             DXGI_FORMAT_BC1_UNORM;
        bool volume = IsTextureType3D(type);
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = volume ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = mip.m_Width;
        desc.Height = mip.m_Height;
        desc.DepthOrArraySize = volume ? dmMath::Max(1U, (uint32_t)mip.m_Depth) : 1;
        desc.MipLevels = 1;
        desc.Format = native;
        desc.SampleDesc.Count = 1;
        uint32_t upload_layers = volume ? 1 : mip.m_SubUpdate ? dmMath::Max(1U, (uint32_t)mip.m_LayerCount) :
                                                                layers;
        uint64_t total = 0;
        for (uint32_t layer = 0; layer < upload_layers; ++layer)
        {
            UINT64 bytes;
            context->m_Device->GetCopyableFootprints(&desc, 0, 1, 0, 0, 0, 0, &bytes);
            total = ((total + 511) & ~511ULL) + bytes;
        }
        return total;
    }
} // namespace dmGraphicsTest
