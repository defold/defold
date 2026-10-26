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
#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <dlib/array.h>
#include <basis/transcoder/basisu_transcoder.h>

#include "graphics.h"

#include "test_transcoder_assets.h"
#include "test_transcoder_ktx2_assets.h"

// Same limit res_texture.cpp uses for the output arrays it hands to Transcode()
static const uint32_t MAX_MIPMAP_COUNT = 15;

// The transcoder walks m_MipMapSizeCompressed sequentially (mip-major, layer-minor) and never looks
// at m_MipMapOffset, so a synthetic Image with N identical slices is enough to exercise the layout.
// This mirrors how script_resource.cpp builds a throwaway Image for resource.create_texture().
struct TranscodeInput
{
    TranscodeInput(uint32_t mip_count, uint32_t layer_count)
    {
        uint32_t slice_count = mip_count * layer_count;

        m_CompressedSizes.SetCapacity(slice_count);
        m_Bytes.SetCapacity(BLANK_BASIS_SIZE * slice_count);

        for (uint32_t i = 0; i < slice_count; ++i)
        {
            m_CompressedSizes.Push(BLANK_BASIS_SIZE);
            m_Bytes.PushArray(BLANK_BASIS, BLANK_BASIS_SIZE);
        }

        memset(&m_Image, 0, sizeof(m_Image));
        m_Image.m_MipMapSize.m_Count           = mip_count;
        m_Image.m_MipMapSizeCompressed.m_Data  = m_CompressedSizes.Begin();
        m_Image.m_MipMapSizeCompressed.m_Count = m_CompressedSizes.Size();
    }

    dmGraphics::TextureImage::Image m_Image;
    dmArray<uint32_t>               m_CompressedSizes;
    dmArray<uint8_t>                m_Bytes;
};

// Transcodes mip_count * layer_count copies of the same source slice and returns the per-slice size
// reported for mip 0. Every slice must be byte-identical to slice 0 - they all come from the same
// payload, so any disagreement means the slices were not laid out at the reported stride.
static uint32_t TranscodeAndCheckSlices(dmGraphics::TextureFormat format, uint32_t mip_count, uint32_t layer_count)
{
    TranscodeInput input(mip_count, layer_count);

    uint8_t* images[MAX_MIPMAP_COUNT] = {};
    uint32_t sizes[MAX_MIPMAP_COUNT]  = {};
    uint32_t num_mips                 = mip_count;

    if (!dmGraphics::Transcode("test.basis", &input.m_Image, (uint8_t) layer_count, input.m_Bytes.Begin(), format, images, sizes, &num_mips))
    {
        return 0;
    }

    EXPECT_EQ(mip_count, num_mips);

    for (uint32_t mip = 0; mip < num_mips; ++mip)
    {
        EXPECT_NE((uint8_t*) 0, images[mip]);
        EXPECT_EQ(sizes[0], sizes[mip]); // Every mip uses the same payload here
        for (uint32_t layer = 1; layer < layer_count; ++layer)
        {
            EXPECT_EQ(0, memcmp(images[mip], images[mip] + layer * sizes[mip], sizes[mip]));
        }
    }

    uint32_t slice_size = sizes[0];
    for (uint32_t mip = 0; mip < num_mips; ++mip)
    {
        delete[] images[mip];
    }
    return slice_size;
}

// The transcoder always produces RGBA32 for the uncompressed formats and then packs each slice down
// in place. It must report - and lay the slices out at - the *packed* size, i.e. exactly the size
// the rest of the engine computes for that format. Otherwise every layer past the first is read
// from the wrong offset by the graphics adapters, which read array layers as tightly packed slices.
// Regression test for https://github.com/defold/defold/issues/12868
static void AssertSliceSizeMatchesFormat(dmGraphics::TextureFormat format, uint32_t pixel_count, uint32_t mip_count, uint32_t layer_count)
{
    uint32_t expected = pixel_count * (dmGraphics::GetTextureFormatBitsPerPixel(format) / 8);
    ASSERT_EQ(expected, TranscodeAndCheckSlices(format, mip_count, layer_count));
}

