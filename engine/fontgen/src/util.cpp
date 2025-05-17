// Copyright 2020-2025 The Defold Foundation
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

#include "util.h"
#include <stdio.h>

namespace dmFontGen
{

void DebugPrintBitmap(uint8_t* bitmap, int w, int h)
{
    printf("--------------------------------------------\n");
    for (int j=0; j < h; ++j)
    {
        for (int i=0; i < w; ++i)
        {
            putchar(" .:ioVM@"[bitmap[j*w+i]>>5]);
        }
        putchar('\n');
    }
    printf("--------------------------------------------\n");
}

// From atlaspacker.c: apCopyRGBA

// Copy the source image into the target image
// Can handle cases where the target texel is outside of the destination
// Transparent source texels are ignored
void CopyRGBA(uint8_t* dest, int dest_width, int dest_height, int dest_channels,
                const uint8_t* source, int source_width, int source_height, int source_channels,
                int dest_x, int dest_y, int rotation)
{
    for (int sy = 0; sy < source_height; ++sy)
    {
        for (int sx = 0; sx < source_width; ++sx, source += source_channels)
        {
            // Skip copying texels with alpha=0
            if (source_channels == 4 && source[3] == 0)
                continue;

            // Map the current coord into the rotated space
            //apPos rotated_pos = apRotate(sx, sy, source_width, source_height, rotation);

            int target_x = dest_x + sx;//rotated_pos.x;
            int target_y = dest_y + sy;//rotated_pos.y;

            // If the target is outside of the destination area, we skip it
            if (target_x < 0 || target_x >= dest_width ||
                target_y < 0 || target_y >= dest_height)
                continue;

            int dest_index = target_y * dest_width * dest_channels + target_x * dest_channels;

            uint8_t color[4] = {255,255,255,255};
            for (int c = 0; c < source_channels; ++c)
                color[c] = source[c];

            int alphathreshold = 8;
            if (alphathreshold >= 0 && color[3] <= alphathreshold)
                continue; // Skip texels that are <= the alpha threshold

            for (int c = 0; c < dest_channels; ++c)
                dest[dest_index+c] = color[c];

            // if (color[3] > 0 && color[3] < 255)
            // {
            //     uint32_t r = dest[dest_index+0] + 48;
            //     dest[dest_index+0] = (uint8_t)(r > 255 ? 255 : r);
            //     dest[dest_index+1] = dest[dest_index+1] / 2;
            //     dest[dest_index+2] = dest[dest_index+2] / 2;

            //     uint32_t a = dest[dest_index+3] + 128;
            //     dest[dest_index+3] = (uint8_t)(a > 255 ? 255 : a);
            // }
        }
    }
}

} // namespace
