// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at https://www.defold.com/license

#include <jc_test/jc_test.h>
#include <basis/encoder/basisu_comp.h>
#include <basis/transcoder/basisu_transcoder.h>
#include <dlib/zlib.h>
#include "../texc_private.h"

static void Write32(uint8_t* bytes, uint32_t value)
{
    for (uint32_t i = 0; i < 4; ++i)
        bytes[i] = (uint8_t)(value >> (i * 8));
}

static void MakeRawKtx2(dmArray<uint8_t>& data)
{
    // Two authored levels: a 2x2 RG texture and a deliberately different 1x1 mip.
    const uint8_t magic[] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 13, 10, 26, 10};
    data.SetCapacity(198);
    data.SetSize(198);
    memset(data.Begin(), 0, data.Size());
    uint8_t* p = data.Begin();
    memcpy(p, magic, 12);
    Write32(p + 12, 16); // VK_FORMAT_R8G8_UNORM
    Write32(p + 16, 1);
    Write32(p + 20, 2);
    Write32(p + 24, 2);
    Write32(p + 36, 1);
    Write32(p + 40, 2);
    Write32(p + 48, 128);
    Write32(p + 52, 60);
    Write32(p + 80, 188);
    Write32(p + 88, 8);
    Write32(p + 96, 8);
    Write32(p + 104, 196);
    Write32(p + 112, 2);
    Write32(p + 120, 2);
    Write32(p + 128, 60);
    p[136] = 2;
    p[138] = 56;
    p[140] = 1;
    p[142] = 1;
    p[148] = 2;
    p[158] = 7;
    p[172] = 8;
    p[174] = 7;
    p[175] = 1;
    const uint8_t rg[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    memcpy(p + 188, rg, sizeof(rg));
}

TEST(Ktx2, RawChannelsAndAuthoredMips)
{
    dmArray<uint8_t> data;
    MakeRawKtx2(data);
    dmTexc::Ktx2Info info;
    const char* error = 0;
    dmTexc::Ktx2Texture* texture = dmTexc::LoadKtx2(data.Begin(), data.Size(), &info, &error);
    ASSERT_NE((dmTexc::Ktx2Texture*)0, texture);
    ASSERT_EQ(2U, info.m_Channels);
    ASSERT_EQ(2U, info.m_LevelCount);
    ASSERT_EQ(16U, info.m_VkFormat);
    ASSERT_EQ(0U, info.m_Supercompression);
    dmArray<uint8_t> pixels;
    ASSERT_TRUE(dmTexc::DecodeKtx2Mip(texture, 0, pixels, &error));
    const uint8_t expected[] = {10, 20, 0, 255, 30, 40, 0, 255, 50, 60, 0, 255, 70, 80, 0, 255};
    ASSERT_EQ(sizeof(expected), (size_t)pixels.Size());
    ASSERT_EQ(0, memcmp(expected, pixels.Begin(), sizeof(expected)));
    ASSERT_TRUE(dmTexc::DecodeKtx2Mip(texture, 1, pixels, &error));
    ASSERT_EQ(90, pixels[0]);
    ASSERT_EQ(100, pixels[1]);
    ASSERT_FALSE(dmTexc::DecodeKtx2Mip(texture, 2, pixels, &error));
    dmTexc::DestroyKtx2(texture);
}

TEST(Ktx2, RejectMalformedAndUnsupportedInput)
{
    dmTexc::Ktx2Info info;
    const char* error = 0;
    const uint32_t offsets[] = {12, 16, 20, 28, 32, 36, 40, 44, 48, 52, 80, 84, 88, 96};
    for (uint32_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i)
    {
        dmArray<uint8_t> data;
        MakeRawKtx2(data);
        Write32(data.Begin() + offsets[i], 0xffffffff);
        dmTexc::Ktx2Texture* texture = dmTexc::LoadKtx2(data.Begin(), data.Size(), &info, &error);
        ASSERT_EQ((dmTexc::Ktx2Texture*)0, texture);
        ASSERT_NE((const char*)0, error);
    }
    dmArray<uint8_t> data;
    MakeRawKtx2(data);
    for (uint32_t size = 0; size < data.Size(); ++size)
        ASSERT_EQ((dmTexc::Ktx2Texture*)0, dmTexc::LoadKtx2(data.Begin(), size, &info, &error));
}

