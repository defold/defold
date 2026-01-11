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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../dlib/image.h"

#include "data/color_check_2x2.png.embed.h"
#include "data/color_check_2x2_premult.png.embed.h"
#include "data/color_check_2x2_indexed.png.embed.h"
#include "data/case2319.jpg.embed.h"
#include "data/color16_check_2x2.png.embed.h"
#include "data/gray_check_2x2.png.embed.h"
#include "data/gray_alpha_check_2x2.png.embed.h"
#include "data/defold_64.jpg.embed.h"
#include "data/defold_64_progressive.jpg.embed.h"

/*
 * Imagemagick conversion
 *
 * Convert 8-bit png to 16-bit (per channel)
 * convert src/test/data/color_check_2x2.png -depth 16 -define png:color-type='2' -define png:bit-depth=16 src/test/color16_check_2x2.png
 */


TEST(dmImage, Empty)
{
    dmImage::Image image;
    dmImage::Result r =  dmImage::Load(0, 0, false, false, &image);
    ASSERT_EQ(dmImage::RESULT_IMAGE_ERROR, r);
}

TEST(dmImage, Corrupt)
{
    uint32_t size = 256 * 256 * 4;
    void* b = malloc(size);
    memset(b, 0, size);
    dmImage::Image image;
    dmImage::Result r =  dmImage::Load(b, size, false, false, &image);
    free(b);
    ASSERT_EQ(dmImage::RESULT_IMAGE_ERROR, r);
}

TEST(dmImage, PngColor)
{
    for (int iter = 0; iter < 2; iter++) {
        dmImage::Image image;
        dmImage::Result r =  dmImage::Load(COLOR_CHECK_2X2_PNG, COLOR_CHECK_2X2_PNG_SIZE, iter == 0, false, &image);
        ASSERT_EQ(dmImage::RESULT_OK, r);
        ASSERT_EQ(2U, image.m_Width);
        ASSERT_EQ(2U, image.m_Height);
        ASSERT_EQ(dmImage::TYPE_RGBA, image.m_Type);
        ASSERT_NE((void*) 0, image.m_Buffer);

        const uint8_t* b = (const uint8_t*) image.m_Buffer;
        int i = 0;
        ASSERT_EQ(255U, (uint32_t) b[i++]);
        ASSERT_EQ(0U, (uint32_t) b[i++]);
        ASSERT_EQ(0U, (uint32_t) b[i++]);
        ASSERT_EQ(255U, (uint32_t) b[i++]);
        ASSERT_EQ(0U, (uint32_t) b[i++]);
        ASSERT_EQ(255U, (uint32_t) b[i++]);
        ASSERT_EQ(0U, (uint32_t) b[i++]);
        ASSERT_EQ(255U, (uint32_t) b[i++]);
        ASSERT_EQ(0U, (uint32_t) b[i++]);
        ASSERT_EQ(0U, (uint32_t) b[i++]);
        ASSERT_EQ(255U, (uint32_t) b[i++]);
        ASSERT_EQ(255U, (uint32_t) b[i++]);
        ASSERT_EQ(0U, (uint32_t) b[i++]);
        ASSERT_EQ(0U, (uint32_t) b[i++]);
        ASSERT_EQ(0U, (uint32_t) b[i++]);
        ASSERT_EQ(127U, (uint32_t) b[i++]);
        dmImage::Free(&image);
    }
}

TEST(dmImage, Premult)
{
    dmImage::Image image;
    dmImage::Result r =  dmImage::Load(COLOR_CHECK_2X2_PREMULT_PNG, COLOR_CHECK_2X2_PREMULT_PNG_SIZE, true, false, &image);
    ASSERT_EQ(dmImage::RESULT_OK, r);
    ASSERT_EQ(2U, image.m_Width);
    ASSERT_EQ(2U, image.m_Height);
    ASSERT_EQ(dmImage::TYPE_RGBA, image.m_Type);
    ASSERT_NE((void*) 0, image.m_Buffer);

    const uint8_t* b = (const uint8_t*) image.m_Buffer;
    int i = 0;
    ASSERT_EQ(76U, (uint32_t) b[i++]);
    ASSERT_EQ(0U, (uint32_t) b[i++]);
    ASSERT_EQ(0U, (uint32_t) b[i++]);
    ASSERT_EQ(76U, (uint32_t) b[i++]);

    ASSERT_EQ(0U, (uint32_t) b[i++]);
    ASSERT_EQ(127U, (uint32_t) b[i++]);
    ASSERT_EQ(0U, (uint32_t) b[i++]);
    ASSERT_EQ(127U, (uint32_t) b[i++]);

    ASSERT_EQ(0U, (uint32_t) b[i++]);
    ASSERT_EQ(1U, (uint32_t) b[i++]);
    ASSERT_EQ(204U, (uint32_t) b[i++]);
    ASSERT_EQ(204U, (uint32_t) b[i++]);

    ASSERT_EQ(0U, (uint32_t) b[i++]);
    ASSERT_EQ(0U, (uint32_t) b[i++]);
    ASSERT_EQ(0U, (uint32_t) b[i++]);
    ASSERT_EQ(0U, (uint32_t) b[i++]);
    dmImage::Free(&image);
}

