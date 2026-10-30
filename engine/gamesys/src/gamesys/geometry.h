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

#ifndef DM_GAMESYS_GEOMETRY_H
#define DM_GAMESYS_GEOMETRY_H

#include <stdint.h>

namespace dmGameSystem
{
    // Four rows and columns, with two triangles for each of the nine cells.
    const uint32_t SLICE9_VERTEX_COUNT = 16;
    const uint32_t SLICE9_INDEX_COUNT  = 54;

    struct Slice9Params
    {
        float   m_Slice9[4]; // Left, top, right, bottom margins in pixels.
        float   m_Width;
        float   m_Height;
        float   m_PivotX;    // Normalized origin: 0 = left/bottom, 0.5 = center.
        float   m_PivotY;
        uint8_t m_FlipHorizontal : 1;
        uint8_t m_FlipVertical : 1;
    };

    /**
     * Generate slice-9 vertex positions, normalized to the box size with the pivot subtracted.
     * Flips exchange the fixed-size borders. Margins are not clamped to the size.
     * Dimensions <= 0.001 disable the inner subdivisions along that axis.
     *
     * @param params Box dimensions, margins, pivot and flip flags
     * @param positions Output buffer of SLICE9_VERTEX_COUNT interleaved XY pairs in row-major order, starting at the bottom left
     */
    void GenerateSlice9Positions(const Slice9Params& params, float positions[SLICE9_VERTEX_COUNT * 2]);

    /**
     * Generate slice-9 texture coordinates matching the vertex positions.
     * Handles rotated atlas regions and both flips.
     *
     * @param params Slice-9 margins and flip flags
     * @param texture_width Original atlas width in pixels, before texture resizing
     * @param texture_height Original atlas height in pixels, before texture resizing
     * @param tex_coords Unflipped TextureSet UV pairs in bottom-left, top-left, top-right, bottom-right order
     * @param uvs Output buffer of SLICE9_VERTEX_COUNT interleaved UV pairs in the same order as GenerateSlice9Positions
     */
    void GenerateSlice9TexCoords(const Slice9Params& params, float texture_width, float texture_height, const float tex_coords[8], float uvs[SLICE9_VERTEX_COUNT * 2]);

    /**
     * Get indices for two counterclockwise triangles per slice-9 cell, sharing the bottom-left to top-right diagonal.
     * The indices refer to the row-major grid produced by GenerateSlice9Positions.
     *
     * @return Buffer of SLICE9_INDEX_COUNT indices with static lifetime
     */
    const uint16_t* GetSlice9Indices();
}

#endif // DM_GAMESYS_GEOMETRY_H
