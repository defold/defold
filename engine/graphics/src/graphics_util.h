#ifndef DM_GRAPHICS_UTIL_H
#define DM_GRAPHICS_UTIL_H

#include <dlib/endian.h>
#include <dmsdk/vectormath/cpp/vectormath_aos.h>

namespace dmGraphics
{
    inline uint32_t PackRGBA(const Vectormath::Aos::Vector4& in_color)
    {
        uint8_t r = (uint8_t)(in_color.getX() * 255.0f);
        uint8_t g = (uint8_t)(in_color.getY() * 255.0f);
        uint8_t b = (uint8_t)(in_color.getZ() * 255.0f);
        uint8_t a = (uint8_t)(in_color.getW() * 255.0f);

#if DM_ENDIAN == DM_ENDIAN_LITTLE
        uint32_t c = a << 24 | b << 16 | g << 8 | r;
#else
        uint32_t c = r << 24 | g << 16 | b << 8 | a;
#endif
        return c;
    }

    inline const Vectormath::Aos::Vector4 UnpackRGBA(uint32_t in_color)
    {
#if DM_ENDIAN == DM_ENDIAN_LITTLE
        float r = (float)((in_color & 0x000000FF)      ) / 255.0f;
        float g = (float)((in_color & 0x0000FF00) >>  8) / 255.0f;
        float b = (float)((in_color & 0x00FF0000) >> 16) / 255.0f;
        float a = (float)((in_color & 0xFF000000) >> 24) / 255.0f;
#else
        float r = (float)((in_color & 0xFF000000) >> 24) / 255.0f;
        float g = (float)((in_color & 0x00FF0000) >> 16) / 255.0f;
        float b = (float)((in_color & 0x0000FF00) >>  8) / 255.0f;
        float a = (float)((in_color & 0x000000FF)      ) / 255.0f;
#endif
        return Vectormath::Aos::Vector4(r,g,b,a);
    }
}



#endif // #ifndef DM_GRAPHICS_UTIL_H
