#include "texc.h"
#include "texc_private.h"

#include <assert.h>

#include <dlib/log.h>
#include <dlib/math.h>

#include <PVRTexture.h>
#include <PVRTextureUtilities.h>

namespace dmTexc
{
    static pvrtexture::PixelType ConvertPixelFormat(PixelFormat pixel_format)
    {
        switch (pixel_format)
        {
        // Raw
        case PF_L8:
            return pvrtexture::PixelType('l', 0, 0, 0, 8, 0, 0, 0);
        case PF_L8A8:
            return pvrtexture::PixelType('l', 'a', 0, 0, 8, 8, 0, 0);
        case PF_R8G8B8:
            return pvrtexture::PixelType('r', 'g', 'b', 0, 8, 8, 8, 0);
        case PF_R8G8B8A8:
            return pvrtexture::PixelType('r', 'g', 'b', 'a', 8, 8, 8, 8);
        case PF_R5G6B5:
            return pvrtexture::PixelType('r', 'g', 'b', 0, 5, 6, 5, 0);
        case PF_R4G4B4A4:
            return pvrtexture::PixelType('r', 'g', 'b', 'a', 4, 4, 4, 4);

        // PVRTC
        case PF_RGB_PVRTC_2BPPV1:
            return ePVRTPF_PVRTCI_2bpp_RGB;
        case PF_RGB_PVRTC_4BPPV1:
            return ePVRTPF_PVRTCI_4bpp_RGB;
        case PF_RGBA_PVRTC_2BPPV1:
            return ePVRTPF_PVRTCI_2bpp_RGBA;
        case PF_RGBA_PVRTC_4BPPV1:
            return ePVRTPF_PVRTCI_4bpp_RGBA;

        // ETC
        case PF_RGB_ETC1:
            return ePVRTPF_ETC1;

        // DXT
        /*
        JIRA issue: DEF-994
        case PF_RGB_DXT1:
        case PF_RGBA_DXT1:
            return ePVRTPF_DXT1;
        case PF_RGBA_DXT3:
            return ePVRTPF_DXT3;
        case PF_RGBA_DXT5:
            return ePVRTPF_DXT5;
            break;
        */
            default:
                break;
        }
        // Should never get here
        assert(false);
        return pvrtexture::PixelType();
    }

    static pvrtexture::ECompressorQuality ConvertCompressionLevel(CompressionLevel compression_level)
    {
        switch (compression_level)
        {
            case CL_FAST:
                return pvrtexture::ePVRTCFast;
            case CL_NORMAL:
                return pvrtexture::ePVRTCNormal;
            case CL_HIGH:
                return pvrtexture::ePVRTCHigh;
            case CL_BEST:
                return pvrtexture::ePVRTCBest;
            default:
                dmLogError("Unknown compression level (%d). Using normal level", (uint32_t) compression_level);
                break;
        }

        return pvrtexture::ePVRTCNormal;
    }

    static EPVRTColourSpace ConvertColorSpace(ColorSpace color_space)
    {
        switch (color_space)
        {
        case CS_LRGB:
            return ePVRTCSpacelRGB;
        case CS_SRGB:
            return ePVRTCSpacesRGB;
        }
        // Should never get here
        assert(false);
        return ePVRTCSpacesRGB;
    }

    static EPVRTAxis ConvertFlipAxis(FlipAxis flip_axis)
    {
        switch (flip_axis)
        {
            case FLIP_AXIS_X:
                return ePVRTAxisX;
            case FLIP_AXIS_Y:
                return ePVRTAxisY;
            case FLIP_AXIS_Z:
                return ePVRTAxisZ;
        }

        // Should never get here
        assert(false);
        return ePVRTAxisX;
    }

    HTexture Create(uint32_t width, uint32_t height, PixelFormat pixel_format, ColorSpace color_space, void* data)
    {
        pvrtexture::PixelType pf = ConvertPixelFormat(pixel_format);
        EPVRTColourSpace cs = ConvertColorSpace(color_space);
        uint32_t depth = 1;
        uint32_t num_mip_maps = 1;
        uint32_t num_array_members = 1;
        uint32_t num_faces = 1;
        EPVRTVariableType var_type = ePVRTVarTypeUnsignedByteNorm;
        bool pre_multiplied = false;
        pvrtexture::CPVRTextureHeader header(pf.PixelTypeID, height, width, depth, num_mip_maps,
                num_array_members, num_faces, cs, var_type, pre_multiplied);

        pvrtexture::CPVRTexture* pvrt_tex = (pvrtexture::CPVRTexture*) new pvrtexture::CPVRTexture(header, data);
        if(!pvrt_tex)
        {
            dmLogError("Failed to create PVRTTexture");
            return 0;
        }

        Texture* t = new Texture;
        t->m_CompressedMips.SetCapacity(16); //  counts for 64kpix textures
        t->m_PVRTexture = pvrt_tex;
        t->m_CompressionFlags = 0;
        return t;
    }

