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

#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>
#include "graphics.h"
#include <basis/transcoder/basisu_transcoder.h>

namespace dmGraphics
{
    static bool TextureFormatToBasisFormat(dmGraphics::TextureFormat format, basist::transcoder_texture_format& out)
    {
        switch(format)
        {
        case dmGraphics::TEXTURE_FORMAT_LUMINANCE:          out = basist::transcoder_texture_format::cTFRGBA32; return true;
        case dmGraphics::TEXTURE_FORMAT_LUMINANCE_ALPHA:    out = basist::transcoder_texture_format::cTFRGBA32; return true;
        case dmGraphics::TEXTURE_FORMAT_RGB:                out = basist::transcoder_texture_format::cTFRGBA32; return true;
        case dmGraphics::TEXTURE_FORMAT_RGBA:               out = basist::transcoder_texture_format::cTFRGBA32; return true;
        case dmGraphics::TEXTURE_FORMAT_RGB_16BPP:          out = basist::transcoder_texture_format::cTFRGB565; return true;
        case dmGraphics::TEXTURE_FORMAT_RGBA_16BPP:         out = basist::transcoder_texture_format::cTFRGBA4444; return true;
        case dmGraphics::TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:   out = basist::transcoder_texture_format::cTFPVRTC1_4_RGB; return true;
        case dmGraphics::TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1:  out = basist::transcoder_texture_format::cTFPVRTC1_4_RGBA; return true;
        case dmGraphics::TEXTURE_FORMAT_RGB_ETC1:           out = basist::transcoder_texture_format::cTFETC1_RGB; return true;
        case dmGraphics::TEXTURE_FORMAT_R_ETC2:             out = basist::transcoder_texture_format::cTFETC2_EAC_R11; return true;
        case dmGraphics::TEXTURE_FORMAT_RG_ETC2:            out = basist::transcoder_texture_format::cTFETC2_EAC_RG11; return true;
        case dmGraphics::TEXTURE_FORMAT_RGBA_ETC2:          out = basist::transcoder_texture_format::cTFETC2_RGBA; return true;
        case dmGraphics::TEXTURE_FORMAT_RGBA_ASTC_4X4:      out = basist::transcoder_texture_format::cTFASTC_4x4_RGBA; return true;
        case dmGraphics::TEXTURE_FORMAT_RGB_BC1:            out = basist::transcoder_texture_format::cTFBC1_RGB; return true;
        case dmGraphics::TEXTURE_FORMAT_RGBA_BC3:           out = basist::transcoder_texture_format::cTFBC3_RGBA; return true;
        case dmGraphics::TEXTURE_FORMAT_R_BC4:              out = basist::transcoder_texture_format::cTFBC4_R; return true;
        case dmGraphics::TEXTURE_FORMAT_RG_BC5:             out = basist::transcoder_texture_format::cTFBC5_RG; return true;
        case dmGraphics::TEXTURE_FORMAT_RGBA_BC7:           out = basist::transcoder_texture_format::cTFBC7_RGBA; return true;
        default: break;
        }
        return false;
    }


#define TEX_TRANSCODE_DEBUG 0
    #if defined(TEX_TRANSCODE_DEBUG)
        const char* ToString(dmGraphics::TextureFormat format)
        {
            switch(format)
            {
                case dmGraphics::TEXTURE_FORMAT_LUMINANCE: return "TEXTURE_FORMAT_LUMINANCE";
                case dmGraphics::TEXTURE_FORMAT_LUMINANCE_ALPHA: return "TEXTURE_FORMAT_LUMINANCE_ALPHA";
                case dmGraphics::TEXTURE_FORMAT_RGB: return "TEXTURE_FORMAT_RGB";
                case dmGraphics::TEXTURE_FORMAT_RGBA: return "TEXTURE_FORMAT_RGBA";
                case dmGraphics::TEXTURE_FORMAT_RGB_16BPP: return "TEXTURE_FORMAT_RGB_16BPP";
                case dmGraphics::TEXTURE_FORMAT_RGBA_16BPP: return "TEXTURE_FORMAT_RGBA_16BPP";
                case dmGraphics::TEXTURE_FORMAT_RGB_PVRTC_4BPPV1: return "TEXTURE_FORMAT_RGB_PVRTC_4BPPV1";
                case dmGraphics::TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1: return "TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1";
                case dmGraphics::TEXTURE_FORMAT_RGB_ETC1: return "TEXTURE_FORMAT_RGB_ETC1";
                case dmGraphics::TEXTURE_FORMAT_R_ETC2: return "TEXTURE_FORMAT_R_ETC2";
                case dmGraphics::TEXTURE_FORMAT_RG_ETC2: return "TEXTURE_FORMAT_RG_ETC2";
                case dmGraphics::TEXTURE_FORMAT_RGBA_ETC2: return "TEXTURE_FORMAT_RGBA_ETC2";
                case dmGraphics::TEXTURE_FORMAT_RGBA_ASTC_4X4: return "TEXTURE_FORMAT_RGBA_ASTC_4X4";
                case dmGraphics::TEXTURE_FORMAT_RGB_BC1: return "TEXTURE_FORMAT_RGB_BC1";
                case dmGraphics::TEXTURE_FORMAT_RGBA_BC3: return "TEXTURE_FORMAT_RGBA_BC3";
                case dmGraphics::TEXTURE_FORMAT_R_BC4: return "TEXTURE_FORMAT_R_BC4";
                case dmGraphics::TEXTURE_FORMAT_RG_BC5: return "TEXTURE_FORMAT_RG_BC5";
                case dmGraphics::TEXTURE_FORMAT_RGBA_BC7: return "TEXTURE_FORMAT_RGBA_BC7";
                default: return "";
            }
        }
        const char* ToString(basist::transcoder_texture_format format)
        {
            switch(format)
            {
                case basist::transcoder_texture_format::cTFRGBA32: return "cTFRGBA32";
                case basist::transcoder_texture_format::cTFRGB565: return "cTFRGB565";
                case basist::transcoder_texture_format::cTFRGBA4444: return "cTFRGBA4444";
                case basist::transcoder_texture_format::cTFPVRTC1_4_RGB: return "cTFPVRTC1_4_RGB";
                case basist::transcoder_texture_format::cTFPVRTC1_4_RGBA: return "cTFPVRTC1_4_RGBA";
                case basist::transcoder_texture_format::cTFETC1_RGB: return "cTFETC1_RGB";
                case basist::transcoder_texture_format::cTFETC2_RGBA: return "cTFETC2_RGBA";
                case basist::transcoder_texture_format::cTFETC2_EAC_R11: return "cTFETC2_EAC_R11";
                case basist::transcoder_texture_format::cTFETC2_EAC_RG11: return "cTFETC2_EAC_RG11";
                case basist::transcoder_texture_format::cTFASTC_4x4_RGBA: return "cTFASTC_4x4_RGBA";
                case basist::transcoder_texture_format::cTFBC1_RGB: return "cTFBC1_RGB";
                case basist::transcoder_texture_format::cTFBC3_RGBA: return "cTFBC3_RGBA";
                case basist::transcoder_texture_format::cTFBC4_R: return "cTFBC4_R";
                case basist::transcoder_texture_format::cTFBC5_RG: return "cTFBC5_RG";
                case basist::transcoder_texture_format::cTFBC7_RGBA: return "cTFBC7_RGBA";
                default: return "";
            }
        }
    #else
        const char* ToString(dmGraphics::TextureFormat format)
        {
            return "";
        }
        const char* ToString(basist::transcoder_texture_format)
        {
            return "";
        }
    #endif


