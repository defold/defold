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
#include <dlib/image.h>
#include <string.h> // memcmp

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "../texc.h"
#include "../texc_private.h"

class TexcTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

uint8_t default_data_l[4] =
{
        255, 0, 0, 255
};

static dmTexc::Image* CreateDefaultL8()
{
    return dmTexc::CreateImage(0, 2, 2, dmTexc::PF_L8, dmTexc::CS_LRGB, sizeof(default_data_l), default_data_l);
}

uint16_t default_data_l8a8[4] =
{
        0xffff, 0xff00, 0xff00, 0xffff,
};

static dmTexc::Image* CreateDefaultL8A8()
{
    return dmTexc::CreateImage(0, 2, 2, dmTexc::PF_L8A8, dmTexc::CS_LRGB, sizeof(default_data_l8a8), (uint8_t*)default_data_l8a8);
}

uint16_t default_data_rgb_565[4] =
{
    dmTexc::RGB888ToRGB565(0xff, 0, 0),
    dmTexc::RGB888ToRGB565(0, 0xff, 0),
    dmTexc::RGB888ToRGB565(0, 0, 0xff),
    dmTexc::RGB888ToRGB565(0xff, 0xff, 0xff),
};

static dmTexc::Image* CreateDefaultRGB16()
{
    return dmTexc::CreateImage(0, 2, 2, dmTexc::PF_R5G6B5, dmTexc::CS_LRGB, sizeof(default_data_rgb_565), (uint8_t*)default_data_rgb_565);
}

uint8_t default_data_rgb_888[4*3] =
{
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 255
};

static dmTexc::Image* CreateDefaultRGB24()
{
    return dmTexc::CreateImage(0, 2, 2, dmTexc::PF_R8G8B8, dmTexc::CS_LRGB, sizeof(default_data_rgb_888), (uint8_t*)default_data_rgb_888);
}

uint8_t default_data_rgba_8888[4*4] =
{
        255, 0, 0, 255,
        0, 255, 0, 255,
        0, 0, 255, 255,
        255, 255, 255, 255
};

static dmTexc::Image* CreateDefaultRGBA32()
{
    return dmTexc::CreateImage(0, 2, 2, dmTexc::PF_R8G8B8A8, dmTexc::CS_LRGB, sizeof(default_data_rgba_8888), (uint8_t*)default_data_rgba_8888);
}

uint16_t default_data_rgba_4444[4] =
{
        dmTexc::RGBA8888ToRGBA4444(255, 0, 0, 255),
        dmTexc::RGBA8888ToRGBA4444(0, 255, 0, 255),
        dmTexc::RGBA8888ToRGBA4444(0, 0, 255, 255),
        dmTexc::RGBA8888ToRGBA4444(255, 255, 255, 255)
};

static dmTexc::Image* CreateDefaultRGBA16()
{
    return dmTexc::CreateImage(0, 2, 2, dmTexc::PF_R4G4B4A4, dmTexc::CS_LRGB, sizeof(default_data_rgba_4444), (uint8_t*)default_data_rgba_4444);
}



struct Format
{
    dmTexc::Image* (*m_CreateFn)();
    uint32_t m_BytesPerPixel;
    void* m_DefaultData;
    dmTexc::PixelFormat m_PixelFormat;
};
Format formats[] =
{
        {CreateDefaultL8, 1, default_data_l, dmTexc::PF_L8},
        {CreateDefaultL8A8, 2, default_data_l8a8, dmTexc::PF_L8A8},
        {CreateDefaultRGB24, 3, default_data_rgb_888, dmTexc::PF_R8G8B8},
        {CreateDefaultRGBA32, 4, default_data_rgba_8888, dmTexc::PF_R8G8B8A8},
        {CreateDefaultRGB16, 2, default_data_rgb_565, dmTexc::PF_R5G6B5},
        {CreateDefaultRGBA16, 2, default_data_rgba_4444, dmTexc::PF_R4G4B4A4},
};
static const size_t format_count = sizeof(formats)/sizeof(Format);