// Returns the number of pixels in the test payload, derived from the RGBA transcode so that no test
// has to hardcode the payload dimensions.
static uint32_t GetPayloadPixelCount()
{
    uint32_t rgba_size = TranscodeAndCheckSlices(dmGraphics::TEXTURE_FORMAT_RGBA, 1, 1);
    EXPECT_LT(0U, rgba_size);
    EXPECT_EQ(0U, rgba_size % 4);
    return rgba_size / 4;
}

TEST(Transcode, UncompressedSliceSizeTextureArray)
{
    const uint32_t layer_count = 4;
    uint32_t pixel_count = GetPayloadPixelCount();
    ASSERT_LT(0U, pixel_count);

    AssertSliceSizeMatchesFormat(dmGraphics::TEXTURE_FORMAT_RGBA, pixel_count, 1, layer_count);
    AssertSliceSizeMatchesFormat(dmGraphics::TEXTURE_FORMAT_RGB, pixel_count, 1, layer_count);
    AssertSliceSizeMatchesFormat(dmGraphics::TEXTURE_FORMAT_LUMINANCE_ALPHA, pixel_count, 1, layer_count);
    AssertSliceSizeMatchesFormat(dmGraphics::TEXTURE_FORMAT_LUMINANCE, pixel_count, 1, layer_count);
}

// A single image is the common case (a plain 2D texture) and must report the same per-slice size.
TEST(Transcode, UncompressedSliceSizeSingleImage)
{
    uint32_t pixel_count = GetPayloadPixelCount();
    ASSERT_LT(0U, pixel_count);

    AssertSliceSizeMatchesFormat(dmGraphics::TEXTURE_FORMAT_RGB, pixel_count, 1, 1);
    AssertSliceSizeMatchesFormat(dmGraphics::TEXTURE_FORMAT_LUMINANCE, pixel_count, 1, 1);
}

// Mipmapped array: the slices are stored mip-major / layer-minor, one allocation per mip.
TEST(Transcode, UncompressedSliceSizeMipmappedArray)
{
    uint32_t pixel_count = GetPayloadPixelCount();
    ASSERT_LT(0U, pixel_count);

    AssertSliceSizeMatchesFormat(dmGraphics::TEXTURE_FORMAT_RGB, pixel_count, 3, 4);
    AssertSliceSizeMatchesFormat(dmGraphics::TEXTURE_FORMAT_RGBA, pixel_count, 3, 4);
    AssertSliceSizeMatchesFormat(dmGraphics::TEXTURE_FORMAT_LUMINANCE, pixel_count, 3, 1);
}

