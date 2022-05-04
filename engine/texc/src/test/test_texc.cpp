// Copyright 2020-2022 The Defold Foundation
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
    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

uint8_t default_data_l[4] =
{
        255, 0, 0, 255
};

static dmTexc::HTexture CreateDefaultL8(dmTexc::CompressionType compression_type)
{
    return dmTexc::Create(0, 2, 2, dmTexc::PF_L8, dmTexc::CS_LRGB, compression_type, default_data_l);
}

uint16_t default_data_l8a8[4] =
{
        0xffff, 0xff00, 0xff00, 0xffff,
};

static dmTexc::HTexture CreateDefaultL8A8(dmTexc::CompressionType compression_type)
{
    return dmTexc::Create(0, 2, 2, dmTexc::PF_L8A8, dmTexc::CS_LRGB, compression_type, default_data_l8a8);
}

uint16_t default_data_rgb_565[4] =
{
    dmTexc::RGB888ToRGB565(0xff, 0, 0),
    dmTexc::RGB888ToRGB565(0, 0xff, 0),
    dmTexc::RGB888ToRGB565(0, 0, 0xff),
    dmTexc::RGB888ToRGB565(0xff, 0xff, 0xff),
};

static dmTexc::HTexture CreateDefaultRGB16(dmTexc::CompressionType compression_type)
{
    return dmTexc::Create(0, 2, 2, dmTexc::PF_R5G6B5, dmTexc::CS_LRGB, compression_type, default_data_rgb_565);
}

uint8_t default_data_rgb_888[4*3] =
{
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 255
};

static dmTexc::HTexture CreateDefaultRGB24(dmTexc::CompressionType compression_type)
{
    return dmTexc::Create(0, 2, 2, dmTexc::PF_R8G8B8, dmTexc::CS_LRGB, compression_type, default_data_rgb_888);
}

uint8_t default_data_rgba_8888[4*4] =
{
        255, 0, 0, 255,
        0, 255, 0, 255,
        0, 0, 255, 255,
        255, 255, 255, 255
};

static dmTexc::HTexture CreateDefaultRGBA32(dmTexc::CompressionType compression_type)
{
    return dmTexc::Create(0, 2, 2, dmTexc::PF_R8G8B8A8, dmTexc::CS_LRGB, compression_type, default_data_rgba_8888);
}

uint16_t default_data_rgba_4444[4] =
{
        dmTexc::RGBA8888ToRGBA4444(255, 0, 0, 255),
        dmTexc::RGBA8888ToRGBA4444(0, 255, 0, 255),
        dmTexc::RGBA8888ToRGBA4444(0, 0, 255, 255),
        dmTexc::RGBA8888ToRGBA4444(255, 255, 255, 255)
};

static dmTexc::HTexture CreateDefaultRGBA16(dmTexc::CompressionType compression_type)
{
    return dmTexc::Create(0, 2, 2, dmTexc::PF_R4G4B4A4, dmTexc::CS_LRGB, dmTexc::CT_NONE, default_data_rgba_4444);
}



struct Format
{
    dmTexc::HTexture (*m_CreateFn)(dmTexc::CompressionType);
    uint32_t m_BytesPerPixel;
    void* m_DefaultData;
    dmTexc::CompressionType m_CompressionType;
    dmTexc::PixelFormat m_PixelFormat;
};
Format formats[] =
{
        {CreateDefaultL8, 1, default_data_l, dmTexc::CT_NONE, dmTexc::PF_L8},
        {CreateDefaultL8A8, 2, default_data_l8a8, dmTexc::CT_NONE, dmTexc::PF_L8A8},
        {CreateDefaultRGB24, 3, default_data_rgb_888, dmTexc::CT_NONE, dmTexc::PF_R8G8B8},
        {CreateDefaultRGBA32, 4, default_data_rgba_8888, dmTexc::CT_NONE, dmTexc::PF_R8G8B8A8},
        {CreateDefaultRGB16, 2, default_data_rgb_565, dmTexc::CT_NONE, dmTexc::PF_R5G6B5},
        {CreateDefaultRGBA16, 2, default_data_rgba_4444, dmTexc::CT_NONE, dmTexc::PF_R4G4B4A4},
};
static const size_t format_count = sizeof(formats)/sizeof(Format);

