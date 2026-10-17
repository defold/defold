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
#include "texc_pixel.h"
#include <simd/simd_runtime.h>
#include <dlib/log.h>
#include <dlib/math.h>

namespace dmTexc
{
    static PreMultiplyAlphaFn    g_PreMultiplyAlpha = Scalar::PreMultiplyAlpha;
    static ResizeRGBA8Fn         g_ResizeRGBA8 = Scalar::ResizeRGBA8;
    static ConvertPreviewFn      g_ConvertPreview = Scalar::ConvertPreview;
    static ResizeRGBA32FPixelsFn g_ResizeRGBA32FPixels = Scalar::ResizeRGBA32FPixels;

#if defined(DM_TEXC_HAS_NEON)
    static bool g_UseNeon = false;
#endif

    static int TexcSetupSimd()
    {
        struct Kernels
        {
            dsimd_version_t       m_Version;
            PreMultiplyAlphaFn    m_PreMultiply;
            ResizeRGBA8Fn         m_Resize;
            ConvertPreviewFn      m_Preview;
            ResizeRGBA32FPixelsFn m_ResizeHDR;
        };
        static const Kernels kernels[] = {
#if defined(DM_TEXC_HAS_AVX512)
            { DSIMD_VERSION_AVX512, AVX512::PreMultiplyAlpha, AVX512::ResizeRGBA8, AVX512::ConvertPreview, AVX512::ResizeRGBA32FPixels },
#endif
#if defined(DM_TEXC_HAS_AVX2)
            { DSIMD_VERSION_AVX2, AVX2::PreMultiplyAlpha, AVX2::ResizeRGBA8, AVX2::ConvertPreview, AVX2::ResizeRGBA32FPixels },
#endif
#if defined(DM_TEXC_HAS_SSE42)
            { DSIMD_VERSION_SSE42, SSE42::PreMultiplyAlpha, SSE42::ResizeRGBA8, SSE42::ConvertPreview, SSE42::ResizeRGBA32FPixels },
#endif
#if defined(DM_TEXC_HAS_SSE3)
            { DSIMD_VERSION_SSE3, SSE3::PreMultiplyAlpha, SSE3::ResizeRGBA8, SSE3::ConvertPreview, SSE3::ResizeRGBA32FPixels },
#endif
#if defined(DM_TEXC_HAS_NEON)
            { DSIMD_VERSION_NEON, NEON::PreMultiplyAlpha, NEON::ResizeRGBA8, NEON::ConvertPreview, NEON::ResizeRGBA32FPixels },
#endif
            { DSIMD_VERSION_SCALAR, Scalar::PreMultiplyAlpha, Scalar::ResizeRGBA8, Scalar::ConvertPreview, Scalar::ResizeRGBA32FPixels }
        };
        dsimd_version_t version = SimdGetVersion();

        bool            x86 = (version >= DSIMD_VERSION_SSE2 && version <= DSIMD_VERSION_SSE42) || version >= DSIMD_VERSION_AVX2;
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(kernels); ++i)
        {
            const Kernels* selected = &kernels[i];
            if (selected->m_Version == version || selected->m_Version == DSIMD_VERSION_SCALAR ||
                (x86 && selected->m_Version != DSIMD_VERSION_NEON && selected->m_Version < version))
            {
                g_PreMultiplyAlpha = selected->m_PreMultiply;
                g_ResizeRGBA8 = selected->m_Resize;
                g_ConvertPreview = selected->m_Preview;
                g_ResizeRGBA32FPixels = selected->m_ResizeHDR;
                version = selected->m_Version;
#if defined(DM_TEXC_HAS_NEON)
                g_UseNeon = version == DSIMD_VERSION_NEON;
#endif
                break;
            }
        }
        static const char* const names[] = { "Scalar", "SSE2", "SSE3", "SSSE3", "SSE4.1", "SSE4.2", "NEON", "Wasm SIMD128", "AVX2", "AVX-512 (F+BW)" };
        // dmLog may not be initialized during library loading.
        fprintf(stderr, "INFO:TEXC: Selected %s\n", names[version]);
        return 0;
    }

    // Select once at library load, before JNI and native callers can enter texc.
    static int g_TexcSetupSimd = TexcSetupSimd();

    void       PreMultiplyAlpha(uint8_t* data, uint32_t width, uint32_t height)
    {
        g_PreMultiplyAlpha(data, width, height);
    }

    bool ResizeRGBA8(const uint8_t* source, uint32_t source_width, uint32_t source_height, uint8_t* destination, uint32_t width, uint32_t height)
    {
        return g_ResizeRGBA8(source, source_width, source_height, destination, width, height);
    }


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
#if defined(DM_TEXC_HAS_NEON)
        if (g_UseNeon)
        {
            NEON::RGBA8888ToRGB565(data, width * height, color_rgb);
            return;
        }