    void Destroy(HTexture texture)
    {
        Texture* t = (Texture *) texture;
        for(size_t i = 0; i < t->m_CompressedMips.Size(); ++i)
        {
            delete[] t->m_CompressedMips[i].m_Data;
        }
        if(t->m_PVRTexture)
        {
            delete t->m_PVRTexture;
        }
        delete t;
    }

    bool GetHeader(HTexture texture, Header* out_header)
    {
        Texture* t = (Texture*) texture;
        if (t != dmTexc::INVALID_TEXTURE && t->m_PVRTexture != 0x0 && out_header != 0x0)
        {
            PVRTextureHeaderV3 src = t->m_PVRTexture->getFileHeader();
            out_header->m_Version = src.u32Version;
            out_header->m_Flags = src.u32Flags;
            out_header->m_PixelFormat = src.u64PixelFormat;
            out_header->m_ColourSpace = src.u32ColourSpace;
            out_header->m_ChannelType = src.u32ChannelType;
            out_header->m_Height = src.u32Height;
            out_header->m_Width = src.u32Width;
            out_header->m_Depth = src.u32Depth;
            out_header->m_NumSurfaces = src.u32NumSurfaces;
            out_header->m_NumFaces = src.u32NumFaces;
            out_header->m_MipMapCount = src.u32MIPMapCount;
            out_header->m_MetaDataSize = src.u32MetaDataSize;
            return true;
        }
        else
        {
            return false;
        }
    }

    uint32_t GetDataSizeCompressed(HTexture texture, uint32_t mip_map)
    {
        Texture* t = (Texture*) texture;
        return mip_map < t->m_CompressedMips.Size() ? t->m_CompressedMips[mip_map].m_ByteSize : 0;
    }

    uint32_t GetDataSizeUncompressed(HTexture texture, uint32_t mip_map)
    {
        Texture* t = (Texture*) texture;
        return mip_map < t->m_PVRTexture->getNumMIPLevels() ? t->m_PVRTexture->getDataSize(mip_map) : 0;
    }

    uint32_t GetTotalDataSize(HTexture texture)
    {
        Texture* t = (Texture*) texture;
        size_t size = 0;
        size_t mip_maps = t->m_PVRTexture->getNumMIPLevels();
        for (size_t mip_map = 0; mip_map < mip_maps; ++mip_map)
        {
            size += mip_map < t->m_CompressedMips.Size() ? t->m_CompressedMips[mip_map].m_ByteSize : t->m_PVRTexture->getDataSize(mip_map);
        }
        return size;
    }

    uint32_t GetData(HTexture texture, void* out_data, uint32_t out_data_size)
    {
        Texture* t = (Texture*) texture;
        uint32_t offset = 0;
        uint8_t* out = (uint8_t*)out_data;
        uint32_t mip_maps = t->m_PVRTexture->getNumMIPLevels();
        for (uint32_t mip_map = 0; mip_map < mip_maps; ++mip_map)
        {
            uint32_t size;
            void* data;
            if(mip_map < t->m_CompressedMips.Size())
            {
                size = t->m_CompressedMips[mip_map].m_ByteSize;
                data = t->m_CompressedMips[mip_map].m_Data;
            }
            else
            {
                size = t->m_PVRTexture->getDataSize(mip_map);
                data = t->m_PVRTexture->getDataPtr(mip_map);
            }
            dmMath::Min(size, out_data_size - offset);
            memcpy(out, data, size);
            offset += size;
            out += size;
            if (offset >= out_data_size)
                break;
        }
        return offset;
    }

    uint64_t GetCompressionFlags(HTexture texture)
    {
        Texture* t = (Texture*) texture;
        return t->m_CompressionFlags;
    }

    bool Resize(HTexture texture, uint32_t width, uint32_t height)
    {
        Texture* t = (Texture*) texture;
        return pvrtexture::Resize(*t->m_PVRTexture, width, height, t->m_PVRTexture->getDepth(), pvrtexture::eResizeLinear);
    }

    bool PreMultiplyAlpha(HTexture texture)
    {
        Texture* t = (Texture*) texture;
        return pvrtexture::PreMultiplyAlpha(*t->m_PVRTexture);
    }

