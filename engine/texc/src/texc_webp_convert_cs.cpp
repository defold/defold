#include <stdint.h>

namespace dmTexc
{
    void RGB565ToRGB888(const uint16_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgb)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            const uint16_t c = *(data++);
            *(color_rgb++) = (c>>8) & 0xf8;
            *(color_rgb++) = (c>>3) & 0xfc;
            *(color_rgb++) = (c<<3) & 0xf8;
        }
    }

    void RGBA4444ToRGBA8888(const uint16_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgba)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            const uint16_t c = *(data++);
            *(color_rgba++) = ((c>>8) & 0xf0);
            *(color_rgba++) = ((c>>4) & 0xf0);
            *(color_rgba++) = ((c) & 0xf0);
            *(color_rgba++) = ((c<<4) & 0xf0);
        }
    }

    void L8ToRGB888(const uint8_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgb)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            *(color_rgb++) = *(data);
            *(color_rgb++) = *(data);
            *(color_rgb++) = *(data++);
        }
    }

    void L8A8ToRGBA8888(const uint8_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgba)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            *(color_rgba++) = *(data);
            *(color_rgba++) = *(data);
            *(color_rgba++) = *(data++);
            *(color_rgba++) = *(data++);
        }
    }

}