#endif
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
#if defined(DM_TEXC_HAS_NEON)
        if (g_UseNeon)
        {
            NEON::RGBA8888ToRGBA4444(data, width * height, color_rgba);
            return;
        }
#endif
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

    // This loop auto-vectorizes well; handwritten SIMD showed no worthwhile gain on NEON.
    void ABGR8888ToRGBA8888(const uint8_t* data, uint32_t width, uint32_t height, uint8_t* color_rgba)
    {
        for (uint32_t i = 0; i < width * height; ++i)
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
#if defined(DM_TEXC_HAS_NEON)
        if (g_UseNeon)
        {
            NEON::RGBA8888ToRGB888(data, width * height, color_rgb);
            return;
        }
#endif
        for(uint32_t i = 0; i < width*height; ++i)
        {
            *(color_rgb++) = *(data++);
            *(color_rgb++) = *(data++);
            *(color_rgb++) = *(data++);
            data++;
        }
    }

    void FlipImageX_RGBA8888(uint32_t* data, uint32_t width, uint32_t height)
    {
#if defined(DM_TEXC_HAS_NEON)
        if (g_UseNeon)
        {
            NEON::FlipImageX((uint8_t*)data, width, height, 4);
            return;
        }
#endif
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

    void FlipImageX_RGBA32F(float* data, uint32_t width, uint32_t height)
    {
#if defined(DM_TEXC_HAS_NEON)
        if (g_UseNeon)
        {
            NEON::FlipImageX((uint8_t*)data, width, height, 16);
            return;
        }
#endif
        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width/2; ++x)
            {
                uint32_t x2 = width - x - 1;
                float* a = data + (x + y * width) * 4;
                float* b = data + (x2 + y * width) * 4;
                for (uint32_t c = 0; c < 4; ++c)
                {
                    float t = a[c];
                    a[c] = b[c];
                    b[c] = t;
                }
            }
        }
    }

    void FlipImageY_RGBA32F(float* data, uint32_t width, uint32_t height)
    {
        if (!width || height < 2)
            return;
        size_t row_elements = (size_t)width * 4;
        for (uint32_t y = 0; y < height / 2; ++y)
        {
            float* a = data + (size_t)y * row_elements;
            float* b = data + (size_t)(height - y - 1) * row_elements;
            // Swap contiguous rows so the compiler can vectorize without channel-overlap checks.
            for (size_t x = 0; x < row_elements; ++x)
            {
                float value = a[x];
                a[x] = b[x];
                b[x] = value;
            }
        }
    }

    Image* ResizeRGBA8888(Image* image, uint32_t width, uint32_t height)
    {
        if (!width || !height || width > 16384 || height > 16384)
            return 0;
        uint32_t data_count = GetDataSize(PF_R8G8B8A8, width, height);
        if (!data_count)
            return 0;
        uint8_t* data = (uint8_t*)malloc(data_count);
        if (!data)
            return 0;
        if (!ResizeRGBA8(image->m_Data, image->m_Width, image->m_Height, data, width, height))
        {
            free(data);
            return 0;
        }
        Image* resized = new Image;
        resized->m_Path = 0;
        resized->m_Data = data;
        resized->m_DataCount = data_count;
        resized->m_Width = width;
        resized->m_Height = height;
        resized->m_PixelFormat = PF_R8G8B8A8;
        resized->m_ColorSpace = image->m_ColorSpace;
        return resized;
    }

    Image* ResizeRGBA32F(Image* image, uint32_t width, uint32_t height)
    {
        Image* resized = new Image;
        resized->m_Path = 0;
        resized->m_Width = width;
        resized->m_Height = height;
        resized->m_PixelFormat = PF_RGBA32F;
        resized->m_ColorSpace = image->m_ColorSpace;
        resized->m_DataCount = GetDataSize(PF_RGBA32F, width, height);
        if (resized->m_DataCount == 0)
        {
            delete resized;
            return 0;
        }

        resized->m_Data = (uint8_t*)malloc(resized->m_DataCount);
        if (!resized->m_Data)
        {
            delete resized;
            return 0;
        }

        g_ResizeRGBA32FPixels((const float*)image->m_Data, image->m_Width, image->m_Height, (float*)resized->m_Data, width, height);

        return resized;
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
        case PF_RGBA16F:    return 8;
        case PF_RGBA32F:    return 16;

        default:
            assert("not supported");
            return 0;
        }
    }

    uint32_t GetDataSize(PixelFormat pf, uint32_t width, uint32_t height)
    {
        uint32_t bytes_per_pixel = GetBytesPerPixel(pf);
        uint64_t data_size = (uint64_t)bytes_per_pixel * (uint64_t)width * (uint64_t)height;
        return data_size <= 0xffffffffULL ? (uint32_t)data_size : 0;
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

    bool ConvertRGBA32FToPf(const uint8_t* input, uint32_t width, uint32_t height, PixelFormat pf, void* out_data)
    {
        uint32_t input_data_size = GetDataSize(PF_RGBA32F, width, height);
        if (input_data_size == 0)
        {
            return false;
        }

        switch(pf)
        {
        case PF_RGBA32F:
            memcpy((uint8_t*)out_data, input, input_data_size);
            return true;
        case PF_RGBA16F:
        {
            const float* src = (const float*)input;
            uint16_t* dst = (uint16_t*)out_data;
            uint32_t component_count = input_data_size / sizeof(float);
#if defined(DM_TEXC_HAS_NEON)
            if (g_UseNeon)
            {
                NEON::FloatToHalf(src, component_count, dst);
                return true;
            }
#endif
            for (uint32_t i = 0; i < component_count; ++i)
            {
                dst[i] = FloatToHalf(src[i]);
            }
            return true;
        }
        default:
            dmLogError("ConvertRGBA32FToPf: Format not yet supported: %d", pf);
            return false;
        }
    }

    // NOTE: This is used by the Defold Editor when Generating a Preview Image for atlases
    // We merge all the required steps to optimize since this blocks the editor UI while this is generated
    void ConvertPremultiplyAndFlip_ABGR8888ToRGBA8888(const uint8_t* input_data, uint8_t* output_data, uint32_t width, uint32_t height)
    {
        g_ConvertPreview(input_data, output_data, width, height);
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
                data[0] = addNoise(data[0], (int8_t)(rnd * bpp_mul - bpp_bias));
                data[1] = addNoise(data[1], (int8_t)((1.0f - rnd) * bpp_mul - bpp_bias)); // As seen in the shadertoy by Mikkel Gjoel
                data[2] = addNoise(data[2], (int8_t)(rnd * bpp_mul - bpp_bias));
                data[3] = addNoise(data[3], (int8_t)(rnd * bpp_mul - bpp_bias));
                data += 4;
            }
        }
    }

    void DitherRGBx565(uint8_t* data, uint32_t width, uint32_t height)
    {
#if defined(DM_TEXC_HAS_NEON)
        if (g_UseNeon)
        {
            NEON::DitherRGBx565(data, width, height);
            return;
        }
#endif
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
                data[0] = addNoise(data[0], (int8_t)(rnd * bpp_mul_5 - bpp_bias_5));
                data[1] = addNoise(data[1], (int8_t)((1.0f - rnd) * bpp_mul_6 - bpp_bias_6)); // As seen in the shadertoy by Mikkel Gjoel
                data[2] = addNoise(data[2], (int8_t)(rnd * bpp_mul_5 - bpp_bias_5));
                // the alpha channel is ignored in this case
                data += 4;
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