TEST_F(TexcTest, Load)
{
    uint8_t out[4*4];
    uint8_t expected_rgba[4*4];
    for (uint32_t i = 0; i < format_count ; ++i)
    {
        Format& format = formats[i];
        dmTexc::HTexture texture = (*format.m_CreateFn)(format.m_CompressionType);
        ASSERT_NE(dmTexc::INVALID_TEXTURE, texture);
        dmTexc::Header header;
        dmTexc::GetHeader(texture, &header);
        ASSERT_EQ(2u, header.m_Width);
        ASSERT_EQ(2u, header.m_Height);
        uint32_t outsize = dmTexc::GetData(texture, out, sizeof(out));
        // At this point, it's RGBA8888

        ASSERT_EQ(header.m_Width*header.m_Height*4U, outsize);

        bool result = ConvertToRGBA8888((const uint8_t*)format.m_DefaultData, header.m_Width, header.m_Height, format.m_PixelFormat, expected_rgba);
        ASSERT_TRUE(result);
        ASSERT_ARRAY_EQ_LEN(expected_rgba, out, sizeof(out));

        dmTexc::Destroy(texture);
    }
}

static void ComparePixel(uint8_t* expected, uint8_t* current, uint32_t num_channels)
{
    ASSERT_ARRAY_EQ_LEN(expected, current, num_channels);
}

TEST_F(TexcTest, Resize)
{
    uint8_t orig[(4*4)*4];
    uint8_t resized[(4*4)*4];

    // original/resized sizes
    uint32_t owidth = 2;
    uint32_t oheight = 2;
    uint32_t rwidth = 4;
    uint32_t rheight = 4;

    for (uint32_t i = 0; i < format_count; ++i)
    {
        Format& format = formats[i];

        dmTexc::HTexture texture = (*format.m_CreateFn)(format.m_CompressionType);
        dmTexc::Header header;

        dmTexc::GetData(texture, orig, sizeof(orig));

        ASSERT_TRUE(dmTexc::Resize(texture, 4, 4));
        dmTexc::GetHeader(texture, &header);
        ASSERT_EQ(4u, header.m_Width);
        ASSERT_EQ(4u, header.m_Height);

        dmTexc::GetData(texture, resized, sizeof(resized));

        uint32_t bpp = 4;
        uint32_t ox,oy,rx,ry;

        // Check the four corners
        ox=0; oy=0; rx=0; ry=0;
        ComparePixel(&orig[ox*bpp+oy*owidth*bpp],      &resized[rx*bpp+ry*rwidth*bpp], 4);

        ox=owidth-1; oy=0; rx=rwidth-1; ry=0;
        ComparePixel(&orig[ox*bpp+oy*owidth*bpp],      &resized[rx*bpp+ry*rwidth*bpp], 4);

        ox=0; oy=oheight-1; rx=0; ry=rheight-1;
        ComparePixel(&orig[ox*bpp+oy*owidth*bpp],      &resized[rx*bpp+ry*rwidth*bpp], 4);

        ox=owidth-1; oy=oheight-1; rx=rwidth-1; ry=rheight-1;
        ComparePixel(&orig[ox*bpp+oy*owidth*bpp],      &resized[rx*bpp+ry*rwidth*bpp], 4);

        dmTexc::Destroy(texture);
    }
}

TEST_F(TexcTest, PreMultipliedAlpha)
{
    // We convert to 32 bit formats internally, with default alpha
    for (uint32_t i = 0; i < format_count; ++i)
    {
        Format& format = formats[i];
        dmTexc::HTexture texture = (*format.m_CreateFn)(format.m_CompressionType);
        ASSERT_TRUE(dmTexc::PreMultiplyAlpha(texture));
        dmTexc::Destroy(texture);
    }
}

