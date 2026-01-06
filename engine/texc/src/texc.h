// Copyright 2020-2025 The Defold Foundation
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

#ifndef DM_TEXC_H
#define DM_TEXC_H

#include <stdint.h>

/**
 * Texture processing
 */
namespace dmTexc
{
    // Matches the enums in graphics_ddf.proto

    enum PixelFormat
    {
        PF_L8,
        PF_R8G8B8,
        PF_R8G8B8A8,
        PF_A8B8G8R8,
        PF_RGB_PVRTC_2BPPV1,
        PF_RGB_PVRTC_4BPPV1,
        PF_RGBA_PVRTC_2BPPV1,
        PF_RGBA_PVRTC_4BPPV1,
        PF_RGB_ETC1,
        PF_R5G6B5,
        PF_R4G4B4A4,
        PF_L8A8,
        PF_RGBA_ETC2,
        PF_RGB_BC1,
        PF_RGBA_BC3,
        PF_R_BC4,
        PF_RG_BC5,
        PF_RGBA_BC7,

        // ASTC formats
        PF_RGBA_ASTC_4x4,
        PF_RGBA_ASTC_5x4,
        PF_RGBA_ASTC_5x5,
        PF_RGBA_ASTC_6x5,
        PF_RGBA_ASTC_6x6,
        PF_RGBA_ASTC_8x5,
        PF_RGBA_ASTC_8x6,
        PF_RGBA_ASTC_8x8,
        PF_RGBA_ASTC_10x5,
        PF_RGBA_ASTC_10x6,
        PF_RGBA_ASTC_10x8,
        PF_RGBA_ASTC_10x10,
        PF_RGBA_ASTC_12x10,
        PF_RGBA_ASTC_12x12,
    };

    enum ColorSpace
    {
        CS_LRGB,
        CS_SRGB,
    };

    enum CompressionLevel
    {
        CL_FAST,
        CL_NORMAL,
        CL_HIGH,
        CL_BEST,
        CL_ENUM,
    };

    enum CompressionType
    {
        CT_DEFAULT,     // == NONE
        CT_WEBP,        // Deprecated
        CT_WEBP_LOSSY,  // Deprecated
        CT_BASIS_UASTC,
        CT_BASIS_ETC1S,
        CT_ASTC,
    };

    enum CompressionFlags
    {
        CF_ALPHA_CLEAN = 1,
    };

    enum FlipAxis
    {
        FLIP_AXIS_X = 0,
        FLIP_AXIS_Y = 1,
        FLIP_AXIS_Z = 2
    };

    struct Image
    {
        const char* m_Path;
        uint8_t*    m_Data;
        uint32_t    m_DataCount;
        uint32_t    m_Width;
        uint32_t    m_Height;
        PixelFormat m_PixelFormat;
        ColorSpace  m_ColorSpace;
    };

    Image*   CreateImage(const char* path, uint32_t width, uint32_t height, PixelFormat pixel_format, ColorSpace colorSpace, uint32_t data_size, uint8_t* data);
    void     CreatePreviewImage(uint32_t width, uint32_t height, uint32_t data_size, const uint8_t* input_data, uint8_t* output_data);
    void     DestroyImage(Image* image);

    uint32_t GetWidth(Image* image);
    uint32_t GetHeight(Image* image);

    // Resize a texture. The texture must have format PF_R8G8B8A8 to be resized.
    Image* Resize(Image* image, uint32_t width, uint32_t height);

    // Pre-multiply the color with alpha in a texture. The texture must have format PF_R8G8B8A8 for the alpha to be pre-multiplied.
    bool PreMultiplyAlpha(Image* image);

    // Generate mip maps. The texture must have format PF_R8G8B8A8 for mip maps to be generated.
    bool GenMipMaps(Image* image); // This doesn't exist??

    // Flips a texture vertically
    bool Flip(Image* image, FlipAxis flip_axis);

    // Dithers a texture given the intended target pixel format
    bool Dither(Image* image, PixelFormat pixel_format);

    // **********************************************************************
    // Font API

    struct Buffer
    {
        uint8_t* m_Data;
        uint32_t m_DataCount; // Encoded size
        uint32_t m_Width;
        uint32_t m_Height;
        bool     m_IsCompressed;
    };

    // Compresses an image buffer
    Buffer* CompressBuffer(uint8_t* byte, uint32_t byte_count);

    // Destroys a buffer created by CompressBuffer
    void DestroyBuffer(Buffer* buffer);


    // **********************************************************************
    // Texture compression api

    struct BasisUEncodeSettings
    {
        // Input
        const char* m_Path;
        int         m_Width;
        int         m_Height;
        PixelFormat m_PixelFormat;
        ColorSpace  m_ColorSpace;
        uint8_t*    m_Data;
        uint32_t    m_DataCount;

        int         m_NumThreads;
        bool        m_Debug;

        // Output
        PixelFormat m_OutPixelFormat;

        // Naming matching variables in basis_compressor_params (basis_comp.h)
        bool        m_rdo_uastc;
        uint32_t    m_pack_uastc_flags;
        int         m_rdo_uastc_dict_size;
        float       m_rdo_uastc_quality_scalar;
    };

    // Encode a texture into basis format. Caller must call free() on the returned data.
    bool BasisUEncode(BasisUEncodeSettings* settings, uint8_t** out, uint32_t* out_size);

    struct DefaultEncodeSettings
    {
        // Input
        const char* m_Path;
        int         m_Width;
        int         m_Height;
        PixelFormat m_PixelFormat;
        ColorSpace  m_ColorSpace;
        uint8_t*    m_Data;
        uint32_t    m_DataCount;

        int         m_NumThreads;
        bool        m_Debug;

        // Output
        PixelFormat m_OutPixelFormat;
    };

    // Encode a texture into correct pixel format. Caller must call free() on the returned data.
    bool DefaultEncode(DefaultEncodeSettings* settings, uint8_t** out, uint32_t* out_size);

    struct ASTCEncodeSettings
    {
        // Input
        const char* m_Path;
        int         m_Width;
        int         m_Height;
        PixelFormat m_PixelFormat;
        ColorSpace  m_ColorSpace;
        uint8_t*    m_Data;
        uint32_t    m_DataCount;

        int         m_NumThreads;
        float       m_QualityLevel;
        PixelFormat m_OutPixelFormat;
    };

    // Encode a texture into basis format. Caller must call free() on the returned data.
    bool ASTCEncode(ASTCEncodeSettings* settings, uint8_t** out, uint32_t* out_size);
}

#endif // DM_TEXC_H
