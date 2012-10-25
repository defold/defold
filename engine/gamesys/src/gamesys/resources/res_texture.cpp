#include "res_texture.h"

#include <dlib/log.h>
#include <graphics/graphics_ddf.h>
#include <graphics/graphics.h>

namespace dmGameSystem
{
    static dmGraphics::TextureFormat TextureImageToTextureFormat(dmGraphics::TextureImage::Image* image)
    {
        switch (image->m_Format)
        {
        case dmGraphics::TextureImage::TEXTURE_FORMAT_LUMINANCE:
            return dmGraphics::TEXTURE_FORMAT_LUMINANCE;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGB:
            return dmGraphics::TEXTURE_FORMAT_RGB;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA:
            return dmGraphics::TEXTURE_FORMAT_RGBA;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_DXT1:
            return dmGraphics::TEXTURE_FORMAT_RGB_DXT1;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_DXT1:
            return dmGraphics::TEXTURE_FORMAT_RGBA_DXT1;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_DXT3:
            return dmGraphics::TEXTURE_FORMAT_RGBA_DXT3;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_DXT5:
            return dmGraphics::TEXTURE_FORMAT_RGBA_DXT5;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_PVRTC_2BPPV1:
            return dmGraphics::TEXTURE_FORMAT_RGB_PVRTC_2BPPV1;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:
            return dmGraphics::TEXTURE_FORMAT_RGB_PVRTC_4BPPV1;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1:
            return dmGraphics::TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1:
            return dmGraphics::TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1;
        default:
            assert(0);
        }
    }

    dmResource::Result AcquireResources(dmGraphics::HContext context, const void* buffer, uint32_t buffer_size, dmGraphics::HTexture texture, dmGraphics::HTexture* texture_out)
    {
        *texture_out = 0;
        dmGraphics::TextureImage* texture_image;
        dmDDF::Result e = dmDDF::LoadMessage<dmGraphics::TextureImage>(buffer, buffer_size, (&texture_image));
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        bool found_match = false;
        for (uint32_t i = 0; i < texture_image->m_Alternatives.m_Count; ++i)
        {
            dmGraphics::TextureImage::Image* image = &texture_image->m_Alternatives[i];
            dmGraphics::TextureFormat format = TextureImageToTextureFormat(image);

            if (!dmGraphics::IsTextureFormatSupported(format))
            {
                continue;
            }

            found_match = true;
            dmGraphics::TextureParams params;
            dmGraphics::GetDefaultTextureFilters(context, params.m_MinFilter, params.m_MagFilter);
            params.m_Format = format;
            params.m_Width = image->m_Width;
            params.m_Height = image->m_Height;
            params.m_OriginalWidth = image->m_OriginalWidth;
            params.m_OriginalHeight = image->m_OriginalHeight;
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
            break;
        }

        dmDDF::FreeMessage(texture_image);

        if (found_match)
        {
            *texture_out = texture;
            return dmResource::RESULT_OK;
        }
        else
        {
            dmLogWarning("No matching texture format found");
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }

    dmResource::Result ResTextureCreate(dmResource::HFactory factory,
                                           void* context,
                                           const void* buffer, uint32_t buffer_size,
                                           dmResource::SResourceDescriptor* resource,
                                           const char* filename)
    {
        dmGraphics::HContext graphics_context = (dmGraphics::HContext)context;
        dmGraphics::HTexture texture;
        dmResource::Result r = AcquireResources(graphics_context, buffer, buffer_size, 0, &texture);
        if (r == dmResource::RESULT_OK)
        {
            resource->m_Resource = (void*) texture;
        }
        return r;
    }

    dmResource::Result ResTextureDestroy(dmResource::HFactory factory,
                                            void* context,
                                            dmResource::SResourceDescriptor* resource)
    {
        dmGraphics::DeleteTexture((dmGraphics::HTexture) resource->m_Resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResTextureRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmGraphics::HContext graphics_context = (dmGraphics::HContext)context;
        dmGraphics::HTexture texture = (dmGraphics::HTexture)resource->m_Resource;
        dmResource::Result r = AcquireResources(graphics_context, buffer, buffer_size, texture, &texture);
        return r;
    }
}
