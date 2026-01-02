// Copyright 2020-2025 The Defold Foundation
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

#include "gamesys.h"
#include "gamesys_private.h"

#include "resources/res_texture.h"

#include <graphics/graphics.h>

namespace dmGameSystem
{
    // Creates a texture image in DDF format which can be serialized into a protobuf message
    void MakeTextureImage(CreateTextureResourceParams params, dmGraphics::TextureImage* texture_image)
    {
        uint32_t* mip_map_data_size          = new uint32_t[params.m_MaxMipMaps];
        uint32_t* mip_map_offsets            = new uint32_t[params.m_MaxMipMaps];
        uint32_t* mip_map_offsets_compressed = new uint32_t[1];
        uint32_t* mip_map_dimensions         = new uint32_t[params.m_MaxMipMaps * 2];
        uint8_t layer_count                  = GetLayerCount(params.m_Type) * dmMath::Max((uint8_t) 1, params.m_LayerCount);

        uint32_t data_size = 0;

        if (params.m_CompressionType == dmGraphics::TextureImage::COMPRESSION_TYPE_ASTC)
        {
            if (params.m_MaxMipMaps > 1)
            {
                dmLogWarning("%s: Currently only support 1 mipmap for compressed textures", __FUNCTION__);
            }
            mip_map_offsets[0]      = 0;
            mip_map_data_size[0]    = params.m_DataSize;
            mip_map_dimensions[0]   = params.m_Width;
            mip_map_dimensions[1]   = params.m_Height;
        }
        else
        {
            uint16_t mm_width  = params.m_Width;
            uint16_t mm_height = params.m_Height;
            uint16_t mm_depth  = params.m_Depth;

            for (uint32_t i = 0; i < params.m_MaxMipMaps; ++i)
            {
                mip_map_offsets[i]            = (data_size / 8);
                mip_map_dimensions[i * 2 + 0] = mm_width;
                mip_map_dimensions[i * 2 + 1] = mm_height;

                // Calculate the data size per mipmap in bytes
                // Graphics APIs require that the data size is _per slice_ and not the whole texture.
                // This is a quirk from how the OpenGL adapter is implemented, and should probably be fixed.
                uint32_t data_size_per_slice  = mm_width * mm_height * params.m_TextureBpp;
                data_size                    += data_size_per_slice * layer_count;
                mip_map_data_size[i]          = data_size_per_slice / 8;

                mm_width                     /= 2;
                mm_height                    /= 2;
            }
            assert(data_size > 0);

            data_size *= layer_count * mm_depth;
        }

        uint32_t image_data_size = 0;
        uint8_t* image_data      = 0;

        if (params.m_Buffer)
        {
            uint8_t* data     = 0;
            uint32_t datasize = 0;
            dmBuffer::GetBytes(params.m_Buffer, (void**)&data, &datasize);
            image_data      = data;
            image_data_size = datasize;
        }
        else if (params.m_Data)
        {
            image_data      = (uint8_t*) params.m_Data;
            image_data_size = params.m_DataSize;
        }
        else
        {
            image_data_size  = data_size / 8; // bits -> bytes for compression formats
            image_data = new uint8_t[image_data_size];
            memset(image_data, 0, image_data_size);
        }

        // Note: Right now we only support creating compressed textures with 1 mipmap,
        //       so we only need a pointer here for the data offset.
        mip_map_offsets_compressed[0] = image_data_size;

        texture_image->m_Alternatives.m_Data   = new dmGraphics::TextureImage::Image[1];
        texture_image->m_Alternatives.m_Count  = 1;
        texture_image->m_Type                  = params.m_TextureType;
        texture_image->m_Count                 = layer_count;
        texture_image->m_UsageFlags            = params.m_UsageFlags;
        texture_image->m_ImageDataAddress      = (uint64_t) image_data;

        dmGraphics::TextureImage::Image* image = &texture_image->m_Alternatives.m_Data[0];
        image->m_Width                        = params.m_Width;
        image->m_Height                       = params.m_Height;
        image->m_Depth                        = params.m_Depth;
        image->m_OriginalWidth                = params.m_Width;
        image->m_OriginalHeight               = params.m_Height;
        image->m_OriginalDepth                = params.m_Depth;
        image->m_Format                       = params.m_TextureFormat;
        image->m_DataSize                     = image_data_size;
        image->m_CompressionType              = params.m_CompressionType;
        image->m_MipMapOffset.m_Data          = mip_map_offsets;
        image->m_MipMapOffset.m_Count         = params.m_MaxMipMaps;
        image->m_MipMapSize.m_Data            = mip_map_data_size;
        image->m_MipMapSize.m_Count           = params.m_MaxMipMaps;
        image->m_MipMapSizeCompressed.m_Data  = mip_map_offsets_compressed;
        image->m_MipMapSizeCompressed.m_Count = 1;

        image->m_MipMapDimensions.m_Data  = mip_map_dimensions;
        image->m_MipMapDimensions.m_Count = params.m_MaxMipMaps * 2;
    }

