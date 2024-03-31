// Copyright 2020-2024 The Defold Foundation
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


#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/dstrings.h> // dmSnPrintf

#include "texc.h"
#include "texc_private.h"

// #define STB_IMAGE_WRITE_IMPLEMENTATION
// #include <stb/stb_image_write.h>

#include <basis/encoder/basisu_enc.h>
#include <basis/encoder/basisu_comp.h>
#include <basis/encoder/basisu_frontend.h>


namespace dmTexc
{
    static bool EncodeDefault(Texture* texture, int num_threads, PixelFormat pixel_format, CompressionType compression_type, CompressionLevel compression_level)
    {
        // static int image = 0;
        // ++image;

        for (uint32_t i = 0; i < texture->m_Mips.Size(); ++i)
        {
            TextureData* mip_level = &texture->m_Mips[i];
            uint32_t size = GetDataSize(pixel_format, mip_level->m_Width, mip_level->m_Height);
            uint8_t* packed_data = new uint8_t[size];

            if (pixel_format == PF_R4G4B4A4) {
                DitherRGBA4444(mip_level->m_Data, mip_level->m_Width, mip_level->m_Height);
            }
            else if(pixel_format == PF_R5G6B5) {
                DitherRGBx565(mip_level->m_Data, mip_level->m_Width, mip_level->m_Height);
            }

            ConvertRGBA8888ToPf(mip_level->m_Data, mip_level->m_Width, mip_level->m_Height, pixel_format, packed_data);

            uint8_t* old_data = mip_level->m_Data;
            mip_level->m_Data = packed_data;
            mip_level->m_ByteSize = size;

            // char name[256];
            // // dmSnPrintf(name, sizeof(name), "../mipimages_enc_default_stb/image%d_mip%02u.png", image, i);
            // dmSnPrintf(name, sizeof(name), "../mipimages_enc_default_basis/image%d_mip%02u.png", image, i);
            // int result = stbi_write_png(name, mip_level->m_Width, mip_level->m_Height, 4, (void*)mip_level->m_Data, mip_level->m_Width*4);
            // printf("Wrote %s\n", name);

            delete[] old_data;
        }

        return true;
    }

    static bool CreateDefault(Texture* texture, uint32_t width, uint32_t height, PixelFormat pixel_format, ColorSpace color_space, CompressionType compression_type, void* data)
    {
        uint32_t size = width * height * 4;
        uint8_t* base_image = new uint8_t[size];
        if (!ConvertToRGBA8888((uint8_t*)data, width, height, pixel_format, base_image))
        {
            delete[] base_image;
            return false;
        }

        TextureData mip_level;
        mip_level.m_Data = base_image;
        mip_level.m_ByteSize = size;
        mip_level.m_IsCompressed = false;
        mip_level.m_Width = width;
        mip_level.m_Height = height;

        texture->m_Mips.SetCapacity(64);
        texture->m_Mips.Push(mip_level);

        texture->m_CompressionFlags = 0;
        texture->m_PixelFormat = pixel_format;
        texture->m_ColorSpace = color_space;
        texture->m_Width = width;
        texture->m_Height = height;
        return true;
    }

    static void DestroyDefault(Texture* texture)
    {
        for(uint32_t i = 0; i < texture->m_Mips.Size(); ++i)
        {
            delete[] texture->m_Mips[i].m_Data;
        }
    }

    static uint8_t* GenMipMapDefault(Texture* texture, int miplevel, const basisu::image& origimage, uint32_t mip_width, uint32_t mip_height, ColorSpace color_space)
    {
        (void)texture;
        (void)miplevel;
        uint32_t num_channels = 4;
        uint32_t size = mip_width * mip_height * num_channels;
        uint8_t* mip_data = new uint8_t[mip_width * mip_height * num_channels];

        basisu::image mipimage(mip_width, mip_height);

        bool srgb = false;
        const char* filter = "tent";
        basisu::image_resample(origimage, mipimage, srgb, filter);

        basisu::color_rgba* basisimage = mipimage.get_ptr();
        memcpy(mip_data, basisimage, size);

        // char name[256];
        // stbi_flip_vertically_on_write(true);
        // dmSnPrintf(name, sizeof(name), "/Users/mawe/work/projects/users/mawe/TextureCompression/mipmaps/%s_mip%02d_%s.png", texture->m_Name?texture->m_Name:"unknown", miplevel, filter);
        // int result = stbi_write_png(name, mip_width, mip_height, 4, (void*)mip_data, mip_width*4);

        // stbi_flip_vertically_on_write(false);
        // printf("Wrote %s\n", name);

        return mip_data;
    }