TEST(Transcode, RuntimeUASTC)
{
    ASSERT_FALSE(basist::basisu_transcoder_supports_ktx2());
    ASSERT_FALSE(basist::basisu_transcoder_supports_ktx2_zstd());
    basist::basisu_transcoder_init();
    basist::basisu_transcoder transcoder;
    ASSERT_EQ(basist::basis_tex_format::cUASTC_LDR_4x4, transcoder.get_basis_tex_format(UASTC_BASIS, sizeof(UASTC_BASIS)));
    ASSERT_TRUE(transcoder.start_transcoding(UASTC_BASIS, sizeof(UASTC_BASIS)));

    const basist::transcoder_texture_format formats[] = {
        basist::transcoder_texture_format::cTFRGBA32,
        basist::transcoder_texture_format::cTFBC1_RGB,
        basist::transcoder_texture_format::cTFBC3_RGBA,
        basist::transcoder_texture_format::cTFBC4_R,
        basist::transcoder_texture_format::cTFBC5_RG,
        basist::transcoder_texture_format::cTFBC7_RGBA,
        basist::transcoder_texture_format::cTFASTC_4x4_RGBA,
        basist::transcoder_texture_format::cTFETC1_RGB,
        basist::transcoder_texture_format::cTFETC2_RGBA,
        basist::transcoder_texture_format::cTFETC2_EAC_R11,
        basist::transcoder_texture_format::cTFETC2_EAC_RG11,
        basist::transcoder_texture_format::cTFPVRTC1_4_RGB,
        basist::transcoder_texture_format::cTFPVRTC1_4_RGBA,
    };
    for (uint32_t i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i)
    {
        uint8_t decoded[16 * 16 * 4] = {};
        uint32_t count = formats[i] == basist::transcoder_texture_format::cTFRGBA32 ? 16 * 16 : 4 * 4;
        ASSERT_TRUE(basist::basis_is_format_supported(formats[i], basist::basis_tex_format::cUASTC_LDR_4x4));
        ASSERT_TRUE(transcoder.transcode_image_level(UASTC_BASIS, sizeof(UASTC_BASIS), 0, 0, decoded, count, formats[i]));
        if (formats[i] == basist::transcoder_texture_format::cTFRGBA32)
        {
            uint32_t error = 0;
            for (uint32_t y = 0; y < 16; ++y)
            {
                for (uint32_t x = 0; x < 16; ++x)
                {
                    const uint8_t expected[] = {(uint8_t)(x * 16), (uint8_t)(y * 16), (uint8_t)((x + y) * 8)};
                    for (uint32_t c = 0; c < 3; ++c)
                    {
                        int delta = (int)decoded[(y * 16 + x) * 4 + c] - expected[c];
                        error += delta < 0 ? -delta : delta;
                    }
                    ASSERT_EQ(255, decoded[(y * 16 + x) * 4 + 3]);
                }
            }
            ASSERT_LT(error, 16U * 16U * 3U * 20U);
        }
    }
}

TEST(Transcode, RuntimeUASTCAlphaMipmaps)
{
    // Separate .basis payloads exercise the same mip layout used by Defold.
    uint32_t compressed_sizes[] = {sizeof(UASTC_ALPHA_7X5_BASIS), sizeof(UASTC_ALPHA_3X2_BASIS), sizeof(UASTC_ALPHA_1X1_BASIS)};
    const uint32_t widths[] = {7, 3, 1};
    const uint32_t heights[] = {5, 2, 1};
    const uint32_t mip_count = sizeof(widths) / sizeof(widths[0]);
    dmArray<uint8_t> bytes;
    bytes.SetCapacity(compressed_sizes[0] + compressed_sizes[1] + compressed_sizes[2]);
    bytes.PushArray(UASTC_ALPHA_7X5_BASIS, compressed_sizes[0]);
    bytes.PushArray(UASTC_ALPHA_3X2_BASIS, compressed_sizes[1]);
    bytes.PushArray(UASTC_ALPHA_1X1_BASIS, compressed_sizes[2]);

    dmGraphics::TextureImage::Image image = {};
    image.m_MipMapSize.m_Count = mip_count;
    image.m_MipMapSizeCompressed.m_Data = compressed_sizes;
    image.m_MipMapSizeCompressed.m_Count = mip_count;

    const dmGraphics::TextureFormat formats[] = {
        dmGraphics::TEXTURE_FORMAT_RGBA,
        dmGraphics::TEXTURE_FORMAT_RGBA_BC3,
        dmGraphics::TEXTURE_FORMAT_RGBA_BC7,
        dmGraphics::TEXTURE_FORMAT_RGBA_ASTC_4X4,
        dmGraphics::TEXTURE_FORMAT_RGBA_ETC2,
    };
    for (uint32_t i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i)
    {
        uint8_t* images[MAX_MIPMAP_COUNT] = {};
        uint32_t sizes[MAX_MIPMAP_COUNT] = {};
        uint32_t num_mips = mip_count;
        bool success = dmGraphics::Transcode("uastc_alpha.basis", &image, 1, bytes.Begin(), formats[i], images, sizes, &num_mips);
        EXPECT_TRUE(success);
        if (success)
        {
            EXPECT_EQ(mip_count, num_mips);
            for (uint32_t mip = 0; mip < mip_count; ++mip)
            {
                uint32_t width = widths[mip];
                uint32_t height = heights[mip];
                if (formats[i] == dmGraphics::TEXTURE_FORMAT_RGBA)
                {
                    EXPECT_EQ(width * height * 4, sizes[mip]);
                    for (uint32_t y = 0; y < height; ++y)
                    {
                        for (uint32_t x = 0; x < width; ++x)
                        {
                            uint32_t alpha = width + height > 2 ? (x + y) * 255 / (width + height - 2) : 128;
                            const uint32_t expected[] = {64, 128, 192, alpha};
                            for (uint32_t c = 0; c < 4; ++c)
                            {
                                int delta = (int)images[mip][(y * width + x) * 4 + c] - (int)expected[c];
                                EXPECT_LT(delta < 0 ? -delta : delta, 8);
                            }
                        }
                    }
                }
                else
                {
                    // These formats use 16-byte 4x4 blocks, including sub-block mips.
                    EXPECT_EQ(((width + 3) / 4) * ((height + 3) / 4) * 16, sizes[mip]);
                }
            }
        }
        for (uint32_t mip = 0; mip < mip_count; ++mip)
            delete[] images[mip];
    }
}