TEST_F(TexcTest, Load)
{
    uint8_t expected_rgba[4*4];
    for (uint32_t i = 0; i < format_count ; ++i)
    {
        Format& format = formats[i];
        dmTexc::Image* image = (*format.m_CreateFn)();
        ASSERT_NE((dmTexc::Image*)0, image);
        ASSERT_EQ(2u, image->m_Width);
        ASSERT_EQ(2u, image->m_Height);

        uint32_t outsize = image->m_DataCount;
        // At this point, it's RGBA8888

        ASSERT_EQ(image->m_Width*image->m_Height*4U, outsize);

        bool result = ConvertToRGBA8888((const uint8_t*)format.m_DefaultData, image->m_Width, image->m_Height, format.m_PixelFormat, expected_rgba);
        ASSERT_TRUE(result);
        ASSERT_ARRAY_EQ_LEN(expected_rgba, image->m_Data, image->m_DataCount);

        dmTexc::DestroyImage(image);
    }
}

static void ComparePixel(uint8_t* expected, uint8_t* current, uint32_t num_channels)
{
    ASSERT_ARRAY_EQ_LEN(expected, current, num_channels);
}

TEST_F(TexcTest, Resize)
{
    // original/resized sizes
    uint32_t owidth = 2;
    uint32_t oheight = 2;
    uint32_t rwidth = 4;
    uint32_t rheight = 4;
    uint32_t bpp = 4;

    for (uint32_t i = 0; i < format_count; ++i)
    {
        Format& format = formats[i];

        dmTexc::Image* image = (*format.m_CreateFn)();

        dmTexc::Image* resized = dmTexc::Resize(image, rwidth, rheight);

        ASSERT_NE((dmTexc::Image*)0, resized);
        ASSERT_EQ(4u, resized->m_Width);
        ASSERT_EQ(4u, resized->m_Height);

        uint8_t* orig = image->m_Data;
        uint8_t* expected = resized->m_Data;

        uint32_t ox,oy,rx,ry;

        // Check the four corners
        ox=0; oy=0; rx=0; ry=0;
        ComparePixel(&orig[ox*bpp+oy*owidth*bpp],      &expected[rx*bpp+ry*rwidth*bpp], 4);

        ox=owidth-1; oy=0; rx=rwidth-1; ry=0;
        ComparePixel(&orig[ox*bpp+oy*owidth*bpp],      &expected[rx*bpp+ry*rwidth*bpp], 4);

        ox=0; oy=oheight-1; rx=0; ry=rheight-1;
        ComparePixel(&orig[ox*bpp+oy*owidth*bpp],      &expected[rx*bpp+ry*rwidth*bpp], 4);

        ox=owidth-1; oy=oheight-1; rx=rwidth-1; ry=rheight-1;
        ComparePixel(&orig[ox*bpp+oy*owidth*bpp],      &expected[rx*bpp+ry*rwidth*bpp], 4);

        dmTexc::DestroyImage(resized);
        dmTexc::DestroyImage(image);
    }
}

TEST_F(TexcTest, PreMultipliedAlpha)
{
    // We convert to 32 bit formats internally, with default alpha
    for (uint32_t i = 0; i < format_count; ++i)
    {
        Format& format = formats[i];
        dmTexc::Image* image = (*format.m_CreateFn)();
        ASSERT_TRUE(dmTexc::PreMultiplyAlpha(image));
        dmTexc::DestroyImage(image);
    }
}


#define ASSERT_RGBA(exp, act)\
    ASSERT_EQ((exp)[0], (act)[0]);\
    ASSERT_EQ((exp)[1], (act)[1]);\
    ASSERT_EQ((exp)[2], (act)[2]);\
    ASSERT_EQ((exp)[3], (act)[3]);\

