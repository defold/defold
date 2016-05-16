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

}

#endif // DM_TEXC_PRIVATE_H
