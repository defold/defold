// Copyright 2020-2026 The Defold Foundation
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dlib/log.h>

#define STB_IMAGE_STATIC
#define STBI_ONLY_HDR
#define STBI_NO_STDIO
#define STBI_NO_THREAD_LOCALS
#define STB_IMAGE_IMPLEMENTATION
#include "../../dlib/src/stb/stb_image.h"

namespace dmTexc
{
    bool IsHDR(uint32_t data_size, const uint8_t* data)
    {
        if (!data || data_size > 0x7fffffffU)
        {
            return false;
        }

        return stbi_is_hdr_from_memory((const stbi_uc*)data, (int)data_size) != 0;
    }

    bool LoadHDR(uint32_t data_size, const uint8_t* data, Image* image)
    {
        if (!image)
        {
            return false;
        }

        memset(image, 0, sizeof(*image));

        if (!IsHDR(data_size, data))
        {
            return false;
        }

        int width = 0;
        int height = 0;
        int components = 0;
        float* hdr_data = stbi_loadf_from_memory((const stbi_uc*)data, (int)data_size, &width, &height, &components, 4);
        if (!hdr_data)
        {
            dmLogError("Failed to decode HDR image: '%s'", stbi_failure_reason());
            return false;
        }

        uint64_t data_count = (uint64_t)width * (uint64_t)height * 4U * sizeof(float);
        if (width <= 0 || height <= 0 || data_count > 0xffffffffULL)
        {
            stbi_image_free(hdr_data);
            return false;
        }

        image->m_Path = strdup("hdr");
        image->m_Data = (uint8_t*)malloc((size_t)data_count);
        image->m_DataCount = (uint32_t)data_count;
        image->m_Width = (uint32_t)width;
        image->m_Height = (uint32_t)height;
        image->m_PixelFormat = PF_RGBA32F;
        image->m_ColorSpace = CS_LRGB;

        if (!image->m_Path || !image->m_Data)
        {
            stbi_image_free(hdr_data);
            DestroyLoadedImage(image);
            return false;
        }

        memcpy(image->m_Data, hdr_data, (size_t)data_count);
        stbi_image_free(hdr_data);
        return true;
    }

}
