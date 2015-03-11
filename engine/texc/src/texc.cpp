#include "texc.h"

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
        case PF_R8G8B8:
            return pvrtexture::PixelType('r', 'g', 'b', 0, 8, 8, 8, 0);
        case PF_R8G8B8A8:
            return pvrtexture::PixelType('r', 'g', 'b', 'a', 8, 8, 8, 8);

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
        return new pvrtexture::CPVRTexture(header, data);
    }

    void Destroy(HTexture texture)
    {
        delete (pvrtexture::CPVRTexture*)texture;
    }

    bool GetHeader(HTexture texture, Header* out_header)
    {
        if (texture != INVALID_TEXTURE && out_header != 0x0)
        {
            PVRTextureHeaderV3 src = ((pvrtexture::CPVRTexture*)texture)->getFileHeader();
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

    uint32_t GetDataSize(HTexture texture, uint32_t mip_map)
    {
        pvrtexture::CPVRTexture* t = (pvrtexture::CPVRTexture*)texture;
        uint32_t size = t->getDataSize(mip_map);
        return size;
    }

    uint32_t GetData(HTexture texture, void* out_data, uint32_t out_data_size)
    {
        uint8_t* out = (uint8_t*)out_data;
        pvrtexture::CPVRTexture* t = (pvrtexture::CPVRTexture*)texture;
        uint32_t offset = 0;
        uint32_t mip_maps = t->getNumMIPLevels();
        for (uint32_t mip_map = 0; mip_map < mip_maps; ++mip_map)
        {
            uint32_t size = GetDataSize(t, mip_map);
            size = dmMath::Min(size, out_data_size - offset);
            void* data = t->getDataPtr(mip_map);
            memcpy(out, data, size);
            offset += size;
            out += size;
            if (offset >= out_data_size)
                break;
        }
        return offset;
    }

    bool Resize(HTexture texture, uint32_t width, uint32_t height)
    {
        pvrtexture::CPVRTexture* t = (pvrtexture::CPVRTexture*)texture;
        return pvrtexture::Resize(*t, width, height, t->getDepth(), pvrtexture::eResizeLinear);
    }

    bool PreMultiplyAlpha(HTexture texture)
    {
        pvrtexture::CPVRTexture* t = (pvrtexture::CPVRTexture*)texture;
        return pvrtexture::PreMultiplyAlpha(*t);
    }

    bool GenMipMaps(HTexture texture)
    {
        pvrtexture::CPVRTexture* t = (pvrtexture::CPVRTexture*)texture;
        return pvrtexture::GenerateMIPMaps(*t, pvrtexture::eResizeLinear);
    }

    bool Transcode(HTexture texture, PixelFormat pixel_format, ColorSpace color_space, CompressionLevel compression_level)
    {
        pvrtexture::PixelType pf = ConvertPixelFormat(pixel_format);
        EPVRTVariableType var_type = ePVRTVarTypeUnsignedByteNorm;
        EPVRTColourSpace cs = ConvertColorSpace(color_space);
        pvrtexture::ECompressorQuality quality = ConvertCompressionLevel(compression_level);
        bool dither = true;
        return pvrtexture::Transcode(*(pvrtexture::CPVRTexture*)texture, pf, var_type,
                cs, quality, dither);
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

    DM_TEXC_TRAMPOLINE5(HTexture, Create, uint32_t, uint32_t, PixelFormat, ColorSpace, void*);
    DM_TEXC_TRAMPOLINE1(void, Destroy, HTexture);
    DM_TEXC_TRAMPOLINE2(bool, GetHeader, HTexture, Header*);
    DM_TEXC_TRAMPOLINE2(uint32_t, GetDataSize, HTexture, uint32_t);
    DM_TEXC_TRAMPOLINE3(uint32_t, GetData, HTexture, void*, uint32_t);
    DM_TEXC_TRAMPOLINE3(bool, Resize, HTexture, uint32_t, uint32_t);
    DM_TEXC_TRAMPOLINE1(bool, PreMultiplyAlpha, HTexture);
    DM_TEXC_TRAMPOLINE1(bool, GenMipMaps, HTexture);
    DM_TEXC_TRAMPOLINE4(bool, Transcode, HTexture, PixelFormat, ColorSpace, CompressionLevel);
}