    static bool GenMipMapsDefault(Texture* texture)
    {
        uint32_t width = texture->m_Width;
        uint32_t height = texture->m_Height;

        // We make a straight unaltered copy of the input data as the first mip level
        uint8_t* mip0 = texture->m_Mips[0].m_Data;

        basisu::image origimage;
        origimage.init(mip0, width, height, 4);

        int level = 0;
        while (width * height != 1)
        {
            width /= 2;
            height /= 2;
            width = dmMath::Max(1U, width);
            height = dmMath::Max(1U, height);

            uint8_t* mipmap = GenMipMapDefault(texture, level, origimage, width, height, texture->m_ColorSpace);
            level++;

            TextureData mip_level;
            mip_level.m_Width = width;
            mip_level.m_Height = height;
            mip_level.m_Data = mipmap;
            mip_level.m_ByteSize = width * height * 4;
            mip_level.m_IsCompressed = false;
            texture->m_Mips.Push(mip_level);
        }
        return true;
    }

    static bool ResizeDefault(Texture* texture, uint32_t width, uint32_t height)
    {
        uint32_t num_channels = 4;
        uint32_t new_size = width * height * num_channels;
        uint8_t* new_data = new uint8_t[new_size];
        uint8_t* mip0 = texture->m_Mips[0].m_Data;

        basisu::image origimage;
        origimage.init(mip0, texture->m_Width, texture->m_Height, num_channels);

        basisu::image mipimage(width, height);
        basisu::image_resample(origimage, mipimage);

        basisu::color_rgba* basisimage = mipimage.get_ptr();
        memcpy(new_data, basisimage, new_size);

        delete[] texture->m_Mips[0].m_Data;
        texture->m_Mips[0].m_Data = new_data;
        texture->m_Mips[0].m_ByteSize = new_size;
        texture->m_Mips[0].m_Width = width;
        texture->m_Mips[0].m_Height = height;
        texture->m_Width = width;
        texture->m_Height = height;

        return true;
    }

    static uint32_t GetTotalDataSizeDefault(Texture* texture)
    {
        uint32_t total_size = 0;
        for (uint32_t i = 0; i < texture->m_Mips.Size(); ++i)
        {
            const TextureData* mip_level = &texture->m_Mips[i];
            total_size += mip_level->m_ByteSize;
        }
        return total_size;
    }

    static uint32_t GetDataDefault(Texture* texture, void* out_data, uint32_t out_data_size)
    {
        uint8_t* write_ptr = (uint8_t*)out_data;
        uint32_t total_size = 0;
        for (uint32_t i = 0; i < texture->m_Mips.Size(); ++i)
        {
            const TextureData* mip_level = &texture->m_Mips[i];
            memcpy(write_ptr, mip_level->m_Data, mip_level->m_ByteSize);

            write_ptr += mip_level->m_ByteSize;
            total_size += mip_level->m_ByteSize;
        }
        return total_size;
    }

    static bool PreMultiplyAlphaDefault(Texture* texture)
    {
        // Do we need to check for alpha?
        TextureData* mip_level = &texture->m_Mips[0];
        PreMultiplyAlpha(mip_level->m_Data, texture->m_Width, texture->m_Height);
        return true;
    }

    static bool FlipDefault(Texture* texture, FlipAxis flip_axis)
    {
        TextureData* mip_level = &texture->m_Mips[0];
        switch(flip_axis)
        {
        case FLIP_AXIS_Y:   FlipImageY_RGBA8888((uint32_t*)mip_level->m_Data, texture->m_Width, texture->m_Height);
                            return true;
        case FLIP_AXIS_X:   FlipImageX_RGBA8888((uint32_t*)mip_level->m_Data, texture->m_Width, texture->m_Height);
                            return true;
        default:
            dmLogError("Unexpected flip direction: %d", flip_axis);
            return false;
        }
    }

    static bool GetHeaderDefault(Texture* texture, Header* out_header)
    {
        out_header->m_Width = texture->m_Width;
        out_header->m_Height = texture->m_Height;
        out_header->m_PixelFormat = texture->m_PixelFormat;
        out_header->m_Depth = 1;
        out_header->m_NumFaces = 1;
        out_header->m_Flags = 0;
        out_header->m_MipMapCount = texture->m_Mips.Size();
        return true;
    }

    void GetEncoderDefault(Encoder* encoder)
    {
        encoder->m_FnCreate             = CreateDefault;
        encoder->m_FnDestroy            = DestroyDefault;
        encoder->m_FnGenMipMaps         = GenMipMapsDefault;
        encoder->m_FnResize             = ResizeDefault;
        encoder->m_FnEncode             = EncodeDefault;
        encoder->m_FnGetTotalDataSize   = GetTotalDataSizeDefault;
        encoder->m_FnGetData            = GetDataDefault;
        encoder->m_FnPreMultiplyAlpha   = PreMultiplyAlphaDefault;
        encoder->m_FnFlip               = FlipDefault;
        encoder->m_FnGetHeader          = GetHeaderDefault;
    }
}
