#include <string.h>
#include <dlib/log.h>
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
#include "../stb_image/stb_image.h"

namespace dmImage
{
    void Premultiply(uint8_t* buffer, int width, int height)
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

    Result Load(const void* buffer, uint32_t buffer_size, bool premult, Image* image)
    {
        int x, y, comp;

        unsigned char* ret = stbi_load_from_memory((const stbi_uc*) buffer, (int) buffer_size, &x, &y, &comp, 0);

        if (ret) {
            Image i;
            i.m_Width = (uint32_t) x;
            i.m_Height = (uint32_t) y;
            switch (comp) {
            case 1:
                i.m_Type = TYPE_LUMINANCE;
                break;
            case 2:
                // Luminance + alpha. Convert to luminance
                i.m_Type = TYPE_LUMINANCE;
                ret = stbi__convert_format(ret, 2, 1, x, y);
                break;
            case 3:
                i.m_Type = TYPE_RGB;
                break;
            case 4:
                i.m_Type = TYPE_RGBA;
                if (premult) {
                    Premultiply(ret, x, y);
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

    uint32_t BytesPerPixel(Type type)
    {
        switch (type)
        {
        case dmImage::TYPE_RGB:
            return 3;
        case dmImage::TYPE_RGBA:
            return 4;
        case dmImage::TYPE_LUMINANCE:
            return 1;
        }
        return 0;
    }
}

