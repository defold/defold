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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <basis/encoder/basisu_enc.h>
#include <dlib/thread.h>
#include <dlib/mutex.h>
#include <simd/simd_runtime.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "texc_private.h"
#include "texc_resize.h"

#if defined(DM_TEXC_TEST_WIDE)
namespace dmTexc
{
    namespace Wide
    {
        void PreMultiplyAlpha(uint8_t*, uint32_t, uint32_t);
        void ConvertPreview(const uint8_t*, uint8_t*, uint32_t, uint32_t);
        bool ResizeRGBA8(const uint8_t*, uint32_t, uint32_t, uint8_t*, uint32_t, uint32_t);
        void ResizeRGBA32FPixels(const float*, uint32_t, uint32_t, float*, uint32_t, uint32_t);
    } // namespace Wide
    namespace Reference
    {
        bool ResizeRGBA8(const uint8_t*, uint32_t, uint32_t, uint8_t*, uint32_t, uint32_t);
    }
} // namespace dmTexc
#endif

struct TestKernels
{
    dsimd_version_t               m_Version;
    dmTexc::PreMultiplyAlphaFn    m_PreMultiply;
    dmTexc::ResizeRGBA8Fn         m_Resize;
    dmTexc::ConvertPreviewFn      m_Preview;
    dmTexc::ResizeRGBA32FPixelsFn m_ResizeHDR;
};

// Native fixtures cover the wrapper, fallback and supported kernels; emulated builds reuse them for one AVX path.
static const TestKernels g_Kernels[] = {
#if defined(DM_TEXC_TEST_WIDE)
    { DSIMD_VERSION_SCALAR, dmTexc::Wide::PreMultiplyAlpha, dmTexc::Wide::ResizeRGBA8, dmTexc::Wide::ConvertPreview, dmTexc::Wide::ResizeRGBA32FPixels },
#else
    { DSIMD_VERSION_SCALAR, dmTexc::PreMultiplyAlpha, dmTexc::ResizeRGBA8, dmTexc::ConvertPremultiplyAndFlip_ABGR8888ToRGBA8888, dmTexc::Scalar::ResizeRGBA32FPixels },
    { DSIMD_VERSION_SCALAR, dmTexc::Scalar::PreMultiplyAlpha, dmTexc::Scalar::ResizeRGBA8, dmTexc::Scalar::ConvertPreview, dmTexc::Scalar::ResizeRGBA32FPixels },
#if defined(DM_TEXC_HAS_SSE3)
    { DSIMD_VERSION_SSE3, dmTexc::SSE3::PreMultiplyAlpha, dmTexc::SSE3::ResizeRGBA8, dmTexc::SSE3::ConvertPreview, dmTexc::SSE3::ResizeRGBA32FPixels },
#endif
#if defined(DM_TEXC_HAS_SSE42)
    { DSIMD_VERSION_SSE42, dmTexc::SSE42::PreMultiplyAlpha, dmTexc::SSE42::ResizeRGBA8, dmTexc::SSE42::ConvertPreview, dmTexc::SSE42::ResizeRGBA32FPixels },
#endif
#if defined(DM_TEXC_HAS_AVX2)
    { DSIMD_VERSION_AVX2, dmTexc::AVX2::PreMultiplyAlpha, dmTexc::AVX2::ResizeRGBA8, dmTexc::AVX2::ConvertPreview, dmTexc::AVX2::ResizeRGBA32FPixels },
#endif
#if defined(DM_TEXC_HAS_AVX512)
    { DSIMD_VERSION_AVX512, dmTexc::AVX512::PreMultiplyAlpha, dmTexc::AVX512::ResizeRGBA8, dmTexc::AVX512::ConvertPreview, dmTexc::AVX512::ResizeRGBA32FPixels },
#endif
#if defined(DM_TEXC_HAS_NEON)
    { DSIMD_VERSION_NEON, dmTexc::NEON::PreMultiplyAlpha, dmTexc::NEON::ResizeRGBA8, dmTexc::NEON::ConvertPreview, dmTexc::NEON::ResizeRGBA32FPixels },
#endif
#endif
};

static bool IsSupported(dsimd_version_t version)
{
    dsimd_version_t supported = SimdGetVersion();
    bool            x86 = (supported >= DSIMD_VERSION_SSE2 && supported <= DSIMD_VERSION_SSE42) || supported >= DSIMD_VERSION_AVX2;
    return version == DSIMD_VERSION_SCALAR || version == supported ||
    (x86 && version >= DSIMD_VERSION_SSE2 && version <= DSIMD_VERSION_SSE42 && version <= supported) ||
    (supported == DSIMD_VERSION_AVX512 && version == DSIMD_VERSION_AVX2);
}

static uint32_t GetKernels(dmTexc::PreMultiplyAlphaFn* kernels)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < sizeof(g_Kernels) / sizeof(g_Kernels[0]); ++i)
        if (IsSupported(g_Kernels[i].m_Version))
            kernels[count++] = g_Kernels[i].m_PreMultiply;
    return count;
}

static uint32_t GetKernels(dmTexc::ResizeRGBA8Fn* kernels)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < sizeof(g_Kernels) / sizeof(g_Kernels[0]); ++i)
        if (IsSupported(g_Kernels[i].m_Version))
            kernels[count++] = g_Kernels[i].m_Resize;
    return count;
}

static uint32_t GetKernels(dmTexc::ConvertPreviewFn* kernels)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < sizeof(g_Kernels) / sizeof(g_Kernels[0]); ++i)
        if (IsSupported(g_Kernels[i].m_Version))
            kernels[count++] = g_Kernels[i].m_Preview;
    return count;
}

static uint32_t GetKernels(dmTexc::ResizeRGBA32FPixelsFn* kernels)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < sizeof(g_Kernels) / sizeof(g_Kernels[0]); ++i)
        if (IsSupported(g_Kernels[i].m_Version))
            kernels[count++] = g_Kernels[i].m_ResizeHDR;
    return count;
}

static uint32_t RandomBits(uint32_t* state)
{
    uint32_t bits = *state;
    bits ^= bits << 13;
    bits ^= bits >> 17;
    bits ^= bits << 5;
    *state = bits;
    return bits;
}

#if !defined(DM_TEXC_TEST_WIDE)
// Snapshot of the previous scalar conversion, used to check byte compatibility.
static uint16_t OriginalFloatToHalf(uint32_t bits)
{
    uint32_t sign = (bits >> 16) & 0x8000;
    int32_t  exponent = ((bits >> 23) & 0xff) - 112;
    uint32_t mantissa = bits & 0x7fffff;
    if (exponent <= 0)
    {
        if (exponent < -10)
            return (uint16_t)sign;
        mantissa = (mantissa | 0x800000) >> (1 - exponent);
        return (uint16_t)(sign | ((mantissa + 0x1000) >> 13));
    }
    if (exponent >= 31)
        return (uint16_t)(sign | 0x7c00 | (mantissa >> 13));
    return (uint16_t)(sign | ((exponent << 10) + ((mantissa + 0x1000) >> 13)));
}

static void ReferenceRGBA8Packing(const uint8_t* source, uint32_t count, dmTexc::PixelFormat format, uint8_t* destination)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint8_t* p = source + i * 4;
        if (format == dmTexc::PF_R8G8B8)
            memcpy(destination + i * 3, p, 3);
        else if (format == dmTexc::PF_L8A8)
        {
            destination[i * 2] = p[0];
            destination[i * 2 + 1] = p[3];
        }
        else
        {
            uint16_t packed = format == dmTexc::PF_R5G6B5 ? (uint16_t)(((p[0] >> 3) << 11) | ((p[1] >> 2) << 5) | (p[2] >> 3)) : (uint16_t)(((p[0] >> 4) << 12) | ((p[1] >> 4) << 8) | ((p[2] >> 4) << 4) | (p[3] >> 4));
            memcpy(destination + i * 2, &packed, sizeof(packed));
        }
    }
}