TEST_F(TexcTest, FlipAxis)
{

    /* Original image:
     *  +--------+--------+
     *  |  red   | green  |
     *  +--------+--------+
     *  |  blue  | white  |
     *  +--------+--------+
     */

    const uint8_t red[4]   = {255,   0,   0, 255};
    const uint8_t green[4] = {  0, 255,   0, 255};
    const uint8_t blue[4]  = {  0,   0, 255, 255};
    const uint8_t white[4] = {255, 255, 255, 255};

    dmTexc::Image* image = CreateDefaultRGBA32();

    // Original values
    ASSERT_RGBA(image->m_Data,      red);
    ASSERT_RGBA(image->m_Data+4,  green);
    ASSERT_RGBA(image->m_Data+8,   blue);
    ASSERT_RGBA(image->m_Data+12, white);

    /* Flip X axis:
     *  +--------+--------+
     *  | green  |  red   |
     *  +--------+--------+
     *  | white  |  blue  |
     *  +--------+--------+
     */
    ASSERT_TRUE(dmTexc::Flip(image, dmTexc::FLIP_AXIS_X));
    ASSERT_RGBA(image->m_Data,    green);
    ASSERT_RGBA(image->m_Data+4,    red);
    ASSERT_RGBA(image->m_Data+8,  white);
    ASSERT_RGBA(image->m_Data+12,  blue);

    /* Flip Y axis:
     *  +--------+--------+
     *  | white  |  blue  |
     *  +--------+--------+
     *  | green  |  red   |
     *  +--------+--------+
     */
    ASSERT_TRUE(dmTexc::Flip(image, dmTexc::FLIP_AXIS_Y));
    ASSERT_RGBA(image->m_Data,    white);
    ASSERT_RGBA(image->m_Data+4,   blue);
    ASSERT_RGBA(image->m_Data+8,  green);
    ASSERT_RGBA(image->m_Data+12,   red);

    // Flip Z axis (no change)
    ASSERT_FALSE(dmTexc::Flip(image, dmTexc::FLIP_AXIS_Z));
    ASSERT_RGBA(image->m_Data,    white);
    ASSERT_RGBA(image->m_Data+4,   blue);
    ASSERT_RGBA(image->m_Data+8,  green);
    ASSERT_RGBA(image->m_Data+12,   red);

    dmTexc::DestroyImage(image);
}

#undef ASSERT_RGBA

TEST(Helpers, FlipY)
{
    const uint32_t width = 8;
    const uint32_t height = 8;
    uint32_t image[width*height];

    for (uint32_t y = 0; y < height; ++y)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            image[x + width * y] = x + width * (height - y - 1);
        }
    }

    dmTexc::FlipImageY_RGBA8888(image, width, height);

    for (uint32_t y = 0; y < height; ++y)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            ASSERT_EQ(x + width * y, image[x + width * y]);
        }
    }


    int w = 7;
    int h = 7;

    for (uint32_t y = 0; y < h; ++y)
    {
        for (uint32_t x = 0; x < w; ++x)
        {
            image[x + w * y] = x + w * (h - y - 1);
        }
    }

    dmTexc::FlipImageY_RGBA8888(image, w, h);

    for (uint32_t y = 0; y < h; ++y)
    {
        for (uint32_t x = 0; x < w; ++x)
        {
            ASSERT_EQ(x + w * y, image[x + w * y]);
        }
    }

}

TEST(Helpers, FlipX)
{
    const uint32_t width = 8;
    const uint32_t height = 8;
    uint32_t image[width*height];

    for (uint32_t y = 0; y < height; ++y)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            image[x + width * y] = (width - x - 1) + width * y;
        }
    }

    dmTexc::FlipImageX_RGBA8888(image, width, height);

    for (uint32_t y = 0; y < height; ++y)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            ASSERT_EQ(x + width * y, image[x + width * y]);
        }
    }

    int w = 7;
    int h = 7;

    for (uint32_t y = 0; y < h; ++y)
    {
        for (uint32_t x = 0; x < w; ++x)
        {
            image[x + w * y] = (w - x - 1) + w * y;
        }
    }

    dmTexc::FlipImageX_RGBA8888(image, w, h);

    for (uint32_t y = 0; y < h; ++y)
    {
        for (uint32_t x = 0; x < w; ++x)
        {
            ASSERT_EQ(x + w * y, image[x + w * y]);
        }
    }
}

struct CompileInfo
{
    const char*             m_Path;
    dmTexc::PixelFormat     m_InputFormat;
    dmTexc::PixelFormat     m_OutputFormat;
    dmTexc::ColorSpace      m_ColorSpace;
};
CompileInfo compile_info[] =
{
    {"src/test/data/a.png", dmTexc::PF_R8G8B8A8, dmTexc::PF_R5G6B5, dmTexc::CS_SRGB},
};

class TexcCompileTest : public jc_test_params_class<CompileInfo>
{
protected:
    void SetUp() override
    {
        const CompileInfo& info = GetParam();
        uint8_t* image = stbi_load(info.m_Path, &m_Width, &m_Height, 0, 0);
        ASSERT_TRUE(image != 0);

        m_Image = dmTexc::CreateImage(info.m_Path, m_Width, m_Height, info.m_InputFormat, info.m_ColorSpace, m_Width*m_Height*4, image);

        stbi_image_free(image);
    }

