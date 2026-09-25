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

#ifndef DM_TEXC_PRIVATE_H
#define DM_TEXC_PRIVATE_H

#include <dlib/array.h>
#include <stdlib.h>
#include <stdint.h>

#include "texc.h"

/**
 * Texture processing internal types
 */
namespace dmTexc
{
    struct Ktx2Texture;
    void InitBasisU();
    struct Ktx2Info
    {
        uint32_t m_Width;
        uint32_t m_Height;
        uint32_t m_LevelCount;
        uint32_t m_Channels;
        uint32_t m_VkFormat;
        uint32_t m_Supercompression;
        bool m_Srgb;
        bool m_Premultiplied;
        // Flips needed to normalize the encoded orientation to top-left.
        bool m_FlipX;
        bool m_FlipY;
        bool m_CanRepack;
    };

    // The texture owns a copy of the encoded input. Returned mip buffers belong to the caller.
    Ktx2Texture* LoadKtx2(const uint8_t* data, uint32_t size, Ktx2Info* info, const char** error);
    void DestroyKtx2(Ktx2Texture* texture);
    // RGBA8 with channel swizzle applied, retaining source orientation and premultiplication.
    bool DecodeKtx2Mip(Ktx2Texture* texture, uint32_t level, dmArray<uint8_t>& pixels, const char** error);
    // Single-mip Basis payloads for ETC1S/UASTC, native blocks for BC7.
    bool RepackKtx2Mip(Ktx2Texture* texture, uint32_t level, dmArray<uint8_t>& bytes, const char** error);
    bool EncodeKtx2Mip(Ktx2Texture* texture, uint32_t width, uint32_t height, const uint8_t* pixels, uint32_t size, dmArray<uint8_t>& bytes, const char** error);

    static const uint32_t COMPRESSION_ENABLED_PIXELCOUNT_THRESHOLD = 64; // do not compress mips with less than this pixelcount

    Image* Resize(Image* image, uint32_t width, uint32_t height, bool srgb, bool premultiplied);
    Image* ResizeBasis(Image* image, uint32_t width, uint32_t height, bool srgb, bool premultiplied);

    uint16_t RGB888ToRGB565(uint8_t red, uint8_t green, uint8_t blue);
    uint16_t RGBA8888ToRGBA4444(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);

    void RGB565ToRGB888(const uint16_t* data, uint32_t width, uint32_t height, uint8_t* color_rgb);

    void RGBA4444ToRGBA8888(const uint16_t* data, uint32_t width, uint32_t height, uint8_t* color_rgba);

    void L8ToRGB888(const uint8_t* data, uint32_t width, uint32_t height, uint8_t* color_rgb);

    void L8A8ToRGBA8888(const uint8_t* data, uint32_t width, uint32_t height, uint8_t* color_rgba);

    void PreMultiplyAlpha(uint8_t* data, uint32_t width, uint32_t height);
    void FlipImageX_RGBA8888(uint32_t* data, uint32_t width, uint32_t height);
    void FlipImageY_RGBA8888(uint32_t* data, uint32_t width, uint32_t height);
    void FlipImageX_RGBA32F(float* data, uint32_t width, uint32_t height);
    void FlipImageY_RGBA32F(float* data, uint32_t width, uint32_t height);
    Image* ResizeRGBA32F(Image* image, uint32_t width, uint32_t height);

    uint32_t GetDataSize(PixelFormat pf, uint32_t width, uint32_t height);
    bool ConvertToRGBA8888(const uint8_t* data, uint32_t width, uint32_t height, PixelFormat pf, uint8_t* out);
    void ConvertRGBA8888ToPf(const uint8_t* input, uint32_t width, uint32_t height, PixelFormat pf, void* out_data);
    bool ConvertRGBA32FToPf(const uint8_t* input, uint32_t width, uint32_t height, PixelFormat pf, void* out_data);
    void ConvertPremultiplyAndFlip_ABGR8888ToRGBA8888(const uint8_t* input_data, uint8_t* output_data, uint32_t width, uint32_t height);

    uint32_t    GetDataSize(PixelFormat pf, uint32_t width, uint32_t height);
    bool        ConvertToRGBA8888(const uint8_t* data, uint32_t width, uint32_t height, PixelFormat pf, uint8_t* out);
    void        ConvertRGBA8888ToPf(const uint8_t* input, uint32_t width, uint32_t height, PixelFormat pf, void* out_data);
    bool        ConvertRGBA32FToPf(const uint8_t* input, uint32_t width, uint32_t height, PixelFormat pf, void* out_data);
    void        ConvertPremultiplyAndFlip_ABGR8888ToRGBA8888(const uint8_t* input_data, uint8_t* output_data, uint32_t width, uint32_t height);

    // Dithers an image where the target format is RGBA4444
    // Input/output image is RGBA8888
    void        DitherRGBA4444(uint8_t* data, uint32_t width, uint32_t height);
    // Dithers an image where the target format is RGB565
    // Input/output image is RGBA8888
    void        DitherRGBx565(uint8_t* data, uint32_t width, uint32_t height);

    void        DebugPrint(uint8_t* p, uint32_t width, uint32_t height, uint32_t num_channels);
}

#endif // DM_TEXC_PRIVATE_H
