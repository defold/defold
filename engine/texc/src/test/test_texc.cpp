#include <gtest/gtest.h>

#include "../texc.h"

class TexcTest : public ::testing::Test
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

static dmTexc::HTexture CreateDefaultL()
{
    return dmTexc::Create(2, 2, dmTexc::PF_L8, dmTexc::CS_LRGB, default_data_l);
}

uint8_t default_data_rgb[4*3] =
{
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 255
};

static dmTexc::HTexture CreateDefaultRGB()
{
    return dmTexc::Create(2, 2, dmTexc::PF_R8G8B8, dmTexc::CS_LRGB, default_data_rgb);
}

uint8_t default_data_rgba[4*4] =
{
        255, 0, 0, 255,
        0, 255, 0, 255,
        0, 0, 255, 255,
        255, 255, 255, 255
};

static dmTexc::HTexture CreateDefaultRGBA()
{
    return dmTexc::Create(2, 2, dmTexc::PF_R8G8B8A8, dmTexc::CS_LRGB, default_data_rgba);
}

static dmTexc::HTexture CreateDefaultRGBA16x16()
{
    uint8_t data[16*16*4];
    memset(data, 0xff, sizeof(data));
    return dmTexc::Create(16, 16, dmTexc::PF_R8G8B8A8, dmTexc::CS_LRGB, data);
}

struct Format
{
    dmTexc::HTexture (*m_CreateFn)();
    uint32_t m_ComponentCount;
    uint8_t* m_DefaultData;
};
Format formats[] =
{
        {CreateDefaultL, 1, default_data_l},
        {CreateDefaultRGB, 1, default_data_rgb},
        {CreateDefaultRGBA, 1, default_data_rgba},
};

TEST_F(TexcTest, Load)
{
    uint8_t out[4*4];
    for (uint32_t i = 0; i < 3; ++i)
    {
        Format& format = formats[i];
        dmTexc::HTexture texture = (*format.m_CreateFn)();
        ASSERT_NE(dmTexc::INVALID_TEXTURE, texture);
        dmTexc::Header header;
        dmTexc::GetHeader(texture, &header);
        ASSERT_EQ(2u, header.m_Width);
        ASSERT_EQ(2u, header.m_Height);
        dmTexc::GetData(texture, out, 16);
        ASSERT_EQ(0, memcmp(format.m_DefaultData, out, 4*format.m_ComponentCount));
        dmTexc::Destroy(texture);
    }
}

TEST_F(TexcTest, Resize)
{
    // For some reason only RGBA supports resizing
    bool supported[] = {false, false, true};
    for (uint32_t i = 0; i < 3; ++i)
    {
        Format& format = formats[i];
        dmTexc::HTexture texture = (*format.m_CreateFn)();
        dmTexc::Header header;
        if (supported[i])
        {
            ASSERT_TRUE(dmTexc::Resize(texture, 4, 4));
            dmTexc::GetHeader(texture, &header);
            ASSERT_EQ(4u, header.m_Width);
            ASSERT_EQ(4u, header.m_Height);
        }
        else
        {
            ASSERT_FALSE(dmTexc::Resize(texture, 4, 4));
            dmTexc::GetHeader(texture, &header);
            ASSERT_EQ(2u, header.m_Width);
            ASSERT_EQ(2u, header.m_Height);
        }
        dmTexc::Destroy(texture);
    }
}

TEST_F(TexcTest, PreMultipliedAlpha)
{
    // Only RGBA supports pre-multiplication, which makes sense
    bool supported[] = {false, false, true};
    for (uint32_t i = 0; i < 3; ++i)
    {
        Format& format = formats[i];
        dmTexc::HTexture texture = (*format.m_CreateFn)();
        if (supported[i])
            ASSERT_TRUE(dmTexc::PreMultiplyAlpha(texture));
        else
            ASSERT_FALSE(dmTexc::PreMultiplyAlpha(texture));
        dmTexc::Destroy(texture);
    }
}

TEST_F(TexcTest, MipMaps)
{
    // For some reason only RGBA supports mip-map generation
    bool supported[] = {false, false, true};
    for (uint32_t i = 0; i < 3; ++i)
    {
        Format& format = formats[i];
        dmTexc::HTexture texture = (*format.m_CreateFn)();
        if (supported[i])
            ASSERT_TRUE(dmTexc::GenMipMaps(texture));
        else
            ASSERT_FALSE(dmTexc::GenMipMaps(texture));
        dmTexc::Destroy(texture);
    }
}