    void DestroyTextureImage(dmGraphics::TextureImage& texture_image, bool destroy_image_data)
    {
        for (int i = 0; i < texture_image.m_Alternatives.m_Count; ++i)
        {
            dmGraphics::TextureImage::Image& image = texture_image.m_Alternatives.m_Data[i];
            delete[] image.m_MipMapOffset.m_Data;
            delete[] image.m_MipMapSize.m_Data;
            delete[] image.m_MipMapSizeCompressed.m_Data;
            delete[] image.m_MipMapDimensions.m_Data;
        }
        delete[] texture_image.m_Alternatives.m_Data;

        if (destroy_image_data)
        {
            delete[] (uint8_t*) texture_image.m_ImageDataAddress;
        }
    }

    void FillTextureResourceBuffer(const dmGraphics::TextureImage* texture_image, dmArray<uint8_t>& texture_resource_buffer)
    {
        dmArray<uint8_t> ddf_buffer;
        dmDDF::Result ddf_result = dmDDF::SaveMessageToArray(texture_image, dmGraphics::TextureImage::m_DDFDescriptor, ddf_buffer);
        assert(ddf_result == dmDDF::RESULT_OK);

        // Construct the header size + message
        const uint32_t TEXTURE_RES_MESSAGE_SIZE = sizeof(int32_t) + ddf_buffer.Size();

        texture_resource_buffer.SetCapacity(TEXTURE_RES_MESSAGE_SIZE);
        texture_resource_buffer.SetSize(TEXTURE_RES_MESSAGE_SIZE);

        int32_t* texture_resource_header_size = (int32_t*) texture_resource_buffer.Begin();
        *texture_resource_header_size = ddf_buffer.Size();
        memcpy(texture_resource_buffer.Begin() + sizeof(int32_t), ddf_buffer.Begin(), ddf_buffer.Size());
    }