    bool GenMipMaps(HTexture texture)
    {
        Texture* t = (Texture*) texture;
        return pvrtexture::GenerateMIPMaps(*t->m_PVRTexture, pvrtexture::eResizeLinear);
    }

    bool Flip(HTexture texture, FlipAxis flip_axis)
    {
        Texture* t = (Texture*) texture;
        return pvrtexture::Flip(*t->m_PVRTexture, ConvertFlipAxis(flip_axis));
    }

    bool Transcode(HTexture texture, PixelFormat pixel_format, ColorSpace color_space, CompressionLevel compression_level, CompressionType compression_type, DitherType dither_type)
    {
        Texture* t = (Texture*) texture;
        pvrtexture::PixelType pf = ConvertPixelFormat(pixel_format);
        EPVRTVariableType var_type = ePVRTVarTypeUnsignedByteNorm;
        EPVRTColourSpace cs = ConvertColorSpace(color_space);
        pvrtexture::ECompressorQuality quality = ConvertCompressionLevel(compression_level);
        if(!pvrtexture::Transcode(*t->m_PVRTexture, pf, var_type, cs, quality, dither_type == DT_DEFAULT))
        {
            dmLogError("Failed to transcode texture");
            return false;
        }

        if(!t->m_CompressedMips.Empty())
        {
            for(size_t i = 0; i < t->m_CompressedMips.Size(); ++i)
            {
                delete[] t->m_CompressedMips[i].m_Data;
            }
            t->m_CompressedMips.SetSize(0);
        }

        switch(compression_type)
        {
            case CT_WEBP:
            case CT_WEBP_LOSSY:
                 if(!CompressWebP(texture, pixel_format, color_space, compression_level, compression_type))
                 {
                     dmLogError("Failed to compress texture with WebP compression");
                     return false;
                 }
                 break;
            default:
                break;
        }
        return true;
    }

#define DM_TEXC_TRAMPOLINE1(ret, name, t1) \
    ret TEXC_##name(t1 a1)\
    {\
        return name(a1);\
    }\

#define DM_TEXC_TRAMPOLINE2(ret, name, t1, t2) \
    ret TEXC_##name(t1 a1, t2 a2)\
    {\
        return name(a1, a2);\
    }\

#define DM_TEXC_TRAMPOLINE3(ret, name, t1, t2, t3) \
    ret TEXC_##name(t1 a1, t2 a2, t3 a3)\
    {\
        return name(a1, a2, a3);\
    }\

#define DM_TEXC_TRAMPOLINE4(ret, name, t1, t2, t3, t4) \
    ret TEXC_##name(t1 a1, t2 a2, t3 a3, t4 a4)\
    {\
        return name(a1, a2, a3, a4);\
    }\

#define DM_TEXC_TRAMPOLINE5(ret, name, t1, t2, t3, t4, t5) \
    ret TEXC_##name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5)\
    {\
        return name(a1, a2, a3, a4, a5);\
    }\

#define DM_TEXC_TRAMPOLINE6(ret, name, t1, t2, t3, t4, t5, t6) \
    ret TEXC_##name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6)\
    {\
        return name(a1, a2, a3, a4, a5, a6);\
    }\

    DM_TEXC_TRAMPOLINE5(HTexture, Create, uint32_t, uint32_t, PixelFormat, ColorSpace, void*);
    DM_TEXC_TRAMPOLINE1(void, Destroy, HTexture);
    DM_TEXC_TRAMPOLINE2(bool, GetHeader, HTexture, Header*);
    DM_TEXC_TRAMPOLINE2(uint32_t, GetDataSizeCompressed, HTexture, uint32_t);
    DM_TEXC_TRAMPOLINE2(uint32_t, GetDataSizeUncompressed, HTexture, uint32_t);
    DM_TEXC_TRAMPOLINE1(uint32_t, GetTotalDataSize, HTexture);
    DM_TEXC_TRAMPOLINE3(uint32_t, GetData, HTexture, void*, uint32_t);
    DM_TEXC_TRAMPOLINE1(uint64_t, GetCompressionFlags, HTexture);
    DM_TEXC_TRAMPOLINE3(bool, Resize, HTexture, uint32_t, uint32_t);
    DM_TEXC_TRAMPOLINE1(bool, PreMultiplyAlpha, HTexture);
    DM_TEXC_TRAMPOLINE1(bool, GenMipMaps, HTexture);
    DM_TEXC_TRAMPOLINE2(bool, Flip, HTexture, FlipAxis);
    DM_TEXC_TRAMPOLINE6(bool, Transcode, HTexture, PixelFormat, ColorSpace, CompressionLevel, CompressionType, DitherType);
}
