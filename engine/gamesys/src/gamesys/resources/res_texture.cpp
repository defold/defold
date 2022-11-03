// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
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
#include <dlib/profile.h>
#include <dlib/time.h>
#include <dlib/math.h>
#include <graphics/graphics.h>

namespace dmGameSystem
{
    static const uint8_t  SPECIFIC_MIPMAP_DONT_CARE = 0xff;
    static const uint32_t MAX_MIPMAP_COUNT = 32;
    struct ImageDesc
    {
        dmGraphics::TextureImage* m_DDFImage;
        uint8_t*                  m_DecompressedData[MAX_MIPMAP_COUNT];
        uint32_t                  m_DecompressedDataSize[MAX_MIPMAP_COUNT];
        bool                      m_UseBlankTexture;
        uint16_t                  m_RegionUpdateX;
        uint16_t                  m_RegionUpdateY;
        uint8_t                   m_MipMap;
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

    static uint16_t GetMipmapSize(uint16_t size_0, uint8_t mipmap)
    {
        for (uint32_t i = 0; i < mipmap; ++i)
        {
            size_0 /= 2;
        }
        return size_0;
    }

    static inline uint8_t GetMipmapCount(uint16_t width, uint16_t height)
    {
        uint16_t dim = dmMath::Max(width, height);
        return (uint8_t) ceil(log2f(dim));
    }