TEST(dmImage, Indexed)
{
    dmImage::Image image;
    dmImage::Result r =  dmImage::Load(COLOR_CHECK_2X2_INDEXED_PNG, COLOR_CHECK_2X2_INDEXED_PNG_SIZE, true, false, &image);
    ASSERT_EQ(dmImage::RESULT_OK, r);
    ASSERT_EQ(2U, image.m_Width);
    ASSERT_EQ(2U, image.m_Height);
    ASSERT_EQ(dmImage::TYPE_RGB, image.m_Type);
    ASSERT_NE((void*) 0, image.m_Buffer);

    const uint8_t* b = (const uint8_t*) image.m_Buffer;
    int i = 0;
    ASSERT_EQ(255U, (uint32_t) b[i++]);
    ASSERT_EQ(0U, (uint32_t) b[i++]);
    ASSERT_EQ(0U, (uint32_t) b[i++]);

    ASSERT_EQ(0U, (uint32_t) b[i++]);
    ASSERT_EQ(255U, (uint32_t) b[i++]);
    ASSERT_EQ(0U, (uint32_t) b[i++]);

    ASSERT_EQ(0U, (uint32_t) b[i++]);
    ASSERT_EQ(0U, (uint32_t) b[i++]);
    ASSERT_EQ(255U, (uint32_t) b[i++]);

    ASSERT_EQ(255U, (uint32_t) b[i++]);
    ASSERT_EQ(255U, (uint32_t) b[i++]);
    ASSERT_EQ(0U, (uint32_t) b[i++]);
    dmImage::Free(&image);
}

TEST(dmImage, Png16Color)
{
    // // 16-bit images are not supported.
    // dmImage::Image image;
    // dmImage::Result r =  dmImage::Load(COLOR16_CHECK_2X2_PNG, COLOR16_CHECK_2X2_PNG_SIZE, false, &image);
    // ASSERT_EQ(dmImage::RESULT_IMAGE_ERROR, r);
    for (int iter = 0; iter < 2; iter++) {
        dmImage::Image image;
        dmImage::Result r =  dmImage::Load(COLOR_CHECK_2X2_PNG, COLOR_CHECK_2X2_PNG_SIZE, iter == 0, false, &image);
        ASSERT_EQ(dmImage::RESULT_OK, r);
        ASSERT_EQ(2U, image.m_Width);
        ASSERT_EQ(2U, image.m_Height);
        ASSERT_EQ(dmImage::TYPE_RGBA, image.m_Type);
        ASSERT_NE((void*) 0, image.m_Buffer);

        const uint8_t* b = (const uint8_t*) image.m_Buffer;
        int i = 0;
        ASSERT_EQ(255U, (uint32_t) b[i++]);
        ASSERT_EQ(0U, (uint32_t) b[i++]);
        ASSERT_EQ(0U, (uint32_t) b[i++]);
        ASSERT_EQ(255U, (uint32_t) b[i++]);
        ASSERT_EQ(0U, (uint32_t) b[i++]);
        ASSERT_EQ(255U, (uint32_t) b[i++]);
        ASSERT_EQ(0U, (uint32_t) b[i++]);
        ASSERT_EQ(255U, (uint32_t) b[i++]);
        ASSERT_EQ(0U, (uint32_t) b[i++]);
        ASSERT_EQ(0U, (uint32_t) b[i++]);
        ASSERT_EQ(255U, (uint32_t) b[i++]);
        ASSERT_EQ(255U, (uint32_t) b[i++]);
        ASSERT_EQ(0U, (uint32_t) b[i++]);
        ASSERT_EQ(0U, (uint32_t) b[i++]);
        ASSERT_EQ(0U, (uint32_t) b[i++]);
        ASSERT_EQ(127U, (uint32_t) b[i++]);
        dmImage::Free(&image);
    }
}

TEST(dmImage, PngGray)
{
    dmImage::Image image;
    dmImage::Result r =  dmImage::Load(GRAY_CHECK_2X2_PNG, GRAY_CHECK_2X2_PNG_SIZE, false, false, &image);
    ASSERT_EQ(dmImage::RESULT_OK, r);
    ASSERT_EQ(2U, image.m_Width);
    ASSERT_EQ(2U, image.m_Height);
    ASSERT_EQ(dmImage::TYPE_LUMINANCE, image.m_Type);
    ASSERT_NE((void*) 0, image.m_Buffer);

    const uint8_t* b = (const uint8_t*) image.m_Buffer;
    int i = 0;
    ASSERT_EQ(0U, (uint32_t) b[i++]);
    ASSERT_EQ(255U, (uint32_t) b[i++]);
    ASSERT_EQ(255U, (uint32_t) b[i++]);
    ASSERT_EQ(0U, (uint32_t) b[i++]);
    dmImage::Free(&image);
}

