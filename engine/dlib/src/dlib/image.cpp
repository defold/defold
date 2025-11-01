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

#include <string.h>
#include <dlib/log.h>
#include <dlib/static_assert.h>
#include "image.h"

//#define STBI_NO_JPEG
//#define STBI_NO_PNG
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_STDIO
#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_THREAD_LOCALS
#include "../stb/stb_image.h"

namespace dmImage
{
    static void PremultiplyRGBA(uint8_t* buffer, int width, int height)
    {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int index = (y * width + x) * 4;
                uint32_t a = buffer[index + 3];
                uint32_t r = (buffer[index + 0] * a + 255) >> 8;
                uint32_t g = (buffer[index + 1] * a + 255) >> 8;
                uint32_t b = (buffer[index + 2] * a + 255) >> 8;
                buffer[index + 0] = r;
                buffer[index + 1] = g;
                buffer[index + 2] = b;
            }
        }
    }

    static void PremultiplyLuminance(uint8_t* buffer, int width, int height)
    {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int index     = (y * width + x) * 2;
                uint32_t a    = buffer[index + 1];
                uint32_t r    = (buffer[index] * a + 255) >> 8;
                buffer[index] = r;
            }
        }
    }

    HImage NewImage(const void* buffer, uint32_t buffer_size, bool premult)
    {
        Image* image = new Image();

        if (Load(buffer, buffer_size, premult, false, image) != RESULT_OK)
        {
            delete image;
            return 0;
        }

        return (HImage) image;
    }

    void DeleteImage(Image* image)
    {
        Free(image);
        delete image;
    }

    Result Load(const void* buffer, uint32_t buffer_size, bool premult, bool flip_vertically, Image* image)
    {
        int x, y, comp;

        stbi_set_flip_vertically_on_load(flip_vertically);

        unsigned char* ret = stbi_load_from_memory((const stbi_uc*) buffer, (int) buffer_size, &x, &y, &comp, 0);

        // Reset to default state
        stbi_set_flip_vertically_on_load(0);

        if (ret) {
            Image i;
            i.m_Width = (uint32_t) x;
            i.m_Height = (uint32_t) y;
            switch (comp) {
            case 1:
                i.m_Type = TYPE_LUMINANCE;
                break;
            case 2:
                i.m_Type = TYPE_LUMINANCE_ALPHA;
                if (premult)
                {
                    PremultiplyLuminance(ret, x, y);
                }
                break;
            case 3:
                i.m_Type = TYPE_RGB;
                break;
            case 4:
                i.m_Type = TYPE_RGBA;
                if (premult)
                {
                    PremultiplyRGBA(ret, x, y);
                }
                break;
            default:
                dmLogError("Unexpected number of components in image (%d)", comp);
                free(ret);
                return RESULT_IMAGE_ERROR;
            }
            i.m_Buffer = (void*) ret;
            *image = i;
            return RESULT_OK;
        } else {
            dmLogError("Failed to load image: '%s'", stbi_failure_reason());
            return RESULT_IMAGE_ERROR;
        }
    }

    void Free(Image* image)
    {
        free(image->m_Buffer);
        memset(image, 0, sizeof(*image));
    }

    Type GetType(HImage image)
    {
        return image->m_Type;
    }

    uint32_t GetWidth(HImage image)
    {
        return image->m_Width;
    }

    uint32_t GetHeight(HImage image)
    {
        return image->m_Height;
    }

    const void* GetData(HImage image)
    {
        return image->m_Buffer;
    }

    static bool IsAstc(const void* mem, uint32_t memsize)
    {
        DM_STATIC_ASSERT(sizeof(struct AstcHeader) == 16, Invalid_Struct_Size);

        if (memsize < 16)
            return false;

        AstcHeader* header = (AstcHeader*)mem;
        return header->m_Magic[0] == 0x13
            && header->m_Magic[1] == 0xAB
            && header->m_Magic[2] == 0xA1
            && header->m_Magic[3] == 0x5C;
    }

    bool GetAstcBlockSize(const void* mem, uint32_t memsize, uint32_t* width, uint32_t* height, uint32_t* depth)
    {
        if (!IsAstc(mem, memsize))
            return false;

        AstcHeader* header = (AstcHeader*)mem;
        *width  = header->m_BlockSizes[0];
        *height = header->m_BlockSizes[1];
        *depth  = header->m_BlockSizes[2];
        return true;
    }

    bool GetAstcDimensions(const void* mem, uint32_t memsize, uint32_t* width, uint32_t* height, uint32_t* depth)
    {
        if (!IsAstc(mem, memsize))
            return false;

        AstcHeader* header = (AstcHeader*)mem;
        *width  = header->m_DimensionX[0] + (header->m_DimensionX[1] << 8) + (header->m_DimensionX[2] << 16);
        *height = header->m_DimensionY[0] + (header->m_DimensionY[1] << 8) + (header->m_DimensionY[2] << 16);
        *depth  = header->m_DimensionZ[0] + (header->m_DimensionZ[1] << 8) + (header->m_DimensionZ[2] << 16);
        return true;
    }
}

