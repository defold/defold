#include <gtest/gtest.h>
#include <dlib/image.h>
#include <dlib/webp.h>

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

static dmTexc::HTexture CreateDefaultL8()
{
    return dmTexc::Create(2, 2, dmTexc::PF_L8, dmTexc::CS_LRGB, default_data_l);
}

uint16_t default_data_l8a8[4] =
{
        0xffff, 0xff00, 0xff00, 0xffff,
};

static dmTexc::HTexture CreateDefaultL8A8()
{
    return dmTexc::Create(2, 2, dmTexc::PF_L8A8, dmTexc::CS_LRGB, default_data_l8a8);
}

uint16_t default_data_rgb_565[4] =
{
        0x1f << 0,
        0x1f << 5,
        0x1f << 11,
        0xffff
};

static dmTexc::HTexture CreateDefaultRGB16()
{
    return dmTexc::Create(2, 2, dmTexc::PF_R5G6B5, dmTexc::CS_LRGB, default_data_rgb_565);
}

uint8_t default_data_rgb_888[4*3] =
{
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 255
};

static dmTexc::HTexture CreateDefaultRGB32()
{
    return dmTexc::Create(2, 2, dmTexc::PF_R8G8B8, dmTexc::CS_LRGB, default_data_rgb_888);
}

uint8_t default_data_rgba_8888[4*4] =
{
        255, 0, 0, 255,
        0, 255, 0, 255,
        0, 0, 255, 255,
        255, 255, 255, 255
};

static dmTexc::HTexture CreateDefaultRGBA32()
{
    return dmTexc::Create(2, 2, dmTexc::PF_R8G8B8A8, dmTexc::CS_LRGB, default_data_rgba_8888);
}

static dmTexc::HTexture CreateDefaultRGBA32(uint32_t w, uint32_t h)
{
    uint32_t* data = new uint32_t[w*h];
    for(uint32_t y = 0, i = 0; y < w; ++y)
    {
        for(uint32_t x = 0; x < h; ++x)
        {
            uint32_t r = x & 0xff;
            uint32_t g = (y & 0xff) << 8;
            uint32_t b = (255-(i & 0xff)) << 16;
            uint32_t a = (i & 0xff) << 24;
            data[(y*w)+x] = r | g | b | a;
            ++i;
        }
    }
    dmTexc::HTexture texture = dmTexc::Create(w, h, dmTexc::PF_R8G8B8A8, dmTexc::CS_LRGB, data);
    delete[] data;
    return texture;
}

uint16_t default_data_rgba_4444[4] =
{
        0xf000 | (0xf << 0),
        0xf000 | (0xf << 4),
        0xf000 | (0xf << 8),
        0xffff
};

static dmTexc::HTexture CreateDefaultRGBA16()
{
    return dmTexc::Create(2, 2, dmTexc::PF_R4G4B4A4, dmTexc::CS_LRGB, default_data_rgba_4444);
}



struct Format
{
    dmTexc::HTexture (*m_CreateFn)();
    uint32_t m_ComponentCount;
    uint32_t m_BytesPerPixel;
    void* m_DefaultData;
};
Format formats[] =
{
        {CreateDefaultL8, 1, 1, default_data_l},
        {CreateDefaultL8A8, 1, 2, default_data_l8a8},
        {CreateDefaultRGB32, 1, 4, default_data_rgb_888},
        {CreateDefaultRGBA32, 1, 4, default_data_rgba_8888},
        {CreateDefaultRGB16, 1, 2, default_data_rgb_565},
        {CreateDefaultRGBA16, 1, 2, default_data_rgba_4444},
};
static const size_t format_count = sizeof(formats)/sizeof(Format);

TEST_F(TexcTest, Load)
{
    uint8_t out[4*4];
    for (uint32_t i = 0; i < format_count ; ++i)
    {
        Format& format = formats[i];
        dmTexc::HTexture texture = (*format.m_CreateFn)();
        ASSERT_NE(dmTexc::INVALID_TEXTURE, texture);
        dmTexc::Header header;
        dmTexc::GetHeader(texture, &header);
        ASSERT_EQ(2u, header.m_Width);
        ASSERT_EQ(2u, header.m_Height);
        dmTexc::GetData(texture, out, 16);
        ASSERT_EQ(0, memcmp(format.m_DefaultData, out, format.m_BytesPerPixel*format.m_ComponentCount));
        dmTexc::Destroy(texture);
    }
}

