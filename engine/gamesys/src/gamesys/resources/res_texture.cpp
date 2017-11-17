#include "res_texture.h"

#include <dlib/log.h>
#include <dlib/webp.h>
#include <dlib/time.h>
#include <graphics/graphics_ddf.h>
#include <graphics/graphics.h>

namespace dmGameSystem
{
    static const uint32_t m_MaxMipCount = 32;
    struct ImageDesc
    {
        dmGraphics::TextureImage* m_DDFImage;
        uint8_t* m_DecompressedData[m_MaxMipCount];
        bool m_UseBlankTexture;
    };

    static dmWebP::TextureEncodeFormat TextureFormatFormatToEncodeFormat(dmGraphics::TextureImage::TextureFormat format)
    {
        switch (format)
        {
            case dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_PVRTC_2BPPV1:
            case dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:
            case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1:
            case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1:
                return dmWebP::TEXTURE_ENCODE_FORMAT_PVRTC1;
            case dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_ETC1:
                return dmWebP::TEXTURE_ENCODE_FORMAT_ETC1;
            case dmGraphics::TextureImage::TEXTURE_FORMAT_LUMINANCE:
                return dmWebP::TEXTURE_ENCODE_FORMAT_L8;
            case dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_16BPP:
                return dmWebP::TEXTURE_ENCODE_FORMAT_RGB565;
            case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_16BPP:
                return dmWebP::TEXTURE_ENCODE_FORMAT_RGBA4444;
            case dmGraphics::TextureImage::TEXTURE_FORMAT_LUMINANCE_ALPHA:
                return dmWebP::TEXTURE_ENCODE_FORMAT_L8A8;
            default:
                assert(0);
        }
    }

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
            case dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_16BPP:
                return dmGraphics::TEXTURE_FORMAT_RGB_16BPP;
            case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_16BPP:
                return dmGraphics::TEXTURE_FORMAT_RGBA_16BPP;
            case dmGraphics::TextureImage::TEXTURE_FORMAT_LUMINANCE_ALPHA:
                return dmGraphics::TEXTURE_FORMAT_LUMINANCE_ALPHA;

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

    bool SynchronizeTexture(dmGraphics::HTexture texture, bool wait)
    {
        while(dmGraphics::GetTextureStatusFlags(texture) & dmGraphics::TEXTURE_STATUS_DATA_PENDING)
        {
            if(!wait)
                return false;
            dmTime::Sleep(250);
        }
        return true;
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
        dmGraphics::SetTextureAsync(texture, params);
    }