    dmResource::Result AcquireResources(const char* path, dmResource::SResourceDescriptor* resource_desc, dmGraphics::HContext context, ImageDesc* image_desc, dmGraphics::HTexture texture, dmGraphics::HTexture* texture_out)
    {
        DM_PROFILE_DYN(path, 0);

        dmResource::Result result = dmResource::RESULT_FORMAT_ERROR;
        for (uint32_t i = 0; i < image_desc->m_DDFImage->m_Alternatives.m_Count; ++i)
        {
            dmGraphics::TextureImage::Image* image = &image_desc->m_DDFImage->m_Alternatives[i];


            dmGraphics::TextureFormat original_format = TextureImageToTextureFormat(image->m_Format);
            dmGraphics::TextureFormat output_format = original_format;

            uint32_t num_mips = image->m_MipMapOffset.m_Count;
            if (dmGraphics::IsFormatTranscoded(image->m_CompressionType))
            {
                num_mips = MAX_MIPMAP_COUNT;
                output_format = dmGraphics::GetSupportedCompressionFormat(context, output_format, image->m_Width, image->m_Height);
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

            dmGraphics::TextureParams params;
            dmGraphics::GetDefaultTextureFilters(context, params.m_MinFilter, params.m_MagFilter);
            params.m_Format    = output_format;
            params.m_Width     = image->m_Width;
            params.m_Height    = image->m_Height;
            params.m_X         = image_desc->m_RegionUpdateX;
            params.m_Y         = image_desc->m_RegionUpdateY;
            params.m_MipMap    = image_desc->m_MipMap;
            params.m_SubUpdate = false;

            assert(image->m_MipMapOffset.m_Count <= MAX_MIPMAP_COUNT);

            if (!texture)
            {
                dmGraphics::TextureCreationParams creation_params;
                if (image_desc->m_DDFImage->m_Type == dmGraphics::TextureImage::TYPE_2D)
                {
                    creation_params.m_Type = dmGraphics::TEXTURE_TYPE_2D;
                } else if (image_desc->m_DDFImage->m_Type == dmGraphics::TextureImage::TYPE_CUBEMAP)
                {
                    creation_params.m_Type = dmGraphics::TEXTURE_TYPE_CUBE_MAP;
                } else {
                    assert(0);
                }

                creation_params.m_Width          = image->m_Width;
                creation_params.m_Height         = image->m_Height;
                creation_params.m_OriginalWidth  = image->m_OriginalWidth;
                creation_params.m_OriginalHeight = image->m_OriginalHeight;
                creation_params.m_MipMapCount    = image->m_MipMapOffset.m_Count;
                texture                          = dmGraphics::NewTexture(context, creation_params);
            }
            else
            {
                uint16_t tex_width_full    = dmGraphics::GetTextureWidth(texture);
                uint16_t tex_height_full   = dmGraphics::GetTextureHeight(texture);
                uint16_t tex_width_mipmap  = GetMipmapSize(tex_width_full, params.m_MipMap);
                uint16_t tex_height_mipmap = GetMipmapSize(tex_height_full, params.m_MipMap);
                uint8_t  tex_mipmap_count  = GetMipmapCount(tex_width_full, tex_height_full);
                params.m_SubUpdate         = params.m_X > 0 || params.m_Y > 0 || params.m_Width < tex_width_mipmap || params.m_Height < tex_height_mipmap;

                if (params.m_MipMap > tex_mipmap_count)
                {
                    dmLogError("Texture mipmap level %u exceeds maximum mipmap level %u.", params.m_MipMap, tex_mipmap_count);
                    break;
                }

                if ((params.m_X + params.m_Width) > tex_width_mipmap || (params.m_Y + params.m_Height) > tex_height_mipmap)
                {
                    dmLogError("Texture size %ux%u at offset %u,%u exceeds maximum texture size (%ux%u) for mipmap level %u.",
                        params.m_Width, params.m_Height, params.m_X, params.m_Y, tex_width_mipmap, tex_height_mipmap, params.m_MipMap);
                    break;
                }
            }

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

            // If we requested to upload a specific mipmap, upload only that level
            // It is expect that we only have offsets for that level in the image desc as well
            if (params.m_MipMap != SPECIFIC_MIPMAP_DONT_CARE)
            {
                if (image_desc->m_DecompressedData[0] == 0)
                {
                    params.m_Data     = &image->m_Data[image->m_MipMapOffset[0]];
                    params.m_DataSize = image->m_MipMapSize[0];
                }
                else
                {
                    params.m_Data     = image_desc->m_DecompressedData[0];
                    params.m_DataSize = image_desc->m_DecompressedDataSize[0];
                }
                dmGraphics::SetTextureAsync(texture, params);
            }
            else
            {
                for (uint32_t i = 0; i < num_mips; ++i)
                {
                    params.m_MipMap = i;
                    params.m_Data = image_desc->m_DecompressedData[i] == 0 ? &image->m_Data[image->m_MipMapOffset[i]] : image_desc->m_DecompressedData[i];
                    params.m_DataSize = image_desc->m_DecompressedData[i] == 0 ? image->m_MipMapSize[i] : image_desc->m_DecompressedDataSize[i];
                    dmGraphics::SetTextureAsync(texture, params);

                    params.m_Width >>= 1;
                    params.m_Height >>= 1;
                    if (params.m_Width == 0)
                    {
                        params.m_Width = 1;
                    }
                    if (params.m_Height == 0)
                    {
                        params.m_Height = 1;
                    }
                }
            }
            break;
        }

        if (result == dmResource::RESULT_FORMAT_ERROR)
        {
            dmLogError("No matching texture format found for %s. Using blank texture.", path);

            if (!texture)
            {
                dmGraphics::TextureCreationParams creation_params;
                creation_params.m_Type = dmGraphics::TEXTURE_TYPE_2D;
                creation_params.m_Width = 1;
                creation_params.m_Height = 1;
                creation_params.m_OriginalWidth = 1;
                creation_params.m_OriginalHeight = 1;
                creation_params.m_MipMapCount = 1;
                texture = dmGraphics::NewTexture(context, creation_params);
            }

            if (texture)
            {
                dmGraphics::TextureParams params;
                dmGraphics::GetDefaultTextureFilters(context, params.m_MinFilter, params.m_MagFilter);
                SetBlankTexture(texture, params);
                result = dmResource::RESULT_OK;
            }
        }

        if (result == dmResource::RESULT_OK)
        {
            *texture_out = texture;
            return dmResource::RESULT_OK;
        }
        return result;
    }

    static ImageDesc* CreateImage(const char* path, dmGraphics::HContext context, dmGraphics::TextureImage* texture_image)
    {
        ImageDesc* image_desc = new ImageDesc;
        memset(image_desc, 0x0, sizeof(ImageDesc));
        image_desc->m_DDFImage = texture_image;
        image_desc->m_MipMap = SPECIFIC_MIPMAP_DONT_CARE;
        return image_desc;
    }

    static void DestroyImage(ImageDesc* image_desc)
    {
        for (uint32_t i = 0; i < MAX_MIPMAP_COUNT; ++i)
        {
            if(image_desc->m_DecompressedData[i])
                delete[] image_desc->m_DecompressedData[i];
        }
        delete image_desc;
    }

    dmResource::Result ResTexturePreload(const dmResource::ResourcePreloadParams& params)
    {
        DM_PROFILE(__FUNCTION__);
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
        ResTextureReCreateParams* recreate_params = (ResTextureReCreateParams*)params.m_Message;

        dmGraphics::TextureImage* texture_image = (dmGraphics::TextureImage*) recreate_params->m_TextureImage; //params.m_Message;
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

        image_desc->m_RegionUpdateX = recreate_params->m_X;
        image_desc->m_RegionUpdateY = recreate_params->m_Y;
        image_desc->m_MipMap        = recreate_params->m_MipMap;

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