    dmGraphics::TextureImage::TextureFormat GraphicsTextureFormatToImageFormat(dmGraphics::TextureFormat textureformat)
    {
    #define GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(x) case dmGraphics::x: return dmGraphics::TextureImage::x
        switch(textureformat)
        {
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_LUMINANCE);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGB);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGB_PVRTC_2BPPV1);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGB_PVRTC_4BPPV1);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGB_ETC1);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_ETC2);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGB_BC1);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_BC3);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_R_BC4);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RG_BC5);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_BC7);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGB16F);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGB32F);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA16F);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA32F);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_R16F);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RG16F);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_R32F);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RG32F);

            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_ASTC_4X4);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_ASTC_5X4);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_ASTC_5X5);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_ASTC_6X5);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_ASTC_6X6);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_ASTC_8X5);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_ASTC_8X6);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_ASTC_8X8);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_ASTC_10X5);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_ASTC_10X6);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_ASTC_10X8);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_ASTC_10X10);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_ASTC_12X10);
            GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_ASTC_12X12);

            default:break;
        };
    #undef GRAPHICS_TO_TEXTURE_IMAGE_ENUM_CASE
        return (dmGraphics::TextureImage::TextureFormat) -1;
    }

    dmGraphics::TextureImage::Type GraphicsTextureTypeToImageType(dmGraphics::TextureType texturetype)
    {
        switch(texturetype)
        {
            case dmGraphics::TEXTURE_TYPE_2D:       return dmGraphics::TextureImage::TYPE_2D;
            case dmGraphics::TEXTURE_TYPE_2D_ARRAY: return dmGraphics::TextureImage::TYPE_2D_ARRAY;
            case dmGraphics::TEXTURE_TYPE_3D:       return dmGraphics::TextureImage::TYPE_3D;
            case dmGraphics::TEXTURE_TYPE_CUBE_MAP: return dmGraphics::TextureImage::TYPE_CUBEMAP;
            case dmGraphics::TEXTURE_TYPE_IMAGE_2D: return dmGraphics::TextureImage::TYPE_2D_IMAGE;
            case dmGraphics::TEXTURE_TYPE_IMAGE_3D: return dmGraphics::TextureImage::TYPE_3D_IMAGE;
            default: assert(0);
        }
        dmLogError("Unsupported texture type (%d)", texturetype);
        return (dmGraphics::TextureImage::Type) -1;
    }

    dmResource::Result CreateTextureResource(dmResource::HFactory factory, const CreateTextureResourceParams& create_params, void** resource_out)
    {
        dmGraphics::TextureImage texture_image = {};
        MakeTextureImage(create_params, &texture_image);

        dmArray<uint8_t> texture_resource_buffer;
        FillTextureResourceBuffer(&texture_image, texture_resource_buffer);

        void* resource = 0x0;
        dmResource::Result res = dmResource::CreateResource(factory, create_params.m_Path, texture_resource_buffer.Begin(), texture_resource_buffer.Size(), &resource);

        DestroyTextureImage(texture_image, create_params.m_Buffer == 0 && create_params.m_Data == 0);

        if (res != dmResource::RESULT_OK)
        {
            return res;
        }

        assert(create_params.m_Collection);
        *resource_out = resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result SetTextureResource(dmResource::HFactory factory, const SetTextureResourceParams& params)
    {
        // Note: We only support uploading a single mipmap for a single slice at a time
        const uint32_t NUM_MIP_MAPS = 1;

        dmGraphics::TextureImage::Image image  = {};
        dmGraphics::TextureImage texture_image = {};
        texture_image.m_Alternatives.m_Data    = &image;
        texture_image.m_Alternatives.m_Count   = 1;
        texture_image.m_Type                   = GraphicsTextureTypeToImageType(params.m_TextureType);
        texture_image.m_Count                  = 1;
        texture_image.m_ImageDataAddress       = (uint64_t) params.m_Data;

        image.m_Width                = params.m_Width;
        image.m_Height               = params.m_Height;
        image.m_Depth                = params.m_Depth;
        image.m_OriginalWidth        = params.m_Width;
        image.m_OriginalHeight       = params.m_Height;
        image.m_OriginalDepth        = params.m_Depth;
        image.m_Format               = GraphicsTextureFormatToImageFormat(params.m_TextureFormat);
        image.m_CompressionType      = params.m_CompressionType;

        uint32_t mip_map_dimensions[]    = {params.m_Width, params.m_Height};
        image.m_MipMapDimensions.m_Data  = mip_map_dimensions;
        image.m_MipMapDimensions.m_Count = 2;

        // Note: When uploading cubemap faces on OpenGL, we expect that the "data size" is **per** slice
        //       and not the entire data size of the buffer. For vulkan we don't look at this value but instead
        //       calculate a slice size. Maybe we should do one or the other..
        uint32_t mip_map_offsets             = 0;
        uint32_t mip_map_sizes               = params.m_DataSize / GetLayerCount(params.m_TextureType);
        image.m_MipMapOffset.m_Data          = &mip_map_offsets;
        image.m_MipMapOffset.m_Count         = NUM_MIP_MAPS;
        image.m_MipMapSize.m_Data            = &mip_map_sizes;
        image.m_MipMapSize.m_Count           = NUM_MIP_MAPS;
        image.m_MipMapSizeCompressed.m_Data  = &mip_map_sizes;
        image.m_MipMapSizeCompressed.m_Count = NUM_MIP_MAPS;

        ResTextureReCreateParams recreate_params;
        recreate_params.m_TextureImage = &texture_image;

        ResTextureUploadParams& upload_params = recreate_params.m_UploadParams;
        upload_params.m_X                     = params.m_X;
        upload_params.m_Y                     = params.m_Y;
        upload_params.m_Z                     = params.m_Z;
        upload_params.m_Page                  = params.m_Slice;
        upload_params.m_MipMap                = params.m_MipMap;
        upload_params.m_SubUpdate             = params.m_SubUpdate;
        upload_params.m_UploadSpecificMipmap  = 1;

        return dmResource::SetResource(factory, params.m_PathHash, (void*) &recreate_params);
    }
}
