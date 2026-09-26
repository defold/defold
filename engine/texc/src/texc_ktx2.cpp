// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at https://www.defold.com/license

#include "texc_private.h"

#include <basis/transcoder/basisu_transcoder.h>
#include <basis/transcoder/basisu_transcoder_uastc.h>
#include <basis/encoder/basisu_basis_file.h>
#include <basis/encoder/basisu_bc7e_scalar.h>
#include <basis/encoder/basisu_comp.h>
#include <basis/zstd/zstd.h>
#include <dlib/zlib.h>
#include <string.h>

namespace dmTexc
{
    static const uint8_t KTX2_MAGIC[] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};
    static const uint64_t MAX_DECODED_SIZE = 512ULL * 1024 * 1024;

    struct Ktx2Level
    {
        uint32_t m_Offset;
        uint32_t m_Size;
        uint32_t m_UncompressedSize;
    };

    struct Ktx2Texture
    {
        Ktx2Info m_Info;
        dmArray<uint8_t> m_Data;
        Ktx2Level m_Levels[16];
        uint32_t m_Format;
        uint32_t m_Supercompression;
        uint32_t m_Model;
        uint32_t m_Channel0;
        char m_Swizzle[4];
        basist::ktx2_transcoder m_Transcoder;
        bool m_Transcoding;
        bool m_Uastc;
    };

    static uint32_t Read32(const uint8_t* p)
    {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }

    static uint64_t Read64(const uint8_t* p)
    {
        return Read32(p) | ((uint64_t)Read32(p + 4) << 32);
    }

    static bool Fail(const char** error, const char* message)
    {
        *error = message;
        return false;
    }

    static bool Range(uint64_t offset, uint64_t length, uint32_t size)
    {
        return offset <= size && length <= size - offset;
    }

    static bool Overlaps(uint64_t offset, uint64_t length, uint64_t other_offset, uint64_t other_length)
    {
        return length && other_length && offset < other_offset + other_length && other_offset < offset + length;
    }

    static uint32_t Dimension(uint32_t size, uint32_t level)
    {
        uint32_t result = size >> level;
        return result ? result : 1;
    }

    static bool ApplyMetadata(Ktx2Texture* texture, const char* key, const char* value, uint32_t value_size, const char** error)
    {
        if (!strcmp(key, "KTXorientation"))
        {
            if (value_size != 3 || value[2] || (value[0] != 'r' && value[0] != 'l') || (value[1] != 'd' && value[1] != 'u'))
                return Fail(error, "Unsupported KTX2 orientation");
            texture->m_Info.m_FlipX = value[0] == 'l';
            texture->m_Info.m_FlipY = value[1] == 'u';
        }
        else if (!strcmp(key, "KTXswizzle"))
        {
            if (value_size != 5 || value[4])
                return Fail(error, "Invalid KTX2 swizzle");
            for (uint32_t i = 0; i < 4; ++i)
            {
                if (!value[i] || !strchr("rgba01", value[i]))
                    return Fail(error, "Unsupported KTX2 swizzle");
            }
            memcpy(texture->m_Swizzle, value, 4);
        }
        return true;
    }

    static bool ReadMetadata(Ktx2Texture* texture, uint32_t offset, uint32_t length, const char** error)
    {
        const uint8_t* data = texture->m_Data.Begin();
        uint32_t end = offset + length;
        while (offset < end)
        {
            if (end - offset < 4)
                return Fail(error, "Truncated KTX2 metadata");
            uint32_t size = Read32(data + offset);
            offset += 4;
            if (!size || size > end - offset)
                return Fail(error, "Invalid KTX2 metadata length");
            const char* key = (const char*)(data + offset);
            const char* separator = (const char*)memchr(key, 0, size);
            if (!separator)
                return Fail(error, "Invalid KTX2 metadata key");
            const char* value = separator + 1;
            uint32_t value_size = size - (uint32_t)(value - key);
            if (!ApplyMetadata(texture, key, value, value_size, error))
                return false;
            uint64_t padded_size = ((uint64_t)size + 3) & ~3ULL;
            if (padded_size > end - offset)
                return Fail(error, "Invalid KTX2 metadata padding");
            offset += (uint32_t)padded_size;
        }
        return offset == end || Fail(error, "Invalid KTX2 metadata padding");
    }

    static bool UnwrapZlib(Ktx2Texture* texture, const char** error);

    static bool ReadBasisKtx2(Ktx2Texture* texture, const char** error)
    {
        // BasisU does not handle Zlib wrapping. Remove it without decoding the UASTC blocks.
        if (texture->m_Supercompression == 3 && !UnwrapZlib(texture, error))
            return false;
        basist::ktx2_transcoder& reader = texture->m_Transcoder;
        if (!reader.init(texture->m_Data.Begin(), texture->m_Data.Size()))
            return Fail(error, "Invalid Basis KTX2 data");
        if (!reader.is_etc1s() && !reader.is_uastc())
            return Fail(error, "Unsupported KTX2 Basis codec");

        Ktx2Info& info = texture->m_Info;
        info.m_Width = reader.get_width();
        info.m_Height = reader.get_height();
        info.m_LevelCount = reader.get_levels();
        info.m_Srgb = reader.is_srgb();
        info.m_Premultiplied = (reader.get_dfd_flags() & 1) != 0;
        texture->m_Channel0 = reader.get_dfd_channel_id0();
        texture->m_Uastc = reader.is_uastc();
        if (texture->m_Uastc)
        {
            switch (texture->m_Channel0)
            {
            case basist::KTX2_DF_CHANNEL_UASTC_RGB: info.m_Channels = 3; break;
            case basist::KTX2_DF_CHANNEL_UASTC_RGBA: info.m_Channels = 4; break;
            case basist::KTX2_DF_CHANNEL_UASTC_RRR: info.m_Channels = 1; break;
            case basist::KTX2_DF_CHANNEL_UASTC_RRRG: case basist::KTX2_DF_CHANNEL_UASTC_RG: info.m_Channels = 2; break;
            default: return Fail(error, "Unsupported UASTC channel mapping");
            }
        }
        else
        {
            uint32_t channel1 = reader.get_dfd_channel_id1();
            bool two_planes = reader.get_dfd_total_samples() == 2;
            if ((texture->m_Channel0 != basist::KTX2_DF_CHANNEL_ETC1S_RGB && texture->m_Channel0 != basist::KTX2_DF_CHANNEL_ETC1S_RRR) ||
                (two_planes && channel1 != basist::KTX2_DF_CHANNEL_ETC1S_GGG && channel1 != basist::KTX2_DF_CHANNEL_ETC1S_AAA))
                return Fail(error, "Unsupported ETC1S channel mapping");
            info.m_Channels = texture->m_Channel0 == basist::KTX2_DF_CHANNEL_ETC1S_RRR ? (two_planes ? 2 : 1) : (two_planes ? 4 : 3);
        }
        for (uint32_t i = 0; i < info.m_LevelCount; ++i)
        {
            const basist::ktx2_level_index& level = reader.get_level_index()[i];
            texture->m_Levels[i].m_Offset = (uint32_t)level.m_byte_offset.get_uint64();
            texture->m_Levels[i].m_Size = (uint32_t)level.m_byte_length.get_uint64();
            texture->m_Levels[i].m_UncompressedSize = (uint32_t)level.m_uncompressed_byte_length.get_uint64();
        }
        const basist::key_value_vec& metadata = reader.get_key_values();
        for (uint32_t i = 0; i < metadata.size(); ++i)
        {
            const basist::key_value& entry = metadata[i];
            // BasisU appends a terminator beyond the value bytes stored in KTX2.
            if (!ApplyMetadata(texture, (const char*)entry.m_key.data(), (const char*)entry.m_value.data(), entry.m_value.size() - 1, error))
                return false;
        }
        return true;
    }

    static bool ReadKtx2(Ktx2Texture* texture, const uint8_t* data, uint32_t size, const char** error)
    {
        if (size < 80 || memcmp(data, KTX2_MAGIC, sizeof(KTX2_MAGIC)))
            return Fail(error, "Invalid KTX2 header");
        Ktx2Info& info = texture->m_Info;
        memset(&info, 0, sizeof(info));
        texture->m_Format = Read32(data + 12);
        info.m_VkFormat = texture->m_Format;
        info.m_Width = Read32(data + 20);
        info.m_Height = Read32(data + 24);
        info.m_LevelCount = Read32(data + 40);
        if (!info.m_LevelCount && texture->m_Format > 0 && texture->m_Format < 145)
            info.m_LevelCount = 1; // Uncompressed containers may request generated mipmaps.
        texture->m_Supercompression = Read32(data + 44);
        info.m_Supercompression = texture->m_Supercompression;
        texture->m_Transcoding = false;
        memcpy(texture->m_Swizzle, "rgba", 4);
        if (!info.m_Width || !info.m_Height || Read32(data + 28) || Read32(data + 32) || Read32(data + 36) != 1)
            return Fail(error, "Only non-array 2D KTX2 textures are supported");
        if (Read32(data + 16) != 1)
            return Fail(error, "Only 8-bit and supported block-compressed KTX2 textures are supported");
        if (!info.m_LevelCount || info.m_LevelCount > 16 || !Range(80, 24ULL * info.m_LevelCount, size))
            return Fail(error, "Invalid KTX2 mip level count");
        uint32_t max_levels = 1;
        for (uint32_t n = info.m_Width > info.m_Height ? info.m_Width : info.m_Height; n > 1; n >>= 1)
            ++max_levels;
        if (info.m_LevelCount > max_levels || info.m_Width > 65535 || info.m_Height > 65535)
            return Fail(error, "Invalid KTX2 dimensions or mip level count");

        uint32_t dfd_offset = Read32(data + 48), dfd_size = Read32(data + 52);
        uint32_t kvd_offset = Read32(data + 56), kvd_size = Read32(data + 60);
        uint64_t sgd_offset = Read64(data + 64), sgd_size = Read64(data + 72);
        uint32_t header_size = 80 + info.m_LevelCount * 24;
        if (dfd_offset < header_size || !Range(dfd_offset, dfd_size, size) || dfd_size < 44 ||
            (kvd_size && (kvd_offset < header_size || !Range(kvd_offset, kvd_size, size))) ||
            (sgd_size && (sgd_offset < header_size || !Range(sgd_offset, sgd_size, size))))
            return Fail(error, "Invalid KTX2 descriptor or metadata range");
        if (Overlaps(dfd_offset, dfd_size, kvd_offset, kvd_size) ||
            Overlaps(dfd_offset, dfd_size, sgd_offset, sgd_size) ||
            Overlaps(kvd_offset, kvd_size, sgd_offset, sgd_size))
            return Fail(error, "Overlapping KTX2 metadata");
        const uint8_t* dfd = data + dfd_offset;
        uint32_t block_size = dfd[10] | ((uint32_t)dfd[11] << 8);
        if (Read32(dfd) != dfd_size || Read32(dfd + 4) != 0 || dfd[8] != 2 || dfd[9] || block_size + 4 != dfd_size || (block_size - 24) % 16)
            return Fail(error, "Unsupported KTX2 data format descriptor");
        if ((dfd[14] != 1 && dfd[14] != 2) || (dfd[15] & ~1U))
            return Fail(error, "Unsupported KTX2 transfer function or alpha flags");
        if (dfd[13] > 1)
            return Fail(error, "Unsupported KTX2 color primaries; expected unspecified or BT.709");
        info.m_Srgb = dfd[14] == 2;
        info.m_Premultiplied = (dfd[15] & 1) != 0;
        texture->m_Model = dfd[12];
        texture->m_Uastc = texture->m_Format == 0 && texture->m_Model == 166;
        bool etc1s = texture->m_Format == 0 && texture->m_Model == 163;
        if (!texture->m_Format)
        {
            if (etc1s)
            {
                if (dfd_size != 44 && dfd_size != 60)
                    return Fail(error, "Unsupported ETC1S descriptor count");
                if (texture->m_Supercompression != 1 || !sgd_size)
                    return Fail(error, "ETC1S KTX2 requires BasisLZ global data");
            }
            else if (texture->m_Uastc)
            {
                if (dfd_size != 44)
                    return Fail(error, "Unsupported UASTC descriptor count");
                if (texture->m_Supercompression != 0 && texture->m_Supercompression != 2 && texture->m_Supercompression != 3)
                    return Fail(error, "Unsupported UASTC supercompression");
            }
            else
                return Fail(error, "Unsupported KTX2 Basis codec");
        }
        else
        {
            switch (texture->m_Format)
            {
            case 9: case 15: info.m_Channels = 1; break; // R8 UNORM/SRGB
            case 16: case 22: info.m_Channels = 2; break; // RG8
            case 23: case 29: info.m_Channels = 3; break; // RGB8
            case 37: case 43: info.m_Channels = 4; break; // RGBA8
            case 145: case 146: info.m_Channels = 4; break; // BC7
            default: return Fail(error, "Unsupported KTX2 vkFormat; expected 8-bit R/RG/RGB/RGBA or BC7");
            }
            if (texture->m_Supercompression != 0 && texture->m_Supercompression != 2 && texture->m_Supercompression != 3)
                return Fail(error, "Unsupported KTX2 supercompression");
            bool srgb = texture->m_Format == 15 || texture->m_Format == 22 || texture->m_Format == 29 || texture->m_Format == 43 || texture->m_Format == 146;
            if (srgb != info.m_Srgb || texture->m_Model != (texture->m_Format >= 145 ? 134 : 1))
                return Fail(error, "KTX2 vkFormat conflicts with its data format descriptor");
        }
        bool block_compressed = !texture->m_Format || texture->m_Format >= 145;
        if (dfd[16] != (block_compressed ? 3 : 0) || dfd[17] != (block_compressed ? 3 : 0) || dfd[18] || dfd[19])
            return Fail(error, "Unsupported KTX2 texel block dimensions");
        if (!etc1s && sgd_size)
            return Fail(error, "Unexpected KTX2 global data");
        if (texture->m_Format && texture->m_Format < 145)
        {
            if (dfd_size != 28 + 16 * info.m_Channels)
                return Fail(error, "Invalid KTX2 channel descriptor count");
            for (uint32_t channel = 0; channel < info.m_Channels; ++channel)
            {
                const uint8_t* sample = dfd + 28 + 16 * channel;
                if (sample[0] != channel * 8 || sample[1] || sample[2] != 7 || (sample[3] & 0xE0) ||
                    (sample[3] & 15) != (channel == 3 ? 15 : channel))
                    return Fail(error, "Unsupported KTX2 channel layout");
            }
        }
        uint64_t decoded_size = 0;
        // Validate all ranges before giving any input to third-party decoders.
        for (uint32_t i = 0; i < info.m_LevelCount; ++i)
        {
            const uint8_t* index = data + 80 + i * 24;
            uint64_t offset = Read64(index), length = Read64(index + 8), unpacked = Read64(index + 16);
            uint32_t width = Dimension(info.m_Width, i), height = Dimension(info.m_Height, i);
            decoded_size += (uint64_t)width * height * 4;
            if (decoded_size > MAX_DECODED_SIZE)
                return Fail(error, "KTX2 exceeds the 512 MiB decoded size limit");
            if (offset < header_size || !length || !Range(offset, length, size) || unpacked > MAX_DECODED_SIZE)
                return Fail(error, "Invalid KTX2 mip level range");
            if (Overlaps(offset, length, dfd_offset, dfd_size) || Overlaps(offset, length, kvd_offset, kvd_size) || Overlaps(offset, length, sgd_offset, sgd_size))
                return Fail(error, "KTX2 mip overlaps metadata");
            for (uint32_t previous = 0; previous < i; ++previous)
                if (Overlaps(offset, length, texture->m_Levels[previous].m_Offset, texture->m_Levels[previous].m_Size))
                    return Fail(error, "Overlapping KTX2 mip levels");
            if (!etc1s)
            {
                uint64_t expected = (texture->m_Uastc || texture->m_Format >= 145)
                    ? (uint64_t)((width + 3) / 4) * ((height + 3) / 4) * 16
                    : (uint64_t)width * height * info.m_Channels;
                if (unpacked != expected || (!texture->m_Supercompression && length != expected))
                    return Fail(error, "Invalid KTX2 mip level byte length");
            }
            texture->m_Levels[i].m_Offset = (uint32_t)offset;
            texture->m_Levels[i].m_Size = (uint32_t)length;
            texture->m_Levels[i].m_UncompressedSize = (uint32_t)unpacked;
        }
        texture->m_Data.SetCapacity(size);
        texture->m_Data.SetSize(size);
        memcpy(texture->m_Data.Begin(), data, size);
        return texture->m_Format ? ReadMetadata(texture, kvd_offset, kvd_size, error) : ReadBasisKtx2(texture, error);
    }

    Ktx2Texture* LoadKtx2(const uint8_t* data, uint32_t size, Ktx2Info* info, const char** error)
    {
        Ktx2Texture* texture = new Ktx2Texture;
        if (!ReadKtx2(texture, data, size, error))
        {
            delete texture;
            return 0;
        }
        *info = texture->m_Info;
        // Only RGB/RGBA with identity swizzle can be repacked without channel processing.
        info->m_CanRepack = (!texture->m_Format || texture->m_Format >= 145) && info->m_Channels >= 3 && !memcmp(texture->m_Swizzle, "rgba", 4);
        if (memcmp(texture->m_Swizzle, "rgba", 4))
            info->m_Channels = 4;
        return texture;
    }

    void DestroyKtx2(Ktx2Texture* texture)
    {
        delete texture;
    }

    struct InflateContext
    {
        uint8_t* m_Data;
        uint32_t m_Capacity;
        uint32_t m_Size;
    };

    static bool WriteInflated(void* context, const void* data, uint32_t size)
    {
        InflateContext* output = (InflateContext*)context;
        if (size > output->m_Capacity - output->m_Size)
            return false;
        memcpy(output->m_Data + output->m_Size, data, size);
        output->m_Size += size;
        return true;
    }

    static bool ReadLevel(Ktx2Texture* texture, uint32_t level, dmArray<uint8_t>& bytes, const char** error)
    {
        const Ktx2Level& mip = texture->m_Levels[level];
        const uint8_t* source = texture->m_Data.Begin() + mip.m_Offset;
        bytes.SetCapacity(mip.m_UncompressedSize);
        bytes.SetSize(mip.m_UncompressedSize);
        if (!texture->m_Supercompression)
            memcpy(bytes.Begin(), source, bytes.Size());
        else if (texture->m_Supercompression == 2)
        {
            size_t size = ZSTD_decompress(bytes.Begin(), bytes.Size(), source, mip.m_Size);
            if (ZSTD_isError(size) || size != bytes.Size())
                return Fail(error, "Invalid KTX2 Zstd mip data");
        }
        else
        {
            InflateContext output = {bytes.Begin(), bytes.Size(), 0};
            if (dmZlib::InflateBuffer(source, mip.m_Size, &output, WriteInflated) != dmZlib::RESULT_OK || output.m_Size != bytes.Size())
                return Fail(error, "Invalid KTX2 Zlib mip data");
        }
        return true;
    }

    static bool UnwrapZlib(Ktx2Texture* texture, const char** error)
    {
        const basist::ktx2_header* source_header = (const basist::ktx2_header*)texture->m_Data.Begin();
        uint32_t prefix_size = (uint32_t)source_header->m_dfd_byte_offset + (uint32_t)source_header->m_dfd_byte_length;
        if (source_header->m_kvd_byte_length)
            prefix_size = (uint32_t)source_header->m_kvd_byte_offset + (uint32_t)source_header->m_kvd_byte_length;
        uint64_t total_size = ((uint64_t)prefix_size + 15) & ~15ULL;
        for (uint32_t i = 0; i < texture->m_Info.m_LevelCount; ++i)
            total_size += texture->m_Levels[i].m_UncompressedSize;
        if (total_size > UINT32_MAX)
            return Fail(error, "Unwrapped KTX2 exceeds the container size limit");
        dmArray<uint8_t> data;
        data.SetCapacity((uint32_t)total_size);
        data.SetSize((uint32_t)total_size);
        memset(data.Begin(), 0, data.Size());
        memcpy(data.Begin(), texture->m_Data.Begin(), prefix_size);
        basist::ktx2_header* header = (basist::ktx2_header*)data.Begin();
        header->m_supercompression_scheme = basist::KTX2_SS_NONE;
        basist::ktx2_level_index* levels = (basist::ktx2_level_index*)(data.Begin() + sizeof(*header));
        uint32_t offset = (prefix_size + 15) & ~15U;
        for (uint32_t i = texture->m_Info.m_LevelCount; i > 0; --i)
        {
            dmArray<uint8_t> blocks;
            if (!ReadLevel(texture, i - 1, blocks, error))
                return false;
            memcpy(data.Begin() + offset, blocks.Begin(), blocks.Size());
            levels[i - 1].m_byte_offset = offset;
            levels[i - 1].m_byte_length = blocks.Size();
            offset += blocks.Size();
        }
        texture->m_Data.Swap(data);
        texture->m_Supercompression = basist::KTX2_SS_NONE;
        return true;
    }

    static bool RepackEtc1sMip(Ktx2Texture* texture, uint32_t level, basisu::basisu_backend_output& output, const char** error)
    {
        basist::ktx2_transcoder& reader = texture->m_Transcoder;
        if (!texture->m_Transcoding)
        {
            if (!reader.start_transcoding())
                return Fail(error, "Invalid ETC1S KTX2 global data");
            texture->m_Transcoding = true;
        }
        if (reader.is_video())
            return Fail(error, "ETC1S video cannot be repacked as independent texture mips");

        const basist::ktx2_etc1s_global_data_header& header = reader.get_etc1s_header();
        const basist::ktx2_etc1s_image_desc& image = reader.get_etc1s_image_descs()[level];
        uint64_t offset = reader.get_header().m_sgd_byte_offset.get_uint64()
                        + sizeof(header) + reader.get_etc1s_image_descs().size() * sizeof(image);
        uint64_t size = (uint64_t)header.m_endpoints_byte_length + header.m_selectors_byte_length + header.m_tables_byte_length;
        if (!Range(offset, size, texture->m_Data.Size()))
            return Fail(error, "Invalid ETC1S KTX2 codebook range");

        const uint8_t* data = texture->m_Data.Begin() + offset;
        output.m_tex_format = basist::basis_tex_format::cETC1S;
        output.m_etc1s = true;
        output.m_num_endpoints = header.m_endpoint_count;
        output.m_num_selectors = header.m_selector_count;
        output.m_endpoint_palette.append(data, header.m_endpoints_byte_length);
        data += header.m_endpoints_byte_length;
        output.m_selector_palette.append(data, header.m_selectors_byte_length);
        data += header.m_selectors_byte_length;
        output.m_slice_image_tables.append(data, header.m_tables_byte_length);

        const Ktx2Level& mip = texture->m_Levels[level];
        uint32_t slices = reader.get_has_alpha() ? 2 : 1;
        for (uint32_t i = 0; i < slices; ++i)
        {
            uint32_t slice_offset = i ? image.m_alpha_slice_byte_offset : image.m_rgb_slice_byte_offset;
            uint32_t slice_size = i ? image.m_alpha_slice_byte_length : image.m_rgb_slice_byte_length;
            if (!Range(slice_offset, slice_size, mip.m_Size))
                return Fail(error, "Invalid ETC1S KTX2 slice range");
            basisu::basisu_backend_slice_desc slice;
            slice.m_orig_width = Dimension(texture->m_Info.m_Width, level);
            slice.m_orig_height = Dimension(texture->m_Info.m_Height, level);
            slice.m_num_blocks_x = (slice.m_orig_width + 3) / 4;
            slice.m_num_blocks_y = (slice.m_orig_height + 3) / 4;
            slice.m_alpha = i != 0;
            output.m_slice_desc.push_back(slice);
            output.m_slice_image_data.resize(i + 1);
            output.m_slice_image_data[i].append(texture->m_Data.Begin() + mip.m_Offset + slice_offset, slice_size);

            // Basis stores a checksum of the unpacked ETC1 blocks. This is a lossless
            // unpack for validation and the checksum; the original ETC1S bytes are retained.
            uint32_t block_count = slice.m_num_blocks_x * slice.m_num_blocks_y;
            dmArray<uint8_t> blocks;
            blocks.SetCapacity(block_count * 8);
            blocks.SetSize(block_count * 8);
            uint32_t flags = i ? basist::cDecodeFlagsTranscodeAlphaDataToOpaqueFormats : 0;
            if (!reader.transcode_image_level(level, 0, 0, blocks.Begin(), block_count, basist::transcoder_texture_format::cTFETC1_RGB, flags))
                return Fail(error, "Invalid ETC1S KTX2 slice data");
            output.m_slice_image_crcs.push_back(basist::crc16(blocks.Begin(), blocks.Size(), 0));
        }
        return true;
    }

    bool RepackKtx2Mip(Ktx2Texture* texture, uint32_t level, dmArray<uint8_t>& basis, const char** error)
    {
        if (level >= texture->m_Info.m_LevelCount || (texture->m_Format && texture->m_Format < 145))
            return Fail(error, "KTX2 mip has no supported compressed payload");
        if (texture->m_Format >= 145)
        {
            if (!ReadLevel(texture, level, basis, error))
                return false;
            for (uint32_t offset = 0; offset < basis.Size(); offset += 16)
            {
                basist::color_rgba block[16];
                if (!basist::bc7u::unpack_bc7(basis.Begin() + offset, block))
                    return Fail(error, "Invalid BC7 KTX2 block");
            }
            return true;
        }
        InitBasisU();
        // Let BasisU write each retained mip in the same .basis layout as its encoder.
        basisu::basisu_backend_output output;
        output.m_srgb = texture->m_Info.m_Srgb;
        if (texture->m_Uastc)
        {
            dmArray<uint8_t> blocks;
            if (!ReadLevel(texture, level, blocks, error))
                return false;
            for (uint32_t offset = 0; offset < blocks.Size(); offset += 16)
            {
                basist::unpacked_uastc_block unpacked;
                if (!basist::unpack_uastc(*(const basist::uastc_block*)(blocks.Begin() + offset), unpacked, false))
                    return Fail(error, "Invalid UASTC KTX2 block");
            }
            output.m_tex_format = basist::basis_tex_format::cUASTC_LDR_4x4;
            basisu::basisu_backend_slice_desc slice;
            slice.m_orig_width = Dimension(texture->m_Info.m_Width, level);
            slice.m_orig_height = Dimension(texture->m_Info.m_Height, level);
            slice.m_num_blocks_x = (slice.m_orig_width + 3) / 4;
            slice.m_num_blocks_y = (slice.m_orig_height + 3) / 4;
            slice.m_alpha = texture->m_Info.m_Channels == 4;
            output.m_slice_desc.push_back(slice);
            output.m_slice_image_data.resize(1);
            output.m_slice_image_data[0].append(blocks.Begin(), blocks.Size());
            output.m_slice_image_crcs.push_back(basist::crc16(blocks.Begin(), blocks.Size(), 0));
        }
        else if (!RepackEtc1sMip(texture, level, output, error))
        {
            return false;
        }
        basisu::basisu_file file;
        basist::key_value_vec metadata;
        if (!file.init(output, basist::cBASISTexType2D, 0, 0, false, 0, metadata))
            return Fail(error, "Failed to repack KTX2 mip as BasisU");
        const basisu::uint8_vec& encoded = file.get_compressed_data();
        basis.SetCapacity(encoded.size());
        basis.SetSize(encoded.size());
        memcpy(basis.Begin(), encoded.data(), encoded.size());
        return true;
    }

    static bool InitBc7Encoder()
    {
        bc7e_scalar::bc7e_compress_block_init();
        return true;
    }

    bool EncodeKtx2Mip(Ktx2Texture* texture, uint32_t width, uint32_t height, const uint8_t* pixels, uint32_t size, dmArray<uint8_t>& bytes, const char** error)
    {
        if (!width || !height || width > 65535 || height > 65535 ||
            (uint64_t)width * height * 4 != size || size > MAX_DECODED_SIZE)
            return Fail(error, "Invalid KTX2 encoding dimensions or pixel data");
        if (texture->m_Format && texture->m_Format < 145)
            return Fail(error, "Uncompressed KTX2 does not require a block encoder");
        InitBasisU();
        basisu::image image(pixels, width, height, 4);
        if (texture->m_Format >= 145)
        {
            static const bool initialized = InitBc7Encoder();
            (void)initialized;
            bc7e_scalar::bc7e_compress_block_params params;
            bc7e_scalar::bc7e_compress_block_params_init_slow(&params, texture->m_Info.m_Srgb);
            uint32_t blocks_x = (width + 3) / 4;
            uint32_t blocks_y = (height + 3) / 4;
            bytes.SetCapacity(blocks_x * blocks_y * 16);
            bytes.SetSize(blocks_x * blocks_y * 16);
            for (uint32_t y = 0; y < blocks_y; ++y)
            {
                for (uint32_t x = 0; x < blocks_x; ++x)
                {
                    basisu::color_rgba block[16];
                    uint64_t encoded[2];
                    image.extract_block_clamped(block, x * 4, y * 4, 4, 4);
                    bc7e_scalar::bc7e_compress_blocks(1, encoded, (const uint32_t*)block, &params);
                    memcpy(bytes.Begin() + (y * blocks_x + x) * 16, encoded, 16);
                }
            }
            return true;
        }

        basisu::basis_compressor_params params;
        basisu::job_pool jobs(1);
        params.m_pJob_pool = &jobs;
        params.m_multithreading = false;
        params.m_read_source_images = false;
        params.m_write_output_basis_or_ktx2_files = false;
        params.m_status_output = false;
        params.m_mip_gen = false;
        params.set_format_mode(texture->m_Uastc ? basist::basis_tex_format::cUASTC_LDR_4x4 : basist::basis_tex_format::cETC1S);
        params.set_srgb_options(texture->m_Info.m_Srgb);
        params.m_quality_level = 255;
        params.m_pack_uastc_ldr_4x4_flags = 3;
        params.m_source_images.push_back(image);
        basisu::basis_compressor compressor;
        if (!compressor.init(params) || compressor.process() != basisu::basis_compressor::cECSuccess)
            return Fail(error, "Failed to encode KTX2 mip using its source compression format");
        const basisu::uint8_vec& encoded = compressor.get_output_basis_file();
        bytes.SetCapacity(encoded.size());
        bytes.SetSize(encoded.size());
        memcpy(bytes.Begin(), encoded.data(), encoded.size());
        return true;
    }

    bool DecodeKtx2Mip(Ktx2Texture* texture, uint32_t level, dmArray<uint8_t>& pixels, const char** error)
    {
        if (level >= texture->m_Info.m_LevelCount)
            return Fail(error, "Invalid KTX2 mip level");
        uint32_t width = Dimension(texture->m_Info.m_Width, level), height = Dimension(texture->m_Info.m_Height, level);
        pixels.SetCapacity(width * height * 4);
        pixels.SetSize(width * height * 4);
        dmArray<uint8_t> bytes;
        if (!texture->m_Format)
        {
            InitBasisU();
            if (!texture->m_Transcoding)
            {
                if (!texture->m_Transcoder.start_transcoding())
                    return Fail(error, "Invalid Basis KTX2 global data");
                texture->m_Transcoding = true;
            }
            if (!texture->m_Transcoder.transcode_image_level(level, 0, 0, pixels.Begin(), width * height, basist::transcoder_texture_format::cTFRGBA32))
                return Fail(error, texture->m_Supercompression == 2 ? "Invalid Zstd-compressed Basis KTX2 mip data" : "Invalid Basis KTX2 mip data");
            for (uint32_t i = 0; i < width * height; ++i)
            {
                uint8_t* p = pixels.Begin() + i * 4;
                if (texture->m_Info.m_Channels == 1)
                {
                    p[1] = p[2] = 0;
                    p[3] = 255;
                }
                else if (texture->m_Info.m_Channels == 2)
                {
                    if (texture->m_Model == 163 || texture->m_Channel0 == 5)
                        p[1] = p[3];
                    p[2] = 0;
                    p[3] = 255;
                }
            }
        }
        else
        {
            if (!ReadLevel(texture, level, bytes, error))
                return false;
            if (texture->m_Format >= 145)
            {
                uint32_t blocks_x = (width + 3) / 4;
                for (uint32_t y = 0; y < height; y += 4)
                {
                    for (uint32_t x = 0; x < width; x += 4)
                    {
                        basist::color_rgba block[16];
                        if (!basist::bc7u::unpack_bc7(bytes.Begin() + ((y / 4) * blocks_x + x / 4) * 16, block))
                            return Fail(error, "Invalid BC7 KTX2 block");
                        for (uint32_t by = 0; by < 4 && y + by < height; ++by)
                            for (uint32_t bx = 0; bx < 4 && x + bx < width; ++bx)
                                memcpy(pixels.Begin() + ((y + by) * width + x + bx) * 4, &block[by * 4 + bx], 4);
                    }
                }
            }
            else
            {
                for (uint32_t i = 0; i < width * height; ++i)
                {
                    uint8_t* p = pixels.Begin() + i * 4;
                    p[0] = p[1] = p[2] = 0;
                    p[3] = 255;
                    memcpy(p, bytes.Begin() + i * texture->m_Info.m_Channels, texture->m_Info.m_Channels);
                }
            }
        }
        if (memcmp(texture->m_Swizzle, "rgba", 4))
        {
            for (uint32_t i = 0; i < width * height; ++i)
            {
                uint8_t* p = pixels.Begin() + i * 4;
                uint8_t original[4];
                memcpy(original, p, 4);
                for (uint32_t c = 0; c < 4; ++c)
                {
                    char swizzle = texture->m_Swizzle[c];
                    const char* channels = "rgba";
                    p[c] = swizzle == '0' ? 0 : swizzle == '1' ? 255 : original[strchr(channels, swizzle) - channels];
                }
            }
        }
        return true;
    }
}