    bool IsFormatTranscoded(dmGraphics::TextureImage::CompressionType compression_type)
    {
        return compression_type == dmGraphics::TextureImage::COMPRESSION_TYPE_BASIS_UASTC ||
               compression_type == dmGraphics::TextureImage::COMPRESSION_TYPE_BASIS_ETC1S;
    }

    struct ImageTranscodeLevelData
    {
        uint32_t m_OriginalWidth;
        uint32_t m_OriginalHeight;
        uint32_t m_TotalBlocks;
        uint32_t m_Size;          // Number of bytes the basis transcoder writes for this level
        uint32_t m_BytesPerBlock; // For compressed formats only
    };

    struct ImageTranscodeState
    {
        ImageTranscodeState()
        : m_Transcoder()
        , m_LevelData(0)
        , m_SrcDataPtr(0)
        {}

        basist::basisu_transcoder m_Transcoder;
        basist::basisu_image_info m_Info;
        ImageTranscodeLevelData*  m_LevelData;
        uint8_t*                  m_SrcDataPtr;
        uint32_t                  m_SrcDataSize;
    };

    static void TranscoderDeleteStateArray(ImageTranscodeState* states, uint32_t num_states)
    {
        for (uint32_t i = 0; i < num_states; ++i)
        {
            delete[] states[i].m_LevelData;
        }

        delete[] states;
    }