// Packing keeps legacy bits and red-channel luminance, including unaligned inputs, tails and allocation guards.
TEST(TexcImageOps, RGBA8PackingMatchesOriginal)
{
    const dmTexc::PixelFormat formats[] = { dmTexc::PF_R8G8B8, dmTexc::PF_L8A8, dmTexc::PF_R5G6B5, dmTexc::PF_R4G4B4A4 };
    for (uint32_t width = 0; width <= 65; ++width)
        for (uint32_t height = 0; height <= 7; ++height)
            for (uint32_t offset = 0; offset < 16; ++offset)
                for (uint32_t f = 0; f < sizeof(formats) / sizeof(formats[0]); ++f)
                {
                    uint32_t count = width * height;
                    uint32_t input_size = count * 4;
                    uint32_t output_size = count * (f == 0 ? 3 : 2);
                    uint32_t output_offset = (offset * 7 + 3) & 15;
                    if (f >= 2)
                        output_offset &= ~1u;
                    uint8_t* input = (uint8_t*)malloc(offset + input_size + 1);
                    uint8_t* original = (uint8_t*)malloc(offset + input_size + 1);
                    uint8_t* output = (uint8_t*)malloc(output_offset + output_size + 1);
                    uint8_t* expected = (uint8_t*)malloc(output_offset + output_size + 1);
                    memset(input, 0xa5, offset + input_size + 1);
                    memset(output, 0x5a, output_offset + output_size + 1);
                    memset(expected, 0x5a, output_offset + output_size + 1);
                    for (uint32_t i = 0; i < input_size; ++i)
                        input[offset + i] = (uint8_t)(i * 73 + width * 17 + offset * 31);
                    memcpy(original, input, offset + input_size + 1);
                    ReferenceRGBA8Packing(input + offset, count, formats[f], expected + output_offset);
                    dmTexc::ConvertRGBA8888ToPf(input + offset, width, height, formats[f], output + output_offset);
                    ASSERT_EQ(0, memcmp(expected, output, output_offset + output_size + 1));
                    ASSERT_EQ(0, memcmp(original, input, offset + input_size + 1));
                    free(input);
                    free(original);
                    free(output);
                    free(expected);
                }
}

// Shrinking conversions support identical input/output buffers and leave the unused source suffix untouched.
TEST(TexcImageOps, RGBA8PackingInPlace)
{
    const dmTexc::PixelFormat formats[] = { dmTexc::PF_R8G8B8, dmTexc::PF_L8A8, dmTexc::PF_R5G6B5, dmTexc::PF_R4G4B4A4 };
    for (uint32_t count = 0; count <= 257; ++count)
        for (uint32_t f = 0; f < sizeof(formats) / sizeof(formats[0]); ++f)
        {
            uint32_t size = count * 4;
            uint8_t* actual = (uint8_t*)malloc(size + 1);
            uint8_t* expected = (uint8_t*)malloc(size + 1);
            uint8_t* source = (uint8_t*)malloc(size + 1);
            for (uint32_t i = 0; i <= size; ++i)
                source[i] = (uint8_t)(i * 73 + count * 19);
            memcpy(actual, source, size + 1);
            memcpy(expected, source, size + 1);
            ReferenceRGBA8Packing(source, count, formats[f], expected);
            dmTexc::ConvertRGBA8888ToPf(actual, count, 1, formats[f], actual);
            ASSERT_EQ(0, memcmp(expected, actual, size + 1));
            free(actual);
            free(expected);
            free(source);
        }
}

// Reversing pixel positions must preserve all bits, including NaN payloads and signed zeros.
TEST(TexcImageOps, FlipsPreservePixelsAndAllocationBounds)
{
    uint32_t random = 0x648217ab;
    for (uint32_t width = 0; width <= 65; ++width)
    {
        for (uint32_t height = 0; height <= 7; ++height)
        {
            for (uint32_t pixel_size = 4; pixel_size <= 16; pixel_size += 12)
            {
                uint32_t size = width * height * pixel_size;
                for (uint32_t offset = 0; offset < 16; offset += 4)
                {
                    uint8_t* source = (uint8_t*)malloc(size + offset + 1);
                    uint8_t* actual = (uint8_t*)malloc(size + offset + 1);
                    memset(source, 0xa7, size + offset + 1);
                    for (uint32_t i = 0; i < size; ++i)
                        source[offset + i] = (uint8_t)RandomBits(&random);
                    for (uint32_t axis = 0; axis < 2; ++axis)
                    {
                        memcpy(actual, source, size + offset + 1);
                        if (pixel_size == 4)
                        {
                            if (axis == 0)
                                dmTexc::FlipImageX_RGBA8888((uint32_t*)(actual + offset), width, height);
                            else
                                dmTexc::FlipImageY_RGBA8888((uint32_t*)(actual + offset), width, height);
                        }
                        else
                        {
                            if (axis == 0)
                                dmTexc::FlipImageX_RGBA32F((float*)(actual + offset), width, height);
                            else
                                dmTexc::FlipImageY_RGBA32F((float*)(actual + offset), width, height);
                        }
                        for (uint32_t y = 0; y < height; ++y)
                        {
                            for (uint32_t x = 0; x < width; ++x)
                            {
                                uint32_t sx = axis == 0 ? width - 1 - x : x;
                                uint32_t sy = axis == 1 ? height - 1 - y : y;
                                ASSERT_EQ(0, memcmp(actual + offset + (y * width + x) * pixel_size, source + offset + (sy * width + sx) * pixel_size, pixel_size));
                            }
                        }
                        ASSERT_EQ(0, memcmp(source, actual, offset));
                        ASSERT_EQ(0xa7, actual[offset + size]);
                    }
                    free(actual);
                    free(source);
                }
            }
        }
    }
}

// Contiguous float-row swaps preserve special-value bits, the odd middle row, guards and zero-sized inputs.
TEST(TexcImageOps, FlipYRGBA32FPreservesSpecialValues)
{
    const uint32_t patterns[] = { 0, 0x80000000u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0xffc54321u, 0x7f800001u, 0xff800001u, 1, 0x80000001u, 0x7f7fffffu, 0xff7fffffu };
    const uint32_t width = 17, height = 5;
    const size_t   row_bytes = width * 4 * sizeof(float);
    const size_t   bytes = row_bytes * height;
    float*         storage = (float*)malloc(bytes + 2 * sizeof(float));
    float*         data = storage + 1;
    uint8_t*       source = (uint8_t*)malloc(bytes);
    memset(storage, 0xa7, bytes + 2 * sizeof(float));
    for (size_t i = 0; i < bytes / sizeof(float); ++i)
        memcpy(source + i * sizeof(float), &patterns[i % (sizeof(patterns) / sizeof(patterns[0]))], sizeof(float));
    memcpy(data, source, bytes);
    dmTexc::FlipImageY_RGBA32F(data, width, height);
    for (uint32_t y = 0; y < height; ++y)
        ASSERT_EQ(0, memcmp((uint8_t*)data + y * row_bytes, source + (height - y - 1) * row_bytes, row_bytes));
    dmTexc::FlipImageY_RGBA32F(data, width, height);
    ASSERT_EQ(0, memcmp(data, source, bytes));
    const uint8_t guard[] = { 0xa7, 0xa7, 0xa7, 0xa7 };
    ASSERT_EQ(0, memcmp(storage, guard, sizeof(guard)));
    ASSERT_EQ(0, memcmp((uint8_t*)data + bytes, guard, sizeof(guard)));
    dmTexc::FlipImageY_RGBA32F(0, 0, height);
    dmTexc::FlipImageY_RGBA32F(0, width, 0);
    dmTexc::FlipImageY_RGBA32F(0, width, 1);
    free(source);
    free(storage);
}

// SIMD rounding, subnormal handling and NaN/overflow payloads must match the previous scalar converter.
TEST(TexcImageOps, HalfConversionMatchesOriginalBits)
{
    const uint32_t mantissas[] = { 0, 1, 0xfff, 0x1000, 0x1001, 0x1fff, 0x2000, 0x2001, 0x3fefff, 0x3ff000, 0x3ff001, 0x7fefff, 0x7ff000, 0x7ff001, 0x7fffff };
    const uint32_t count = 2 * 256 * 15 + 1000000;
    uint32_t*      bits = (uint32_t*)malloc(count * sizeof(uint32_t));
    uint16_t*      actual = (uint16_t*)malloc(count * sizeof(uint16_t));
    uint32_t       index = 0;
    for (uint32_t sign = 0; sign < 2; ++sign)
        for (uint32_t exponent = 0; exponent < 256; ++exponent)
            for (uint32_t m = 0; m < DM_ARRAY_SIZE(mantissas); ++m)
                bits[index++] = (sign << 31) | (exponent << 23) | mantissas[m];
    uint32_t random = 0x891647a9;
    while (index < count)
        bits[index++] = RandomBits(&random);
    ASSERT_TRUE(dmTexc::ConvertRGBA32FToPf((const uint8_t*)bits, count / 4, 1, dmTexc::PF_RGBA16F, actual));
    for (uint32_t i = 0; i < count; ++i)
        ASSERT_EQ(OriginalFloatToHalf(bits[i]), actual[i]);
    free(actual);
    free(bits);
}

