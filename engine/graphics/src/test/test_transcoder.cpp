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

#include "graphics.h"

#include "test_transcoder_assets.h"

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

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
