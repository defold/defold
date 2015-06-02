#ifndef DM_TEXC_H
#define DM_TEXC_H

#include <stdint.h>

#include <dlib/shared_library.h>

/**
 * Texture processing
 * Essentially a wrapper for PVRTexLib
 */
namespace dmTexc
{
    enum PixelFormat
    {
        PF_L8,
        PF_R8G8B8,
        PF_R8G8B8A8,

        PF_RGB_PVRTC_2BPPV1,
        PF_RGB_PVRTC_4BPPV1,
        PF_RGBA_PVRTC_2BPPV1,
        PF_RGBA_PVRTC_4BPPV1,
        PF_RGB_ETC1,
        /*
        JIRA issue: DEF-994
        PF_RGB_DXT1,
        PF_RGBA_DXT1,
        PF_RGBA_DXT3,
        PF_RGBA_DXT5
        */
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
    };

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

    /**
     * Texture handle
     */
    typedef void* HTexture;

    /**
     * Invalid texture handle
     */
    const HTexture INVALID_TEXTURE = 0;

#define DM_TEXC_PROTO(ret, name,  ...) \
    \
    ret name(__VA_ARGS__);\
    extern "C" DM_DLLEXPORT ret TEXC_##name(__VA_ARGS__);

    /**
     * Create a texture
     */
    DM_TEXC_PROTO(HTexture, Create, uint32_t width, uint32_t height, PixelFormat pixel_format,
            ColorSpace colorSpace, void* data);
    /**
     * Destroy a texture
     */
    DM_TEXC_PROTO(void, Destroy, HTexture texture);

    DM_TEXC_PROTO(bool, GetHeader, HTexture texture, Header* out_header);
    DM_TEXC_PROTO(uint32_t, GetDataSize, HTexture texture, uint32_t mip_map);
    DM_TEXC_PROTO(uint32_t, GetData, HTexture texture, void* out_data, uint32_t out_data_size);

    /**
     * Resize a texture.
     * The texture must have format PF_R8G8B8A8 to be resized.
     */
    DM_TEXC_PROTO(bool, Resize, HTexture texture, uint32_t width, uint32_t height);
    /**
     * Pre-multiply the color with alpha in a texture.
     * The texture must have format PF_R8G8B8A8 for the alpha to be pre-multiplied.
     */
    DM_TEXC_PROTO(bool, PreMultiplyAlpha, HTexture texture);
    /**
     * Generate mip maps.
     * The texture must have format PF_R8G8B8A8 for mip maps to be generated.
     */
    DM_TEXC_PROTO(bool, GenMipMaps, HTexture texture);
    /**
     * Transcode a texture into another format.
     */
    DM_TEXC_PROTO(bool, Transcode, HTexture texture, PixelFormat pixelFormat, ColorSpace color_space, CompressionLevel compressionLevel);

#undef DM_TEXC_PROTO

}

#endif // DM_TEXC_H