TEST_F(TexcTest, MipMaps)
{
    for (uint32_t i = 0; i < format_count; ++i)
    {
        Format& format = formats[i];
        dmTexc::HTexture texture = (*format.m_CreateFn)(format.m_CompressionType);
        ASSERT_TRUE(dmTexc::GenMipMaps(texture));
        dmTexc::Destroy(texture);
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

    uint8_t out[4*4];
    dmTexc::HTexture texture = CreateDefaultRGBA32(dmTexc::CT_NONE);

    // Original values
    dmTexc::GetData(texture, out, 16);
    ASSERT_RGBA(out,      red);
    ASSERT_RGBA(out+4,  green);
    ASSERT_RGBA(out+8,   blue);
    ASSERT_RGBA(out+12, white);

    /* Flip X axis:
     *  +--------+--------+
     *  | green  |  red   |
     *  +--------+--------+
     *  | white  |  blue  |
     *  +--------+--------+
     */
    ASSERT_TRUE(dmTexc::Flip(texture, dmTexc::FLIP_AXIS_X));
    dmTexc::GetData(texture, out, 16);
    ASSERT_RGBA(out,    green);
    ASSERT_RGBA(out+4,    red);
    ASSERT_RGBA(out+8,  white);
    ASSERT_RGBA(out+12,  blue);

    /* Flip Y axis:
     *  +--------+--------+
     *  | white  |  blue  |
     *  +--------+--------+
     *  | green  |  red   |
     *  +--------+--------+
     */
    ASSERT_TRUE(dmTexc::Flip(texture, dmTexc::FLIP_AXIS_Y));
    dmTexc::GetData(texture, out, 16);
    ASSERT_RGBA(out,    white);
    ASSERT_RGBA(out+4,   blue);
    ASSERT_RGBA(out+8,  green);
    ASSERT_RGBA(out+12,   red);

    // Flip Z axis (no change)
    ASSERT_TRUE(dmTexc::Flip(texture, dmTexc::FLIP_AXIS_Z));
    dmTexc::GetData(texture, out, 16);
    ASSERT_RGBA(out,    white);
    ASSERT_RGBA(out+4,   blue);
    ASSERT_RGBA(out+8,  green);
    ASSERT_RGBA(out+12,   red);

    dmTexc::Destroy(texture);
}

#undef ASSERT_RGBA

// static void PrintTexture(const char* msg, uint32_t* data, uint32_t width, uint32_t height)
// {
//     printf("%s\n", msg);
//     for (uint32_t y = 0; y < height; ++y)
//     {
//         for (uint32_t x = 0; x < width; ++x)
//         {
//             printf("0x%08X,", data[x + width * y]);
//         }
//         printf("\n");
//     }
// }

static void PrintTexture(const char* msg, uint32_t* data, uint32_t width, uint32_t height)
{
    (void)msg, (void)data, (void)width, (void)height;
}

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

    PrintTexture("\nBEFORE", image, width, height);

    dmTexc::FlipImageY_RGBA8888(image, width, height);

    PrintTexture("\nAFTER", image, width, height);

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


    PrintTexture("\nBEFORE", image, w, h);

    dmTexc::FlipImageY_RGBA8888(image, w, h);

    PrintTexture("\nAFTER", image, w, h);

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

    PrintTexture("\nBEFORE", image, width, height);

    dmTexc::FlipImageX_RGBA8888(image, width, height);

    PrintTexture("\nAFTER", image, width, height);

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

    PrintTexture("\nBEFORE", image, w, h);

    dmTexc::FlipImageX_RGBA8888(image, w, h);

    PrintTexture("\nAFTER", image, w, h);

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
    dmTexc::CompressionType m_CompressionType;
    dmTexc::PixelFormat     m_InputFormat;
    dmTexc::PixelFormat     m_OutputFormat;
    dmTexc::ColorSpace      m_ColorSpace;
};
CompileInfo compile_info[] =
{
    {"src/test/data/a.png", dmTexc::CT_NONE, dmTexc::PF_R8G8B8A8, dmTexc::PF_R5G6B5, dmTexc::CS_SRGB},
};

class TexcCompileTest : public jc_test_params_class<CompileInfo>
{
protected:
    virtual void SetUp()
    {
        const CompileInfo& info = GetParam();
        uint8_t* image = stbi_load(info.m_Path, &m_Width, &m_Height, 0, 0);
        ASSERT_TRUE(image != 0);

        m_Texture = dmTexc::Create(info.m_Path, m_Width, m_Height, info.m_InputFormat, info.m_ColorSpace, info.m_CompressionType, image);
        printf("image: %s   %u x %u\n", info.m_Path, m_Width, m_Height);
    }

    virtual void TearDown()
    {
        dmTexc::Destroy(m_Texture);
    }

    dmTexc::HTexture m_Texture;
    int m_Width;
    int m_Height;
};


TEST_P(TexcCompileTest, FlipX)
{
    ASSERT_TRUE(dmTexc::Flip(m_Texture, dmTexc::FLIP_AXIS_X));
}

TEST_P(TexcCompileTest, FlipY)
{
    ASSERT_TRUE(dmTexc::Flip(m_Texture, dmTexc::FLIP_AXIS_Y));
}

TEST_P(TexcCompileTest, PreMultiplyAlpha)
{
    ASSERT_TRUE(dmTexc::PreMultiplyAlpha(m_Texture));
}

TEST_P(TexcCompileTest, GenMipMaps)
{
    ASSERT_TRUE(dmTexc::GenMipMaps(m_Texture));
}

TEST_P(TexcCompileTest, Encode)
{
    const CompileInfo& info = GetParam();
    ASSERT_TRUE(dmTexc::Encode(m_Texture, info.m_OutputFormat, info.m_ColorSpace, dmTexc::CL_BEST, info.m_CompressionType, true, 1));
}

INSTANTIATE_TEST_CASE_P(TexcCompileTest, TexcCompileTest, jc_test_values_in(compile_info));


int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);

    int ret = jc_test_run_all();
    return ret;
}