TEST(dmImage, PngGrayAlpha)
{
    dmImage::Image image;
    dmImage::Result r =  dmImage::Load(GRAY_ALPHA_CHECK_2X2_PNG, GRAY_ALPHA_CHECK_2X2_PNG_SIZE, false, false, &image);
    ASSERT_EQ(dmImage::RESULT_OK, r);
    ASSERT_EQ(2U, image.m_Width);
    ASSERT_EQ(2U, image.m_Height);
    ASSERT_EQ(dmImage::TYPE_LUMINANCE_ALPHA, image.m_Type);
    ASSERT_NE((void*) 0, image.m_Buffer);

    // DMSDK Test
    dmImage::HImage h_image = &image;
    ASSERT_EQ(2U, dmImage::GetWidth(h_image));
    ASSERT_EQ(2U, dmImage::GetHeight(h_image));
    ASSERT_EQ(dmImage::TYPE_LUMINANCE_ALPHA, dmImage::GetType(h_image));
    ASSERT_NE((void*) 0, dmImage::GetData(h_image));
    ASSERT_EQ(image.m_Buffer, dmImage::GetData(h_image));

    const uint8_t* b = (const uint8_t*) image.m_Buffer;
    int i = 0;

    // Pixel 1
    ASSERT_EQ(0U, (uint32_t) b[i++]);
    ASSERT_EQ(255U, (uint32_t) b[i++]);

    // Pixel 2
    ASSERT_EQ(255U, (uint32_t) b[i++]);
    ASSERT_EQ(255U, (uint32_t) b[i++]);

    // Pixel 3
    ASSERT_EQ(255U, (uint32_t) b[i++]);
    ASSERT_EQ(255U, (uint32_t) b[i++]);

    // Pixel 4
    ASSERT_EQ(0U, (uint32_t) b[i++]);
    ASSERT_EQ(255U, (uint32_t) b[i++]);

    dmImage::Free(&image);
}

TEST(dmImage, Jpeg)
{
    dmImage::Image image;
    dmImage::Result r =  dmImage::Load(DEFOLD_64_JPG, DEFOLD_64_JPG_SIZE, false, false, &image);
    ASSERT_EQ(dmImage::RESULT_OK, r);
    ASSERT_EQ(64U, image.m_Width);
    ASSERT_EQ(64U, image.m_Height);
    ASSERT_EQ(dmImage::TYPE_RGB, image.m_Type);
    ASSERT_NE((void*) 0, image.m_Buffer);
    dmImage::Free(&image);
}

TEST(dmImage, ProgressiveJpeg)
{
    dmImage::Image image;
    dmImage::Result r =  dmImage::Load(DEFOLD_64_PROGRESSIVE_JPG, DEFOLD_64_PROGRESSIVE_JPG_SIZE, false, false, &image);
    ASSERT_EQ(dmImage::RESULT_OK, r);
    ASSERT_EQ(64U, image.m_Width);
    ASSERT_EQ(64U, image.m_Height);
    ASSERT_EQ(dmImage::TYPE_RGB, image.m_Type);
    ASSERT_NE((void*) 0, image.m_Buffer);
    dmImage::Free(&image);
}

TEST(dmImage, case2319)
{
    dmImage::Image image;
    dmImage::Result r =  dmImage::Load(CASE2319_JPG, CASE2319_JPG_SIZE, false, false, &image);

    ASSERT_EQ(dmImage::RESULT_OK, r);
    ASSERT_EQ(165U, image.m_Width);
    ASSERT_EQ(240U, image.m_Height);
    ASSERT_EQ(dmImage::TYPE_RGB, image.m_Type);
    ASSERT_NE((void*) 0, image.m_Buffer);

    const uint8_t* b = (const uint8_t*) image.m_Buffer;
    int i = 0;
    ASSERT_EQ(137U, (uint32_t) b[i++]);
    ASSERT_EQ(182U, (uint32_t) b[i++]);
    ASSERT_EQ(162U, (uint32_t) b[i++]);

    // Pixel at 67x43
    i = (43 * image.m_Width + 67) * 3;
    ASSERT_EQ(255U, (uint32_t) b[i++]);
    ASSERT_EQ(204U, (uint32_t) b[i++]);
    ASSERT_EQ(16U, (uint32_t) b[i++]);

    dmImage::Free(&image);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
