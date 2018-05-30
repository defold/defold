#include "image.h"

#include <dlib/log.h>

#define STBI_NO_HDR
#define STBI_NO_STDIO
#define STBI_FAILURE_USERMSG
// r15 of https://code.google.com/p/stblib/source/browse/trunk/libraries/stb_image.c
#include "../stb_image/stb_image.c"
// r13 of https://jpeg-compressor.googlecode.com/svn/trunk/jpgd.cpp
// r11 of https://jpeg-compressor.googlecode.com/svn/trunk/jpgd.h
#include "../jpgd/jpgd.h"

namespace dmImage
{
    static bool IsJpeg(const uint8_t* buffer, uint32_t buffer_size)
    {
        return (buffer_size >= 10 &&
                buffer[0] == 0xff &&
                buffer[1] == 0xd8 &&
                buffer[2] == 0xff &&
                buffer[3] == 0xe0 &&
                buffer[6] == 0x4a &&
                buffer[7] == 0x46 &&
                buffer[8] == 0x49 &&
                buffer[9] == 0x46 &&
                buffer[10] == 0x00);
    }

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

    Result Load(const void* buffer, uint32 buffer_size, bool premult, Image* image)
    {
        int x, y, comp;

        unsigned char* ret;
        if (IsJpeg((const uint8_t*) buffer, buffer_size)) {
            // For progressive jpeg support. stb_image is non-progressive only
            ret = jpgd::decompress_jpeg_image_from_memory((const unsigned char *) buffer, (int) buffer_size, &x, &y, &comp, 3);
        } else {
            ret = stbi_load_from_memory((const stbi_uc*) buffer, (int) buffer_size, &x, &y, &comp, 0);
        }

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
                ret = convert_format(ret, 2, 1, x, y);
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

