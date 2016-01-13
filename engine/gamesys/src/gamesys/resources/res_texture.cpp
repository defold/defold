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
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_PVRTC_2BPPV1:
            return dmGraphics::TEXTURE_FORMAT_RGB_PVRTC_2BPPV1;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:
            return dmGraphics::TEXTURE_FORMAT_RGB_PVRTC_4BPPV1;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1:
            return dmGraphics::TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1:
            return dmGraphics::TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_ETC1:
            return dmGraphics::TEXTURE_FORMAT_RGB_ETC1;
        /*
        JIRA issue: DEF-994
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_DXT1:
            return dmGraphics::TEXTURE_FORMAT_RGB_DXT1;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_DXT1:
            return dmGraphics::TEXTURE_FORMAT_RGBA_DXT1;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_DXT3:
            return dmGraphics::TEXTURE_FORMAT_RGBA_DXT3;
        case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_DXT5:
            return dmGraphics::TEXTURE_FORMAT_RGBA_DXT5;
        */
        default:
            assert(0);
        }
    }

    dmResource::Result AcquireResources(dmGraphics::HContext context, dmGraphics::TextureImage* texture_image, dmGraphics::HTexture texture, dmGraphics::HTexture* texture_out)
    {
        bool found_match = false;
        for (uint32_t i = 0; i < texture_image->m_Alternatives.m_Count; ++i)
        {
            dmGraphics::TextureImage::Image* image = &texture_image->m_Alternatives[i];
            dmGraphics::TextureFormat format = TextureImageToTextureFormat(image);

            if (!dmGraphics::IsTextureFormatSupported(context, format))
            {
                continue;
            }

            found_match = true;
            dmGraphics::TextureCreationParams creation_params;
            dmGraphics::TextureParams params;
            dmGraphics::GetDefaultTextureFilters(context, params.m_MinFilter, params.m_MagFilter);
            params.m_Format = format;
            params.m_Width = image->m_Width;
            params.m_Height = image->m_Height;

            if (texture_image->m_Type == dmGraphics::TextureImage::TYPE_2D) {
                creation_params.m_Type = dmGraphics::TEXTURE_TYPE_2D;
            } else if (texture_image->m_Type == dmGraphics::TextureImage::TYPE_CUBEMAP) {
                creation_params.m_Type = dmGraphics::TEXTURE_TYPE_CUBE_MAP;
            } else {
                assert(0);
            }
            creation_params.m_Width = image->m_Width;
            creation_params.m_Height = image->m_Height;
            creation_params.m_OriginalWidth = image->m_OriginalWidth;
            creation_params.m_OriginalHeight = image->m_OriginalHeight;

            if (!texture)
                texture = dmGraphics::NewTexture(context, creation_params);

            // Need to revert to simple bilinear filtering if no mipmaps were supplied
            if (image->m_MipMapOffset.m_Count <= 1) {
                if (params.m_MinFilter == dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST) {
                    params.m_MinFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
                } else if (params.m_MinFilter == dmGraphics::TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST) {
                    params.m_MinFilter = dmGraphics::TEXTURE_FILTER_NEAREST;
                }
            }

            uint32_t max_size = dmGraphics::GetMaxTextureSize(context);
            if (params.m_Width > max_size || params.m_Height > max_size) {
                // SetTexture will fail if texture is too big; fall back to 1x1 texture.
                dmLogError("Texture size %ux%u exceeds maximum supported texture size (%ux%u). Using blank texture.", params.m_Width, params.m_Height, max_size, max_size);
                const static uint8_t blank[6*4] = {0};
                params.m_Width = 1;
                params.m_Height = 1;
                params.m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
                params.m_Data = blank;
                params.m_DataSize = 4;
                params.m_MipMap = 0;
                dmGraphics::SetTexture(texture, params);
                break;
            }

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

    dmResource::Result ResTexturePreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGraphics::TextureImage* texture_image;
        dmDDF::Result e = dmDDF::LoadMessage<dmGraphics::TextureImage>(params.m_Buffer, params.m_BufferSize, (&texture_image));
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        *params.m_PreloadData = texture_image;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResTextureCreate(const dmResource::ResourceCreateParams& params)
    {
        dmGraphics::HContext graphics_context = (dmGraphics::HContext) params.m_Context;
        dmGraphics::HTexture texture;
        dmResource::Result r = AcquireResources(graphics_context, (dmGraphics::TextureImage*) params.m_PreloadData, 0, &texture);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) texture;
        }
        return r;
    }

    dmResource::Result ResTextureDestroy(const dmResource::ResourceDestroyParams& params)
    {
        dmGraphics::DeleteTexture((dmGraphics::HTexture) params.m_Resource->m_Resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResTextureRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmGraphics::TextureImage* texture_image;
        dmDDF::Result e = dmDDF::LoadMessage<dmGraphics::TextureImage>(params.m_Buffer, params.m_BufferSize, (&texture_image));
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmGraphics::HContext graphics_context = (dmGraphics::HContext) params.m_Context;
        dmGraphics::HTexture texture = (dmGraphics::HTexture) params.m_Resource->m_Resource;
        dmResource::Result r = AcquireResources(graphics_context, texture_image, texture, &texture);
        return r;
    }
}
