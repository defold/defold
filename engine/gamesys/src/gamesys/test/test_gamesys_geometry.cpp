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

#include "../geometry.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

using namespace dmGameSystem;

static const float EPSILON = 0.00001f;

// Verifies fractional dimensions, pivot offsets, and all asymmetric border flips.
TEST(Slice9Geometry, Positions)
{
    Slice9Params params = {};
    params.m_Width = 32.5f;
    params.m_Height = 24.25f;
    params.m_Slice9[0] = 2;
    params.m_Slice9[1] = 3;
    params.m_Slice9[2] = 4;
    params.m_Slice9[3] = 5;
    params.m_PivotX = 0.75f;
    params.m_PivotY = 0.25f;

    const float expected_x[2][4] = {
        {-24.375f, -22.375f, 4.125f, 8.125f},
        {-24.375f, -20.375f, 6.125f, 8.125f}
    };

    const float expected_y[2][4] = {
        {-6.0625f, -1.0625f, 15.1875f, 18.1875f},
        {-6.0625f, -3.0625f, 13.1875f, 18.1875f}
    };

    for (uint32_t flip = 0; flip < 4; ++flip)
    {
        params.m_FlipHorizontal = (flip & 1) != 0;
        params.m_FlipVertical = (flip & 2) != 0;

        float positions[SLICE9_VERTEX_COUNT * 2];
        GenerateSlice9Positions(params, positions);

        for (uint32_t i = 0; i < SLICE9_VERTEX_COUNT; ++i)
        {
            ASSERT_NEAR(expected_x[params.m_FlipHorizontal][i % 4], positions[i * 2] * params.m_Width, EPSILON);
            ASSERT_NEAR(expected_y[params.m_FlipVertical][i / 4], positions[i * 2 + 1] * params.m_Height, EPSILON);
        }
    }
}

// Verifies atlas pixel margins and logical flips for both packed orientations.
TEST(Slice9Geometry, TexCoords)
{
    Slice9Params params = {};
    params.m_Width = 32;
    params.m_Height = 24;
    params.m_Slice9[0] = 2;
    params.m_Slice9[1] = 3;
    params.m_Slice9[2] = 4;
    params.m_Slice9[3] = 5;
    params.m_PivotX = 0.5f;
    params.m_PivotY = 0.5f;

    const float tex_coords[2][8] = {
        {0.125f, 0.25f, 0.125f, 0.75f, 0.625f, 0.75f, 0.625f, 0.25f},
        {0.125f, 0.75f, 0.625f, 0.75f, 0.625f, 0.25f, 0.125f, 0.25f}
    };

    const float expected_u[2][4] = {
        {0.125f, 0.1328125f, 0.609375f, 0.625f},
        {0.125f, 0.14453125f, 0.61328125f, 0.625f}
    };

    const float expected_v[2][4] = {
        {0.25f, 0.2890625f, 0.7265625f, 0.75f},
        {0.75f, 0.734375f, 0.28125f, 0.25f}
    };

    for (uint32_t rotated = 0; rotated < 2; ++rotated)
    {
        for (uint32_t flip = 0; flip < 4; ++flip)
        {
            params.m_FlipHorizontal = (flip & 1) != 0;
            params.m_FlipVertical = (flip & 2) != 0;

            float uvs[SLICE9_VERTEX_COUNT * 2];
            GenerateSlice9TexCoords(params, 256, 128, tex_coords[rotated], uvs);

            for (uint32_t i = 0; i < SLICE9_VERTEX_COUNT; ++i)
            {
                const uint32_t x = params.m_FlipHorizontal ? 3 - i % 4 : i % 4;
                const uint32_t y = params.m_FlipVertical ? 3 - i / 4 : i / 4;
                ASSERT_NEAR(expected_u[rotated][rotated ? y : x], uvs[i * 2], EPSILON);
                ASSERT_NEAR(expected_v[rotated][rotated ? x : y], uvs[i * 2 + 1], EPSILON);
            }
        }
    }
}

// Verifies the untextured UV fallback uses bottom/top margins and honors flips.
TEST(Slice9Geometry, UnitTexture)
{
    Slice9Params params = {};
    params.m_Width = 32;
    params.m_Height = 16;
    params.m_Slice9[0] = 2;
    params.m_Slice9[1] = 3;
    params.m_Slice9[2] = 4;
    params.m_Slice9[3] = 5;

    const float tc[8] = {0, 0, 0, 1, 1, 1, 1, 0};
    const float expected_u[4] = {0, 0.0625f, 0.875f, 1};
    const float expected_v[4] = {0, 0.3125f, 0.8125f, 1};

    for (uint32_t flip = 0; flip < 4; ++flip)
    {
        params.m_FlipHorizontal = (flip & 1) != 0;
        params.m_FlipVertical = (flip & 2) != 0;

        float uvs[SLICE9_VERTEX_COUNT * 2];
        GenerateSlice9TexCoords(params, 32, 16, tc, uvs);

        for (uint32_t i = 0; i < SLICE9_VERTEX_COUNT; ++i)
        {
            ASSERT_NEAR(expected_u[params.m_FlipHorizontal ? 3 - i % 4 : i % 4], uvs[i * 2], EPSILON);
            ASSERT_NEAR(expected_v[params.m_FlipVertical ? 3 - i / 4 : i / 4], uvs[i * 2 + 1], EPSILON);
        }
    }
}