// Odd pixel counts and SIMD-unaligned buffers must not overrun the float or half arrays.
TEST(TexcImageOps, HalfConversionTailsAndAlignment)
{
    uint32_t random = 0x427819a3;
    for (uint32_t width = 1; width <= 65; ++width)
    {
        for (uint32_t offset = 0; offset < 4; ++offset)
        {
            uint32_t  count = width * 4;
            uint32_t* bits = (uint32_t*)malloc((count + offset) * sizeof(uint32_t));
            uint16_t* actual = (uint16_t*)malloc((count + offset) * sizeof(uint16_t));
            memset(actual, 0xa7, (count + offset) * sizeof(uint16_t));
            for (uint32_t i = 0; i < count; ++i)
                bits[offset + i] = RandomBits(&random);
            ASSERT_TRUE(dmTexc::ConvertRGBA32FToPf((const uint8_t*)(bits + offset), width, 1, dmTexc::PF_RGBA16F, actual + offset));
            for (uint32_t i = 0; i < offset; ++i)
                ASSERT_EQ(0xa7a7, actual[i]);
            for (uint32_t i = 0; i < count; ++i)
                ASSERT_EQ(OriginalFloatToHalf(bits[offset + i]), actual[offset + i]);
            free(actual);
            free(bits);
        }
    }
}

// Negative noise must survive vectorized loops and tails, with channel clamping unchanged.
TEST(TexcImageOps, DitherSignedNoiseAndWidthIndependence)
{
    const uint8_t values[] = { 0, 3, 7, 8, 72, 248, 252, 255 };
    for (uint32_t width = 1; width <= 65; ++width)
    {
        for (uint32_t v = 0; v < DM_ARRAY_SIZE(values); ++v)
        {
            uint8_t value = values[v];
            for (uint32_t format = 0; format < 2; ++format)
            {
                uint8_t data[65 * 4];
                memset(data, value, sizeof(data));
                if (format == 0)
                    dmTexc::DitherRGBA4444(data, width, 1);
                else
                    dmTexc::DitherRGBx565(data, width, 1);
                int     negative = format == 0 ? -8 : -4;
                int     positive = format == 0 ? 8 : 2;
                uint8_t red = (uint8_t)(value + negative < 0 ? 0 : value + negative);
                uint8_t green = (uint8_t)(value + positive > 255 ? 255 : value + positive);
                ASSERT_EQ(red, data[0]);
                ASSERT_EQ(green, data[1]);
                ASSERT_EQ(red, data[2]);
                ASSERT_EQ(format == 0 ? red : value, data[3]);
            }
        }
    }
}

static uint8_t DitherReferenceChannel(uint8_t value, int noise)
{
    int result = value + noise;
    return (uint8_t)(result < 0 ? 0 : result > 255 ? 255 :
                                                     result);
}

// Every pixel must use its global coordinates; vector blocks must leave RGB565 alpha intact.
TEST(TexcImageOps, DitherMatchesSignedReference)
{
    const uint32_t widths[] = { 1, 3, 4, 15, 16, 17, 31, 32, 33, 65, 257, 16384 };
    uint32_t       state = 0x719ab513;
    for (uint32_t w = 0; w < DM_ARRAY_SIZE(widths); ++w)
    {
        uint32_t width = widths[w];
        uint32_t height = width == 16384 ? 3 : 7;
        uint32_t size = width * height * 4;
        for (uint32_t offset = 0; offset < 4; ++offset)
        {
            uint8_t* input = (uint8_t*)malloc(size + offset);
            uint8_t* actual = (uint8_t*)malloc(size + offset);
            for (uint32_t i = 0; i < size + offset; ++i)
                input[i] = (uint8_t)RandomBits(&state);
            for (uint32_t format = 0; format < 2; ++format)
            {
                memcpy(actual, input, size + offset);
                if (format == 0)
                    dmTexc::DitherRGBA4444(actual + offset, width, height);
                else
                    dmTexc::DitherRGBx565(actual + offset, width, height);
                for (uint32_t y = 0; y < height; ++y)
                {
                    for (uint32_t x = 0; x < width; ++x)
                    {
                        float value = (float)x * 0.06711056f + (float)y * 0.00583715f;
                        value -= (int32_t)value;
                        value *= 52.9829189f;
                        float    random = value - (int32_t)value;
                        int      red = format == 0 ? (int)(random * 16 - 8) : (int)(random * 8 - 4);
                        int      green = format == 0 ? (int)((1.0f - random) * 16 - 8) : (int)((1.0f - random) * 4 - 2);
                        uint32_t pixel = offset + (y * width + x) * 4;
                        ASSERT_EQ(DitherReferenceChannel(input[pixel], red), actual[pixel]);
                        ASSERT_EQ(DitherReferenceChannel(input[pixel + 1], green), actual[pixel + 1]);
                        ASSERT_EQ(DitherReferenceChannel(input[pixel + 2], red), actual[pixel + 2]);
                        ASSERT_EQ(format == 0 ? DitherReferenceChannel(input[pixel + 3], red) : input[pixel + 3], actual[pixel + 3]);
                    }
                }
                ASSERT_EQ(0, memcmp(actual, input, offset));
            }
            free(actual);
            free(input);
        }
    }
}

#endif

// Exhausts all 65536 channel/alpha pairs and verifies truncation and unchanged alpha.
TEST(TexcPremultiply, AllChannelAlphaPairs)
{
    dmTexc::PreMultiplyAlphaFn kernels[8];
    uint32_t                   count = GetKernels(kernels);
    for (uint32_t k = 0; k < count; ++k)
    {
        uint8_t* data = (uint8_t*)malloc(256 * 256 * 4);
        ASSERT_NE((uint8_t*)0, data);
        for (uint32_t alpha = 0; alpha < 256; ++alpha)
            for (uint32_t channel = 0; channel < 256; ++channel)
            {
                uint8_t* pixel = data + (alpha * 256 + channel) * 4;
                pixel[0] = (uint8_t)channel;
                pixel[1] = (uint8_t)(255 - channel);
                pixel[2] = (uint8_t)((channel + 127) & 255);
                pixel[3] = (uint8_t)alpha;
            }
        kernels[k](data, 256, 256);
        for (uint32_t alpha = 0; alpha < 256; ++alpha)
            for (uint32_t channel = 0; channel < 256; ++channel)
            {
                const uint8_t* pixel = data + (alpha * 256 + channel) * 4;
                ASSERT_EQ(channel * alpha / 255, (uint32_t)pixel[0]);
                ASSERT_EQ((255 - channel) * alpha / 255, (uint32_t)pixel[1]);
                ASSERT_EQ(((channel + 127) & 255) * alpha / 255, (uint32_t)pixel[2]);
                ASSERT_EQ(alpha, (uint32_t)pixel[3]);
            }
        free(data);
    }
}