TEST(Transcode, RuntimeDisabledCodecs)
{
    ASSERT_FALSE(basist::basis_is_format_supported(basist::transcoder_texture_format::cTFBC7_RGBA, basist::basis_tex_format::cXUASTC_LDR_4x4));
    ASSERT_FALSE(basist::basis_is_format_supported(basist::transcoder_texture_format::cTFBC7_RGBA, basist::basis_tex_format::cXUBC7));
    ASSERT_FALSE(basist::basis_is_format_supported(basist::transcoder_texture_format::cTFBC6H, basist::basis_tex_format::cUASTC_HDR_4x4));
    ASSERT_FALSE(basist::basis_is_format_supported(basist::transcoder_texture_format::cTFBC6H, basist::basis_tex_format::cASTC_HDR_6x6));
    ASSERT_FALSE(basist::basis_is_format_supported(basist::transcoder_texture_format::cTFBC6H, basist::basis_tex_format::cUASTC_HDR_6x6_INTERMEDIATE));
}

TEST(Transcode, ImportedKtx2DesktopAndMobileTargets)
{
    const dmGraphics::TextureFormat formats[] = {
        dmGraphics::TEXTURE_FORMAT_RGBA,
        dmGraphics::TEXTURE_FORMAT_RGBA_BC7,
        dmGraphics::TEXTURE_FORMAT_RGBA_ASTC_4X4,
        dmGraphics::TEXTURE_FORMAT_RGBA_ETC2
    };
    const uint8_t* sources[] = {KTX2_UASTC, KTX2_BC7};
    const uint32_t* source_sizes[] = {KTX2_UASTC_SIZES, KTX2_BC7_SIZES};
    for (uint32_t source = 0; source < 2; ++source)
    {
        dmGraphics::TextureImage::Image image;
        memset(&image, 0, sizeof(image));
        image.m_MipMapSize.m_Count = 4;
        image.m_MipMapSizeCompressed.m_Data = (uint32_t*)source_sizes[source];
        image.m_MipMapSizeCompressed.m_Count = 4;
        for (uint32_t format = 0; format < sizeof(formats) / sizeof(formats[0]); ++format)
        {
            uint8_t* images[MAX_MIPMAP_COUNT] = {};
            uint32_t sizes[MAX_MIPMAP_COUNT] = {};
            uint32_t count = 4;
            ASSERT_TRUE(dmGraphics::Transcode("imported.ktx2", &image, 1, (uint8_t*)sources[source], formats[format], images, sizes, &count));
            ASSERT_EQ(4U, count);
            for (uint32_t mip = 0; mip < count; ++mip)
            {
                uint32_t width = dmMath::Max(1U, 8U >> mip);
                uint32_t height = dmMath::Max(1U, 4U >> mip);
                uint32_t expected = formats[format] == dmGraphics::TEXTURE_FORMAT_RGBA
                    ? width * height * 4 : ((width + 3) / 4) * ((height + 3) / 4) * 16;
                ASSERT_NE((uint8_t*)0, images[mip]);
                ASSERT_EQ(expected, sizes[mip]);
                delete[] images[mip];
            }
        }
    }
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