// Verifies zero and threshold dimensions collapse inner subdivisions without division by zero.
TEST(Slice9Geometry, SmallDimensions)
{
    const float dimensions[] = {0, 0.0005f, 0.001f};
    const float expected[4] = {0, 0, 1, 1};
    const float tc[8] = {0, 0, 0, 1, 1, 1, 1, 0};

    for (uint32_t d = 0; d < sizeof(dimensions) / sizeof(dimensions[0]); ++d)
    {
        Slice9Params params = {};
        params.m_Width = dimensions[d];
        params.m_Height = dimensions[d];
        params.m_Slice9[0] = 2;
        params.m_Slice9[1] = 3;
        params.m_Slice9[2] = 4;
        params.m_Slice9[3] = 5;

        float positions[SLICE9_VERTEX_COUNT * 2];
        float uvs[SLICE9_VERTEX_COUNT * 2];
        GenerateSlice9Positions(params, positions);
        GenerateSlice9TexCoords(params, params.m_Width, params.m_Height, tc, uvs);

        for (uint32_t i = 0; i < SLICE9_VERTEX_COUNT; ++i)
        {
            ASSERT_EQ(expected[i % 4], positions[i * 2]);
            ASSERT_EQ(expected[i / 4], positions[i * 2 + 1]);
            ASSERT_EQ(expected[i % 4], uvs[i * 2]);
            ASSERT_EQ(expected[i / 4], uvs[i * 2 + 1]);
        }
    }
}

// Verifies dimensions above the threshold retain their fractional subdivisions.
TEST(Slice9Geometry, AboveSmallDimensionThreshold)
{
    Slice9Params params = {};
    params.m_Width = 0.002f;
    params.m_Height = 0.002f;
    params.m_Slice9[0] = 0.0005f;
    params.m_Slice9[1] = 0.0005f;
    params.m_Slice9[2] = 0.0005f;
    params.m_Slice9[3] = 0.0005f;
    float positions[SLICE9_VERTEX_COUNT * 2];
    GenerateSlice9Positions(params, positions);
    ASSERT_NEAR(0.25f, positions[2], EPSILON);
    ASSERT_NEAR(0.75f, positions[4], EPSILON);
    ASSERT_NEAR(0.25f, positions[9], EPSILON);
    ASSERT_NEAR(0.75f, positions[17], EPSILON);
}

// Verifies oversized margins remain unclamped and zero margins retain the fixed grid.
TEST(Slice9Geometry, DegenerateAndOverlappingCells)
{
    Slice9Params params[2] = {};
    params[0].m_Width = params[1].m_Width = 8;
    params[0].m_Height = params[1].m_Height = 8;
    params[1].m_Slice9[0] = 12;
    params[1].m_Slice9[1] = 10;
    params[1].m_Slice9[2] = 10;
    params[1].m_Slice9[3] = 12;

    const float expected[2][4] = {{0, 0, 1, 1}, {0, 1.5f, -0.25f, 1}};
    for (uint32_t p = 0; p < 2; ++p)
    {
        float positions[SLICE9_VERTEX_COUNT * 2];
        GenerateSlice9Positions(params[p], positions);

        for (uint32_t i = 0; i < SLICE9_VERTEX_COUNT; ++i)
        {
            ASSERT_EQ(expected[p][i % 4], positions[i * 2]);
            ASSERT_EQ(expected[p][i / 4], positions[i * 2 + 1]);
        }
    }
}

// Verifies all nine cells use the same diagonal and counterclockwise winding.
TEST(Slice9Geometry, Indices)
{
    const uint16_t* indices = GetSlice9Indices();
    for (uint32_t cell = 0; cell < 9; ++cell)
    {
        const uint32_t bottom_left = (cell / 3) * 4 + cell % 3;
        const uint16_t* triangles = indices + cell * 6;

        ASSERT_EQ(bottom_left, triangles[0]);
        ASSERT_EQ(bottom_left + 1, triangles[1]);
        ASSERT_EQ(bottom_left + 5, triangles[2]);
        ASSERT_EQ(bottom_left, triangles[3]);
        ASSERT_EQ(bottom_left + 5, triangles[4]);
        ASSERT_EQ(bottom_left + 4, triangles[5]);

        for (uint32_t t = 0; t < 6; t += 3)
        {
            const int ax = triangles[t] % 4, ay = triangles[t] / 4;
            const int bx = triangles[t + 1] % 4, by = triangles[t + 1] / 4;
            const int cx = triangles[t + 2] % 4, cy = triangles[t + 2] / 4;
            ASSERT_GT((bx - ax) * (cy - ay) - (by - ay) * (cx - ax), 0);
        }
    }
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