// Checks unaligned starts, exact allocation ends, small mips, odd dimensions and zero sizes.
TEST(TexcPremultiply, TailsAndAlignment)
{
    dmTexc::PreMultiplyAlphaFn kernels[8];
    uint32_t                   count = GetKernels(kernels);
    for (uint32_t k = 0; k < count; ++k)
    {
        static const uint32_t widths[] = { 0, 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 33 };
        static const uint32_t heights[] = { 0, 1, 3 };
        for (uint32_t offset = 0; offset < 16; ++offset)
            for (uint32_t w = 0; w < sizeof(widths) / sizeof(widths[0]); ++w)
                for (uint32_t h = 0; h < sizeof(heights) / sizeof(heights[0]); ++h)
                {
                    uint32_t count = widths[w] * heights[h] * 4;
                    uint8_t* allocation = (uint8_t*)malloc(offset + count + 1);
                    uint8_t* data = allocation + offset;
                    uint8_t  expected[33 * 3 * 4];
                    memset(allocation, 0xa5, offset + count + 1);
                    for (uint32_t i = 0; i < count; ++i)
                        data[i] = expected[i] = (uint8_t)(i * 37 + offset * 11);
                    for (uint32_t i = 0; i < count; i += 4)
                        for (uint32_t channel = 0; channel < 3; ++channel)
                            expected[i + channel] = (uint8_t)(expected[i + channel] * expected[i + 3] / 255);
                    kernels[k](data, widths[w], heights[h]);
                    ASSERT_EQ(0, memcmp(expected, data, count));
                    for (uint32_t i = 0; i < offset; ++i)
                        ASSERT_EQ(0xa5, allocation[i]);
                    ASSERT_EQ(0xa5, data[count]);
                    free(allocation);

                    // ASan catches any full-register read beyond the last valid pixel.
                    allocation = (uint8_t*)malloc(offset + count + 1);
                    data = allocation + offset + 1;
                    memset(allocation, 0xa5, offset + count + 1);
                    kernels[k](data, widths[w], heights[h]);
                    free(allocation);
                }
        kernels[k](0, 0, 8);
        kernels[k](0, 8, 0);
    }
}

#if !defined(DM_TEXC_TEST_WIDE)
static void Reference(const uint8_t* source, uint8_t* destination, uint32_t bytes)
{
    for (uint32_t i = 0; i < bytes; ++i)
        destination[i] = source[(i & ~3u) + 3 - (i & 3)];
}

// Every byte value reaches every channel, including the tail; in-place conversion is preserved.
TEST(TexcConvert, ByteOrderAndInPlace)
{
    uint8_t source[259 * 4], expected[259 * 4], output[259 * 4];
    for (uint32_t i = 0; i < sizeof(source); ++i)
        source[i] = (uint8_t)((i / 4) + (i % 4) * 61);
    Reference(source, expected, sizeof(source));
    dmTexc::ABGR8888ToRGBA8888(source, 259, 1, output);
    ASSERT_EQ(0, memcmp(expected, output, sizeof(output)));
    memcpy(output, source, sizeof(output));
    dmTexc::ABGR8888ToRGBA8888(output, 259, 1, output);
    ASSERT_EQ(0, memcmp(expected, output, sizeof(output)));
    dmTexc::ABGR8888ToRGBA8888(output, 259, 1, output);
    ASSERT_EQ(0, memcmp(source, output, sizeof(output)));
}

// Independent input/output alignments, odd dimensions and zero sizes cannot change guards or input.
TEST(TexcConvert, TailsAndAlignment)
{
    const uint32_t widths[] = { 0, 1, 2, 3, 4, 5, 7, 15, 16, 17, 31, 32, 33, 47, 48, 49, 63, 64, 65, 67 };
    const uint32_t heights[] = { 0, 1, 3 };
    for (uint32_t input_offset = 0; input_offset < 16; ++input_offset)
        for (uint32_t output_offset = 0; output_offset < 16; ++output_offset)
            for (uint32_t w = 0; w < sizeof(widths) / sizeof(widths[0]); ++w)
                for (uint32_t h = 0; h < sizeof(heights) / sizeof(heights[0]); ++h)
                {
                    uint32_t bytes = widths[w] * heights[h] * 4;
                    uint8_t  source[16 + 67 * 3 * 4 + 1], original[sizeof(source)];
                    uint8_t  output[sizeof(source)], expected[67 * 3 * 4];
                    memset(source, 0xa5, sizeof(source));
                    memset(output, 0x5a, sizeof(output));
                    for (uint32_t i = 0; i < bytes; ++i)
                        source[input_offset + i] = (uint8_t)(i * 37 + input_offset * 11);
                    memcpy(original, source, sizeof(source));
                    Reference(source + input_offset, expected, bytes);
                    dmTexc::ABGR8888ToRGBA8888(source + input_offset, widths[w], heights[h], output + output_offset);
                    ASSERT_EQ(0, memcmp(expected, output + output_offset, bytes));
                    ASSERT_EQ(0, memcmp(original, source, sizeof(source)));
                    for (uint32_t i = 0; i < output_offset; ++i)
                        ASSERT_EQ(0x5a, output[i]);
                    for (uint32_t i = output_offset + bytes; i < sizeof(output); ++i)
                        ASSERT_EQ(0x5a, output[i]);
                }
    dmTexc::ABGR8888ToRGBA8888(0, 0, 8, 0);
    dmTexc::ABGR8888ToRGBA8888(0, 8, 0, 0);
}

// Exact allocation ends let ASan detect full-register reads or writes beyond small mips.
TEST(TexcConvert, ExactAllocationEnds)
{
    for (uint32_t pixels = 1; pixels < 68; ++pixels)
        for (uint32_t offset = 0; offset < 16; ++offset)
        {
            uint32_t bytes = pixels * 4;
            uint8_t* source = (uint8_t*)malloc(offset + bytes);
            uint8_t* output = (uint8_t*)malloc(15 - offset + bytes);
            uint8_t  expected[67 * 4];
            for (uint32_t i = 0; i < bytes; ++i)
                source[offset + i] = (uint8_t)(i * 73 + offset);
            Reference(source + offset, expected, bytes);
            dmTexc::ABGR8888ToRGBA8888(source + offset, pixels, 1, output + 15 - offset);
            ASSERT_EQ(0, memcmp(expected, output + 15 - offset, bytes));
            free(source);
            free(output);
        }
}

// Bob's ABGR image creation reaches the readable converter and preserves its metadata.
TEST(TexcConvert, ImageCreationUsesConversion)
{
    uint8_t source[33 * 3 * 4], expected[sizeof(source)];
    for (uint32_t i = 0; i < sizeof(source); ++i)
        source[i] = (uint8_t)(i * 19);
    Reference(source, expected, sizeof(source));
    dmTexc::Image* image = dmTexc::CreateImage("abgr-conversion", 33, 3, dmTexc::PF_A8B8G8R8, dmTexc::CS_SRGB, sizeof(source), source);
    ASSERT_NE((dmTexc::Image*)0, image);
    ASSERT_EQ(33u, image->m_Width);
    ASSERT_EQ(3u, image->m_Height);
    ASSERT_EQ(dmTexc::PF_A8B8G8R8, image->m_PixelFormat);
    ASSERT_EQ(dmTexc::CS_SRGB, image->m_ColorSpace);
    ASSERT_EQ(0, strcmp("abgr-conversion", image->m_Path));
    ASSERT_EQ(sizeof(source), (size_t)image->m_DataCount);
    ASSERT_EQ(0, memcmp(expected, image->m_Data, sizeof(expected)));
    dmTexc::DestroyImage(image);
}

#endif

static void Reference(const uint8_t* source, uint8_t* destination, uint32_t width, uint32_t height)
{
    for (uint32_t y = 0; y < height; ++y)
        for (uint32_t x = 0; x < width; ++x)
        {
            size_t input = ((size_t)y * width + x) * 4;
            size_t output = ((size_t)(height - 1 - y) * width + x) * 4;
            for (uint32_t c = 0; c < 3; ++c)
                destination[output + c] = (uint8_t)((uint32_t)source[input + 3 - c] * source[input] / 255);
            destination[output + 3] = source[input];
        }
}

// Every channel/alpha pair must truncate exactly, preserve alpha and reverse rows without reversing columns.
TEST(TexcPreview, AllChannelAlphaPairs)
{
    const uint32_t width = 257, height = 256;
    size_t         bytes = (size_t)width * height * 4;
    uint8_t*       source = (uint8_t*)malloc(bytes);
    uint8_t*       expected = (uint8_t*)malloc(bytes);
    uint8_t*       output = (uint8_t*)malloc(bytes);
    for (uint32_t i = 0; i < width * height; ++i)
    {
        source[i * 4] = (uint8_t)(i >> 8);
        source[i * 4 + 1] = (uint8_t)i;
        source[i * 4 + 2] = (uint8_t)(i + 61);
        source[i * 4 + 3] = (uint8_t)(255 - i);
    }
    Reference(source, expected, width, height);
    dmTexc::ConvertPreviewFn kernels[8];
    uint32_t                 count = GetKernels(kernels);
    for (uint32_t k = 0; k < count; ++k)
    {
        kernels[k](source, output, width, height);
        ASSERT_EQ(0, memcmp(expected, output, bytes));
    }
    free(source);
    free(expected);
    free(output);
}