    static bool TranscodeInitializeState(const char* path, ImageTranscodeState& state, uint8_t* data_ptr, uint32_t data_size, basist::transcoder_texture_format transcoder_format)
    {
        if (!state.m_Transcoder.validate_header(data_ptr, data_size))
        {
            dmLogError("Failed to validate header (Basis) for file '%s'", path);
            return false;
        }

        state.m_Transcoder.get_image_info(data_ptr, data_size, state.m_Info, 0);
        state.m_Transcoder.start_transcoding(data_ptr, data_size);
        state.m_SrcDataPtr        = data_ptr;
        state.m_SrcDataSize       = data_size;

        state.m_LevelData = new ImageTranscodeLevelData[state.m_Info.m_total_levels];

        int image_index = 0; // const
        for (uint32_t level_index = 0; level_index < state.m_Info.m_total_levels; ++level_index)
        {
            uint32_t orig_width, orig_height, total_blocks;
            if (!state.m_Transcoder.get_image_level_desc(data_ptr, data_size, image_index, level_index, orig_width, orig_height, total_blocks))
            {
                dmLogError("Failed to get image descriptor at level %d for (Basis) file '%s'", level_index, path);
                return false;
            }

            state.m_LevelData[level_index].m_OriginalWidth  = orig_width;
            state.m_LevelData[level_index].m_OriginalHeight = orig_height;
            state.m_LevelData[level_index].m_TotalBlocks    = total_blocks;

            if (basist::basis_transcoder_format_is_uncompressed(transcoder_format))
            {
                const uint32_t bytes_per_pixel = basist::basis_get_uncompressed_bytes_per_pixel(transcoder_format);
                const uint32_t bytes_per_line  = orig_width * bytes_per_pixel;
                const uint32_t bytes_per_slice = bytes_per_line * orig_height;

                state.m_LevelData[level_index].m_Size = bytes_per_slice;
            }
            else
            {
                uint32_t bytes_per_block = basis_get_bytes_per_block_or_pixel(transcoder_format);
                uint32_t required_size   = total_blocks * bytes_per_block;

                if (transcoder_format == basist::transcoder_texture_format::cTFPVRTC1_4_RGB ||
                    transcoder_format == basist::transcoder_texture_format::cTFPVRTC1_4_RGBA)
                {
                    // For PVRTC1, Basis only writes (or requires) total_blocks * bytes_per_block. But GL requires extra padding for very small textures:
                    // https://www.khronos.org/registry/OpenGL/extensions/IMG/IMG_texture_compression_pvrtc.txt
                    // The transcoder will clear the extra bytes followed the used blocks to 0.
                    const uint32_t width = (orig_width + 3) & ~3;
                    const uint32_t height = (orig_height + 3) & ~3;
                    required_size = (dmMath::Max(8U, width) * dmMath::Max(8U, height) * 4 + 7) / 8;
                    assert(required_size >= total_blocks * bytes_per_block);
                }

                state.m_LevelData[level_index].m_BytesPerBlock = bytes_per_block;
                state.m_LevelData[level_index].m_Size          = required_size;
            }
        }

        return true;
    }

