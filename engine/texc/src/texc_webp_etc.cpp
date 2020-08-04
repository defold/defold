// Copyright 2020 The Defold Foundation
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