// Odd rows, every pointer alignment and exact allocation ends guard against crossing row tails or touching input.
TEST(TexcPreview, AlignmentRowsAndTails)
{
    const uint32_t           widths[] = { 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 257 };
    const uint32_t           heights[] = { 1, 2, 3, 5 };
    dmTexc::ConvertPreviewFn kernels[8];
    uint32_t                 count = GetKernels(kernels);
    for (uint32_t offset = 0; offset < 64; ++offset)
        for (uint32_t w = 0; w < sizeof(widths) / sizeof(widths[0]); ++w)
            for (uint32_t h = 0; h < sizeof(heights) / sizeof(heights[0]); ++h)
                for (uint32_t footer = 0; footer < 2; ++footer)
                {
                    size_t   bytes = (size_t)widths[w] * heights[h] * 4;
                    uint32_t input_offset = offset + 1, output_offset = ((offset * 13 + 11) & 63) + 1;
                    uint8_t* input_storage = (uint8_t*)malloc(input_offset + bytes + footer);
                    uint8_t* output_storage = (uint8_t*)malloc(output_offset + bytes + footer);
                    uint8_t* source = input_storage + input_offset;
                    uint8_t* output = output_storage + output_offset;
                    uint8_t* expected = (uint8_t*)malloc(bytes);
                    uint8_t* original = (uint8_t*)malloc(bytes);
                    memset(input_storage, 0xa5, input_offset + bytes + footer);
                    for (size_t i = 0; i < bytes; ++i)
                        source[i] = (uint8_t)(i * 37 + offset * 11);
                    memcpy(original, source, bytes);
                    Reference(source, expected, widths[w], heights[h]);
                    for (uint32_t k = 0; k < count; ++k)
                    {
                        memset(output_storage, 0x5a, output_offset + bytes + footer);
                        kernels[k](source, output, widths[w], heights[h]);
                        ASSERT_EQ(0, memcmp(expected, output, bytes));
                        ASSERT_EQ(0, memcmp(original, source, bytes));
                        ASSERT_EQ(0x5a, output_storage[output_offset - 1]);
                        ASSERT_EQ(0xa5, input_storage[input_offset - 1]);
                        if (footer)
                        {
                            ASSERT_EQ(0x5a, output[bytes]);
                            ASSERT_EQ(0xa5, source[bytes]);
                        }
                    }
                    free(input_storage);
                    free(output_storage);
                    free(expected);
                    free(original);
                }
    for (uint32_t k = 0; k < count; ++k)
    {
        kernels[k](0, 0, 0, 17);
        kernels[k](0, 0, 17, 0);
    }
}

// The public editor entry point must keep the exact conversion, truncation and vertical-flip behavior.
TEST(TexcPreview, CreatePreviewImage)
{
    const uint8_t input[] = { 128, 64, 128, 255, 255, 4, 5, 6, 0, 255, 255, 255, 1, 255, 255, 255 };
    const uint8_t expected[] = { 0, 0, 0, 0, 1, 1, 1, 1, 128, 64, 32, 128, 6, 5, 4, 255 };
    uint8_t       output[sizeof(input)];
    dmTexc::CreatePreviewImage(2, 2, sizeof(input), input, output);
    ASSERT_EQ(0, memcmp(expected, output, sizeof(output)));
}

// Match Lanczos4 weights, edges and byte rounding where BasisU's costs do not overflow.
TEST(TexcResize, MatchesBasisLanczos4)
{
    const uint32_t sizes[][4] = {
        { 1, 1, 7, 9 }, { 2, 2, 4, 4 }, { 3, 5, 11, 7 }, { 11, 7, 3, 5 }, { 1, 37, 13, 3 }, { 37, 1, 3, 13 }, { 19, 31, 7, 41 }, { 19, 31, 41, 7 }, { 63, 65, 32, 33 }, { 64, 64, 32, 32 }, { 64, 64, 1, 1 }, { 7, 9, 7, 9 }, { 71, 53, 97, 83 }, { 512, 1, 1, 1 }, { 1, 512, 1, 1 }, { 16384, 1, 8, 1 },
        { 512, 512, 32, 32 }, { 512, 512, 16, 16 }, { 512, 256, 32, 16 }, { 256, 512, 16, 32 }, { 64, 512, 64, 32 }, { 64, 1024, 64, 32 }
    };
    dmTexc::ResizeRGBA8Fn kernels[8];
    uint32_t              kernel_count = GetKernels(kernels);
    uint32_t              state = 987654321;
    for (uint32_t shape = 0; shape < sizeof(sizes) / sizeof(sizes[0]); ++shape)
    {
        uint32_t sw = sizes[shape][0], sh = sizes[shape][1], dw = sizes[shape][2], dh = sizes[shape][3];
        size_t   source_size = (size_t)sw * sh * 4, destination_size = (size_t)dw * dh * 4;
        uint8_t* storage = (uint8_t*)malloc(source_size + 16);
        uint8_t* output = (uint8_t*)malloc(destination_size + 32);
        uint8_t* source = storage + shape % 16;
        for (uint32_t pattern = 0; pattern < 5; ++pattern)
        {
            for (size_t i = 0; i < source_size; ++i)
            {
                uint32_t x = (uint32_t)(i / 4) % sw, y = (uint32_t)(i / 4) / sw, c = (uint32_t)i % 4;
                switch (pattern)
                {
                    case 0:
                        source[i] = (uint8_t)(37 + c * 53);
                        break;
                    case 1:
                        source[i] = (x == sw / 2 && y == sh / 2) ? 255 : 0;
                        break;
                    case 2:
                        source[i] = (uint8_t)(x * 3 + y * 5 + c * 29);
                        break;
                    case 3:
                        source[i] = ((x + y + c) % 2) ? 255 : 0;
                        break;
                    default:
                        source[i] = (uint8_t)RandomBits(&state);
                        break;
                }
            }
            basisu::image original;
            original.init(source, sw, sh, 4);
            basisu::image expected(dw, dh);
#if defined(DM_TEXC_TEST_WIDE)
            // The portable x86 kernels use separate multiply/add. Compare with a
            // scalar build using the same rounding, rather than ARM64 BasisU's FMA.
            ASSERT_TRUE(dmTexc::Reference::ResizeRGBA8(source, sw, sh, (uint8_t*)expected.get_ptr(), dw, dh));
#else
            ASSERT_TRUE(basisu::image_resample(original, expected));
#endif
            for (uint32_t k = 0; k < kernel_count; ++k)
            {
                uint32_t offset = (shape + pattern) % 16;
                memset(output, 0xa5, destination_size + 32);
                ASSERT_TRUE(kernels[k](source, sw, sh, output + offset, dw, dh));
                const uint8_t* reference = (const uint8_t*)expected.get_ptr();
                for (size_t i = 0; i < destination_size; ++i)
                {
                    if (reference[i] != output[offset + i])
                        printf("Mismatch %ux%u -> %ux%u, pattern %u, kernel %u, byte %zu: %u != %u\n", sw, sh, dw, dh, pattern, k, i, output[offset + i], reference[i]);
                    ASSERT_EQ(reference[i], output[offset + i]);
                }
                for (size_t i = 0; i < offset; ++i)
                    ASSERT_EQ(0xa5, output[i]);
                for (size_t i = offset + destination_size; i < destination_size + 32; ++i)
                    ASSERT_EQ(0xa5, output[i]);
            }
        }
        free(storage);
        free(output);
    }
}

namespace ResizePassOrder
{
    enum Pass
    {
        NONE,
        HORIZONTAL,
        VERTICAL
    };
    static Pass     g_FirstPass;
    static uint32_t g_ConvertedRows;

    // Observe row callback order without performing expensive intermediate filtering.
    static void ConvertRow(const uint8_t*, float*, uint32_t)
    {
        ++g_ConvertedRows;
    }
    static void FilterHorizontal(const float*, float*, uint32_t, const dmTexc::ResizeAxis*)
    {
        if (g_FirstPass == NONE)
            g_FirstPass = HORIZONTAL;
    }
    static void AccumulateRow(const float*, float*, size_t, float, bool)
    {
        if (g_FirstPass == NONE)
            g_FirstPass = VERTICAL;
    }
    static void StoreRow(const float*, uint8_t*, uint32_t)
    {
    }
} // namespace ResizePassOrder

