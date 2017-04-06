#include <stdint.h>

namespace dmTexc
{
    static inline void PVRTCDecomposeBlock(uint64_t data, uint32_t& color_a_rgba, uint32_t& color_b_rgba, uint32_t& modulation)
    {
        modulation = data & 0xffffffff;
        uint32_t c = data >> 32;
        if ((c & 0x8000) != 0)
        {
            // Opaque c Mode - RGB 555 -> RGB 888. Opaque bit is alpha LSB
            color_a_rgba = 0xf0000000 | ((c & 0x7c00) >> 7) | ((c & 0x3e0)  << 6) | ((c & 0x1f) << 19);
        }
        else
        {
            // Transparent c Mode - ARGB 3444 -> RGBA 8888
            color_a_rgba = ((c & 0x7000) << 17) | ((c & 0xf00) >> 4) | ((c & 0xf0)  << 8) | ((c & 0xf) << 20);
        }

        if (c & 0x80000000)
        {
            // Opaque c Mode - RGB 554 -> RGB 888. Mode bit is blue LSB. Opaque bit is alpha LSB
            color_b_rgba = 0xf0000000 | ((c & 0x7c000000) >> 23) | ((c & 0x3e00000)  >> 10) | ((c & 0x1f0000) << 3);
        }
        else
        {
            // Transparent c Mode - ARGB 3443 -> RGBA 8888. Mode bit is blue LSB
            color_b_rgba = ((c & 0x70000000) << 1) | ((c & 0xf000000) >> 20) | ((c & 0xf00000)  >> 8) | ((c & 0xf0000) << 4);
        }
    }

    void PVRTCDecomposeBlocks(const uint64_t* data, const uint32_t width, const uint32_t height, uint32_t* color_a_rgba, uint32_t* color_b_rgba, uint32_t* modulation)
    {
        for(uint32_t y = 0; y < height; y ++)
        {
            // morton order y (max 0xffff). Ref: http://graphics.stanford.edu/~seander/bithacks.html#InterleaveBMN
            uint32_t my = (y | (y << 8)) & 0x00FF00FF;
            my = (my | (my << 4)) & 0x0F0F0F0F;
            my = (my | (my << 2)) & 0x33333333;
            my = (my | (my << 1)) & 0x55555555;
            const uint32_t yi = y*width;

            for(uint32_t x = 0; x < width; x ++)
            {
                // morton order x (max 0xffff)
                uint32_t mx = (x | (x << 8)) & 0x00FF00FF;
                mx = (mx | (mx << 4)) & 0x0F0F0F0F;
                mx = (mx | (mx << 2)) & 0x33333333;
                mx = (mx | (mx << 1)) & 0x55555555;
                const uint32_t mi = my | (mx << 1);
                const uint32_t li = yi+x;

                PVRTCDecomposeBlock(data[mi], color_a_rgba[li], color_b_rgba[li], modulation[li]);
            }
        }
    }
}
