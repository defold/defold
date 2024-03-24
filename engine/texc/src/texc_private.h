// Copyright 2020-2024 The Defold Foundation
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

#include <basis/encoder/basisu_enc.h>

/**
 * Texture processing internal types
 */
namespace dmTexc
{
    static const uint32_t COMPRESSION_ENABLED_PIXELCOUNT_THRESHOLD = 64; // do not compress mips with less than this pixelcount

    struct Texture;

    struct Encoder
    {
        bool     (*m_FnCreate)(Texture* texture, uint32_t width, uint32_t height, PixelFormat pixel_format, ColorSpace color_space, CompressionType compression_type, void* data);
        void     (*m_FnDestroy)(Texture* texture);
        bool     (*m_FnGenMipMaps)(Texture* texture);
        bool     (*m_FnResize)(Texture* texture, uint32_t width, uint32_t height);
        bool     (*m_FnEncode)(Texture* texture, int num_threads, PixelFormat pixel_format, CompressionType compression_type, CompressionLevel compression_level);
        uint32_t (*m_FnGetTotalDataSize)(Texture* texture);
        uint32_t (*m_FnGetData)(Texture* texture, void* out_data, uint32_t out_data_size);
        bool     (*m_FnPreMultiplyAlpha)(Texture* texture);
        bool     (*m_FnFlip)(Texture* texture, FlipAxis flip_axis);
        bool     (*m_FnGetHeader)(Texture* texture, Header* out_header);
    };

    struct TextureData
    {
        uint8_t* m_Data;
        uint32_t m_ByteSize; // Encoded size
        uint32_t m_Width;
        uint32_t m_Height;
        uint8_t  m_IsCompressed;
    };

    struct Texture
    {
        dmArray<TextureData> m_Mips;
        const char*          m_Name; // For easier debugging
        PixelFormat m_PixelFormat;
        ColorSpace m_ColorSpace;
        CompressionType m_CompressionType;
        uint32_t m_Width;
        uint32_t m_Height;
        uint64_t m_CompressionFlags;

        Encoder m_Encoder;

        // Used with CT_BASIS_xx encoding
        basisu::image m_BasisImage; // Original image
        dmArray<uint8_t> m_BasisFile;
        bool m_BasisGenMipmaps;
    };


    uint16_t RGB888ToRGB565(uint8_t red, uint8_t green, uint8_t blue);
    uint16_t RGBA8888ToRGBA4444(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);

    void RGB565ToRGB888(const uint16_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgb);

    void RGBA4444ToRGBA8888(const uint16_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgba);

    void L8ToRGB888(const uint8_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgb);

    void L8A8ToRGBA8888(const uint8_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgba);

    void PreMultiplyAlpha(uint8_t* data, const uint32_t width, const uint32_t height);
    void FlipImageX_RGBA8888(uint32_t* data, const uint32_t width, const uint32_t height);
    void FlipImageY_RGBA8888(uint32_t* data, const uint32_t width, const uint32_t height);

    uint32_t    GetDataSize(PixelFormat pf, uint32_t width, uint32_t height);
    bool        ConvertToRGBA8888(const uint8_t* data, const uint32_t width, const uint32_t height, PixelFormat pf, uint8_t* out);
    void        ConvertRGBA8888ToPf(const uint8_t* input, uint32_t width, uint32_t height, PixelFormat pf, void* out_data);

    // Dithers an image where the target format is RGBA4444
    // Input/output image is RGBA8888
    void        DitherRGBA4444(uint8_t* data, uint32_t width, uint32_t height);
    // Dithers an image where the target format is RGB565
    // Input/output image is RGBA8888
    void        DitherRGBx565(uint8_t* data, uint32_t width, uint32_t height);

    void        DebugPrint(uint8_t* p, uint32_t width, uint32_t height, uint32_t num_channels);
}

#endif // DM_TEXC_PRIVATE_H