    void TearDown() override
    {
        dmTexc::DestroyImage(m_Image);
    }

    dmTexc::Image* m_Image;
    int m_Width;
    int m_Height;
};


TEST_P(TexcCompileTest, FlipX)
{
    ASSERT_TRUE(dmTexc::Flip(m_Image, dmTexc::FLIP_AXIS_X));
}

TEST_P(TexcCompileTest, FlipY)
{
    ASSERT_TRUE(dmTexc::Flip(m_Image, dmTexc::FLIP_AXIS_Y));
}

TEST_P(TexcCompileTest, PreMultiplyAlpha)
{
    ASSERT_TRUE(dmTexc::PreMultiplyAlpha(m_Image));
}

TEST_P(TexcCompileTest, EncodeBasisU)
{
    const CompileInfo& info = GetParam();

    dmTexc::BasisUEncodeSettings settings;
    memset(&settings, 0, sizeof(settings));

    settings.m_Path = info.m_Path;
    settings.m_Width = m_Width;
    settings.m_Height = m_Height;

    settings.m_PixelFormat = info.m_InputFormat;
    settings.m_ColorSpace  = info.m_ColorSpace;

    settings.m_Data      = m_Image->m_Data;
    settings.m_DataCount = m_Image->m_DataCount;

    settings.m_NumThreads = 4;
    settings.m_Debug = false;

    settings.m_OutPixelFormat = info.m_OutputFormat;

    // Naming matching variables in basis_compressor_params (basis_comp.h)
    // CL_NORMAL
    settings.m_rdo_uastc = 0;
    settings.m_pack_uastc_flags = 2;

    uint8_t* out = 0;
    uint32_t out_size = 0;
    ASSERT_TRUE(dmTexc::BasisUEncode(&settings, &out, &out_size));

    free(out);
}

INSTANTIATE_TEST_CASE_P(TexcCompileTest, TexcCompileTest, jc_test_values_in(compile_info));

// We use a smaller texture to test ASTC with, so encoding is a bit faster.
TEST(TexcCompileTestASTC, Encode)
{
    const char* path = "src/test/data/a_small.png";

    int width, height;

    uint8_t* image_data = stbi_load(path, &width, &height, 0, 0);
    ASSERT_TRUE(image_data != 0);

    dmTexc::Image* image = dmTexc::CreateImage(path, width, height, dmTexc::PF_R8G8B8A8, dmTexc::CS_SRGB, width*height*4, image_data);

    dmTexc::ASTCEncodeSettings settings;
    memset(&settings, 0, sizeof(settings));

    // Note: We can't use the pixel format from the params.
    settings.m_Path        = path;
    settings.m_Width       = width;
    settings.m_Height      = height;
    settings.m_PixelFormat = dmTexc::PF_R8G8B8A8;
    settings.m_ColorSpace  = dmTexc::CS_SRGB;
    settings.m_Data        = image->m_Data;
    settings.m_DataCount   = image->m_DataCount;

    dmTexc::PixelFormat pixel_formats_astc[] = {
        dmTexc::PF_RGBA_ASTC_4x4,
        dmTexc::PF_RGBA_ASTC_5x4,
        dmTexc::PF_RGBA_ASTC_5x5,
        dmTexc::PF_RGBA_ASTC_6x5,
        dmTexc::PF_RGBA_ASTC_6x6,
        dmTexc::PF_RGBA_ASTC_8x5,
        dmTexc::PF_RGBA_ASTC_8x6,
        dmTexc::PF_RGBA_ASTC_8x8,
        dmTexc::PF_RGBA_ASTC_10x5,
        dmTexc::PF_RGBA_ASTC_10x6,
        dmTexc::PF_RGBA_ASTC_10x8,
        dmTexc::PF_RGBA_ASTC_10x10,
        dmTexc::PF_RGBA_ASTC_12x10,
        dmTexc::PF_RGBA_ASTC_12x12,
    };

    uint8_t* out = 0;
    uint32_t out_size = 0;

    for (int i = 0; i < DM_ARRAY_SIZE(pixel_formats_astc); ++i)
    {
        settings.m_OutPixelFormat = pixel_formats_astc[i];
        ASSERT_TRUE(dmTexc::ASTCEncode(&settings, &out, &out_size));
        free(out);
        out = 0;
    }

    dmTexc::DestroyImage(image);
    free(image_data);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);

    int ret = jc_test_run_all();
    return ret;
}
