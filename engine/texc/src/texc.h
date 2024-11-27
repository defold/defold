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
        PF_RGBA_ASTC_4x4,
        PF_RGB_BC1,
        PF_RGBA_BC3,
        PF_R_BC4,
        PF_RG_BC5,
        PF_RGBA_BC7,
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

    enum DitherType
    {
        DT_NONE = 0,
        DT_DEFAULT = 1
    };

    // OLD API
    struct Header
    {
        uint32_t m_Version;
        uint32_t m_Flags;
        uint64_t m_PixelFormat;
        uint32_t m_ColourSpace;
        uint32_t m_ChannelType;
        uint32_t m_Height;
        uint32_t m_Width;
        uint32_t m_Depth;
        uint32_t m_NumSurfaces;
        uint32_t m_NumFaces;
        uint32_t m_MipMapCount;
        uint32_t m_MetaDataSize;
    };

    struct Texture
    {
        void* m_Impl;
    };

    struct Buffer
    {
        uint8_t* m_Data;
        uint32_t m_DataCount; // Encoded size
        uint32_t m_Width;
        uint32_t m_Height;
        bool     m_IsCompressed;
    };

    Texture* CreateTexture(const char* name, uint32_t width, uint32_t height, PixelFormat pixel_format, ColorSpace colorSpace, CompressionType compression_type, void* data);
    void DestroyTexture(Texture* texture);

    // Get header (info) of a texture
    bool GetHeader(Texture* texture, Header* out_header);

    // Get the compressed data size in bytes of a mip map. Returns 0 if not compressed
    uint32_t GetDataSizeCompressed(Texture* texture, uint32_t mip_map);

    // Get the uncompressed data size in bytes of a mip map in a texture
    uint32_t GetDataSizeUncompressed(Texture* texture, uint32_t mip_map);

    // Get the total data size in bytes including all mip maps in a texture (compressed or not)
    uint32_t GetTotalDataSize(Texture* texture);

    // Get the data pointer to texture (mip maps linear layout in memory)
    uint32_t GetData(Texture* texture, void* out_data, uint32_t out_data_size);

    // Get compression flags
    uint64_t GetCompressionFlags(Texture* texture);

    // Resize a texture. The texture must have format PF_R8G8B8A8 to be resized.
    bool Resize(Texture* texture, uint32_t width, uint32_t height);

    // Pre-multiply the color with alpha in a texture. The texture must have format PF_R8G8B8A8 for the alpha to be pre-multiplied.
    bool PreMultiplyAlpha(Texture* texture);

    // Generate mip maps. The texture must have format PF_R8G8B8A8 for mip maps to be generated.
    bool GenMipMaps(Texture* texture);

    // Flips a texture vertically
    bool Flip(Texture* texture, FlipAxis flip_axis);

    // Encode a texture into basis format.
    bool Encode(Texture* texture, PixelFormat pixelFormat, ColorSpace color_space, CompressionLevel compressionLevel, CompressionType compression_type, bool mipmaps, int max_threads);

    // Now only used for font glyphs
    // Compresses an image buffer
    Buffer* CompressBuffer(uint8_t* byte, uint32_t byte_count);

    // Destroys a buffer created by CompressBuffer
    void DestroyBuffer(Buffer* buffer);
}

#endif // DM_TEXC_H
