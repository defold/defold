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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <slug/slug.hpp>
#include <slug/slugc.h>
#include <string.h>

// The shared path preparer computes cubic extrema and emits encodable curves
// without font types, preserving the approximation used by fonts and SVGs.
TEST(Slug, CubicPath)
{
    dmSlug::OutlineCommand input[] = {
        { dmSlug::OUTLINE_MOVE_TO, { { 0, 0 } } },
        { dmSlug::OUTLINE_CUBIC_TO, { { 0, 100 }, { 100, 100 }, { 100, 0 } } },
        { dmSlug::OUTLINE_CLOSE, {} }
    };
    dmSlug::Outline outline = { input, 3 };
    float           x0, y0, x1, y1;
    ASSERT_TRUE(dmSlug::GetOutlineBounds(&outline, &x0, &y0, &x1, &y1));
    ASSERT_EQ(0.0f, x0);
    ASSERT_EQ(0.0f, y0);
    ASSERT_EQ(100.0f, x1);
    ASSERT_EQ(75.0f, y1);
    dmArray<dmSlug::CurveCommand> curves;
    ASSERT_TRUE(dmSlug::OutlineToQuadratics(&outline, 1, 0.001f, &curves));
    dmSlug::Data data;
    dmSlug::Path path;
    ASSERT_TRUE(dmSlug::AddPath(&data, curves.Begin(), curves.Size(), 100, 8, &path));
    ASSERT_TRUE(path.m_CurveCount > 1);
    ASSERT_TRUE(data.m_Curves.Size() > 0);
    ASSERT_TRUE(data.m_Bands.Size() > 0);
}

// The C tool bridge produces deterministic, owned numeric textures independently
// of fontc. Freeing its output clears all pointers and byte counts.
TEST(Slug, CompileAtlas)
{
    const float commands[] = { 0, 0, 0, 0, 0, 0, 0, 1, 100, 0, 0, 0, 0, 0, 1, 100, 80, 0, 0, 0, 0, 1, 0, 80, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0 };
    const float stops[] = { 0, 0.2f, 0.4f, 0.6f, 0.5f };
    SlugPath    path = {};
    path.m_Commands = commands;
    path.m_CommandCount = 5;
    path.m_Stops = stops;
    path.m_StopCount = 1;
    path.m_PaintTransform[0] = path.m_PaintTransform[3] = 1;
    SlugImage   image = { 100, 80, &path, 1 };
    SlugTexture a[3] = {}, b[3] = {};
    uint32_t    paint_a, paint_b;
    ASSERT_EQ(SLUG_RESULT_OK, SlugCompileAtlas(&image, 1, &a[0], &a[1], &a[2], &paint_a));
    ASSERT_EQ(SLUG_RESULT_OK, SlugCompileAtlas(&image, 1, &b[0], &b[1], &b[2], &paint_b));
    ASSERT_EQ(paint_a, paint_b);
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(4096u, a[i].m_Width);
        ASSERT_TRUE(a[i].m_PixelCount > 0);
        ASSERT_EQ(a[i].m_PixelCount, b[i].m_PixelCount);
        ASSERT_EQ(0, memcmp(a[i].m_Pixels, b[i].m_Pixels, a[i].m_PixelCount));
        SlugFreeTexture(&a[i]);
        SlugFreeTexture(&b[i]);
        ASSERT_EQ((uint8_t*)0, a[i].m_Pixels);
        ASSERT_EQ(0u, a[i].m_PixelCount);
    }
}

// Capacity failures leave no partial output allocation in the native bridge.
TEST(Slug, CapacityFailure)
{
    const float command[7] = {};
    const float stop[5] = {};
    SlugPath    path = {};
    path.m_Commands = command;
    path.m_CommandCount = 4097;
    path.m_Stops = stop;
    path.m_StopCount = 1;
    SlugImage   image = { 100, 80, &path, 1 };
    SlugTexture textures[3] = {};
    uint32_t    paint;
    ASSERT_EQ(SLUG_RESULT_CAPACITY, SlugCompileAtlas(&image, 1, &textures[0], &textures[1], &textures[2], &paint));
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ((uint8_t*)0, textures[i].m_Pixels);
        ASSERT_EQ(0u, textures[i].m_PixelCount);
        SlugFreeTexture(&textures[i]);
    }
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
