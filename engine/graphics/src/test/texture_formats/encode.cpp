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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <texc/texc.h>
#include <basis/transcoder/basisu_transcoder.h>
#include <basis/encoder/basisu_gpu_texture.h>

#define STB_IMAGE_WRITE_STATIC
#define STBIWDEF static inline
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

static const uint32_t SIZE = 256;

struct Format
{
    const char*                       m_Suffix;
    basist::transcoder_texture_format m_Format;
};

struct ASTCFormat
{
    const char*            m_Suffix;
    dmTexc::PixelFormat    m_Format;
    basisu::texture_format m_BasisFormat;
};

static bool Save(const char* directory, const char* suffix, const basisu::gpu_image& texture)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/testimage.%s", directory, suffix);
    FILE* file = fopen(path, "wb");
    if (!file)
    {
        fprintf(stderr, "Cannot open %s: %s\n", path, strerror(errno));
        return false;
    }
    bool written = fwrite(texture.get_ptr(), 1, texture.get_size_in_bytes(), file) == texture.get_size_in_bytes();
    written = fclose(file) == 0 && written;
    if (!written)
    {
        fprintf(stderr, "Cannot write texture payload: %s\n", path);
        return false;
    }
    basisu::image decoded;
    if (!texture.unpack(decoded, false))
    {
        fprintf(stderr, "Cannot CPU-decode texture payload: %s\n", path);
        return false;
    }
    // Missing channels have the same values as a normalized GPU texture sample.
    for (uint32_t y = 0; y < SIZE; ++y)
    {
        for (uint32_t x = 0; x < SIZE; ++x)
        {
            decoded(x, y).a = 255;
            if (strcmp(suffix, "bc4") == 0 || strcmp(suffix, "etc2_r") == 0)
            {
                decoded(x, y).g = 0;
            }
            if (strcmp(suffix, "bc4") == 0 || strcmp(suffix, "bc5") == 0 ||
                strcmp(suffix, "etc2_r") == 0 || strcmp(suffix, "etc2_rg") == 0)
            {
                decoded(x, y).b = 0;
            }
        }
    }
    snprintf(path, sizeof(path), "%s/../graphics_reference/texture_%s.png", directory, suffix);
    if (!stbi_write_png(path, SIZE, SIZE, 4, decoded.get_ptr(), SIZE * 4))
    {
        fprintf(stderr, "Cannot write decoded reference: %s\n", path);
        return false;
    }
    return true;
}

