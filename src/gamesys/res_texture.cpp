#include "res_texture.h"

#include <graphics/graphics_ddf.h>
#include <graphics/graphics_device.h>

namespace dmGameSystem
{
    static dmGraphics::TextureFormat TextureImageToTextureFormat(dmGraphics::TextureImage* image)
    {
        switch (image->m_Format)
        {
        case dmGraphics::TextureImage::LUMINANCE:
            return dmGraphics::TEXTURE_FORMAT_LUMINANCE;
            break;
        case dmGraphics::TextureImage::RGB:
            return dmGraphics::TEXTURE_FORMAT_RGB;
            break;
        case dmGraphics::TextureImage::RGBA:
            return dmGraphics::TEXTURE_FORMAT_RGBA;
            break;
        case dmGraphics::TextureImage::RGB_DXT1:
            return dmGraphics::TEXTURE_FORMAT_RGB_DXT1;
            break;
        case dmGraphics::TextureImage::RGBA_DXT1:
            return dmGraphics::TEXTURE_FORMAT_RGBA_DXT1;
            break;
        case dmGraphics::TextureImage::RGBA_DXT3:
            return dmGraphics::TEXTURE_FORMAT_RGBA_DXT3;
            break;
        case dmGraphics::TextureImage::RGBA_DXT5:
            return dmGraphics::TEXTURE_FORMAT_RGBA_DXT5;
            break;

        default:
            assert(0);
        }

    }

    dmResource::CreateResult ResTextureCreate(dmResource::HFactory factory,
                                           void* context,
                                           const void* buffer, uint32_t buffer_size,
                                           dmResource::SResourceDescriptor* resource,
                                           const char* filename)
    {
        dmGraphics::TextureImage* image;
        dmDDF::Result e = dmDDF::LoadMessage<dmGraphics::TextureImage>(buffer, buffer_size, (&image));
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        dmGraphics::TextureFormat format;
        format = TextureImageToTextureFormat(image);
        dmGraphics::HTexture texture = dmGraphics::CreateTexture(image->m_Width, image->m_Height, format);

        int w = image->m_Width;
        int h = image->m_Height;
        for (int i = 0; i < (int) image->m_MipMapOffset.m_Count; ++i)
        {
            dmGraphics::SetTextureData(texture, i, w, h, 0, format, &image->m_Data[image->m_MipMapOffset[i]], image->m_MipMapSize[i]);
            w >>= 1;
            h >>= 1;
            if (w == 0) w = 1;
            if (h == 0) h = 1;
        }

        dmDDF::FreeMessage(image);

        resource->m_Resource = (void*) texture;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResTextureDestroy(dmResource::HFactory factory,
                                            void* context,
                                            dmResource::SResourceDescriptor* resource)
    {
        dmGraphics::DestroyTexture((dmGraphics::HTexture) resource->m_Resource);
        return dmResource::CREATE_RESULT_OK;
    }
}
