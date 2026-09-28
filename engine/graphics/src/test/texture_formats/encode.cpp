// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0 (https://www.defold.com/license).
// Offline asset generation only. The graphics test does not link these encoders.
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

static bool Save(const char* directory, const char* suffix, const basisu::gpu_image& texture)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/testimage.%s", directory, suffix);
    FILE* file = fopen(path, "wb");
    if (!file)
        return false;
    bool written = fwrite(texture.get_ptr(), 1, texture.get_size_in_bytes(), file) == texture.get_size_in_bytes();
    written = fclose(file) == 0 && written;
    basisu::image decoded;
    if (!written || !texture.unpack(decoded, false))
        return false;
    // Missing channels have the same values as a normalized GPU texture sample.
    for (uint32_t y = 0; y < SIZE; ++y)
        for (uint32_t x = 0; x < SIZE; ++x)
        {
            decoded(x, y).a = 255;
            if (strcmp(suffix, "bc4") == 0 || strcmp(suffix, "etc2_r") == 0)
                decoded(x, y).g = 0;
            if (strcmp(suffix, "bc4") == 0 || strcmp(suffix, "bc5") == 0 ||
                strcmp(suffix, "etc2_r") == 0 || strcmp(suffix, "etc2_rg") == 0)
                decoded(x, y).b = 0;
        }
    snprintf(path, sizeof(path), "%s/../graphics_reference/texture_%s.png", directory, suffix);
    return stbi_write_png(path, SIZE, SIZE, 4, decoded.get_ptr(), SIZE * 4) != 0;
}

int main(int argc, char** argv)
{
    if (argc != 2)
        return 1;
    char path[1024];
    snprintf(path, sizeof(path), "%s/testimage.rgba", argv[1]);
    FILE* file = fopen(path, "rb");
    if (!file)
        return 1;
    uint8_t pixels[SIZE * SIZE * 4];
    bool loaded = fread(pixels, 1, sizeof(pixels), file) == sizeof(pixels);
    fclose(file);
    if (!loaded)
        return 1;

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
        return 1;
    basist::basisu_transcoder_init();
    basist::basisu_transcoder transcoder;
    if (!transcoder.start_transcoding(basis, basis_size))
        return 1;
    struct Format { const char* m_Suffix; basist::transcoder_texture_format m_Format; };
    const Format formats[] = {
        {"bc1", basist::transcoder_texture_format::cTFBC1_RGB},
        {"bc3", basist::transcoder_texture_format::cTFBC3_RGBA},
        {"bc4", basist::transcoder_texture_format::cTFBC4_R},
        {"bc5", basist::transcoder_texture_format::cTFBC5_RG},
        {"bc7", basist::transcoder_texture_format::cTFBC7_RGBA},
        {"etc1", basist::transcoder_texture_format::cTFETC1_RGB},
        {"etc2_r", basist::transcoder_texture_format::cTFETC2_EAC_R11},
        {"etc2_rg", basist::transcoder_texture_format::cTFETC2_EAC_RG11},
        {"etc2_rgba", basist::transcoder_texture_format::cTFETC2_RGBA},
        {"pvrtc4_rgb", basist::transcoder_texture_format::cTFPVRTC1_4_RGB},
        {"pvrtc4_rgba", basist::transcoder_texture_format::cTFPVRTC1_4_RGBA},
    };
    for (uint32_t i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i)
    {
        basisu::gpu_image texture(basist::basis_get_basisu_texture_format(formats[i].m_Format), SIZE, SIZE);
        if (!transcoder.transcode_image_level(basis, basis_size, 0, 0, texture.get_ptr(), texture.get_total_blocks(), formats[i].m_Format) ||
            !Save(argv[1], formats[i].m_Suffix, texture))
            return 1;
        printf("Generated %s\n", formats[i].m_Suffix);
    }
    free(basis);

    struct ASTCFormat { const char* m_Suffix; dmTexc::PixelFormat m_Format; basisu::texture_format m_BasisFormat; };
#define ASTC(w, h) {"astc_" #w "x" #h, dmTexc::PF_RGBA_ASTC_##w##x##h, basisu::texture_format::cASTC_LDR_##w##x##h}
    const ASTCFormat astc_formats[] = {
        ASTC(4,4), ASTC(5,4), ASTC(5,5), ASTC(6,5), ASTC(6,6), ASTC(8,5), ASTC(8,6),
        ASTC(8,8), ASTC(10,5), ASTC(10,6), ASTC(10,8), ASTC(10,10), ASTC(12,10), ASTC(12,12),
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
            return 1;
        basisu::gpu_image texture(astc_formats[i].m_BasisFormat, SIZE, SIZE);
        if (data_size != texture.get_size_in_bytes())
            return 1;
        memcpy(texture.get_ptr(), data, data_size);
        free(data);
        if (!Save(argv[1], astc_formats[i].m_Suffix, texture))
            return 1;
        printf("Generated %s\n", astc_formats[i].m_Suffix);
    }
    return 0;
}
