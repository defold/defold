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

#ifndef DM_READ_PIXELS_TESTS_H
#define DM_READ_PIXELS_TESTS_H

#include <string.h>
#include <dlib/array.h>
#include <dlib/log.h>
#include <dmsdk/graphics/graphics.h>

// Compare vertically offset crops, including both edges, with a full read.
// This catches origin, row order, row pitch and writes beyond the requested size.
static bool CheckReadPixelsRegions(dmGraphics::HContext context, uint32_t width, uint32_t height, const uint8_t* expected_pixels)
{
    dmArray<uint8_t> full;
    if (!expected_pixels)
    {
        full.SetCapacity(width * height * 4);
        full.SetSize(full.Capacity());
        memset(full.Begin(), 0, full.Size());
        dmGraphics::ReadPixels(context, 0, 0, width, height, full.Begin(), full.Size());
        expected_pixels = full.Begin();
    }
    const uint32_t crop_width = width > 13 ? 13 : width;
    const uint32_t crop_height = height > 3 ? 3 : 1;
    const uint32_t x = (width - crop_width) / 2;
    const uint32_t ys[] = { 0, (height - crop_height) / 2, height - crop_height };
    dmArray<uint8_t> crop;
    crop.SetCapacity(crop_width * crop_height * 4 + 16);
    crop.SetSize(crop.Capacity());
    for (uint32_t i = 0; i < 3; ++i)
    {
        memset(crop.Begin(), 0xa5, crop.Size());
        dmGraphics::ReadPixels(context, x, ys[i], crop_width, crop_height, crop.Begin(), crop.Size());
        for (uint32_t row = 0; row < crop_height; ++row)
        {
            const uint8_t* expected = expected_pixels + ((height - ys[i] - crop_height + row) * width + x) * 4;
            if (memcmp(expected, crop.Begin() + row * crop_width * 4, crop_width * 4))
            {
                dmLogError("ReadPixels crop mismatch at y=%u, row=%u", ys[i], row);
                return false;
            }
        }
        for (uint32_t j = crop_width * crop_height * 4; j < crop.Size(); ++j)
            if (crop[j] != 0xa5) return false;
    }
    return true;
}

