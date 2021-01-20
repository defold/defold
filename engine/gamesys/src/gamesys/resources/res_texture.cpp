// Copyright 2020 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "res_texture.h"

#include <dlib/log.h>
#include <dlib/webp.h>
#include <dlib/profile.h>
#include <dlib/time.h>
#include <dlib/math.h>
#include <graphics/graphics.h>
#include <graphics/graphics_transcoder.h>

namespace dmGameSystem
{
    static const uint32_t s_MaxMipCount = 32;
    struct ImageDesc
    {
        dmGraphics::TextureImage* m_DDFImage;
        uint8_t* m_DecompressedData[s_MaxMipCount];
        uint32_t m_DecompressedDataSize[s_MaxMipCount];
        bool m_UseBlankTexture;
    };

    static dmGraphics::TextureFormat TextureImageToTextureFormat(dmGraphics::TextureImage::TextureFormat format)
    {
#define CASE_TF(_X) case dmGraphics::TextureImage::TEXTURE_FORMAT_ ## _X:    return dmGraphics::TEXTURE_FORMAT_ ## _X
        switch (format)
        {
            CASE_TF(LUMINANCE);
            CASE_TF(LUMINANCE_ALPHA);
            CASE_TF(RGB);
            CASE_TF(RGBA);
            CASE_TF(RGB_16BPP);
            CASE_TF(RGBA_16BPP);
            CASE_TF(RGB_PVRTC_2BPPV1);
            CASE_TF(RGB_PVRTC_4BPPV1);
            CASE_TF(RGBA_PVRTC_2BPPV1);
            CASE_TF(RGBA_PVRTC_4BPPV1);
            CASE_TF(RGB_ETC1);
            CASE_TF(RGBA_ETC2);
            CASE_TF(RGBA_ASTC_4x4);
            CASE_TF(RGB_BC1);
            CASE_TF(RGBA_BC3);
            CASE_TF(R_BC4);
            CASE_TF(RG_BC5);
            CASE_TF(RGBA_BC7);
            default:
                assert(0);
                return (dmGraphics::TextureFormat)-1;
#undef CASE_TF
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

    dmResource::Result AcquireResources(const char* path, dmResource::SResourceDescriptor* resource_desc, dmGraphics::HContext context, ImageDesc* image_desc, dmGraphics::HTexture texture, dmGraphics::HTexture* texture_out)
    {
        dmResource::Result result = dmResource::RESULT_FORMAT_ERROR;
        for (uint32_t i = 0; i < image_desc->m_DDFImage->m_Alternatives.m_Count; ++i)
        {
            dmGraphics::TextureImage::Image* image = &image_desc->m_DDFImage->m_Alternatives[i];


            dmGraphics::TextureFormat original_format = TextureImageToTextureFormat(image->m_Format);
            dmGraphics::TextureFormat output_format = original_format;

            uint32_t num_mips = image->m_MipMapOffset.m_Count;
            if (dmGraphics::IsFormatTranscoded(image->m_CompressionType))
            {
                num_mips = s_MaxMipCount;
                output_format = dmGraphics::GetSupportedFormat(context, output_format);
                bool result = dmGraphics::Transcode(path, image, output_format, image_desc->m_DecompressedData, image_desc->m_DecompressedDataSize, &num_mips);
                if (!result)
                {
                    dmLogError("Failed to transcode %s", path);
                    continue;
                }
            }
            else if (!dmGraphics::IsTextureFormatSupported(context, original_format))
            {
                continue;
            }

            result = dmResource::RESULT_OK;

            dmGraphics::TextureCreationParams creation_params;
            dmGraphics::TextureParams params;
            dmGraphics::GetDefaultTextureFilters(context, params.m_MinFilter, params.m_MagFilter);
            params.m_Format = output_format;
            params.m_Width = image->m_Width;
            params.m_Height = image->m_Height;

            assert(image->m_MipMapOffset.m_Count <= s_MaxMipCount);

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
            creation_params.m_MipMapCount = image->m_MipMapOffset.m_Count;

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

            for (uint32_t i = 0; i < num_mips; ++i)
            {
                params.m_MipMap = i;
                params.m_Data = image_desc->m_DecompressedData[i] == 0 ? &image->m_Data[image->m_MipMapOffset[i]] : image_desc->m_DecompressedData[i];
                params.m_DataSize = image_desc->m_DecompressedData[i] == 0 ? image->m_MipMapSize[i] : image_desc->m_DecompressedDataSize[i];
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

    static ImageDesc* CreateImage(const char* path, dmGraphics::HContext context, dmGraphics::TextureImage* texture_image)
    {
        ImageDesc* image_desc = new ImageDesc;
        memset(image_desc, 0x0, sizeof(ImageDesc));
        image_desc->m_DDFImage = texture_image;
        return image_desc;
    }

    static void DestroyImage(ImageDesc* image_desc)
    {
        for (uint32_t i = 0; i < s_MaxMipCount; ++i)
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

        ImageDesc* image_desc = CreateImage(params.m_Filename, (dmGraphics::HContext) params.m_Context, texture_image);
        *params.m_PreloadData = image_desc;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResTexturePostCreate(const dmResource::ResourcePostCreateParams& params)
    {
        // Poll state of texture async texture processing and return state. RESULT_PENDING indicates we need to poll again.
        dmGraphics::HTexture texture = (dmGraphics::HTexture) params.m_Resource->m_Resource;
        if(!SynchronizeTexture(texture, false))
        {
            return dmResource::RESULT_PENDING;
        }

        ImageDesc* image_desc = (ImageDesc*) params.m_PreloadData;
        dmDDF::FreeMessage(image_desc->m_DDFImage);
        DestroyImage(image_desc);
        params.m_Resource->m_ResourceSize = dmGraphics::GetTextureResourceSize(texture);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResTextureCreate(const dmResource::ResourceCreateParams& params)
    {
        dmGraphics::HContext graphics_context = (dmGraphics::HContext) params.m_Context;
        dmGraphics::HTexture texture;
        dmResource::Result r = AcquireResources(params.m_Filename, params.m_Resource, graphics_context, (ImageDesc*) params.m_PreloadData, 0, &texture);
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
        ImageDesc* image_desc = CreateImage(params.m_Filename, (dmGraphics::HContext) params.m_Context, texture_image);

        // Set up the new texture (version), wait for it to finish before issuing new requests
        SynchronizeTexture(texture, true);
        dmResource::Result r = AcquireResources(params.m_Filename, params.m_Resource, graphics_context, image_desc, texture, &texture);

        // Wait for any async texture uploads
        SynchronizeTexture(texture, true);

        DestroyImage(image_desc);

        if( params.m_Message == 0 )
        {
            dmDDF::FreeMessage(texture_image);
        }
        if(r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_ResourceSize = dmGraphics::GetTextureResourceSize(texture);
        }
        return r;
    }
}
