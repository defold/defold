// Copyright 2020 The Defold Foundation
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
#include "graphics_transcoder.h"
#include <graphics/graphics_ddf.h>
#include <basis/transcoder/basisu_transcoder.h>

namespace dmGraphics
{
    static bool TextureFormatToBasisFormat(dmGraphics::TextureFormat format, basist::transcoder_texture_format& out)
    {
        switch(format)
        {
        case dmGraphics::TEXTURE_FORMAT_LUMINANCE:          out = basist::cTFRGBA32; return true;
        case dmGraphics::TEXTURE_FORMAT_LUMINANCE_ALPHA:    out = basist::cTFRGBA32; return true;
        case dmGraphics::TEXTURE_FORMAT_RGB:                out = basist::cTFRGBA32; return true;
        case dmGraphics::TEXTURE_FORMAT_RGBA:               out = basist::cTFRGBA32; return true;
        case dmGraphics::TEXTURE_FORMAT_RGB_16BPP:          out = basist::cTFRGB565; return true;
        case dmGraphics::TEXTURE_FORMAT_RGBA_16BPP:         out = basist::cTFRGBA4444; return true;
        case dmGraphics::TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:   out = basist::cTFPVRTC1_4_RGB; return true;
        case dmGraphics::TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1:  out = basist::cTFPVRTC1_4_RGBA; return true;
        case dmGraphics::TEXTURE_FORMAT_RGB_ETC1:           out = basist::cTFETC1_RGB; return true;
        case dmGraphics::TEXTURE_FORMAT_RGBA_ETC2:          out = basist::cTFETC2_RGBA; return true;
        case dmGraphics::TEXTURE_FORMAT_RGBA_ASTC_4x4:      out = basist::cTFASTC_4x4_RGBA; return true;
        case dmGraphics::TEXTURE_FORMAT_RGB_BC1:            out = basist::cTFBC1_RGB; return true;
        case dmGraphics::TEXTURE_FORMAT_RGBA_BC3:           out = basist::cTFBC3_RGBA; return true;
        case dmGraphics::TEXTURE_FORMAT_R_BC4:              out = basist::cTFBC4_R; return true;
        case dmGraphics::TEXTURE_FORMAT_RG_BC5:             out = basist::cTFBC5_RG; return true;
        case dmGraphics::TEXTURE_FORMAT_RGBA_BC7:           out = basist::cTFBC7_RGBA; return true;
        default: break;
        }
        return false;
    }


#define TEX_TRANSCODE_DEBUG 1
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
                case dmGraphics::TEXTURE_FORMAT_RGBA_ETC2: return "TEXTURE_FORMAT_RGBA_ETC2";
                case dmGraphics::TEXTURE_FORMAT_RGBA_ASTC_4x4: return "TEXTURE_FORMAT_RGBA_ASTC_4x4";
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
                case basist::cTFRGBA32: return "cTFRGBA32";
                case basist::cTFRGB565: return "cTFRGB565";
                case basist::cTFRGBA4444: return "cTFRGBA4444";
                case basist::cTFPVRTC1_4_RGB: return "cTFPVRTC1_4_RGB";
                case basist::cTFPVRTC1_4_RGBA: return "cTFPVRTC1_4_RGBA";
                case basist::cTFETC1_RGB: return "cTFETC1_RGB";
                case basist::cTFETC2_RGBA: return "cTFETC2_RGBA";
                case basist::cTFASTC_4x4_RGBA: return "cTFASTC_4x4_RGBA";
                case basist::cTFBC1_RGB: return "cTFBC1_RGB";
                case basist::cTFBC3_RGBA: return "cTFBC3_RGBA";
                case basist::cTFBC4_R: return "cTFBC4_R";
                case basist::cTFBC5_RG: return "cTFBC5_RG";
                case basist::cTFBC7_RGBA: return "cTFBC7_RGBA";
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
        if (compression_type == dmGraphics::TextureImage::COMPRESSION_TYPE_BASIS_UASTC ||
            compression_type == dmGraphics::TextureImage::COMPRESSION_TYPE_BASIS_ETC1S )
            return true;
        return false;
    }

    bool Transcode(const char* path, dmGraphics::TextureImage::Image* image, dmGraphics::TextureFormat format,
                    uint8_t** images, uint32_t* sizes, uint32_t* num_transcoded_mips)
    {
        DM_PROFILE(Graphics, "TranscodeBasis");

        static int first = 1;
        if (first)
        {
            first = 0;
            basist::basisu_transcoder_init();
        }

        basist::basisu_transcoder tr(0);

        uint32_t max_num_images = *num_transcoded_mips;
        uint32_t total_size = 0;

        // Currently storing one basis file with multiple mip map levels
        uint32_t size = image->m_Data.m_Count;
        uint8_t* ptr = image->m_Data.m_Data; // image->m_MipMapOffset[i];
        if (!tr.validate_header(ptr, size))
        {
            dmLogError("Failed to validate header (Basis) for file '%s'", path);
            return false;
        }

        basist::basisu_image_info info;
        tr.get_image_info(ptr, size, info, 0);

        basist::transcoder_texture_format transcoder_format;
        if (!TextureFormatToBasisFormat(format, transcoder_format))
        {
            dmLogError("Failed to texture format %d to basis format %d for file '%s'", format, transcoder_format, path);
            return false;
        }

        dmLogWarning("MAWE: Transcoding: %p from %d to %d (%s -> %s)", path, format, transcoder_format, ToString(format), ToString(transcoder_format));

        //uint32_t block_size = (uint32_t)basist::basis_get_bytes_per_block_or_pixel(transcoder_format);

        tr.start_transcoding(ptr, size);

        dmLogWarning("MAWE: total input size %u", size);
        dmLogWarning("MAWE: num mipmaps %u", info.m_total_levels);

        uint32_t image_index = 0;
        for (uint32_t level_index = 0; level_index < info.m_total_levels; ++level_index) {

            uint32_t orig_width, orig_height, total_blocks;
            if (!tr.get_image_level_desc(ptr, size, image_index, level_index, orig_width, orig_height, total_blocks))
                return 0;

        dmLogWarning("MAWE: Transcoding mipmap %u  mip size: %u x %u", level_index, orig_width, orig_height);


            // from basis_wrappers.cpp

            //uint32_t flags = get_alpha_for_opaque_formats ? cDecodeFlagsTranscodeAlphaDataToOpaqueFormats : 0;
            uint32_t flags = 0;

            bool status = false;
            uint32_t level_size = 0;
            uint8_t* level_data = 0;
            if (basist::basis_transcoder_format_is_uncompressed(transcoder_format))
            {
                const uint32_t bytes_per_pixel = basist::basis_get_uncompressed_bytes_per_pixel(transcoder_format);
                const uint32_t bytes_per_line = orig_width * bytes_per_pixel;
                const uint32_t bytes_per_slice = bytes_per_line * orig_height;

                level_size = bytes_per_slice;
                level_data = new uint8_t[bytes_per_slice];

                status = tr.transcode_image_level(
                    ptr, size, image_index, level_index,
                    level_data, orig_width * orig_height,
                    transcoder_format,
                    flags,
                    orig_width,
                    nullptr,
                    orig_height);

                // If we wanted a Luminance, LuminanceAlpha or RGB
                // let's convert back to those formats
                if (transcoder_format == basist::cTFRGBA32 && format != dmGraphics::TEXTURE_FORMAT_RGBA)
                {
                    int num_channels = 0;
                    if (format == dmGraphics::TEXTURE_FORMAT_LUMINANCE)
                        num_channels = 1;
                    else if (format == dmGraphics::TEXTURE_FORMAT_LUMINANCE_ALPHA)
                        num_channels = 2;
                    else if (format == dmGraphics::TEXTURE_FORMAT_RGB)
                        num_channels = 3;
                    if (num_channels > 0)
                    {
                        uint8_t* p = level_data;
                        uint8_t* pend = level_data+bytes_per_slice;
                        uint8_t* packed_dst = level_data;
                        while (p < pend)
                        {
                            for (int c = 0; c < num_channels; ++c)
                            {
                                *packed_dst++ = p[c];
                            }
                            p += 4;
                        }
                    }
                }
            }
            else
            {
                uint32_t bytes_per_block = basis_get_bytes_per_block_or_pixel(transcoder_format);

                uint32_t required_size = total_blocks * bytes_per_block;

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

                level_size = required_size;
                level_data = new uint8_t[required_size];

                status = tr.transcode_image_level(
                    ptr, size, image_index, level_index,
                    level_data, level_size / bytes_per_block,
                    transcoder_format,
                    flags);
            }

            if (!status)
            {
                dmLogError("Transcoding failed on level %d for %s\n", level_index, path);
                if (level_data)
                    delete[] level_data;
                return false;
            }

            if (level_index < max_num_images)
            {
                images[level_index] = level_data;
                sizes[level_index] = level_size;

                total_size += level_size;
            }
        };

        *num_transcoded_mips = info.m_total_levels;

        printf("total size: %u (kb) in %u mip levels\n", total_size / 1024, info.m_total_levels);

        return true;
    }
}