// Retain overlapping rows for 16x/32x reductions and ordinary vertical-first passes, without repeated filtering.
TEST(TexcResize, ReductionsReuseFilteredRows)
{
    const struct
    {
        uint32_t              m_SourceWidth, m_SourceHeight, m_Width, m_Height;
        ResizePassOrder::Pass m_FirstPass;
    } cases[] = {
        { 512, 512, 32, 32, ResizePassOrder::HORIZONTAL },
        { 1024, 1024, 32, 32, ResizePassOrder::HORIZONTAL },
        { 1024, 512, 64, 32, ResizePassOrder::HORIZONTAL },
        { 512, 1024, 32, 64, ResizePassOrder::HORIZONTAL },
        { 512, 512, 512, 256, ResizePassOrder::VERTICAL },
        { 512, 512, 512, 128, ResizePassOrder::VERTICAL },
        { 512, 512, 512, 64, ResizePassOrder::VERTICAL }
    };
    const dmTexc::ResizeKernels kernels = {
        ResizePassOrder::ConvertRow, ResizePassOrder::FilterHorizontal, ResizePassOrder::AccumulateRow, ResizePassOrder::StoreRow
    };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(cases); ++i)
    {
        uint32_t sw = cases[i].m_SourceWidth, sh = cases[i].m_SourceHeight, dw = cases[i].m_Width, dh = cases[i].m_Height;
        uint8_t* source = (uint8_t*)malloc((size_t)sw * sh * 4);
        uint8_t* destination = (uint8_t*)malloc((size_t)dw * dh * 4);
        ASSERT_NE((uint8_t*)0, source);
        ASSERT_NE((uint8_t*)0, destination);
        ResizePassOrder::g_FirstPass = ResizePassOrder::NONE;
        ResizePassOrder::g_ConvertedRows = 0;
        ASSERT_TRUE(dmTexc::ResizeRGBA8WithKernels(source, sw, sh, destination, dw, dh, &kernels));
        ASSERT_EQ(cases[i].m_FirstPass, ResizePassOrder::g_FirstPass);
        ASSERT_EQ(sh, ResizePassOrder::g_ConvertedRows);
        free(source);
        free(destination);
    }
}

// Extreme aspect ratios exercise large pass costs using small input/output buffers.
TEST(TexcResize, PassOrderCostsDoNotOverflow)
{
    static const struct
    {
        uint32_t              m_SourceWidth, m_SourceHeight, m_Width, m_Height;
        ResizePassOrder::Pass m_FirstPass;
    } cases[] = {
        { 1, 16384, 12288, 1, ResizePassOrder::VERTICAL },
        { 16384, 1, 1, 12288, ResizePassOrder::HORIZONTAL },
        { 2, 4, 4, 2, ResizePassOrder::VERTICAL },
        { 4, 2, 2, 4, ResizePassOrder::HORIZONTAL }
    };
    const dmTexc::ResizeKernels kernels = {
        ResizePassOrder::ConvertRow, ResizePassOrder::FilterHorizontal, ResizePassOrder::AccumulateRow, ResizePassOrder::StoreRow
    };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(cases); ++i)
    {
        uint8_t* source = (uint8_t*)malloc((size_t)cases[i].m_SourceWidth * cases[i].m_SourceHeight * 4);
        uint8_t* destination = (uint8_t*)malloc((size_t)cases[i].m_Width * cases[i].m_Height * 4);
        ASSERT_NE((uint8_t*)0, source);
        ASSERT_NE((uint8_t*)0, destination);
        ResizePassOrder::g_FirstPass = ResizePassOrder::NONE;
        ASSERT_TRUE(dmTexc::ResizeRGBA8WithKernels(source, cases[i].m_SourceWidth, cases[i].m_SourceHeight, destination, cases[i].m_Width, cases[i].m_Height, &kernels));
        ASSERT_EQ(cases[i].m_FirstPass, ResizePassOrder::g_FirstPass);
        free(source);
        free(destination);
    }
}

// A 16384-to-1 reduction exceeds BasisU's 16-bit tap count; keep known constant colors.
TEST(TexcResize, MaximumReductionPreservesUniformColor)
{
    uint8_t  color[] = { 37, 90, 143, 196 };
    uint8_t* source = (uint8_t*)malloc(16384 * 4);
    for (uint32_t i = 0; i < 16384; ++i)
        memcpy(source + i * 4, color, 4);
    dmTexc::ResizeRGBA8Fn kernels[8];
    uint32_t              count = GetKernels(kernels);
    for (uint32_t i = 0; i < count; ++i)
    {
        uint8_t output[4];
        ASSERT_TRUE(kernels[i](source, 16384, 1, output, 1, 1));
        ASSERT_ARRAY_EQ_LEN(color, output, 4);
        ASSERT_TRUE(kernels[i](source, 1, 16384, output, 1, 1));
        ASSERT_ARRAY_EQ_LEN(color, output, 4);
    }
    free(source);
}

// Invalid dimensions fail without writing, and same-size copies keep every byte.
TEST(TexcResize, DimensionsAndIdentity)
{
    uint8_t               source[4] = { 0, 37, 255, 128 };
    uint8_t               destination[4] = { 0xa5, 0xa5, 0xa5, 0xa5 };
    dmTexc::ResizeRGBA8Fn kernels[8];
    uint32_t              count = GetKernels(kernels);
    for (uint32_t i = 0; i < count; ++i)
    {
        ASSERT_FALSE(kernels[i](source, 0, 1, destination, 1, 1));
        ASSERT_FALSE(kernels[i](source, 1, 0, destination, 1, 1));
        ASSERT_FALSE(kernels[i](source, 1, 1, destination, 0, 1));
        ASSERT_FALSE(kernels[i](source, 1, 1, destination, 1, 0));
        ASSERT_FALSE(kernels[i](source, 16385, 1, destination, 1, 1));
        ASSERT_FALSE(kernels[i](source, 1, 1, destination, 1, 16385));
        ASSERT_EQ(0xa5, destination[0]);
        ASSERT_TRUE(kernels[i](source, 1, 1, destination, 1, 1));
        ASSERT_ARRAY_EQ_LEN(source, destination, 4);
        memset(destination, 0xa5, sizeof(destination));
    }
}

// Exact allocations expose source overreads and destination overwrites at small mip tails.
TEST(TexcResize, ExactAllocationEnds)
{
    const uint8_t         color[] = { 0, 37, 255, 128 };
    dmTexc::ResizeRGBA8Fn kernels[8];
    uint32_t              count = GetKernels(kernels);
    for (uint32_t sw = 1; sw <= 7; ++sw)
    {
        for (uint32_t sh = 1; sh <= 5; ++sh)
        {
            uint32_t dw = sw * 2 + 1, dh = sh * 2 + 1;
            uint8_t* source = (uint8_t*)malloc(sw * sh * 4);
            uint8_t* destination = (uint8_t*)malloc(dw * dh * 4);
            for (uint32_t i = 0; i < sw * sh; ++i)
                memcpy(source + i * 4, color, 4);
            for (uint32_t k = 0; k < count; ++k)
            {
                ASSERT_TRUE(kernels[k](source, sw, sh, destination, dw, dh));
                for (uint32_t i = 0; i < dw * dh; ++i)
                    ASSERT_ARRAY_EQ_LEN(color, destination + i * 4, 4);
                for (uint32_t i = 0; i < sw * sh; ++i)
                    ASSERT_ARRAY_EQ_LEN(color, source + i * 4, 4);
            }
            free(source);
            free(destination);
        }
    }
}

