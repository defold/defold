#pragma once

#include <stdint.h>

namespace dmFontGen
{
    /*
     * Outputs a w*h single channel bitmap to stdout
     */
    void DebugPrintBitmap(uint8_t* bitmap, int w, int h);

    // Copy the source image into the target image
    // Can handle cases where the target texel is outside of the destination
    // Transparent source texels are ignored
    void CopyRGBA(uint8_t* dest, int dest_width, int dest_height, int dest_channels,
                            const uint8_t* source, int source_width, int source_height, int source_channels,
                            int dest_x, int dest_y, int rotation);

}