    bool WebPDecodeTexture(uint32_t mipmap, uint32_t width, int32_t height, dmGraphics::TextureImage::Image* image, uint8_t*& decompressed_data, uint32_t& decompressed_data_size)
    {
        uint32_t compressed_data_size = image->m_MipMapSizeCompressed[mipmap];
        if(!compressed_data_size)
        {
            decompressed_data = 0;
            decompressed_data_size = 0;
            return true;
        }

        uint8_t* compressed_data = &image->m_Data[image->m_MipMapOffset[mipmap]];
        decompressed_data_size = image->m_MipMapSize[mipmap];
        decompressed_data = new uint8_t[decompressed_data_size];
        if(!decompressed_data)
        {
            dmLogError("Not enough memory to decode WebP encoded image (%u bytes). Using blank texture.", decompressed_data_size);
            return false;
        }

        dmWebP::Result webp_res;
        uint32_t stride = decompressed_data_size/height;
        switch (image->m_Format)
        {
            case dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_PVRTC_2BPPV1:
            case dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:
            case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1:
            case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1:
            case dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_ETC1:
            case dmGraphics::TextureImage::TEXTURE_FORMAT_LUMINANCE:
            case dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_16BPP:
            case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_16BPP:
            case dmGraphics::TextureImage::TEXTURE_FORMAT_LUMINANCE_ALPHA:
                webp_res = dmWebP::DecodeCompressedTexture(compressed_data, compressed_data_size, decompressed_data, decompressed_data_size, stride, TextureFormatFormatToEncodeFormat(image->m_Format));
            break;

            default:
                if(stride == width*3)
                {
                    webp_res = dmWebP::DecodeRGB(compressed_data, compressed_data_size, decompressed_data, decompressed_data_size, stride);
                }
                else
                {
                    webp_res = dmWebP::DecodeRGBA(compressed_data, compressed_data_size, decompressed_data, decompressed_data_size, stride);
                }
            break;
        }
        if(webp_res != dmWebP::RESULT_OK)
        {
            dmLogError("Failed to decode WebP encoded image, code(%d). Using blank texture.", (int32_t) webp_res);
            delete[] decompressed_data;
            return false;
        }

        if(image->m_CompressionFlags & dmGraphics::TextureImage::COMPRESSION_FLAG_ALPHA_CLEAN)
        {
            switch (image->m_Format)
            {
                case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA:
                {
                    uint32_t* p_end = (uint32_t*)(decompressed_data+decompressed_data_size);
                    for(uint32_t* p = (uint32_t*) decompressed_data; p != p_end; ++p)
                    {
                        uint32_t rgba = *p;
                        if((!(rgba & 0xff000000)) && (rgba & 0x00ffffff))
                            *p = 0;
                    }
                }
                break;

                case dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_16BPP:
                {
                    uint16_t* p_end = (uint16_t*)(decompressed_data+decompressed_data_size);
                    for(uint16_t* p = (uint16_t*) decompressed_data; p != p_end; ++p)
                    {
                        uint16_t rgba = *p;
                        if((!(rgba & 0x000f)) && (rgba & 0xfff0))
                            *p = 0;
                    }
                }
                break;

                case dmGraphics::TextureImage::TEXTURE_FORMAT_LUMINANCE_ALPHA:
                {
                    uint16_t* p_end = (uint16_t*)(decompressed_data+decompressed_data_size);
                    for(uint16_t* p = (uint16_t*) decompressed_data; p != p_end; ++p)
                    {
                        uint16_t l8a8 = *p;
                        if((!(l8a8 & 0xff00)) && (l8a8 & 0x00ff))
                            *p = 0;
                    }
                }
                break;

                default:
                break;
            }
        }
        return true;
    }

