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