// Verifies target selection, MSAA resolve, cube faces, BGRA packing and invalid
// regions. The capture harness supplies an MRT draw; DX12 uses per-attachment clears.
// The caller owns an active frame and supplies a target to restore afterwards.
static bool CheckReadPixelsTargets(dmGraphics::HContext context, dmGraphics::HRenderTarget restore_target,
    void (*draw_mrt)(dmGraphics::HContext, uint32_t, void*), void* user_data)
{
    using namespace dmGraphics;
    const uint32_t size = 16;
    bool valid = true;
    DisableState(context, STATE_SCISSOR_TEST);
    SetColorMask(context, true, true, true, true);
    for (uint32_t kind = 0; kind < 3; ++kind)
    {
        RenderTargetCreationParams params;
        params.m_SampleCount = kind == 1 ? 4 : 1;
        params.m_TextureType = kind == 2 ? TEXTURE_TYPE_CUBE_MAP : TEXTURE_TYPE_2D;
        params.m_ColorBufferCreationParams[0].m_Width = size;
        params.m_ColorBufferCreationParams[0].m_Height = size;
        params.m_ColorBufferParams[0].m_Width = size;
        params.m_ColorBufferParams[0].m_Height = size;
        params.m_ColorBufferParams[0].m_Format = TEXTURE_FORMAT_RGBA;
        params.m_ColorBufferLoadOps[0] = ATTACHMENT_OP_LOAD;
        params.m_ColorBufferStoreOps[0] = ATTACHMENT_OP_STORE;
        params.m_ColorBufferCreationParams[1] = params.m_ColorBufferCreationParams[0];
        params.m_ColorBufferParams[1] = params.m_ColorBufferParams[0];
        params.m_ColorBufferLoadOps[1] = ATTACHMENT_OP_LOAD;
        params.m_ColorBufferStoreOps[1] = ATTACHMENT_OP_STORE;
        const uint32_t buffers = BUFFER_TYPE_COLOR0_BIT | (kind == 1 ? BUFFER_TYPE_COLOR1_BIT : 0);
        HRenderTarget target = NewRenderTarget(context, buffers, params);
        if (!target) return false;
        const uint32_t faces = kind == 2 ? 6 : 1;
        // Populate every face before reading, to distinguish the selected face
        // from face zero even when readback interrupts and resumes a render pass.
        for (uint32_t face = 0; face < faces; ++face)
        {
            RenderTargetBindingParams binding;
            binding.m_CubeMapFace = (CubeMapFace) face;
            SetRenderTarget(context, target, binding);
            Clear(context, BUFFER_TYPE_COLOR0_BIT, 31 + face * 23, 73, 149, 255, 1.0f, 0);
            if (kind == 1)
            {
                if (draw_mrt)
                    draw_mrt(context, size, user_data);
                else
                    Clear(context, BUFFER_TYPE_COLOR1_BIT, 0, 255, 0, 255, 1.0f, 0);
            }
        }
        for (uint32_t face = 0; face < faces; ++face)
        {
            RenderTargetBindingParams binding;
            binding.m_CubeMapFace = (CubeMapFace) face;
            SetRenderTarget(context, target, binding);
            uint8_t pixels[size * size * 4];
            memset(pixels, 0, sizeof(pixels));
            ReadPixels(context, 0, 0, size, size, pixels, sizeof(pixels));
            for (uint32_t i = 0; i < sizeof(pixels); i += 4)
                valid &= pixels[i] == 149 && pixels[i + 1] == 73 && pixels[i + 2] == 31 + face * 23 && pixels[i + 3] == 255;
            // A second clear and read must include the new work after resuming.
            Clear(context, BUFFER_TYPE_COLOR0_BIT, 197, 59, 11, 255, 1.0f, 0);
            ReadPixels(context, 1, 2, 1, 1, pixels, sizeof(pixels));
            valid &= pixels[0] == 11 && pixels[1] == 59 && pixels[2] == 197 && pixels[3] == 255;
        }
        uint8_t pixels[size * size * 4];
        memset(pixels, 0xa5, sizeof(pixels));
        // Expected validation errors must not fail the image runner. Common
        // argument validation only needs one target; bounds vary by adapter.
        const LogSeverity log_level = dmLogGetLevel();
        dmLogSetLevel(LOG_SEVERITY_FATAL);
        if (kind == 0)
        {
            ReadPixels(context, -1, 0, 1, 1, pixels, sizeof(pixels));
            ReadPixels(context, 0, -1, 1, 1, pixels, sizeof(pixels));
            ReadPixels(context, 0, 0, size, size, pixels, 1);
            ReadPixels(context, 0, 0, 0, 1, pixels, sizeof(pixels));
            ReadPixels(context, 0, 0, 1, 0, pixels, sizeof(pixels));
            ReadPixels(context, 0, 0, 1, 1, 0, sizeof(pixels));
            ReadPixels(context, 0, 0, 0xffffffffu, 0xffffffffu, pixels, sizeof(pixels));
        }
        ReadPixels(context, size, 0, 1, 1, pixels, sizeof(pixels));
        ReadPixels(context, 0, size, 1, 1, pixels, sizeof(pixels));
        dmLogSetLevel(log_level);
        for (uint32_t i = 0; i < sizeof(pixels); ++i) valid &= pixels[i] == 0xa5;
        SetRenderTarget(context, restore_target, RenderTargetBindingParams());
        DeleteRenderTarget(context, target);
        if (!valid)
        {
            dmLogError("ReadPixels target checks failed for kind %u (2D=0, MSAA=1, cube=2)", kind);
            return false;
        }
    }
    return valid;
}

#endif