TEST_F(TexcTest, Transcode)
{
    dmTexc::HTexture texture = CreateDefaultRGBA();
    dmTexc::Header header;

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_L8, dmTexc::CS_LRGB, dmTexc::CL_NORMAL, dmTexc::CT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char l8[8] = {'l', 0, 0, 0, 8, 0, 0, 0};
    ASSERT_EQ(0, memcmp(l8, (void*)&header.m_PixelFormat, 8));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_R8G8B8, dmTexc::CS_LRGB, dmTexc::CL_NORMAL, dmTexc::CT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char r8g8b8[8] = {'r', 'g', 'b', 0, 8, 8, 8, 0};
    ASSERT_EQ(0, memcmp(r8g8b8, (void*)&header.m_PixelFormat, 8));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_R8G8B8A8, dmTexc::CS_LRGB, dmTexc::CL_NORMAL, dmTexc::CT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char r8g8b8a8[8] = {'r', 'g', 'b', 'a', 8, 8, 8, 8};
    ASSERT_EQ(0, memcmp(r8g8b8a8, (void*)&header.m_PixelFormat, 8));

    dmTexc::Destroy(texture);
}

TEST_F(TexcTest, TranscodeWebPLossless)
{
    dmTexc::HTexture texture = CreateDefaultRGBA16x16();
    dmTexc::Header header;

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_L8, dmTexc::CS_LRGB, dmTexc::CL_NORMAL, dmTexc::CT_WEBP));
    dmTexc::GetHeader(texture, &header);
    char l8[8] = {'l', 0, 0, 0, 8, 0, 0, 0};
    ASSERT_EQ(0, memcmp(l8, (void*)&header.m_PixelFormat, 8));
    ASSERT_NE(0, dmTexc::GetDataSizeCompressed(texture, 0));
    ASSERT_NE(dmTexc::GetDataSizeUncompressed(texture, 0), dmTexc::GetDataSizeCompressed(texture, 0));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_R8G8B8, dmTexc::CS_LRGB, dmTexc::CL_NORMAL, dmTexc::CT_WEBP));
    dmTexc::GetHeader(texture, &header);
    char r8g8b8[8] = {'r', 'g', 'b', 0, 8, 8, 8, 0};
    ASSERT_EQ(0, memcmp(r8g8b8, (void*)&header.m_PixelFormat, 8));
    ASSERT_NE(0, dmTexc::GetDataSizeCompressed(texture, 0));
    ASSERT_NE(dmTexc::GetDataSizeUncompressed(texture, 0), dmTexc::GetDataSizeCompressed(texture, 0));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_R8G8B8A8, dmTexc::CS_LRGB, dmTexc::CL_NORMAL, dmTexc::CT_WEBP));
    dmTexc::GetHeader(texture, &header);
    char r8g8b8a8[8] = {'r', 'g', 'b', 'a', 8, 8, 8, 8};
    ASSERT_EQ(0, memcmp(r8g8b8a8, (void*)&header.m_PixelFormat, 8));
    ASSERT_NE(0, dmTexc::GetDataSizeCompressed(texture, 0));
    ASSERT_NE(dmTexc::GetDataSizeUncompressed(texture, 0), dmTexc::GetDataSizeCompressed(texture, 0));

    dmTexc::Destroy(texture);
}

TEST_F(TexcTest, TranscodeWebPLossy)
{
    dmTexc::HTexture texture = CreateDefaultRGBA16x16();
    dmTexc::Header header;

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_L8, dmTexc::CS_LRGB, dmTexc::CL_NORMAL, dmTexc::CT_WEBP_LOSSY));
    dmTexc::GetHeader(texture, &header);
    char l8[8] = {'l', 0, 0, 0, 8, 0, 0, 0};
    ASSERT_EQ(0, memcmp(l8, (void*)&header.m_PixelFormat, 8));
    ASSERT_NE(0, dmTexc::GetDataSizeCompressed(texture, 0));
    ASSERT_NE(dmTexc::GetDataSizeUncompressed(texture, 0), dmTexc::GetDataSizeCompressed(texture, 0));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_R8G8B8, dmTexc::CS_LRGB, dmTexc::CL_NORMAL, dmTexc::CT_WEBP_LOSSY));
    dmTexc::GetHeader(texture, &header);
    char r8g8b8[8] = {'r', 'g', 'b', 0, 8, 8, 8, 0};
    ASSERT_EQ(0, memcmp(r8g8b8, (void*)&header.m_PixelFormat, 8));
    ASSERT_NE(0, dmTexc::GetDataSizeCompressed(texture, 0));
    ASSERT_NE(dmTexc::GetDataSizeUncompressed(texture, 0), dmTexc::GetDataSizeCompressed(texture, 0));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_R8G8B8A8, dmTexc::CS_LRGB, dmTexc::CL_NORMAL, dmTexc::CT_WEBP_LOSSY));
    dmTexc::GetHeader(texture, &header);
    char r8g8b8a8[8] = {'r', 'g', 'b', 'a', 8, 8, 8, 8};
    ASSERT_EQ(0, memcmp(r8g8b8a8, (void*)&header.m_PixelFormat, 8));
    ASSERT_NE(0, dmTexc::GetDataSizeCompressed(texture, 0));
    ASSERT_NE(dmTexc::GetDataSizeUncompressed(texture, 0), dmTexc::GetDataSizeCompressed(texture, 0));

    dmTexc::Destroy(texture);
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
    dmTexc::HTexture texture = CreateDefaultRGBA();

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

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
