#include <stdint.h>
#include <stdio.h>
#include <gtest/gtest.h>
#include "../dlib/image.h"

#include "data/color_check_2x2.png.embed.h"
#include "data/color16_check_2x2.png.embed.h"
#include "data/gray_check_2x2.png.embed.h"
#include "data/gray_alpha_check_2x2.png.embed.h"
#include "data/defold_64.jpg.embed.h"

/*
 * Imagemagick conversion
 *
 * Convert 8-bit png to 16-bit (per channel)
 * convert src/test/data/color_check_2x2.png -depth 16 -define png:color-type='2' -define png:bit-depth=16 src/test/color16_check_2x2.png
 */

TEST(dmImage, PngColor)
{
    dmImage::Image image;
    dmImage::Result r =  dmImage::Load(COLOR_CHECK_2X2_PNG, COLOR_CHECK_2X2_PNG_SIZE, &image);
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

TEST(dmImage, Png16Color)
{
    // 16-bit images are not supported.
    dmImage::Image image;
    dmImage::Result r =  dmImage::Load(COLOR16_CHECK_2X2_PNG, COLOR16_CHECK_2X2_PNG_SIZE, &image);
    ASSERT_EQ(dmImage::RESULT_IMAGE_ERROR, r);
}

TEST(dmImage, PngGray)
{
    dmImage::Image image;
    dmImage::Result r =  dmImage::Load(GRAY_CHECK_2X2_PNG, GRAY_CHECK_2X2_PNG_SIZE, &image);
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
    // Test implicit alpha channel removal for 2-component images.
    dmImage::Image image;
    dmImage::Result r =  dmImage::Load(GRAY_ALPHA_CHECK_2X2_PNG, GRAY_ALPHA_CHECK_2X2_PNG_SIZE, &image);
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

TEST(dmImage, Jpeg)
{
    dmImage::Image image;
    dmImage::Result r =  dmImage::Load(DEFOLD_64_JPG, DEFOLD_64_JPG_SIZE, &image);
    ASSERT_EQ(dmImage::RESULT_OK, r);
    ASSERT_EQ(64U, image.m_Width);
    ASSERT_EQ(64U, image.m_Height);
    ASSERT_EQ(dmImage::TYPE_RGB, image.m_Type);
    ASSERT_NE((void*) 0, image.m_Buffer);
    dmImage::Free(&image);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