TEST_F(TexcTest, Resize)
{
    // For some reason only RGBA supports resizing
    bool supported[] = {false, false, false, true, false, false};
    for (uint32_t i = 0; i < format_count; ++i)
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
    bool supported[] = {false, false, false, true, false, false};
    for (uint32_t i = 0; i < format_count; ++i)
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
    bool supported[] = {false, false, false, true, false, false};
    for (uint32_t i = 0; i < format_count; ++i)
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
    dmTexc::HTexture texture = CreateDefaultRGBA32();
    dmTexc::Header header;

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_L8, dmTexc::CS_LRGB, dmTexc::CL_NORMAL, dmTexc::CT_DEFAULT, dmTexc::DT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char l8[8] = {'l', 0, 0, 0, 8, 0, 0, 0};
    ASSERT_EQ(0, memcmp(l8, (void*)&header.m_PixelFormat, 8));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_L8A8, dmTexc::CS_LRGB, dmTexc::CL_NORMAL, dmTexc::CT_DEFAULT, dmTexc::DT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char l8a8[8] = {'l', 'a', 0, 0, 8, 8, 0, 0};
    ASSERT_EQ(0, memcmp(l8a8, (void*)&header.m_PixelFormat, 8));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_R8G8B8, dmTexc::CS_LRGB, dmTexc::CL_NORMAL, dmTexc::CT_DEFAULT, dmTexc::DT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char r8g8b8[8] = {'r', 'g', 'b', 0, 8, 8, 8, 0};
    ASSERT_EQ(0, memcmp(r8g8b8, (void*)&header.m_PixelFormat, 8));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_R8G8B8A8, dmTexc::CS_LRGB, dmTexc::CL_NORMAL, dmTexc::CT_DEFAULT, dmTexc::DT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char r8g8b8a8[8] = {'r', 'g', 'b', 'a', 8, 8, 8, 8};
    ASSERT_EQ(0, memcmp(r8g8b8a8, (void*)&header.m_PixelFormat, 8));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_R5G6B5, dmTexc::CS_LRGB, dmTexc::CL_NORMAL, dmTexc::CT_DEFAULT, dmTexc::DT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char r5g6b5[8] = {'r', 'g', 'b', 0, 5, 6, 5, 0};
    ASSERT_EQ(0, memcmp(r5g6b5, (void*)&header.m_PixelFormat, 8));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_R4G4B4A4, dmTexc::CS_LRGB, dmTexc::CL_NORMAL, dmTexc::CT_DEFAULT, dmTexc::DT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char r4g4b4a4[8] = {'r', 'g', 'b', 'a', 4, 4, 4, 4};
    ASSERT_EQ(0, memcmp(r4g4b4a4, (void*)&header.m_PixelFormat, 8));

    dmTexc::Destroy(texture);
}

TEST_F(TexcTest, TranscodeWebPLossless)
{
    dmTexc::HTexture texture = CreateDefaultRGBA32(256, 256);
    dmTexc::Header header;

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_L8, dmTexc::CS_LRGB, dmTexc::CL_FAST, dmTexc::CT_WEBP, dmTexc::DT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char l8[8] = {'l', 0, 0, 0, 8, 0, 0, 0};
    ASSERT_EQ(0, memcmp(l8, (void*)&header.m_PixelFormat, 8));
    ASSERT_NE(0, dmTexc::GetDataSizeCompressed(texture, 0));
    ASSERT_NE(dmTexc::GetDataSizeUncompressed(texture, 0), dmTexc::GetDataSizeCompressed(texture, 0));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_L8A8, dmTexc::CS_LRGB, dmTexc::CL_FAST, dmTexc::CT_WEBP, dmTexc::DT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char l8a8[8] = {'l', 'a', 0, 0, 8, 8, 0, 0};
    ASSERT_EQ(0, memcmp(l8a8, (void*)&header.m_PixelFormat, 8));
    ASSERT_NE(0, dmTexc::GetDataSizeCompressed(texture, 0));
    ASSERT_NE(dmTexc::GetDataSizeUncompressed(texture, 0), dmTexc::GetDataSizeCompressed(texture, 0));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_R8G8B8, dmTexc::CS_LRGB, dmTexc::CL_FAST, dmTexc::CT_WEBP, dmTexc::DT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char r8g8b8[8] = {'r', 'g', 'b', 0, 8, 8, 8, 0};
    ASSERT_EQ(0, memcmp(r8g8b8, (void*)&header.m_PixelFormat, 8));
    ASSERT_NE(0, dmTexc::GetDataSizeCompressed(texture, 0));
    ASSERT_NE(dmTexc::GetDataSizeUncompressed(texture, 0), dmTexc::GetDataSizeCompressed(texture, 0));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_R8G8B8A8, dmTexc::CS_LRGB, dmTexc::CL_FAST, dmTexc::CT_WEBP, dmTexc::DT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char r8g8b8a8[8] = {'r', 'g', 'b', 'a', 8, 8, 8, 8};
    ASSERT_EQ(0, memcmp(r8g8b8a8, (void*)&header.m_PixelFormat, 8));
    ASSERT_NE(0, dmTexc::GetDataSizeCompressed(texture, 0));
    ASSERT_NE(dmTexc::GetDataSizeUncompressed(texture, 0), dmTexc::GetDataSizeCompressed(texture, 0));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_R5G6B5, dmTexc::CS_LRGB, dmTexc::CL_FAST, dmTexc::CT_WEBP, dmTexc::DT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char r5g6b5[8] = {'r', 'g', 'b', 0, 5, 6, 5, 0};
    ASSERT_EQ(0, memcmp(r5g6b5, (void*)&header.m_PixelFormat, 8));
    ASSERT_NE(0, dmTexc::GetDataSizeCompressed(texture, 0));
    ASSERT_NE(dmTexc::GetDataSizeUncompressed(texture, 0), dmTexc::GetDataSizeCompressed(texture, 0));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_R4G4B4A4, dmTexc::CS_LRGB, dmTexc::CL_FAST, dmTexc::CT_WEBP, dmTexc::DT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char r4g4b4a4[8] = {'r', 'g', 'b', 'a', 4, 4, 4, 4};
    ASSERT_EQ(0, memcmp(r4g4b4a4, (void*)&header.m_PixelFormat, 8));
    ASSERT_NE(0, dmTexc::GetDataSizeCompressed(texture, 0));
    ASSERT_NE(dmTexc::GetDataSizeUncompressed(texture, 0), dmTexc::GetDataSizeCompressed(texture, 0));

    dmTexc::Destroy(texture);
}

