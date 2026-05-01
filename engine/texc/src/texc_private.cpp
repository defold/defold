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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "texc.h"
#include "texc_private.h"
#include <dlib/log.h>

namespace dmTexc
{
    void RGB565ToRGB888(const uint16_t* data, uint32_t width, uint32_t height, uint8_t* color_rgb)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            const uint16_t c = *(data++);
            uint32_t r5 = (c>>11) & 0x1f; // [0,31]
            uint32_t g6 = (c>>5) & 0x3f; // [0,63]
            uint32_t b5 = (c>>0) & 0x1f; // [0,31]
            // Map to range [0,255]
            *(color_rgb++) = (r5 * 255 + 15) / 31;
            *(color_rgb++) = (g6 * 255 + 31) / 63;
            *(color_rgb++) = (b5 * 255 + 15) / 31;
        }
    }

    uint16_t RGB888ToRGB565(uint8_t red, uint8_t green, uint8_t blue)
    {
        uint16_t r = ((red >> 3) & 0x1f) << 11;
        uint16_t g = ((green >> 2) & 0x3f) << 5;
        uint16_t b = ((blue >> 3) & 0x1f);
        return r | g | b;
    }

    void RGB565ToRGBA8888(const uint16_t* data, uint32_t width, uint32_t height, uint8_t* color_rgba)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            const uint16_t c = *(data++);
            uint32_t r5 = (c>>11) & 0x1f; // [0,31]
            uint32_t g6 = (c>>5) & 0x3f; // [0,63]
            uint32_t b5 = (c>>0) & 0x1f; // [0,31]
            // Map to range [0,255]
            *(color_rgba++) = (r5 * 255 + 15) / 31;
            *(color_rgba++) = (g6 * 255 + 31) / 63;
            *(color_rgba++) = (b5 * 255 + 15) / 31;
            *(color_rgba++) = 255;
        }
    }

    // https://docs.microsoft.com/en-us/windows/win32/directshow/working-with-16-bit-rgb
    void RGBA8888ToRGB565(const uint8_t* data, uint32_t width, uint32_t height, uint16_t* color_rgb)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            uint8_t red = data[0];
            uint8_t green = data[1];
            uint8_t blue = data[2];
            uint16_t r = ((red >> 3) & 0x1f) << 11;
            uint16_t g = ((green >> 2) & 0x3f) << 5;
            uint16_t b = ((blue >> 3) & 0x1f);

            *(color_rgb++) = (r | g | b);
            data += 4;
        }
    }

    uint16_t RGBA8888ToRGBA4444(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
    {
        uint16_t r = (red >> 4) << 12;
        uint16_t g = (green >> 4) << 8;
        uint16_t b = (blue >> 4) << 4;
        uint16_t a = (alpha >> 4) << 0;
        return (r | g | b | a);
    }

    void RGBA4444ToRGBA8888(const uint16_t* data, uint32_t width, uint32_t height, uint8_t* color_rgba)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            const uint16_t c = *(data++);
            // Range [0,15]
            uint32_t r4 = (c>>12) & 0xf;
            uint32_t g4 = (c>>8) & 0xf;
            uint32_t b4 = (c>>4) & 0xf;
            uint32_t a4 = (c>>0) & 0xf;
            // Map to range [0,255]
            *(color_rgba++) = (r4 * 255 + 7) / 15;
            *(color_rgba++) = (g4 * 255 + 7) / 15;
            *(color_rgba++) = (b4 * 255 + 7) / 15;
            *(color_rgba++) = (a4 * 255 + 7) / 15;
        }
    }

    void RGBA8888ToRGBA4444(const uint8_t* data, uint32_t width, uint32_t height, uint16_t* color_rgba)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            uint16_t r = (data[0] >> 4) << 12;
            uint16_t g = (data[1] >> 4) << 8;
            uint16_t b = (data[2] >> 4) << 4;
            uint16_t a = (data[3] >> 4) << 0;
            *(color_rgba++) = (r | g | b | a);
            data += 4;
        }
    }

    void L8ToRGB888(const uint8_t* data, uint32_t width, uint32_t height, uint8_t* color_rgb)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            *(color_rgb++) = *(data);
            *(color_rgb++) = *(data);
            *(color_rgb++) = *(data++);
        }
    }

    void L8ToRGBA8888(const uint8_t* data, uint32_t width, uint32_t height, uint8_t* color_rgba)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            *(color_rgba++) = *(data);
            *(color_rgba++) = *(data);
            *(color_rgba++) = *(data++);
            *(color_rgba++) = 255;
        }
    }

    void RGBA8888ToL8(const uint8_t* data, uint32_t width, uint32_t height, uint8_t* color_l)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            *(color_l++) = *(data);
            data += 4;
        }
    }

    void L8A8ToRGBA8888(const uint8_t* data, uint32_t width, uint32_t height, uint8_t* color_rgba)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            *(color_rgba++) = *(data);
            *(color_rgba++) = *(data);
            *(color_rgba++) = *(data++);
            *(color_rgba++) = *(data++);
        }
    }

    void RGBA8888ToL8A8(const uint8_t* data, uint32_t width, uint32_t height, uint8_t* color_la)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            *(color_la++) = data[0];
            *(color_la++) = data[3];
            data += 4;
        }
    }

    void RGB888ToRGBA8888(const uint8_t* data, uint32_t width, uint32_t height, uint8_t* color_rgba)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            *(color_rgba++) = *(data++);
            *(color_rgba++) = *(data++);
            *(color_rgba++) = *(data++);
            *(color_rgba++) = 255;
        }
    }

    void ABGR8888ToRGBA8888(const uint8_t* data, uint32_t width, uint32_t height, uint8_t* color_rgba)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            uint8_t a = *(data++);
            uint8_t b = *(data++);
            uint8_t g = *(data++);
            uint8_t r = *(data++);

            *(color_rgba++) = r;
            *(color_rgba++) = g;
            *(color_rgba++) = b;
            *(color_rgba++) = a;
        }
    }

    void RGBA8888ToRGB888(const uint8_t* data, uint32_t width, uint32_t height, uint8_t* color_rgb)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            *(color_rgb++) = *(data++);
            *(color_rgb++) = *(data++);
            *(color_rgb++) = *(data++);
            data++;
        }
    }

    void PreMultiplyAlpha(uint8_t* data, uint32_t width, uint32_t height)
    {
        // If this is an issue, we could simd it
        // e.g (v6) https://github.com/Wizermil/premultiply_alpha/blob/master/premultiply_alpha/premultiply_alpha.hpp

        for (uint32_t i = 0; i < width*height; ++i)
        {
            uint32_t a = data[3];
            data[0] = (uint8_t)( (data[0] * a) / 255 );
            data[1] = (uint8_t)( (data[1] * a) / 255 );
            data[2] = (uint8_t)( (data[2] * a) / 255 );
            data += 4;
        }
    }

    void FlipImageX_RGBA8888(uint32_t* data, uint32_t width, uint32_t height)
    {
        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width/2; ++x)
            {
                uint32_t x2 = width - x - 1;
                uint32_t rgba = data[x + y * width];
                data[x + y * width] = data[x2 + y * width];
                data[x2 + y * width] = rgba;
            }
        }
    }

    void FlipImageY_RGBA8888(uint32_t* data, uint32_t width, uint32_t height)
    {
        for (uint32_t y = 0; y < height/2; ++y)
        {
            uint32_t y2 = height - y - 1;
            for (uint32_t x = 0; x < width; ++x)
            {
                uint32_t rgba = data[x + y * width];
                data[x + y * width] = data[x + y2 * width];
                data[x + y2 * width] = rgba;
            }
        }
    }

    bool HasAlpha(PixelFormat pf)
    {
        switch(pf)
        {
        case PF_R8G8B8A8:
        case PF_A8B8G8R8:
        case PF_RGBA_PVRTC_2BPPV1:
        case PF_RGBA_PVRTC_4BPPV1:
        case PF_R4G4B4A4:
        case PF_L8A8:
        case PF_RGBA_ETC2:
        case PF_RGBA_ASTC_4x4:
        case PF_RGBA_BC3:
        case PF_RGBA_BC7:
            return true;

        case PF_L8:
        case PF_R8G8B8:
        case PF_RGB_PVRTC_2BPPV1:
        case PF_RGB_PVRTC_4BPPV1:
        case PF_RGB_ETC1:
        case PF_R5G6B5:
        case PF_RGB_BC1:
        case PF_R_BC4:
        case PF_RG_BC5:
        default:
            return false;
        }
    }

    static uint32_t GetBytesPerPixel(PixelFormat pf)
    {
        switch(pf)
        {
        case PF_R8G8B8A8:   return 4;
        case PF_A8B8G8R8:   return 4;
        case PF_R8G8B8:     return 3;
        case PF_R4G4B4A4:   return 2;
        case PF_L8A8:       return 2;
        case PF_R5G6B5:     return 2;
        case PF_L8:         return 1;

        default:
            assert("not supported");
            return 0;
        }
    }

    uint32_t GetDataSize(PixelFormat pf, uint32_t width, uint32_t height)
    {
        uint32_t bytes_per_pixel = GetBytesPerPixel(pf);
        return bytes_per_pixel * width * height;
    }

    bool ConvertToRGBA8888(const uint8_t* input, uint32_t width, uint32_t height, PixelFormat pf, uint8_t* out)
    {
        switch(pf)
        {
        case PF_L8:         L8ToRGBA8888(input, width, height, out); break;
        case PF_L8A8:       L8A8ToRGBA8888(input, width, height, out); break;
        case PF_R8G8B8:     RGB888ToRGBA8888(input, width, height, out); break;
        case PF_R5G6B5:     RGB565ToRGBA8888((uint16_t*)input, width, height, out); break;
        case PF_R4G4B4A4:   RGBA4444ToRGBA8888((uint16_t*)input, width, height, out); break;
        case PF_A8B8G8R8:   ABGR8888ToRGBA8888(input, width, height, out); break;
        case PF_R8G8B8A8:   memcpy(out, input, width*height*4); break;
        default:
            dmLogError("Format not yet supported: %d", pf);
            return false;
        }
        return true;
    }

    void ConvertRGBA8888ToPf(const uint8_t* input, uint32_t width, uint32_t height, PixelFormat pf, void* out_data)
    {
        switch(pf)
        {
        case PF_L8:         RGBA8888ToL8(input, width, height, (uint8_t*)out_data); break;
        case PF_L8A8:       RGBA8888ToL8A8(input, width, height, (uint8_t*)out_data); break;
        case PF_R8G8B8:     RGBA8888ToRGB888(input, width, height, (uint8_t*)out_data); break;
        case PF_R5G6B5:     RGBA8888ToRGB565(input, width, height, (uint16_t*)out_data); break;
        case PF_R4G4B4A4:   RGBA8888ToRGBA4444(input, width, height, (uint16_t*)out_data); break;
        case PF_R8G8B8A8:   memcpy((uint8_t*)out_data, input, width*height*4); break;
        default:
            dmLogError("ConvertRGBA8888ToPf: Format not yet supported: %d", pf);
        }
    }

    // NOTE: This is used by the Defold Editor when Generating a Preview Image for atlases
    // We merge all the required steps to optimize since this blocks the editor UI while this is generated
    void ConvertPremultiplyAndFlip_ABGR8888ToRGBA8888(const uint8_t* input_data, uint8_t* output_data, uint32_t width, uint32_t height)
    {
        for (uint32_t y = 0; y < height; ++y)
        {
            uint32_t       flipped_y = height - y - 1;
            const uint8_t* src = input_data + (y * width * 4);
            uint8_t*       dst = output_data + (flipped_y * width * 4);

#pragma clang loop vectorize(enable) interleave(enable)
            for (uint32_t x = 0; x < width; ++x)
            {
                uint8_t a = *(src++);
                uint8_t b = *(src++);
                uint8_t g = *(src++);
                uint8_t r = *(src++);

                *(dst++) = (uint8_t)((r * a) / 255);
                *(dst++) = (uint8_t)((g * a) / 255);
                *(dst++) = (uint8_t)((b * a) / 255);
                *(dst++) = a;
            }
        }
    }

    static inline float fract(float v)
    {
        float f = v - (int32_t)v;
        return f;
    }

    //note: returns [-intensity;intensity[, magnitude of 2x intensity
    //note: from "NEXT GENERATION POST PROCESSING IN CALL OF DUTY: ADVANCED WARFARE"
    //      http://advances.realtimerendering.com/s2014/index.html
    static inline float InterleavedGradientNoise(float u, float v)
    {
        const float magic[3] = { 0.06711056f, 0.00583715f, 52.9829189f };
        return fract( magic[2] * fract( u * magic[0] + v * magic[1] ) );
    }

    static inline uint8_t addNoise(uint8_t v_in, int8_t noise)
    {
        int16_t v = v_in + noise;
        if (v > 255)
            v = 255;
        else if(v < 0)
            v = 0;
        return (uint8_t)v;
    }

    void DitherRGBA4444(uint8_t* data, uint32_t width, uint32_t height)
    {
        // Since we are going to convert this data to rgba4444 we the minimal value for a
        // color change is 2^8 / 2^4 = 16
        uint8_t bpp_mul = 16;
        uint8_t bpp_bias = bpp_mul / 2;

        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                float rnd = InterleavedGradientNoise((float)x, (float)y);
                data[0] = addNoise(data[0], (uint8_t)(rnd * bpp_mul - bpp_bias));
                data[1] = addNoise(data[1], (uint8_t)((1.0f - rnd) * bpp_mul - bpp_bias)); // As seen in the shadertoy by Mikkel Gjoel
                data[2] = addNoise(data[2], (uint8_t)(rnd * bpp_mul - bpp_bias));
                data[3] = addNoise(data[3], (uint8_t)(rnd * bpp_mul - bpp_bias));
                data+=4;
            }
        }
    }

    void DitherRGBx565(uint8_t* data, uint32_t width, uint32_t height)
    {
        // Since we are going to convert this data to rgba4444 we the minimal value for a color change is
        uint8_t bpp_mul_5 = 8; // (1<<8)/(1<<5)
        uint8_t bpp_bias_5 = bpp_mul_5 / 2;
        uint8_t bpp_mul_6 = 4; // (1<<8)/(1<<6)
        uint8_t bpp_bias_6 = bpp_mul_6 / 2;

        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                float rnd = InterleavedGradientNoise((float)x, (float)y);
                data[0] = addNoise(data[0], (uint8_t)(rnd * bpp_mul_5 - bpp_bias_5));
                data[1] = addNoise(data[1], (uint8_t)((1.0f - rnd) * bpp_mul_6 - bpp_bias_6)); // As seen in the shadertoy by Mikkel Gjoel
                data[2] = addNoise(data[2], (uint8_t)(rnd * bpp_mul_5 - bpp_bias_5));
                // the alpha channel is ignored in this case
                data+=4;
            }
        }
    }

    void DebugPrint(uint8_t* p, uint32_t width, uint32_t height, uint32_t num_channels)
    {
        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                for (uint32_t c = 0; c < num_channels; ++c)
                {
                    printf("%02x", p[c]);
                }
                p += num_channels;
                printf(" ");
            }
            printf("\n");
        }
        printf("\n");
    }
}