    // The basis transcoder only produces RGBA32 for the uncompressed formats, so LUMINANCE,
    // LUMINANCE_ALPHA and RGB have to be packed down from four channels afterwards. Returns 0 for
    // the formats that are used as-is. Must agree with dmGraphics::GetTextureFormatBitsPerPixel(),
    // which is what the rest of the engine sizes these formats with (kept local so that this
    // library doesn't have to link the graphics core).
    static uint32_t GetPackedChannelCount(basist::transcoder_texture_format transcoder_format, dmGraphics::TextureFormat graphics_format)
    {
        if (transcoder_format != basist::transcoder_texture_format::cTFRGBA32)
        {
            return 0;
        }

        switch(graphics_format)
        {
        case dmGraphics::TEXTURE_FORMAT_LUMINANCE:       return 1;
        case dmGraphics::TEXTURE_FORMAT_LUMINANCE_ALPHA: return 2;
        case dmGraphics::TEXTURE_FORMAT_RGB:             return 3;
        default:                                         return 0; // TEXTURE_FORMAT_RGBA, no packing needed
        }
    }

    // Size of one image slice as it is handed to the graphics API. For the packed formats this is
    // smaller than what the transcoder writes (m_Size), and it is the packed size that every
    // consumer assumes: res_texture.cpp passes it as TextureParams::m_DataSize, and all adapters
    // read array layers / cubemap faces as tightly packed slices at that stride.
    static uint32_t GetTranscodedSliceSize(const ImageTranscodeLevelData& level, basist::transcoder_texture_format transcoder_format, dmGraphics::TextureFormat graphics_format)
    {
        uint32_t num_channels = GetPackedChannelCount(transcoder_format, graphics_format);
        if (num_channels > 0)
        {
            return level.m_OriginalWidth * level.m_OriginalHeight * num_channels;
        }
        return level.m_Size;
    }

    static bool TranscodeLevel(const char* path, ImageTranscodeState& state, uint8_t* level_data, uint8_t level_index,  basist::transcoder_texture_format transcoder_format, dmGraphics::TextureFormat graphics_format)
    {
        int image_index = 0;
        uint32_t flags = 0;

        if (basist::basis_transcoder_format_is_uncompressed(transcoder_format))
        {
            if (!state.m_Transcoder.transcode_image_level(
                    state.m_SrcDataPtr,
                    state.m_SrcDataSize,
                    image_index,
                    level_index,
                    level_data,
                    state.m_LevelData[level_index].m_OriginalWidth * state.m_LevelData[level_index].m_OriginalHeight,
                    transcoder_format,
                    flags,
                    state.m_LevelData[level_index].m_OriginalWidth,
                    nullptr,
                    state.m_LevelData[level_index].m_OriginalHeight))
            {
                return false;
            }

            // If we wanted a Luminance, LuminanceAlpha or RGB
            // let's convert back to those formats.
            // This shrinks the slice to GetTranscodedSliceSize() bytes, which is the stride
            // Transcode() lays the images out with - the two must not disagree, or every slice past
            // the first ends up at the wrong offset (https://github.com/defold/defold/issues/12868).
            uint32_t num_channels = GetPackedChannelCount(transcoder_format, graphics_format);
            if (num_channels > 0)
            {
                uint8_t* p = level_data;
                uint8_t* pend = level_data + state.m_LevelData[level_index].m_Size;
                uint8_t* packed_dst = level_data;
                while (p < pend)
                {
                    for (uint32_t c = 0; c < num_channels; ++c)
                    {
                        *packed_dst++ = p[c];
                    }
                    p += 4;
                }
            }
        }
        else
        {
            return state.m_Transcoder.transcode_image_level(
                state.m_SrcDataPtr,
                state.m_SrcDataSize,
                image_index,
                level_index,
                level_data,
                state.m_LevelData[level_index].m_Size / state.m_LevelData[level_index].m_BytesPerBlock,
                transcoder_format,
                flags);
        }

        return true;
    }