TEST_F(TexcTest, TranscodeWebPLossy)
{
    dmTexc::HTexture texture = CreateDefaultRGBA32(256, 256);
    dmTexc::Header header;

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_L8, dmTexc::CS_LRGB, dmTexc::CL_FAST, dmTexc::CT_WEBP_LOSSY, dmTexc::DT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char l8[8] = {'l', 0, 0, 0, 8, 0, 0, 0};
    ASSERT_EQ(0, memcmp(l8, (void*)&header.m_PixelFormat, 8));
    ASSERT_NE(0, dmTexc::GetDataSizeCompressed(texture, 0));
    ASSERT_NE(dmTexc::GetDataSizeUncompressed(texture, 0), dmTexc::GetDataSizeCompressed(texture, 0));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_L8A8, dmTexc::CS_LRGB, dmTexc::CL_FAST, dmTexc::CT_WEBP_LOSSY, dmTexc::DT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char l8a8[8] = {'l', 'a', 0, 0, 8, 8, 0, 0};
    ASSERT_EQ(0, memcmp(l8a8, (void*)&header.m_PixelFormat, 8));
    ASSERT_NE(0, dmTexc::GetDataSizeCompressed(texture, 0));
    ASSERT_NE(dmTexc::GetDataSizeUncompressed(texture, 0), dmTexc::GetDataSizeCompressed(texture, 0));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_R8G8B8, dmTexc::CS_LRGB, dmTexc::CL_FAST, dmTexc::CT_WEBP_LOSSY, dmTexc::DT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char r8g8b8[8] = {'r', 'g', 'b', 0, 8, 8, 8, 0};
    ASSERT_EQ(0, memcmp(r8g8b8, (void*)&header.m_PixelFormat, 8));
    ASSERT_NE(0, dmTexc::GetDataSizeCompressed(texture, 0));
    ASSERT_NE(dmTexc::GetDataSizeUncompressed(texture, 0), dmTexc::GetDataSizeCompressed(texture, 0));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_R8G8B8A8, dmTexc::CS_LRGB, dmTexc::CL_FAST, dmTexc::CT_WEBP_LOSSY, dmTexc::DT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char r8g8b8a8[8] = {'r', 'g', 'b', 'a', 8, 8, 8, 8};
    ASSERT_EQ(0, memcmp(r8g8b8a8, (void*)&header.m_PixelFormat, 8));
    ASSERT_NE(0, dmTexc::GetDataSizeCompressed(texture, 0));
    ASSERT_NE(dmTexc::GetDataSizeUncompressed(texture, 0), dmTexc::GetDataSizeCompressed(texture, 0));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_R5G6B5, dmTexc::CS_LRGB, dmTexc::CL_FAST, dmTexc::CT_WEBP_LOSSY, dmTexc::DT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char r5g6b5[8] = {'r', 'g', 'b', 0, 5, 6, 5, 0};
    ASSERT_EQ(0, memcmp(r5g6b5, (void*)&header.m_PixelFormat, 8));
    ASSERT_NE(0, dmTexc::GetDataSizeCompressed(texture, 0));
    ASSERT_NE(dmTexc::GetDataSizeUncompressed(texture, 0), dmTexc::GetDataSizeCompressed(texture, 0));

    ASSERT_TRUE(dmTexc::Transcode(texture, dmTexc::PF_R4G4B4A4, dmTexc::CS_LRGB, dmTexc::CL_FAST, dmTexc::CT_WEBP_LOSSY, dmTexc::DT_DEFAULT));
    dmTexc::GetHeader(texture, &header);
    char r4g4b4a4[8] = {'r', 'g', 'b', 'a', 4, 4, 4, 4};
    ASSERT_EQ(0, memcmp(r4g4b4a4, (void*)&header.m_PixelFormat, 8));
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
    dmTexc::HTexture texture = CreateDefaultRGBA32();

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

static void TranscodeWebEncodedFormat(dmTexc::PixelFormat format, dmWebP::TextureEncodeFormat encode_format)
{
    dmTexc::HTexture texture_default = CreateDefaultRGBA32(256, 256);
    ASSERT_TRUE(dmTexc::PreMultiplyAlpha(texture_default));
    ASSERT_TRUE(dmTexc::Transcode(texture_default, format, dmTexc::CS_LRGB, dmTexc::CL_FAST, dmTexc::CT_DEFAULT, dmTexc::DT_NONE));
    dmTexc::HTexture texture = CreateDefaultRGBA32(256, 256);
    ASSERT_TRUE(dmTexc::PreMultiplyAlpha(texture));
    ASSERT_TRUE(dmTexc::Transcode(texture, format, dmTexc::CS_LRGB, dmTexc::CL_FAST, dmTexc::CT_WEBP, dmTexc::DT_NONE));

    size_t compressed_size = dmTexc::GetDataSizeCompressed(texture, 0);
    uint8_t* compressed_data = new uint8_t[compressed_size];
    dmTexc::GetData(texture, compressed_data, compressed_size);
    size_t uncompressed_size = dmTexc::GetDataSizeUncompressed(texture, 0);
    uint8_t* uncompressed_data = new uint8_t[uncompressed_size];

    dmTexc::Header header;
    dmTexc::GetHeader(texture, &header);
    dmWebP::Result res = dmWebP::DecodeCompressedTexture(compressed_data, compressed_size, uncompressed_data, uncompressed_size,
            uncompressed_size/header.m_Height, encode_format);
    ASSERT_EQ(dmWebP::RESULT_OK, res);

    uint8_t* default_texture_data = new uint8_t[uncompressed_size];
    dmTexc::GetData(texture_default, default_texture_data, uncompressed_size);
    ASSERT_EQ(0, memcmp(uncompressed_data, default_texture_data, uncompressed_size));

    dmTexc::Destroy(texture);
    dmTexc::Destroy(texture_default);
    delete[] compressed_data;
    delete[] uncompressed_data;
    delete[] default_texture_data;
}

TEST_F(TexcTest, TranscodeWebEncodedTextureFormat)
{
    TranscodeWebEncodedFormat(dmTexc::PF_RGBA_PVRTC_4BPPV1, dmWebP::TEXTURE_ENCODE_FORMAT_PVRTC1);
    TranscodeWebEncodedFormat(dmTexc::PF_RGB_PVRTC_4BPPV1, dmWebP::TEXTURE_ENCODE_FORMAT_PVRTC1);
    TranscodeWebEncodedFormat(dmTexc::PF_RGBA_PVRTC_2BPPV1, dmWebP::TEXTURE_ENCODE_FORMAT_PVRTC1);
    TranscodeWebEncodedFormat(dmTexc::PF_RGB_PVRTC_2BPPV1, dmWebP::TEXTURE_ENCODE_FORMAT_PVRTC1);
    TranscodeWebEncodedFormat(dmTexc::PF_RGB_ETC1, dmWebP::TEXTURE_ENCODE_FORMAT_ETC1);
    TranscodeWebEncodedFormat(dmTexc::PF_L8, dmWebP::TEXTURE_ENCODE_FORMAT_L8);
    TranscodeWebEncodedFormat(dmTexc::PF_L8A8, dmWebP::TEXTURE_ENCODE_FORMAT_L8A8);
    TranscodeWebEncodedFormat(dmTexc::PF_R5G6B5, dmWebP::TEXTURE_ENCODE_FORMAT_RGB565);
    TranscodeWebEncodedFormat(dmTexc::PF_R4G4B4A4, dmWebP::TEXTURE_ENCODE_FORMAT_RGBA4444);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
