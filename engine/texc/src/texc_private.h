// Copyright 2020 The Defold Foundation
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

#ifndef DM_TEXC_PRIVATE_H
#define DM_TEXC_PRIVATE_H

#include <dlib/array.h>
#include <stdlib.h>
#include <stdint.h>
#include <PVRTexture.h>
#include "texc.h"

/**
 * Texture processing internal types
 */
namespace dmTexc
{
    static const uint32_t COMPRESSION_ENABLED_PIXELCOUNT_THRESHOLD = 64; // do not compress mips with less than this pixelcount

    struct TextureData
    {
        size_t m_ByteSize;
        uint8_t m_IsCompressed;
        uint8_t* m_Data;
    };

    struct Texture
    {
        dmArray<TextureData> m_CompressedMips;
        pvrtexture::CPVRTexture* m_PVRTexture;
        uint64_t m_CompressionFlags;
    };


    bool CompressWebP(HTexture texture, PixelFormat pixel_format, ColorSpace color_space, CompressionLevel compression_level, CompressionType compression_type);
    HBuffer CompressWebPBuffer(uint32_t width, uint32_t height, uint32_t bpp, void* data, uint32_t size, PixelFormat pixel_format, CompressionLevel compression_level, CompressionType compression_type);

    void PVRTCDecomposeBlocks(const uint64_t* data, const uint32_t width, const uint32_t height, uint32_t* color_a_rgba, uint32_t* color_b_rgba, uint32_t* modulation);

    void ETCDecomposeBlocks(const uint64_t* data, const uint32_t width, const uint32_t height, uint32_t* base_colors, uint32_t* pixel_indices);

    void RGB565ToRGB888(const uint16_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgb);

    void RGBA4444ToRGBA8888(const uint16_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgba);

    void L8ToRGB888(const uint8_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgb);

    void L8A8ToRGBA8888(const uint8_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgba);

}

#endif // DM_TEXC_PRIVATE_H
