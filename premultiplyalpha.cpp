
#include <stdint.h>

#if defined(__SSE4_2__)
#include <immintrin.h>
#endif

void ConvertPremultiplyAndFlip_ABGR8888ToRGBA8888(const uint8_t* input_data, uint8_t* output_data, const uint32_t width, const uint32_t height)
{
    for (uint32_t y = 0; y < height; ++y)
    {
        uint32_t flipped_y = height - y - 1;
        const uint8_t* src = input_data + (y * width * 4);
        uint8_t* dst = output_data + (flipped_y * width * 4);

#if defined(__SSE4_2__)
        const __m128i shuffle_rgba = _mm_setr_epi8(6, 7, 4, 5, 2, 3, 0, 1, 14, 15, 12, 13, 10, 11, 8, 9);
        const __m128i one = _mm_set1_epi16(1);
        const __m128i zero = _mm_setzero_si128();

        uint32_t x = 0;
        const uint32_t simd_width = width & ~1U;
        const uint8_t* src_ptr = src;
        uint8_t* dst_ptr = dst;

        // SIMD: handle two pixels (8 bytes) per iteration.
        for (; x < simd_width; x += 2)
        {
            // Load ABGR bytes and widen to 16-bit lanes for math.
            __m128i pixels = _mm_cvtepu8_epi16(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(src_ptr)));

            // Broadcast alpha for each pixel into the matching lanes.
            __m128i alpha = _mm_shufflelo_epi16(pixels, _MM_SHUFFLE(0, 0, 0, 0));
            alpha = _mm_shufflehi_epi16(alpha, _MM_SHUFFLE(0, 0, 0, 0));

            // Premultiply channels and divide by 255 using rounding.
            __m128i mul = _mm_mullo_epi16(pixels, alpha);
            __m128i div = _mm_add_epi16(mul, _mm_srli_epi16(mul, 8));
            div = _mm_add_epi16(div, one);
            __m128i premult = _mm_srli_epi16(div, 8);

            // Preserve alpha, shuffle from ABGR to RGBA, and pack back to bytes.
            premult = _mm_blend_epi16(premult, pixels, 0x11);
            premult = _mm_shuffle_epi8(premult, shuffle_rgba);

            __m128i packed = _mm_packus_epi16(premult, zero);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(dst_ptr), packed);

            src_ptr += 8;
            dst_ptr += 8;
        }

        // Tail: process a single remaining pixel if the width is odd.
        for (; x < width; ++x)
        {
            uint8_t a = *(src_ptr++);
            uint8_t b = *(src_ptr++);
            uint8_t g = *(src_ptr++);
            uint8_t r = *(src_ptr++);

            *(dst_ptr++) = (uint8_t)((r * a) / 255);
            *(dst_ptr++) = (uint8_t)((g * a) / 255);
            *(dst_ptr++) = (uint8_t)((b * a) / 255);
            *(dst_ptr++) = a;
        }
#else
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
#endif
    }
}