TEST(Ktx2, BasisDecodeRecompressAndLosslessRepack)
{
    basisu::basisu_encoder_init();
    for (uint32_t variant = 0; variant < 8; ++variant)
    {
        bool uastc = (variant & 1) != 0;
        uint8_t alpha = (variant & 2) ? 128 : 255;
        bool srgb = (variant & 4) != 0;
        basisu::basis_compressor_params params;
        basisu::job_pool jobs(1);
        params.m_pJob_pool = &jobs;
        params.m_read_source_images = false;
        params.m_write_output_basis_or_ktx2_files = false;
        params.m_status_output = false;
        params.m_create_ktx2_file = true;
        params.m_ktx2_uastc_supercompression = basist::KTX2_SS_NONE;
        params.set_format_mode(uastc ? basist::basis_tex_format::cUASTC_LDR_4x4 : basist::basis_tex_format::cETC1S);
        params.set_srgb_options(srgb);
        basist::add_key_value(params.m_key_values, "KTXorientation", "ru");
        params.m_quality_level = 128;
        params.m_source_images.push_back(basisu::image(8, 4));
        for (uint32_t y = 0; y < 4; ++y)
            for (uint32_t x = 0; x < 8; ++x)
                params.m_source_images[0](x, y) = basisu::color_rgba(64, 128, 192, alpha);
        basisu::basis_compressor compressor;
        ASSERT_TRUE(compressor.init(params));
        ASSERT_EQ(basisu::basis_compressor::cECSuccess, compressor.process());
        const basisu::uint8_vec& ktx2 = compressor.get_output_ktx2_file();
        dmTexc::Ktx2Info info;
        const char* error = 0;
        dmTexc::Ktx2Texture* texture = dmTexc::LoadKtx2(ktx2.data(), ktx2.size(), &info, &error);
        ASSERT_NE((dmTexc::Ktx2Texture*)0, texture);
        ASSERT_EQ(8U, info.m_Width);
        ASSERT_EQ(4U, info.m_Height);
        ASSERT_EQ(srgb, info.m_Srgb);
        ASSERT_FALSE(info.m_FlipX);
        ASSERT_TRUE(info.m_FlipY);
        dmArray<uint8_t> pixels;
        ASSERT_TRUE(dmTexc::DecodeKtx2Mip(texture, 0, pixels, &error));
        ASSERT_NEAR(64, pixels[0], 4);
        ASSERT_NEAR(128, pixels[1], 4);
        ASSERT_NEAR(192, pixels[2], 4);
        ASSERT_NEAR(alpha, pixels[3], 1);

        // Recompression preserves the transfer function and does not gamma-convert data.
        dmTexc::BasisUEncodeSettings settings;
        memset(&settings, 0, sizeof(settings));
        settings.m_Path = "KTX2";
        settings.m_Width = info.m_Width;
        settings.m_Height = info.m_Height;
        settings.m_PixelFormat = dmTexc::PF_R8G8B8A8;
        settings.m_OutPixelFormat = dmTexc::PF_R8G8B8A8;
        settings.m_ColorSpace = info.m_Srgb ? dmTexc::CS_SRGB : dmTexc::CS_LRGB;
        settings.m_Data = pixels.Begin();
        settings.m_DataCount = pixels.Size();
        settings.m_NumThreads = 1;
        settings.m_pack_uastc_flags = 2;
        uint8_t* encoded = 0;
        uint32_t encoded_size = 0;
        ASSERT_TRUE(dmTexc::BasisUEncode(&settings, &encoded, &encoded_size));
        basist::basisu_transcoder recompressed;
        basist::basisu_file_info recompressed_info;
        ASSERT_TRUE(recompressed.get_file_info(encoded, encoded_size, recompressed_info));
        ASSERT_EQ(srgb, recompressed_info.m_srgb);
        ASSERT_TRUE(recompressed.start_transcoding(encoded, encoded_size));
        uint8_t roundtrip[8 * 4 * 4];
        ASSERT_TRUE(recompressed.transcode_image_level(encoded, encoded_size, 0, 0, roundtrip, 32, basist::transcoder_texture_format::cTFRGBA32));
        for (uint32_t i = 0; i < pixels.Size(); ++i)
            ASSERT_NEAR(pixels[i], roundtrip[i], 4);
        free(encoded);

        dmArray<uint8_t> same_format;
        ASSERT_TRUE(dmTexc::EncodeKtx2Mip(texture, 8, 4, pixels.Begin(), pixels.Size(), same_format, &error));
        basist::basisu_transcoder same_format_decoder;
        ASSERT_TRUE(same_format_decoder.get_file_info(same_format.Begin(), same_format.Size(), recompressed_info));
        ASSERT_EQ(srgb, recompressed_info.m_srgb);
        ASSERT_EQ(uastc ? basist::basis_tex_format::cUASTC_LDR_4x4 : basist::basis_tex_format::cETC1S, recompressed_info.m_tex_format);
        ASSERT_TRUE(same_format_decoder.start_transcoding(same_format.Begin(), same_format.Size()));
        ASSERT_TRUE(same_format_decoder.transcode_image_level(same_format.Begin(), same_format.Size(), 0, 0, roundtrip, 32, basist::transcoder_texture_format::cTFRGBA32));
        for (uint32_t i = 0; i < pixels.Size(); ++i)
            ASSERT_NEAR(pixels[i], roundtrip[i], 4);

        {
            dmArray<uint8_t> repacked;
            ASSERT_TRUE(dmTexc::RepackKtx2Mip(texture, 0, repacked, &error));
            basist::basisu_transcoder transcoder;
            ASSERT_TRUE(transcoder.validate_file_checksums(repacked.Begin(), repacked.Size(), true));
            basist::basisu_file_info file_info;
            ASSERT_TRUE(transcoder.get_file_info(repacked.Begin(), repacked.Size(), file_info));
            ASSERT_EQ(srgb, file_info.m_srgb);
            ASSERT_EQ(uastc ? basist::basis_tex_format::cUASTC_LDR_4x4 : basist::basis_tex_format::cETC1S, file_info.m_tex_format);
            ASSERT_TRUE(transcoder.start_transcoding(repacked.Begin(), repacked.Size()));
            uint8_t decoded[8 * 4 * 4];
            ASSERT_TRUE(transcoder.transcode_image_level(repacked.Begin(), repacked.Size(), 0, 0, decoded, 32, basist::transcoder_texture_format::cTFRGBA32));
            ASSERT_EQ(0, memcmp(decoded, pixels.Begin(), sizeof(decoded)));
            // Repacking must retain compressed slices and ETC1S codebooks byte for byte.
            const basist::basis_file_header* header = (const basist::basis_file_header*)repacked.Begin();
            const basist::basis_slice_desc* slice = (const basist::basis_slice_desc*)(repacked.Begin() + (uint32_t)header->m_slice_desc_file_ofs);
            basist::ktx2_transcoder source;
            ASSERT_TRUE(source.init(ktx2.data(), ktx2.size()));
            const basist::ktx2_level_index& level = source.get_level_index()[0];
            if (uastc)
            {
                ASSERT_EQ(level.m_byte_length.get_uint64(), (uint64_t)(uint32_t)slice->m_file_size);
                ASSERT_EQ(0, memcmp(ktx2.data() + level.m_byte_offset.get_uint64(), repacked.Begin() + (uint32_t)slice->m_file_ofs, (uint32_t)slice->m_file_size));
            }
            else
            {
                ASSERT_TRUE(source.start_transcoding());
                const basist::ktx2_etc1s_image_desc& desc = source.get_etc1s_image_descs()[0];
                ASSERT_EQ((uint32_t)desc.m_rgb_slice_byte_length, (uint32_t)slice[0].m_file_size);
                ASSERT_EQ(0, memcmp(ktx2.data() + level.m_byte_offset.get_uint64() + (uint32_t)desc.m_rgb_slice_byte_offset,
                                    repacked.Begin() + (uint32_t)slice[0].m_file_ofs, (uint32_t)slice[0].m_file_size));
                if (source.get_has_alpha())
                {
                    ASSERT_EQ((uint32_t)desc.m_alpha_slice_byte_length, (uint32_t)slice[1].m_file_size);
                    ASSERT_EQ(0, memcmp(ktx2.data() + level.m_byte_offset.get_uint64() + (uint32_t)desc.m_alpha_slice_byte_offset,
                                        repacked.Begin() + (uint32_t)slice[1].m_file_ofs, (uint32_t)slice[1].m_file_size));
                }
                const basist::ktx2_etc1s_global_data_header& global = source.get_etc1s_header();
                const uint8_t* codebooks = ktx2.data() + source.get_header().m_sgd_byte_offset.get_uint64()
                                        + sizeof(global) + source.get_etc1s_image_descs().size() * sizeof(desc);
                ASSERT_EQ((uint32_t)global.m_endpoints_byte_length, (uint32_t)header->m_endpoint_cb_file_size);
                ASSERT_EQ(0, memcmp(codebooks, repacked.Begin() + (uint32_t)header->m_endpoint_cb_file_ofs, (uint32_t)global.m_endpoints_byte_length));
                codebooks += global.m_endpoints_byte_length;
                ASSERT_EQ((uint32_t)global.m_selectors_byte_length, (uint32_t)header->m_selector_cb_file_size);
                ASSERT_EQ(0, memcmp(codebooks, repacked.Begin() + (uint32_t)header->m_selector_cb_file_ofs, (uint32_t)global.m_selectors_byte_length));
                codebooks += global.m_selectors_byte_length;
                ASSERT_EQ((uint32_t)global.m_tables_byte_length, (uint32_t)header->m_tables_file_size);
                ASSERT_EQ(0, memcmp(codebooks, repacked.Begin() + (uint32_t)header->m_tables_file_ofs, (uint32_t)global.m_tables_byte_length));
            }
        }
        dmTexc::DestroyKtx2(texture);
        if (alpha == 128)
        {
            // The same compressed slices can represent R/G data instead of color/alpha.
            dmArray<uint8_t> rg;
            rg.SetCapacity(ktx2.size());
            rg.SetSize(ktx2.size());
            memcpy(rg.Begin(), ktx2.data(), ktx2.size());
            uint32_t dfd_offset = (uint32_t)((const basist::ktx2_header*)ktx2.data())->m_dfd_byte_offset;
            rg[dfd_offset + 31] = uastc ? 5 : 3; // UASTC RRRG / ETC1S RRR
            if (!uastc)
                rg[dfd_offset + 47] = 4; // ETC1S GGG
            texture = dmTexc::LoadKtx2(rg.Begin(), rg.Size(), &info, &error);
            ASSERT_NE((dmTexc::Ktx2Texture*)0, texture);
            ASSERT_EQ(2U, info.m_Channels);
            ASSERT_FALSE(info.m_CanRepack); // Requires channel processing, so cannot preserve blocks.
            ASSERT_TRUE(dmTexc::DecodeKtx2Mip(texture, 0, pixels, &error));
            ASSERT_NEAR(64, pixels[0], 4);
            ASSERT_NEAR(128, pixels[1], 1);
            ASSERT_EQ(0, pixels[2]);
            ASSERT_EQ(255, pixels[3]);
            dmTexc::DestroyKtx2(texture);
        }
    }
}
