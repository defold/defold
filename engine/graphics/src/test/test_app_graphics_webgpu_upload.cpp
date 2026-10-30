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

#include <dlib/time.h>
#include "webgpu/graphics_webgpu_private.h"
#include "test_app_graphics_texture_upload.h"
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

namespace dmGraphicsTest
{
    struct MapResult
    {
        bool m_Done;
        bool m_Success;
    };
#if defined(DM_GRAPHICS_WEBGPU2)
    static void Mapped(WGPUMapAsyncStatus status, WGPUStringView, void* data, void*)
#else
    static void Mapped(WGPUBufferMapAsyncStatus status, void* data)
#endif
    {
        MapResult* result = (MapResult*)data;
#if defined(DM_GRAPHICS_WEBGPU2)
        result->m_Success = status == WGPUMapAsyncStatus_Success;
#else
        result->m_Success = status == WGPUBufferMapAsyncStatus_Success;
#endif
        result->m_Done = true;
    }

    void PrepareWebGPUTextureUploadReadback(dmGraphics::HContext context, dmGraphics::HTexture handle)
    {
        dmGraphics::WebGPUTexture* texture = dmGraphics::GetAssetFromContainer<dmGraphics::WebGPUTexture>(context->m_AssetHandleContainer, handle);
        texture->m_UsageFlags = (WGPUTextureUsage)(texture->m_UsageFlags | WGPUTextureUsage_CopySrc);
    }

    bool ReadWebGPUTextureUpload(dmGraphics::HContext _context, dmGraphics::HTexture handle, const TextureUploadReadback& params, dmArray<uint8_t>& pixels)
    {
        using namespace dmGraphics;
        WebGPUContext* context = (WebGPUContext*)_context;
        WebGPUTexture* texture = GetAssetFromContainer<WebGPUTexture>(_context->m_AssetHandleContainer, handle);
        uint32_t row_pitch = (params.m_RowBytes + 255) & ~255U;
        uint64_t size = (uint64_t)row_pitch * params.m_Rows * params.m_Depth * params.m_Layers;
#if defined(DM_GRAPHICS_WEBGPU2)
        WGPUBufferDescriptor desc = WGPU_BUFFER_DESCRIPTOR_INIT;
        WGPUTexelCopyTextureInfo source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
        WGPUTexelCopyBufferInfo destination = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
        WGPUCommandEncoderDescriptor encoder_desc = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
#else
        WGPUBufferDescriptor desc = {};
        WGPUImageCopyTexture source = {};
        WGPUImageCopyBuffer destination = {};
        WGPUCommandEncoderDescriptor encoder_desc = {};
#endif
        desc.size = size;
        desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
        WGPUBuffer buffer = wgpuDeviceCreateBuffer(context->m_Device, &desc);
        source.texture = texture->m_Texture;
        source.mipLevel = params.m_Mip;
        source.aspect = WGPUTextureAspect_All;
        destination.buffer = buffer;
        destination.layout.bytesPerRow = row_pitch;
        destination.layout.rowsPerImage = params.m_Rows;
        WGPUExtent3D extent = { params.m_Width, params.m_Height, params.m_Depth * params.m_Layers };
        TextureFormatCompressedBlockSize block;
        if (GetTextureFormatCompressedBlockSize(params.m_Format, &block))
        {
            extent.width = ((extent.width + block.m_Width - 1) / block.m_Width) * block.m_Width;
            extent.height = ((extent.height + block.m_Height - 1) / block.m_Height) * block.m_Height;
        }
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(context->m_Device, &encoder_desc);
        wgpuCommandEncoderCopyTextureToBuffer(encoder, &source, &destination, &extent);
        WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, NULL);
        wgpuQueueSubmit(context->m_Queue, 1, &commands);
        wgpuCommandBufferRelease(commands);
        wgpuCommandEncoderRelease(encoder);
        MapResult result = {};
#if defined(DM_GRAPHICS_WEBGPU2)
        WGPUBufferMapCallbackInfo callback = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
        callback.mode = WGPUCallbackMode_AllowProcessEvents;
        callback.callback = Mapped;
        callback.userdata1 = &result;
        wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, size, callback);
#else
        wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, size, Mapped, &result);
#endif
        while (!result.m_Done)
        {
#if defined(__EMSCRIPTEN__)
            emscripten_sleep(1);
#else
            wgpuInstanceProcessEvents(context->m_Instance);
            dmTime::Sleep(100);
#endif
        }
        if (result.m_Success)
        {
            const uint8_t* mapped = (const uint8_t*)wgpuBufferGetConstMappedRange(buffer, 0, size);
            for (uint32_t row = 0; row < params.m_Rows * params.m_Depth * params.m_Layers; ++row)
                memcpy(pixels.Begin() + row * params.m_RowBytes, mapped + row * row_pitch, params.m_RowBytes);
            wgpuBufferUnmap(buffer);
        }
        wgpuBufferRelease(buffer);
        return result.m_Success;
    }
} // namespace dmGraphicsTest