    bool Transcode(const char* path, dmGraphics::TextureImage::Image* image, uint8_t image_count, uint8_t* image_bytes, dmGraphics::TextureFormat format,
                    uint8_t** images, uint32_t* sizes, uint32_t* num_transcoded_mips)
    {
        DM_PROFILE(__FUNCTION__);

        assert(image_count > 0);

        static int first = 1;
        if (first)
        {
            first = 0;
            basist::basisu_transcoder_init();
        }

        basist::transcoder_texture_format transcoder_format;
        if (!TextureFormatToBasisFormat(format, transcoder_format))
        {
            dmLogError("Failed to convert texture format %d to basis format %d for file '%s'", format, (int)transcoder_format, path);
            return false;
        }

    #if defined(TEX_TRANSCODE_DEBUG)
        dmLogInfo("Transcoding: %s from %d to %d (%s -> %s)", path, format, (int)transcoder_format, ToString(format), ToString(transcoder_format));
    #endif

        uint32_t max_mipmap_count              = dmMath::Min(*num_transcoded_mips, image[0].m_MipMapSize.m_Count);
        uint32_t images_and_mipmap_count       = max_mipmap_count * image_count;
        ImageTranscodeState* image_transcoders = new ImageTranscodeState[images_and_mipmap_count];

        uint32_t slice_data_offset = 0;
        for (int i = 0; i < image_count * max_mipmap_count; ++i)
        {
            uint32_t size = image->m_MipMapSizeCompressed[i];
            uint8_t* ptr  = &image_bytes[slice_data_offset];

            if (!TranscodeInitializeState(path, image_transcoders[i], ptr, size, transcoder_format))
            {
                TranscoderDeleteStateArray(image_transcoders, images_and_mipmap_count);
                return false;
            }

            slice_data_offset += size;
        }

        uint32_t transcoder_index = 0;

        for (int i = 0; i < max_mipmap_count; ++i)
        {
            const ImageTranscodeLevelData& level = image_transcoders[transcoder_index].m_LevelData[0];

            uint32_t transcode_size = level.m_Size;
            uint32_t slice_size     = GetTranscodedSliceSize(level, transcoder_format, format);

            // Slice j is transcoded in place at data_write_ptr + j * slice_size. Transcoding it
            // temporarily spills up to transcode_size bytes into the slices that haven't been
            // written yet, and the packing step immediately shrinks it back down to slice_size, so
            // only the last slice needs room for that spill.
            uint32_t alloc_size     = (image_count - 1) * slice_size + dmMath::Max(slice_size, transcode_size);
            uint8_t* data_write_ptr = new uint8_t[alloc_size];

            images[i] = data_write_ptr;
            sizes[i]  = slice_size;

            for (int j = 0; j < image_count; ++j)
            {
                if (!TranscodeLevel(path, image_transcoders[transcoder_index], data_write_ptr, 0, transcoder_format, format))
                {
                    dmLogError("Transcoding failed on level %d for %s", i, path);
                    delete[] images[i];
                    images[i] = 0;
                    sizes[i]  = 0;
                    TranscoderDeleteStateArray(image_transcoders, images_and_mipmap_count);
                    return false;
                }

                data_write_ptr += slice_size;
                transcoder_index++;
            }
        }

        *num_transcoded_mips = max_mipmap_count;

        TranscoderDeleteStateArray(image_transcoders, images_and_mipmap_count);
        return true;
    }
}
