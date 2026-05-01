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

#include <dlib/dstrings.h>

#include "texc/texc.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

dmTexc::PixelFormat GetPixelFormatFromChannels(int channels)
{
    switch(channels)
    {
        case 1: return dmTexc::PF_L8;
        case 2: return dmTexc::PF_L8A8;
        case 3: return dmTexc::PF_R8G8B8;
        case 4: return dmTexc::PF_R8G8B8A8;
        default:break;
    }
    return (dmTexc::PixelFormat) -1;
}

struct EncodeParams
{
    const char*              m_PathIn;
    const char*              m_PathOut;
    dmTexc::ColorSpace       m_ColorSpace;
    dmTexc::CompressionType  m_CompressionType;
    dmTexc::CompressionLevel m_CompressionLevel;
    uint8_t                  m_PremultiplyAlpha : 1;
    uint8_t                  m_FlipY            : 1;
    uint8_t                  m_FlipX            : 1;
    uint8_t                  m_MipMaps          : 1;
    uint8_t                  m_Verbose          : 1;
};

EncodeParams GetDefaultEncodeParams()
{
    EncodeParams params       = {};
    params.m_MipMaps          = 0;
    params.m_PremultiplyAlpha = 1;
    params.m_ColorSpace       = dmTexc::CS_SRGB;
    params.m_CompressionType  = dmTexc::CT_DEFAULT;
    params.m_CompressionLevel = dmTexc::CL_NORMAL;
    return params;
}

inline bool IsArg(const char* arg)
{
    size_t str_len = strlen(arg);
    return str_len > 2 && arg[0] == '-' && arg[1] == '-';
}

template <typename T>
struct ArgNameToTypeValue
{
    const char* m_Name;
    const T     m_TypeValue;
};

template <typename T>
T GetArgTypeValue(const char* arg, ArgNameToTypeValue<T>* args, T default_value)
{
    int i=0;
    while(args[i].m_Name)
    {
        if (dmStrCaseCmp(args[i].m_Name, arg) == 0)
        {
            return args[i].m_TypeValue;
        }
        i++;
    }

    return default_value;
}

template <typename T>
const char* GetArgTypeName(T from_value, ArgNameToTypeValue<T>* args)
{
    int i=0;
    while(args[i].m_Name)
    {
        if (args[i].m_TypeValue ==  from_value)
        {
            return args[i].m_Name;
        }
        i++;
    }

    return "<unknown>";
}

ArgNameToTypeValue<dmTexc::ColorSpace> g_cs_lut[] = {
    {"LRGB", dmTexc::CS_LRGB},
    {"SRGB", dmTexc::CS_SRGB},
    {0}
};

ArgNameToTypeValue<dmTexc::CompressionType> g_ct_lut[] = {
    {"DEFAULT",     dmTexc::CT_DEFAULT},
    {"WEBP",        dmTexc::CT_WEBP},
    {"WEBP_LOSSY",  dmTexc::CT_WEBP_LOSSY},
    {"BASIS_UASTC", dmTexc::CT_BASIS_UASTC},
    {"BASIS_ETC1S", dmTexc::CT_BASIS_ETC1S},
    {0}
};

ArgNameToTypeValue<dmTexc::CompressionLevel> g_cl_lut[] = {
    {"FAST",   dmTexc::CL_FAST},
    {"NORMAL", dmTexc::CL_NORMAL},
    {"HIGH",   dmTexc::CL_HIGH},
    {"BEST",   dmTexc::CL_BEST},
    {"ENUM",   dmTexc::CL_ENUM},
    {0}
};

void PrintEncodeParams(EncodeParams params)
{
    if (!params.m_Verbose)
        return;
#define TRUE_FALSE_LABEL(cond) (cond?"TRUE":"FALSE")
    printf("-----------------------------\n");
    printf("Input path        : %s\n", params.m_PathIn);
    printf("Output path       : %s\n", params.m_PathOut);
    printf("Color space       : %s\n", GetArgTypeName(params.m_ColorSpace, g_cs_lut));
    printf("Compression type  : %s\n", GetArgTypeName(params.m_CompressionType, g_ct_lut));
    printf("Compression level : %s\n", GetArgTypeName(params.m_CompressionLevel, g_cl_lut));
    printf("PreMultiplyAlpha  : %s\n", TRUE_FALSE_LABEL(params.m_PremultiplyAlpha));
    printf("Flip-x            : %s\n", TRUE_FALSE_LABEL(params.m_FlipX));
    printf("Flip-y            : %s\n", TRUE_FALSE_LABEL(params.m_FlipY));
    printf("Mipmaps           : %s\n", TRUE_FALSE_LABEL(params.m_MipMaps));
#undef TRUE_FALSE_LABEL
}

void GetEncodeParamsFromArgs(int argc, const char* argv[], EncodeParams& params)
{
    params.m_PathIn  = argc > 1 ? argv[1] : 0;
    params.m_PathOut = argc > 2 ? argv[2] : 0;

    for (int i = 3; i < argc; ++i)
    {
        if (IsArg(argv[i]))
        {
            const char* actual_arg    = argv[i]+2;
            #define CMP_ARG(name)      (dmStrCaseCmp(actual_arg, name) == 0)
            #define CMP_ARG_1_OP(name) (CMP_ARG(name) && (i+1) < argc)

            if (CMP_ARG("premultiply-alpha"))
                params.m_PremultiplyAlpha = 1;
            else if (CMP_ARG("flip-x"))
                params.m_FlipX = 1;
            else if (CMP_ARG("flip-y"))
                params.m_FlipY = 1;
            else if (CMP_ARG("verbose"))
                params.m_Verbose = 1;
            else if (CMP_ARG("mipmaps"))
                params.m_MipMaps = 1;
            else if (CMP_ARG_1_OP("color-space"))
                params.m_ColorSpace = GetArgTypeValue(argv[++i], g_cs_lut, params.m_ColorSpace);
            else if (CMP_ARG_1_OP("compression-type"))
                params.m_CompressionType = GetArgTypeValue(argv[++i], g_ct_lut, params.m_CompressionType);
            else if (CMP_ARG_1_OP("compression-level"))
                params.m_CompressionLevel = GetArgTypeValue(argv[++i], g_cl_lut, params.m_CompressionLevel);

            #undef CMP_ARG_1_OP
            #undef CMP_ARG
        }
    }
}

