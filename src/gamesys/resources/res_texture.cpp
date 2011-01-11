#include "res_texture.h"

#include <graphics/graphics_ddf.h>
#include <graphics/graphics.h>

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
        dmGraphics::HContext graphics_context = (dmGraphics::HContext)context;
        dmGraphics::TextureImage* image;
        dmDDF::Result e = dmDDF::LoadMessage<dmGraphics::TextureImage>(buffer, buffer_size, (&image));
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        dmGraphics::TextureParams params;
        params.m_Format = TextureImageToTextureFormat(image);
        params.m_Width = image->m_Width;
        params.m_Height = image->m_Height;
        dmGraphics::HTexture texture = dmGraphics::NewTexture(graphics_context, params);

        for (int i = 0; i < (int) image->m_MipMapOffset.m_Count; ++i)
        {
            params.m_MipMap = i;
            params.m_Data = &image->m_Data[image->m_MipMapOffset[i]];
            params.m_DataSize = image->m_MipMapSize[i];

            dmGraphics::SetTexture(texture, params);
            params.m_Width >>= 1;
            params.m_Height >>= 1;
            if (params.m_Width == 0) params.m_Width = 1;
            if (params.m_Height == 0) params.m_Height = 1;
        }

        dmDDF::FreeMessage(image);

        resource->m_Resource = (void*) texture;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResTextureDestroy(dmResource::HFactory factory,
                                            void* context,
                                            dmResource::SResourceDescriptor* resource)
    {
        dmGraphics::DeleteTexture((dmGraphics::HTexture) resource->m_Resource);
        return dmResource::CREATE_RESULT_OK;
    }
}