// Offline fixture generator: reads <directory>/testimage.rgba, writes raw GPU
// payloads beside it and CPU-decoded PNGs in ../graphics_reference. Build as
// described in README.md, then run generate.py --encoder <path-to-this-tool>.
// The graphics test only uploads the resulting payloads; it does not link encoders.
int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <texture_formats-directory>\n", argv[0]);
        return 1;
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/testimage.rgba", argv[1]);
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        fprintf(stderr, "Cannot open %s: %s\n", path, strerror(errno));
        return 1;
    }
    uint8_t pixels[SIZE * SIZE * 4];
    bool    loaded = fread(pixels, 1, sizeof(pixels), file) == sizeof(pixels);
    fclose(file);
    if (!loaded)
    {
        fprintf(stderr, "Cannot read %u RGBA bytes from %s\n", (uint32_t)sizeof(pixels), path);
        return 1;
    }

    dmTexc::BasisUEncodeSettings settings = {};
    settings.m_Path = path;
    settings.m_Width = settings.m_Height = SIZE;
    settings.m_PixelFormat = dmTexc::PF_R8G8B8A8;
    settings.m_ColorSpace = dmTexc::CS_LRGB;
    settings.m_Data = pixels;
    settings.m_DataCount = sizeof(pixels);
    settings.m_NumThreads = 1;
    settings.m_pack_uastc_flags = 2;
    uint8_t* basis = 0;
    uint32_t basis_size = 0;
    if (!dmTexc::BasisUEncode(&settings, &basis, &basis_size))
    {
        fprintf(stderr, "Cannot encode UASTC source: %s\n", path);
        return 1;
    }
    basist::basisu_transcoder_init();
    basist::basisu_transcoder transcoder;
    if (!transcoder.start_transcoding(basis, basis_size))
    {
        fprintf(stderr, "Cannot start UASTC transcoding: %s\n", path);
        free(basis);
        return 1;
    }
    const Format formats[] = {
        { "bc1", basist::transcoder_texture_format::cTFBC1_RGB },
        { "bc3", basist::transcoder_texture_format::cTFBC3_RGBA },
        { "bc4", basist::transcoder_texture_format::cTFBC4_R },
        { "bc5", basist::transcoder_texture_format::cTFBC5_RG },
        { "bc7", basist::transcoder_texture_format::cTFBC7_RGBA },
        { "etc1", basist::transcoder_texture_format::cTFETC1_RGB },
        { "etc2_r", basist::transcoder_texture_format::cTFETC2_EAC_R11 },
        { "etc2_rg", basist::transcoder_texture_format::cTFETC2_EAC_RG11 },
        { "etc2_rgba", basist::transcoder_texture_format::cTFETC2_RGBA },
        { "pvrtc4_rgb", basist::transcoder_texture_format::cTFPVRTC1_4_RGB },
        { "pvrtc4_rgba", basist::transcoder_texture_format::cTFPVRTC1_4_RGBA },
    };
    for (uint32_t i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i)
    {
        basisu::gpu_image texture(basist::basis_get_basisu_texture_format(formats[i].m_Format), SIZE, SIZE);
        if (!transcoder.transcode_image_level(basis, basis_size, 0, 0, texture.get_ptr(), texture.get_total_blocks(), formats[i].m_Format))
        {
            fprintf(stderr, "Cannot transcode %s to %s\n", path, formats[i].m_Suffix);
            free(basis);
            return 1;
        }
        if (!Save(argv[1], formats[i].m_Suffix, texture))
        {
            free(basis);
            return 1;
        }
        printf("Generated %s\n", formats[i].m_Suffix);
    }
    free(basis);

#define ASTC(w, h) { "astc_" #w "x" #h, dmTexc::PF_RGBA_ASTC_##w##x##h, basisu::texture_format::cASTC_LDR_##w##x##h }
    const ASTCFormat astc_formats[] = {
        ASTC(4, 4),
        ASTC(5, 4),
        ASTC(5, 5),
        ASTC(6, 5),
        ASTC(6, 6),
        ASTC(8, 5),
        ASTC(8, 6),
        ASTC(8, 8),
        ASTC(10, 5),
        ASTC(10, 6),
        ASTC(10, 8),
        ASTC(10, 10),
        ASTC(12, 10),
        ASTC(12, 12),
    };
#undef ASTC
    for (uint32_t i = 0; i < sizeof(astc_formats) / sizeof(astc_formats[0]); ++i)
    {
        dmTexc::ASTCEncodeSettings astc = {};
        astc.m_Path = path;
        astc.m_Width = astc.m_Height = SIZE;
        astc.m_PixelFormat = dmTexc::PF_R8G8B8A8;
        astc.m_ColorSpace = dmTexc::CS_LRGB;
        astc.m_Data = pixels;
        astc.m_DataCount = sizeof(pixels);
        astc.m_NumThreads = 1;
        astc.m_QualityLevel = 100.0f;
        astc.m_OutPixelFormat = astc_formats[i].m_Format;
        uint8_t* data = 0;
        uint32_t data_size = 0;
        if (!dmTexc::ASTCEncode(&astc, &data, &data_size))
        {
            fprintf(stderr, "Cannot encode %s as %s\n", path, astc_formats[i].m_Suffix);
            return 1;
        }
        basisu::gpu_image texture(astc_formats[i].m_BasisFormat, SIZE, SIZE);
        if (data_size != texture.get_size_in_bytes())
        {
            fprintf(stderr, "Unexpected %s payload size: %u (expected %u)\n", astc_formats[i].m_Suffix, data_size, texture.get_size_in_bytes());
            free(data);
            return 1;
        }
        memcpy(texture.get_ptr(), data, data_size);
        free(data);
        if (!Save(argv[1], astc_formats[i].m_Suffix, texture))
        {
            return 1;
        }
        printf("Generated %s\n", astc_formats[i].m_Suffix);
    }
    return 0;
}
