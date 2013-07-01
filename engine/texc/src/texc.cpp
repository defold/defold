#include "texc.h"

#include <assert.h>
#include <unistd.h>

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
        case PF_L8:
            return pvrtexture::PixelType('l', 0, 0, 0, 8, 0, 0, 0);
        case PF_R8G8B8:
            return pvrtexture::PixelType('r', 'g', 'b', 0, 8, 8, 8, 0);
        case PF_R8G8B8A8:
            return pvrtexture::PixelType('r', 'g', 'b', 'a', 8, 8, 8, 8);
        }
        // Should never get here
        assert(false);
        return pvrtexture::PixelType();
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
        return CS_SRGB;
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
            assert(sizeof(Header) == sizeof(PVRTextureHeaderV3));
            PVRTextureHeaderV3 src = ((pvrtexture::CPVRTexture*)texture)->getFileHeader();
            memcpy(out_header, &src, sizeof(PVRTextureHeaderV3));
            return true;
        }
        else
        {
            return false;
        }
    }

    uint32_t GetData(HTexture texture, void* out_data, uint32_t out_data_size)
    {
        uint8_t* out = (uint8_t*)out_data;
        pvrtexture::CPVRTexture* t = (pvrtexture::CPVRTexture*)texture;
        uint32_t offset = 0;
        uint32_t mip_maps = t->getNumMIPLevels();
        for (uint32_t mip_map = 0; mip_map < mip_maps; ++mip_map)
        {
            uint32_t size = t->getTextureSize(mip_map) * t->getBitsPerPixel() / 8;
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

    bool Transcode(HTexture texture, PixelFormat pixel_format, ColorSpace color_space)
    {
        pvrtexture::PixelType pf = ConvertPixelFormat(pixel_format);
        EPVRTVariableType var_type = ePVRTVarTypeUnsignedByteNorm;
        EPVRTColourSpace cs = ConvertColorSpace(color_space);
        pvrtexture::ECompressorQuality quality = pvrtexture::ePVRTCNormal;
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
    DM_TEXC_TRAMPOLINE3(uint32_t, GetData, HTexture, void*, uint32_t);
    DM_TEXC_TRAMPOLINE3(bool, Resize, HTexture, uint32_t, uint32_t);
    DM_TEXC_TRAMPOLINE1(bool, PreMultiplyAlpha, HTexture);
    DM_TEXC_TRAMPOLINE1(bool, GenMipMaps, HTexture);
    DM_TEXC_TRAMPOLINE3(bool, Transcode, HTexture, PixelFormat, ColorSpace);
}
