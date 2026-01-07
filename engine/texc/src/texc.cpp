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

#include "texc.h"
#include "texc_private.h"

#include <assert.h>
#include <stdlib.h>

#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/dstrings.h>

namespace dmTexc
{
    Image* CreateImage(const char* path, uint32_t width, uint32_t height, PixelFormat pixel_format, ColorSpace color_space, uint32_t data_size, uint8_t* data)
    {
        Image* image = new Image;
        image->m_Path = strdup(path?path:"null");
        image->m_Width = width;
        image->m_Height = height;
        image->m_PixelFormat = pixel_format;
        image->m_ColorSpace = color_space;

        image->m_DataCount = width * height * 4;
        image->m_Data = (uint8_t*)malloc(image->m_DataCount);

        if (!ConvertToRGBA8888((uint8_t*)data, width, height, pixel_format, image->m_Data))
        {
            DestroyImage(image);
            return 0;
        }
        return image;
    }

    void CreatePreviewImage(uint32_t width, uint32_t height, uint32_t data_size, const uint8_t* input_data, uint8_t* output_data)
    {
        ConvertPremultiplyAndFlip_ABGR8888ToRGBA8888(input_data, output_data, width, height);
    }

    void DestroyImage(Image* image)
    {
        free((void*)image->m_Data);
        free((void*)image->m_Path);
        delete image;
    }

    uint32_t GetWidth(Image* image)
    {
        return image->m_Width;
    }

    uint32_t GetHeight(Image* image)
    {
        return image->m_Height;
    }

    Image* Resize(Image* image, uint32_t width, uint32_t height)
    {
        Image* resized = dmTexc::ResizeBasis(image, width, height);
        resized->m_Path = strdup(image->m_Path?image->m_Path:"null");
        return resized;
    }

    // Pre-multiply the color with alpha in a texture. The texture must have format PF_R8G8B8A8 for the alpha to be pre-multiplied.
    bool PreMultiplyAlpha(Image* image)
    {
        PreMultiplyAlpha(image->m_Data, image->m_Width, image->m_Height);
        return true;
    }

    // Flips a texture
    bool Flip(Image* image, FlipAxis flip_axis)
    {
        switch(flip_axis)
        {
        case FLIP_AXIS_Y:   FlipImageY_RGBA8888((uint32_t*)image->m_Data, image->m_Width, image->m_Height);
                            return true;
        case FLIP_AXIS_X:   FlipImageX_RGBA8888((uint32_t*)image->m_Data, image->m_Width, image->m_Height);
                            return true;
        default:
            dmLogError("Unexpected flip direction: %d", flip_axis);
            return false;
        }
    }

    // Dithers a texture
    bool Dither(Image* image, PixelFormat pixel_format)
    {
        if (pixel_format == PF_R4G4B4A4) {
            DitherRGBA4444(image->m_Data, image->m_Width, image->m_Height);
            return true;
        }
        else if(pixel_format == PF_R5G6B5) {
            DitherRGBx565(image->m_Data, image->m_Width, image->m_Height);
            return true;
        }
        return false;
    }
}
