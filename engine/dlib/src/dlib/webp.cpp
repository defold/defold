#include <assert.h>
#include "webp/webp/decode.h"
#include "webp.h"
#include <dlib/log.h>

namespace dmWebP
{
    Result DecodeRGB(const void* data, size_t data_size, void* output_buffer, size_t output_buffer_size, size_t output_stride)
    {
        uint8_t* decoded_data = WebPDecodeRGBInto((const uint8_t*) data, data_size, (uint8_t*) output_buffer, output_buffer_size, output_stride);
        if(decoded_data != output_buffer)
            return RESULT_MEM_ERROR;
        return RESULT_OK;
    }

    Result DecodeRGBA(const void* data, size_t data_size, void* output_buffer, size_t output_buffer_size, size_t output_stride)
    {
        uint8_t* decoded_data = WebPDecodeRGBAInto((const uint8_t*) data, data_size, (uint8_t*) output_buffer, output_buffer_size, output_stride);
        if(decoded_data != output_buffer)
            return RESULT_MEM_ERROR;
        return RESULT_OK;
    }


    // Compose 32-bit colora RGBA, colorb RGBA and 32-bit modulation buffers into PVRTC1 64-bit blocks.
    // Data is transformed from linear bitmap space to morton order (PVRTC1), ref: http://graphics.stanford.edu/~seander/bithacks.html#InterleaveBMN
    static void PVRTComposeBlocks(uint64_t* data, const uint32_t width, const uint32_t height, const uint32_t* color_a, const uint32_t* color_b, const uint32_t* modulation)
    {
        for(uint32_t y = 0; y < height; ++y)
        {
            // morton order y (max 0xffff).
            uint32_t my = (y | (y << 8)) & 0x00FF00FF;
            my = (my | (my << 4)) & 0x0F0F0F0F;
            my = (my | (my << 2)) & 0x33333333;
            my = (my | (my << 1)) & 0x55555555;
            const uint32_t yi = y*width;

            for(uint32_t x = 0; x < width; ++x)
            {
                const uint32_t li = yi+x;
                uint64_t color;
                uint32_t c = color_a[li];
                if((c & 0xf0000000) == 0xf0000000)
                {
                    // RGB 888 -> RGB 555. MSB is color mode
                    color = ((c & 0xff) << 7) | ((c & 0xff00) >> 6) | ((c & 0xff0000) >> 19) | 0x8000;
                }
                else
                {
                    // ARGB 4444. MSB is color mode
                    color = ((c & 0xff) << 4) | ((c & 0xff00) >> 8) | ((c & 0xff0000) >> 20) | ((c & 0xff000000) >> 17);
                }
                c = color_b[li];
                if((c & 0xf0000000) == 0xf0000000)
                {
                    // RGB 554 (Blue is 3 + 1 Modulation mode). MSB is color mode
                    color |= ((c & 0xff) << 23) | ((c & 0xff00) << 10) | ((c & 0xff0000) >> 3) | 0x80000000;
                }
                else
                {
                    // ARGB 4443 (Blue is 3 + 1 Modulation mode). MSB is color mode
                    color |= ((c & 0xff) << 20) | ((c & 0xff00) << 8) | ((c & 0xff0000) >> 4) | ((c & 0xff000000) >> 1);
                }

                // morton order x (max 0xffff)
                uint32_t mx = (x | (x << 8)) & 0x00FF00FF;
                mx = (mx | (mx << 4)) & 0x0F0F0F0F;
                mx = (mx | (mx << 2)) & 0x33333333;
                mx = (mx | (mx << 1)) & 0x55555555;
                const uint32_t mi = my | (mx << 1);

                data[mi] = modulation[li] | (color << 32);
            }
        }
    }

    static void ETCComposeBlocks(uint64_t* data, const uint32_t width, const uint32_t height, const uint32_t* base_colors, const uint32_t* pixel_indices)
    {
        for(uint32_t y = 0; y < height; ++y)
        {
            uint32_t yi = y*width;
            for(uint32_t x = 0; x < width; ++x)
            {
                const uint32_t li = yi+x;
                data[li] = base_colors[li] | ((uint64_t)pixel_indices[li] << 32);
            }
        }
    }

    Result DecodeCompressedTexture(const void* data, size_t data_size, void* output_buffer, size_t output_buffer_size, size_t output_stride, TextureCompression texture_compression)
    {
        uint32_t uncompressed_page_count;
        switch(texture_compression)
        {
            case TEXTURE_COMPRESSION_PVRTC1:
                uncompressed_page_count = 3;
            break;

            case TEXTURE_COMPRESSION_ETC1:
                uncompressed_page_count = 2;
            break;

            default:
                return RESULT_TEXTURE_COMPRESSION_ERROR;
            break;
        }

        const uint32_t uncompressed_page_size = output_buffer_size>>1;
        uint8_t* decomposed_data = new uint8_t[uncompressed_page_size*uncompressed_page_count];
        if(decomposed_data == 0x0)
        {
            return dmWebP::RESULT_MEM_ERROR;
        }
        uint8_t* decoded_data = WebPDecodeRGBAInto((const uint8_t*) data, data_size, decomposed_data, uncompressed_page_size*uncompressed_page_count, output_stride<<1);
        if(decoded_data != (uint8_t*) decomposed_data)
        {
            delete[] decomposed_data;
            return RESULT_MEM_ERROR;
        }

        const uint32_t height = (output_buffer_size/output_stride)>>2;
        const uint32_t width = (output_buffer_size/height)>>3;

        dmWebP::Result res = RESULT_OK;
        switch(texture_compression)
        {
            case TEXTURE_COMPRESSION_PVRTC1:
                PVRTComposeBlocks((uint64_t*) output_buffer, width, height, (uint32_t*)decomposed_data, (uint32_t*)(decomposed_data+uncompressed_page_size), (uint32_t*)(decomposed_data+(uncompressed_page_size<<1)));
            break;

            case TEXTURE_COMPRESSION_ETC1:
                ETCComposeBlocks((uint64_t*) output_buffer, width, height, (uint32_t*)decomposed_data, (uint32_t*)(decomposed_data+uncompressed_page_size));
            break;

            default:
                res = RESULT_TEXTURE_COMPRESSION_ERROR;
            break;
        }

        delete[] decomposed_data;
        return res;
    }

}
