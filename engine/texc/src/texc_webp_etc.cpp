#include <stdint.h>

namespace dmTexc
{
    static inline void ETCDecomposeBlock(uint64_t data, uint32_t& base_colors, uint32_t& pixel_indices)
    {
        pixel_indices = data >> 32;
        base_colors = data & 0xffffffff;
    }

    void ETCDecomposeBlocks(const uint64_t* data, const uint32_t width, const uint32_t height, uint32_t* base_colors, uint32_t* pixel_indices)
    {
        for(uint32_t y = 0; y < height; y ++)
        {
            uint32_t yi = y*width;
            for(uint32_t x = 0; x < width; x ++)
            {
                uint32_t li = yi+x;
                ETCDecomposeBlock(data[y*width+x], base_colors[li], pixel_indices[li]);
            }
        }
    }
}
