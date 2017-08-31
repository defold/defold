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

    struct CompressedTextureData
    {
        size_t m_ByteSize;
        uint8_t* m_Data;
    };

    struct Texture
    {
        dmArray<CompressedTextureData> m_CompressedMips;
        pvrtexture::CPVRTexture* m_PVRTexture;
        uint64_t m_CompressionFlags;
    };

    bool CompressWebP(HTexture texture, PixelFormat pixel_format, ColorSpace color_space, CompressionLevel compression_level, CompressionType compression_type);

    void PVRTCDecomposeBlocks(const uint64_t* data, const uint32_t width, const uint32_t height, uint32_t* color_a_rgba, uint32_t* color_b_rgba, uint32_t* modulation);

    void ETCDecomposeBlocks(const uint64_t* data, const uint32_t width, const uint32_t height, uint32_t* base_colors, uint32_t* pixel_indices);

    void RGB565ToRGB888(const uint16_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgb);

    void RGBA4444ToRGBA8888(const uint16_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgba);

    void L8ToRGB888(const uint8_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgb);

    void L8A8ToRGBA8888(const uint8_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgba);

}

#endif // DM_TEXC_PRIVATE_H