// Independent known colors and public image metadata remain correct for both color spaces.
TEST(TexcResize, UniformColorAndImageMetadata)
{
    uint8_t color[] = { 0, 37, 255, 128 };
    for (uint32_t space = 0; space < 2; ++space)
    {
        dmTexc::Image* image = dmTexc::CreateImage("resize-test", 1, 1, dmTexc::PF_R8G8B8A8, (dmTexc::ColorSpace)space, sizeof(color), color);
        dmTexc::Image* resized = dmTexc::Resize(image, 13, 7);
        ASSERT_NE((dmTexc::Image*)0, resized);
        ASSERT_EQ(dmTexc::PF_R8G8B8A8, resized->m_PixelFormat);
        ASSERT_EQ(space, (uint32_t)resized->m_ColorSpace);
        ASSERT_EQ(13u, resized->m_Width);
        ASSERT_EQ(7u, resized->m_Height);
        ASSERT_EQ(13u * 7u * 4u, resized->m_DataCount);
        ASSERT_EQ(0, strcmp("resize-test", resized->m_Path));
        for (uint32_t pixel = 0; pixel < 13 * 7; ++pixel)
            ASSERT_ARRAY_EQ_LEN(color, resized->m_Data + pixel * 4, 4);
        ASSERT_EQ((dmTexc::Image*)0, dmTexc::Resize(image, 0, 7));
        dmTexc::DestroyImage(resized);
        dmTexc::DestroyImage(image);
    }
}

struct ResizeThreadJob
{
    const uint8_t*  m_Source;
    uint8_t*        m_Destination;
    uint32_t        m_Width;
    uint32_t        m_Height;
    dmMutex::HMutex m_Start;
    bool            m_Success;
};

static void ResizeWorker(void* argument)
{
    ResizeThreadJob* job = (ResizeThreadJob*)argument;
    dmMutex::Lock(job->m_Start);
    dmMutex::Unlock(job->m_Start);
    job->m_Success = dmTexc::ResizeRGBA8(job->m_Source, 67, 53, job->m_Destination, job->m_Width, job->m_Height);
}

// Independent Bob-style worker calls share read-only input and keep separate scratch/output.
TEST(TexcResize, ConcurrentCallsHaveIndependentScratch)
{
    uint8_t  source[67 * 53 * 4];
    uint32_t state = 123456789;
    for (size_t i = 0; i < sizeof(source); ++i)
        source[i] = (uint8_t)RandomBits(&state);
    const uint32_t   sizes[][2] = { { 31, 29 }, { 81, 41 } };
    ResizeThreadJob  jobs[2];
    dmThread::Thread threads[2];
    uint8_t*         expected[2];
    bool             expected_success[2];
    dmMutex::HMutex  start = dmMutex::New();
    dmMutex::Lock(start);
    for (uint32_t i = 0; i < 2; ++i)
    {
        size_t size = sizes[i][0] * sizes[i][1] * 4;
        expected[i] = (uint8_t*)malloc(size);
        jobs[i].m_Source = source;
        jobs[i].m_Destination = (uint8_t*)malloc(size);
        jobs[i].m_Width = sizes[i][0];
        jobs[i].m_Height = sizes[i][1];
        jobs[i].m_Start = start;
        jobs[i].m_Success = false;
        expected_success[i] = dmTexc::ResizeRGBA8(source, 67, 53, expected[i], sizes[i][0], sizes[i][1]);
        threads[i] = dmThread::New(ResizeWorker, 0x80000, &jobs[i], "texc_resize");
    }
    dmMutex::Unlock(start);
    for (uint32_t i = 0; i < 2; ++i)
        dmThread::Join(threads[i]);
    dmMutex::Delete(start);
    for (uint32_t i = 0; i < 2; ++i)
    {
        ASSERT_TRUE(expected_success[i]);
        ASSERT_TRUE(jobs[i].m_Success);
        ASSERT_ARRAY_EQ_LEN(expected[i], jobs[i].m_Destination, sizes[i][0] * sizes[i][1] * 4);
        free(expected[i]);
        free(jobs[i].m_Destination);
    }
}

// Float coordinates preserve the sampling contract; double interpolation checks numerical accuracy independently.
static void CheckReference(const float* source, uint32_t sw, uint32_t sh, const float* output, uint32_t dw, uint32_t dh)
{
    float sx = (float)sw / dw, sy = (float)sh / dh;
    for (uint32_t y = 0; y < dh; ++y)
        for (uint32_t x = 0; x < dw; ++x)
        {
            float px = (x + 0.5f) * sx - 0.5f, py = (y + 0.5f) * sy - 0.5f;
            px = px < 0.0f ? 0.0f : px;
            py = py < 0.0f ? 0.0f : py;
            uint32_t x0 = (uint32_t)px < sw ? (uint32_t)px : sw - 1;
            uint32_t y0 = (uint32_t)py < sh ? (uint32_t)py : sh - 1;
            uint32_t x1 = x0 + 1 < sw ? x0 + 1 : sw - 1;
            uint32_t y1 = y0 + 1 < sh ? y0 + 1 : sh - 1;
            double   tx = px - x0, ty = py - y0;
            for (uint32_t c = 0; c < 4; ++c)
            {
                double a = source[((size_t)y0 * sw + x0) * 4 + c];
                double b = source[((size_t)y0 * sw + x1) * 4 + c];
                double d = source[((size_t)y1 * sw + x0) * 4 + c];
                double e = source[((size_t)y1 * sw + x1) * 4 + c];
                double top = a * (1.0 - tx) + b * tx;
                double bottom = d * (1.0 - tx) + e * tx;
                double expected = top * (1.0 - ty) + bottom * ty;
                double scale = fmax(fmax(fabs(a), fabs(b)), fmax(fabs(d), fabs(e)));
                double tolerance = FLT_EPSILON * (8.0 * scale + 4.0 * fabs(expected));
                ASSERT_LE(fabs(output[((size_t)y * dw + x) * 4 + c] - expected), tolerance);
            }
        }
}

// Known averages, unclamped HDR ranges and repeated edge pixels guard sampling and RGBA lane order.
TEST(TexcResizeHDR, KnownColorsAndEdges)
{
    const float                   input[] = { -8, 2, 16, 0, 0, 6, 32, 1, 8, 10, 48, 2, 16, 14, 64, 3 };
    const float                   average[] = { 4, 8, 40, 1.5f };
    dmTexc::ResizeRGBA32FPixelsFn kernels[8];
    uint32_t                      count = GetKernels(kernels);
    for (uint32_t k = 0; k < count; ++k)
    {
        float output[4 * 4 * 4];
        kernels[k](input, 2, 2, output, 1, 1);
        ASSERT_EQ(0, memcmp(average, output, sizeof(average)));
        kernels[k](input, 2, 2, output, 4, 4);
        ASSERT_EQ(0, memcmp(input, output, 4 * sizeof(float)));
        ASSERT_EQ(0, memcmp(input + 12, output + 60, 4 * sizeof(float)));
        CheckReference(input, 2, 2, output, 4, 4);
        kernels[k](average, 1, 1, output, 4, 4);
        for (uint32_t i = 0; i < 16; ++i)
            ASSERT_EQ(0, memcmp(average, output + i * 4, sizeof(average)));
    }
}

// Upsampling, reduction, identity and one-dimensional images use an independent double reference.
TEST(TexcResizeHDR, ShapesAndNumericalRange)
{
    const uint32_t shapes[][4] = {
        { 1, 1, 7, 9 }, { 1, 37, 13, 3 }, { 37, 1, 3, 13 }, { 3, 5, 11, 7 }, { 11, 7, 3, 5 }, { 19, 31, 7, 41 }, { 19, 31, 41, 7 }, { 63, 65, 32, 33 }, { 64, 64, 1, 1 }, { 7, 9, 7, 9 }, { 71, 53, 97, 83 }, { 16384, 1, 8, 1 }
    };
    dmTexc::ResizeRGBA32FPixelsFn kernels[8];
    uint32_t                      count = GetKernels(kernels), state = 987654321;
    for (uint32_t i = 0; i < sizeof(shapes) / sizeof(shapes[0]); ++i)
    {
        uint32_t sw = shapes[i][0], sh = shapes[i][1], dw = shapes[i][2], dh = shapes[i][3];
        float*   input = (float*)malloc((size_t)sw * sh * 4 * sizeof(float));
        float*   output = (float*)malloc((size_t)dw * dh * 4 * sizeof(float));
        for (size_t j = 0; j < (size_t)sw * sh * 4; ++j)
            input[j] = ldexpf((float)((int32_t)(RandomBits(&state) & 65535) - 32768), ((int32_t)j % 4) * 20 - 35);
        for (uint32_t k = 0; k < count; ++k)
        {
            kernels[k](input, sw, sh, output, dw, dh);
            CheckReference(input, sw, sh, output, dw, dh);
        }
        free(input);
        free(output);
    }
}

