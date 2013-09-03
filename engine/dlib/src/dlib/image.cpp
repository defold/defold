#include <dlib/log.h>
#include "image.h"

#define STBI_NO_HDR
#define STBI_NO_STDIO
#define STBI_FAILURE_USERMSG
// r15 of https://code.google.com/p/stblib/source/browse/trunk/libraries/stb_image.c
#include "../stb_image/stb_image.c"

namespace dmImage
{
    Result Load(const void* buffer, uint32 buffer_size, Image* image)
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
                ret = convert_format(ret, 2, 1, x, y);
                break;
            case 3:
                i.m_Type = TYPE_RGB;
                break;
            case 4:
                i.m_Type = TYPE_RGBA;
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