    dmResource::Result AcquireResources(dmResource::SResourceDescriptor* resource_desc, dmGraphics::HContext context, ImageDesc* image_desc, dmGraphics::HTexture texture, dmGraphics::HTexture* texture_out)
    {
        dmResource::Result result = dmResource::RESULT_FORMAT_ERROR;
        for (uint32_t i = 0; i < image_desc->m_DDFImage->m_Alternatives.m_Count; ++i)
        {
            dmGraphics::TextureImage::Image* image = &image_desc->m_DDFImage->m_Alternatives[i];
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

            assert(image->m_MipMapOffset.m_Count <= m_MaxMipCount);

            if (image_desc->m_DDFImage->m_Type == dmGraphics::TextureImage::TYPE_2D) {
                creation_params.m_Type = dmGraphics::TEXTURE_TYPE_2D;
            } else if (image_desc->m_DDFImage->m_Type == dmGraphics::TextureImage::TYPE_CUBEMAP) {
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
                // dmGraphics::SetTextureAsync will fail if texture is too big; fall back to 1x1 texture.
                dmLogError("Texture size %ux%u exceeds maximum supported texture size (%ux%u). Using blank texture.", params.m_Width, params.m_Height, max_size, max_size);
                SetBlankTexture(texture, params);
                break;
            }

            if(image_desc->m_UseBlankTexture)
            {
                SetBlankTexture(texture, params);
                break;
            }

            for (int i = 0; i < (int) image->m_MipMapOffset.m_Count; ++i)
            {
                params.m_MipMap = i;
                params.m_Data = image_desc->m_DecompressedData[i] == 0 ? &image->m_Data[image->m_MipMapOffset[i]] : image_desc->m_DecompressedData[i];
                params.m_DataSize = image->m_MipMapSize[i];
                dmGraphics::SetTextureAsync(texture, params);

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

    ImageDesc* CreateImage(dmGraphics::HContext context, dmGraphics::TextureImage* texture_image)
    {
        ImageDesc* image_desc = new ImageDesc;
        memset(image_desc, 0x0, sizeof(ImageDesc));
        image_desc->m_DDFImage = texture_image;
        for(uint32_t i = 0; i < texture_image->m_Alternatives.m_Count; ++i)
        {
            dmGraphics::TextureImage::Image* image = &texture_image->m_Alternatives[i];
            if (!dmGraphics::IsTextureFormatSupported(context, TextureImageToTextureFormat(image)))
            {
                continue;
            }
            switch(image->m_CompressionType)
            {
                case dmGraphics::TextureImage::COMPRESSION_TYPE_WEBP:
                case dmGraphics::TextureImage::COMPRESSION_TYPE_WEBP_LOSSY:
                {
                    dmGraphics::TextureImage::Image* new_image = new dmGraphics::TextureImage::Image();
                    memcpy(new_image, image, sizeof(dmGraphics::TextureImage::Image));

                    uint32_t w = image->m_Width;
                    uint32_t h = image->m_Height;
                    for (int i = 0; i < (int) image->m_MipMapOffset.m_Count; ++i)
                    {
                        uint8_t* decompressed_data;
                        uint32_t decompressed_data_size;
                        if(WebPDecodeTexture(i, w, h, image, decompressed_data, decompressed_data_size))
                        {
                            image_desc->m_DecompressedData[i] = decompressed_data;
                        }
                        else
                        {
                            image_desc->m_UseBlankTexture = true;
                            break;
                        }
                        w >>= 1;
                        h >>= 1;
                        if (w == 0) w = 1;
                        if (h == 0) h = 1;
                    }
                }
                break;

                default:
                break;
            }
            break;
        }
        return image_desc;
    }

    void DestroyImage(ImageDesc* image_desc)
    {
        for (int i = 0; i < m_MaxMipCount; ++i)
        {
            if(image_desc->m_DecompressedData[i])
                delete[] image_desc->m_DecompressedData[i];
        }
        delete image_desc;
    }

    dmResource::Result ResTexturePreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGraphics::TextureImage* texture_image;
        dmDDF::Result e = dmDDF::LoadMessage<dmGraphics::TextureImage>(params.m_Buffer, params.m_BufferSize, (&texture_image));
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        ImageDesc* image_desc = CreateImage((dmGraphics::HContext) params.m_Context, texture_image);
        *params.m_PreloadData = image_desc;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResTexturePostCreate(const dmResource::ResourcePostCreateParams& params)
    {
        // Poll state of texture async texture processing and return state. RESULT_PENDING indicates we need to poll again.
        if(!SynchronizeTexture((dmGraphics::HTexture) params.m_Resource->m_Resource, false))
        {
            return dmResource::RESULT_PENDING;
        }

        ImageDesc* image_desc = (ImageDesc*) params.m_PreloadData;
        dmDDF::FreeMessage(image_desc->m_DDFImage);
        DestroyImage(image_desc);

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResTextureCreate(const dmResource::ResourceCreateParams& params)
    {
        dmGraphics::HContext graphics_context = (dmGraphics::HContext) params.m_Context;
        dmGraphics::HTexture texture;
        dmResource::Result r = AcquireResources(params.m_Resource, graphics_context, (ImageDesc*) params.m_PreloadData, 0, &texture);
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

        // Create the image from the DDF data.
        // Note that the image desc for performance reasons keeps references to the DDF image, meaning they're invalid after the DDF message has been free'd!
        ImageDesc* image_desc = CreateImage((dmGraphics::HContext) params.m_Context, texture_image);

        // Set up the new texture (version), wait for it to finish before issuing new requests
        SynchronizeTexture(texture, true);
        dmResource::Result r = AcquireResources(params.m_Resource, graphics_context, image_desc, texture, &texture);

        // Wait for any async texture uploads
        SynchronizeTexture(texture, true);

        DestroyImage(image_desc);

        if( params.m_Message == 0 )
        {
            dmDDF::FreeMessage(texture_image);
        }
        return r;
    }
}