int DoEncode(EncodeParams params)
{
    int x,y,n;
    unsigned char *data = stbi_load(params.m_PathIn, &x, &y, &n, 0);

    uint32_t data_size = x * y * n;

    dmTexc::Image* image = dmTexc::CreateImage(params.m_PathIn, x, y, GetPixelFormatFromChannels(n), params.m_ColorSpace, data_size,data);

    if (params.m_PremultiplyAlpha && !dmTexc::PreMultiplyAlpha(image))
    {
        printf("Unable to premultiply alpha\n");
        return -1;
    }

    if (params.m_FlipY && !dmTexc::Flip(image, dmTexc::FLIP_AXIS_Y))
    {
        printf("Unable to flip Y\n");
        return -1;
    }

    if (params.m_FlipX && !dmTexc::Flip(image, dmTexc::FLIP_AXIS_X))
    {
        printf("Unable to flip Y\n");
        return -1;
    }

    if (params.m_MipMaps)
    {
        printf("Mipmap generation is currently not implement. File a feature request if you need it.\n");
    }

    uint8_t* out = 0;
    uint32_t out_size = 0;

    if (params.m_CompressionType == dmTexc::CT_DEFAULT)
    {
        dmTexc::DefaultEncodeSettings settings = {};
        settings.m_Path           = image->m_Path;
        settings.m_Width          = x;
        settings.m_Height         = y;
        settings.m_PixelFormat    = image->m_PixelFormat;
        settings.m_ColorSpace     = image->m_ColorSpace;
        settings.m_Data           = image->m_Data;
        settings.m_DataCount      = image->m_DataCount;
        settings.m_NumThreads     = 4;
        settings.m_Debug          = false;
        settings.m_OutPixelFormat = image->m_PixelFormat;

        if (!dmTexc::DefaultEncode(&settings, &out, &out_size))
        {
            printf("Unable to encode\n");
            dmTexc::DestroyImage(image);
            free(out);
            return -1;
        }
    }
    else if (params.m_CompressionType == dmTexc::CT_BASIS_UASTC ||
             params.m_CompressionType == dmTexc::CT_BASIS_ETC1S)
    {
        dmTexc::BasisUEncodeSettings settings = {};
        settings.m_Path           = image->m_Path;
        settings.m_Width          = x;
        settings.m_Height         = y;
        settings.m_PixelFormat    = image->m_PixelFormat;
        settings.m_ColorSpace     = image->m_ColorSpace;
        settings.m_Data           = image->m_Data;
        settings.m_DataCount      = image->m_DataCount;
        settings.m_NumThreads     = 4;
        settings.m_Debug          = false;
        settings.m_OutPixelFormat = dmTexc::PF_RGBA_BC3;

        // These params corresponds to the "BEST" preset
        settings.m_rdo_uastc                = 1;
        settings.m_pack_uastc_flags         = 0;
        settings.m_rdo_uastc_dict_size      = 4096;
        settings.m_rdo_uastc_quality_scalar = 3.0f;

        if (!dmTexc::BasisUEncode(&settings, &out, &out_size))
        {
            printf("Unable to encode to BasisU\n");
            dmTexc::DestroyImage(image);
            free(out);
            return -1;
        }
    }

    dmTexc::DestroyImage(image);

    stbi_image_free(data);

    FILE* f = fopen(params.m_PathOut, "w");
    fwrite(out, out_size, 1, f);
    fclose(f);

    free(out);

    return 0;
}

void ShowHelp()
{
#define PRINT_ARG_LIST(lst) \
    { \
        for(int i=0; lst[i].m_Name != 0; i++) \
            printf("    %s\n", lst[i].m_Name); \
    }
    printf("Usage: texconvert <input-file> <output-file> [options]\n");
    printf("Options:\n");
    printf("  --premultiply-alpha         : Use premultiply alpha\n");
    printf("  --flip-x                    : Flip image on X axis\n");
    printf("  --flip-y                    : Flip image on Y axis\n");
    printf("  --verbose                   : Verbose output\n");
    printf("  --mipmaps                   : Generate mipmaps\n");
    printf("  --color-space <color-space> : Sets the color space, defaults to 'SRGB'. Supported values are:\n");
    PRINT_ARG_LIST(g_cs_lut);
    printf("  --compression-type <type>   : Sets the compression type, defaults to 'DEFAULT'. Supported values are:\n");
    PRINT_ARG_LIST(g_ct_lut);
    printf("  --compression-level <level> : Sets the compression level, defaults to 'NORMAL'. Supported values are:\n");
    PRINT_ARG_LIST(g_cl_lut);
#undef PRINT_ARG_LIST
}

int main(int argc, char const *argv[])
{
    EncodeParams params = GetDefaultEncodeParams();
    GetEncodeParamsFromArgs(argc, argv, params);

    if (!params.m_PathIn || !params.m_PathOut)
    {
        if (!params.m_PathIn)
        {
            printf("No input file found in arguments\n");
        }

        if (!params.m_PathOut)
        {
            printf("No output path found in arguments\n");
        }

        ShowHelp();
        return -1;
    }

    PrintEncodeParams(params);

    return DoEncode(params);
}
