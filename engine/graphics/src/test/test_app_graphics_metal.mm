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

#include <QuartzCore/QuartzCore.h>
#include <Metal.hpp>
#include "test_app_graphics_texture_upload.h"

namespace dmGraphicsTest
{
    bool ReadMetalTextureUpload(dmGraphics::HContext, dmGraphics::HTexture texture, const TextureUploadReadback& params, dmArray<uint8_t>& pixels)
    {
        void* handle = 0;
        if (dmGraphics::GetTextureHandle(texture, &handle) != dmGraphics::HANDLE_RESULT_OK)
            return false;
        MTL::Texture* native = (MTL::Texture*)handle;
        uint32_t row_pitch = (params.m_RowBytes + 255) & ~255U;
        uint32_t image_pitch = row_pitch * params.m_Rows;
        uint32_t slice_pitch = image_pitch * params.m_Depth;
        MTL::Buffer* buffer = native->device()->newBuffer(slice_pitch * params.m_Layers, MTL::ResourceStorageModeShared);
        MTL::CommandQueue* queue = native->device()->newCommandQueue();
        MTL::CommandBuffer* command = queue->commandBuffer();
        MTL::BlitCommandEncoder* blit = command->blitCommandEncoder();
        MTL::Origin origin = { 0, 0, 0 };
        MTL::Size size = { params.m_Width, params.m_Height, params.m_Depth };
        for (uint32_t layer = 0; layer < params.m_Layers; ++layer)
            blit->copyFromTexture(native, layer, params.m_Mip, origin, size, buffer, layer * slice_pitch, row_pitch, image_pitch);
        blit->endEncoding();
        command->commit();
        command->waitUntilCompleted();
        bool success = command->status() == MTL::CommandBufferStatusCompleted;
        if (success)
            for (uint32_t row = 0; row < params.m_Rows * params.m_Depth * params.m_Layers; ++row)
                memcpy(pixels.Begin() + row * params.m_RowBytes, (uint8_t*)buffer->contents() + row * row_pitch, params.m_RowBytes);
        buffer->release();
        queue->release();
        return success;
    }
} // namespace dmGraphicsTest
