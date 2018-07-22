#include <stdint.h>
#include <stdio.h>
#include <gtest/gtest.h>
#include "../dlib/image.h"

#include "data/color_check_2x2.png.embed.h"
#include "data/color_check_2x2_premult.png.embed.h"
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

// Hack to save RGB-textures in tga-format for debugging
static void SaveTga(const void* lpBits, uint32_t width, uint32_t height, FILE* fptr)
{
    putc(0,fptr);
    putc(0,fptr);
    putc(2,fptr);                         /* uncompressed RGB */
    putc(0,fptr); putc(0,fptr);
    putc(0,fptr); putc(0,fptr);
    putc(0,fptr);
    putc(0,fptr); putc(0,fptr);           /* X origin */
    putc(0,fptr); putc(0,fptr);           /* y origin */
    putc((width & 0x00FF),fptr);
    putc((width & 0xFF00) / 256,fptr);
    putc((height & 0x00FF),fptr);
    putc((height & 0xFF00) / 256,fptr);
    putc(24,fptr);                        /* 24 bit bitmap */
    putc(0,fptr);

    for (uint32_t y = 0; y <  height; ++y) {
        const uint8_t* p = (const uint8_t*) lpBits;
        p += (height - y - 1) * width * 3;
        for (uint32_t x = 0; x <  width * 3; x += 3) {
            putc(((int) p[x + 2]) & 0xff, fptr);
            putc(((int) p[x + 1]) & 0xff, fptr);
            putc(((int) p[x + 0]) & 0xff, fptr);
        }
    }
}

TEST(dmImage, Empty)
{
    dmImage::Image image;
    dmImage::Result r =  dmImage::Load(0, 0, false, &image);
    ASSERT_EQ(dmImage::RESULT_IMAGE_ERROR, r);
}

TEST(dmImage, Corrupt)
{
    uint32_t size = 256 * 256 * 4;
    void* b = malloc(size);
    memset(b, 0, size);
    dmImage::Image image;
    dmImage::Result r =  dmImage::Load(b, size, false, &image);
    free(b);
    ASSERT_EQ(dmImage::RESULT_IMAGE_ERROR, r);
}

TEST(dmImage, PngColor)
{
    for (int iter = 0; iter < 2; iter++) {
        dmImage::Image image;
        dmImage::Result r =  dmImage::Load(COLOR_CHECK_2X2_PNG, COLOR_CHECK_2X2_PNG_SIZE, iter == 0, &image);
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
    dmImage::Result r =  dmImage::Load(COLOR_CHECK_2X2_PREMULT_PNG, COLOR_CHECK_2X2_PREMULT_PNG_SIZE, true, &image);
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

TEST(dmImage, Png16Color)
{
    // // 16-bit images are not supported.
    // dmImage::Image image;
    // dmImage::Result r =  dmImage::Load(COLOR16_CHECK_2X2_PNG, COLOR16_CHECK_2X2_PNG_SIZE, false, &image);
    // ASSERT_EQ(dmImage::RESULT_IMAGE_ERROR, r);
    for (int iter = 0; iter < 2; iter++) {
        dmImage::Image image;
        dmImage::Result r =  dmImage::Load(COLOR_CHECK_2X2_PNG, COLOR_CHECK_2X2_PNG_SIZE, iter == 0, &image);
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
    dmImage::Result r =  dmImage::Load(GRAY_CHECK_2X2_PNG, GRAY_CHECK_2X2_PNG_SIZE, false, &image);
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
    dmImage::Result r =  dmImage::Load(GRAY_ALPHA_CHECK_2X2_PNG, GRAY_ALPHA_CHECK_2X2_PNG_SIZE, false, &image);
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
    dmImage::Result r =  dmImage::Load(DEFOLD_64_JPG, DEFOLD_64_JPG_SIZE, false, &image);
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
    dmImage::Result r =  dmImage::Load(DEFOLD_64_PROGRESSIVE_JPG, DEFOLD_64_PROGRESSIVE_JPG_SIZE, false, &image);
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
    dmImage::Result r =  dmImage::Load(CASE2319_JPG, CASE2319_JPG_SIZE, false, &image);

    ASSERT_EQ(dmImage::RESULT_OK, r);
    ASSERT_EQ(165U, image.m_Width);
    ASSERT_EQ(240U, image.m_Height);
    ASSERT_EQ(dmImage::TYPE_RGB, image.m_Type);
    ASSERT_NE((void*) 0, image.m_Buffer);

    FILE* file = fopen("tmp/test.tga", "wb");
    SaveTga(image.m_Buffer, image.m_Width, image.m_Height, file);
    fclose(file);

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
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
