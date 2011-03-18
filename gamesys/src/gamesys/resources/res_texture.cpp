#include "res_texture.h"

#include <graphics/graphics_ddf.h>
#include <graphics/graphics.h>

namespace dmGameSystem
{
    static dmGraphics::TextureFormat TextureImageToTextureFormat(dmGraphics::TextureImage* image)
    {
        switch (image->m_Format)
        {
        case dmGraphics::TextureImage::TEXTURE_FORMAT_LUMINANCE:
            return dmGraphics::TEXTURE_FORMAT_LUMINANCE;
            break;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGB:
            return dmGraphics::TEXTURE_FORMAT_RGB;
            break;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA:
            return dmGraphics::TEXTURE_FORMAT_RGBA;
            break;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_DXT1:
            return dmGraphics::TEXTURE_FORMAT_RGB_DXT1;
            break;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_DXT1:
            return dmGraphics::TEXTURE_FORMAT_RGBA_DXT1;
            break;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_DXT3:
            return dmGraphics::TEXTURE_FORMAT_RGBA_DXT3;
            break;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_DXT5:
            return dmGraphics::TEXTURE_FORMAT_RGBA_DXT5;
            break;

        default:
            assert(0);
        }

    }

    dmGraphics::HTexture AcquireResources(dmGraphics::HContext context, const void* buffer, uint32_t buffer_size, dmGraphics::HTexture texture)
    {
        dmGraphics::TextureImage* image;
        dmDDF::Result e = dmDDF::LoadMessage<dmGraphics::TextureImage>(buffer, buffer_size, (&image));
        if ( e != dmDDF::RESULT_OK )
        {
            return 0;
        }

        dmGraphics::TextureParams params;
        params.m_Format = TextureImageToTextureFormat(image);
        params.m_Width = image->m_Width;
        params.m_Height = image->m_Height;
        if (!texture)
            texture = dmGraphics::NewTexture(context, params);

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

        return texture;
    }

    dmResource::CreateResult ResTextureCreate(dmResource::HFactory factory,
                                           void* context,
                                           const void* buffer, uint32_t buffer_size,
                                           dmResource::SResourceDescriptor* resource,
                                           const char* filename)
    {
        dmGraphics::HContext graphics_context = (dmGraphics::HContext)context;
        dmGraphics::HTexture texture = AcquireResources(graphics_context, buffer, buffer_size, 0);
        if (texture)
        {
            resource->m_Resource = (void*) texture;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }

    dmResource::CreateResult ResTextureDestroy(dmResource::HFactory factory,
                                            void* context,
                                            dmResource::SResourceDescriptor* resource)
    {
        dmGraphics::DeleteTexture((dmGraphics::HTexture) resource->m_Resource);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResTextureRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmGraphics::HContext graphics_context = (dmGraphics::HContext)context;
        dmGraphics::HTexture texture = (dmGraphics::HTexture)resource->m_Resource;
        if (AcquireResources(graphics_context, buffer, buffer_size, texture))
        {
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }
}