// Float-aligned, SIMD-unaligned pointers and exact allocation ends prevent overreads and writes outside the image.
TEST(TexcResizeHDR, AlignmentAndAllocationEnds)
{
    const uint32_t                widths[] = { 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17 };
    dmTexc::ResizeRGBA32FPixelsFn kernels[8];
    uint32_t                      count = GetKernels(kernels);
    for (uint32_t offset = 0; offset < 16; ++offset)
        for (uint32_t w = 0; w < sizeof(widths) / sizeof(widths[0]); ++w)
            for (uint32_t footer = 0; footer < 2; ++footer)
            {
                uint32_t sw = widths[w], sh = 3, dw = sw * 2 + 1, dh = 5;
                size_t   inputs = (size_t)sw * sh * 4, outputs = (size_t)dw * dh * 4;
                size_t   input_offset = offset + 1, output_offset = (offset * 13 % 16) + 1;
                float*   input_storage = (float*)malloc((input_offset + inputs + footer) * sizeof(float));
                float*   output_storage = (float*)malloc((output_offset + outputs + footer) * sizeof(float));
                float*   input = input_storage + input_offset;
                float*   output = output_storage + output_offset;
                input[-1] = 12345.0f;
                if (footer)
                    input[inputs] = 12345.0f;
                for (size_t j = 0; j < inputs; ++j)
                    input[j] = (float)((int32_t)(j * 37 % 4096) - 2048) / 8;
                for (uint32_t k = 0; k < count; ++k)
                {
                    for (size_t j = 0; j < output_offset + outputs + footer; ++j)
                        output_storage[j] = -12345.0f;
                    kernels[k](input, sw, sh, output, dw, dh);
                    CheckReference(input, sw, sh, output, dw, dh);
                    ASSERT_EQ(-12345.0f, output[-1]);
                    ASSERT_EQ(12345.0f, input[-1]);
                    for (size_t j = 0; j < inputs; ++j)
                        ASSERT_EQ((float)((int32_t)(j * 37 % 4096) - 2048) / 8, input[j]);
                    if (footer)
                    {
                        ASSERT_EQ(-12345.0f, output[outputs]);
                        ASSERT_EQ(12345.0f, input[inputs]);
                    }
                }
                free(input_storage);
                free(output_storage);
            }
}

// Special values retain the old float operation graph, including overflow and NaN from zero-weight infinities.
TEST(TexcResizeHDR, SpecialValues)
{
    const float                   values[] = { 0.0f, -0.0f, FLT_MIN, -FLT_MIN, FLT_MAX, -FLT_MAX, INFINITY, -INFINITY, NAN };
    dmTexc::ResizeRGBA32FPixelsFn kernels[8];
    uint32_t                      count = GetKernels(kernels);
    for (uint32_t a = 0; a < sizeof(values) / sizeof(values[0]); ++a)
        for (uint32_t b = 0; b < sizeof(values) / sizeof(values[0]); ++b)
            for (uint32_t dw = 1; dw <= 3; ++dw)
            {
                float input[] = { values[a], values[b], values[a], values[b], values[b], values[a], values[b], values[a] };
                for (uint32_t k = 0; k < count; ++k)
                {
                    float output[12];
                    kernels[k](input, 2, 1, output, dw, 1);
                    for (uint32_t x = 0; x < dw; ++x)
                        for (uint32_t c = 0; c < 4; ++c)
                        {
                            float px = (x + 0.5f) * (2.0f / dw) - 0.5f;
                            px = px < 0.0f ? 0.0f : px;
                            uint32_t x0 = (uint32_t)px, x1 = x0 + 1 < 2 ? x0 + 1 : 1;
                            float    top = input[x0 * 4 + c] + (input[x1 * 4 + c] - input[x0 * 4 + c]) * (px - x0);
                            float    expected = top + (top - top) * 0.0f;
                            float    actual = output[x * 4 + c];
                            if (isnan(expected))
                                ASSERT_TRUE(isnan(actual));
                            else
                            {
                                ASSERT_EQ(expected, actual);
                                ASSERT_EQ(!!signbit(expected), !!signbit(actual));
                            }
                        }
                }
            }
}

// Public HDR resizing keeps format, color space, path, allocation ownership and invalid-destination behavior.
TEST(TexcResizeHDR, ImageMetadataAndDispatch)
{
    float         color[] = { -3, 5, 100, 0.5f };
    dmTexc::Image image;
    memset(&image, 0, sizeof(image));
    image.m_Path = "hdr-test";
    image.m_Data = (uint8_t*)color;
    image.m_DataCount = sizeof(color);
    image.m_PixelFormat = dmTexc::PF_RGBA32F;
    image.m_Width = image.m_Height = 1;
    for (uint32_t space = 0; space < 2; ++space)
    {
        image.m_ColorSpace = (dmTexc::ColorSpace)space;
        dmTexc::Image* output = dmTexc::Resize(&image, 7, 9);
        ASSERT_NE((dmTexc::Image*)0, output);
        ASSERT_EQ(7u, output->m_Width);
        ASSERT_EQ(9u, output->m_Height);
        ASSERT_EQ(7u * 9u * sizeof(color), output->m_DataCount);
        ASSERT_EQ(dmTexc::PF_RGBA32F, output->m_PixelFormat);
        ASSERT_EQ(space, (uint32_t)output->m_ColorSpace);
        ASSERT_EQ(0, strcmp("hdr-test", output->m_Path));
        ASSERT_NE(image.m_Data, output->m_Data);
        ASSERT_NE(image.m_Path, output->m_Path);
        for (uint32_t i = 0; i < 7 * 9; ++i)
            ASSERT_EQ(0, memcmp(color, output->m_Data + i * sizeof(color), sizeof(color)));
        dmTexc::DestroyImage(output);
    }
    ASSERT_EQ((dmTexc::Image*)0, dmTexc::ResizeRGBA32F(&image, 0, 1));
    ASSERT_EQ((dmTexc::Image*)0, dmTexc::ResizeRGBA32F(&image, 1, 0));
    ASSERT_EQ((dmTexc::Image*)0, dmTexc::ResizeRGBA32F(&image, 65536, 65536));
}

#if defined(DM_TEXC_TEST_WIDE)
// Wide vertical accumulation must match scalar tap ordering for reductions, enlargements and row tails.
TEST(TexcWide, ResizeMatchesScalar)
{
    const uint32_t shapes[][4] = {
        { 1, 1, 7, 9 }, { 2, 2, 4, 4 }, { 3, 5, 11, 7 }, { 11, 7, 3, 5 }, { 1, 37, 13, 3 }, { 37, 1, 3, 13 }, { 19, 31, 7, 41 }, { 19, 31, 41, 7 }, { 63, 65, 32, 33 }, { 64, 64, 32, 32 }, { 7, 9, 7, 9 }, { 71, 53, 97, 83 }
    };
    for (uint32_t shape = 0; shape < sizeof(shapes) / sizeof(shapes[0]); ++shape)
    {
        uint32_t sw = shapes[shape][0], sh = shapes[shape][1], dw = shapes[shape][2], dh = shapes[shape][3];
        size_t   input_bytes = (size_t)sw * sh * 4, output_bytes = (size_t)dw * dh * 4;
        uint8_t* input = (uint8_t*)malloc(input_bytes);
        uint8_t* expected = (uint8_t*)malloc(output_bytes);
        uint8_t* output = (uint8_t*)malloc(output_bytes);
        for (uint32_t pattern = 0; pattern < 2; ++pattern)
        {
            for (size_t i = 0; i < input_bytes; ++i)
                input[i] = pattern ? (uint8_t)(i * 37 + shape * 11) : (uint8_t)((i % 4) * 61);
            ASSERT_TRUE(dmTexc::Reference::ResizeRGBA8(input, sw, sh, expected, dw, dh));
            ASSERT_TRUE(dmTexc::Wide::ResizeRGBA8(input, sw, sh, output, dw, dh));
            ASSERT_EQ(0, memcmp(expected, output, output_bytes));
            if (!pattern)
                for (size_t i = 0; i < output_bytes; ++i)
                    ASSERT_EQ((uint8_t)((i % 4) * 61), output[i]);
        }
        free(input);
        free(expected);
        free(output);
    }
    ASSERT_FALSE(dmTexc::Wide::ResizeRGBA8(0, 0, 1, 0, 1, 1));
}
#endif

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
