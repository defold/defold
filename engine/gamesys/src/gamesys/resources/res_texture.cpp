#include "res_texture.h"

#include <dlib/log.h>
#include <dlib/webp.h>
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

    void SetBlankTexture(dmGraphics::HTexture texture, dmGraphics::TextureParams& params)
    {
        const static uint8_t blank[6*4] = {0};
        params.m_Width = 1;
        params.m_Height = 1;
        params.m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
        params.m_Data = blank;
        params.m_DataSize = 4;
        params.m_MipMap = 0;
        dmGraphics::SetTexture(texture, params);
    }

    bool WebPDecodeTexture(dmGraphics::HTexture texture, dmGraphics::TextureParams& params, dmGraphics::TextureImage::Image* image)
    {
        uint32_t compressed_data_size = image->m_MipMapSizeCompressed[params.m_MipMap];
        if(!compressed_data_size)
        {
            params.m_Data = &image->m_Data[image->m_MipMapOffset[params.m_MipMap]];
            params.m_DataSize = image->m_MipMapSize[params.m_MipMap];
            dmGraphics::SetTexture(texture, params);
            return true;
        }

        uint8_t* compressed_data = &image->m_Data[image->m_MipMapOffset[params.m_MipMap]];
        uint32_t decompressed_data_size = image->m_MipMapSize[params.m_MipMap];
        uint8_t* decompressed_data = new uint8_t[decompressed_data_size];
        if(!decompressed_data)
        {
            dmLogError("Not enough memory to decode WebP encoded image (%u bytes). Using blank texture.", decompressed_data_size);
            return false;
        }

        uint32_t stride = decompressed_data_size/params.m_Height;
        dmWebP::Result webp_res;
        if(stride == params.m_Width*3)
        {
            webp_res = dmWebP::DecodeRGB(compressed_data, compressed_data_size, decompressed_data, decompressed_data_size, stride);
        }
        else
        {
            webp_res = dmWebP::DecodeRGBA(compressed_data, compressed_data_size, decompressed_data, decompressed_data_size, stride);
        }
        if(webp_res != dmWebP::RESULT_OK)
        {
            dmLogError("Failed to decode WebP encoded image, code(%d). Using blank texture.", (int32_t) webp_res);
            delete[] decompressed_data;
            return false;
        }

        if(image->m_CompressionFlags & dmGraphics::TextureImage::COMPRESSION_FLAG_ALPHA_CLEAN)
        {
            uint32_t* p_end = (uint32_t*)(decompressed_data+decompressed_data_size);
            for(uint32_t* p = (uint32_t*) decompressed_data; p != p_end; ++p)
            {
                uint32_t rgba = *p;
                if((!(rgba & 0xff000000)) && (rgba & 0x00ffffff))
                    *p = 0;
            }
        }

        params.m_DataSize = decompressed_data_size;
        params.m_Data = decompressed_data;
        dmGraphics::SetTexture(texture, params);
        delete[] decompressed_data;
        return true;
    }

    dmResource::Result AcquireResources(dmGraphics::HContext context, dmGraphics::TextureImage* texture_image, dmGraphics::HTexture texture, dmGraphics::HTexture* texture_out)
    {
        dmResource::Result result = dmResource::RESULT_FORMAT_ERROR;
        for (uint32_t i = 0; i < texture_image->m_Alternatives.m_Count; ++i)
        {
            dmGraphics::TextureImage::Image* image = &texture_image->m_Alternatives[i];
            dmGraphics::TextureFormat format = TextureImageToTextureFormat(image);

            if (!dmGraphics::IsTextureFormatSupported(context, format))
            {
                continue;
            }
            result = dmResource::RESULT_OK;

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
                SetBlankTexture(texture, params);
                break;
            }

            for (int i = 0; i < (int) image->m_MipMapOffset.m_Count; ++i)
            {
                params.m_MipMap = i;
                switch(image->m_CompressionType)
                {
                    case dmGraphics::TextureImage::COMPRESSION_TYPE_WEBP:
                    case dmGraphics::TextureImage::COMPRESSION_TYPE_WEBP_LOSSY:
                        if(!WebPDecodeTexture(texture, params, image))
                        {
                            SetBlankTexture(texture, params);
                        }
                        break;

                    default:
                        params.m_Data = &image->m_Data[image->m_MipMapOffset[i]];
                        params.m_DataSize = image->m_MipMapSize[i];
                        dmGraphics::SetTexture(texture, params);
                        break;
                }
                params.m_Width >>= 1;
                params.m_Height >>= 1;
                if (params.m_Width == 0) params.m_Width = 1;
                if (params.m_Height == 0) params.m_Height = 1;
            }
            break;
        }

        if (result == dmResource::RESULT_OK)
        {
            *texture_out = texture;
            return dmResource::RESULT_OK;
        }
        if (result == dmResource::RESULT_FORMAT_ERROR)
        {
            dmLogWarning("No matching texture format found");
        }
        return result;
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
        dmDDF::FreeMessage(params.m_PreloadData);
        return r;
    }

    dmResource::Result ResTextureDestroy(const dmResource::ResourceDestroyParams& params)
    {
        dmGraphics::DeleteTexture((dmGraphics::HTexture) params.m_Resource->m_Resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResTextureRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmGraphics::TextureImage* texture_image = (dmGraphics::TextureImage*)params.m_Message;
        if( !texture_image )
        {
            dmDDF::Result e = dmDDF::LoadMessage<dmGraphics::TextureImage>(params.m_Buffer, params.m_BufferSize, (&texture_image));
            if ( e != dmDDF::RESULT_OK )
            {
                return dmResource::RESULT_FORMAT_ERROR;
            }
        }
        dmGraphics::HContext graphics_context = (dmGraphics::HContext) params.m_Context;
        dmGraphics::HTexture texture = (dmGraphics::HTexture) params.m_Resource->m_Resource;
        dmResource::Result r = AcquireResources(graphics_context, texture_image, texture, &texture);

        if( params.m_Message == 0 )
        {
            dmDDF::FreeMessage(texture_image);
        }
        return r;
    }
}
