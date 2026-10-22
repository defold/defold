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

#include "geometry.h"

namespace dmGameSystem
{
    static float InverseDimension(float dimension)
    {
        return dimension > 0.001f ? 1.0f / dimension : 0.0f;
    }

    void GenerateSlice9Positions(const Slice9Params& params, float positions[SLICE9_VERTEX_COUNT * 2])
    {
        const float sx = InverseDimension(params.m_Width);
        const float sy = InverseDimension(params.m_Height);
        const float* slice9 = params.m_Slice9;
        const float xs[4] = {0.0f, sx * slice9[params.m_FlipHorizontal ? 2 : 0],
                           1.0f - sx * slice9[params.m_FlipHorizontal ? 0 : 2], 1.0f};
        const float ys[4] = {0.0f, sy * slice9[params.m_FlipVertical ? 1 : 3],
                           1.0f - sy * slice9[params.m_FlipVertical ? 3 : 1], 1.0f};

        for (uint32_t y = 0; y < 4; ++y)
        {
            for (uint32_t x = 0; x < 4; ++x)
            {
                const uint32_t index = (y * 4 + x) * 2;
                positions[index] = xs[x] - params.m_PivotX;
                positions[index + 1] = ys[y] - params.m_PivotY;
            }
        }
    }

    void GenerateSlice9TexCoords(const Slice9Params& params, float texture_width, float texture_height,
                                const float tex_coords[8], float uvs[SLICE9_VERTEX_COUNT * 2])
    {
        const float su = InverseDimension(texture_width);
        const float sv = InverseDimension(texture_height);
        const float* slice9 = params.m_Slice9;
        const float* tc = tex_coords;
        const bool rotated = tc[0] != tc[2] && tc[3] != tc[5];
        static const uint32_t uv_indices[2][4] = {{0, 1, 2, 3}, {3, 2, 1, 0}};
        const uint32_t* ui = uv_indices[rotated ? params.m_FlipVertical : params.m_FlipHorizontal];
        const uint32_t* vi = uv_indices[rotated ? params.m_FlipHorizontal : params.m_FlipVertical];
        float us[4], vs[4];
        if (rotated)
        {
            us[ui[0]] = tc[0];
            us[ui[1]] = tc[0] + su * slice9[3];
            us[ui[2]] = tc[2] - su * slice9[1];
            us[ui[3]] = tc[2];
            vs[vi[0]] = tc[1];
            vs[vi[1]] = tc[1] - sv * slice9[0];
            vs[vi[2]] = tc[5] + sv * slice9[2];
            vs[vi[3]] = tc[5];
        }
        else
        {
            us[ui[0]] = tc[0];
            us[ui[1]] = tc[0] + su * slice9[0];
            us[ui[2]] = tc[4] - su * slice9[2];
            us[ui[3]] = tc[4];
            vs[vi[0]] = tc[1];
            vs[vi[1]] = tc[1] + sv * slice9[3];
            vs[vi[2]] = tc[3] - sv * slice9[1];
            vs[vi[3]] = tc[3];
        }

        for (uint32_t y = 0; y < 4; ++y)
        {
            for (uint32_t x = 0; x < 4; ++x)
            {
                const uint32_t index = (y * 4 + x) * 2;
                uvs[index] = us[rotated ? y : x];
                uvs[index + 1] = vs[rotated ? x : y];
            }
        }
    }

    const uint16_t* GetSlice9Indices()
    {
        static const uint16_t indices[SLICE9_INDEX_COUNT] = {
             0,  1,  5,  0,  5,  4,   1,  2,  6,  1,  6,  5,   2,  3,  7,  2,  7,  6,
             4,  5,  9,  4,  9,  8,   5,  6, 10,  5, 10,  9,   6,  7, 11,  6, 11, 10,
             8,  9, 13,  8, 13, 12,   9, 10, 14,  9, 14, 13,  10, 11, 15, 10, 15, 14,
        };
        return indices;
    }
}
